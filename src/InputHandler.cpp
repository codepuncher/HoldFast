#include "PCH.h"
#include "InputHandler.h"

namespace logger = SKSE::log;

InputHandler* InputHandler::GetSingleton()
{
	static InputHandler instance;
	return &instance;
}

void InputHandler::UpdateShortPressMenu()
{
	auto* controlMap = RE::ControlMap::GetSingleton();
	if (!controlMap) {
		logger::error("ControlMap unavailable — short press will have no effect");
		shortPressMenuName.clear();
		return;
	}

	constexpr auto kStart = static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kStart);
	const auto     userEvent = controlMap->GetUserEventName(kStart, RE::INPUT_DEVICE::kGamepad);

	if (userEvent == "Tween Menu") {
		shortPressMenuName = RE::TweenMenu::MENU_NAME;
	} else if (userEvent == "Journal") {
		shortPressMenuName = RE::JournalMenu::MENU_NAME;
	} else {
		if (userEvent.empty()) {
			logger::warn("Start has no binding in kGameplay context — short press disabled");
		} else {
			logger::warn("Unknown user event '{}' for Start — short press disabled", userEvent);
		}
		shortPressMenuName.clear();
	}

	logger::info("Start short press bound to '{}'", shortPressMenuName.empty() ? "(none)" : shortPressMenuName);
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	const RE::MenuOpenCloseEvent*               a_event,
	RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (a_event && !a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME) {
		UpdateShortPressMenu();
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
	RE::InputEvent* const*               a_events,
	RE::BSTEventSource<RE::InputEvent*>*)
{
	if (!a_events) {
		return RE::BSEventNotifyControl::kContinue;
	}

	// If any pausing menu is open (Journal, Map, Inventory, Console, etc.), pass all input
	// through so the active menu can handle Start normally. Also clears any captured press
	// so _pressTime can't fire a spurious dispatch once the menu closes.
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
		if (btn->GetIDCode() != static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kStart)) {
			continue;
		}

		if (btn->IsDown()) {
			_pressTime    = std::chrono::steady_clock::now();
			_mapTriggered = false;
			shouldBlock   = true;
		} else if (btn->IsHeld() && _pressTime) {
			shouldBlock = true;
			if (!_mapTriggered && btn->HeldDuration() >= holdDuration) {
				auto* uiQueue = RE::UIMessageQueue::GetSingleton();
				if (!uiQueue) {
					logger::error("UIMessageQueue unavailable — hold threshold reached but map not opened");
				} else {
					uiQueue->AddMessage(RE::MapMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				}
				_mapTriggered = true;
			}
		} else if (btn->IsUp() && _pressTime) {
			shouldBlock = true;

			const auto held = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - *_pressTime).count();
			_pressTime.reset();

			if (_mapTriggered) {
				_mapTriggered = false;
			} else {
				if (held > 10.0f) {
					logger::warn("Start press duration {:.1f}s exceeds sanity limit — discarded", held);
					continue;
				}

				auto* uiQueue = RE::UIMessageQueue::GetSingleton();
				if (!uiQueue) {
					logger::error("UIMessageQueue unavailable — Start press consumed but no menu opened");
					continue;
				}

				if (!shortPressMenuName.empty()) {
					uiQueue->AddMessage(shortPressMenuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				}
			}
		}
	}

	return shouldBlock ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
}
