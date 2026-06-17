#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "LongPressAction.h"

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

	static constexpr float kMinHoldDuration{ HoldFast::kMinHoldDuration };
	static constexpr float kDefaultHoldDuration{ HoldFast::kDefaultHoldDuration };
	static constexpr float kMaxHoldDuration{ HoldFast::kMaxHoldDuration };

	using LongPressAction = ::LongPressAction;
	using ButtonConfig = ::ButtonConfig;

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
	enum class JournalTab : std::uint32_t
	{
		kQuest = 0,
		kStats = 1,
		kSystem = 2,
		// Sentinel: opens Journal on the System tab then navigates to the MCM overlay.
		// Not a real sJournalTabIdx value — kSystem (2) is written to the relocation.
		kMCM = 3,
	};
	static inline REL::Relocation<std::uint32_t*> sJournalTabIdx{ RELOCATION_ID(520167, 406697) };

	struct ButtonState : ButtonConfig
	{
		RE::BSFixedString                                    shortPressUserEvent;
		std::optional<std::chrono::steady_clock::time_point> pressTime;
		bool                                                 triggered{ false };
	};

	bool                 ScanInputEvents(RE::InputEvent* const* a_events);
	bool                 ProcessButton(const RE::ButtonEvent* btn, ButtonState& state);
	static bool          DispatchViaMenuOpenHandler(const RE::BSFixedString& userEvent, std::uint32_t keyCode, const std::string& logContext);
	static bool          DispatchViaQuickSaveLoadHandler(const RE::BSFixedString& userEvent, std::uint32_t keyCode, const std::string& logContext);
	static bool          DispatchViaFavoritesHandler(const RE::BSFixedString& userEvent, std::uint32_t keyCode, const std::string& logContext);
	static bool          DispatchViaHandler(RE::MenuEventHandler* handler, std::string_view handlerName, const RE::BSFixedString& userEvent, std::uint32_t keyCode, const std::string& logContext);
	static void          DispatchShortPress(const ButtonState& state, float held);
	static std::uint32_t JournalTabToIndex(JournalTab tab);
	static void          CloseJournal();
	void                 DispatchLongPress(const ButtonState& state);
	void                 OpenJournalOnTab(JournalTab tab, const std::string& buttonName);
	void                 RestoreJournalTab();
	void                 InvokeScaleformTab(JournalTab tab);
	void                 InvokeRestoreTabIfNeeded(JournalTab tab);
	void                 SnapshotJournalTab(RE::UI* ui);
	void                 DetectQJOIfNeeded(RE::GFxMovieView* movie);
	void                 HandleMCMQuickexit();
	void                 ResetMCMQuickexitState();

	float                    holdDuration{ kDefaultHoldDuration };
	std::vector<ButtonState> _buttons;

	// Saved tab index to restore sJournalTabIdx after any Journal long-press, so the
	// player's next normal Journal open lands on the tab they had before the long-press.
	// Restored on Journal close, or immediately as a fail-safe if the Journal never opens.
	// _pendingTab: target tab to set via Scaleform on Journal open (QJO compat).
	// _lastKnownTab: (QJO only) tab the player was last on when the Journal was open; used to
	//               counter QJO's forced kSystem override on every Journal open. Updated by the
	//               GameIsPaused input snapshot whenever the Journal is open (skips kQuest=0,
	//               the navigation-away transient set by the SWF before CloseMenu). Empty until
	//               first snapshot fires.
	// _qjoInstalled: cached result of QJO presence detection (detected on first Journal open).
	//               When false/empty, QJO tab-restore logic is skipped entirely.
	JournalTab                _savedTabIdx{ JournalTab::kQuest };
	bool                      _tabRestorePending{ false };
	bool                      _journalOpenDispatched{ false };
	std::optional<JournalTab> _pendingTab{};
	std::optional<JournalTab> _lastKnownTab{};
	std::optional<bool>       _qjoInstalled{};
	std::string               _pendingMCMModName;
	bool                      _mcmQuickexit{ false };
	bool                      _mcmWasOpen{ false };
	bool                      _mcmModPageSeen{ false };
};
