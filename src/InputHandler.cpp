#include "PCH.h"

#include "InputHandler.h"

InputHandler* InputHandler::GetSingleton()
{
	static InputHandler instance;
	return &instance;
}

void InputHandler::SetButton(std::uint32_t a_keyCode, std::string a_name) noexcept
{
	buttonKeyCode = a_keyCode;
	buttonName = std::move(a_name);
}

void InputHandler::UpdateShortPressBinding()
{
	auto* controlMap = RE::ControlMap::GetSingleton();
	if (!controlMap) {
		logger::error("ControlMap unavailable — short press will have no effect");
		shortPressUserEvent = "";
		return;
	}

	shortPressUserEvent = controlMap->GetUserEventName(buttonKeyCode, RE::INPUT_DEVICE::kGamepad);

	if (shortPressUserEvent.empty()) {
		logger::warn("{} has no binding in kGameplay context — short press disabled", buttonName);
	} else {
		logger::info("{} short press user event: '{}'", buttonName, shortPressUserEvent);
	}
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	const RE::MenuOpenCloseEvent* a_event,
	RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_eventSource*/)
{
	if (a_event && !a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME) {
		UpdateShortPressBinding();
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

	// If any pausing menu is open (Journal, Map, Inventory, Console, etc.), pass all input
	// through so the active menu can handle the configured button normally. Also clears any
	// captured press so _pressTime can't fire a spurious dispatch once the menu closes.
	auto* ui = RE::UI::GetSingleton();
	if (ui && ui->GameIsPaused()) {
		_pressTime.reset();
		_mapTriggered = false;
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
		if (btn->GetIDCode() != buttonKeyCode) {
			continue;
		}
		if (ProcessButton(btn)) {
			shouldBlock = true;
		}
	}

	// kStop halts the entire frame's event batch for all downstream sinks, not just the
	// configured button. Concurrent input in the same frame is intentionally suppressed
	// while a hold is in progress. Acceptable: simultaneous Start/Back + action is not a
	// normal use case in Skyrim.
	return shouldBlock ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
}

bool InputHandler::ProcessButton(const RE::ButtonEvent* btn)
{
	if (btn->IsDown()) {
		_pressTime = std::chrono::steady_clock::now();
		_mapTriggered = false;
		return true;
	}

	if (btn->IsHeld() && _pressTime) {
		if (!_mapTriggered && btn->HeldDuration() >= holdDuration) {
			_mapTriggered = true;
			auto* uiQueue = RE::UIMessageQueue::GetSingleton();
			if (!uiQueue) {
				logger::error("UIMessageQueue unavailable — hold threshold reached but map not opened");
			} else {
				uiQueue->AddMessage(RE::MapMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
			}
		}
		return true;
	}

	if (btn->IsUp() && _pressTime) {
		if (_mapTriggered) {
			_mapTriggered = false;
		} else {
			const auto held = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - *_pressTime)
			                      .count();
			DispatchShortPress(held);
		}
		_pressTime.reset();
		return true;
	}

	return false;
}

void InputHandler::DispatchShortPress(float held) const
{
	// Best-effort guard against stale _pressTime from process suspension (e.g. Alt-Tab).
	// Any legitimate short press has held < holdDuration <= kMaxHoldDuration, so this only
	// fires when _pressTime accumulated wall-clock time during suspension while game time
	// was frozen. Known limitation: suspensions shorter than kMaxHoldDuration seconds may
	// still dispatch a spurious short press.
	if (held > kMaxHoldDuration) {
		logger::warn("{} press duration {:.1f}s exceeds sanity limit — discarded", buttonName, held);
		return;
	}

	if (shortPressUserEvent.empty()) {
		logger::warn("{} short press has no binding — press consumed but no menu opened", buttonName);
		return;
	}

	auto* menuControls = RE::MenuControls::GetSingleton();
	if (!menuControls || !menuControls->menuOpenHandler) {
		logger::error("MenuControls or menuOpenHandler unavailable — short press consumed but no menu opened");
		return;
	}

	auto deleter = [](RE::ButtonEvent* e) {
		e->~ButtonEvent();
		RE::free(e);
	};
	std::unique_ptr<RE::ButtonEvent, decltype(deleter)> syntheticEvent{
		RE::ButtonEvent::Create(
			RE::INPUT_DEVICE::kGamepad,
			shortPressUserEvent,
			buttonKeyCode,
			1.0F,  // value=1.0 → IsPressed()=true, IsDown()=true
			0.0F   // heldDownSecs=0.0 → IsDown()=true
			),
		deleter
	};
	if (!syntheticEvent) {
		logger::error("Failed to allocate synthetic ButtonEvent — short press consumed but no menu opened");
		return;
	}

	if (!menuControls->menuOpenHandler->CanProcess(syntheticEvent.get())) {
		logger::warn("MenuOpenHandler rejected short press — press consumed but no menu opened");
		return;
	}

	menuControls->menuOpenHandler->ProcessButton(syntheticEvent.get());
}
