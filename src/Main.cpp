#include "BetterTelekinesis/Main.h"
#include "BetterTelekinesis/Events.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

void InitializeHooking()
{
	log::trace("Initializing trampoline...");
	auto& trampoline = GetTrampoline();
	trampoline.create(2048);
	log::trace("Trampoline initialized.");
	BetterTelekinesis::BetterTelekinesisPlugin::InstallHooks();
}

void InitializeMessaging()
{
	if (!GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) {
			switch (message->type) {
			// Skyrim lifecycle events.
			case MessagingInterface::kPostLoad:  // Called after all plugins have finished running SKSEPlugin_Load.
				// It is now safe to do multithreaded operations, or operations against other plugins.
				break;
			case MessagingInterface::kInputLoaded:  // Called when all game data has been found.
				BetterTelekinesis::MenuOpenCloseEventHandler::Register();
				BetterTelekinesis::HotkeyPressedEventHandler::Register();
				break;
			case MessagingInterface::kDataLoaded:  // All ESM/ESL/ESP plugins have loaded, main menu is now active.
				// It is now safe to access form data.
				BetterTelekinesis::BetterTelekinesisPlugin::Initialize();
				break;
			default:
				break;
			}
		})) {
		report_and_fail("Unable to register message listener.");
	}
}

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= PluginDeclaration::GetSingleton()->GetName();
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
	spdlog::level::level_enum level = spdlog::level::info;

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(level);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e][%l] %v"s);

	logger::info(FMT_STRING("{} v{}"), PluginDeclaration::GetSingleton()->GetName(), PluginDeclaration::GetSingleton()->GetVersion());
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	REL::Module::reset();

#ifdef _DEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif

	SKSE::Init(a_skse);

	InitializeLog();
	BetterTelekinesis::Config::ReadSettings();
	if (BetterTelekinesis::Config::DebugLogMode) {
		spdlog::default_logger()->set_level(spdlog::level::trace);
	}

	logger::info("Game version : {}", a_skse->RuntimeVersion().string());

	InitializeHooking();
	InitializeMessaging();

	return true;
}
