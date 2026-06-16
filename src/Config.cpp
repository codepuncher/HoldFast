#include "PCH.h"

#include <cctype>

#include "Config.h"
#include "InputHandler.h"
#include "Utils.h"

namespace
{
	constexpr auto kIniPath = R"(Data\SKSE\Plugins\HoldFast.ini)";

	[[nodiscard]] float ReadHoldDuration(const CSimpleIniA& ini)
	{
		const auto raw = static_cast<float>(ini.GetDoubleValue("General", "fHoldDuration", HoldFast::kDefaultHoldDuration));
		const auto duration = HoldFast::ClampHoldDuration(raw, HoldFast::kDefaultHoldDuration, HoldFast::kMinHoldDuration, HoldFast::kMaxHoldDuration);
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
		if (raw < HoldFast::kMinHoldDuration) {
			logger::warn("fHoldDuration ({:.2f}) is below minimum {:.1f} — using default {:.1f}", raw, HoldFast::kMinHoldDuration, HoldFast::kDefaultHoldDuration);
			return duration;
		}
		logger::warn("fHoldDuration ({:.2f}) exceeds maximum {:.1f} — capping", raw, HoldFast::kMaxHoldDuration);
		return duration;
	}

	[[nodiscard]] std::string GetMCMTarget(const CSimpleIniA& ini, const char* modKey)
	{
		const char* raw = ini.GetValue("General", modKey, nullptr);
		if (!raw) {
			return "None";
		}
		const auto trimmed = HoldFast::TrimWhitespace(raw);
		if (trimmed.empty()) {
			return "None";
		}
		std::string lower{ trimmed };
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return lower == "none" ? "None" : std::string{ trimmed };
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

	constexpr auto kValidActions = "Map, System, Quests, Stats, Inventory, Magic, Favorites/Favourites, TweenMenu, Wait, NewSave, QuickSave, Bestiary, CharacterSheet, MCM, None";

	settings.startAction = hasStart ? ParseAction(rawStart) : LongPressAction::kNone;
	if (hasStart && settings.startAction == LongPressAction::kNone) {
		std::string lower{ HoldFast::TrimWhitespace(rawStart) };
		for (auto& c : lower) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		if (lower != "none") {
			logger::warn("sButtonStartAction='{}' is not a recognised action (valid: {}) — disabling button", rawStart, kValidActions);
		}
	}
	settings.backAction = hasBack ? ParseAction(rawBack) : LongPressAction::kNone;
	if (hasBack && settings.backAction == LongPressAction::kNone) {
		std::string lower{ HoldFast::TrimWhitespace(rawBack) };
		for (auto& c : lower) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		if (lower != "none") {
			logger::warn("sButtonBackAction='{}' is not a recognised action (valid: {}) — disabling button", rawBack, kValidActions);
		}
	}

	settings.startMCMModName = GetMCMTarget(ini, "sButtonStartMCMModName");
	settings.backMCMModName = GetMCMTarget(ini, "sButtonBackMCMModName");
	settings.startMCMQuickexit = ini.GetBoolValue("General", "bButtonStartMCMQuickexit", true);
	settings.backMCMQuickexit = ini.GetBoolValue("General", "bButtonBackMCMQuickexit", true);

	if (settings.startAction == LongPressAction::kMCM &&
		HoldFast::CaseInsensitiveEqual(settings.startMCMModName, "None")) {
		logger::warn("sButtonStartAction=MCM but sButtonStartMCMModName is not set — Start button will open MCM without navigating to a specific mod");
	}
	if (settings.backAction == LongPressAction::kMCM &&
		HoldFast::CaseInsensitiveEqual(settings.backMCMModName, "None")) {
		logger::warn("sButtonBackAction=MCM but sButtonBackMCMModName is not set — Back button will open MCM without navigating to a specific mod");
	}

	return settings;
}

bool HoldFast::Config::SaveSettings(const Settings& settings)
{
	CSimpleIniA ini;
	ini.SetSpaces(false);
	const auto loadRc = ini.LoadFile(kIniPath);
	if (loadRc < SI_OK && loadRc != SI_FILE) {
		logger::warn("SaveSettings: failed to parse existing HoldFast.ini (rc={}) — existing content may be lost", static_cast<int>(loadRc));
	}

	const auto startActionName = std::string{ ActionName(settings.startAction) };
	const auto backActionName = std::string{ ActionName(settings.backAction) };

	const auto holdDurationStr = fmt::format("{:g}", settings.holdDuration);
	ini.SetValue("General", "fHoldDuration", holdDurationStr.c_str());
	ini.SetValue("General", "sButtonStartAction", startActionName.c_str());
	ini.SetValue("General", "sButtonBackAction", backActionName.c_str());
	ini.SetValue("General", "sButtonStartMCMModName", settings.startMCMModName.c_str());
	ini.SetValue("General", "sButtonBackMCMModName", settings.backMCMModName.c_str());
	ini.SetBoolValue("General", "bButtonStartMCMQuickexit", settings.startMCMQuickexit);
	ini.SetBoolValue("General", "bButtonBackMCMQuickexit", settings.backMCMQuickexit);

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
		buttons.push_back({
			.keyCode = static_cast<std::uint32_t>(Key::kStart),
			.name = "Start",
			.action = settings.startAction,
			.mcmModName = settings.startMCMModName,
			.mcmQuickexit = settings.startMCMQuickexit,
		});
	}
	if (settings.backAction != LongPressAction::kNone) {
		buttons.push_back({
			.keyCode = static_cast<std::uint32_t>(Key::kBack),
			.name = "Back",
			.action = settings.backAction,
			.mcmModName = settings.backMCMModName,
			.mcmQuickexit = settings.backMCMQuickexit,
		});
	}
	return buttons;
}

void HoldFast::Config::ApplySettings(InputHandler& handler, const Settings& settings)
{
	handler.SetHoldDuration(settings.holdDuration);
	handler.SetButtons(BuildButtons(settings));
	handler.UpdateShortPressBinding();
}
