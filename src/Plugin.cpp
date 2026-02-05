#include "BetterTelekinesis/Main.h"

using namespace Xbyak;
namespace BetterTelekinesis
{
	static bool AE = REL::Module::IsAE();

	void BetterTelekinesisPlugin::Update()
	{
		float diff = RE::Main::QFrameAnimTime();
		time += diff;

		reachSpell = 0.0f;
		castingSwordBarrage = false;
		castingNormal = false;
		auto plr = RE::PlayerCharacter::GetSingleton();

		if (plr != nullptr) {
			float dist_telek = 0.0f;
			if (!REL::Module::IsVR()) {
				dist_telek = plr->GetPlayerRuntimeData().telekinesisDistance;
			} else {
				dist_telek = plr->GetVRPlayerRuntimeData().telekinesisDistance;
			}
			if (dist_telek > 0.0f) {
				if (!REL::Module::IsVR()) {
					auto efls = plr->AsMagicTarget()->GetActiveEffectList();
					if (efls != nullptr) {
						for (auto x : *efls) {
							auto st = IsOurSpell(x->GetBaseObject());
							if (st == OurSpellTypes::TelekReach) {
								reachSpell = dist_telek;
							} else if (st == OurSpellTypes::SwordBarrage) {
								castingSwordBarrage = true;
							} else if (st == OurSpellTypes::SwordBlast) {
								continue;
							} else if (reinterpret_cast<RE::TelekinesisEffect*>(x) != nullptr) {
								castingNormal = true;
							}
						}
					}
				} else {
					plr->AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* activeEffect) -> RE::BSContainer::ForEachResult {
						if (auto mgef = activeEffect ? activeEffect->GetBaseObject() : nullptr; mgef) {
							if (activeEffect->flags.all(RE::ActiveEffect::Flag::kInactive) || activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
								return RE::BSContainer::ForEachResult::kContinue;
							}

							auto st = IsOurSpell(activeEffect->GetBaseObject());
							if (st == OurSpellTypes::TelekReach) {
								reachSpell = dist_telek;
							} else if (st == OurSpellTypes::SwordBarrage) {
								castingSwordBarrage = true;
							} else if (st == OurSpellTypes::SwordBlast) {
								return RE::BSContainer::ForEachResult::kContinue;
							} else if (reinterpret_cast<RE::TelekinesisEffect*>(activeEffect) != nullptr) {
								castingNormal = true;
							}
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
				}
			}

			auto casters = std::vector{ plr->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand), plr->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand) };
			if (casters[0] != nullptr && casters[1] != nullptr && casters[0]->state != RE::MagicCaster::State::kNone && casters[1]->state != RE::MagicCaster::State::kNone) {
				auto items = std::vector{ casters[0]->currentSpell, casters[1]->currentSpell };
				if (items[0] != nullptr && items[1] != nullptr) {
					int ourMode = 0;
					for (int i = 0; i < 2; i++) {
						bool has = false;
						auto itm = items[i];
						auto& effls = itm->effects;
						if (!effls.empty()) {
							for (auto x : effls) {
								auto fs = x->baseEffect;
								if (fs == nullptr) {
									continue;
								}

								if (fs->GetArchetype() == RE::EffectSetting::Archetype::kTelekinesis || fs->GetArchetype() == RE::EffectSetting::Archetype::kGrabActor || IsOurSpell(fs) != OurSpellTypes::None) {
									has = true;
									break;
								}
							}
						}

						if (has) {
							ourMode |= 1 << i;
						}
					}

					if (ourMode == 3) {
						casters[0]->InterruptCast(true);
						casters[1]->InterruptCast(true);
					}
				}
			}
		}

		{
			std::scoped_lock lock(updateLocker);
			int counter = ++heldUpdateCounter;

			auto ef = GetCurrentRelevantActiveEffects();

			for (auto x : ef) {
				uint32_t handleId = 0;
				if (skyrim_cast<RE::TelekinesisEffect*>(x) != nullptr) {
					handleId = skyrim_cast<RE::TelekinesisEffect*>(x)->grabbedObject.native_handle();
				} else if (skyrim_cast<RE::GrabActorEffect*>(x) != nullptr) {
					handleId = skyrim_cast<RE::GrabActorEffect*>(x)->grabbedActor.native_handle();
				}

				if (handleId == 0) {
					continue;
				}

				std::shared_ptr<HeldObjectData> od = nullptr;
				if (!cachedHeldHandles.contains(handleId)) {
					od = std::make_shared<HeldObjectData>();
					od->objectHandleId = handleId;
					od->isActor = skyrim_cast<RE::GrabActorEffect*>(x) != nullptr;
					od->effect = x->GetBaseObject();
					cachedHeldHandles[handleId] = od;

					std::shared_ptr<SwordInstance> sw = nullptr;
					if (normalSwords->lookup.contains(handleId)) {
						sw = normalSwords->lookup.at(handleId);
						sw->held = true;
						sw->heldTime = time;
						if (normalSwords->forcedGrab == sw) {
							normalSwords->forcedGrab = nullptr;
						}
					} else if (ghostSwords->lookup.contains(handleId)) {
						sw = ghostSwords->lookup.at(handleId);
						sw->held = true;
						sw->heldTime = time;
						if (ghostSwords->forcedGrab == sw) {
							ghostSwords->forcedGrab = nullptr;
						}
					}
				} else {
					std::shared_ptr<SwordInstance> sw = nullptr;
					if (normalSwords->lookup.contains(handleId)) {
						sw = normalSwords->lookup.at(handleId);
						sw->heldTime = time;
					} else if (ghostSwords->lookup.contains(handleId)) {
						sw = ghostSwords->lookup.at(handleId);
						sw->heldTime = time;
					}
				}
				od = cachedHeldHandles.find(handleId)->second;
				od->elapsed += diff;
				od->updateCounter = counter;
			}

			std::vector<unsigned int> rem;
			for (auto& [fst, snd] : cachedHeldHandles) {
				if (snd != nullptr) {
					if (snd->updateCounter != counter) {
						if (rem.empty()) {
							rem = std::vector<unsigned int>();
						}
						rem.push_back(fst);
						continue;
					}

					auto objHolder = RE::TESObjectREFR::LookupByHandle(snd->objectHandleId).get();
					if (objHolder != nullptr) {
						auto ptr = objHolder;
						UpdateHeldObject(ptr, snd, ef);
					} else {
						if (rem.empty()) {
							rem = std::vector<unsigned int>();
						}
						rem.push_back(fst);
					}
				}
			}

			if (!rem.empty()) {
				for (auto u : rem) {
					cachedHeldHandles.erase(u);

					std::shared_ptr<SwordInstance> sw = nullptr;
					if (normalSwords->lookup.contains(u)) {
						sw = normalSwords->lookup.at(u);
						sw->held = false;
					} else if (ghostSwords->lookup.contains(u)) {
						sw = ghostSwords->lookup.at(u);
						sw->held = false;
					}
				}
			}
		}

		UpdateSwordEffects();

		auto main = RE::Main::GetSingleton();
		if (main == nullptr || !main->gameActive) {
			return;
		}

		UpdateAutoLearnSpells();

