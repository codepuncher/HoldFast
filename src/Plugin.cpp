#include "PCH.h"

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

float ReadHoldDuration()
{
	CSimpleIniA ini;
	const auto  rc = ini.LoadFile(R"(Data\SKSE\Plugins\QuickMap.ini)");
	if (rc < SI_OK) {
		logger::warn("QuickMap.ini not found or could not be parsed (rc={}) — using default hold duration 1.0s", static_cast<int>(rc));
	}
	const auto duration = static_cast<float>(ini.GetDoubleValue("General", "fHoldDuration", 1.0));
	if (duration <= 0.0F) {
		logger::warn("fHoldDuration ({:.2f}) must be positive — using default 1.0", duration);
		return 1.0F;
	}
	return duration;
}

void OnInputLoaded()
{
	auto* handler = InputHandler::GetSingleton();
	handler->holdDuration = ReadHoldDuration();
	logger::info("Hold duration: {:.2f}s", handler->holdDuration);

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

	handler->UpdateShortPressMenu();
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
				InputHandler::GetSingleton()->UpdateShortPressMenu();
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
