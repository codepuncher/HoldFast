#include "PCH.h"

#include <cctype>
#include <unordered_map>

#include "InputHandler.h"

namespace logger = SKSE::log;

void SetupLog()
{
	auto logsFolder = logger::log_directory();
	if (!logsFolder) {
		SKSE::stl::report_and_fail("SKSE log_directory not provided, logs can't be written");
	}

	auto logPath = *logsFolder / "QuickMap.log";
	auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
	auto spdlogger = std::make_shared<spdlog::logger>("global", std::move(fileSink));
	spdlog::set_default_logger(std::move(spdlogger));
	spdlog::set_level(spdlog::level::info);
	spdlog::flush_on(spdlog::level::info);
}

float ReadHoldDuration(const CSimpleIniA& a_ini)
{
	const auto duration = static_cast<float>(a_ini.GetDoubleValue("General", "fHoldDuration", InputHandler::kDefaultHoldDuration));
	if (duration <= 0.0F) {
		logger::warn("fHoldDuration ({:.2f}) must be positive — using default {:.1f}", duration, InputHandler::kDefaultHoldDuration);
		return InputHandler::kDefaultHoldDuration;
	}
	return duration;
}

struct ButtonDef
{
	std::uint32_t keyCode;
	std::string   displayName;
};

ButtonDef ReadButton(const CSimpleIniA& a_ini)
{
	using Key = RE::BSWin32GamepadDevice::Key;

	static const std::unordered_map<std::string, ButtonDef> kButtonMap{
		{ "start", { .keyCode = static_cast<std::uint32_t>(Key::kStart), .displayName = "Start" } },
		{ "back", { .keyCode = static_cast<std::uint32_t>(Key::kBack), .displayName = "Back" } },
	};

	std::string raw = a_ini.GetValue("General", "sButton", "Start");
	std::string lower = raw;
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	const auto it = kButtonMap.find(lower);
	if (it == kButtonMap.end()) {
		logger::warn("sButton '{}' is not a recognised button (valid: Start, Back) — using Start", raw);
		return { .keyCode = InputHandler::kDefaultButton, .displayName = "Start" };
	}

	return it->second;
}

void OnInputLoaded()
{
	auto* handler = InputHandler::GetSingleton();

	CSimpleIniA ini;
	const auto  rc = ini.LoadFile(R"(Data\SKSE\Plugins\QuickMap.ini)");
	if (rc < SI_OK) {
		logger::warn("QuickMap.ini not found or could not be parsed (rc={}) — using defaults", static_cast<int>(rc));
	}

	const float holdDuration = ReadHoldDuration(ini);
	handler->SetHoldDuration(holdDuration);
	logger::info("Hold duration: {:.2f}s", holdDuration);

	auto [buttonKeyCode, buttonName] = ReadButton(ini);
	logger::info("Trigger button: {}", buttonName);
	handler->SetButton(buttonKeyCode, std::move(buttonName));

	auto* inputDeviceMgr = RE::BSInputDeviceManager::GetSingleton();
	if (!inputDeviceMgr) {
		logger::error("Failed to get BSInputDeviceManager");
		return;
	}

	inputDeviceMgr->PrependEventSink(handler);
	logger::info("Input sink registered");

	auto* ui = RE::UI::GetSingleton();
	if (!ui) {
		logger::error("Failed to get UI — controls rebind detection disabled");
	} else {
		ui->AddEventSink<RE::MenuOpenCloseEvent>(handler);
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
