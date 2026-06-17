#include "PCH.h"

#include "Config.h"
#include "InputHandler.h"
#include "MCMNavigator.h"
#include "MenuUI.h"
#include "SKSEMCP/utils.hpp"
#include "Utils.h"

namespace
{
	bool IsFrameworkInstalled()
	{
		static const bool installed = SKSEMenuFramework::IsInstalled();
		return installed;
	}

	// menuFramework (HMODULE) is declared as a file-scope static in
	// SKSEMenuFramework.hpp (pulled in via SKSEMCP/utils.hpp). Do not
	// redeclare it here — that would shadow the variable used by every
	// GetProcAddress call inside that header.
	bool EnsureFrameworkLoaded()
	{
		if (menuFramework) {
			return true;
		}
		menuFramework = GetModuleHandleW(L"SKSEMenuFramework.dll");
		if (menuFramework) {
			return true;
		}
		menuFramework = LoadLibraryW(LR"(Data\SKSE\Plugins\SKSEMenuFramework.dll)");
		return menuFramework != nullptr;
	}

	bool HasBlockingWindowExport()
	{
		static bool exportResolved = false;
		static bool hasExport = false;

		if (exportResolved) {
			return hasExport;
		}

		// If the framework DLL isn't loaded yet, keep retrying on future calls.
		if (!EnsureFrameworkLoaded()) {
			return false;
		}

		hasExport = GetProcAddress(menuFramework, "IsAnyBlockingWindowOpened") != nullptr;
		exportResolved = true;
		return hasExport;
	}

	struct MenuState
	{
		HoldFast::Config::Settings stagedSettings{};
		bool                       hasPendingChanges{ false };
	};

	MenuState& GetMenuState()
	{
		static MenuState state{};
		return state;
	}

	bool SettingsEqual(const HoldFast::Config::Settings& lhs, const HoldFast::Config::Settings& rhs)
	{
		return lhs.holdDuration == rhs.holdDuration &&
		       lhs.startAction == rhs.startAction &&
		       lhs.backAction == rhs.backAction &&
		       lhs.startMCMModName == rhs.startMCMModName &&
		       lhs.backMCMModName == rhs.backMCMModName &&
		       lhs.startMCMQuickexit == rhs.startMCMQuickexit &&
		       lhs.backMCMQuickexit == rhs.backMCMQuickexit;
	}

	bool DrawActionCombo(const char* label, InputHandler::LongPressAction& value)
	{
		const char* preview = HoldFast::Config::ActionName(value);
		if (!ImGuiMCP::BeginCombo(label, preview)) {
			return false;
		}

		bool changed = false;
		for (const auto& option : HoldFast::Config::kActionOptions) {
			const bool isSelected = (option.action == value);
			if (ImGuiMCP::Selectable(option.name, isSelected)) {
				value = option.action;
				changed = true;
			}
		}
		ImGuiMCP::EndCombo();
		return changed;
	}

	void SavePendingChanges()
	{
		auto& state = GetMenuState();
		if (!state.hasPendingChanges) {
			return;
		}

		state.stagedSettings.holdDuration = HoldFast::ClampHoldDuration(
			state.stagedSettings.holdDuration,
			InputHandler::kDefaultHoldDuration,
			InputHandler::kMinHoldDuration,
			InputHandler::kMaxHoldDuration);

		const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
		const auto  pluginName = plugin ? plugin->GetName() : "HoldFast";

		if (!HoldFast::Config::SaveSettings(state.stagedSettings)) {
			logger::error("{}: failed to persist menu changes", pluginName);
			return;
		}

		HoldFast::Config::ApplySettings(*InputHandler::GetSingleton(), state.stagedSettings);
		logger::info("{}: applied settings from SKSE Menu Framework", pluginName);
		state.hasPendingChanges = false;
	}

	void ReloadFromConfig(bool applyRuntime)
	{
		auto& state = GetMenuState();
		state.stagedSettings = HoldFast::Config::LoadSettings();
		state.hasPendingChanges = false;
		if (applyRuntime) {
			HoldFast::Config::ApplySettings(*InputHandler::GetSingleton(), state.stagedSettings);
		}
	}

