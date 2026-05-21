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
		} else {
			logger::info("{} short press user event: '{}'", bs.name, bs.shortPressUserEvent);
		}
	}
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	const RE::MenuOpenCloseEvent* a_event,
	RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_eventSource*/)
{
	if (a_event && a_event->menuName == RE::JournalMenu::MENU_NAME) {
		if (a_event->opening) {
			// Journal opened — our tab write has been consumed. Restore the original value
			// so subsequent player-initiated opens use their own last-visited tab.
			if (_tabRestorePending) {
				if (sJournalTabIdx.get()) {
					*sJournalTabIdx = _savedTabIdx;
				}
				_tabRestorePending = false;
			}
		} else {
			UpdateShortPressBinding();
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	RE::InputEvent* const* a_events,
	RE::BSTEventSource<RE::InputEvent*>* /*a_eventSource*/)
{
	if (!a_events) {
		return RE::BSEventNotifyControl::kContinue;
	}

	// If any pausing menu is open, pass all input through and clear any captured press so it
	// can't fire a spurious dispatch once the menu closes.
	auto* ui = RE::UI::GetSingleton();
	if (ui && ui->GameIsPaused()) {
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
			if (btn->GetIDCode() == bs.keyCode) {
				if (ProcessButton(btn, bs)) {
					shouldBlock = true;
				}
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
		if (state.triggered) {
			state.triggered = false;
		} else {
			const auto held = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - *state.pressTime)
			                      .count();
			DispatchShortPress(state, held);
		}
		state.pressTime.reset();
		return true;
	}

	return false;
}

void InputHandler::DispatchLongPress(const ButtonState& state)
{
	auto* uiQueue = RE::UIMessageQueue::GetSingleton();
	if (!uiQueue) {
		logger::error("{} long press: UIMessageQueue unavailable — action not dispatched", state.name);
		return;
	}

	switch (state.action) {
	case LongPressAction::kMap:
		uiQueue->AddMessage(RE::MapMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		break;
	case LongPressAction::kSystem:
		OpenJournalOnTab(JournalTab::kSystem, state.name);
		uiQueue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		break;
	case LongPressAction::kQuests:
		OpenJournalOnTab(JournalTab::kQuest, state.name);
		uiQueue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		break;
	case LongPressAction::kStats:
		OpenJournalOnTab(JournalTab::kStats, state.name);
		uiQueue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		break;
	case LongPressAction::kJournal:
		uiQueue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		break;
	default:
		break;
	}
}

void InputHandler::OpenJournalOnTab(JournalTab tab, const std::string& buttonName)
{
	if (!sJournalTabIdx.get()) {
		logger::warn("{} long press: sJournalTabIdx unavailable — opening Journal on last-visited tab", buttonName);
		return;
	}
	if (!_tabRestorePending) {
		_savedTabIdx = *sJournalTabIdx;
	}
	_tabRestorePending = true;
	*sJournalTabIdx = tab;
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

	auto* menuControls = RE::MenuControls::GetSingleton();
	if (!menuControls || !menuControls->menuOpenHandler) {
		logger::error("MenuControls or menuOpenHandler unavailable — {} short press consumed but no menu opened", state.name);
		return;
	}

	auto deleter = [](RE::ButtonEvent* e) {
		e->~ButtonEvent();
		RE::free(e);
	};
	std::unique_ptr<RE::ButtonEvent, decltype(deleter)> syntheticEvent{
		RE::ButtonEvent::Create(
			RE::INPUT_DEVICE::kGamepad,
			state.shortPressUserEvent,
			state.keyCode,
			1.0F,  // value=1.0 → IsPressed()=true, IsDown()=true
			0.0F   // heldDownSecs=0.0 → IsDown()=true
			),
		deleter
	};
	if (!syntheticEvent) {
		logger::error("Failed to allocate synthetic ButtonEvent — {} short press consumed but no menu opened", state.name);
		return;
	}

	if (!menuControls->menuOpenHandler->CanProcess(syntheticEvent.get())) {
		logger::warn("MenuOpenHandler rejected {} short press — press consumed but no menu opened", state.name);
		return;
	}

	menuControls->menuOpenHandler->ProcessButton(syntheticEvent.get());
}
