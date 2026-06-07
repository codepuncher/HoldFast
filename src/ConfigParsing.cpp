#ifndef PLUGIN_TESTS_ONLY
#	include "PCH.h"
#else
#	include <algorithm>
#	include <string>
#endif

#include <cctype>
#include <unordered_map>

#include "Config.h"
#include "Utils.h"

namespace
{
	using LongPressAction = InputHandler::LongPressAction;
}

InputHandler::LongPressAction HoldFast::Config::ParseAction(std::string_view raw, const char* sourceKey, bool logWarnings)
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
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	const auto it = kActionMap.find(lower);
	if (it != kActionMap.end()) {
		return it->second;
	}
#ifndef PLUGIN_TESTS_ONLY
	if (logWarnings) {
		logger::warn("{}='{}' is not a recognised action (valid: Map, System, Quests, Stats, Inventory, Magic, Favorites/Favourites, TweenMenu, Wait, NewSave, QuickSave, Bestiary, CharacterSheet, None) — disabling button",
			sourceKey, raw);
	}
#else
	(void)logWarnings;
	(void)sourceKey;
#endif
	return LongPressAction::kNone;
}

std::string_view HoldFast::Config::ActionName(LongPressAction action)
{
	for (const auto& option : kActionOptions) {
		if (option.action == action) {
			return option.name;
		}
	}
	return "None";
}
