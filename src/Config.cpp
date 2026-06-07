#include "PCH.h"

#include <cctype>
#include <unordered_map>

#include "Config.h"
#include "Utils.h"

namespace
{
	constexpr auto kIniPath = R"(Data\SKSE\Plugins\HoldFast.ini)";

	using LongPressAction = InputHandler::LongPressAction;

	[[nodiscard]] float ReadHoldDuration(const CSimpleIniA& ini)
	{
		const auto raw = static_cast<float>(ini.GetDoubleValue("General", "fHoldDuration", InputHandler::kDefaultHoldDuration));
		const auto duration = HoldFast::ClampHoldDuration(raw, InputHandler::kDefaultHoldDuration, InputHandler::kMaxHoldDuration);
		if (duration == raw) {
			return duration;
		}
		if (!std::isfinite(raw)) {
			logger::warn("fHoldDuration is non-finite — using default {:.1f}", InputHandler::kDefaultHoldDuration);
			return duration;
		}
		if (raw <= 0.0F) {
			logger::warn("fHoldDuration ({:.2f}) must be positive — using default {:.1f}", raw, InputHandler::kDefaultHoldDuration);
			return duration;
		}
		logger::warn("fHoldDuration ({:.2f}) exceeds maximum {:.1f} — capping", raw, InputHandler::kMaxHoldDuration);
		return duration;
	}
}

HoldFast::Config::Settings HoldFast::Config::LoadSettings()
{
	Settings settings{};

	CSimpleIniA ini;
	const auto  rc = ini.LoadFile(kIniPath);
	if (rc < SI_OK) {
		logger::warn("HoldFast.ini not found or could not be parsed (rc={}) — using defaults", static_cast<int>(rc));
	}

	settings.holdDuration = ReadHoldDuration(ini);

	const char* rawStart = ini.GetValue("General", "sButtonStartAction", nullptr);
	const char* rawBack = ini.GetValue("General", "sButtonBackAction", nullptr);
	const bool  hasStart = rawStart && !HoldFast::TrimWhitespace(rawStart).empty();
	const bool  hasBack = rawBack && !HoldFast::TrimWhitespace(rawBack).empty();

	if (!hasStart && !hasBack) {
		logger::info("No button action keys found — applying defaults (Start=Map, Back=System)");
		return settings;
	}

	settings.startAction = hasStart ? ParseAction(rawStart, "sButtonStartAction", true) : LongPressAction::kNone;
	settings.backAction = hasBack ? ParseAction(rawBack, "sButtonBackAction", true) : LongPressAction::kNone;
	return settings;
}

bool HoldFast::Config::SaveSettings(const Settings& settings)
{
	CSimpleIniA ini;
	ini.LoadFile(kIniPath);

	const auto startActionName = std::string{ ActionName(settings.startAction) };
	const auto backActionName = std::string{ ActionName(settings.backAction) };

	ini.SetDoubleValue("General", "fHoldDuration", static_cast<double>(settings.holdDuration));
	ini.SetValue("General", "sButtonStartAction", startActionName.c_str());
	ini.SetValue("General", "sButtonBackAction", backActionName.c_str());

	const auto rc = ini.SaveFile(kIniPath);
	if (rc < SI_OK) {
		logger::error("Failed to write HoldFast.ini (rc={})", static_cast<int>(rc));
		return false;
	}
	return true;
}

std::vector<InputHandler::ButtonConfig> HoldFast::Config::BuildButtons(const Settings& settings)
{
	using Key = RE::BSWin32GamepadDevice::Key;

	std::vector<InputHandler::ButtonConfig> buttons;
	if (settings.startAction != LongPressAction::kNone) {
		buttons.push_back({ .keyCode = static_cast<std::uint32_t>(Key::kStart), .name = "Start", .action = settings.startAction });
	}
	if (settings.backAction != LongPressAction::kNone) {
		buttons.push_back({ .keyCode = static_cast<std::uint32_t>(Key::kBack), .name = "Back", .action = settings.backAction });
	}
	return buttons;
}

void HoldFast::Config::ApplySettings(InputHandler& handler, const Settings& settings)
{
	handler.SetHoldDuration(settings.holdDuration);
	handler.SetButtons(BuildButtons(settings));
	handler.UpdateShortPressBinding();
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
	if (logWarnings) {
		logger::warn("{}='{}' is not a recognised action (valid: Map, System, Quests, Stats, Inventory, Magic, Favorites/Favourites, TweenMenu, Wait, NewSave, QuickSave, Bestiary, CharacterSheet, None) — disabling button",
			sourceKey, raw);
	}
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
