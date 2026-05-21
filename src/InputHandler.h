#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

	static constexpr float kDefaultHoldDuration{ 0.5F };
	static constexpr float kMaxHoldDuration{ 5.0F };

	enum class LongPressAction
	{
		kNone,
		kMap,
		kSystem,
		kQuests,
		kStats,
		kJournal,
	};

	struct ButtonConfig
	{
		std::uint32_t   keyCode{};
		std::string     name;
		LongPressAction action{ LongPressAction::kNone };
	};

	void SetHoldDuration(float a_duration) noexcept { holdDuration = a_duration; }

	// Replace the tracked button list. Entries with action == kNone are excluded by the caller.
	// Call before registering the input sink.
	void SetButtons(std::vector<ButtonConfig> a_configs);

	// Queries ControlMap for the short-press user event for every tracked button and caches it.
	// Call at kInputLoaded, kPostLoadGame, kNewGame, and on JournalMenu close.
	void UpdateShortPressBinding();

	~InputHandler() override = default;

private:
	InputHandler() = default;

	// Game-internal variable controlling which tab the Journal Menu opens on.
	// RELOCATION_ID(520167 = SE 1.5.97, 406697 = AE 1.6.x).
	// Sourced from stay-at-system-page (MIT) and confirmed in jpsteel/skyrim-questjournal.
	enum class JournalTab : std::uint32_t
	{
		kQuest = 0,
		kStats = 1,
		kSystem = 2,
	};
	static inline REL::Relocation<JournalTab*> sJournalTabIdx{ RELOCATION_ID(520167, 406697) };

	struct ButtonState : ButtonConfig
	{
		RE::BSFixedString                                    shortPressUserEvent;
		std::optional<std::chrono::steady_clock::time_point> pressTime;
		bool                                                 triggered{ false };
	};

	bool        ProcessButton(const RE::ButtonEvent* btn, ButtonState& state);
	static void DispatchShortPress(const ButtonState& state, float held);
	void        DispatchLongPress(const ButtonState& state);
	void        OpenJournalOnTab(JournalTab tab, const std::string& buttonName);

	float                    holdDuration{ kDefaultHoldDuration };
	std::vector<ButtonState> _buttons;

	// Saved tab index used to restore sJournalTabIdx once the Journal has opened,
	// so a tab-specific long-press does not permanently override the player's last-visited tab.
	JournalTab _savedTabIdx{ JournalTab::kQuest };
	bool       _tabRestorePending{ false };
};
