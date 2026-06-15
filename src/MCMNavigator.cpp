#include "PCH.h"

#include "MCMNavigator.h"

#include <cctype>
#include <functional>
#include <mutex>

namespace MCMNavigator
{
	namespace
	{
		constexpr int kMaxRetries = 20;
		constexpr int kModRetryFrames = 3;  // ~50ms at 60fps; enough for MCM list to finish animating

		constexpr auto kConfigPanel = "_root.ConfigPanelFader.configPanel.";
		constexpr auto kModListPanel = "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.";
		constexpr auto kModList = "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.modListFader.list.";

		bool CaseInsensitiveLess(const std::string& a, const std::string& b)
		{
			return std::ranges::lexicographical_compare(
				a, b,
				[](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
		}

		bool CaseInsensitiveEqual(std::string_view a, std::string_view b)
		{
			return std::ranges::equal(
				a, b,
				[](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
		}

		std::string_view StripModNamePrefix(std::string_view name)
		{
			const auto pos = name.find("::");
			return pos != std::string_view::npos ? name.substr(pos + 2) : name;
		}

		struct NavigationTarget
		{
			std::string modName;
			int         modRetries{ 0 };
		};

		// Set once per dispatch, consumed by the async chain.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline NavigationTarget g_target{};
		// Navigation in-flight guard.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline std::atomic<bool> g_lock{ false };

		// Protected by g_cacheMutex — render thread reads, game/UI thread writes.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline std::mutex g_cacheMutex{};
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline std::vector<std::string> g_modCache{};
		// Debounces cache population AddUITask calls.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline std::atomic<bool> g_cachePending{ false };
		// Set once CacheModListFromPapyrus succeeds — prevents repeated VM lookups.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline std::atomic<bool> g_skyUICacheDone{ false };
		// Set once EnsureCachePopulated successfully schedules CacheModListFromPapyrus.
		// Prevents the every-frame settings UI call from re-scheduling on each retry failure.
		// TryCacheFromOpenMCM uses g_cachePending for its own debounce and handles retries.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
		inline std::atomic<bool> g_papyrusEagerScheduled{ false };

		RE::GFxMovieView* GetJournalView()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return nullptr;
			}
			auto menu = ui->GetMenu(RE::JournalMenu::MENU_NAME);
			return menu ? menu->uiMovie.get() : nullptr;
		}

		void AddUITask(std::function<void()> func)
		{
			const auto* taskIface = SKSE::GetTaskInterface();
			if (!taskIface) {
				logger::warn("MCMNavigator: task interface unavailable");
				return;
			}
			taskIface->AddUITask(std::move(func));
		}

		void DelayCallForUI(std::function<void()> func, int framesLeft)
		{
			if (framesLeft <= 0) {
				AddUITask(std::move(func));
				return;
			}
			AddUITask([func = std::move(func), framesLeft]() mutable {
				DelayCallForUI(std::move(func), framesLeft - 1);
			});
		}

		std::vector<std::string> CollectEntryNames(RE::GFxMovieView* view, const std::string& listPath, const char* memberName)
		{
			std::vector<std::string> names;
			if (!view) {
				return names;
			}

			RE::GFxValue listObj;
			view->GetVariable(&listObj, listPath.c_str());
			if (!listObj.IsObject()) {
				return names;
			}

			RE::GFxValue entryList;
			listObj.GetMember("_entryList", &entryList);
			if (!entryList.IsArray()) {
				return names;
			}

			const auto length = entryList.GetArraySize();
			names.reserve(length);
			for (std::uint32_t i = 0; i < length; i++) {
				RE::GFxValue entry;
				entryList.GetElement(i, &entry);
				RE::GFxValue nameVal;
				entry.GetMember(memberName, &nameVal);
				if (nameVal.IsString()) {
					names.emplace_back(nameVal.GetString());
				}
			}
			return names;
		}

		// Uses doSetSelectedIndex + onItemPress to select and click the entry.
		bool SelectEntryByName(const std::string& listPath, const char* varName, std::string_view targetName)
		{
			auto* view = GetJournalView();
			if (!view) {
				return false;
			}

			RE::GFxValue listObj;
			view->GetVariable(&listObj, listPath.c_str());
			if (!listObj.IsObject()) {
				return false;
			}

			RE::GFxValue entryList;
			listObj.GetMember("_entryList", &entryList);
			if (!entryList.IsArray()) {
				return false;
			}

			const auto length = entryList.GetArraySize();
			if (length == 0) {
				logger::debug("MCMNavigator: {} list is empty — consider increasing delay", listPath);
				return false;
			}

			int index = -1;
			for (std::uint32_t i = 0; i < length; i++) {
				RE::GFxValue entry;
				entryList.GetElement(i, &entry);
				RE::GFxValue nameVal;
				entry.GetMember(varName, &nameVal);
				if (!nameVal.IsString()) {
					continue;
				}
				if (CaseInsensitiveEqual(targetName, nameVal.GetString())) {
					index = static_cast<int>(i);
					break;
				}
			}

			if (index < 0) {
				logger::warn("MCMNavigator: '{}' not found in {}", targetName, listPath);
				return false;
			}

			std::array<RE::GFxValue, 2> args{ static_cast<double>(index), 0.0 };
			listObj.Invoke("doSetSelectedIndex", nullptr, args.data(), 2);
			listObj.Invoke("onItemPress", nullptr, args.data(), 2);
			logger::debug("MCMNavigator: selected '{}' at index {} in {}", targetName, index, listPath);
			return true;
		}

		void TransitionToModList()
		{
			auto* view = GetJournalView();
			if (!view) {
				return;
			}
			RE::GFxValue      arg{ 4.0 };  // TRANSITION_TO_LIST state
			const std::string setStatePath = std::string{ kModListPanel } + "setState";
			view->Invoke(setStatePath.c_str(), nullptr, &arg, 1);
		}

		void OpenMod()
		{
			if (g_target.modRetries >= kMaxRetries) {
				logger::warn("MCMNavigator: mod selection retry limit reached");
				g_target.modRetries = 0;
				g_lock = false;
				return;
			}

			auto* view = GetJournalView();
			if (!view) {
				g_lock = false;
				return;
			}

			// Poll disableSelection — list is animating, not ready yet.
			// Also retry if the mod list path doesn't resolve (MCM may still be fading in).
			RE::GFxValue      disabled;
			const std::string disablePath = std::string{ kModList } + "disableSelection";
			view->GetVariable(&disabled, disablePath.c_str());
			if (!disabled.IsBool() || disabled.GetBool()) {
				g_target.modRetries++;
				DelayCallForUI(OpenMod, kModRetryFrames);
				return;
			}

			// List is ready — but _entryList may still be empty if SkyUI hasn't
			// populated the data yet. Retry until we have at least one entry.
			auto mods = CollectEntryNames(view, std::string{ kModList }, "text");
			if (mods.empty()) {
				g_target.modRetries++;
				DelayCallForUI(OpenMod, kModRetryFrames);
				return;
			}

			std::ranges::sort(mods, CaseInsensitiveLess);
			{
				std::scoped_lock lock(g_cacheMutex);
				if (g_modCache.empty()) {
					g_modCache = std::move(mods);
					logger::debug("MCMNavigator: cached {} mod names", g_modCache.size());
				}
			}

			g_target.modRetries = 0;
			if (!SelectEntryByName(kModList, "text", g_target.modName)) {
				logger::warn("MCMNavigator: mod '{}' not found — MCM mod list shown", g_target.modName);
			}
			g_lock = false;
		}
	}

	bool IsMCMOpen()
	{
		auto* view = GetJournalView();
		if (!view) {
			return false;
		}
		RE::GFxValue      alpha;
		const std::string alphaPath = std::string{ kConfigPanel } + "_alpha";
		view->GetVariable(&alpha, alphaPath.c_str());
		return alpha.IsNumber() && alpha.GetNumber() == 100.0;
	}

	bool IsAnyModOpen()
	{
		auto* view = GetJournalView();
		if (!view) {
			return false;
		}
		RE::GFxValue      state;
		const std::string statePath = std::string{ kModListPanel } + "_state";
		view->GetVariable(&state, statePath.c_str());
		return state.IsNumber() && state.GetNumber() == 2.0;
	}

	bool IsModAlreadyOpen(std::string_view modName)
	{
		if (!IsAnyModOpen()) {
			return false;
		}
		auto* view = GetJournalView();
		if (!view) {
			return false;
		}
		RE::GFxValue      titleText;
		const std::string titlePath = std::string{ kModListPanel } + "_titleText";
		view->GetVariable(&titleText, titlePath.c_str());
		return titleText.IsString() && CaseInsensitiveEqual(modName, titleText.GetString());
	}

	void CacheModListFromGFx()
	{
		auto* view = GetJournalView();
		if (!view || !IsMCMOpen()) {
			return;
		}
		auto names = CollectEntryNames(view, std::string{ kModList }, "text");
		std::ranges::sort(names, CaseInsensitiveLess);
		if (names.empty()) {
			return;
		}
		std::scoped_lock lock(g_cacheMutex);
		g_modCache = std::move(names);
	}

	void TryCacheFromOpenMCM()
	{
		if (!IsMCMOpen()) {
			return;
		}
		const auto* taskIface = SKSE::GetTaskInterface();
		if (!taskIface) {
			return;
		}
		// Debounce — only one pending AddUITask at a time.
		if (g_cachePending.exchange(true)) {
			return;
		}
		taskIface->AddUITask([]() {
			if (!IsMCMOpen()) {
				g_cachePending = false;
				return;
			}
			CacheModListFromGFx();
			if (g_skyUICacheDone) {
				g_cachePending = false;
				return;
			}
			// CacheModListFromPapyrus uses Papyrus VM APIs — must run on the game thread.
			const auto* inner = SKSE::GetTaskInterface();
			if (!inner) {
				g_cachePending = false;
				return;
			}
			inner->AddTask(CacheModListFromPapyrus);
		});
	}

	std::vector<std::string> GetCachedModNames()
	{
		std::scoped_lock lock(g_cacheMutex);
		return g_modCache;
	}

	void NavigateToTargetImpl(const std::string& modName)
	{
		if (modName.empty() || modName == "None") {
			return;
		}

		if (g_lock.exchange(true)) {
			logger::debug("MCMNavigator: navigation already in flight — skipping");
			return;
		}

		if (IsAnyModOpen() && !IsModAlreadyOpen(modName)) {
			logger::debug("MCMNavigator: a different mod is open — transitioning to mod list");
			TransitionToModList();
		}

		g_target.modName = modName;
		g_target.modRetries = 0;

		if (IsModAlreadyOpen(modName)) {
			g_lock = false;
		} else if (!SKSE::GetTaskInterface()) {
			logger::warn("MCMNavigator: task interface unavailable — navigation cancelled");
			g_lock = false;
		} else {
			DelayCallForUI(OpenMod, kModRetryFrames);
		}
	}

	void NavigateToTarget(const std::string& modName) noexcept
	{
		try {
			NavigateToTargetImpl(modName);
		} catch (...) {
			g_lock = false;
		}
	}

	void ReadModArraysIntoCache(const RE::BSTSmartPointer<RE::BSScript::Array>& namesArr)
	{
		std::vector<std::string> modNames;
		modNames.reserve(namesArr->size());

		for (RE::BSScript::Array::size_type i = 0; i < namesArr->size(); ++i) {
			const auto& nameElem = (*namesArr)[i];
			if (!nameElem.IsString()) {
				continue;
			}
			const auto modName = nameElem.GetString();
			if (modName.empty()) {
				continue;
			}
			modNames.emplace_back(StripModNamePrefix(modName));
		}

		if (modNames.empty()) {
			return;
		}

		std::ranges::sort(modNames, CaseInsensitiveLess);
		std::scoped_lock lock(g_cacheMutex);
		g_modCache = std::move(modNames);
		logger::info("MCMNavigator: cached {} mods from SKI_ConfigManager", g_modCache.size());
	}

	void CacheModListFromPapyrus()
	{
		if (g_skyUICacheDone) {
			return;
		}
		// Always clear g_cachePending on exit — needed when called via EnsureCachePopulated's
		// AddTask dispatch. Harmless double-clear when called from TryCacheFromOpenMCM's lambda.
		struct ClearPending
		{
			ClearPending() = default;
			ClearPending(const ClearPending&) = default;
			ClearPending(ClearPending&&) = default;
			ClearPending& operator=(const ClearPending&) = default;
			ClearPending& operator=(ClearPending&&) = default;
			~ClearPending() { g_cachePending = false; }
		} clearPending;

		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}

		auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance");
		if (!quest) {
			logger::info("MCMNavigator: SKI_ConfigManagerInstance not found — SkyUI not installed, skipping Papyrus cache");
			g_skyUICacheDone = true;
			return;
		}

		const auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return;
		}

