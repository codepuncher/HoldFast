#include "PCH.h"

#include "Config.h"
#include "InputHandler.h"
#include "Utils.h"

namespace
{
	constexpr auto kIniPath = R"(Data\SKSE\Plugins\HoldFast.ini)";

	[[nodiscard]] float ReadHoldDuration(const CSimpleIniA& ini)
	{
		const auto raw = static_cast<float>(ini.GetDoubleValue("General", "fHoldDuration", HoldFast::kDefaultHoldDuration));
		const auto duration = HoldFast::ClampHoldDuration(raw, HoldFast::kDefaultHoldDuration, HoldFast::kMaxHoldDuration);
		if (duration == raw) {
			return duration;
		}
		if (!std::isfinite(raw)) {
			logger::warn("fHoldDuration is non-finite — using default {:.1f}", HoldFast::kDefaultHoldDuration);
			return duration;
		}
		if (raw <= 0.0F) {
			logger::warn("fHoldDuration ({:.2f}) must be positive — using default {:.1f}", raw, HoldFast::kDefaultHoldDuration);
			return duration;
		}
		logger::warn("fHoldDuration ({:.2f}) exceeds maximum {:.1f} — capping", raw, HoldFast::kMaxHoldDuration);
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

	settings.startAction = hasStart ? ParseAction(rawStart) : LongPressAction::kNone;
	if (hasStart && settings.startAction == LongPressAction::kNone) {
		std::string lower{ HoldFast::TrimWhitespace(rawStart) };
		for (auto& c : lower) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		if (lower != "none") {
			logger::warn("sButtonStartAction='{}' is not a recognised action (valid: Map, System, Quests, Stats, Inventory, Magic, Favorites/Favourites, TweenMenu, Wait, NewSave, QuickSave, Bestiary, CharacterSheet, None) — disabling button", rawStart);
		}
	}
	settings.backAction = hasBack ? ParseAction(rawBack) : LongPressAction::kNone;
	if (hasBack && settings.backAction == LongPressAction::kNone) {
		std::string lower{ HoldFast::TrimWhitespace(rawBack) };
		for (auto& c : lower) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		if (lower != "none") {
			logger::warn("sButtonBackAction='{}' is not a recognised action (valid: Map, System, Quests, Stats, Inventory, Magic, Favorites/Favourites, TweenMenu, Wait, NewSave, QuickSave, Bestiary, CharacterSheet, None) — disabling button", rawBack);
		}
	}
	return settings;
}

bool HoldFast::Config::SaveSettings(const Settings& settings)
{
	CSimpleIniA ini;
	ini.SetSpaces(false);
	ini.LoadFile(kIniPath);

	const auto startActionName = std::string{ ActionName(settings.startAction) };
	const auto backActionName = std::string{ ActionName(settings.backAction) };

	const auto holdDurationStr = fmt::format("{:g}", settings.holdDuration);
	ini.SetValue("General", "fHoldDuration", holdDurationStr.c_str());
	ini.SetValue("General", "sButtonStartAction", startActionName.c_str());
	ini.SetValue("General", "sButtonBackAction", backActionName.c_str());

	const auto rc = ini.SaveFile(kIniPath);
	if (rc < SI_OK) {
		logger::error("Failed to write HoldFast.ini (rc={})", static_cast<int>(rc));
		return false;
	}
	return true;
}

std::vector<ButtonConfig> HoldFast::Config::BuildButtons(const Settings& settings)
{
	using Key = RE::BSWin32GamepadDevice::Key;

	std::vector<ButtonConfig> buttons;
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
