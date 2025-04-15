#include "BetterTelekinesis/Events.h"

namespace BetterTelekinesis
{

	HotkeyPressedEventHandler* HotkeyPressedEventHandler::GetSingleton()
	{
		static HotkeyPressedEventHandler singleton;
		return &singleton;
	}

	void HotkeyPressedEventHandler::Register()
	{
		RE::BSInputDeviceManager::GetSingleton()->AddEventSink(GetSingleton());
	}

	MenuOpenCloseEventHandler* MenuOpenCloseEventHandler::GetSingleton()
	{
		static MenuOpenCloseEventHandler singleton;
		return &singleton;
	}

	void MenuOpenCloseEventHandler::Register()
	{
		RE::UI::GetSingleton()->AddEventSink(GetSingleton());
	}

	RE::BSEventNotifyControl HotkeyPressedEventHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (a_event) {
			for (auto event = *a_event; event; event = event->next) {
				if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
					auto buttonEvent = dynamic_cast<const RE::ButtonEvent*>(event);
					if (buttonEvent && buttonEvent->IsPressed()) {
						auto key = buttonEvent->GetIDCode();
						if (buttonEvent->device == RE::INPUT_DEVICE::kKeyboard) {
							if (Config::DontLaunchIfRunningOutOfMagicka || Config::LaunchIsHotkeyInstead || Config::ThrowActorDamage > 0.0f) {
								if (key == static_cast<unsigned>(Config::AbortTelekinesisHotkey)) {
									BetterTelekinesisPlugin::_try_drop_now();
								}
							}
						}
					}
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl MenuOpenCloseEventHandler::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (a_event) {
			if (a_event->menuName == RE::MainMenu::MENU_NAME) {
				if (a_event->opening) {
					BetterTelekinesisPlugin::OnMainMenuOpen();
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
