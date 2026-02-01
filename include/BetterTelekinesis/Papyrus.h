#pragma once

#include "BetterTelekinesis/Config.h"

namespace Papyrus
{
	inline constexpr auto MCM = "DE_BT_MCMScript"sv;

	using VM = RE::BSScript::Internal::VirtualMachine;
	using StackID = RE::VMStackID;

#define STATIC_ARGS [[maybe_unused]] VM *a_vm, [[maybe_unused]] StackID a_stackID, RE::StaticFunctionTag *

	void OnConfigClose(RE::TESQuest*);
	bool Register(RE::BSScript::IVirtualMachine* a_vm);
}
