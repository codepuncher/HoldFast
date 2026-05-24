#include "PCH.h"

#include "InputHandler.h"

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
		DetectQJOIfNeeded();
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

	// Journal closed. _lastKnownTab was last written by the input snapshot (GameIsPaused
	// block) which runs on every input while the journal is open — including the button
	// press that triggers close. No reliable data is available here (uiMovie is null and
	// sJournalTabIdx is always kSystem due to QJO's Hook_ProcessMessage).
	if (_tabRestorePending) {
		RestoreJournalTab();
	}
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

	// If any pausing menu is open, pass all input through and clear any captured press so it
	// can't fire a spurious dispatch once the menu closes.
	if (ui && ui->GameIsPaused()) {
		SnapshotJournalTab(ui);
		for (auto& bs : _buttons) {
			bs.pressTime.reset();
			bs.triggered = false;
		}
		return RE::BSEventNotifyControl::kContinue;
	}

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

	// kStop halts the entire frame's event batch for all downstream sinks. With both Start
	// and Back tracked by default, this fires on every press of either managed button.
	// Pressing any other input while a managed button hold is in progress is suppressed from
	// downstream sinks. This is intentional: hold detection requires exclusive ownership of
	// those frames. Selective kStop per event is not feasible with CommonLib's batch API.
	return shouldBlock ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
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

	// UIMessageQueue direct-open actions — no gamepad UserEvent path exists for these.
	// InventoryMenu (context=kNone) and MagicMenu (context=kItemMenu) are not handled by
	// menuOpenHandler in the Gameplay context; direct AddMessage is the only viable path.
	switch (state.action) {
	case LongPressAction::kMap:
	case LongPressAction::kMagic:
	case LongPressAction::kInventory:
		{
			auto* uiQueue = RE::UIMessageQueue::GetSingleton();
			if (!uiQueue) {
				logger::error("{}: UIMessageQueue unavailable — action not dispatched", logCtx);
				return;
			}
			std::string_view menuName;
			switch (state.action) {
			case LongPressAction::kMap:
				menuName = RE::MapMenu::MENU_NAME;
				break;
			case LongPressAction::kMagic:
				menuName = RE::MagicMenu::MENU_NAME;
				break;
			default:
				menuName = RE::InventoryMenu::MENU_NAME;
				break;
			}
			logger::info("{}: opening {}", logCtx, menuName);
			uiQueue->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
			return;
		}
	default:
		break;
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

	case LongPressAction::kFavorites:
		logger::info("{}: opening Favorites", logCtx);
		DispatchViaFavoritesHandler(userEvents->favorites, state.keyCode, logCtx);
		return;

	case LongPressAction::kQuests:
	case LongPressAction::kSystem:
	case LongPressAction::kStats:
		{
			logger::info("{}: opening Journal", logCtx);
			JournalTab targetTab = JournalTab::kQuest;
			switch (state.action) {
			case LongPressAction::kSystem:
				targetTab = JournalTab::kSystem;
				break;
			case LongPressAction::kStats:
				targetTab = JournalTab::kStats;
				break;
			default:
				break;  // kQuests: kQuest(0)
			}
			OpenJournalOnTab(targetTab, state.name);
			if (!DispatchViaMenuOpenHandler(userEvents->journal, state.keyCode, logCtx)) {
				RestoreJournalTab();
				return;
			}
			// Re-write target tab after menuOpenHandler->ProcessButton() resets sJournalTabIdx internally.
			// AddMessage is queued for the next frame so the Journal will read our value.
			if (sJournalTabIdx.get()) {
				*sJournalTabIdx = static_cast<std::uint32_t>(targetTab);
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

void InputHandler::OpenJournalOnTab(JournalTab tab, const std::string& buttonName)
{
	_pendingTab = tab;

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
	*sJournalTabIdx = static_cast<std::uint32_t>(tab);
}

void InputHandler::RestoreJournalTab()
{
	if (sJournalTabIdx.get()) {
		*sJournalTabIdx = static_cast<std::uint32_t>(_savedTabIdx);
	}
	_tabRestorePending = false;
	_pendingTab.reset();
}

void InputHandler::SnapshotJournalTab(RE::UI* ui)
{
	// Snapshot the journal's current tab on every input while the journal is open. The SWF
	// is alive here (game is paused by the journal), and the last snapshot before the player
	// presses close captures the correct final tab — before the SWF is freed.
	// Only needed when QJO is installed; on vanilla, sJournalTabIdx is reliable.
	DetectQJOIfNeeded();
	if (!_qjoInstalled.value_or(false)) {
		return;
	}
	auto j = ui->GetMenu(RE::JournalMenu::MENU_NAME);
	if (!j || !j->uiMovie) {
		return;
	}
	RE::GFxValue tv;
	if (!j->uiMovie->GetVariable(&tv, "_root.QuestJournalFader.Menu_mc.iCurrentTab") ||
		tv.GetType() != RE::GFxValue::ValueType::kNumber) {
		return;
	}
	const auto captured = static_cast<JournalTab>(static_cast<std::uint32_t>(tv.GetNumber()));
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
	const auto tabIdx = static_cast<std::uint32_t>(tab);

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
		const bool qjoOk = journal->uiMovie->Invoke(
			"_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.QJO_EndPage",
			nullptr, nullptr, 0);
		logger::info("Journal long press: QJO_EndPage {}", qjoOk ? "ok" : "not found — vanilla fallback");
		if (!qjoOk) {
			RE::GFxValue tabValue{ static_cast<double>(tabIdx) };
			const bool   setOk = journal->uiMovie->SetVariable("_root.QuestJournalFader.Menu_mc.iCurrentTab", tabValue);
			if (!setOk) {
				logger::warn("Journal long press: SetVariable(iCurrentTab={}) failed", tabIdx);
			}
			// SwitchPageToFront(tabIdx, abForceFade) — second arg is abForceFade, not abTabsDisabled.
			// true forces an immediate tab transition even if a fade is already in progress.
			std::array<RE::GFxValue, 2> fallback{ static_cast<double>(tabIdx), true /* abForceFade */ };
			journal->uiMovie->Invoke(
				"_root.QuestJournalFader.Menu_mc.SwitchPageToFront",
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
		"_root.QuestJournalFader.Menu_mc.RestoreSavedSettings",
		nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
	logger::info("Journal long press: RestoreSavedSettings({}) {}", tabIdx, ok ? "ok" : "FAIL");
}

void InputHandler::InvokeRestoreTabIfNeeded(JournalTab tab)
{
	// Only needed when QJO is installed — QJO unconditionally forces sJournalTabIdx to
	// kSystem on every Journal open, making sJournalTabIdx unreliable for tab tracking.
	// On vanilla, sJournalTabIdx is the authoritative tab selection; no Scaleform call needed.
	DetectQJOIfNeeded();
	if (!_qjoInstalled.value_or(false)) {
		return;
	}

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

	RE::GFxValue current;
	const bool   got = journal->uiMovie->GetVariable(&current, "_root.QuestJournalFader.Menu_mc.iCurrentTab");
	if (!got || current.GetType() != RE::GFxValue::ValueType::kNumber) {
		logger::warn("QJO tab restore: could not read iCurrentTab");
		return;
	}
	const auto currentIdx = static_cast<std::uint32_t>(current.GetNumber());
	if (currentIdx == tabIdx) {
		return;
	}

	logger::info("QJO tab restore: current={} expected={} — calling RestoreSavedSettings", currentIdx, tabIdx);
	std::array<RE::GFxValue, 2> args{ static_cast<double>(tabIdx), false /* abTabsDisabled */ };
	journal->uiMovie->Invoke(
		"_root.QuestJournalFader.Menu_mc.RestoreSavedSettings",
		nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
}

void InputHandler::DetectQJOIfNeeded()
{
	if (_qjoInstalled.has_value()) {
		return;
	}
	auto* ui = RE::UI::GetSingleton();
	if (!ui) {
		return;
	}
	auto journal = ui->GetMenu(RE::JournalMenu::MENU_NAME);
	if (!journal || !journal->uiMovie) {
		return;
	}
	// Probe for a QJO-specific function in the Quests page SWF. QJO_EndPage is defined by
	// QJO and absent in vanilla — GetVariable returns undefined (or fails) without QJO.
	RE::GFxValue result;
	const bool   found = journal->uiMovie->GetVariable(
		&result, "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.QJO_EndPage");
	_qjoInstalled = found && result.GetType() != RE::GFxValue::ValueType::kUndefined;
	logger::info("QJO detection: {}", *_qjoInstalled ? "QJO installed" : "vanilla Journal");
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
