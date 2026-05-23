#include "PCH.h"

#include <cctype>
#include <unordered_map>

#include "InputHandler.h"
#include "Utils.h"

using HoldFast::ClampHoldDuration;
using HoldFast::TrimWhitespace;

void SetupLog()
{
	auto logsFolder = logger::log_directory();
	if (!logsFolder) {
		util::report_and_fail("SKSE log_directory not provided, logs can't be written");
	}

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	const auto  logName = plugin ? std::string{ plugin->GetName() } + ".log" : "HoldFast.log";
	auto        logPath = *logsFolder / logName;

	auto                          fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
	std::vector<spdlog::sink_ptr> sinks{ fileSink };
	if (IsDebuggerPresent()) {
		sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
	}

	auto spdlogger = std::make_shared<spdlog::logger>("global", sinks.begin(), sinks.end());
	spdlog::set_default_logger(std::move(spdlogger));
	spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%s:%#] %v");
#ifdef NDEBUG
	spdlog::set_level(spdlog::level::info);
#else
	spdlog::set_level(spdlog::level::trace);
#endif
	spdlog::flush_on(spdlog::level::info);
}

float ReadHoldDuration(const CSimpleIniA& a_ini)
{
	const auto raw = static_cast<float>(a_ini.GetDoubleValue("General", "fHoldDuration", InputHandler::kDefaultHoldDuration));
	const auto duration = ClampHoldDuration(raw, InputHandler::kDefaultHoldDuration, InputHandler::kMaxHoldDuration);
	if (duration != raw) {
		if (!std::isfinite(raw)) {
			logger::warn("fHoldDuration is non-finite — using default {:.1f}", InputHandler::kDefaultHoldDuration);
		} else if (raw <= 0.0F) {
			logger::warn("fHoldDuration ({:.2f}) must be positive — using default {:.1f}", raw, InputHandler::kDefaultHoldDuration);
		} else {
			logger::warn("fHoldDuration ({:.2f}) exceeds maximum {:.1f} — capping", raw, InputHandler::kMaxHoldDuration);
		}
	}
	return duration;
}

using LongPressAction = InputHandler::LongPressAction;

LongPressAction ReadLongPressAction(std::string_view raw, const char* iniKey)
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
		{ "none", LongPressAction::kNone },
	};

	const auto  trimmed = TrimWhitespace(raw);
	std::string lower{ trimmed };
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	const auto it = kActionMap.find(lower);
	if (it == kActionMap.end()) {
		logger::warn("{}='{}' is not a recognised action (valid: Map, System, Quests, Stats, Inventory, Magic, Favorites/Favourites, TweenMenu, Wait, None) — disabling button", iniKey, raw);
		return LongPressAction::kNone;
	}
	return it->second;
}

using ButtonConfig = InputHandler::ButtonConfig;

std::vector<ButtonConfig> ReadButtons(const CSimpleIniA& a_ini)
{
	using Key = RE::BSWin32GamepadDevice::Key;

	struct ButtonDef
	{
		const char*   iniKey;
		std::uint32_t keyCode;
		const char*   name;
	};

	static const std::array<ButtonDef, 2> kButtonDefs{ {
		{ .iniKey = "sButtonStartAction", .keyCode = static_cast<std::uint32_t>(Key::kStart), .name = "Start" },
		{ .iniKey = "sButtonBackAction", .keyCode = static_cast<std::uint32_t>(Key::kBack), .name = "Back" },
	} };

	// Only use legacy mode when the old sButton key is explicitly present.
	// If all keys are absent (e.g. missing INI), apply the new defaults directly.
	// Blank or whitespace-only values (e.g. sButtonStartAction=  ) are treated as absent
	// to avoid silently disabling buttons when the INI has an empty entry.
	auto hasNonEmptyValue = [&](const char* key) {
		const char* v = a_ini.GetValue("General", key, nullptr);
		return v != nullptr && !TrimWhitespace(v).empty();
	};
	const bool hasNewStyleKeys =
		hasNonEmptyValue("sButtonStartAction") || hasNonEmptyValue("sButtonBackAction");

	if (!hasNewStyleKeys && hasNonEmptyValue("sButton")) {
		const char* legacyRaw = a_ini.GetValue("General", "sButton", nullptr);
		// Legacy fallback: [General] sButton=Start|Back → Map action.
		static const std::unordered_map<std::string, ButtonConfig> kLegacyMap{
			{ "start", { .keyCode = static_cast<std::uint32_t>(Key::kStart), .name = "Start", .action = LongPressAction::kMap } },
			{ "back", { .keyCode = static_cast<std::uint32_t>(Key::kBack), .name = "Back", .action = LongPressAction::kMap } },
		};

		std::string lower{ TrimWhitespace(legacyRaw) };
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

		const auto it = kLegacyMap.find(lower);
		if (it == kLegacyMap.end()) {
			logger::warn("sButton='{}' is not a recognised button (valid: Start, Back) — using Start", legacyRaw);
			return { { .keyCode = static_cast<std::uint32_t>(Key::kStart), .name = "Start", .action = LongPressAction::kMap } };
		}
		logger::info("Legacy config: {} → Map", it->second.name);
		return { it->second };
	}

	if (!hasNewStyleKeys) {
		// No INI keys at all — apply new defaults.
		logger::info("No button action keys found — applying defaults (Start=Map, Back=System)");
		return {
			{ .keyCode = static_cast<std::uint32_t>(Key::kStart), .name = "Start", .action = LongPressAction::kMap },
			{ .keyCode = static_cast<std::uint32_t>(Key::kBack), .name = "Back", .action = LongPressAction::kSystem },
		};
	}

	std::vector<ButtonConfig> result;
	for (const auto& def : kButtonDefs) {
		const char* raw = a_ini.GetValue("General", def.iniKey, nullptr);
		if (!raw || TrimWhitespace(raw).empty()) {
			continue;  // absent or blank key → kNone, no warning
		}
		const auto action = ReadLongPressAction(raw, def.iniKey);
		if (action == LongPressAction::kNone) {
			continue;  // kNone entries are excluded
		}
		result.push_back({ .keyCode = def.keyCode, .name = def.name, .action = action });
	}
	return result;
}

