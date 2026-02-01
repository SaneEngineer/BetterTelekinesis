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
			if (buttonEvent && (buttonEvent->IsPressed() || buttonEvent->IsDown())) {
				auto key = buttonEvent->GetIDCode();
				if (buttonEvent->device == RE::INPUT_DEVICE::kKeyboard) {
					if (Config::AbortTelekinesisHotkeyEnabled) {
						if (key == static_cast<unsigned>(Config::AbortTelekinesisHotkey)) {
							BetterTelekinesisPlugin::TryDropNow();
						}
					} 
					
					if (Config::ActorDamageToggleEnabled) {
						if (key == static_cast<unsigned>(Config::ToggleActorDamageHotkey)) {
							logger::debug("Actor Damage Toggle Hotkey Pressed");
							BetterTelekinesisPlugin::actorDamageToggled = !BetterTelekinesisPlugin::actorDamageToggled;
							if (BetterTelekinesisPlugin::actorDamageToggled) {
								logger::debug("Toggled Damage Off");
								Config::HoldActorDamage = 0.0f;
							} else {
								logger::debug("Toggled Damage On");
								Config::HoldActorDamage = BetterTelekinesisPlugin::origActorDamage;
							}
						}
					}
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
