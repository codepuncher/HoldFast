#include "PCH.h"

#include "BindingResolver.h"
#include "InputHandler.h"
#include "MCMNavigator.h"
#include "MenuUI.h"

namespace
{
	constexpr auto kGfxCurrentTab = "_root.QuestJournalFader.Menu_mc.iCurrentTab";
	constexpr auto kGfxRestoreSavedSettings = "_root.QuestJournalFader.Menu_mc.RestoreSavedSettings";
	constexpr auto kGfxConfigPanelOpen = "_root.QuestJournalFader.Menu_mc.ConfigPanelOpen";
	constexpr auto kGfxSwitchPageToFront = "_root.QuestJournalFader.Menu_mc.SwitchPageToFront";
	constexpr auto kGfxQuestsFader = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc";
	constexpr auto kGfxQJOEndPage = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.QJO_EndPage";
	constexpr auto kBestiaryMenuName = "BestiaryMenu";
	constexpr auto kCharacterSheetMenuName = "CharacterSheet";
	constexpr auto kQuickLootMenuName = "LootMenu";

	/**
	 * Wall-clock guard for DispatchShortPress, kept separate from kMaxHoldDuration so the
	 * hold threshold and the OS-suspend discard threshold don't conflate.
	 */
	constexpr float kSuspensionGuardDuration = 30.0F;

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

	constexpr std::array<std::string_view, 26> kBlockingMenuNames{
		RE::DialogueMenu::MENU_NAME,
		RE::InventoryMenu::MENU_NAME,
		RE::ContainerMenu::MENU_NAME,
		RE::MagicMenu::MENU_NAME,
		RE::BarterMenu::MENU_NAME,
		RE::GiftMenu::MENU_NAME,
		RE::CraftingMenu::MENU_NAME,
		RE::FavoritesMenu::MENU_NAME,
		RE::BookMenu::MENU_NAME,
		RE::LockpickingMenu::MENU_NAME,
		RE::RaceSexMenu::MENU_NAME,
		RE::TrainingMenu::MENU_NAME,
		RE::TutorialMenu::MENU_NAME,
		RE::MessageBoxMenu::MENU_NAME,
		RE::Console::MENU_NAME,
		RE::ConsoleNativeUIMenu::MENU_NAME,
		RE::StatsMenu::MENU_NAME,
		RE::SleepWaitMenu::MENU_NAME,
		RE::TweenMenu::MENU_NAME,
		RE::ModManagerMenu::MENU_NAME,
		RE::CreationClubMenu::MENU_NAME,
		RE::JournalMenu::MENU_NAME,
		RE::MapMenu::MENU_NAME,
		kCharacterSheetMenuName,
		kBestiaryMenuName,
		kQuickLootMenuName,
	};

