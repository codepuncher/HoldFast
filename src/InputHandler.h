#pragma once

class InputHandler :
	public RE::BSTEventSink<RE::InputEvent*>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static InputHandler* GetSingleton();

	RE::BSEventNotifyControl ProcessEvent(
		RE::InputEvent* const*               a_events,
		RE::BSTEventSource<RE::InputEvent*>* a_source) override;

	RE::BSEventNotifyControl ProcessEvent(
		const RE::MenuOpenCloseEvent*               a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

	float       holdDuration{ 1.0F };
	std::string shortPressMenuName;

	// Queries ControlMap for the menu bound to Start and caches it in shortPressMenuName.
	// Call at kInputLoaded, kPostLoadGame, and kNewGame.
	void UpdateShortPressMenu();

private:
	InputHandler() = default;
	~InputHandler() = default;
	InputHandler(const InputHandler&) = delete;
	InputHandler& operator=(const InputHandler&) = delete;

	bool ProcessStartButton(const RE::ButtonEvent* btn);
	void DispatchShortPress(float held) const;

	std::optional<std::chrono::steady_clock::time_point> _pressTime;
	bool                                                 _mapTriggered{ false };
};
