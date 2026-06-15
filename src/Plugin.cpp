#include "PCH.h"

#include "Config.h"
#include "InputHandler.h"
#include "MCMNavigator.h"
#include "MenuUI.h"

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

void OnInputLoaded()
{
	auto* handler = InputHandler::GetSingleton();

	const auto settings = HoldFast::Config::LoadSettings();
	logger::info("Hold duration: {:.2f}s", settings.holdDuration);
	logger::info("Start → {}", HoldFast::Config::ActionName(settings.startAction));
	logger::info("Back → {}", HoldFast::Config::ActionName(settings.backAction));

	auto buttons = HoldFast::Config::BuildButtons(settings);
	if (buttons.empty()) {
		logger::warn("No buttons configured — HoldFast long-press interception disabled");
	}

	handler->SetHoldDuration(settings.holdDuration);
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
				if (const auto* taskIface = SKSE::GetTaskInterface()) {
					taskIface->AddTask(MCMNavigator::CacheModListFromPapyrus);
				}
				break;
			default:
				break;
			}
		})) {
		logger::error("Failed to register messaging listener");
		return false;
	}

	HoldFastMenuUI::Register();

	return true;
}