	bool IsBlockingMenuOpen(RE::UI* ui)
	{
		return std::ranges::any_of(kBlockingMenuNames, [ui](std::string_view name) {
			return ui->IsMenuOpen(name);
		});
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
			bs.comboModifiers.clear();
		}
		return;
	}

	std::vector<HoldFast::RawMapping> mappings;
	const auto*                       gameplayContext = controlMap->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];
	if (gameplayContext) {
		const auto& deviceMappings = gameplayContext->deviceMappings[RE::INPUT_DEVICE::kGamepad];
		mappings.reserve(deviceMappings.size());
		for (const auto& mapping : deviceMappings) {
			// BSFixedString::c_str() never returns null (falls back to an empty string).
			mappings.push_back({
				.eventID = std::string{ mapping.eventID.c_str() },
				.inputKey = mapping.inputKey,
				.modifier = mapping.modifier,
			});
		}
	} else {
		logger::error("Gameplay input context unavailable — short press will have no effect");
	}

	for (auto& bs : _buttons) {
		const auto keyCode = static_cast<std::uint16_t>(bs.keyCode);
		const auto resolved = HoldFast::ResolveBinding(keyCode, mappings);
		bs.shortPressUserEvent = resolved.soloEvent.c_str();
		bs.comboModifiers = resolved.comboModifiers;

		if (bs.shortPressUserEvent.empty()) {
			logger::warn("{} has no unmodified binding in ControlMap — short press disabled", bs.name);
		} else {
			logger::info("{} short press user event: '{}'", bs.name, bs.shortPressUserEvent);
		}
		if (!bs.comboModifiers.empty()) {
			logger::info("{} is a combo terminal for {} modifier(s) — combo presses pass through", bs.name, bs.comboModifiers.size());
		}
		if (HoldFast::IsUsedAsModifier(keyCode, mappings)) {
			logger::warn("{} is bound as a combo modifier — combos starting with it will not work while HoldFast tracks it", bs.name);
		}
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
		_journalOpenDispatched = false;
		if (_pendingTab.has_value()) {
			const auto tab = *_pendingTab;
			logger::info("Journal opening — switching to tab {}", static_cast<std::uint32_t>(tab));
			/**
			 * Synchronous call: opening=true fires during Skyrim's UI update phase,
			 * not during input polling, so Scaleform calls are safe here.
			 */
			InvokeScaleformTab(tab);
			/**
			 * Reset after invoke. No retry: if uiMovie was unavailable, keeping _pendingTab
			 * set would fire again on the next unrelated Journal open, which is confusing.
			 */
			_pendingTab.reset();
		} else if (_lastKnownTab.has_value()) {
			/**
			 * Counter QJO's forced kSystem override on all Journal opens: restore to the
			 * tab the player was last on (skip until first snapshot fires).
			 */
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

	/**
	 * Fail-safe: if a tab restore is pending but the Journal is not open (or UI singleton
	 * is unavailable), the Journal failed to open or the close event was not delivered:
	 * restore sJournalTabIdx now rather than leaving the forced value in place indefinitely.
	 * Safe to check here: dispatch queues AddMessage for the next frame, so by the time
	 * we receive further input events the Journal must already be open (game paused) or
	 * have never opened. The Journal open case is excluded by IsMenuOpen.
	 * _journalOpenDispatched suppresses this for one frame after dispatch so we don't
	 * restore before the Journal has had a chance to read our forced sJournalTabIdx value.
	 * If the Journal never opens (AddMessage dropped), the flag is cleared here so the
	 * restore still fires on the following frame rather than being suppressed indefinitely.
	 */
	if (_tabRestorePending && (!ui || !ui->IsMenuOpen(RE::JournalMenu::MENU_NAME))) {
		if (_journalOpenDispatched) {
			_journalOpenDispatched = false;
		} else {
			RestoreJournalTab();
		}
	}

	/**
	 * If SKSE Menu Framework owns input focus, pass input through and clear held-state
	 * captures so Start/Back interception cannot fight the settings UI.
	 */
	if (!_buttons.empty() && HoldFastMenuUI::IsBlockingInput()) {
		for (auto& bs : _buttons) {
			bs.pressTime.reset();
			bs.triggered = false;
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	if (ui && (IsBlockingMenuOpen(ui) || ui->GameIsPaused())) {
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

	/**
	 * kStop halts the entire frame's event batch for all downstream sinks. With both Start
	 * and Back tracked by default, this fires on every press of either managed button.
	 * Pressing any other input while a managed button hold is in progress is suppressed from
	 * downstream sinks. This is intentional: hold detection requires exclusive ownership of
	 * those frames. Selective kStop per event is not feasible with CommonLib's batch API.
	 */
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
			break;
		}
	}

	return shouldBlock;
}

/**
 * Queries live gamepad state rather than tracking modifier events, so nothing can go
 * stale across menus or focus changes.
 */
bool InputHandler::IsAnyComboModifierHeld(const ButtonState& state)
{
	if (state.comboModifiers.empty()) {
		return false;
	}
	auto* deviceManager = RE::BSInputDeviceManager::GetSingleton();
	if (!deviceManager) {
		return false;
	}
	auto* gamepad = deviceManager->GetGamepad();
	if (!gamepad) {
		return false;
	}
	return std::ranges::any_of(state.comboModifiers, [gamepad](std::uint16_t modifier) {
		return gamepad->IsPressed(modifier);
	});
}

bool InputHandler::ProcessButton(const RE::ButtonEvent* btn, ButtonState& state)
{
	if (btn->IsDown() && IsAnyComboModifierHeld(state)) {
		// pressTime stays unset so the Held/Up guards below also fall through.
		state.pressTime.reset();
		state.triggered = false;
		return false;
	}

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
			_journalOpenDispatched = true;
			/**
			 * Re-write target tab after menuOpenHandler->ProcessButton() resets sJournalTabIdx internally.
			 * AddMessage is queued for the next frame so the Journal will read our value.
			 * For kMCM, write kSystem (2): MCM is accessed via the System tab.
			 */
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
		/**
		 * sJournalTabIdx unavailable: skip write/restore bookkeeping for the relocation,
		 * but keep _pendingTab set so InvokeScaleformTab still fires on opening=true.
		 * Set _tabRestorePending so the existing fail-safe clears _pendingTab if the Journal
		 * never opens (RestoreJournalTab handles the unavailable relocation gracefully).
		 */
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

/**
 * Snapshot the journal's current tab on every input while the journal is open. The SWF
 * is alive here (game is paused by the journal), and the last snapshot before the player
 * presses close captures the correct final tab, before the SWF is freed.
 * Only needed when QJO is installed; on vanilla, sJournalTabIdx is reliable.
 */
void InputHandler::SnapshotJournalTab(RE::UI* ui)
{
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
	/**
	 * Skip kQuest (0): when QJO is installed the SWF sets iCurrentTab=0 just before calling
	 * CloseMenu to open QJO's quests view. Snapshotting 0 would cause the next Journal open
	 * to restore to that navigation-away state. The player's last meaningful tab is whatever
	 * was captured before the L2/R2 press that triggered the QJO quests view.
	 */
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
		/**
		 * QJO_EndPage closes the Journal and opens QuestMenu (QJO's Quests navigation path).
		 * Falls back to vanilla SwitchPageToFront without QJO.
		 */
		const bool qjoOk = journal->uiMovie->Invoke(kGfxQJOEndPage, nullptr, nullptr, 0);
		logger::info("Journal long press: QJO_EndPage {}", qjoOk ? "ok" : "not found — vanilla fallback");
		if (!qjoOk) {
			RE::GFxValue tabValue{ static_cast<double>(tabIdx) };
			const bool   setOk = journal->uiMovie->SetVariable(kGfxCurrentTab, tabValue);
			if (!setOk) {
				logger::warn("Journal long press: SetVariable(iCurrentTab={}) failed", tabIdx);
			}
			/**
			 * SwitchPageToFront(tabIdx, abForceFade): second arg is abForceFade, not abTabsDisabled.
			 * true forces an immediate tab transition even if a fade is already in progress.
			 */
			std::array<RE::GFxValue, 2> fallback{ static_cast<double>(tabIdx), true /* abForceFade */ };
			journal->uiMovie->Invoke(
				kGfxSwitchPageToFront,
				nullptr, fallback.data(), static_cast<std::uint32_t>(fallback.size()));
		}
		return;
	}

	/**
	 * RestoreSavedSettings(tabIdx, abTabsDisabled): second arg is abTabsDisabled, not abForceFade.
	 * false means tabs are enabled (interactive), which is the normal state.
	 * This function atomically sets the active tab, updates the tab bar highlight,
	 * and fires onTabChange → startPage() to populate page data.
	 */
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

	if (_pendingMCMModName.empty() || _pendingMCMModName == HoldFast::kNoneName) {
		return;
	}
	const auto* taskIface = SKSE::GetTaskInterface();
	if (!taskIface) {
		return;
	}
	std::string modName = std::move(_pendingMCMModName);
	_pendingMCMModName.clear();
	taskIface->AddUITask([mn = std::move(modName)]() noexcept {
		MCMNavigator::NavigateToTarget(mn);
	});
}

/**
 * Only needed when QJO is installed: QJO unconditionally forces sJournalTabIdx to
 * kSystem on every Journal open, making sJournalTabIdx unreliable for tab tracking.
 * On vanilla, sJournalTabIdx is the authoritative tab selection; no Scaleform call needed.
 */
void InputHandler::InvokeRestoreTabIfNeeded(JournalTab tab)
{
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
	/**
	 * Guard: only probe when the Quests page SWF is actually instantiated. On a non-Quests
	 * tab the page may not be loaded yet: GetVariable would return undefined and cache a
	 * false-negative, permanently suppressing QJO navigation for the session.
	 */
	RE::GFxValue questsPage;
	if (!movie->GetVariable(&questsPage, kGfxQuestsFader) || !questsPage.IsObject()) {
		return;
	}
	/**
	 * Probe for a QJO-specific function in the Quests page SWF. QJO_EndPage is defined by
	 * QJO and absent in vanilla; GetVariable returns undefined (or fails) without QJO.
	 */
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
	ResetMCMQuickexitState();
	CloseJournal();
}

void InputHandler::DispatchShortPress(const ButtonState& state, float held)
{
	/**
	 * Best-effort guard against stale pressTime from OS suspension (e.g. Alt-Tab): wall-clock
	 * time accumulates while game time freezes, so held can be arbitrarily large after resume.
	 * kSuspensionGuardDuration (30 s) is a sanity sentinel unrelated to kMaxHoldDuration (the
	 * user-facing hold clamp); keeping them separate makes each threshold's purpose explicit.
	 */
	if (held > kSuspensionGuardDuration) {
		logger::warn("{} press duration {:.1f}s exceeds sanity limit — discarded", state.name, held);
		return;
	}

	auto* ui = RE::UI::GetSingleton();
	if (ui && ui->GameIsPaused()) {
		logger::debug("{} short press discarded — game paused at dispatch", state.name);
		return;
	}

	if (state.shortPressUserEvent.empty()) {
		logger::warn("{} short press has no binding — press consumed but no menu opened", state.name);
		return;
	}

	const std::string logCtx = state.name + " short press";

	// menuOpenHandler rejects Favorites and QuickSave/QuickLoad/NewSave; route to their own handlers.
	const auto* userEvents = RE::UserEvents::GetSingleton();
	if (userEvents && state.shortPressUserEvent == userEvents->favorites) {
		DispatchViaFavoritesHandler(state.shortPressUserEvent, state.keyCode, logCtx);
		return;
	}
	constexpr std::array quickSaveLoadEvents{ &RE::UserEvents::quicksave, &RE::UserEvents::quickload, &RE::UserEvents::newSave };
	if (userEvents && std::ranges::any_of(quickSaveLoadEvents, [&](auto member) { return state.shortPressUserEvent == userEvents->*member; })) {
		DispatchViaQuickSaveLoadHandler(state.shortPressUserEvent, state.keyCode, logCtx);
		return;
	}

	DispatchViaMenuOpenHandler(state.shortPressUserEvent, state.keyCode, logCtx);
}