		const auto handle = policy->GetHandleForObject(
			static_cast<RE::VMTypeID>(RE::FormType::Quest), quest);
		if (handle == policy->EmptyHandle()) {
			logger::info("MCMNavigator: could not get VM handle for SKI_ConfigManagerInstance");
			return;
		}

		RE::BSTSmartPointer<RE::BSScript::Object> managerObj;
		if (!vm->FindBoundObject(handle, "SKI_ConfigManager", managerObj) || !managerObj) {
			logger::info("MCMNavigator: SKI_ConfigManager script not yet bound — will retry when MCM opens");
			return;
		}

		const auto* namesVar = managerObj->GetVariable("_modNames");
		if (!namesVar || !namesVar->IsArray()) {
			// MCM Unlocked replaces SKI_ConfigManager and removes _modNames/_modConfigs.
			// DispatchStaticCall only works for native Papyrus functions — MCMUnlocked's
			// GetModName / GetMarkerScript are scripted functions and cannot be called this
			// way. Fall back to the Flash cache (TryCacheFromOpenMCM), which populates mod
			// names the first time the user opens the Journal with MCM Unlocked.
			logger::info("MCMNavigator: MCM Unlocked detected — Flash cache will populate on first MCM open");
			g_skyUICacheDone = true;
			return;
		}

		const auto namesArr = namesVar->GetArray();
		if (!namesArr) {
			return;
		}

		ReadModArraysIntoCache(namesArr);
		g_skyUICacheDone = true;
	}

	void EnsureCachePopulated()
	{
		if (g_skyUICacheDone) {
			return;
		}
		if (g_papyrusEagerScheduled.exchange(true)) {
			return;
		}
		const auto* taskIface = SKSE::GetTaskInterface();
		if (!taskIface) {
			g_papyrusEagerScheduled = false;
			return;
		}
		taskIface->AddTask(CacheModListFromPapyrus);
	}
}
