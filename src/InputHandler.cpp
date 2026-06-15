#include "PCH.h"

#include "InputHandler.h"
#include "MCMNavigator.h"
#include "MenuUI.h"

namespace
{
	constexpr auto kGfxCurrentTab = "_root.QuestJournalFader.Menu_mc.iCurrentTab";
	constexpr auto kGfxRestoreSavedSettings = "_root.QuestJournalFader.Menu_mc.RestoreSavedSettings";
	constexpr auto kGfxConfigPanelOpen = "_root.QuestJournalFader.Menu_mc.ConfigPanelOpen";
	constexpr auto kGfxSwitchPageToFront = "_root.QuestJournalFader.Menu_mc.SwitchPageToFront";
	constexpr auto kGfxQJOEndPage = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.QJO_EndPage";
	constexpr auto kBestiaryMenuName = "BestiaryMenu";
	constexpr auto kCharacterSheetMenuName = "CharacterSheet";

	std::optional<std::string_view> GetDirectOpenMenuName(InputHandler::LongPressAction action)
	{
		switch (action) {
		case InputHandler::LongPressAction::kMap:
			return RE::MapMenu::MENU_NAME;
		case InputHandler::LongPressAction::kMagic:
			return RE::MagicMenu::MENU_NAME;
		case InputHandler::LongPressAction::kInventory:
			return RE::InventoryMenu::MENU_NAME;
		case InputHandler::LongPressAction::kBestiary:
			return kBestiaryMenuName;
		case InputHandler::LongPressAction::kCharacterSheet:
			return kCharacterSheetMenuName;
		default:
			return std::nullopt;
		}
	}
}

InputHandler* InputHandler::GetSingleton()
{
	static InputHandler instance;
	return &instance;
}

void InputHandler::SetButtons(std::vector<ButtonConfig> a_configs)
{
	_buttons.clear();
	_buttons.reserve(a_configs.size());
	for (auto& cfg : a_configs) {
		ButtonState state;
		static_cast<ButtonConfig&>(state) = std::move(cfg);
		_buttons.push_back(std::move(state));
	}
}