	void ResetToDefaults()
	{
		auto&                            state = GetMenuState();
		const HoldFast::Config::Settings defaults{};
		state.stagedSettings = defaults;
		const auto loaded = HoldFast::Config::LoadSettings();
		state.hasPendingChanges = !SettingsEqual(defaults, loaded);
	}

	void DrawMCMTargetInputs(const char* modLabel, std::string& modName, bool& changed)
	{
		MCMNavigator::EnsureCachePopulated();

		const char* preview = modName.empty() || modName == HoldFast::kNoneName ? "None" : modName.c_str();

		if (!ImGuiMCP::BeginCombo(modLabel, preview)) {
			return;
		}

		const auto cachedMods = MCMNavigator::GetCachedModNames();

		const bool noneSelected = modName.empty() || modName == HoldFast::kNoneName;
		if (ImGuiMCP::Selectable("None", noneSelected)) {
			modName = HoldFast::kNoneName;
			changed = true;
		}

		if (cachedMods.empty()) {
			ImGuiMCP::Selectable("(Open MCM once to populate)", false, ImGuiMCP::ImGuiSelectableFlags_Disabled);
			ImGuiMCP::EndCombo();
			return;
		}

		for (const auto& opt : cachedMods) {
			if (ImGuiMCP::Selectable(opt.c_str(), HoldFast::CaseInsensitiveEqual(opt, modName))) {
				modName = opt;
				changed = true;
				break;
			}
		}

		ImGuiMCP::EndCombo();
	}

	void __stdcall RenderSettings()
	{
		try {
			auto& state = GetMenuState();
			bool  changed = false;
			changed |= ImGuiMCP::SliderFloat("Hold duration", &state.stagedSettings.holdDuration, InputHandler::kMinHoldDuration, InputHandler::kMaxHoldDuration, "%.2fs");
			changed |= DrawActionCombo("Start long-press action", state.stagedSettings.startAction);
			if (state.stagedSettings.startAction == InputHandler::LongPressAction::kMCM) {
				DrawMCMTargetInputs(
					"Start MCM mod name",
					state.stagedSettings.startMCMModName,
					changed);
				changed |= ImGuiMCP::Checkbox("Close journal after leaving MCM mod page##start", &state.stagedSettings.startMCMQuickexit);
			}
			changed |= DrawActionCombo("Back long-press action", state.stagedSettings.backAction);
			if (state.stagedSettings.backAction == InputHandler::LongPressAction::kMCM) {
				DrawMCMTargetInputs(
					"Back MCM mod name",
					state.stagedSettings.backMCMModName,
					changed);
				changed |= ImGuiMCP::Checkbox("Close journal after leaving MCM mod page##back", &state.stagedSettings.backMCMQuickexit);
			}

			if (changed) {
				state.hasPendingChanges = true;
			}

			if (ImGuiMCP::Button("Save to config")) {
				SavePendingChanges();
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Reload from config")) {
				ReloadFromConfig(true);
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Reset to defaults")) {
				ResetToDefaults();
			}
		} catch (...) {
			logger::error("RenderSettings: unhandled exception — skipping UI frame");
		}
	}

}

void HoldFastMenuUI::Register()
{
	if (!IsFrameworkInstalled()) {
		logger::info("SKSE Menu Framework not installed — in-game settings menu disabled (INI config still available)");
		return;
	}
	if (!EnsureFrameworkLoaded()) {
		logger::warn("SKSE Menu Framework detected on disk but DLL is not loaded — integration disabled");
		return;
	}
	if (!GetProcAddress(menuFramework, "AddSectionItem") ||
		!GetProcAddress(menuFramework, "IsAnyBlockingWindowOpened")) {
		logger::warn("SKSE Menu Framework required exports are unavailable — integration disabled");
		return;
	}

	ReloadFromConfig(false);
	SKSEMenuFramework::SetSection("HoldFast");
	SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
	logger::info("SKSE Menu Framework integration registered");
}

bool HoldFastMenuUI::IsBlockingInput()
{
	if (!IsFrameworkInstalled()) {
		return false;
	}
	if (!HasBlockingWindowExport()) {
		return false;
	}
	return SKSEMenuFramework::IsAnyBlockingWindowOpened();
}
