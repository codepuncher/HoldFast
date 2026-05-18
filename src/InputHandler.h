#pragma once

class InputHandler :
	public RE::BSTEventSink<RE::InputEvent*>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static InputHandler* GetSingleton();

	InputHandler(const InputHandler&) = delete;
	InputHandler& operator=(const InputHandler&) = delete;

	RE::BSEventNotifyControl ProcessEvent(
		RE::InputEvent* const*               a_events,
		RE::BSTEventSource<RE::InputEvent*>* a_source) override;

	RE::BSEventNotifyControl ProcessEvent(
		const RE::MenuOpenCloseEvent*               a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

	static constexpr float         kDefaultHoldDuration{ 0.5F };
	static constexpr std::uint32_t kDefaultButton{
		static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kStart)
	};

	void SetHoldDuration(float a_duration) noexcept { holdDuration = a_duration; }
	void SetButton(std::uint32_t a_keyCode, std::string a_name) noexcept;

	// Queries ControlMap for the user event bound to the configured button and caches it.
	// Call at kInputLoaded, kPostLoadGame, and kNewGame.
	void UpdateShortPressBinding();

	~InputHandler() override = default;

private:
	InputHandler() = default;

	bool ProcessButton(const RE::ButtonEvent* btn);
	void DispatchShortPress(float held) const;

	float                                                holdDuration{ kDefaultHoldDuration };
	std::uint32_t                                        buttonKeyCode{ kDefaultButton };
	std::string                                          buttonName{ "Start" };
	RE::BSFixedString                                    shortPressUserEvent;
	std::optional<std::chrono::steady_clock::time_point> _pressTime;
	bool                                                 _mapTriggered{ false };
};
