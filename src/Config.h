#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "LongPressAction.h"

class InputHandler;

namespace HoldFast::Config
{
	struct Settings
	{
		float           holdDuration{ HoldFast::kDefaultHoldDuration };
		LongPressAction startAction{ LongPressAction::kMap };
		LongPressAction backAction{ LongPressAction::kSystem };
		std::string     startMCMModName{ "None" };
		std::string     backMCMModName{ "None" };
		bool            startMCMQuickexit{ true };
		bool            backMCMQuickexit{ true };
	};

	struct ActionOption
	{
		const char*     name;
		LongPressAction action;
	};

	inline constexpr std::array<ActionOption, 15> kActionOptions{ {
		{ "Map", LongPressAction::kMap },
		{ "System", LongPressAction::kSystem },
		{ "Quests", LongPressAction::kQuests },
		{ "Stats", LongPressAction::kStats },
		{ "Inventory", LongPressAction::kInventory },
		{ "Magic", LongPressAction::kMagic },
		{ "Favorites", LongPressAction::kFavorites },
		{ "TweenMenu", LongPressAction::kTweenMenu },
		{ "Wait", LongPressAction::kWait },
		{ "NewSave", LongPressAction::kNewSave },
		{ "QuickSave", LongPressAction::kQuickSave },
		{ "Bestiary", LongPressAction::kBestiary },
		{ "CharacterSheet", LongPressAction::kCharacterSheet },
		{ "MCM", LongPressAction::kMCM },
		{ "None", LongPressAction::kNone },
	} };

	[[nodiscard]] Settings LoadSettings();
	[[nodiscard]] bool     SaveSettings(const Settings& settings);

	[[nodiscard]] std::vector<ButtonConfig> BuildButtons(const Settings& settings);
	void                                    ApplySettings(InputHandler& handler, const Settings& settings);

	[[nodiscard]] LongPressAction ParseAction(std::string_view raw);
	[[nodiscard]] const char*     ActionName(LongPressAction action);
}