void OnInputLoaded()
{
	auto* handler = InputHandler::GetSingleton();

	CSimpleIniA ini;
	const auto  rc = ini.LoadFile(R"(Data\SKSE\Plugins\HoldFast.ini)");
	if (rc < SI_OK) {
		logger::warn("HoldFast.ini not found or could not be parsed (rc={}) — using defaults", static_cast<int>(rc));
	}

	const float holdDuration = ReadHoldDuration(ini);
	handler->SetHoldDuration(holdDuration);
	logger::info("Hold duration: {:.2f}s", holdDuration);

	auto buttons = ReadButtons(ini);
	if (buttons.empty()) {
		logger::warn("No buttons configured — HoldFast is inactive");
		return;
	}

	for (const auto& btn : buttons) {
		static const std::unordered_map<LongPressAction, std::string_view> kActionNames{
			{ LongPressAction::kMap, "Map" },
			{ LongPressAction::kSystem, "System" },
			{ LongPressAction::kQuests, "Quests" },
			{ LongPressAction::kStats, "Stats" },
			{ LongPressAction::kInventory, "Inventory" },
			{ LongPressAction::kMagic, "Magic" },
			{ LongPressAction::kFavorites, "Favorites" },
			{ LongPressAction::kTweenMenu, "TweenMenu" },
			{ LongPressAction::kWait, "Wait" },
		};
		const auto it = kActionNames.find(btn.action);
		const auto actionName = it != kActionNames.end() ? it->second : "Unknown";
		logger::info("{} → {}", btn.name, actionName);
	}

	handler->SetButtons(std::move(buttons));

	auto* inputDeviceMgr = RE::BSInputDeviceManager::GetSingleton();
	if (!inputDeviceMgr) {
		logger::error("Failed to get BSInputDeviceManager");
		return;
	}
	inputDeviceMgr->PrependEventSink(handler);
	logger::info("Input sink registered");

	auto* ui = RE::UI::GetSingleton();
	if (ui) {
		ui->AddEventSink<RE::MenuOpenCloseEvent>(handler);
	} else {
		logger::error("Failed to get UI — Journal close handling and controls rebind detection disabled");
	}

	handler->UpdateShortPressBinding();
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	if (!plugin) {
		logger::error("Failed to get plugin declaration");
		return false;
	}
	logger::info("{} v{} loaded", plugin->GetName(), plugin->GetVersion());

	const auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		logger::error("Failed to get SKSE messaging interface");
		return false;
	}

	if (!messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
			switch (a_msg->type) {
			case SKSE::MessagingInterface::kInputLoaded:
				OnInputLoaded();
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
			case SKSE::MessagingInterface::kNewGame:
				InputHandler::GetSingleton()->UpdateShortPressBinding();
				break;
			default:
				break;
			}
		})) {
		logger::error("Failed to register messaging listener");
		return false;
	}

	return true;
}