		if (Config::HoldActorDamage > 0.0) {
			diff = RE::Main::QFrameAnimTime();
			if (diff <= 0.0f) {
				return;
			}

			if (!REL::Module::IsVR()) {
				if (plr->GetPlayerRuntimeData().telekinesisDistance <= 0.0f) {
					return;
				}
			} else {
				if (plr->GetVRPlayerRuntimeData().telekinesisDistance <= 0.0f) {
					return;
				}
			}

			ForeachHeldHandle([&](const std::shared_ptr<HeldObjectData>& dat) {
				if (!dat->isActor) {
					return;
				}
				{
					auto obj = RE::TESObjectREFR::LookupByHandle(dat->objectHandleId).get();
					if (obj != nullptr) {
						auto actor = obj->As<RE::Actor>();
						if (actor != nullptr) {
							float dam = CalculateCurrentTelekinesisDamage(plr, actor) * diff * static_cast<float>(Config::HoldActorDamage);
							if (dam > 0.0f) {
								actor->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kHealth, -dam);
							}
						}
					}
				}
			});
		}
	}

	void BetterTelekinesisPlugin::UpdateAutoLearnSpells()
	{
		auto plr = RE::PlayerCharacter::GetSingleton();

		auto now = GetTickCount64();
		if (now - lastCheckedLearn < 1000) {
			return;
		}

		lastCheckedLearn = now;

		if (Config::AutoLearnTelekinesisSpell) {
			auto spells = Spells;
			if (spells == nullptr || spells->getAll().empty()) {
				return;
			}

			for (auto form : spells->getAll()) {
				auto sp = form->As<RE::SpellItem>();
				if (sp == nullptr) {
					continue;
				}

				if (!plr->HasSpell(sp)) {
					plr->AddSpell(sp);
				}
			}
		}

		if (Config::AutoLearnTelekinesisVariants) {
			auto prim = PrimarySpells;
			if (prim == nullptr) {
				return;
			}

			auto second = SecondarySpells;
			if (second == nullptr) {
				return;
			}

			bool has = false;
			for (auto form : prim->getAll()) {
				auto sp = form->As<RE::SpellItem>();
				if (sp == nullptr) {
					continue;
				}

				if (plr->HasSpell(sp)) {
					has = true;
					break;
				}
			}

			if (!has) {
				return;
			}

			for (auto form : second->getAll()) {
				auto sp = form->As<RE::SpellItem>();
				if (sp == nullptr) {
					continue;
				}

				if (!plr->HasSpell(sp)) {
					plr->AddSpell(sp);
				}
			}
		}
	}


	void BetterTelekinesisPlugin::OnMainMenuOpen()
	{
		spellInfos[static_cast<int>(SpellTypes::reach)] = new SpellInfo(SpellTypes::reach);
		spellInfos[static_cast<int>(SpellTypes::reach)]->Load(Config::SpellInfo_Reach, "SpellInfo_Reach");
		spellInfos[static_cast<int>(SpellTypes::normal)] = new SpellInfo(SpellTypes::normal);
		spellInfos[static_cast<int>(SpellTypes::normal)]->Load(Config::SpellInfo_Normal, "SpellInfo_Normal");
		spellInfos[static_cast<int>(SpellTypes::single)] = new SpellInfo(SpellTypes::single);
		spellInfos[static_cast<int>(SpellTypes::single)]->Load(Config::SpellInfo_One, "SpellInfo_One");
		spellInfos[static_cast<int>(SpellTypes::enemy)] = new SpellInfo(SpellTypes::enemy);
		spellInfos[static_cast<int>(SpellTypes::enemy)]->Load(Config::SpellInfo_NPC, "SpellInfo_NPC");
		spellInfos[static_cast<int>(SpellTypes::blast)] = new SpellInfo(SpellTypes::blast);
		spellInfos[static_cast<int>(SpellTypes::blast)]->Load(Config::SpellInfo_Blast, "SpellInfo_Blast");
		spellInfos[static_cast<int>(SpellTypes::barrage)] = new SpellInfo(SpellTypes::barrage);
		spellInfos[static_cast<int>(SpellTypes::barrage)]->Load(Config::SpellInfo_Barr, "SpellInfo_Barr");

		for (auto& spellInfo : spellInfos) {
			auto b = spellInfo->spellBook;

			if (b != nullptr)
				LeveledListHelper::AddToLeveledList(b);
		}

		auto cac = Util::CachedFormList::TryParse(Config::EffectInfo_Forms, "EffectInfo_Forms", true, false);
		if (cac != nullptr) {
			for (auto x : cac->getAll()) {
				auto ef = x->As<RE::TESEffectShader>();
				if (ef != nullptr)
					effectInfos.push_back(ef);
			}
		}

		cac = Util::CachedFormList::TryParse(Config::SwordReturn_Marker, "SwordReturn_Marker", true, false);
		if (cac != nullptr && !cac->getAll().empty())
			swordReturnMarker = cac->getAll()[0]->As<RE::TESObjectREFR>();

		InitSwords();

		ApplyTelekinesisSettings();

		if (Config::OverwriteTargetPicker) {
			tempPt1 = RE::NiPoint3();
			tempPt2 = RE::NiPoint3();
			tempPt3 = RE::NiPoint3();
			tempPtBegin = RE::NiPoint3();
			tempPtEnd = RE::NiPoint3();

			auto& effects = RE::TESDataHandler::GetSingleton()->GetFormArray(RE::FormType::MagicEffect);
			auto eqpab = RE::TESForm::LookupByID<RE::SpellItem>(0x1A4CA);

			if (eqpab != nullptr) {
				for (auto form : effects) {
					auto set = form->As<RE::EffectSetting>();
					if (set == nullptr || set->GetArchetype() != RE::EffectSetting::Archetype::kGrabActor) {
						continue;
					}

					if (set->data.equipAbility != nullptr) {
						logger::debug(fmt::runtime("Couldn't set " + std::string(form->GetName()) + " equip ability as it already has one! (" + set->data.equipAbility->GetName() + ")"));

						continue;
					}

					set->data.equipAbility = eqpab;
					logger::debug(fmt::runtime("Set " + std::string(form->GetName()) + " equip ability to " + eqpab->GetName()));
				}
			} else {
				logger::debug("Couldn't set any equip ability of grab actor because the telekinesis effect ability is missing!");
			}
		}
	}

	static float MaxHelper()
	{
		auto fpick = Memory::Internal::read<float>(BetterTelekinesisPlugin::addr_PickDistance);
		return std::max(fpick, BetterTelekinesisPlugin::reachSpell);
	}

	static RE::NiNode* GetVRAimNodeHelper(RE::VR_DEVICE device)
	{
		auto plr = RE::PlayerCharacter::GetSingleton();

		if (plr->GetVRPlayerRuntimeData().isRightHandMainHand && device == RE::VR_DEVICE::kLeftController || plr->GetVRPlayerRuntimeData().isLeftHandMainHand && device == RE::VR_DEVICE::kRightController) {
			return plr->GetVRNodeData()->SecondaryMagicAimNode.get();
		}

		return plr->GetVRNodeData()->PrimaryMagicAimNode.get();
	}

	void BetterTelekinesisPlugin::Initialize()
	{
		const auto now = std::chrono::system_clock::now();
		const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
		std::srand(static_cast<unsigned int>(t_c));

		auto& trampoline = SKSE::GetTrampoline();

		// Allow launch object even if not pulled completely.
		uintptr_t addr = RELOCATION_ID(34250, 35052).address() + REL::Relocate(0x332 - 0x250, 0x95, 0xE7);
		struct Patch : CodeGenerator
		{
			Patch(std::uintptr_t a_func, std::uintptr_t a_target, std::uintptr_t a_targetJumpOffset)
			{
				Label funcLabel;
				Label retnLabel;

				Label jump;

				Label NotIf;
				Label NotElse;

				mov(r13, rax);
				mov(rcx, rax);  //Memory::Internal::read<RE::ActiveEffect*>(ctx::AX);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);  //int launch = ShouldLaunchObjectNow(eff);
				add(rsp, 0x20);

				cmp(rax, 0);  //if (launch > 0)
				jle(NotElse);
				test(rax, rax);
				mov(rax, r13);
				jmp(ptr[rip + retnLabel]);  //ctx->IP = ctx::IP + 6;

				L(NotIf);
				cmp(rax, 0);
				jge(NotElse);  // } else if (launch < 0) {
				mov(rax, r13);

				jmp(ptr[rip + jump]);

				L(NotElse);
				mov(rax, r13);
				cmp(byte[rax + 0xA8], 0);

				jmp(ptr[rip + retnLabel]);

				L(jump);
				dq(a_target + 0x7 + a_targetJumpOffset);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x7);
			}
		};
		Patch patch(reinterpret_cast<uintptr_t>(ShouldLaunchObjectNow), addr, REL::Relocate(0x4CB - 0x339, 0x15B, 0x367));
		patch.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch));

		// Allow reach spell.
		addr = RELOCATION_ID(25591, 26127).address() + REL::Relocate(0xB2E1 - 0xA6A0, 0xDBB, 0xC71);
		struct Patch2 : CodeGenerator
		{
			Patch2(std::uintptr_t a_func, uintptr_t a_target)
			{
				Label retnLabel;
				Label funcLabel;

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x8);
			}
		};
		Patch2 patch2(reinterpret_cast<uintptr_t>(MaxHelper), addr);
		patch2.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch2));

		if (Config::DontDeactivateHavokHoldSpring) {
			// Don't allow spring to deactivate.
			addr = RELOCATION_ID(61571, 62469).address() + REL::Relocate(0x9AE - 0x980, 0x2E);
			REL::safe_fill(addr, 0x90, 2);
		}

		//Fix Telekinesis Launch Angle in VR
		if (Config::FixLaunchAngleVR && REL::Module::IsVR()) {
			addr = RELOCATION_ID(34250, 35052).address() + 0x1C4;
			struct Patch4 : CodeGenerator
			{
				Patch4(uintptr_t a_func, uintptr_t a_target)
				{
					Label retnLabel;
					Label funcLabel;

					mov(ecx, esi);

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);

					mov(rcx, rax);
					mov(rdx, r12);

					movups(xmm0, ptr[rcx + 0x7C]);
					movups(ptr[rbp - 0x9], xmm0);

					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target + 0x8);
				}
			};
			Patch4 patch4(reinterpret_cast<uintptr_t>(GetVRAimNodeHelper), addr);
			patch4.ready();

			trampoline.write_branch<5>(addr, trampoline.allocate(patch4));
		}

		if (Config::FixSuperHugeTelekinesisDistanceBug) {
			if (addr = RELOCATION_ID(39474, 40551).address() + REL::Relocate(0x414 - 0x3E0, 0x31, 0x34); REL::make_pattern<"F3 0F 10 05">().match(addr)) {
				REL::safe_fill(addr, 0x90, 16);
			}

			if (addr = RELOCATION_ID(39464, 40541).address() + REL::Relocate(0x116 - 0x050, 0xD0, 0xC6); REL::make_pattern<"F3 0F 10 05">().match(addr)) {
				REL::safe_fill(addr, 0x90, 24);
			}
		}

		if (Config::OverwriteTargetPicker) {
			ApplyOverwriteTargetPicker();
		}

		if (Config::TelekinesisMaxObjects > 1) {
			ApplyMultiTelekinesis();
		}

		origActorDamage = Config::HoldActorDamage;

		if(!Config::TelekinesisDisarmPerk.empty()) {
			auto TelekDisarmPerk = Util::CachedFormList::TryParse(Config::TelekinesisDisarmPerk, "BetterTelekinesis", "TelekinesisDisarmPerk", false);
			if (TelekDisarmPerk != nullptr) {
				for (auto form : TelekDisarmPerk->getAll()) {
					auto perk = form->As<RE::BGSPerk>();
					if (perk != nullptr) {
						disarmPerk = perk;
						break;
					}
				}
			}
		}
	}

	void BetterTelekinesisPlugin::ForeachHeldHandle(const std::function<void(std::shared_ptr<HeldObjectData>)>& func)
	{
		if (func == nullptr) {
			return;
		}

		//Scoped lock likes unlocking an already unlocked mutex
		std::shared_lock lock(cachedHandlesLocker);
		for (auto& val : cachedHeldHandles | std::views::values) {
			func(val);
		}
	}

	float BetterTelekinesisPlugin::CalculateCurrentTelekinesisDamage(RE::PlayerCharacter* ptrPlr, RE::Actor* actorPtr)
	{
		float dam = Memory::Internal::read<float>(addr_TeleDamBase);

		RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModTelekinesisDamage, ptrPlr, actorPtr, dam);
		float damBase = dam;
		dam = Memory::Internal::read<float>(addr_TeleDamMult);

		RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModTelekinesisDamageMult, ptrPlr, actorPtr, dam);
		float damMult = dam;

		float damTotal = damBase * damMult;
		return damTotal;
	}

	void BetterTelekinesisPlugin::OnLaunchActor(RE::Actor* actorPtr)
	{
		if (Config::ThrowActorDamage <= 0.0 || actorPtr == nullptr || actorPtr->formType.underlying() != 62) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		float damTotal = CalculateCurrentTelekinesisDamage(plr, actorPtr) * static_cast<float>(Config::ThrowActorDamage);
		if (damTotal > 0.0f) {
			actorPtr->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kHealth, -damTotal);
		}
	}

	void BetterTelekinesisPlugin::ApplyTelekinesisSettings()
	{
		Util::WriteFloatMult(506184, 376031, static_cast<float>(Config::BaseDistanceMultiplier));
		Util::WriteFloatMult(506190, 376040, static_cast<float>(Config::BaseDamageMultiplier));
		Util::WriteFloat(506149, 375978, static_cast<float>(Config::ObjectPullSpeedBase));
		Util::WriteFloat(506151, 375981, static_cast<float>(Config::ObjectPullSpeedAccel));
		Util::WriteFloat(506153, 375984, static_cast<float>(Config::ObjectPullSpeedMax));
		Util::WriteFloatMult(506157, 375990, static_cast<float>(Config::ObjectThrowForce));
		Util::WriteFloat(506196, 376049, static_cast<float>(Config::ActorPullSpeed));
		Util::WriteFloatMult(506199, 376054, static_cast<float>(Config::ActorThrowForce));
		Util::WriteFloatMult(506155, 375987, static_cast<float>(Config::ObjectHoldDistance));
		Util::WriteFloatMult(506194, 376046, static_cast<float>(Config::ActorHoldDistance));

		FindNearestNodeHelper::Init();

		if (!Config::TelekinesisSpells.empty()) {
			Spells = Util::CachedFormList::TryParse(Config::TelekinesisSpells, "BetterTelekinesis", "TelekinesisSpells", false);
		}
		if (!Config::TelekinesisPrimary.empty()) {
			PrimarySpells = Util::CachedFormList::TryParse(Config::TelekinesisPrimary, "BetterTelekinesis", "TelekinesisPrimary", false);
		}
		if (!Config::TelekinesisSecondary.empty()) {
			SecondarySpells = Util::CachedFormList::TryParse(Config::TelekinesisSecondary, "BetterTelekinesis", "TelekinesisSecondary", false);
		}

		if (Config::OverwriteTelekinesisSpellBaseCost >= 0.0) {
			int cost = static_cast<int>(std::round(Config::OverwriteTelekinesisSpellBaseCost));
			if (Spells != nullptr) {
				for (auto x : Spells->getAll()) {
					auto spell = x->As<RE::SpellItem>();
					if (spell == nullptr) {
						continue;
					}

					auto fl = spell->GetData()->flags;
					spell->GetData()->flags = fl | 1;
					spell->GetData()->costOverride = cost;
				}
			}
		}

		if (Config::ResponsiveHold) {
			std::vector<float> ls;
			for (int i = 0; i < 2; i++) {
				std::string prls = i == 0 ? (!Config::ResponsiveHoldParams.empty() ? Config::ResponsiveHoldParams : "") : Config::getDefaultResponsiveHoldParameters();
				auto spl = Util::StringHelpers::split(prls, ' ', true);
				ls = std::vector<float>();
				for (auto& x : spl) {
					try {
						float fx = std::stof(x);
						ls.push_back(fx);
					} catch (...) {
						ls = std::vector<float>();
						break;
					}
				}

				if (!ls.empty() && ls.size() == 8) {
					break;
				}
			}

			if (!ls.empty() && ls.size() == 8) {
				// elasticity
				Util::WriteFloat(506169, 376008, ls[0]);
				Util::WriteFloat(506161, 375996, ls[1]);

				// spring damping
				Util::WriteFloat(506167, 376005, ls[2]);
				Util::WriteFloat(506159, 375993, ls[3]);

				// object damping
				Util::WriteFloat(506171, 376011, ls[4]);
				Util::WriteFloat(506163, 375999, ls[5]);

				// max force
				Util::WriteFloat(506173, 376014, ls[6]);
				Util::WriteFloat(506165, 376002, ls[7]);
			}
		}
	}

	void BetterTelekinesisPlugin::TryDropNow()
	{
		auto gameMain = RE::Main::GetSingleton();
		if (gameMain == nullptr || !gameMain->gameActive) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		bool didTimer = false;
		for (int i = 0; i <= 2; i++) {
			auto caster = plr->GetMagicCaster(static_cast<RE::MagicSystem::CastingSource>(i));
			if (caster == nullptr) {
				continue;
			}

			auto item = caster->currentSpell;
			if (item == nullptr) {
				continue;
			}

			auto& effls = item->effects;
			if (effls.empty()) {
				continue;
			}

			bool had = false;
			for (auto ef : effls) {
				auto set = ef->baseEffect;
				if (set == nullptr) {
					continue;
				}

				switch (set->GetArchetype()) {
				case RE::EffectSetting::Archetype::kTelekinesis:
				case RE::EffectSetting::Archetype::kGrabActor:
					{
						had = true;
						break;
					}
				default:
					break;
				}

				if (had) {
					break;
				}
			}

			if (!had) {
				continue;
			}

			if (!didTimer) {
				dropTimer = GetTickCount();
				didTimer = true;
			}
			caster->InterruptCast(true);
		}
	}

	std::vector<RE::ActiveEffect*> BetterTelekinesisPlugin::GetCurrentRelevantActiveEffects()
	{
		auto ls = std::vector<RE::ActiveEffect*>();

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr != nullptr) {
			if (!REL::Module::IsVR()) {
				auto efls = plr->AsMagicTarget()->GetActiveEffectList();
				if (efls != nullptr) {
					for (auto ef : *efls) {
						auto effectSetting = ef->GetBaseObject();
						if (effectSetting->GetArchetype() == RE::EffectSetting::Archetype::kTelekinesis) {
							ls.push_back(ef);
						} else if (effectSetting->GetArchetype() == RE::EffectSetting::Archetype::kGrabActor) {
							ls.push_back(ef);
						}
					}
				}
			} else {
				plr->AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* activeEffect) -> RE::BSContainer::ForEachResult {
					if (auto mgef = activeEffect ? activeEffect->GetBaseObject() : nullptr; mgef) {
						if (activeEffect->flags.all(RE::ActiveEffect::Flag::kInactive) || activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
							return RE::BSContainer::ForEachResult::kContinue;
						}

						auto effectSetting = activeEffect->GetBaseObject();
						if (effectSetting->GetArchetype() == RE::EffectSetting::Archetype::kTelekinesis) {
							ls.push_back(activeEffect);
						} else if (effectSetting->GetArchetype() == RE::EffectSetting::Archetype::kGrabActor) {
							ls.push_back(activeEffect);
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
		}

		return ls;
	}

	void BetterTelekinesisPlugin::ForceUpdateTelekinesis()
	{
		nextUpdateTelek = true;
	}

	bool BetterTelekinesisPlugin::ShouldUpdateTelekinesis(const uint32_t now)
	{
		if (nextUpdateTelek) {
			nextUpdateTelek = false;
			lastUpdateTelek = now;
			return true;
		}

		if (Config::TelekinesisTargetOnlyUpdateIfWeaponOut) {
			auto plr = RE::PlayerCharacter::GetSingleton();
			if (plr != nullptr && plr->AsActorState()->IsWeaponDrawn()) {
				if (!lastWeaponOut) {
					lastWeaponOut = true;
					lastUpdateTelek = now;
					return true;
				}
			} else {
				lastWeaponOut = false;
				return false;
			}
		}

		auto delay = static_cast<uint32_t>(Config::TelekinesisTargetUpdateInterval * 1000.0);
		if (now - lastUpdateTelek >= delay) {
			lastUpdateTelek = now;
			return true;
		}

		return false;
	}

	static unsigned int GetHandleId()
	{
		unsigned int handleId = 0;
		{
			std::scoped_lock lock(lockerPicked);
			if (!BetterTelekinesisPlugin::grabactorPicked.empty()) {
				std::unordered_set<unsigned int> alreadyChosen;
				bool hasBad = false;
				BetterTelekinesisPlugin::ForeachHeldHandle([&](const std::shared_ptr<BetterTelekinesisPlugin::HeldObjectData>& dat) {
					if (hasBad) {
						return;
					}

					if (dat->effect != nullptr || !dat->isActor) {
						hasBad = true;
					} else {
						if (alreadyChosen.empty()) {
							alreadyChosen = std::unordered_set<unsigned int>();
						}
						alreadyChosen.insert(dat->objectHandleId);
					}
				});

				if (!hasBad) {
					for (auto x : BetterTelekinesisPlugin::grabactorPicked) {
						if (alreadyChosen.empty() || !alreadyChosen.contains(x)) {
							handleId = x;
							break;
						}
					}
				}
			}
		}

		BetterTelekinesisPlugin::ForceUpdateTelekinesis();

		return handleId;
	}

	void BetterTelekinesisPlugin::ApplyOverwriteTargetPicker()
	{
		auto addr = RELOCATION_ID(33677, 34457).address() + REL::Relocate(0x10A5 - 0x1010, 0x9A, 0xA2);
		struct Patch : CodeGenerator
		{
			Patch(std::uintptr_t a_func, uintptr_t a_target, std::uintptr_t a_rbpOffset, std::uintptr_t a_targetOffset)
			{
				Label retnLabel;
				Label funcLabel;

				Label NotIf;

				cmp(r13b, 2);  //if (ctx::R13::ToUInt8() == 2) {
				jne(NotIf);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(ecx, eax);
				mov(ptr[rbp + a_rbpOffset], eax);

				jmp(ptr[rip + retnLabel]);

				L(NotIf);
				if (!REL::Module::IsVR()) {
					mov(ecx, ptr[rax + 4]);
					mov(ptr[rbp + a_rbpOffset], ecx);
				} else {
					mov(eax, ptr[rbx + rcx * 4 + 4]);
					mov(ptr[rbp + a_rbpOffset], eax);
				}

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + a_targetOffset);
			}
		};
		Patch patch(reinterpret_cast<uintptr_t>(GetHandleId), addr, REL::Relocate(0x390, 0x360, 0x340), REL::Relocate(0x9, 0x9, 0xA));
		patch.ready();

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_branch<5>(addr, trampoline.allocate(patch));

		addr = RELOCATION_ID(33669, 34449).address() + REL::Relocate(0x500D9 - 0x4FFF0, 0xEB, 0xE9);
		Memory::Internal::write<uint8_t>(addr + 1, 2, true);

		auto col = std::vector{ RE::COL_LAYER::kGround, RE::COL_LAYER::kTerrain, RE::COL_LAYER::kClutter, RE::COL_LAYER::kStatic, RE::COL_LAYER::kWeapon, RE::COL_LAYER::kBiped, RE::COL_LAYER::kActorZone, RE::COL_LAYER::kBipedNoCC };

		for (auto x : col) {
			unsigned long long fl = static_cast<unsigned long long>(1) << static_cast<int>(x);
			RaycastHelper::RaycastMask |= fl;
		}
	}

	BetterTelekinesisPlugin::SpellInfo* BetterTelekinesisPlugin::SpellInfo::Load(const std::string& str, const std::string& setting)
	{
		if (str.empty()) {
			return this;
		}

		auto spl = Util::StringHelpers::split(str, ';', false);
		if (spl.size() < 3) {
			return this;
		}

		for (int i = 0; i < spl.size(); i++) {
			if (spl[i].empty()) {
				continue;
			}

			auto cac = Util::CachedFormList::TryParse(spl[i], setting, true, false);
			if (cac != nullptr && cac->getAll().size() == 1) {
				if (i == 0) {
					this->spellBook = cac->getAll()[0]->As<RE::TESObjectBOOK>();
				} else if (i == 1) {
					this->spell = cac->getAll()[0]->As<RE::SpellItem>();
				} else if (i == 2) {
					this->effect = cac->getAll()[0]->As<RE::EffectSetting>();
				}
			}
		}

		switch (this->type) {
		case SpellTypes::blast:
			{
				auto mspl = Util::StringHelpers::split(!Config::Blast_SwordModel.empty() ? Config::Blast_SwordModel : "", ';', true);
				if (mspl.empty()) {
					mspl = { R"(Weapons\Iron\LongSword.nif)" };
				}

				std::string fname = "BetterTelekinesis.esp";
				int mi = 0;

				ProduceItem(0x805, fname, mspl[mi++ % mspl.size()]);
				for (unsigned int u = 0x88C; u <= 0x8BC; u++) {
					ProduceItem(u, fname, mspl[mi++ % mspl.size()]);
				}
			}
			break;

		case SpellTypes::barrage:
			{
				auto mspl = Util::StringHelpers::split(!Config::Barrage_SwordModel.empty() ? Config::Barrage_SwordModel : "", ';', true);
				if (mspl.empty()) {
					mspl = { R"(Weapons\Iron\LongSword.nif)" };
				}

				std::string fname = "BetterTelekinesis.esp";
				int mi = 0;

				ProduceItem(0x804, fname, mspl[mi++ % mspl.size()]);
				for (unsigned int u = 0x87B; u <= 0x88B; u++) {
					ProduceItem(u, fname, mspl[mi++ % mspl.size()]);
				}
				for (unsigned int u = 0x8BD; u <= 0x8DC; u++) {
					ProduceItem(u, fname, mspl[mi++ % mspl.size()]);
				}
			}
			break;
		default:
			break;
		}

		return this;
	}

	void BetterTelekinesisPlugin::SpellInfo::ProduceItem(RE::FormID formId, const std::string& formFile, const std::string& model)
	{
		auto form = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(formId, formFile);
		if (form == nullptr) {
			return;
		}

		if (this->item.empty()) {
			this->item = std::unordered_set<RE::FormID>();
		}

		this->item.insert(form->GetFormID());
		form->SetModel(model.c_str());
	}

	void BetterTelekinesisPlugin::BeginProfile()
	{
		if (!Config::DebugLogMode) {
			return;
		}

		if (profileTimer == nullptr) {
			profileTimer = std::make_unique<stopwatch::Stopwatch>();
			profileTimer->start();
		}

		profileLast = profileTimer->elapsed<>();
		profileIndex = 0;
	}

	void BetterTelekinesisPlugin::StepProfile()
	{
		if (!Config::DebugLogMode) {
			return;
		}

		uint64_t t = profileTimer->elapsed<>();
		uint64_t diff = t - profileLast;
		profileLast = t;
		profileTimes[profileIndex++] += diff;
	}

	void BetterTelekinesisPlugin::EndProfile()
	{
		if (!Config::DebugLogMode) {
			return;
		}

		profileCounter++;

		uint32_t now = GetTickCount();
		if (now - profileReport < 3000) {
			return;
		}

		profileReport = now;
		std::string bld;
		for (int i = 0; i < profileTimes.size(); i++) {
			auto tot = profileTimes[i];
			if (tot == 0) {
				continue;
			}

			double avg = static_cast<double>(tot) / static_cast<double>(profileCounter);

			if (bld.empty() != 0) {
				bld.append("  ");
			}
			bld.append(fmt::format(fmt::runtime("[" + std::to_string(i) + "] = {.3f}"), avg));
		}

		logger::debug(fmt::runtime(bld + " <- " + std::to_string(profileCounter)));
	}

	bool BetterTelekinesisPlugin::IsCellWithinDistance(const float myX, const float myY, const int coordX, const int coordY, const float maxDist)
	{
		float minX = static_cast<float>(coordX) * 4096.0f;
		float maxX = static_cast<float>(coordX + 1) * 4096.0f;
		float minY = static_cast<float>(coordY) * 4096.0f;
		float maxY = static_cast<float>(coordY + 1) * 4096.0f;

		float smallestDist;
		if (myX < minX) {
			smallestDist = minX - myX;
		} else if (myX > maxX) {
			smallestDist = myX - maxX;
		} else {
			return true;
		}

		if (myY < minY) {
			smallestDist = std::min(smallestDist, minY - myY);
		} else if (myY > maxY) {
			smallestDist = std::min(smallestDist, myY - maxY);
		} else {
			return true;
		}

		return smallestDist < maxDist;
	}

	std::vector<RE::EffectSetting*> BetterTelekinesisPlugin::CalculateCasting()
	{
		auto ls = std::vector<RE::EffectSetting*>();
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr != nullptr) {
			for (int i = 0; i < 2; i++) {
				auto caster = plr->GetMagicCaster(static_cast<RE::MagicSystem::CastingSource>(i));
				if (caster != nullptr) {
					switch (caster->state.get()) {
					case RE::MagicCaster::State::kCharging:
					case RE::MagicCaster::State::kCasting:  //Concentrating?
						{
							auto ef = caster->currentSpell;
							if (ef != nullptr) {
								auto& efls = ef->effects;
								if (efls.empty()) {
									for (auto x : efls) {
										auto xe = x->baseEffect;
										if (xe != nullptr) {
											ls.push_back(xe);
										}
									}
								}
							}
						}
						break;
					default:
						break;
					}
				}
			}
		}

		return ls;
	}

	void BetterTelekinesisPlugin::DisarmActor(RE::Actor* who)
	{
		if (who == nullptr || who->IsPlayerRef()) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		//if (who->IsStaggering()) return;

		auto taskPool = RE::TaskQueueInterface::GetSingleton();

		RE::ActorHandle whoHandle = who->GetHandle();
		RE::ActorHandle plrHandle = plr->GetHandle();

		taskPool->QueueActorDisarm(whoHandle, plrHandle);
	}

	void BetterTelekinesisPlugin::OverwriteTelekinesisTargetPick()
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		if (Config::DebugLogMode) {
			uint32_t now = GetTickCount();
			if (now - lastDebugPick >= 1000) {
				lastDebugPick = now;
				debugPick = true;

				logger::debug(fmt::runtime("================================= (" + std::to_string(GetCurrentRelevantActiveEffects().size()) + ")"));
			} else {
				debugPick = false;
			}
		}

		// Not doing telekinesis then don't care?
		float maxDistance = 0.0f;

		if (!REL::Module::IsVR()) {
			maxDistance = plr->GetPlayerRuntimeData().telekinesisDistance;
		} else {
			maxDistance = plr->GetVRPlayerRuntimeData().telekinesisDistance;
		}
		if (maxDistance <= 0.0f) {
			if (Config::DebugLogMode && debugPick) {
				logger::debug("Not doing telekinesis");
			}
			return;
		}

		auto pcam = RE::PlayerCamera::GetSingleton();
		if (pcam == nullptr) {
			return;
		}

		auto& camNode = pcam->cameraRoot;
		if (camNode == nullptr) {
			return;
		}

		auto cell = plr->GetParentCell();
		if (cell == nullptr || !cell->IsAttached()) {
			return;
		}

		auto tes = RE::TES::GetSingleton();
		if (tes == nullptr) {
			return;
		}
		auto plrNodes = std::vector{ plr->Get3D1(true), plr->Get3D1(false) };

		std::unordered_set<RE::RefHandle> ignoreHandles;

		ForeachHeldHandle([&](const std::shared_ptr<HeldObjectData>& dat) {
			{
				auto objHold = RE::TESObjectREFR::LookupByHandle(dat->objectHandleId).get();
				if (objHold != nullptr) {
					auto rootObj = objHold->Get3D();
					if (rootObj != nullptr) {
						plrNodes.push_back(rootObj);
					}
					ignoreHandles.insert(objHold->GetHandle().native_handle());
				}
			}
		});

		auto camPos = camNode->world.translate;

		auto beginHead = glm::vec4();

		auto unwantedCameraState = REL::Module::IsVR() ? RE::CameraStates::kVR : RE::CameraStates::kFirstPerson;

		if (plrNodes[1] != nullptr && pcam->currentState != nullptr && pcam->currentState->id != unwantedCameraState) {
			auto headNode = plrNodes[1]->GetObjectByName("NPC Head [Head]");
			if (headNode != nullptr) {
				auto headPos = headNode->world.translate;

				beginHead.x = headPos.x;
				beginHead.y = headPos.y;
				beginHead.z = headPos.z;
			}
		}

		auto beginCam = glm::vec4(camPos.x, camPos.y, camPos.z, 1.0f);

		auto TempPt = RE::NiPoint3();
		TempPt.y = maxDistance;
		TempPt = Util::Translate(camNode->world, TempPt);
		auto end = glm::vec4(TempPt.x, TempPt.y, TempPt.z, 1.0f);

		RaycastHelper::do_split_raycast(beginHead, beginCam, end, cell, plrNodes);

		glm::vec4 begin = beginHead.x != 0.0f && beginHead.y != 0.0f && beginHead.z != 0.0f ? beginHead : beginCam;

		int findMask = 3;  // 1 = objects, 2 = actors
		// TODO: only find object or actor?

		tempPtBegin.x = begin[0];
		tempPtBegin.y = begin[1];
		tempPtBegin.z = begin[2];
		tempPtEnd.x = end[0];
		tempPtEnd.y = end[1];
		tempPtEnd.z = end[2];

		auto data = std::make_shared<TelekCalcData>();
		data->begin = begin;
		data->end = end;
		data->chls = std::vector<std::unique_ptr<TelekObjData>>();
		data->findMask = findMask;
		data->maxDistance = maxDistance;
		data->ignore = std::unordered_set<RE::NiNode*>();
		for (auto n : plrNodes) {
			data->ignore.insert(n->AsNode());
		}
		data->ignoreHandle = ignoreHandles;
		data->casting = CalculateCasting();

		FindBestTelekinesis(cell, data);

		if (!cell->IsInteriorCell()) {
			for (uint32_t x = 0; x < tes->gridCells->length; ++x) {
				for (uint32_t y = 0; y < tes->gridCells->length; ++y) {
					if (auto c = tes->gridCells->GetCell(x, y)) {
						if (cell->formID == c->formID) {
							continue;
						}

						if (!IsCellWithinDistance(begin[0], begin[1], c->GetCoordinates()->cellX, c->GetCoordinates()->cellY, maxDistance)) {
							continue;
						}

						FindBestTelekinesis(c, data);
					}
				}
			}
		}

		if (data->chls.empty()) {
			if (Config::DebugLogMode && debugPick) {
				logger::debug("Didn't find any valid object for ray pick");
			}

			return;
		}

		if (data->chls.size() > 1) {
			std::ranges::sort(data->chls, {}, &TelekObjData::distFromRay);
		}

		int objLeftTake = 1;
		int actorLeftTake = 1;

		for (int i = 0; i < data->chls.size() && (objLeftTake > 0 || actorLeftTake > 0); i++) {
			auto odata = std::move(data->chls[i]);

			bool isActor = false;
			if (odata->obj->As<RE::Actor>() != nullptr) {
				isActor = true;
				if (actorLeftTake == 0) {
					continue;
				}
			} else {
				if (objLeftTake == 0) {
					continue;
				}
			}

			// Make sure it's in line of sight to us.
			auto End = glm::vec4{ odata->x, odata->y, odata->z, 0.0f };
			auto ray = Raycast::hkpCastRay(data->begin, End);

			auto addedNode = odata->obj->Get3D();
			if (addedNode != nullptr) {
				data->ignore.insert(addedNode->AsNode());
			}

			bool hasLos = true;
			for (auto& [normal, hitFraction, body] : ray.hitArray) {
				if (body == nullptr) {
					continue;
				}

				const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
				const auto flags = collisionObj->GetCollisionLayer();
				unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
				if ((RaycastHelper::RaycastMask & mask) == 0) {
					continue;
				}

				if (auto cobj = Raycast::getAVObject(body)) {
					if (data->ignore.contains(cobj->AsNode())) {
						continue;
					}
				}

				hasLos = false;
				break;
			}

			if (addedNode != nullptr) {
				data->ignore.erase(addedNode->AsNode());
			}

			if (!hasLos) {
				if (Config::DebugLogMode && debugPick) {
					logger::debug(fmt::runtime("Checked BAD object (no LOS): " + std::string(odata->obj->GetName())));
				}

				continue;
			}

			auto objRefHold = RE::TESObjectREFR::LookupByHandle(odata->obj->GetHandle().native_handle()).get();
			if (objRefHold != nullptr) {
				if (isActor) {
					grabactorPicked.push_back(objRefHold->GetHandle().native_handle());
					if (Config::DebugLogMode && debugPick) {
						logger::debug(fmt::runtime("Returned actor: " + std::string(odata->obj->GetName()) + "; dist = " + std::to_string(odata->distFromRay)));
					}
					actorLeftTake--;
				} else {
					telekinesisPicked.push_back(objRefHold->GetHandle().native_handle());
					if (Config::DebugLogMode && debugPick) {
						logger::debug(fmt::runtime("Returned object: " + std::string(odata->obj->GetName()) + "; dist = " + std::to_string(odata->distFromRay)));
					}
					objLeftTake--;
				}
			}

			data->chls[i] = std::move(odata);
		}
	}

	void BetterTelekinesisPlugin::ProcessOneObj(RE::TESObjectREFR* obj, const std::shared_ptr<TelekCalcData>& data, float quickMaxDist)
	{
		// Very quick check to save resources.
		auto opos = obj->GetPosition();
		float objBaseX = opos.x;
		float objBaseY = opos.y;
		float dx = objBaseX - data->begin[0];
		float dy = objBaseY - data->begin[1];

		if ((dx * dx + dy * dy) > quickMaxDist) {
			return;
		}

		float objBaseZ = opos.z;

		StepProfile();  // end of 0

		unsigned int formFlag = obj->formFlags;
		if (formFlag & RE::TESForm::RecordFlags::kDisabled || formFlag & RE::TESForm::RecordFlags::kDeleted) {
			return;
		}

		StepProfile();  // end of 1

		auto objHolder = obj->GetHandle();
		if (!obj->IsHandleValid()) {
			return;
		}
		unsigned int thisHandle = objHolder.native_handle();
		if (std::ranges::find(data->ignoreHandle, thisHandle) != data->ignoreHandle.end()) {
			return;
		}

		StepProfile();  // end of 2

		auto actor = obj->As<RE::Actor>();
		if ((data->findMask & 2) == 0 && actor != nullptr) {
			return;
		}

		if ((data->findMask & 1) == 0 && actor == nullptr) {
			return;
		}

		if (actor != nullptr) {
			if (actor->IsPlayer()) {
				return;
			}

			if (Config::DontPickFriendlyTargets == 1) {
				if (actor->IsPlayerTeammate()) {
					return;
				}
			} else if (Config::DontPickFriendlyTargets == 2) {
				if (actor->IsPlayerTeammate() || !actor->IsHostileToActor(RE::PlayerCharacter::GetSingleton())) {
					return;
				}
			}
		}

		StepProfile();  // end of 3

		tempPt1.x = 0.0f;
		tempPt1.y = 0.0f;
		tempPt1.z = 0.0f;
		tempPt1 = obj->GetBoundMin();

		tempPt2.x = 0.0f;
		tempPt2.y = 0.0f;
		tempPt2.z = 0.0f;
		tempPt2 = obj->GetBoundMax();

		StepProfile();  // end of 4

		// This isn't perfect way to do it in case object is rotated strangely but those are not common cases.
		tempPt1.x = objBaseX + ((tempPt2.x - tempPt1.x) * 0.5f + tempPt1.x);
		tempPt1.y = objBaseY + ((tempPt2.y - tempPt1.y) * 0.5f + tempPt1.y);
		tempPt1.z = objBaseZ + ((tempPt2.z - tempPt1.z) * 0.5f + tempPt1.z);

		float objTotalDist = tempPtBegin.GetDistance(tempPt1);

		if (objTotalDist > data->maxDistance) {
			return;
		}

		StepProfile();  // end of 5
		tempPt2 = tempPtBegin - tempPt1;
		tempPt3 = tempPtEnd - tempPtBegin;
		float dot = tempPt2.Dot(tempPt3);
		tempPt2 = tempPtEnd - tempPtBegin;
		float len = tempPt2.Length();
		len *= len;
		float t = -1.0f;
		if (len > 0.0f) {
			t = -(dot / len);
		}

		float distResult = 999999.0f;
		if (t > 0.0f && t < 1.0f) {
			tempPt2 = tempPt1 - tempPtBegin;   // TempPt1 - TempPtBegin -> TempPt2
			tempPt3 = tempPt1 - tempPtEnd;     // TempPt1 - TempPtEnd -> TempPt3
			tempPt2 = tempPt2.Cross(tempPt3);  // TempPt2 X TempPt3 -> TempPt2
			float dist1 = tempPt2.Length();
			tempPt3 = tempPtBegin - tempPtEnd;
			float dist2 = tempPt3.Length();
			if (dist2 > 0.0f) {
				distResult = dist1 / dist2;
			}
		} else {
			tempPt2 = tempPtBegin - tempPt1;
			float dist1 = tempPt2.Length();
			tempPt2 = tempPtEnd - tempPt1;
			float dist2 = tempPt2.Length();
			distResult = std::min(dist1, dist2);
		}

		double maxDistFromRay = actor != nullptr ? Config::ActorTargetPickerRange : Config::ObjectTargetPickerRange;
		if (distResult > maxDistFromRay) {
			return;
		}

		StepProfile();  // end of 6

		// Verify object.
		if (actor == nullptr) {
			REL::Relocation<bool (*)(RE::TESObjectREFR*)> CanBeTelekinesis{ addr_CanBeTelekinesis };
			if (!CanBeTelekinesis(obj)) {
				return;
			}
		}

		StepProfile();  // end of 7

		if (!CanPickTelekinesisTarget(obj, data->casting)) {
			return;
		}

		StepProfile();  // end of 8

		auto odata = std::make_unique<TelekObjData>();
		odata->obj = obj;
		odata->distFromRay = distResult;
		odata->x = tempPt1.x;
		odata->y = tempPt1.y;
		odata->z = tempPt1.z;
		data->chls.push_back(std::move(odata));

		StepProfile();  // end of 9
	}

	void BetterTelekinesisPlugin::FindBestTelekinesis(RE::TESObjectCELL* cell, const std::shared_ptr<TelekCalcData>& data)
	{
		float quickMaxDist = data->maxDistance + 500.0f;
		quickMaxDist *= quickMaxDist;

		cell->GetRuntimeData().spinLock.Lock();

		try {
			auto& refs = cell->GetRuntimeData().references;
			if (!refs.empty()) {
				for (auto& obj : refs) {
					if (obj == nullptr) {
						continue;
					}

					REL::Relocation<bool (*)(RE::TESObjectREFR*)> CanBeTelekinesis{ addr_CanBeTelekinesis };
					if (CanBeTelekinesis(obj.get()) || Config::TelekinesisDisarmsEnemies && obj->IsActor()) {
						BeginProfile();
						ProcessOneObj(obj.get(), data, quickMaxDist);
						//arrow_debug = false;
						EndProfile();
					}
				}
			}

		} catch (...) {
			logger::error("Exception occured while processing cell references. Ignoring for now.");
		}

		cell->GetRuntimeData().spinLock.Unlock();
	}

	int BetterTelekinesisPlugin::GetCurrentTelekinesisObjectCount(int valueIfActorGrabbed)
	{
		int hasObj = 0;
		bool hadActor = false;
		ForeachHeldHandle([&](const std::shared_ptr<HeldObjectData>& dat) {
			if (hadActor) {
				return;
			}

			if (dat->isActor) {
				hadActor = true;
			} else {
				hasObj++;
			}
		});
		if (hadActor) {
			return valueIfActorGrabbed;
		}
		return hasObj;
	}

	static void ClearGrabbedObjectsHelper()
	{
		auto cg = BetterTelekinesisPlugin::currentGrabindex;
		if (cg != 0) {
			if (BetterTelekinesisPlugin::dontCallClear == 0) {
				BetterTelekinesisPlugin::FreeGrabIndex(cg, "unexpected clear grabbed objects");
			}
		}
	}

	static void ClearGrabbedObjectsHelper2(uintptr_t effect)
	{
		BetterTelekinesisPlugin::FreeGrabIndex(effect, "dtor");
	}

	static void TelekinesisApplyHelper(uintptr_t effect)
	{
		BetterTelekinesisPlugin::SwitchToGrabIndex(effect, "add effect");
		BetterTelekinesisPlugin::dontCallClear++;
	}

	static void TelekinesisApplyHelper2()
	{
		BetterTelekinesisPlugin::SwitchToGrabIndex(0, "add effect finished");
		BetterTelekinesisPlugin::dontCallClear--;
	}

	static void TelekinesisApplyHelper3(uintptr_t effect)
	{
		BetterTelekinesisPlugin::SwitchToGrabIndex(effect, "end of effect launch");
	}

	static void TelekinesisApplyHelper4(uintptr_t addr)
	{
		BetterTelekinesisPlugin::FreeGrabIndex(addr, "end of effect launch finished");
		BetterTelekinesisPlugin::SwitchToGrabIndex(0, "end of effect launch finished");
	}

	static void TelekinesisApplyHelper5(uintptr_t effect)
	{
		float diff = RE::Main::QFrameAnimTime();
		BetterTelekinesisPlugin::SwitchToGrabIndex(effect, "update begin", diff);
		BetterTelekinesisPlugin::dontCallClear++;
	}

	static void TelekinesisApplyHelper6()
	{
		BetterTelekinesisPlugin::SwitchToGrabIndex(0, "update end");
		BetterTelekinesisPlugin::dontCallClear--;
	}

	static bool TelekinesisApplyHelper7(uintptr_t rax, uintptr_t* rdx)
	{
		auto adding = rax;
		auto before = Memory::Internal::read<uintptr_t>(rdx + 0x48);

		if (adding == before) {
			auto item = Memory::Internal::read<uintptr_t>(before + 0x10);  // EffectItem
			if (item != 0) {
				auto ie = Memory::Internal::read<RE::EffectSetting*>(item + 0x10);  // EffectItem.effectSetting

				if (ie != nullptr && ie->GetArchetype() == RE::EffectSetting::Archetype::kTelekinesis) {
					if (BetterTelekinesisPlugin::IsOurSpell(ie) != BetterTelekinesisPlugin::OurSpellTypes::TelekOne) {
						return false;
					}
				}
			}
			return true;
		}
		return false;
	}

	static bool TelekinesisApplyHelper8(RE::ActiveEffect* ef)
	{
		if (ef == nullptr)
			return false;

		auto SpellType = BetterTelekinesisPlugin::IsOurSpell(ef->GetBaseObject());
		if (SpellType != BetterTelekinesisPlugin::OurSpellTypes::TelekOne && SpellType != BetterTelekinesisPlugin::OurSpellTypes::None) {
			if (Config::TelekinesisMaxObjects < 99) {
				if (BetterTelekinesisPlugin::GetCurrentTelekinesisObjectCount() >= Config::TelekinesisMaxObjects) {
					return false;
				}
			}
			return true;
		}

		return false;
	}

	static bool TelekinesisApplyHelper10(uintptr_t rdi, const float xmm1)
	{
		if (xmm1 > 0.0f) {
			auto ef = Memory::Internal::read<RE::ActiveEffect*>(rdi);
			if (reinterpret_cast<RE::TelekinesisEffect*>(ef) != nullptr) {
				auto plr = RE::PlayerCharacter::GetSingleton();
				if (plr != nullptr) {
					RE::ActiveEffect* ef2 = nullptr;
					RE::EffectSetting* setting = nullptr;
					if (!REL::Module::IsVR()) {
						if (auto efs = plr->AsMagicTarget()->GetActiveEffectList()) {
							for (auto& effect : *efs) {
								setting = effect ? effect->GetBaseObject() : nullptr;
								if (setting && setting->HasArchetype(RE::EffectSetting::Archetype::kTelekinesis)) {
									ef2 = effect;
									break;
								}
							}
						}
					} else {
						plr->AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* activeEffect) -> RE::BSContainer::ForEachResult {
							if (auto mgef = activeEffect ? activeEffect->GetBaseObject() : nullptr; mgef) {
								if (activeEffect->flags.all(RE::ActiveEffect::Flag::kInactive) || activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
									return RE::BSContainer::ForEachResult::kContinue;
								}

								setting = activeEffect ? activeEffect->GetBaseObject() : nullptr;
								if (setting && setting->HasArchetype(RE::EffectSetting::Archetype::kTelekinesis)) {
									ef2 = activeEffect;
									return RE::BSContainer::ForEachResult::kStop;
								}
							}
							return RE::BSContainer::ForEachResult::kContinue;
						});
					}

					if (ef2 != nullptr && ef2 != ef) {
						return true;
					}
				}
			}
		}

		return false;
	};

	void BetterTelekinesisPlugin::ApplyMultiTelekinesis()
	{
		auto& trampoline = SKSE::GetTrampoline();

		// Clear grab objects func itself.
		uintptr_t addr = RELOCATION_ID(39480, 40557).address() + REL::Relocate(0x30, 0x30, 0x31);

		struct Patch : CodeGenerator
		{
			Patch(std::uintptr_t a_func, uintptr_t a_target, Reg64 a_movReg, Reg64 a_rcxStoreReg)
			{
				Label retnLabel;
				Label funcLabel;

				mov(r13, r8);
				mov(r14, rax);
				mov(r15, rdx);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(r8, r13);
				mov(rax, r14);
				mov(rdx, r15);
				mov(rcx, a_rcxStoreReg);

				mov(a_movReg, 0x768);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x5);
			}
		};
		Patch patch(reinterpret_cast<uintptr_t>(ClearGrabbedObjectsHelper), addr, Reg64(REL::Relocate(Reg::RBP, Reg::RSI, Reg::RCX)), Reg64(REL::Relocate(Reg::RDI, Reg::RBP, Reg::RSI)));
		patch.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch));

		// Telekinesis dtor
		addr = RELOCATION_ID(34252, 35049).address() + REL::Relocate(0xF, 0xF);
		struct Patch2 : CodeGenerator
		{
			Patch2(std::uintptr_t a_func, uintptr_t a_target)
			{
				Label retnLabel;
				Label funcLabel;

				mov(ptr[rsp + 0x40], rbx);

				mov(rbx, rcx);
				mov(rdi, rdx);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(rcx, rbx);
				mov(rdx, rdi);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x7);
			}
		};
		Patch2 patch2(reinterpret_cast<uintptr_t>(ClearGrabbedObjectsHelper2), addr);
		patch2.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch2));
		REL::safe_write(addr + 0x5, REL::NOP2, 2);

		addr = RELOCATION_ID(34259, 35046).address();
		// Telekinesis apply begin.
		struct Patch3 : CodeGenerator
		{
			Patch3(std::uintptr_t a_func, uintptr_t a_target)
			{
				Label retnLabel;
				Label funcLabel;

				push(rbx);
				sub(rsp, 0x40);
				mov(rbx, rcx);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(rcx, rbx);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x6);
			}
		};
		Patch3 patch3(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper), addr);
		patch3.ready();

		trampoline.write_branch<6>(addr, trampoline.allocate(patch3));

		addr = RELOCATION_ID(34259, 35046).address() + REL::Relocate(0xE21 - 0xDC0, 0x56, 0xB1);
		struct Patch4 : CodeGenerator
		{
			Patch4(std::uintptr_t a_func, uintptr_t a_target)
			{
				Label retnLabel;
				Label funcLabel;

				mov(byte[rbx + 0xA9], 0);

				mov(rbx, rax);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(rax, rbx);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x7);
			}
		};
		Patch4 patch4(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper2), addr);
		patch4.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch4));

		addr = RELOCATION_ID(34259, 35046).address() + REL::Relocate(0xE30 - 0xDC0, 0x65, 0xC0);
		struct Patch5 : CodeGenerator
		{
			Patch5(std::uintptr_t a_func, uintptr_t a_target)
			{
				Label retnLabel;
				Label funcLabel;

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(rcx, rbx);
				xor_(edx, edx);
				add(rsp, 0x40);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x7);
			}
		};
		Patch5 patch5(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper2), addr);
		patch5.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch5));

		addr = RELOCATION_ID(34256, 35048).address() + REL::Relocate(0x0, 0x0, 0xD);
		struct Patch6 : CodeGenerator
		{
			Patch6(std::uintptr_t a_func, std::uintptr_t a_target, std::uintptr_t a_targetOffset)
			{
				Label retnLabel;
				Label funcLabel;

				if (!REL::Module::IsVR()) {
					sub(rsp, 0x28);
					mov(ptr[rsp + 0x30], rcx);
				} else {
					mov(ptr[rsp + 0x30], rcx);
				}

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				if (!REL::Module::IsVR()) {
					mov(rcx, ptr[rsp + 0x30]);
				} else {
					mov(rcx, rdi);
				}

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + a_targetOffset);
			}
		};
		Patch6 patch6(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper3), addr, REL::Relocate(0x9, 0x9, 0x5));
		patch6.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch6));

		addr = RELOCATION_ID(34256, 35048).address() + REL::Relocate(0xCA8 - 0xC80, 0x7C, 0xD4);
		struct Patch7 : CodeGenerator
		{
			Patch7(std::uintptr_t a_func, uintptr_t a_target, std::uintptr_t a_rspOffset)
			{
				Label retnLabel;
				Label funcLabel;

				mov(rcx, ptr[rsp + 0x30]);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				add(rsp, a_rspOffset);

				if (REL::Module::IsVR()) {
					pop(rdi);
				}

				mov(rax, 1);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x5);
			}
		};
		Patch7 patch7(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper4), addr, REL::Relocate(0x28, 0x28, 0x20));
		patch7.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch7));

		Memory::Internal::write<uint8_t>(addr + 5, 0xC3, true);

		addr = RELOCATION_ID(34260, 35047).address() + REL::Relocate(0x11, 0x12, 0x16);
		struct Patch8 : CodeGenerator
		{
			Patch8(std::uintptr_t a_func, std::uintptr_t a_target, std::uintptr_t a_rspOffset, std::uintptr_t a_targetOffset)
			{
				Label retnLabel;
				Label funcLabel;

				if (!REL::Module::IsVR()) {
					mov(ptr[rsp + a_rspOffset], rbx);
					movaps(xmm7, xmm1);
				} else {
					mov(ptr[rax + 0x8], rbx);
					movaps(ptr[rax - 0x38], xmm6);
					movaps(xmm6, xmm1);
					mov(rdi, rax);
				}

				mov(rsi, rcx);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(rcx, rsi);

				if (REL::Module::IsVR()) {
					mov(rax, rdi);
					movaps(xmm1, xmm6);
				} else {
					movaps(xmm1, xmm7);
				}

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + a_targetOffset);
			}
		};
		Patch8 patch8(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper5), addr, REL::Relocate(0x70, 0x60), REL::Relocate(0x5, 0x5, 0x8));
		patch8.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch8));

		if (REL::Module::IsVR()) {
			REL::safe_write(addr + 0x5, REL::NOP3, 3);
		}

		addr = RELOCATION_ID(34260, 35047).address() + REL::Relocate(0x70B3 - 0x6E40, 0x305, 0x32A);
		struct Patch9 : CodeGenerator
		{
			Patch9(std::uintptr_t a_func, uintptr_t a_target, std::uintptr_t a_rspOffset, Reg64 a_popReg, std::uintptr_t a_targetOffset)
			{
				Label retnLabel;
				Label funcLabel;

				mov(a_popReg, rax);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				mov(rax, a_popReg);

				add(rsp, a_rspOffset);
				pop(a_popReg);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + a_targetOffset);
			}
		};
		Patch9 patch9(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper6), addr, REL::Relocate(0x50, 0x40, 0x60), Reg64(REL::Relocate(Reg64::RDI, Reg64::R14, Reg64::R15)), REL::Relocate(0x5, 0x6, 0x6));
		patch9.ready();

		if (REL::Module::IsSE()) {
			trampoline.write_branch<5>(addr, trampoline.allocate(patch9));
		} else {
			trampoline.write_branch<6>(addr, trampoline.allocate(patch9));
		}

		// Allow more than one instance of the telekinesis active effect.
		if (addr = RELOCATION_ID(33781, 34577).address() + REL::Relocate(0xA29 - 0xA20, 0x9); REL::make_pattern<"48 39 42 48">().match(addr)) {
			struct Patch10 : CodeGenerator
			{
				Patch10(std::uintptr_t a_func, uintptr_t a_target)
				{
					Label retnLabel;
					Label funcLabel;

					Label IfNull;

					push(rcx);
					push(rdx);

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);

					test(eax, eax);
					jne(IfNull);
					pop(rcx);
					pop(rdx);
					jmp(ptr[rip + retnLabel + 0x7]);  //ctx->IP = ctx::IP + 7;

					L(IfNull);
					pop(rcx);
					pop(rdx);

					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target + 0x6);
				}
			};
			Patch10 patch10(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper7), addr);
			patch10.ready();

			trampoline.write_branch<6>(addr, trampoline.allocate(patch10));
		} else {
			stl::report_and_fail("Failed to patch Allow multiple Telekinesis effects");
		}

		// Allow more than one instance of the telekinesis effect (both places must be edited).
		addr = RELOCATION_ID(33785, 34581).address() + REL::Relocate(0xB80 - 0xB70, 0x10);
		struct Patch11 : CodeGenerator
		{
			Patch11(std::uintptr_t a_func, uintptr_t a_target, Reg64 a_raxStoreReg)
			{
				Label retnLabel;
				Label funcLabel;

				Label NotSkip;

				push(rcx);
				mov(rcx, rbx);
				mov(a_raxStoreReg, rax);

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				cmp(eax, 1);
				jne(NotSkip);

				pop(rcx);
				mov(rdx, rbx);
				mov(rax, 1);
				jmp(ptr[rip + retnLabel]);

				L(NotSkip);
				pop(rcx);
				mov(rdx, rbx);
				mov(rax, a_raxStoreReg);
				shr(eax, 0x12);
				mov(rdi, rcx);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(retnLabel);
				dq(a_target + 0x6);
			}
		};
		Patch11 patch11(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper8), addr, Reg64(REL::Relocate(Reg64::RDI, Reg64::R15)));
		patch11.ready();

		trampoline.write_branch<6>(addr, trampoline.allocate(patch11));

		// Fix telekinesis gaining skill for each instance of the effect.
		addr = RELOCATION_ID(33321, 34100).address() + REL::Relocate(0x2D, 0x6A);
		struct Patch12 : CodeGenerator
		{
			Patch12(std::uintptr_t a_func, uintptr_t a_target)
			{
				Label retnLabel;
				Label funcLabel;
				Label funcLabel2;

				Label NotSkip;

				mov(rcx, rdi);
				if (AE) {
					mov(rbx, r11);
				}

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);

				cmp(eax, 1);
				jne(NotSkip);
				mov(rax, 0);
				if (AE) {
					mov(r11, rbx);
					mov(ptr[rsp + 0xA8], rax);
				}
				jmp(ptr[rip + retnLabel]);

				L(NotSkip);
				if (!AE) {
					mov(rcx, rdi);
				} else {
					mov(r11, rbx);
					lea(rdx, ptr[r11 + 0x20]);
					lea(rcx, ptr[r11 + 0x8]);
				}

				sub(rsp, 0x20);
				call(ptr[rip + funcLabel2]);  // Call Original Function ValueModifierEffect::sub_140540570 SE
				add(rsp, 0x20);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(a_func);

				L(funcLabel2);
				dq(RELOCATION_ID(33322, 17201).address());

				L(retnLabel);
				dq(a_target + 0x5);
			}
		};
		Patch12 patch12(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper10), addr);
		patch12.ready();

		trampoline.write_branch<5>(addr, trampoline.allocate(patch12));
	}

	BetterTelekinesisPlugin::SavedGrabIndex::SavedGrabIndex() = default;

	int BetterTelekinesisPlugin::UnsafeFindFreeIndex()
	{
		std::vector<unsigned char> taken_bits(13);
		for (auto& [fst, snd] : savedGrabindex) {
			if (fst == 0) {
				continue;
			}

			int ti = snd->objIndex;
			if (ti < 0 || ti >= 100) {
				continue;
			}

			int ix = ti / 8;
			int jx = ti % 8;
			taken_bits[ix] |= static_cast<unsigned char>(1 << jx);
		}

		if (castingSwordBarrage) {
			for (int j = 0; j < 8; j++) {
				int ji = (placementBarrage + j) % 8;
				ji++;

				int ix = ji / 8;
				int jx = ji % 8;
				if ((taken_bits[ix] & static_cast<unsigned char>(1 << jx)) == 0) {
					placementBarrage++;
					return ji;
				}
			}
		}

		for (int i = 0; i < 100; i++) {
			int ix = i / 8;
			int jx = i % 8;
			if ((taken_bits[ix] & static_cast<unsigned char>(1 << jx)) == 0) {
				return i;
			}
		}

		return -1;
	}

	void BetterTelekinesisPlugin::SwitchToGrabIndex(const uintptr_t addr, const std::string& reason, const float diff)
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		{
			std::scoped_lock lock(grabindexLocker);

			std::shared_ptr<SavedGrabIndex> g;

			if (!savedGrabindex.contains(0)) {
				g = std::make_shared<SavedGrabIndex>();
				g->addr = 0;
				if (!REL::Module::IsVR()) {
					g->wgt = plr->GetPlayerRuntimeData().grabData.grabObjectWeight;
					g->dist = plr->GetPlayerRuntimeData().grabData.grabDistance;
					g->handle = plr->GetPlayerRuntimeData().grabData.grabbedObject.native_handle();
					//g->spring = plr->GetPlayerRuntimeData().grabData.grabSpring;
					memcpy_s(g->spring, 0x30, plr->GetPlayerRuntimeData().grabData.grabSpring.data(), 0x30);
					g->grabtype = plr->GetPlayerRuntimeData().grabType.get();
				} else {
					auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;
					g->wgt = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabObjectWeight;
					g->dist = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabDistance;
					g->handle = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabbedObject.native_handle();
					//g->spring = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring;
					memcpy_s(g->spring, 0x30, plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring.data(), 0x30);
					g->grabtype = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabType;
				}
				g->objIndex = -1;

				savedGrabindex[0] = g;
			}

			if (!savedGrabindex.contains(addr)) {
				g = std::make_shared<SavedGrabIndex>();
				g->addr = addr;
				g->objIndex = UnsafeFindFreeIndex();
				g->rng = std::make_unique<RandomMoveGenerator>();

				savedGrabindex[addr] = g;
			} else {
				g = savedGrabindex.find(addr)->second;

				if (diff > 0.0f && g->rng) {
					g->rng->Update(diff);
				}
			}

			logger::debug(fmt::runtime("switch {:#x} -> {:#x} ({}) "), currentGrabindex, addr, reason);

			if (currentGrabindex == addr) {
				return;
			}

			auto it = savedGrabindex.find(currentGrabindex);
			auto& prev = it->second;

			if (!REL::Module::IsVR()) {
				prev->wgt = plr->GetPlayerRuntimeData().grabData.grabObjectWeight;
				prev->dist = plr->GetPlayerRuntimeData().grabData.grabDistance;
				prev->handle = plr->GetPlayerRuntimeData().grabData.grabbedObject.native_handle();
				//prev->spring = plr->GetPlayerRuntimeData().grabData.grabSpring;
				memcpy_s(prev->spring, 0x30, plr->GetPlayerRuntimeData().grabData.grabSpring.data(), 0x30);
				prev->grabtype = plr->GetPlayerRuntimeData().grabType.get();

				currentGrabindex = addr;

				plr->GetPlayerRuntimeData().grabData.grabObjectWeight = g->wgt;
				plr->GetPlayerRuntimeData().grabData.grabDistance = g->dist;
				plr->GetPlayerRuntimeData().grabData.grabbedObject = RE::TESObjectREFR::LookupByHandle(g->handle).get();
				//plr->GetPlayerRuntimeData().grabData.grabSpring = g->spring;
				memcpy_s(plr->GetPlayerRuntimeData().grabData.grabSpring.data(), 0x30, g->spring, 0x30);
				plr->GetPlayerRuntimeData().grabType = g->grabtype;
			} else {
				auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;
				prev->wgt = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabObjectWeight;
				prev->dist = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabDistance;
				prev->handle = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabbedObject.native_handle();
				//prev->spring = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring;
				memcpy_s(prev->spring, 0x30, plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring.data(), 0x30);
				prev->grabtype = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabType;

				currentGrabindex = addr;

				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabObjectWeight = g->wgt;
				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabDistance = g->dist;
				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabbedObject = RE::TESObjectREFR::LookupByHandle(g->handle).get();
				//plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring = g->spring;
				memcpy_s(plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring.data(), 0x30, g->spring, 0x30);
				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabType = g->grabtype;
			}
		}
	}

	void BetterTelekinesisPlugin::FreeGrabIndex(uintptr_t addr, const std::string& reason)
	{
		if (addr == 0) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}
		{
			std::scoped_lock lock(grabindexLocker);

			logger::debug(fmt::runtime("free {:#x} ({})"), addr, reason);

			if (!savedGrabindex.contains(addr)) {
				return;
			}

			uintptr_t cur_ind = currentGrabindex;
			if (cur_ind != addr) {
				SwitchToGrabIndex(addr, "need to free");
			}
			// Call the func that drops the items from havok.
			dontCallClear = 1;
			if (!REL::Module::IsVR()) {
				plr->DestroyMouseSprings();
			} else {
				auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;

				static REL::Relocation<void*(RE::PlayerCharacter*, RE::VR_DEVICE)> func{ RELOCATION_ID(39480, 40557) };
				func(plr, hand);
			}
			dontCallClear = 0;

			if (cur_ind == addr) {
				SwitchToGrabIndex(0, "returning from free");
			} else {
				SwitchToGrabIndex(cur_ind, "returning from free");
			}

			savedGrabindex.erase(addr);
		}
	}

	void BetterTelekinesisPlugin::ClearGrabIndex(const bool onlyIfCount)
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		{
			std::scoped_lock lock(grabindexLocker);

			logger::debug("clear");

			for (const auto& key : savedGrabindex | std::views::keys) {
				if (key == 0) {
					continue;
				}

				FreeGrabIndex(key, "clear");
			}

			// Current must be Zero or uninitiated, both is ok.
			RE::ObjectRefHandle grabbedObject;
			if (!REL::Module::IsVR()) {
				grabbedObject = plr->GetPlayerRuntimeData().grabData.grabbedObject;
			} else {
				auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;
				grabbedObject = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabbedObject;
			}

			if (!onlyIfCount || grabbedObject.get() != nullptr) {
				dontCallClear = 1;
				//Clear Grabbed
				if (!REL::Module::IsVR()) {
					plr->DestroyMouseSprings();
				} else {
					auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;
					static REL::Relocation<void*(RE::PlayerCharacter*, RE::VR_DEVICE)> func{ RELOCATION_ID(39480, 40557) };
					func(plr, hand);
				}
				dontCallClear = 0;
			}
		}
	}

	void BetterTelekinesisPlugin::SelectRotationOffset(int index, int& x, int& y)
	{
		if (index < 0 || index >= _rot_offsets.size()) {
			return;
		}

		auto& [fst, snd] = _rot_offsets[index];
		x = fst;
		y = snd;
	}

	const std::vector<std::pair<int, int>> BetterTelekinesisPlugin::_rot_offsets = { std::pair(0, 0), std::pair(1, 1), std::pair(1, -1), std::pair(-1, 1), std::pair(-1, -1), std::pair(1, 0), std::pair(-1, 0), std::pair(2, 2), std::pair(0, -1), std::pair(0, 1), std::pair(-2, -2), std::pair(2, 1), std::pair(-2, 2), std::pair(2, -2), std::pair(-2, -1), std::pair(1, 2), std::pair(1, -2), std::pair(-2, 1), std::pair(2, 0), std::pair(-1, -2), std::pair(-1, 2), std::pair(2, -1), std::pair(-2, 0), std::pair(2, 3), std::pair(0, -2), std::pair(0, 2), std::pair(-3, -2), std::pair(3, -2), std::pair(-2, 3), std::pair(3, 2), std::pair(-2, -3), std::pair(-3, 2), std::pair(2, -3), std::pair(1, 3), std::pair(-1, -3), std::pair(-1, 3), std::pair(3, 0), std::pair(-3, -1), std::pair(3, -1), std::pair(-3, 1), std::pair(3, 1), std::pair(0, -3), std::pair(0, 3), std::pair(-3, 0), std::pair(1, -3), std::pair(2, 4), std::pair(-4, -2), std::pair(4, -2), std::pair(-2, 4), std::pair(-2, -4), std::pair(4, 2), std::pair(-4, 2), std::pair(2, -4), std::pair(1, 4), std::pair(-3, -3), std::pair(4, 1), std::pair(-3, 3), std::pair(1, -4), std::pair(3, 3), std::pair(-4, -1), std::pair(4, -1), std::pair(-4, 1), std::pair(3, -3), std::pair(-1, 4), std::pair(-1, -4), std::pair(5, 3), std::pair(-4, 0), std::pair(4, 0), std::pair(-5, 3), std::pair(3, -5), std::pair(0, 4), std::pair(0, -4), std::pair(3, 5), std::pair(-5, -3), std::pair(5, -3), std::pair(-3, 5), std::pair(-3, -5), std::pair(4, 4), std::pair(4, -4), std::pair(-4, 4), std::pair(-4, -4), std::pair(5, 2), std::pair(-5, 2), std::pair(3, -4), std::pair(2, 5), std::pair(-4, -3), std::pair(5, -2), std::pair(-3, 4), std::pair(2, -5), std::pair(1, 5), std::pair(-5, -2), std::pair(5, -1), std::pair(-5, 1), std::pair(4, 3), std::pair(-2, -5), std::pair(-2, 5), std::pair(4, -3), std::pair(-5, -1), std::pair(5, 1), std::pair(-4, 3) };

	void BetterTelekinesisPlugin::ActivateNode(const RE::NiNode* node)
	{
		if (node == nullptr) {
			return;
		}

		auto colNode = node->GetCollisionObject();
		if (colNode == nullptr) {
			return;
		}

		auto rigidBody = colNode->GetRigidBody();
		if (rigidBody == nullptr) {
			return;
		}

		auto vel = RE::hkVector4();

		rigidBody->SetAngularVelocity(vel);
	}

	void BetterTelekinesisPlugin::UpdatePointForward(RE::TESObjectREFR* refr)
	{
		if (refr == nullptr) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		auto pcam = RE::PlayerCamera::GetSingleton();
		if (pcam == nullptr) {
			return;
		}

		RE::NiPoint3 AngleWanted;

		if (!REL::Module::IsVR()) {
			RE::NiQuaternion qt;
			pcam->currentState->GetRotation(qt);
			AngleWanted = Util::NiQuarterionToEulerXYZ(qt);
		} else {
			RE::NiMatrix3 mat;
			if (CastingLeftHandVR()) {
				mat = plr->GetVRNodeData()->LeftWandNode->world.rotate;
			} else {
				mat = plr->GetVRNodeData()->RightWandNode->world.rotate;
			}

			mat.ToEulerAnglesXYZ(AngleWanted);
		}

		refr->SetAngle(AngleWanted);
		refr->Update3DPosition(true);
	}

	void BetterTelekinesisPlugin::UpdateHeldObject(RE::TESObjectREFR* obj, const std::shared_ptr<HeldObjectData>& data, const std::vector<RE::ActiveEffect*>& effectList)
	{
		if (obj == nullptr) {
			return;
		}

		if (Config::PointWeaponsAndProjectilesForward) {
			if (obj->As<RE::TESObjectWEAP>() != nullptr || obj->As<RE::Projectile>() != nullptr || IsOurItem(obj->GetBaseObject()) != OurItemTypes::None) {
				UpdatePointForward(obj);
			}
		}

		if (data->effect != nullptr && data->elapsed >= Config::SwordBarrage_FireDelay && IsOurSpell(data->effect) == OurSpellTypes::SwordBarrage) {
			for (auto x : effectList) {
				uint32_t handleId = 0;
				if (skyrim_cast<RE::TelekinesisEffect*>(x) != nullptr) {
					handleId = skyrim_cast<RE::TelekinesisEffect*>(x)->grabbedObject.native_handle();
				} else if (skyrim_cast<RE::GrabActorEffect*>(x) != nullptr) {
					handleId = skyrim_cast<RE::GrabActorEffect*>(x)->grabbedActor.native_handle();
				}

				if (handleId == data->objectHandleId) {
					std::shared_ptr<SwordInstance> sw = nullptr;
					if (normalSwords->lookup.contains(handleId)) {
						sw = normalSwords->lookup[handleId];
						sw->launchTime = time;
					} else if (ghostSwords->lookup.contains(handleId)) {
						sw = ghostSwords->lookup[handleId];
						sw->launchTime = time;
					}

					x->Dispel(true);
					break;
				}
			}
		}
	}

	void BetterTelekinesisPlugin::InitSwords()
	{
		if (hasIntializedSwords) {
			return;
		}
		hasIntializedSwords = true;

		SwordData::temp1 = RE::NiPoint3();
		SwordData::temp2 = RE::NiPoint3();
		SwordData::temp3 = RE::NiPoint3();
		SwordData::return1 = RE::NiPoint3();
		SwordData::return2 = RE::NiPoint3();

		std::string fileName = "BetterTelekinesis.esp";

		normalSwords->AddSwordFormId(0x80E, fileName, false);
		for (unsigned int u = 0x840; u < 0x870; u++) {
			normalSwords->AddSwordFormId(u, fileName, false);
		}

		ghostSwords->AddSwordFormId(0x80D, fileName, true);
		for (unsigned int u = 0x80F; u <= 0x83F; u++) {
			ghostSwords->AddSwordFormId(u, fileName, true);
		}
	}

	void BetterTelekinesisPlugin::PlaySwordEffect(RE::TESObjectREFR* obj, bool ghost)
	{
		if (obj->Get3D() == nullptr) {
			return;
		}

		if (ghost) {
			auto form = !effectInfos.empty() ? *effectInfos.begin() : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 1.5f);
			}

			if (ghostSwordEffect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(ghostSwordEffect);
				if (form2 != nullptr) {
					obj->ApplyEffectShader(form2, -1.0f);
				}
			}
		} else {
			auto form = !effectInfos.empty() ? effectInfos[0] : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 1.5f);
			}

			if (normalSwordEffect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(normalSwordEffect);
				if (form2 != nullptr) {
					obj->ApplyEffectShader(form2, -1.0f);
				}
			}
		}
	}

	void BetterTelekinesisPlugin::StopSwordEffect(RE::TESObjectREFR* obj, const bool ghost)
	{
		if (obj->Get3D() == nullptr) {
			return;
		}

		if (ghost) {
			if (ghostSwordEffect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(ghostSwordEffect);
				if (form2 != nullptr) {
					REL::Relocation<void (*)(RE::ProcessLists*, RE::TESObjectREFR*, RE::TESEffectShader*)> StopEffect{ RELOCATION_ID(40381, 41395) };
					StopEffect(RE::ProcessLists::GetSingleton(), obj, form2);
				}
			}

			auto form = effectInfos.size() >= 2 ? effectInfos[1] : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 5.0f);
			}
		} else {
			if (normalSwordEffect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(normalSwordEffect);
				if (form2 != nullptr) {
					REL::Relocation<void (*)(RE::ProcessLists*, RE::TESObjectREFR*, RE::TESEffectShader*)> StopEffect{ RELOCATION_ID(40381, 41395) };
					StopEffect(RE::ProcessLists::GetSingleton(), obj, form2);
				}
			}

			auto form = effectInfos.size() >= 2 ? effectInfos[1] : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 5.0f);
			}
		}
	}

	void BetterTelekinesisPlugin::ReturnSwordToPlace(RE::TESObjectREFR* obj)
	{
		auto marker = swordReturnMarker;
		if (marker == nullptr) {
			return;
		}

		auto cell = marker->GetParentCell();
		if (cell == nullptr) {
			return;
		}

		auto markerHold = marker->GetHandle();
		if (!markerHold.get()->IsHandleValid()) {
			return;
		}

		auto ws = cell != nullptr ? cell->GetRuntimeData().worldSpace : nullptr;

		REL::Relocation<void (*)(RE::TESObjectREFR*, RE::ObjectRefHandle&, RE::TESObjectCELL*, RE::TESWorldSpace*, const RE::NiPoint3&, const RE::NiPoint3&)> moveTo{ RELOCATION_ID(56227, 56626) };
		moveTo(obj, markerHold, cell, ws, SwordData::return1, SwordData::return2);
	}

	void BetterTelekinesisPlugin::UpdateSwordEffects()
	{
		double now = time;

		for (int z = 0; z < 2; z++) {
			auto& dat = z == 0 ? normalSwords : ghostSwords;

			if (dat->forcedGrab != nullptr) {
				if (now - dat->forcedGrab->createTime > 0.5) {
					dat->forcedGrab = nullptr;
				}
			}

			for (int i = 0; i < dat->swords.size(); i++) {
				auto& sw = dat->swords[i];
				auto objRef = RE::TESObjectREFR::LookupByHandle(sw->handle).get();

				bool isForced = dat->forcedGrab != nullptr && dat->forcedGrab->handle == sw->handle;
				if (sw->waitingEffect != 0) {
					bool waitMore = false;
					if (sw->IsWaitingEffect(now)) {
						if (objRef != nullptr) {
							auto root = objRef->Get3D();
							if (root == nullptr) {
								waitMore = true;
							} else {
								if (sw->waitEffectCounter == 0) {
									auto scb = root->GetObjectByName("Scb");
									if (scb != nullptr) {
										scb->GetFlags() |= RE::NiAVObject::Flag::kHidden;
									}

									if (sw->waitingEffect == 2) {
										PlaySwordEffect(objRef, true);
									} else if (sw->waitingEffect == 1) {
										PlaySwordEffect(objRef, false);
									}

									root->local.translate.z -= first_TeleportZOffset;
									//root->Update(data); Not working, instead manually update havok position

									auto cobj = root->GetCollisionObject();
									if (cobj != nullptr) {
										auto rigid = cobj->GetRigidBody();
										if (rigid != nullptr) {
											RE::hkVector4 pos = RE::hkVector4();
											rigid->GetPosition(pos);
											pos.quad.m128_f32[2] -= first_TeleportZOffset * 0.0142875f;
											rigid->SetPosition(pos);
										}
									}

									sw->waitEffectCounter = 1;
									waitMore = true;
								} else if (sw->waitEffectCounter == 1) {
									ActivateNode(root->AsNode());
								}
							}
						}
					}

					if (!waitMore) {
						sw->waitingEffect = 0;
						sw->waitEffectCounter = 0;
					}
				}

				if (sw->IsWaitingInvis()) {
					if (now - sw->fadeTime > 3.0) {
						sw->fadedOut = true;
						sw->fadingOut = false;

						objRef = RE::TESObjectREFR::LookupByHandle(sw->handle).get();
						if (objRef->IsHandleValid()) {
							auto obj = objRef;
							ReturnSwordToPlace(obj);
						}
					}
				} else if (!isForced && sw->CanPlayFadeout(now)) {
					sw->fadingOut = true;
					sw->fadeTime = now;

					objRef = RE::TESObjectREFR::LookupByHandle(sw->handle).get();
					if (objRef->IsHandleValid()) {
						auto obj = objRef;
						StopSwordEffect(obj, z == 1);
					}
				}
			}
		}
	}

	void BetterTelekinesisPlugin::TryPlaceSwordNow(const bool ghost)
	{
		InitSwords();

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		double now = time;
		RE::RefHandle chosen = 0;
		auto& data = ghost ? ghostSwords : normalSwords;

		// Barrage rate of fire?
		if (ghost) {
			for (const auto& sw : data->swords) {
				if (now - sw->createTime < Config::SwordBarrage_SpawnDelay) {
					return;
				}
			}
		}

		if (data->forcedGrab != nullptr) {
			return;
		}

		std::shared_ptr<SwordInstance> inst = nullptr;

		// try select random first.
		if (!data->swords.empty()) {
			int randomTried = 0;
			while (randomTried++ < 2) {
				thread_local static std::random_device rd;
				thread_local static std::mt19937 generator(rd());
				thread_local static std::uniform_int_distribution distribution(0, 31);
				int chosenIndex = distribution(generator);
				auto& sword = data->swords[chosenIndex];

				if (sword->IsFreeForSummon(now)) {
					chosen = sword->handle;
					data->nextIndex = chosenIndex + 1;
					inst = sword;
					break;
				}
			}
		}

		if (inst == nullptr) {
			int maxTry = static_cast<int>(data->swords.size());

			for (int i = 0; i < maxTry; i++) {
				int chosenIndex = (data->nextIndex + i) % maxTry;
				auto& sword = data->swords[chosenIndex];

				if (sword->IsFreeForSummon(now)) {
					chosen = sword->handle;
					data->nextIndex = i + 1;
					inst = sword;
					break;
				}
			}
		}

		if (chosen == 0) {
			return;
		}

		auto cell = plr->GetParentCell();
		if (cell == nullptr || !cell->IsAttached()) {
			return;
		}

		if (!CalculateSwordPlacePosition(100.0f, false, ghost)) {
			return;
		}

		auto objRef = RE::TESObjectREFR::LookupByHandle(chosen).get();
		if (objRef == nullptr) {
			return;
		}

		auto plrRef = plr->AsReference();

		auto plrHold = plrRef->GetHandle();

		auto ws = cell != nullptr ? cell->GetRuntimeData().worldSpace : nullptr;

		std::vector<float> go(6);
		for (int i = 0; i < 3; i++) {
			go[i] = SwordData::temp2.y * static_cast<float>(i);
		}
		for (int i = 0; i < 3; i++) {
			go[i + 3] = SwordData::temp3.y * static_cast<float>(i);
		}

		SwordData::temp2.z += first_TeleportZOffset;

		REL::Relocation<void (*)(RE::TESObjectREFR*, RE::ObjectRefHandle&, RE::TESObjectCELL*, RE::TESWorldSpace*, const RE::NiPoint3&, const RE::NiPoint3&)> moveTo{ RELOCATION_ID(56227, 56626) };
		moveTo(objRef, plrHold, cell, ws, SwordData::temp2, SwordData::temp3);

		if (inst != nullptr) {
			inst->waitingEffect = static_cast<unsigned char>(ghost ? 2 : 1);
			inst->createTime = now;
			inst->fadedOut = false;
			inst->Goto = go;
			data->forcedGrab = inst;
		}
	}

	bool BetterTelekinesisPlugin::CalculateSwordPlacePosition(float extraRadiusOfSword, bool forcePlaceInBadPosition, bool ghost)
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return false;
		}

		auto pcam = RE::PlayerCamera::GetSingleton();
		if (pcam == nullptr) {
			return false;
		}

		auto rootPlr = plr->Get3D();
		if (rootPlr == nullptr) {
			return false;
		}

		auto head = plr->GetNodeByName("NPC Head [Head]");
		if (head == nullptr) {
			head = rootPlr;
		}

		auto cell = plr->GetParentCell();
		if (cell == nullptr || !cell->IsAttached()) {
			return false;
		}

		std::vector<RE::NiNode*> ignore_ls;
		ignore_ls.push_back(rootPlr->AsNode());
		auto fs = plr->Get3D1(true);
		if (fs != nullptr) {
			ignore_ls.push_back(fs->AsNode());
		}

		{
			std::scoped_lock lock(swordPositionLocker);
			for (const auto& key : cachedHeldHandles | std::views::keys) {
				{
					auto objRef = RE::TESObjectREFR::LookupByHandle(key).get();
					if (objRef != nullptr) {
						auto onode = objRef->Get3D();
						if (onode != nullptr) {
							ignore_ls.push_back(onode->AsNode());
						}
					}
				}
			}
		}

		auto camWt = rootPlr->world;
		glm::vec4 begin;
		glm::vec4 end;

		auto hpos = head->world.translate;
		auto& bpos = camWt.translate;
		bpos.x = hpos.x;
		bpos.y = hpos.y;
		bpos.z = hpos.z;

		SwordData::temp1.x = 0.0f;
		SwordData::temp1.y = ghost ? static_cast<float>(Config::MagicSwordBarrage_PlaceDistance) : static_cast<float>(Config::MagicSwordBlast_PlaceDistance);
		SwordData::temp1.z = 0.0f;

		SwordData::temp2 = Util::Translate(camWt, SwordData::temp1);

		if (!REL::Module::IsVR()) {
			RE::NiQuaternion qt;

			pcam->currentState->GetRotation(qt);

			SwordData::temp3 = Util::NiQuarterionToEulerXYZ(qt);
		} else {
			RE::NiMatrix3 mat;

			if (CastingLeftHandVR()) {
				mat = plr->GetVRNodeData()->LeftWandNode->world.rotate;
			} else {
				mat = plr->GetVRNodeData()->RightWandNode->world.rotate;
			}

			mat.ToEulerAnglesXYZ(SwordData::temp3);
		}

		begin = { hpos.x, hpos.y, hpos.z, 0.0f };
		end = { SwordData::temp2.x, SwordData::temp2.y, SwordData::temp2.z, 0.0f };

		auto rp = Raycast::hkpCastRay(begin, end);
		float frac = 1.0f;
		for (auto& [normal, hitFraction, body] : rp.hitArray) {
			if (hitFraction >= frac || body == nullptr) {
				continue;
			}

			const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
			const auto flags = collisionObj->GetCollisionLayer();
			unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
			if ((RaycastHelper::RaycastMask & mask) == 0) {
				continue;
			}

			if (collisionObj != nullptr) {
				bool had = false;
				for (auto& co : ignore_ls) {
					if (co != nullptr && RaycastHelper::IsRaycastHitNodeTest(rp, co)) {
						had = true;
						break;
					}
				}

				if (had) {
					continue;
				}
			}

			frac = hitFraction;
		}

		float frac_extent = extraRadiusOfSword / std::max(1.0f, SwordData::temp1.y);
		frac -= frac_extent;

		// Can't fit here.
		if (!forcePlaceInBadPosition && frac < frac_extent) {
			return false;
		}

		if (frac < 1.0f) {
			for (int i = 0; i < 3; i++) {
				end[i] = (end[i] - begin[i]) * frac + begin[i];
			}
		}

		SwordData::temp2.x = end[0];
		SwordData::temp2.y = end[1];
		SwordData::temp2.z = end[2];

		return true;
	}

	int BetterTelekinesisPlugin::ShouldLaunchObjectNow(RE::ActiveEffect* ef)
	{
		if (Config::AlwaysLaunchObjectsEvenWhenNotFinishedPulling) {
			return 1;
		}

		if (ef == nullptr) {
			return 0;
		}

		auto efs = ef->GetBaseObject();
		if (efs == nullptr) {
			return 0;
		}

		auto st = IsOurSpell(efs);
		if (st == OurSpellTypes::SwordBarrage) {
			return 1;
		}

		return 0;
	}

	bool BetterTelekinesisPlugin::CanPickTelekinesisTarget(RE::TESObjectREFR* obj, const std::vector<RE::EffectSetting*>& casting)
	{
		if (obj == nullptr) {
			return false;
		}

		auto bform = obj->GetBaseObject()->As<RE::TESForm>();
		if (bform == nullptr) {
			return false;
		}

		bool castingGhost = false;
		bool castingNormal = false;

		for (auto ef : casting) {
			switch (IsOurSpell(ef)) {
			case OurSpellTypes::SwordBarrage:
				castingGhost = true;
				break;
			case OurSpellTypes::SwordBlast:
				castingNormal = true;
				break;
			default:
				break;
			}
		}

		if (castingGhost) {
			if (ghostSwords->forcedGrab == nullptr) {
				return false;
			}

			if (IsOurItem(bform) != OurItemTypes::GhostSword) {
				return false;
			}

			{
				auto objHandle = obj->GetHandle();
				unsigned int handleId = objHandle.native_handle();
				if (handleId == 0) {
					return false;
				}

				if (ghostSwords->forcedGrab->handle != handleId) {
					return false;
				}
			}
		}

		if (castingNormal) {
			if (normalSwords->forcedGrab == nullptr) {
				return false;
			}

			if (IsOurItem(bform) != OurItemTypes::IronSword) {
				return false;
			}

			{
				auto objHandle = obj->GetHandle();
				unsigned int handleId = objHandle.native_handle();
				if (handleId == 0) {
					return false;
				}

				if (normalSwords->forcedGrab->handle != handleId) {
					return false;
				}
			}
		}

		return true;
	}

	void BetterTelekinesisPlugin::OnFailPickTelekinesisTarget(const RE::EffectSetting* efs, const bool failBecauseAlreadyMax)
	{
		if (efs == nullptr || failBecauseAlreadyMax) {
			return;
		}

		switch (IsOurSpell(efs)) {
		case OurSpellTypes::SwordBarrage:
			{
				TryPlaceSwordNow(true);
			}
			break;

		case OurSpellTypes::SwordBlast:
			{
				TryPlaceSwordNow(false);
			}
			break;
		default:
			break;
		}
	}

	BetterTelekinesisPlugin::OurSpellTypes BetterTelekinesisPlugin::IsOurSpell(const RE::EffectSetting* ef)
	{
		if (ef != nullptr) {
			for (int i = 0; i < static_cast<int>(SpellTypes::max); i++) {
				auto inf = spellInfos[i];

				if (inf->effect != nullptr && inf->effect == ef) {
					switch (static_cast<SpellTypes>(i)) {
					case SpellTypes::normal:
						return OurSpellTypes::TelekNormal;
					case SpellTypes::reach:
						return OurSpellTypes::TelekReach;
					case SpellTypes::single:
						return OurSpellTypes::TelekOne;
					case SpellTypes::enemy:
						return OurSpellTypes::None;
					case SpellTypes::blast:
						return OurSpellTypes::SwordBlast;
					case SpellTypes::barrage:
						return OurSpellTypes::SwordBarrage;
					default:
						break;
					}
				}
			}
		}

		return OurSpellTypes::None;
	}

	BetterTelekinesisPlugin::OurItemTypes BetterTelekinesisPlugin::IsOurItem(const RE::TESForm* baseForm)
	{
		if (baseForm != nullptr) {
			for (int i = 0; i < static_cast<int>(SpellTypes::max); i++) {
				auto inf = spellInfos[i];
				if (!inf->item.empty() && inf->item.contains(baseForm->formID)) {
					switch (static_cast<SpellTypes>(i)) {
					case SpellTypes::normal:
					case SpellTypes::reach:
					case SpellTypes::single:
					case SpellTypes::enemy:
						return OurItemTypes::None;
					case SpellTypes::blast:
						return OurItemTypes::IronSword;
					case SpellTypes::barrage:
						return OurItemTypes::GhostSword;
					default:
						break;
					}
				}
			}
		}

		return OurItemTypes::None;
	}

	bool BetterTelekinesisPlugin::HasAnyNormalTelekInHand()
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return false;
		}

		for (int i = 0; i < 2; i++) {
			auto caster = plr->GetMagicCaster(static_cast<RE::MagicSystem::CastingSource>(i));
			if (caster == nullptr) {
				continue;
			}

			auto item = caster->currentSpell;
			if (item == nullptr) {
				continue;
			}

			auto& efls = item->effects;
			if (efls.empty()) {
				continue;
			}

			for (const auto x : efls) {
				auto ef = x->baseEffect;
				if (ef != nullptr) {
					switch (IsOurSpell(ef)) {
					case OurSpellTypes::TelekNormal:
					case OurSpellTypes::TelekOne:
						return true;
					default:
						break;
					}
				}
			}
		}

		return false;
	}

	bool BetterTelekinesisPlugin::CastingLeftHandVR()
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return false;
		}

		bool left = false;

		if (REL::Module::IsVR()) {
			plr->AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* activeEffect) -> RE::BSContainer::ForEachResult {
				if (auto mgef = activeEffect ? activeEffect->GetBaseObject() : nullptr; mgef) {
					if (activeEffect->flags.all(RE::ActiveEffect::Flag::kInactive) || activeEffect->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
						return RE::BSContainer::ForEachResult::kContinue;
					}

					if (activeEffect->GetBaseObject()->GetArchetype() == RE::EffectSetting::Archetype::kTelekinesis) {
						if (activeEffect->castingSource == RE::MagicSystem::CastingSource::kLeftHand) {
							left = true;
							return RE::BSContainer::ForEachResult::kStop;
						}
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		} else {
			return false;
		}

		return left;
	}

	void SwordData::AddSwordFormId(unsigned int formId, const std::string& fileName, bool ghost)
	{
		auto form = RE::TESDataHandler::GetSingleton()->LookupForm(formId, fileName);
		if (form == nullptr) {
			return;
		}

		auto refr = form->AsReference();
		if (refr != nullptr) {
			this->AddSwordObj(refr, ghost);
		}
	}

	void SwordData::AddSwordObj(RE::TESObjectREFR* obj, const bool ghost)
	{
		if (obj == nullptr) {
			return;
		}

		auto objRef = RE::TESObjectREFR::LookupByHandle(obj->GetHandle().native_handle()).get();
		if (objRef != nullptr) {
			auto sw = std::make_shared<SwordInstance>();
			sw->handle = objRef->GetHandle().native_handle();
			swords.push_back(sw);
			lookup[objRef->GetHandle().native_handle()] = sw;

			int ix = static_cast<int>(swords.size()) - 1;
			auto& allItem = BetterTelekinesisPlugin::spellInfos[ghost ? static_cast<int>(BetterTelekinesisPlugin::SpellTypes::barrage) : static_cast<int>(BetterTelekinesisPlugin::SpellTypes::blast)]->item;
			RE::FormID fid = 0;
			if (!allItem.empty()) {
				fid = *std::next(allItem.begin(), ix % allItem.size());
			}

			auto form = RE::TESForm::LookupByID<RE::TESObjectMISC>(fid);
			if (form != nullptr) {
				objRef->data.objectReference = form;
			}
		}
	}

	bool SwordInstance::IsFreeForSummon(const double now) const
	{
		if (!this->fadedOut || this->held) {
			return false;
		}

		if (this->IsWaitingEffect(now)) {
			return false;
		}

		if (now - this->launchTime < 3.0) {
			return false;
		}

		if (now - this->createTime < 3.0) {
			return false;
		}

		return true;
	}

	bool SwordInstance::IsWaitingEffect(const double now) const
	{
		return this->waitingEffect != 0 && now - this->createTime < 0.3;
	}

	bool SwordInstance::CanPlayFadeout(const double now) const
	{
		if (this->fadedOut || this->held || this->fadingOut || now - this->heldTime < GetLifetime() || now - this->createTime < GetLifetime()) {
			return false;
		}

		return true;
	}

	bool SwordInstance::IsWaitingInvis() const
	{
		if (this->fadedOut || !this->fadingOut) {
			return false;
		}

		return true;
	}

	double SwordInstance::GetLifetime()
	{
		return Config::MagicSword_RemoveDelay;
	}

	void FindNearestNodeHelper::Init()
	{
		inited = true;

		begin = RE::NiPoint3();
		end = RE::NiPoint3();
		temp1 = RE::NiPoint3();
		temp2 = RE::NiPoint3();
		temp3 = RE::NiPoint3();
		temp4 = RE::NiPoint3();

		temp1.x = 0.0f;
		temp1.y = 5000.0f;
		temp1.z = 0.0f;
	}

	RE::NiNode* FindNearestNodeHelper::FindBestNodeInCrosshair(RE::NiNode* root)
	{
		if (!inited) {
			return nullptr;
		}

		auto pcam = RE::PlayerCamera::GetSingleton();
		if (pcam == nullptr) {
			return nullptr;
		}
		auto& camNode = pcam->cameraRoot;
		if (camNode == nullptr) {
			return nullptr;
		}

		auto wt = camNode->world;
		auto wtpos = wt.translate;
		begin.x = wtpos.x;
		begin.y = wtpos.y;
		begin.z = wtpos.z;

		end = Util::Translate(wt, temp1);

		auto r = std::make_shared<tempCalc>();
		r->best = root;
		r->dist = GetDistance(root);

		ExploreCalc(root, r);

		auto ret = r->best;
		return ret;
	}

	void FindNearestNodeHelper::ExploreCalc(const RE::NiNode* current, std::shared_ptr<tempCalc> state)
	{
		auto& arr = current->GetChildren();
		if (arr.empty()) {
			return;
		}

		for (auto& ch : arr) {
			if (ch == nullptr) {
				continue;
			}

			auto cn = ch->AsNode();
			if (cn == nullptr) {
				continue;
			}

			// fade node is stuff like weapon, shield, and they don't allow us to move them by it properly.

			bool exclude = cn->AsFadeNode() != nullptr;
			if (!exclude) {
				RE::COL_LAYER layer = cn->GetCollisionLayer();
				if (layer == RE::COL_LAYER::kUnidentified) {
					exclude = true;
				}

				if (!exclude && !BetterTelekinesisPlugin::excludeActorNodes.empty()) {
					auto& nmb = cn->name;
					if (!nmb.empty() && BetterTelekinesisPlugin::excludeActorNodes.contains(nmb.c_str())) {
						exclude = true;
					}
				}
			}

			if (!exclude) {
				float dx = GetDistance(cn);
				if (dx < state->dist) {
					state->dist = dx;
					state->best = cn;
				}
			}

			ExploreCalc(cn, state);
		}
	}

	float FindNearestNodeHelper::GetDistance(const RE::NiNode* n)
	{
		if (auto np = n->parent; np == nullptr) {
			return 999999.0f;
		}

		auto qpos = n->world.translate;

		temp2 = qpos - begin;
		temp3 = qpos - end;
		temp3 = temp2.Cross(temp3);
		float len1 = temp3.Length();
		temp3 = end - begin;
		float len2 = temp3.Length();

		if (len2 <= 0.0001f) {
			return 999999.0f;
		}

		return len1 / len2;
	}

	float RandomMoveGenerator::GetExtentMult()
	{
		return static_cast<float>(Config::MultiObjectHoverAmount);
	}

	float RandomMoveGenerator::GetCurrentX() const
	{
		return this->currentX;
	}

	float RandomMoveGenerator::GetCurrentY() const
	{
		return this->currentY;
	}

	void RandomMoveGenerator::Update(const float diff)
	{
		if (this->disable || diff <= 0.0f) {
			return;
		}

		if (Config::MultiObjectHoverAmount <= 0.0) {
			this->disable = true;
			return;
		}

		if (this->hasTarget == 0) {
			this->SelectTarget();
		}

		this->UpdateSpeed(diff);

		this->UpdatePos(diff);
	}

	void RandomMoveGenerator::UpdatePos(const float diff)
	{
		this->currentX += this->speedX * diff;
		this->currentY += this->speedY * diff;

		if ((this->hasTarget & 1) != 0) {
			if (this->targetX < 0.0f) {
				if (this->currentX <= this->targetX) {
					this->hasTarget &= 0xFE;
				}
			} else {
				if (this->currentX >= this->targetX) {
					this->hasTarget &= 0xFE;
				}
			}
		}

		if ((this->hasTarget & 2) != 0) {
			if (this->targetY < 0.0f) {
				if (this->currentY <= this->targetY) {
					this->hasTarget &= 0xFD;
				}
			} else {
				if (this->currentY >= this->targetY) {
					this->hasTarget &= 0xFD;
				}
			}
		}
	}

	void RandomMoveGenerator::UpdateSpeed(const float diff)
	{
		float mod;
		if (this->currentX < this->targetX) {
			mod = diff * speedChange;
		} else {
			mod = -(diff * speedChange);
		}

		this->speedX += mod;
		if (std::abs(this->speedX) > maxSpeed) {
			this->speedX = this->speedX < 0.0f ? (-maxSpeed) : maxSpeed;
		}

		if (this->currentY < this->targetY) {
			mod = diff * speedChange;
		} else {
			mod = -(diff * speedChange);
		}

		this->speedY += mod;
		if (std::abs(this->speedY) > maxSpeed) {
			this->speedY = this->speedY < 0.0f ? (-maxSpeed) : maxSpeed;
		}
	}

	void RandomMoveGenerator::SelectTarget()
	{
		thread_local static std::random_device rd;
		thread_local static std::mt19937 generator(rd());
		std::uniform_real_distribution<> distribution(0, 1.0);
		double chosen_x = (distribution(generator) - 0.5) * 2.0 * Config::MultiObjectHoverAmount;
		double chosen_y = (distribution(generator) - 0.5) * 2.0 * Config::MultiObjectHoverAmount;

		int had_q = GetQuadrant(currentX, currentY);
		int has_q = GetQuadrant(static_cast<float>(chosen_x), static_cast<float>(chosen_y));

		if (had_q == has_q) {
			if ((has_q & 1) != 0) {
				chosen_x -= Config::MultiObjectHoverAmount;
			} else {
				chosen_x += Config::MultiObjectHoverAmount;
			}

			if ((has_q & 2) != 0) {
				chosen_y -= Config::MultiObjectHoverAmount;
			} else {
				chosen_y += Config::MultiObjectHoverAmount;
			}
		}

		this->targetX = static_cast<float>(chosen_x);
		this->targetY = static_cast<float>(chosen_y);
		this->hasTarget = 3;
	}

	int RandomMoveGenerator::GetQuadrant(float x, float y)
	{
		int q = 0;

		if (x >= 0.0f) {
			q |= 1;
		}
		if (y >= 0.0f) {
			q |= 2;
		}

		return q;
	}

	void LeveledListHelper::AddLeveledList(std::vector<RE::TESLeveledList*>& ls, unsigned int id)
	{
		auto form = RE::TESForm::LookupByID<RE::TESLeveledList>(id);
		if (form == nullptr) {
			logger::debug(fmt::runtime("Warning: leveled list {#X} was not found!"), id);
			return;
		}

		ls.push_back(form);
	}

	void LeveledListHelper::FindLeveledLists(const schools school, const levels level, std::vector<RE::TESLeveledList*>& all, std::vector<RE::TESLeveledList*>& one)
	{
		struct LevelBase
		{
			RE::FormID all = 0;
			RE::FormID one = 0;
		};

		// [level][school] = {all, one}
		static constexpr std::pair<RE::FormID, RE::FormID> schoolEntries[4][5] = {
			{ { 0x10F64E, 0x9E2B0 },
				{ 0x10F64F, 0x9E2B1 },
				{ 0x10F650, 0x9E2B2 },
				{ 0x10F651, 0x9E2B3 },
				{ 0x10F652, 0x9E2B4 } },
			{ { 0xA297D, 0xA272A },
				{ 0xA297E, 0xA272B },
				{ 0xA297F, 0xA272C },
				{ 0xA2980, 0xA272D },
				{ 0xA2981, 0xA272E } },
			{ { 0xA298C, 0xA2735 },
				{ 0xA298D, 0xA2730 },
				{ 0xA298E, 0xA2731 },
				{ 0xA298F, 0xA2732 },
				{ 0xA2990, 0xA2734 } },
			{ { 0xA2982, 0xA272F },
				{ 0xA2983, 0xA2736 },
				{ 0xA2984, 0xA2737 },
				{ 0xA2985, 0xA2738 },
				{ 0xA2986, 0xA2739 } }
		};

		static constexpr LevelBase levelBases[4] = {
			{ .all = 0xA297A, .one = 0x10FD8C },
			{ .all = 0x10523F, .one = 0x10FD8D },
			{ .all = 0, .one = 0x10FCF0 },
			{ .all = 0, .one = 0x10FCF1 },
		};

		int levelIdx = static_cast<int>(level);
		int schoolIdx = static_cast<int>(school);

		if (level == levels::master) {
			levelIdx = static_cast<int>(levels::expert);
		}

		const auto& [allBaseEntry, oneBaseEntry] = levelBases[levelIdx];
		AddLeveledList(all, allBaseEntry);
		AddLeveledList(one, oneBaseEntry);

		const auto& [allSchoolEntry, oneSchoolEntry] = schoolEntries[levelIdx][schoolIdx];
		if (allSchoolEntry != 0) {
			AddLeveledList(all, allSchoolEntry);
		}
		if (oneSchoolEntry != 0) {
			AddLeveledList(one, oneSchoolEntry);
		}
	}

	void LeveledListHelper::ChangeSpellSchool(RE::SpellItem* spell, RE::TESObjectBOOK* book)
	{
		int minSkill = 0;

		if (auto& efls = spell->effects; !efls.empty()) {
			for (auto x : efls) {
				auto ef = x->baseEffect;
				if (ef == nullptr) {
					continue;
				}

				ef->data.associatedSkill = RE::ActorValue::kAlteration;
				minSkill = std::max(minSkill, ef->GetMinimumSkillLevel());
			}
		}

		std::string str = R"(Clutter\Books\SpellTomeAlterationLowPoly.nif)";
		book->SetModel(str.c_str());

		if (auto form = RE::TESForm::LookupByID<RE::TESObjectSTAT>(0x2FBB3); form != nullptr) {
			book->inventoryModel = form;
		}

		levels lv;
		if (minSkill >= 100) {
			lv = levels::master;
		} else if (minSkill >= 75) {
			lv = levels::expert;
		} else if (minSkill >= 50) {
			lv = levels::adept;
		} else if (minSkill >= 25) {
			lv = levels::apprentice;
		} else {
			lv = levels::novice;
		}

		unsigned int perkId = 0;
		switch (lv) {
		case levels::novice:
			perkId = 0xF2CA6;
			break;
		case levels::apprentice:
			perkId = 0xC44B7;
			break;
		case levels::adept:
			perkId = 0xC44B8;
			break;
		case levels::expert:
			perkId = 0xC44B9;
			break;
		case levels::master:
			perkId = 0xC44BA;
			break;
		}

		auto perk = perkId != 0 ? RE::TESForm::LookupByID<RE::BGSPerk>(perkId) : nullptr;
		if (perk == nullptr) {
			spell->data.castingPerk = nullptr;
		} else {
			spell->data.castingPerk = perk;
		}
	}

	void LeveledListHelper::ActualAdd(RE::TESLeveledList* list, RE::TESObjectBOOK* book)
	{
		if (list != nullptr && book != nullptr) {
			//list->entries.list->addEntry(book, 1, 1, nullptr);

			REL::Relocation<void (*)(RE::TESLeveledList*, int16_t, uint16_t, RE::TESForm*, RE::ContainerItemExtra*)> addEntry{ RELOCATION_ID(14577, 14749).address() };
			addEntry(list, 1, 1, book->As<RE::TESForm>(), nullptr);
		}
	}

	void LeveledListHelper::AddToLeveledList(RE::TESObjectBOOK* spellBook)
	{
		if (spellBook == nullptr || !spellBook->TeachesSpell()) {
			return;
		}

		RE::SpellItem* spell;
		try {
			spell = spellBook->GetSpell();
		} catch (...) {
			logger::debug("AddToLeveledList.getSpell threw exception!");
			return;
		}

		if (spell == nullptr) {
			return;
		}

		if (Config::MakeSwordSpellsAlterationInstead) {
			ChangeSpellSchool(spell, spellBook);
		}

		if (!Config::AddSwordSpellsToLeveledLists) {
			return;
		}

		auto& efls = spell->effects;
		if (efls.empty()) {
			return;
		}

		int high_skill = 0;
		auto av_choice = RE::ActorValue::kTotal;
		for (auto x : efls) {
			auto ef = x->baseEffect;
			if (ef == nullptr) {
				continue;
			}

			high_skill = std::max(high_skill, ef->GetMinimumSkillLevel());
			if (av_choice < RE::ActorValue::kAlteration || av_choice > RE::ActorValue::kRestoration) {
				av_choice = ef->GetMagickSkill();
			}
		}

		schools sc;
		levels lv;

		switch (av_choice) {
		case RE::ActorValue::kAlteration:
			sc = schools::alteration;
			break;
		case RE::ActorValue::kConjuration:
			sc = schools::conjuration;
			break;
		case RE::ActorValue::kDestruction:
			sc = schools::destruction;
			break;
		case RE::ActorValue::kIllusion:
			sc = schools::illusion;
			break;
		case RE::ActorValue::kRestoration:
			sc = schools::restoration;
			break;
		default:
			return;
		}

		if (high_skill >= 100) {
			lv = levels::master;
		} else if (high_skill >= 75) {
			lv = levels::expert;
		} else if (high_skill >= 50) {
			lv = levels::adept;
		} else if (high_skill >= 25) {
			lv = levels::apprentice;
		} else {
			lv = levels::novice;
		}

		std::vector<RE::TESLeveledList*> all;
		std::vector<RE::TESLeveledList*> one;

		FindLeveledLists(sc, lv, all, one);

		for (auto x : all) {
			ActualAdd(x, spellBook);
		}

		for (auto x : one) {
			ActualAdd(x, spellBook);
		}
	}
}
