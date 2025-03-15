#pragma once
#include "BetterTelekinesis/Config.h"
#include "BetterTelekinesis/Main.h"

namespace BetterTelekinesis
{
	class HotkeyPressedEventHandler : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		static HotkeyPressedEventHandler* GetSingleton();
	    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
		static void Register();
	};

	class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuOpenCloseEventHandler* GetSingleton();
		virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
		static void Register();
	};
}
