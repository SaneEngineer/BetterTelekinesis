#include "BetterTelekinesis/Papyrus.h"

namespace Papyrus
{
	void OnConfigClose(RE::TESQuest*)
	{
		BetterTelekinesis::Config::ReadSettings();
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}

		a_vm->RegisterFunction("OnConfigClose", MCM, OnConfigClose);

		return true;
	}
}
