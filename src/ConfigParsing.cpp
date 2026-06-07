#include <cctype>
#include <unordered_map>

#include "Config.h"
#include "Utils.h"

LongPressAction HoldFast::Config::ParseAction(std::string_view raw)
{
	static const std::unordered_map<std::string, LongPressAction> kActionMap{
		{ "map", LongPressAction::kMap },
		{ "system", LongPressAction::kSystem },
		{ "quests", LongPressAction::kQuests },
		{ "stats", LongPressAction::kStats },
		{ "inventory", LongPressAction::kInventory },
		{ "magic", LongPressAction::kMagic },
		{ "favorites", LongPressAction::kFavorites },
		{ "favourites", LongPressAction::kFavorites },
		{ "tweenmenu", LongPressAction::kTweenMenu },
		{ "wait", LongPressAction::kWait },
		{ "newsave", LongPressAction::kNewSave },
		{ "quicksave", LongPressAction::kQuickSave },
		{ "bestiary", LongPressAction::kBestiary },
		{ "charactersheet", LongPressAction::kCharacterSheet },
		{ "none", LongPressAction::kNone },
	};

	const auto  trimmed = HoldFast::TrimWhitespace(raw);
	std::string lower{ trimmed };
	for (auto& c : lower) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	const auto it = kActionMap.find(lower);
	return it != kActionMap.end() ? it->second : LongPressAction::kNone;
}

const char* HoldFast::Config::ActionName(LongPressAction action)
{
	for (const auto& option : kActionOptions) {
		if (option.action == action) {
			return option.name;
		}
	}
	return "None";
}
