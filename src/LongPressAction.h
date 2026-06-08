#pragma once

#include <cstdint>
#include <string>

namespace HoldFast
{
	inline constexpr float kMinHoldDuration = 0.1F;
	inline constexpr float kDefaultHoldDuration = 0.5F;
	inline constexpr float kMaxHoldDuration = 5.0F;
}

enum class LongPressAction
{
	kNone,
	kMap,
	kSystem,
	kQuests,
	kStats,
	kInventory,
	kMagic,
	kFavorites,
	kTweenMenu,
	kWait,
	kNewSave,
	kQuickSave,
	kBestiary,
	kCharacterSheet,
};

struct ButtonConfig
{
	std::uint32_t   keyCode{};
	std::string     name;
	LongPressAction action{ LongPressAction::kNone };
};
