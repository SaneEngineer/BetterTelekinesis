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

	RE::BSEventNotifyControl HotkeyPressedEventHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (a_event) {
			auto event = *a_event;
			auto buttonEvent = event ? event->AsButtonEvent() : nullptr;
			if (buttonEvent) {
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

		return RE::BSEventNotifyControl::kContinue;
	}
}
