#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "InputHandler.h"

namespace HoldFast::Config
{
	struct Settings
	{
		float                         holdDuration{ InputHandler::kDefaultHoldDuration };
		InputHandler::LongPressAction startAction{ InputHandler::LongPressAction::kMap };
		InputHandler::LongPressAction backAction{ InputHandler::LongPressAction::kSystem };
	};

	struct ActionOption
	{
		std::string_view              name;
		InputHandler::LongPressAction action;
	};

	inline constexpr std::array<ActionOption, 14> kActionOptions{ {
		{ "Map", InputHandler::LongPressAction::kMap },
		{ "System", InputHandler::LongPressAction::kSystem },
		{ "Quests", InputHandler::LongPressAction::kQuests },
		{ "Stats", InputHandler::LongPressAction::kStats },
		{ "Inventory", InputHandler::LongPressAction::kInventory },
		{ "Magic", InputHandler::LongPressAction::kMagic },
		{ "Favorites", InputHandler::LongPressAction::kFavorites },
		{ "TweenMenu", InputHandler::LongPressAction::kTweenMenu },
		{ "Wait", InputHandler::LongPressAction::kWait },
		{ "NewSave", InputHandler::LongPressAction::kNewSave },
		{ "QuickSave", InputHandler::LongPressAction::kQuickSave },
		{ "Bestiary", InputHandler::LongPressAction::kBestiary },
		{ "CharacterSheet", InputHandler::LongPressAction::kCharacterSheet },
		{ "None", InputHandler::LongPressAction::kNone },
	} };

	[[nodiscard]] Settings LoadSettings();
	[[nodiscard]] bool     SaveSettings(const Settings& settings);

	[[nodiscard]] std::vector<InputHandler::ButtonConfig> BuildButtons(const Settings& settings);
	void                                                  ApplySettings(InputHandler& handler, const Settings& settings);

	[[nodiscard]] InputHandler::LongPressAction ParseAction(std::string_view raw);
	[[nodiscard]] std::string_view              ActionName(InputHandler::LongPressAction action);
}