void InputHandler::UpdateShortPressBinding()
{
	auto* controlMap = RE::ControlMap::GetSingleton();
	if (!controlMap) {
		logger::error("ControlMap unavailable — short press will have no effect");
		for (auto& bs : _buttons) {
			bs.shortPressUserEvent = "";
		}
		return;
	}

	for (auto& bs : _buttons) {
		bs.shortPressUserEvent = controlMap->GetUserEventName(bs.keyCode, RE::INPUT_DEVICE::kGamepad);
		if (bs.shortPressUserEvent.empty()) {
			logger::warn("{} has no binding in ControlMap — short press disabled", bs.name);
			continue;
		}
		logger::info("{} short press user event: '{}'", bs.name, bs.shortPressUserEvent);
	}
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	const RE::MenuOpenCloseEvent* a_event,
	RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_eventSource*/)
{
	if (!a_event || a_event->menuName != RE::JournalMenu::MENU_NAME) {
		return RE::BSEventNotifyControl::kContinue;
	}

	if (a_event->opening) {
		if (_pendingTab.has_value()) {
			const auto tab = *_pendingTab;
			logger::info("Journal opening — switching to tab {}", static_cast<std::uint32_t>(tab));
			// Synchronous call — opening=true fires during Skyrim's UI update phase,
			// not during input polling, so Scaleform calls are safe here.
			InvokeScaleformTab(tab);
			// Reset after invoke. No retry: if uiMovie was unavailable, keeping _pendingTab
			// set would fire again on the next unrelated Journal open, which is confusing.
			_pendingTab.reset();
		} else if (_lastKnownTab.has_value()) {
			// Counter QJO's forced kSystem override on all Journal opens —
			// restore to the tab the player was last on (skip until first snapshot fires).
			InvokeRestoreTabIfNeeded(*_lastKnownTab);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	// Journal closed.
	if (_tabRestorePending) {
		RestoreJournalTab();
	}
	ResetMCMQuickexitState();
	UpdateShortPressBinding();
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	RE::InputEvent* const* a_events,
	RE::BSTEventSource<RE::InputEvent*>* /*a_eventSource*/)
{
	if (!a_events) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto* ui = RE::UI::GetSingleton();

	// Fail-safe: if a tab restore is pending but the Journal is not open (or UI singleton
	// is unavailable), the Journal failed to open or the close event was not delivered —
	// restore sJournalTabIdx now rather than leaving the forced value in place indefinitely.
	// Safe to check here: dispatch queues AddMessage for the next frame, so by the time
	// we receive further input events the Journal must already be open (game paused) or
	// have never opened. The Journal open case is excluded by IsMenuOpen.
	if (_tabRestorePending && (!ui || !ui->IsMenuOpen(RE::JournalMenu::MENU_NAME))) {
		RestoreJournalTab();
	}

	// If SKSE Menu Framework owns input focus, pass input through and clear held-state
	// captures so Start/Back interception cannot fight the settings UI.
	if (!_buttons.empty() && HoldFastMenuUI::IsBlockingInput()) {
		for (auto& bs : _buttons) {
			bs.pressTime.reset();
			bs.triggered = false;
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	// If any pausing menu is open, pass all input through and clear any captured press so it
	// can't fire a spurious dispatch once the menu closes.
	// Also pass through for kCharacterSheet: it uses kModal but not kPausesGame, so
	// GameIsPaused() stays false while it is open. Without this guard, HoldFast would keep
	// consuming Start/Back and could attempt to dispatch another action on top of it.
	if (ui && (ui->GameIsPaused() || ui->IsMenuOpen(kCharacterSheetMenuName))) {
		if (ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
			SnapshotJournalTab(ui);
			MCMNavigator::TryCacheFromOpenMCM();
			HandleMCMQuickexit();
		}
		for (auto& bs : _buttons) {
			bs.pressTime.reset();
			bs.triggered = false;
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	// kStop halts the entire frame's event batch for all downstream sinks. With both Start
	// and Back tracked by default, this fires on every press of either managed button.
	// Pressing any other input while a managed button hold is in progress is suppressed from
	// downstream sinks. This is intentional: hold detection requires exclusive ownership of
	// those frames. Selective kStop per event is not feasible with CommonLib's batch API.
	return ScanInputEvents(a_events) ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
}

bool InputHandler::ScanInputEvents(RE::InputEvent* const* a_events)
{
	bool shouldBlock = false;

	for (auto* event = *a_events; event; event = event->next) {
		const auto* btn = event->AsButtonEvent();
		if (!btn) {
			continue;
		}
		if (btn->GetDevice() != RE::INPUT_DEVICE::kGamepad) {
			continue;
		}
		for (auto& bs : _buttons) {
			if (btn->GetIDCode() != bs.keyCode) {
				continue;
			}
			if (ProcessButton(btn, bs)) {
				shouldBlock = true;
			}
		}
	}

	return shouldBlock;
}

bool InputHandler::ProcessButton(const RE::ButtonEvent* btn, ButtonState& state)
{
	if (btn->IsDown()) {
		state.pressTime = std::chrono::steady_clock::now();
		state.triggered = false;
		return true;
	}

	if (btn->IsHeld() && state.pressTime) {
		if (!state.triggered && btn->HeldDuration() >= holdDuration) {
			state.triggered = true;
			DispatchLongPress(state);
		}
		return true;
	}

	if (btn->IsUp() && state.pressTime) {
		if (!state.triggered) {
			const auto held = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - *state.pressTime)
			                      .count();
			DispatchShortPress(state, held);
		}
		state.triggered = false;
		state.pressTime.reset();
		return true;
	}

	return false;
}

void InputHandler::DispatchLongPress(const ButtonState& state)
{
	const std::string logCtx = state.name + " long press";

	if (state.action == LongPressAction::kNewSave) {
		logger::info("{}: dispatching NewSave", logCtx);
		auto* saveLoadManager = RE::BGSSaveLoadManager::GetSingleton();
		if (!saveLoadManager) {
			logger::error("{}: BGSSaveLoadManager unavailable — action not dispatched", logCtx);
			return;
		}
		RE::SendHUDMessage::ShowHUDMessage("Saving...");
		saveLoadManager->Save(nullptr);
		return;
	}

	if (const auto menuName = GetDirectOpenMenuName(state.action)) {
		auto* uiQueue = RE::UIMessageQueue::GetSingleton();
		if (!uiQueue) {
			logger::error("{}: UIMessageQueue unavailable — action not dispatched", logCtx);
			return;
		}
		logger::info("{}: opening {}", logCtx, *menuName);
		uiQueue->AddMessage(*menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		return;
	}

	auto* userEvents = RE::UserEvents::GetSingleton();
	if (!userEvents) {
		logger::error("{}: UserEvents unavailable — action not dispatched", logCtx);
		return;
	}

	switch (state.action) {
	case LongPressAction::kTweenMenu:
		logger::info("{}: opening Tween Menu", logCtx);
		DispatchViaMenuOpenHandler(userEvents->tweenMenu, state.keyCode, logCtx);
		return;
	case LongPressAction::kWait:
		logger::info("{}: opening Sleep/Wait", logCtx);
		DispatchViaMenuOpenHandler(userEvents->wait, state.keyCode, logCtx);
		return;
	case LongPressAction::kQuickSave:
		logger::info("{}: dispatching QuickSave", logCtx);
		DispatchViaQuickSaveLoadHandler(userEvents->quicksave, state.keyCode, logCtx);
		return;
	case LongPressAction::kFavorites:
		logger::info("{}: opening Favorites", logCtx);
		DispatchViaFavoritesHandler(userEvents->favorites, state.keyCode, logCtx);
		return;
	case LongPressAction::kQuests:
	case LongPressAction::kSystem:
	case LongPressAction::kStats:
	case LongPressAction::kMCM:
		{
			logger::info("{}: opening Journal", logCtx);
			JournalTab targetTab = JournalTab::kQuest;
			if (state.action == LongPressAction::kSystem) {
				targetTab = JournalTab::kSystem;
			} else if (state.action == LongPressAction::kStats) {
				targetTab = JournalTab::kStats;
			} else if (state.action == LongPressAction::kMCM) {
				targetTab = JournalTab::kMCM;
				_pendingMCMModName = state.mcmModName;
				_mcmQuickexit = state.mcmQuickexit;
				_mcmWasOpen = false;
				_mcmModPageSeen = false;
			}
			OpenJournalOnTab(targetTab, state.name);
			if (!DispatchViaMenuOpenHandler(userEvents->journal, state.keyCode, logCtx)) {
				RestoreJournalTab();
				return;
			}
			// Re-write target tab after menuOpenHandler->ProcessButton() resets sJournalTabIdx internally.
			// AddMessage is queued for the next frame so the Journal will read our value.
			// For kMCM, write kSystem (2) — MCM is accessed via the System tab.
			if (sJournalTabIdx.get()) {
				*sJournalTabIdx = JournalTabToIndex(targetTab);
			}
			return;
		}
	default:
		return;
	}
}

bool InputHandler::DispatchViaMenuOpenHandler(
	const RE::BSFixedString& userEvent,
	std::uint32_t            keyCode,
	const std::string&       logContext)
{
	auto* menuControls = RE::MenuControls::GetSingleton();
	if (!menuControls) {
		logger::error("{}: MenuControls unavailable — action not dispatched", logContext);
		return false;
	}
	if (!menuControls->menuOpenHandler) {
		logger::error("{}: menuOpenHandler unavailable — action not dispatched", logContext);
		return false;
	}
	return DispatchViaHandler(menuControls->menuOpenHandler, "menuOpenHandler", userEvent, keyCode, logContext);
}

bool InputHandler::DispatchViaQuickSaveLoadHandler(
	const RE::BSFixedString& userEvent,
	std::uint32_t            keyCode,
	const std::string&       logContext)
{
	auto* menuControls = RE::MenuControls::GetSingleton();
	if (!menuControls) {
		logger::error("{}: MenuControls unavailable — action not dispatched", logContext);
		return false;
	}
	if (!menuControls->quickSaveLoadHandler) {
		logger::error("{}: quickSaveLoadHandler unavailable — action not dispatched", logContext);
		return false;
	}
	return DispatchViaHandler(menuControls->quickSaveLoadHandler, "quickSaveLoadHandler", userEvent, keyCode, logContext);
}

bool InputHandler::DispatchViaFavoritesHandler(
	const RE::BSFixedString& userEvent,
	std::uint32_t            keyCode,
	const std::string&       logContext)
{
	auto* menuControls = RE::MenuControls::GetSingleton();
	if (!menuControls) {
		logger::error("{}: MenuControls unavailable — action not dispatched", logContext);
		return false;
	}
	if (!menuControls->favoritesHandler) {
		logger::error("{}: favoritesHandler unavailable — action not dispatched", logContext);
		return false;
	}
	return DispatchViaHandler(menuControls->favoritesHandler, "favoritesHandler", userEvent, keyCode, logContext);
}

bool InputHandler::DispatchViaHandler(
	RE::MenuEventHandler*    handler,
	std::string_view         handlerName,
	const RE::BSFixedString& userEvent,
	std::uint32_t            keyCode,
	const std::string&       logContext)
{
	auto deleter = [](RE::ButtonEvent* e) {
		e->~ButtonEvent();
		RE::free(e);
	};
	std::unique_ptr<RE::ButtonEvent, decltype(deleter)> syntheticEvent{
		RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, userEvent, keyCode, 1.0F, 0.0F),
		deleter
	};
	if (!syntheticEvent) {
		logger::error("{}: failed to allocate synthetic ButtonEvent — action not dispatched", logContext);
		return false;
	}

	if (!handler->CanProcess(syntheticEvent.get())) {
		logger::warn("{}: {} rejected event — action not dispatched", logContext, handlerName);
		return false;
	}

	handler->ProcessButton(syntheticEvent.get());
	return true;
}

std::uint32_t InputHandler::JournalTabToIndex(JournalTab tab)
{
	return tab == JournalTab::kMCM ?
	           static_cast<std::uint32_t>(JournalTab::kSystem) :
	           static_cast<std::uint32_t>(tab);
}

void InputHandler::CloseJournal()
{
	auto* uiQueue = RE::UIMessageQueue::GetSingleton();
	if (!uiQueue) {
		return;
	}
	uiQueue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
}

void InputHandler::OpenJournalOnTab(JournalTab tab, const std::string& buttonName)
{
	_pendingTab = tab;

	const auto sJournalValue = JournalTabToIndex(tab);

	if (!sJournalTabIdx.get()) {
		// sJournalTabIdx unavailable — skip write/restore bookkeeping for the relocation,
		// but keep _pendingTab set so InvokeScaleformTab still fires on opening=true.
		// Set _tabRestorePending so the existing fail-safe clears _pendingTab if the Journal
		// never opens (RestoreJournalTab handles the unavailable relocation gracefully).
		logger::warn("{} long press: sJournalTabIdx unavailable — skipping tab index bookkeeping", buttonName);
		_tabRestorePending = true;
		return;
	}
	if (!_tabRestorePending) {
		_savedTabIdx = static_cast<JournalTab>(*sJournalTabIdx);
	}
	_tabRestorePending = true;
	*sJournalTabIdx = sJournalValue;
}

void InputHandler::RestoreJournalTab()
{
	if (sJournalTabIdx.get()) {
		*sJournalTabIdx = static_cast<std::uint32_t>(_savedTabIdx);
	}
	_tabRestorePending = false;
	_pendingTab.reset();
	ResetMCMQuickexitState();
}

void InputHandler::SnapshotJournalTab(RE::UI* ui)
{
	// Snapshot the journal's current tab on every input while the journal is open. The SWF
	// is alive here (game is paused by the journal), and the last snapshot before the player
	// presses close captures the correct final tab — before the SWF is freed.
	// Only needed when QJO is installed; on vanilla, sJournalTabIdx is reliable.

	// Fast path: once detection has confirmed QJO is not installed, skip the GetMenu lookup.
	if (_qjoInstalled == false) {
		return;
	}
	auto j = ui->GetMenu(RE::JournalMenu::MENU_NAME);
	if (!j || !j->uiMovie) {
		return;
	}
	DetectQJOIfNeeded(j->uiMovie.get());
	if (!_qjoInstalled.value_or(false)) {
		return;
	}
	RE::GFxValue tv;
	if (!j->uiMovie->GetVariable(&tv, kGfxCurrentTab) ||
		tv.GetType() != RE::GFxValue::ValueType::kNumber) {
		return;
	}
	const auto num = tv.GetNumber();
	if (!std::isfinite(num) || num < 0.0 || num > static_cast<double>(JournalTab::kSystem)) {
		return;
	}
	const auto captured = static_cast<JournalTab>(static_cast<std::uint32_t>(num));
	// Skip kQuest (0): when QJO is installed the SWF sets iCurrentTab=0 just before calling
	// CloseMenu to open QJO's quests view. Snapshotting 0 would cause the next Journal open
	// to restore to that navigation-away state. The player's last meaningful tab is whatever
	// was captured before the L2/R2 press that triggered the QJO quests view.
	if (captured != JournalTab::kQuest) {
		_lastKnownTab = captured;
	}
}

void InputHandler::InvokeScaleformTab(JournalTab tab)
{
	const auto tabIdx = JournalTabToIndex(tab);

	auto* ui = RE::UI::GetSingleton();
	if (!ui) {
		logger::warn("Journal long press: UI unavailable for Scaleform call");
		return;
	}
	auto journal = ui->GetMenu(RE::JournalMenu::MENU_NAME);
	if (!journal || !journal->uiMovie) {
		logger::warn("Journal long press: uiMovie unavailable for Scaleform call");
		return;
	}

	if (tab == JournalTab::kQuest) {
		// QJO_EndPage closes the Journal and opens QuestMenu (QJO's Quests navigation path).
		// Falls back to vanilla SwitchPageToFront without QJO.
		const bool qjoOk = journal->uiMovie->Invoke(kGfxQJOEndPage, nullptr, nullptr, 0);
		logger::info("Journal long press: QJO_EndPage {}", qjoOk ? "ok" : "not found — vanilla fallback");
		if (!qjoOk) {
			RE::GFxValue tabValue{ static_cast<double>(tabIdx) };
			const bool   setOk = journal->uiMovie->SetVariable(kGfxCurrentTab, tabValue);
			if (!setOk) {
				logger::warn("Journal long press: SetVariable(iCurrentTab={}) failed", tabIdx);
			}
			// SwitchPageToFront(tabIdx, abForceFade) — second arg is abForceFade, not abTabsDisabled.
			// true forces an immediate tab transition even if a fade is already in progress.
			std::array<RE::GFxValue, 2> fallback{ static_cast<double>(tabIdx), true /* abForceFade */ };
			journal->uiMovie->Invoke(
				kGfxSwitchPageToFront,
				nullptr, fallback.data(), static_cast<std::uint32_t>(fallback.size()));
		}
		return;
	}

	// RestoreSavedSettings(tabIdx, abTabsDisabled) — second arg is abTabsDisabled, not abForceFade.
	// false means tabs are enabled (interactive), which is the normal state.
	// This function atomically sets the active tab, updates the tab bar highlight,
	// and fires onTabChange → startPage() to populate page data.
	std::array<RE::GFxValue, 2> args{ static_cast<double>(tabIdx), false /* abTabsDisabled */ };
	const bool                  ok = journal->uiMovie->Invoke(
		kGfxRestoreSavedSettings,
		nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
	logger::info("Journal long press: RestoreSavedSettings({}) {}", tabIdx, ok ? "ok" : "FAIL");

	if (tab != JournalTab::kMCM) {
		return;
	}

	const bool cpOk = journal->uiMovie->Invoke(kGfxConfigPanelOpen, nullptr, nullptr, 0);
	if (!cpOk) {
		logger::info("Journal long press: ConfigPanelOpen not found — SkyUI may not be installed");
		return;
	}

	if (_pendingMCMModName.empty() || _pendingMCMModName == "None") {
		return;
	}
	std::string modName = std::move(_pendingMCMModName);
	_pendingMCMModName.clear();
	const auto* taskIface = SKSE::GetTaskInterface();
	if (!taskIface) {
		return;
	}
	taskIface->AddUITask([mn = std::move(modName)]() noexcept {
		MCMNavigator::NavigateToTarget(mn);
	});
}

void InputHandler::InvokeRestoreTabIfNeeded(JournalTab tab)
{
	// Only needed when QJO is installed — QJO unconditionally forces sJournalTabIdx to
	// kSystem on every Journal open, making sJournalTabIdx unreliable for tab tracking.
	// On vanilla, sJournalTabIdx is the authoritative tab selection; no Scaleform call needed.
	const auto tabIdx = static_cast<std::uint32_t>(tab);

	auto* ui = RE::UI::GetSingleton();
	if (!ui) {
		logger::warn("QJO tab restore: UI unavailable");
		return;
	}
	auto journal = ui->GetMenu(RE::JournalMenu::MENU_NAME);
	if (!journal || !journal->uiMovie) {
		logger::warn("QJO tab restore: uiMovie unavailable");
		return;
	}

	DetectQJOIfNeeded(journal->uiMovie.get());
	if (!_qjoInstalled.value_or(false)) {
		return;
	}

	RE::GFxValue current;
	const bool   got = journal->uiMovie->GetVariable(&current, kGfxCurrentTab);
	if (!got || current.GetType() != RE::GFxValue::ValueType::kNumber) {
		logger::warn("QJO tab restore: could not read iCurrentTab");
		return;
	}
	const auto currentNum = current.GetNumber();
	if (!std::isfinite(currentNum) || currentNum < 0.0 || currentNum > static_cast<double>(JournalTab::kSystem)) {
		logger::warn("QJO tab restore: iCurrentTab value is not a valid number");
		return;
	}
	const auto currentIdx = static_cast<std::uint32_t>(currentNum);
	if (currentIdx == tabIdx) {
		return;
	}

	logger::info("QJO tab restore: current={} expected={} — calling RestoreSavedSettings", currentIdx, tabIdx);
	std::array<RE::GFxValue, 2> args{ static_cast<double>(tabIdx), false /* abTabsDisabled */ };
	const bool                  ok = journal->uiMovie->Invoke(
		kGfxRestoreSavedSettings,
		nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
	if (!ok) {
		logger::warn("QJO tab restore: RestoreSavedSettings({}) FAIL", tabIdx);
	}
}

void InputHandler::DetectQJOIfNeeded(RE::GFxMovieView* movie)
{
	if (_qjoInstalled.has_value() || !movie) {
		return;
	}
	// Probe for a QJO-specific function in the Quests page SWF. QJO_EndPage is defined by
	// QJO and absent in vanilla — GetVariable returns undefined (or fails) without QJO.
	RE::GFxValue result;
	const bool   found = movie->GetVariable(&result, kGfxQJOEndPage);
	_qjoInstalled = found && result.GetType() != RE::GFxValue::ValueType::kUndefined;
	logger::info("QJO detection: {}", *_qjoInstalled ? "QJO installed" : "vanilla Journal");
}

void InputHandler::ResetMCMQuickexitState()
{
	_mcmQuickexit = false;
	_mcmWasOpen = false;
	_mcmModPageSeen = false;
}

void InputHandler::HandleMCMQuickexit()
{
	if (!_mcmQuickexit) {
		return;
	}

	if (!_mcmWasOpen) {
		_mcmWasOpen = MCMNavigator::IsMCMOpen();
		return;
	}

	const bool modOpen = MCMNavigator::IsAnyModOpen();

	if (!_mcmModPageSeen) {
		if (modOpen) {
			_mcmModPageSeen = true;
			return;
		}
		if (MCMNavigator::IsMCMOpen()) {
			return;
		}
		ResetMCMQuickexitState();
		CloseJournal();
		return;
	}

	if (modOpen) {
		return;
	}
	if (MCMNavigator::IsMCMOpen()) {
		return;
	}
	ResetMCMQuickexitState();
	CloseJournal();
}

void InputHandler::DispatchShortPress(const ButtonState& state, float held)
{
	// Best-effort guard against stale pressTime from process suspension (e.g. Alt-Tab).
	// Any legitimate short press has held < holdDuration <= kMaxHoldDuration, so this only
	// fires when pressTime accumulated wall-clock time during suspension while game time
	// was frozen. Known limitation: suspensions shorter than kMaxHoldDuration seconds may
	// still dispatch a spurious short press.
	if (held > kMaxHoldDuration) {
		logger::warn("{} press duration {:.1f}s exceeds sanity limit — discarded", state.name, held);
		return;
	}

	if (state.shortPressUserEvent.empty()) {
		logger::warn("{} short press has no binding — press consumed but no menu opened", state.name);
		return;
	}

	DispatchViaMenuOpenHandler(state.shortPressUserEvent, state.keyCode, state.name + " short press");
}
