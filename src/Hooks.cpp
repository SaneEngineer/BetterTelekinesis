#include "BetterTelekinesis/Main.h"
#include "Shared/Utility/Assembly.h"
#include "Shared/Utility/Memory.h"

using namespace Xbyak;
namespace BetterTelekinesis
{
	static bool AE = REL::Module::IsAE();

	void BetterTelekinesisPlugin::Update()
	{
		float diff = RE::Main::QFrameAnimTime();
		Time += diff;

		_reach_spell = 0.0f;
		casting_sword_barrage = false;
		casting_normal = false;
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
								_reach_spell = dist_telek;
							} else if (st == OurSpellTypes::SwordBarrage) {
								casting_sword_barrage = true;
							} else if (st == OurSpellTypes::SwordBlast) {
								continue;
							} else if (reinterpret_cast<RE::TelekinesisEffect*>(x) != nullptr) {
								casting_normal = true;
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
								_reach_spell = dist_telek;
							} else if (st == OurSpellTypes::SwordBarrage) {
								casting_sword_barrage = true;
							} else if (st == OurSpellTypes::SwordBlast) {
								return RE::BSContainer::ForEachResult::kContinue;
							} else if (reinterpret_cast<RE::TelekinesisEffect*>(activeEffect) != nullptr) {
								casting_normal = true;
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
			int counter = ++HeldUpdateCounter;

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

				std::shared_ptr<held_obj_data> od = nullptr;
				if (!CachedHeldHandles.contains(handleId)) {
					od = std::make_shared<held_obj_data>();
					od->ObjectHandleId = handleId;
					od->IsActor = skyrim_cast<RE::GrabActorEffect*>(x) != nullptr;
					od->Effect = x->GetBaseObject();
					CachedHeldHandles[handleId] = od;

					std::shared_ptr<sword_instance> sw = nullptr;
					if (normal_swords->lookup.contains(handleId)) {
						sw = normal_swords->lookup.at(handleId);
						sw->Held = true;
						sw->HeldTime = Time;
						if (normal_swords->forced_grab == sw) {
							normal_swords->forced_grab = nullptr;
						}
					} else if (ghost_swords->lookup.contains(handleId)) {
						sw = ghost_swords->lookup.at(handleId);
						sw->Held = true;
						sw->HeldTime = Time;
						if (ghost_swords->forced_grab == sw) {
							ghost_swords->forced_grab = nullptr;
						}
					}
				} else {
					std::shared_ptr<sword_instance> sw = nullptr;
					if (normal_swords->lookup.contains(handleId)) {
						sw = normal_swords->lookup.at(handleId);
						sw->HeldTime = Time;
					} else if (ghost_swords->lookup.contains(handleId)) {
						sw = ghost_swords->lookup.at(handleId);
						sw->HeldTime = Time;
					}
				}
				od = CachedHeldHandles.find(handleId)->second;
				od->Elapsed += diff;
				od->UpdateCounter = counter;
			}

			std::vector<unsigned int> rem;
			for (auto& [fst, snd] : CachedHeldHandles) {
				if (snd != nullptr) {
					if (snd->UpdateCounter != counter) {
						if (rem.empty()) {
							rem = std::vector<unsigned int>();
						}
						rem.push_back(fst);
						continue;
					}

					auto objHolder = RE::TESObjectREFR::LookupByHandle(snd->ObjectHandleId).get();
					if (objHolder != nullptr) {
						auto ptr = objHolder;
						update_held_object(ptr, snd, ef);
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
					CachedHeldHandles.erase(u);

					std::shared_ptr<sword_instance> sw = nullptr;
					if (normal_swords->lookup.contains(u)) {
						sw = normal_swords->lookup.at(u);
						sw->Held = false;
					} else if (ghost_swords->lookup.contains(u)) {
						sw = ghost_swords->lookup.at(u);
						sw->Held = false;
					}
				}
			}
		}

		UpdateSwordEffects();

		if (Config::AutoLearnTelekinesisVariants) {
			auto prim = PrimarySpells;
			if (prim == nullptr) {
				return;
			}

			auto second = SecondarySpells;
			if (second == nullptr) {
				return;
			}

			auto now = GetTickCount64();
			if (now - _last_check_learn2 < 1000) {
				return;
			}

			_last_check_learn2 = now;

			auto main = RE::Main::GetSingleton();
			if (main == nullptr || !main->gameActive) {
				return;
			}

			plr = RE::PlayerCharacter::GetSingleton();
			if (plr == nullptr) {
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

		if (Config::AutoLearnTelekinesisSpell) {
			auto spells = Spells;
			if (spells == nullptr || spells->getAll().empty()) {
				return;
			}

			auto now = GetTickCount64();
			if (now - _last_check_learn < 1000) {
				return;
			}

			_last_check_learn = now;

			auto main = RE::Main::GetSingleton();
			if (main == nullptr || !main->gameActive) {
				return;
			}

			plr = RE::PlayerCharacter::GetSingleton();
			if (plr == nullptr) {
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

		if (Config::HoldActorDamage > 0.0) {
			auto main = RE::Main::GetSingleton();
			if (main == nullptr || !main->gameActive) {
				return;
			}

			diff = RE::Main::QFrameAnimTime();
			if (diff <= 0.0f) {
				return;
			}

			plr = RE::PlayerCharacter::GetSingleton();
			if (plr == nullptr) {
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

			ForeachHeldHandle([&](const std::shared_ptr<held_obj_data>& dat) {
				if (!dat->IsActor) {
					return;
				}
				{
					auto obj = RE::TESObjectREFR::LookupByHandle(dat->ObjectHandleId).get();
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

	void BetterTelekinesisPlugin::OnMainMenuOpen()
	{
		spellInfos[static_cast<int>(spell_types::reach)] = new spell_info(spell_types::reach);
		spellInfos[static_cast<int>(spell_types::reach)]->Load(Config::SpellInfo_Reach, "SpellInfo_Reach");
		spellInfos[static_cast<int>(spell_types::normal)] = new spell_info(spell_types::normal);
		spellInfos[static_cast<int>(spell_types::normal)]->Load(Config::SpellInfo_Normal, "SpellInfo_Normal");
		spellInfos[static_cast<int>(spell_types::single)] = new spell_info(spell_types::single);
		spellInfos[static_cast<int>(spell_types::single)]->Load(Config::SpellInfo_One, "SpellInfo_One");
		spellInfos[static_cast<int>(spell_types::enemy)] = new spell_info(spell_types::enemy);
		spellInfos[static_cast<int>(spell_types::enemy)]->Load(Config::SpellInfo_NPC, "SpellInfo_NPC");
		spellInfos[static_cast<int>(spell_types::blast)] = new spell_info(spell_types::blast);
		spellInfos[static_cast<int>(spell_types::blast)]->Load(Config::SpellInfo_Blast, "SpellInfo_Blast");
		spellInfos[static_cast<int>(spell_types::barrage)] = new spell_info(spell_types::barrage);
		spellInfos[static_cast<int>(spell_types::barrage)]->Load(Config::SpellInfo_Barr, "SpellInfo_Barr");

		for (auto& spellInfo : spellInfos) {
			auto b = spellInfo->SpellBook;

			if (b != nullptr)
				leveled_list_helper::AddToLeveledList(b);
		}

		auto cac = Util::CachedFormList::TryParse(Config::EffectInfo_Forms, "EffectInfo_Forms", true, false);
		if (cac != nullptr) {
			for (auto x : cac->getAll()) {
				auto ef = x->As<RE::TESEffectShader>();
				if (ef != nullptr)
					EffectInfos.push_back(ef);
			}
		}

		cac = Util::CachedFormList::TryParse(Config::SwordReturn_Marker, "SwordReturn_Marker", true, false);
		if (cac != nullptr && !cac->getAll().empty())
			sword_ReturnMarker = cac->getAll()[0]->As<RE::TESObjectREFR>();

		InitSwords();

		apply_good_stuff();

		if (Config::OverwriteTargetPicker) {
			TempPt1 = RE::NiPoint3();
			TempPt2 = RE::NiPoint3();
			TempPt3 = RE::NiPoint3();
			TempPtBegin = RE::NiPoint3();
			TempPtEnd = RE::NiPoint3();

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
		return std::max(fpick, BetterTelekinesisPlugin::_reach_spell);
	}

	static RE::NiAVObject* GetNearestActorHelper(RE::TESObjectREFR* obj)
	{
		if (obj != nullptr) {
			auto root = obj->Get3D();
			if (root != nullptr) {
				if (Config::GrabActorNodeNearest) {
					auto sel = find_nearest_node_helper::FindBestNodeInCrosshair(root->AsNode());
					if (sel != nullptr) {
						logger::debug(fmt::runtime("Picked up by " + std::string(sel->name.c_str())));

						return sel;
					}

					return root;
				}

				for (auto& x : BetterTelekinesisPlugin::grabActorNodes) {
					if (auto node = root->GetObjectByName(x); node != nullptr) {
						return node;
					}
				}

				return root;
			}
		}

		return nullptr;
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

		uintptr_t addr = 0;
		auto& trampoline = SKSE::GetTrampoline();

		// Allow launch object even if not pulled completely.
		addr = RELOCATION_ID(34250, 35052).address() + REL::Relocate(0x332 - 0x250, 0x95, 0xE7);
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_func, std::uintptr_t a_target, std::uintptr_t a_targetJumpOffset)
			{
				Xbyak::Label funcLabel;
				Xbyak::Label retnLabel;

				Xbyak::Label jump;

				Xbyak::Label NotIf;
				Xbyak::Label NotElse;

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
		struct Patch2 : Xbyak::CodeGenerator
		{
			Patch2(std::uintptr_t a_func, uintptr_t a_target)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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

		// Can't work, projectiles are handled too different in havok.
		//if(SettingsInstance.AllowProjectileTelekinesis)
		{
			/*Memory.WriteHook(new HookParameters()
                {
                    Address = new IntPtr(0x140556CA6).FromBase(),
                    IncludeLength = 6,
                    ReplaceLength = 6,
                    Before = ctx =>
                    {
                        if (ctx.AX.ToUInt32() == 4)
                            ctx.AX = new IntPtr(3);
                    }
                });

                // 83 C0 FC 83 F8 01
                Memory.WriteHook(new HookParameters()
                {
                    Address = new IntPtr(0x1406AB9F9).FromBase(),
                    IncludeLength = 6,
                    ReplaceLength = 6,
                    Before = ctx =>
                    {
                        if (ctx.AX.ToUInt32() == 4)
                            ctx.AX = new IntPtr(3);
                    }
                });

                Memory.WriteHook(new HookParameters()
                {
                    Address = new IntPtr(0x1406AB19F).FromBase(),
                    IncludeLength = 6,
                    ReplaceLength = 6,
                    After = ctx =>
                    {
                        ctx.AX = IntPtr.Zero;
                    }
                });*/

			/*var addr = gi(33822, 0xCC6 - 0xC30, "40 0F B6 FF 8D 48 FC B8 01 00 00 00 3B C8 0F 46 F8");
                Memory.WriteHook(new HookParameters()
                {
                    Address = addr,
                    IncludeLength = 0,
                    ReplaceLength = 0x11,
                    Before = ctx =>
                    {
                        CollisionLayers layer = (CollisionLayers)ctx.AX.ToInt32Safe();
                        bool ok = false;
                        switch(layer)
                        {
                            // Default game only allows clutter or weapon.
                            case CollisionLayers.Clutter:
                            case CollisionLayers.Weapon:
                                ok = true;
                                break;

                            case CollisionLayers.Projectile:
                                ok = true;
                                break;
                        }

                        ctx.DI = new IntPtr(ok ? 1 : 0);
                    }
                });*/

			// Since all the ones we want to allow are sequential we can only change the comparison operand.
			//var addr = gi(33822, 0xCCD - 0xC30, "B8 01 00 00 00");
			//Memory.WriteUInt8(addr + 1, 2, true);
		}

		if (Config::DontDeactivateHavokHoldSpring) {
			// Don't allow spring to deactivate.
			addr = RELOCATION_ID(61571, 62469).address() + REL::Relocate(0x9AE - 0x980, 0x2E);
			REL::safe_fill(addr, 0x90, 2);
		}

		if (Config::GrabActorNodeNearest || (!Config::GrabActorNodePriority.empty() && Config::GrabActorNodePriority != "NPC Spine2 [Spn2]")) {
			auto spl = Config::GrabActorNodeNearest ? std::vector<std::string>() : Util::StringHelpers::split(!Config::GrabActorNodePriority.empty() ? Config::GrabActorNodePriority : "", ';', true);
			if (!spl.empty()) {
				grabActorNodes = spl;
			}

			spl = !Config::GrabActorNodeNearest ? std::vector<std::string>() : Util::StringHelpers::split((!Config::GrabActorNodeExclude.empty() ? Config::GrabActorNodeExclude : ""), ';', true);
			if (!spl.empty()) {
				ExcludeActorNodes = std::unordered_set<std::string, Util::case_insensitive_unordered_set::hash>();
				for (auto& x : spl) {
					ExcludeActorNodes.insert(x);
				}
			}

			if (Config::GrabActorNodeNearest || !grabActorNodes.empty()) {
				addr = RELOCATION_ID(33826, 34618).address();
				//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 6, Before = [&](std::any ctx) {
				struct Patch3 : Xbyak::CodeGenerator
				{
					Patch3(std::uintptr_t a_func, uintptr_t a_target)
					{
						Xbyak::Label retnLabel;
						Xbyak::Label funcLabel;

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel]);
						add(rsp, 0x20);

						jmp(ptr[rip + retnLabel]);

						L(funcLabel);
						dq(a_func);

						L(retnLabel);
						dq(a_target + 0x6);
					}
				};
				Patch3 patch3(reinterpret_cast<uintptr_t>(GetNearestActorHelper), addr);
				patch3.ready();

				trampoline.write_branch<6>(addr, trampoline.allocate(patch3));
				Memory::Internal::write<uint8_t>(addr + 6, 0xC3, true);
			}
		}

		//Fix Telekinesis Launch Angle in VR
		if (REL::Module::IsVR()) {
			addr = RELOCATION_ID(34250, 35052).address() + 0x1C4;
			struct Patch4 : Xbyak::CodeGenerator
			{
				Patch4(uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label retnLabel;
					Xbyak::Label funcLabel;

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
			apply_overwrite_target_pick();
		}

		if (Config::TelekinesisMaxObjects > 1) {
			apply_multi_telekinesis();
		}
	}

	void BetterTelekinesisPlugin::ForeachHeldHandle(const std::function<void(std::shared_ptr<held_obj_data>)>& func)
	{
		if (func == nullptr) {
			return;
		}

		//Scoped lock likes unlocking an already unlocked mutex
		std::shared_lock lock(CachedHandlesLocker);
		for (auto& val : CachedHeldHandles | std::views::values) {
			func(val);
		}
	}

	float BetterTelekinesisPlugin::CalculateCurrentTelekinesisDamage(RE::PlayerCharacter* ptrPlr, RE::Actor* actorPtr)
	{
		float damBase = 0.0f;
		float damMult = 1.0f;

		float dam = Memory::Internal::read<float>(addr_TeleDamBase);

		RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModTelekinesisDamage, ptrPlr, actorPtr, dam);
		damBase = dam;
		dam = Memory::Internal::read<float>(addr_TeleDamMult);

		RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModTelekinesisDamageMult, ptrPlr, actorPtr, dam);
		damMult = dam;

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

	bool BetterTelekinesisPlugin::find_collision_node(RE::NiNode* root, const int depth)
	{
		if (root == nullptr) {
			return false;
		}

		if (root->collisionObject != nullptr) {
			return true;
		}

		if (depth < 4) {
			auto& chls = root->GetChildren();
			if (chls.empty()) {
				for (auto& ch : chls) {
					auto cn = ch->AsNode();
					if (cn != nullptr && find_collision_node(cn, depth + 1)) {
						return true;
					}
				}
			}
		}

		return false;
	}

#pragma warning(push)
#pragma warning(disable: 4100)
	void BetterTelekinesisPlugin::write_float(const uintptr_t SE_id, const uintptr_t AE_id, const float value)
	{
		if (value < 0.0f)
			return;
		Memory::Internal::write<float>(RELOCATION_ID(SE_id, AE_id).address() + REL::Relocate(8, 8), value);
	}

	void BetterTelekinesisPlugin::write_float_mult(const uintptr_t SE_id, const uintptr_t AE_id, const float value)
	{
		if (value == 1.0f)
			return;
		auto prev = Memory::Internal::read<float>(RELOCATION_ID(SE_id, AE_id).address() + REL::Relocate(8, 8));
		Memory::Internal::write<float>(RELOCATION_ID(SE_id, AE_id).address() + REL::Relocate(8, 8), value * prev);
	}
#pragma warning(pop)

	void BetterTelekinesisPlugin::apply_good_stuff()
	{
		write_float_mult(506184, 376031, static_cast<float>(Config::BaseDistanceMultiplier));
		write_float_mult(506190, 376040, static_cast<float>(Config::BaseDamageMultiplier));
		write_float(506149, 375978, static_cast<float>(Config::ObjectPullSpeedBase));
		write_float(506151, 375981, static_cast<float>(Config::ObjectPullSpeedAccel));
		write_float(506153, 375984, static_cast<float>(Config::ObjectPullSpeedMax));
		write_float_mult(506157, 375990, static_cast<float>(Config::ObjectThrowForce));
		write_float(506196, 376049, static_cast<float>(Config::ActorPullSpeed));
		write_float_mult(506199, 376054, static_cast<float>(Config::ActorThrowForce));
		write_float_mult(506155, 375987, static_cast<float>(Config::ObjectHoldDistance));
		write_float_mult(506194, 376046, static_cast<float>(Config::ActorHoldDistance));

		find_nearest_node_helper::init();

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
				write_float(506169, 376008, ls[0]);
				write_float(506161, 375996, ls[1]);

				// spring damping
				write_float(506167, 376005, ls[2]);
				write_float(506159, 375993, ls[3]);

				// object damping
				write_float(506171, 376011, ls[4]);
				write_float(506163, 375999, ls[5]);

				// max force
				write_float(506173, 376014, ls[6]);
				write_float(506165, 376002, ls[7]);
			}
		}
	}

	void BetterTelekinesisPlugin::_try_drop_now()
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
				drop_timer = GetTickCount();
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
		_next_update_telek = true;
	}

	bool BetterTelekinesisPlugin::ShouldUpdateTelekinesis(const uint32_t now)
	{
		if (_next_update_telek) {
			_next_update_telek = false;
			_last_updated_telek = now;
			return true;
		}

		if (Config::TelekinesisTargetOnlyUpdateIfWeaponOut) {
			auto plr = RE::PlayerCharacter::GetSingleton();
			if (plr != nullptr && plr->AsActorState()->IsWeaponDrawn()) {
				if (!_last_weap_out) {
					_last_weap_out = true;
					_last_updated_telek = now;
					return true;
				}
			} else {
				_last_weap_out = false;
				return false;
			}
		}

		auto delay = static_cast<uint32_t>(Config::TelekinesisTargetUpdateInterval * 1000.0);
		if (now - _last_updated_telek >= delay) {
			_last_updated_telek = now;
			return true;
		}

		return false;
	}

	static unsigned int GetHandleId()
	{
		unsigned int handleId = 0;
		{
			std::scoped_lock lock(locker_picked);
			if (!BetterTelekinesisPlugin::grabactor_picked.empty()) {
				std::unordered_set<unsigned int> alreadyChosen;
				bool hasBad = false;
				BetterTelekinesisPlugin::ForeachHeldHandle([&](const std::shared_ptr<BetterTelekinesisPlugin::held_obj_data>& dat) {
					if (hasBad) {
						return;
					}

					if (dat->Effect != nullptr || !dat->IsActor) {
						hasBad = true;
					} else {
						if (alreadyChosen.empty()) {
							alreadyChosen = std::unordered_set<unsigned int>();
						}
						alreadyChosen.insert(dat->ObjectHandleId);
					}
				});

				if (!hasBad) {
					for (auto x : BetterTelekinesisPlugin::grabactor_picked) {
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

	void BetterTelekinesisPlugin::apply_overwrite_target_pick()
	{
		auto addr = RELOCATION_ID(33677, 34457).address() + REL::Relocate(0x10A5 - 0x1010, 0x9A, 0xA2);
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_func, uintptr_t a_target, std::uintptr_t a_rbpOffset, std::uintptr_t a_targetOffset)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				Xbyak::Label NotIf;

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

	BetterTelekinesisPlugin::spell_info* BetterTelekinesisPlugin::spell_info::Load(const std::string& str, const std::string& setting)
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
					this->SpellBook = cac->getAll()[0]->As<RE::TESObjectBOOK>();
				} else if (i == 1) {
					this->Spell = cac->getAll()[0]->As<RE::SpellItem>();
				} else if (i == 2) {
					this->Effect = cac->getAll()[0]->As<RE::EffectSetting>();
				}
				/*else if (i >= 3)
				{
				    var form = cac.All[0];
				    if (form != null)
				    {
				        uint formId = form.FormId;
				        if (this.Item == null)
				            this.Item = new HashSet<uint>();
				        this.Item.Add(formId);
				    }
				}*/
			}
		}

		switch (this->type) {
		case spell_types::blast:
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

		case spell_types::barrage:
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

	void BetterTelekinesisPlugin::spell_info::ProduceItem(RE::FormID formId, const std::string& formFile, const std::string& model)
	{
		auto form = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(formId, formFile);
		if (form == nullptr) {
			return;
		}

		if (this->Item.empty()) {
			this->Item = std::unordered_set<RE::FormID>();
		}

		this->Item.insert(form->GetFormID());
		form->SetModel(model.c_str());
	}

	void BetterTelekinesisPlugin::begin_profile()
	{
		if (!Config::DebugLogMode) {
			return;
		}

		if (_profile_timer == nullptr) {
			_profile_timer = std::make_unique<stopwatch::Stopwatch>();
			_profile_timer->start();
		}

		_profile_last = _profile_timer->elapsed<>();
		_profile_index = 0;
	}

	void BetterTelekinesisPlugin::step_profile()
	{
		if (!Config::DebugLogMode) {
			return;
		}

		uint64_t t = _profile_timer->elapsed<>();
		uint64_t diff = t - _profile_last;
		_profile_last = t;
		_profile_times[_profile_index++] += diff;
	}

	void BetterTelekinesisPlugin::end_profile()
	{
		if (!Config::DebugLogMode) {
			return;
		}

		_profile_counter++;

		uint32_t now = GetTickCount();
		if (now - _profile_report < 3000) {
			return;
		}

		_profile_report = now;
		std::string bld;
		for (int i = 0; i < _profile_times.size(); i++) {
			auto tot = _profile_times[i];
			if (tot == 0) {
				continue;
			}

			double avg = static_cast<double>(tot) / static_cast<double>(_profile_counter);

			if (bld.empty() != 0) {
				bld.append("  ");
			}
			bld.append(fmt::format(fmt::runtime("[" + std::to_string(i) + "] = {.3f}"), avg));
		}

		logger::debug(fmt::runtime(bld + " <- " + std::to_string(_profile_counter)));
	}

	bool BetterTelekinesisPlugin::is_cell_within_dist(const float myX, const float myY, const int coordX, const int coordY, const float maxDist)
	{
		float minX = static_cast<float>(coordX) * 4096.0f;
		float maxX = static_cast<float>(coordX + 1) * 4096.0f;
		float minY = static_cast<float>(coordY) * 4096.0f;
		float maxY = static_cast<float>(coordY + 1) * 4096.0f;

		float smallestDist = 999999.0f;
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
		if (who == nullptr || who->IsPlayer()) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		// staggered?
		/*uint data = (Memory.ReadUInt32(who.Cast<Actor>() + 0xC4) >> 13) & 1;
		if (data == 0)
		    return;*/

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
			if (now - last_debug_pick >= 1000) {
				last_debug_pick = now;
				debug_pick = true;

				logger::debug(fmt::runtime("================================= (" + std::to_string(GetCurrentRelevantActiveEffects().size()) + ")"));
			} else {
				debug_pick = false;
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
			if (Config::DebugLogMode && debug_pick) {
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

		ForeachHeldHandle([&](const std::shared_ptr<held_obj_data>& dat) {
			{
				auto objHold = RE::TESObjectREFR::LookupByHandle(dat->ObjectHandleId).get();
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

		TempPtBegin.x = begin[0];
		TempPtBegin.y = begin[1];
		TempPtBegin.z = begin[2];
		TempPtEnd.x = end[0];
		TempPtEnd.y = end[1];
		TempPtEnd.z = end[2];

		auto data = std::make_shared<telek_calc_data>();
		data->begin = begin;
		data->end = end;
		data->chls = std::vector<std::unique_ptr<telek_obj_data>>();
		data->findMask = findMask;
		data->maxDistance = maxDistance;
		data->ignore = std::unordered_set<RE::NiNode*>();
		for (auto n : plrNodes) {
			data->ignore.insert(n->AsNode());
		}
		data->ignore_handle = ignoreHandles;
		data->casting = CalculateCasting();

		find_best_telekinesis(cell, data);

		if (!cell->IsInteriorCell()) {
			for (uint32_t x = 0; x < tes->gridCells->length; ++x) {
				for (uint32_t y = 0; y < tes->gridCells->length; ++y) {
					if (auto c = tes->gridCells->GetCell(x, y)) {
						if (cell->formID == c->formID) {
							continue;
						}

						if (!is_cell_within_dist(begin[0], begin[1], c->GetCoordinates()->cellX, c->GetCoordinates()->cellY, maxDistance)) {
							continue;
						}

						find_best_telekinesis(c, data);
					}
				}
			}
		}

		if (data->chls.empty()) {
			if (Config::DebugLogMode && debug_pick) {
				logger::debug("Didn't find any valid object for ray pick");
			}

			return;
		}

		if (data->chls.size() > 1) {
			std::ranges::sort(data->chls, {}, &telek_obj_data::distFromRay);
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

				auto cobj = Raycast::getAVObject(body);
				if (cobj) {
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
				if (Config::DebugLogMode && debug_pick) {
					logger::debug(fmt::runtime("Checked BAD object (no LOS): " + std::string(odata->obj->GetName())));
				}

				continue;
			}

			auto objRefHold = RE::TESObjectREFR::LookupByHandle(odata->obj->GetHandle().native_handle()).get();
			if (objRefHold != nullptr) {
				if (isActor) {
					grabactor_picked.push_back(objRefHold->GetHandle().native_handle());
					if (Config::DebugLogMode && debug_pick) {
						logger::debug(fmt::runtime("Returned actor: " + std::string(odata->obj->GetName()) + "; dist = " + std::to_string(odata->distFromRay)));
					}
					actorLeftTake--;
				} else {
					telekinesis_picked.push_back(objRefHold->GetHandle().native_handle());
					if (Config::DebugLogMode && debug_pick) {
						logger::debug(fmt::runtime("Returned object: " + std::string(odata->obj->GetName()) + "; dist = " + std::to_string(odata->distFromRay)));
					}
					objLeftTake--;
				}
			}

			data->chls[i] = std::move(odata);
		}
	}

	void BetterTelekinesisPlugin::process_one_obj(RE::TESObjectREFR* obj, const std::shared_ptr<telek_calc_data>& data, float quickMaxDist)
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

		step_profile();  // end of 0

		unsigned int formFlag = obj->formFlags;
		if (formFlag & RE::TESForm::RecordFlags::kDisabled || formFlag & RE::TESForm::RecordFlags::kDeleted) {
			return;
		}

		step_profile();  // end of 1

		auto objHolder = obj->GetHandle();
		if (!obj->IsHandleValid()) {
			return;
		}
		unsigned int thisHandle = objHolder.native_handle();
		if (std::ranges::find(data->ignore_handle, thisHandle) != data->ignore_handle.end()) {
			return;
		}

		step_profile();  // end of 2

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

		step_profile();  // end of 3

		TempPt1.x = 0.0f;
		TempPt1.y = 0.0f;
		TempPt1.z = 0.0f;
		TempPt1 = obj->GetBoundMin();
		//obj->InvokeVTableThisCall<RE::TESObjectREFR*>(0x398, TempPt1);

		TempPt2.x = 0.0f;
		TempPt2.y = 0.0f;
		TempPt2.z = 0.0f;
		TempPt2 = obj->GetBoundMax();
		//obj->InvokeVTableThisCall<RE::TESObjectREFR*>(0x3A0, TempPt2);

		step_profile();  // end of 4

		// This isn't perfect way to do it in case object is rotated strangely but those are not common cases.
		TempPt1.x = objBaseX + ((TempPt2.x - TempPt1.x) * 0.5f + TempPt1.x);
		TempPt1.y = objBaseY + ((TempPt2.y - TempPt1.y) * 0.5f + TempPt1.y);
		TempPt1.z = objBaseZ + ((TempPt2.z - TempPt1.z) * 0.5f + TempPt1.z);

		float objTotalDist = TempPtBegin.GetDistance(TempPt1);

		if (objTotalDist > data->maxDistance) {
			return;
		}

		step_profile();  // end of 5
		TempPt2 = TempPtBegin - TempPt1;
		TempPt3 = TempPtEnd - TempPtBegin;
		float dot = TempPt2.Dot(TempPt3);
		TempPt2 = TempPtEnd - TempPtBegin;
		float len = TempPt2.Length();
		len *= len;
		float t = -1.0f;
		if (len > 0.0f) {
			t = -(dot / len);
		}

		float distResult = 999999.0f;
		if (t > 0.0f && t < 1.0f) {
			TempPt2 = TempPt1 - TempPtBegin;   // TempPt1 - TempPtBegin -> TempPt2
			TempPt3 = TempPt1 - TempPtEnd;     // TempPt1 - TempPtEnd -> TempPt3
			TempPt2 = TempPt2.Cross(TempPt3);  // TempPt2 X TempPt3 -> TempPt2
			float dist1 = TempPt2.Length();
			TempPt3 = TempPtBegin - TempPtEnd;
			float dist2 = TempPt3.Length();
			if (dist2 > 0.0f) {
				distResult = dist1 / dist2;
			}
		} else {
			TempPt2 = TempPtBegin - TempPt1;
			float dist1 = TempPt2.Length();
			TempPt2 = TempPtEnd - TempPt1;
			float dist2 = TempPt2.Length();
			distResult = std::min(dist1, dist2);
		}

		double maxDistFromRay = actor != nullptr ? Config::ActorTargetPickerRange : Config::ObjectTargetPickerRange;
		if (distResult > maxDistFromRay) {
			return;
		}

		step_profile();  // end of 6

		// Verify object.
		if (actor == nullptr) {
			REL::Relocation<bool (*)(RE::TESObjectREFR*)> CanBeTelekinesis{ addr_CanBeTelekinesis };
			if (!CanBeTelekinesis(obj)) {
				return;
			}
		}

		step_profile();  // end of 7

		if (!CanPickTelekinesisTarget(obj, data->casting)) {
			return;
		}

		step_profile();  // end of 8

		auto odata = std::make_unique<telek_obj_data>();
		odata->obj = obj;
		odata->distFromRay = distResult;
		odata->x = TempPt1.x;
		odata->y = TempPt1.y;
		odata->z = TempPt1.z;
		data->chls.push_back(std::move(odata));

		step_profile();  // end of 9
	}

	void BetterTelekinesisPlugin::find_best_telekinesis(RE::TESObjectCELL* cell, const std::shared_ptr<telek_calc_data>& data)
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
						begin_profile();
						process_one_obj(obj.get(), data, quickMaxDist);
						//arrow_debug = false;
						end_profile();
					}
				}
			}

		} catch (...) {
		}

		cell->GetRuntimeData().spinLock.Unlock();
	}

	int BetterTelekinesisPlugin::GetCurrentTelekinesisObjectCount(int valueIfActorGrabbed)
	{
		int hasObj = 0;
		bool hadActor = false;
		ForeachHeldHandle([&](const std::shared_ptr<held_obj_data>& dat) {
			if (hadActor) {
				return;
			}

			if (dat->IsActor) {
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
		auto cg = BetterTelekinesisPlugin::current_grabindex;
		if (cg != 0) {
			if (BetterTelekinesisPlugin::_dont_call_clear == 0) {
				BetterTelekinesisPlugin::free_grabindex(cg, "unexpected clear grabbed objects");
			}
		}
	}

	static void ClearGrabbedObjectsHelper2(uintptr_t effect)
	{
		BetterTelekinesisPlugin::free_grabindex(effect, "dtor");
	}

	static void TelekinesisApplyHelper(uintptr_t effect)
	{
		BetterTelekinesisPlugin::switch_to_grabindex(effect, "add effect");
		BetterTelekinesisPlugin::_dont_call_clear++;
	}

	static void TelekinesisApplyHelper2()
	{
		BetterTelekinesisPlugin::switch_to_grabindex(0, "add effect finished");
		BetterTelekinesisPlugin::_dont_call_clear--;
	}

	static void TelekinesisApplyHelper3(uintptr_t effect)
	{
		BetterTelekinesisPlugin::switch_to_grabindex(effect, "end of effect launch");
	}

	static void TelekinesisApplyHelper4(uintptr_t addr)
	{
		BetterTelekinesisPlugin::free_grabindex(addr, "end of effect launch finished");
		BetterTelekinesisPlugin::switch_to_grabindex(0, "end of effect launch finished");
	}

	static void TelekinesisApplyHelper5(uintptr_t effect)
	{
		float diff = RE::Main::QFrameAnimTime();
		BetterTelekinesisPlugin::switch_to_grabindex(effect, "update begin", diff);
		BetterTelekinesisPlugin::_dont_call_clear++;
	}

	static void TelekinesisApplyHelper6()
	{
		BetterTelekinesisPlugin::switch_to_grabindex(0, "update end");
		BetterTelekinesisPlugin::_dont_call_clear--;
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
						auto efs = plr->AsMagicTarget()->GetActiveEffectList();  //FindFirstEffectWithArchetype
						if (efs) {
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

	void BetterTelekinesisPlugin::apply_multi_telekinesis()
	{
		auto& trampoline = SKSE::GetTrampoline();

		// Clear grab objects func itself.
		uintptr_t addr = RELOCATION_ID(39480, 40557).address() + REL::Relocate(0x30, 0x30, 0x31);

		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_func, uintptr_t a_target, Reg64 a_movReg, Reg64 a_rcxStoreReg)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		struct Patch2 : Xbyak::CodeGenerator
		{
			Patch2(std::uintptr_t a_func, uintptr_t a_target)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation2);

		addr = RELOCATION_ID(34259, 35046).address();
		// Telekinesis apply begin.
		struct Patch3 : Xbyak::CodeGenerator
		{
			Patch3(std::uintptr_t a_func, uintptr_t a_target)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		struct Patch4 : Xbyak::CodeGenerator
		{
			Patch4(std::uintptr_t a_func, uintptr_t a_target)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		struct Patch5 : Xbyak::CodeGenerator
		{
			Patch5(std::uintptr_t a_func, uintptr_t a_target)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		struct Patch6 : Xbyak::CodeGenerator
		{
			Patch6(std::uintptr_t a_func, std::uintptr_t a_target, std::uintptr_t a_targetOffset)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		struct Patch7 : Xbyak::CodeGenerator
		{
			Patch7(std::uintptr_t a_func, uintptr_t a_target, std::uintptr_t a_rspOffset)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		struct Patch8 : Xbyak::CodeGenerator
		{
			Patch8(std::uintptr_t a_func, std::uintptr_t a_target, std::uintptr_t a_rspOffset, std::uintptr_t a_targetOffset)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
			Utility::Memory::SafeWrite(addr + 0x5, Utility::Assembly::NoOperation3);
		}

		addr = RELOCATION_ID(34260, 35047).address() + REL::Relocate(0x70B3 - 0x6E40, 0x305, 0x32A);
		struct Patch9 : Xbyak::CodeGenerator
		{
			Patch9(std::uintptr_t a_func, uintptr_t a_target, std::uintptr_t a_rspOffset, Xbyak::Reg64 a_popReg, std::uintptr_t a_targetOffset)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

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
		Patch9 patch9(reinterpret_cast<uintptr_t>(TelekinesisApplyHelper6), addr, REL::Relocate(0x50, 0x40, 0x60), Xbyak::Reg64(REL::Relocate(Xbyak::Reg64::RDI, Xbyak::Reg64::R14, Xbyak::Reg64::R15)), REL::Relocate(0x5, 0x6, 0x6));
		patch9.ready();

		if (REL::Module::IsSE()) {
			trampoline.write_branch<5>(addr, trampoline.allocate(patch9));
		} else {
			trampoline.write_branch<6>(addr, trampoline.allocate(patch9));
		}

		// Allow more than one instance of the telekinesis active effect.
		if (addr = RELOCATION_ID(33781, 34577).address() + REL::Relocate(0xA29 - 0xA20, 0x9); REL::make_pattern<"48 39 42 48">().match(addr)) {
			struct Patch10 : Xbyak::CodeGenerator
			{
				Patch10(std::uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label retnLabel;
					Xbyak::Label funcLabel;

					Xbyak::Label IfNull;

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
		struct Patch11 : Xbyak::CodeGenerator
		{
			Patch11(std::uintptr_t a_func, uintptr_t a_target, Xbyak::Reg64 a_raxStoreReg)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				Xbyak::Label NotSkip;

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
		struct Patch12 : Xbyak::CodeGenerator
		{
			Patch12(std::uintptr_t a_func, uintptr_t a_target)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;
				Xbyak::Label funcLabel2;

				Xbyak::Label NotSkip;

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

	BetterTelekinesisPlugin::saved_grab_index::saved_grab_index() = default;

	int BetterTelekinesisPlugin::unsafe_find_free_index()
	{
		std::vector<unsigned char> taken_bits(13);
		for (auto& [fst, snd] : saved_grabindex) {
			if (fst == 0) {
				continue;
			}

			int ti = snd->index_of_obj;
			if (ti < 0 || ti >= 100) {
				continue;
			}

			int ix = ti / 8;
			int jx = ti % 8;
			taken_bits[ix] |= static_cast<unsigned char>(1 << jx);
		}

		if (casting_sword_barrage) {
			for (int j = 0; j < 8; j++) {
				int ji = (_placement_barrage + j) % 8;
				ji++;

				int ix = ji / 8;
				int jx = ji % 8;
				if ((taken_bits[ix] & static_cast<unsigned char>(1 << jx)) == 0) {
					_placement_barrage++;
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

	void BetterTelekinesisPlugin::switch_to_grabindex(const uintptr_t addr, const std::string& reason, const float diff)
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		{
			std::scoped_lock lock(grabindex_locker);

			std::shared_ptr<saved_grab_index> g;

			if (!saved_grabindex.contains(0)) {
				g = std::make_shared<saved_grab_index>();
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
				g->index_of_obj = -1;

				saved_grabindex[0] = g;
			}

			if (!saved_grabindex.contains(addr)) {
				g = std::make_shared<saved_grab_index>();
				g->addr = addr;
				g->index_of_obj = unsafe_find_free_index();
				g->rng = std::make_unique<random_move_generator>();

				saved_grabindex[addr] = g;
			} else {
				g = saved_grabindex.find(addr)->second;

				if (diff > 0.0f && g->rng) {
					g->rng->update(diff);
				}
			}

			logger::debug(fmt::runtime("switch {:#x} -> {:#x} ({}) "), current_grabindex, addr, reason);

			if (current_grabindex == addr) {
				return;
			}

			auto it = saved_grabindex.find(current_grabindex);
			auto& prev = it->second;

			if (!REL::Module::IsVR()) {
				prev->wgt = plr->GetPlayerRuntimeData().grabData.grabObjectWeight;
				prev->dist = plr->GetPlayerRuntimeData().grabData.grabDistance;
				prev->handle = plr->GetPlayerRuntimeData().grabData.grabbedObject.native_handle();
				//prev->spring = plr->GetPlayerRuntimeData().grabData.grabSpring;
				memcpy_s(prev->spring, 0x30, plr->GetPlayerRuntimeData().grabData.grabSpring.data(), 0x30);
				prev->grabtype = plr->GetPlayerRuntimeData().grabType.get();

				current_grabindex = addr;

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

				current_grabindex = addr;

				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabObjectWeight = g->wgt;
				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabDistance = g->dist;
				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabbedObject = RE::TESObjectREFR::LookupByHandle(g->handle).get();
				//plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring = g->spring;
				memcpy_s(plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabSpring.data(), 0x30, g->spring, 0x30);
				plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabType = g->grabtype;
			}
		}
	}

	void BetterTelekinesisPlugin::free_grabindex(uintptr_t addr, const std::string& reason)
	{
		if (addr == 0) {
			return;
		}

		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}
		{
			std::scoped_lock lock(grabindex_locker);

			logger::debug(fmt::runtime("free {:#x} ({})"), addr, reason);

			if (!saved_grabindex.contains(addr)) {
				return;
			}

			uintptr_t cur_ind = current_grabindex;
			if (cur_ind != addr) {
				switch_to_grabindex(addr, "need to free");
			}
			// Call the func that drops the items from havok.
			_dont_call_clear = 1;
			if (!REL::Module::IsVR()) {
				plr->DestroyMouseSprings();
			} else {
				auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;

				static REL::Relocation<void*(RE::PlayerCharacter*, RE::VR_DEVICE)> func{ RELOCATION_ID(39480, 40557) };
				func(plr, hand);
			}
			_dont_call_clear = 0;

			if (cur_ind == addr) {
				switch_to_grabindex(0, "returning from free");
			} else {
				switch_to_grabindex(cur_ind, "returning from free");
			}

			saved_grabindex.erase(addr);
		}
	}

	void BetterTelekinesisPlugin::clear_grabindex(const bool onlyIfCount)
	{
		auto plr = RE::PlayerCharacter::GetSingleton();
		if (plr == nullptr) {
			return;
		}

		{
			std::scoped_lock lock(grabindex_locker);

			logger::debug("clear");

			for (const auto& key : saved_grabindex | std::views::keys) {
				if (key == 0) {
					continue;
				}

				free_grabindex(key, "clear");
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
				_dont_call_clear = 1;
				//Clear Grabbed
				if (!REL::Module::IsVR()) {
					plr->DestroyMouseSprings();
				} else {
					auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;
					static REL::Relocation<void*(RE::PlayerCharacter*, RE::VR_DEVICE)> func{ RELOCATION_ID(39480, 40557) };
					func(plr, hand);
				}
				_dont_call_clear = 0;
			}
		}
	}

	void BetterTelekinesisPlugin::_select_rotation_offset(int index, int& x, int& y)
	{
		if (index < 0 || index >= _rot_offsets.size()) {
			return;
		}

		auto& [fst, snd] = _rot_offsets[index];
		x = fst;
		y = snd;
	}

	const std::vector<std::pair<int, int>> BetterTelekinesisPlugin::_rot_offsets = { std::pair(0, 0), std::pair(1, 1), std::pair(1, -1), std::pair(-1, 1), std::pair(-1, -1), std::pair(1, 0), std::pair(-1, 0), std::pair(2, 2), std::pair(0, -1), std::pair(0, 1), std::pair(-2, -2), std::pair(2, 1), std::pair(-2, 2), std::pair(2, -2), std::pair(-2, -1), std::pair(1, 2), std::pair(1, -2), std::pair(-2, 1), std::pair(2, 0), std::pair(-1, -2), std::pair(-1, 2), std::pair(2, -1), std::pair(-2, 0), std::pair(2, 3), std::pair(0, -2), std::pair(0, 2), std::pair(-3, -2), std::pair(3, -2), std::pair(-2, 3), std::pair(3, 2), std::pair(-2, -3), std::pair(-3, 2), std::pair(2, -3), std::pair(1, 3), std::pair(-1, -3), std::pair(-1, 3), std::pair(3, 0), std::pair(-3, -1), std::pair(3, -1), std::pair(-3, 1), std::pair(3, 1), std::pair(0, -3), std::pair(0, 3), std::pair(-3, 0), std::pair(1, -3), std::pair(2, 4), std::pair(-4, -2), std::pair(4, -2), std::pair(-2, 4), std::pair(-2, -4), std::pair(4, 2), std::pair(-4, 2), std::pair(2, -4), std::pair(1, 4), std::pair(-3, -3), std::pair(4, 1), std::pair(-3, 3), std::pair(1, -4), std::pair(3, 3), std::pair(-4, -1), std::pair(4, -1), std::pair(-4, 1), std::pair(3, -3), std::pair(-1, 4), std::pair(-1, -4), std::pair(5, 3), std::pair(-4, 0), std::pair(4, 0), std::pair(-5, 3), std::pair(3, -5), std::pair(0, 4), std::pair(0, -4), std::pair(3, 5), std::pair(-5, -3), std::pair(5, -3), std::pair(-3, 5), std::pair(-3, -5), std::pair(4, 4), std::pair(4, -4), std::pair(-4, 4), std::pair(-4, -4), std::pair(5, 2), std::pair(-5, 2), std::pair(3, -4), std::pair(2, 5), std::pair(-4, -3), std::pair(5, -2), std::pair(-3, 4), std::pair(2, -5), std::pair(1, 5), std::pair(-5, -2), std::pair(5, -1), std::pair(-5, 1), std::pair(4, 3), std::pair(-2, -5), std::pair(-2, 5), std::pair(4, -3), std::pair(-5, -1), std::pair(5, 1), std::pair(-4, 3) };

	void BetterTelekinesisPlugin::activate_node(const RE::NiNode* node)
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

	void BetterTelekinesisPlugin::update_point_forward(RE::TESObjectREFR* refr)
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
			RE::NiQuaternion q;
			pcam->currentState->GetRotation(q);

			const double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
			const double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
			AngleWanted.x = std::atan2(sinr_cosp, cosr_cosp);

			// Pitch (y-axis rotation)
			if (const double sinp = 2 * (q.w * q.y - q.z * q.x); std::abs(sinp) >= 1)
				AngleWanted.y = std::copysign(glm::pi<float>() / 2, sinp);
			else
				AngleWanted.y = std::asin(sinp);

			// Yaw (z-axis rotation)
			const double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
			const double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
			AngleWanted.z = std::atan2(siny_cosp, cosy_cosp);

			AngleWanted.x = AngleWanted.x * -1;
			//euler.y = euler.y;
			AngleWanted.z = AngleWanted.z * -1;
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

	void BetterTelekinesisPlugin::update_held_object(RE::TESObjectREFR* obj, const std::shared_ptr<held_obj_data>& data, const std::vector<RE::ActiveEffect*>& effectList)
	{
		if (obj == nullptr) {
			return;
		}

		if (Config::PointWeaponsAndProjectilesForward) {
			if (obj->As<RE::TESObjectWEAP>() != nullptr || obj->As<RE::Projectile>() != nullptr || IsOurItem(obj->GetBaseObject()) != OurItemTypes::None) {
				update_point_forward(obj);
			}
		}

		if (data->Effect != nullptr && data->Elapsed >= Config::SwordBarrage_FireDelay && IsOurSpell(data->Effect) == OurSpellTypes::SwordBarrage) {
			for (auto x : effectList) {
				uint32_t handleId = 0;
				if (skyrim_cast<RE::TelekinesisEffect*>(x) != nullptr) {
					handleId = skyrim_cast<RE::TelekinesisEffect*>(x)->grabbedObject.native_handle();
				} else if (skyrim_cast<RE::GrabActorEffect*>(x) != nullptr) {
					handleId = skyrim_cast<RE::GrabActorEffect*>(x)->grabbedActor.native_handle();
				}

				if (handleId == data->ObjectHandleId) {
					std::shared_ptr<sword_instance> sw = nullptr;
					if (normal_swords->lookup.contains(handleId)) {
						sw = normal_swords->lookup[handleId];
						sw->LaunchTime = Time;
					} else if (ghost_swords->lookup.contains(handleId)) {
						sw = ghost_swords->lookup[handleId];
						sw->LaunchTime = Time;
					}

					x->Dispel(true);
					break;
				}
			}
		}
	}

	bool BetterTelekinesisPlugin::_has_init_sword = false;

	void BetterTelekinesisPlugin::InitSwords()
	{
		if (_has_init_sword) {
			return;
		}
		_has_init_sword = true;

		sword_data::Temp1 = RE::NiPoint3();
		sword_data::Temp2 = RE::NiPoint3();
		sword_data::Temp3 = RE::NiPoint3();
		sword_data::Return1 = RE::NiPoint3();
		sword_data::Return2 = RE::NiPoint3();

		std::string fileName = "BetterTelekinesis.esp";

		normal_swords->AddSword_FormId(0x80E, fileName, false);
		for (unsigned int u = 0x840; u < 0x870; u++) {
			normal_swords->AddSword_FormId(u, fileName, false);
		}

		ghost_swords->AddSword_FormId(0x80D, fileName, true);
		for (unsigned int u = 0x80F; u <= 0x83F; u++) {
			ghost_swords->AddSword_FormId(u, fileName, true);
		}
	}

	unsigned int BetterTelekinesisPlugin::ghost_sword_effect = 0;
	unsigned int BetterTelekinesisPlugin::normal_sword_effect = 0;

	void BetterTelekinesisPlugin::PlaySwordEffect(RE::TESObjectREFR* obj, bool ghost)
	{
		if (obj->Get3D() == nullptr) {
			return;
		}

		if (ghost) {
			auto form = !EffectInfos.empty() ? *EffectInfos.begin() : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 1.5f);
			}

			if (ghost_sword_effect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(ghost_sword_effect);
				if (form2 != nullptr) {
					obj->ApplyEffectShader(form2, -1.0f);
				}
			}
		} else {
			auto form = !EffectInfos.empty() ? EffectInfos[0] : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 1.5f);
			}

			if (normal_sword_effect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(normal_sword_effect);
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
			if (ghost_sword_effect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(ghost_sword_effect);
				if (form2 != nullptr) {
					//obj->StopEffect(form2);
					REL::Relocation<void (*)(RE::BGSPackageDataBool*, RE::TESObjectREFR*, RE::TESEffectShader*)> StopEffect{ RELOCATION_ID(40381, 41395) };
					StopEffect(reinterpret_cast<RE::BGSPackageDataBool*>(RELOCATION_ID(514167, 400315).address()), obj, form2);
				}
			}

			auto form = EffectInfos.size() >= 2 ? EffectInfos[1] : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 5.0f);
			}
		} else {
			if (normal_sword_effect != 0) {
				auto form2 = RE::TESForm::LookupByID<RE::TESEffectShader>(normal_sword_effect);
				if (form2 != nullptr) {
					//obj->StopEffect(form2);
					REL::Relocation<void (*)(RE::BGSPackageDataBool*, RE::TESObjectREFR*, RE::TESEffectShader*)> StopEffect{ RELOCATION_ID(40381, 41395) };
					StopEffect(reinterpret_cast<RE::BGSPackageDataBool*>(RELOCATION_ID(514167, 400315).address()), obj, form2);
				}
			}

			auto form = EffectInfos.size() >= 2 ? EffectInfos[1] : nullptr;
			if (form != nullptr) {
				obj->ApplyEffectShader(form, 5.0f);
			}
		}
	}

	void BetterTelekinesisPlugin::ReturnSwordToPlace(RE::TESObjectREFR* obj)
	{
		auto marker = sword_ReturnMarker;
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
		moveTo(obj, markerHold, cell, ws, sword_data::Return1, sword_data::Return2);
	}

	float BetterTelekinesisPlugin::first_TeleportZOffset = -2000.0f;

	void BetterTelekinesisPlugin::UpdateSwordEffects()
	{
		double now = Time;

		for (int z = 0; z < 2; z++) {
			auto dat = z == 0 ? normal_swords : ghost_swords;

			if (dat->forced_grab != nullptr) {
				if (now - dat->forced_grab->CreateTime > 0.5) {
					dat->forced_grab = nullptr;
				}
			}

			for (int i = 0; i < dat->swords.size(); i++) {
				auto& sw = dat->swords[i];
				auto objRef = RE::TESObjectREFR::LookupByHandle(sw->Handle).get();

				bool isForced = dat->forced_grab != nullptr && dat->forced_grab->Handle == sw->Handle;
				if (sw->WaitingEffect != 0) {
					bool waitMore = false;
					if (sw->IsWaitingEffect(now)) {
						if (objRef != nullptr) {
							auto root = objRef->Get3D();
							if (root == nullptr) {
								waitMore = true;
							} else {
								if (sw->WaitEffectCounter == 0) {
									auto scb = root->GetObjectByName("Scb");
									if (scb != nullptr) {
										scb->GetFlags() |= RE::NiAVObject::Flag::kHidden;
									}

									if (sw->WaitingEffect == 2) {
										PlaySwordEffect(objRef, true);
									} else if (sw->WaitingEffect == 1) {
										PlaySwordEffect(objRef, false);
									}

									root->local.translate.z -= first_TeleportZOffset;
									RE::NiUpdateData data;
									data.time = -1.0f;
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

									sw->WaitEffectCounter = 1;
									waitMore = true;
								} else if (sw->WaitEffectCounter == 1) {
									activate_node(root->AsNode());
								}
							}
						}
					}

					if (!waitMore) {
						sw->WaitingEffect = 0;
						sw->WaitEffectCounter = 0;
					}
				}

				if (sw->IsWaitingInvis()) {
					if (now - sw->FadeTime > 3.0) {
						sw->FadedOut = true;
						sw->FadingOut = false;

						objRef = RE::TESObjectREFR::LookupByHandle(sw->Handle).get();
						if (objRef->IsHandleValid()) {
							auto obj = objRef;
							ReturnSwordToPlace(obj);
						}
					}
				} else if (!isForced && sw->CanPlayFadeout(now)) {
					sw->FadingOut = true;
					sw->FadeTime = now;

					objRef = RE::TESObjectREFR::LookupByHandle(sw->Handle).get();
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

		double now = Time;
		RE::RefHandle chosen = 0;
		int ci = 0;
		auto data = ghost ? ghost_swords : normal_swords;

		// Barrage rate of fire?
		if (ghost) {
			for (const auto& sw : data->swords) {
				if (now - sw->CreateTime < Config::SwordBarrage_SpawnDelay) {
					return;
				}
			}
		}

		if (data->forced_grab != nullptr) {
			return;
		}

		std::shared_ptr<sword_instance> inst = nullptr;

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
					chosen = sword->Handle;
					data->next_index = chosenIndex + 1;
					inst = sword;
					ci = chosenIndex;
					break;
				}
			}
		}

		if (inst == nullptr) {
			int maxTry = static_cast<int>(data->swords.size());

			for (int i = 0; i < maxTry; i++) {
				int chosenIndex = (data->next_index + i) % maxTry;
				auto& sword = data->swords[chosenIndex];

				if (sword->IsFreeForSummon(now)) {
					chosen = sword->Handle;
					data->next_index = i + 1;
					inst = sword;
					ci = i;
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

		if (!CalculateSwordPlacePosition(50.0f, false, ghost)) {
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
			go[i] = sword_data::Temp2.y * i;
		}
		for (int i = 0; i < 3; i++) {
			go[i + 3] = sword_data::Temp3.y * i;
		}

		sword_data::Temp2.z += first_TeleportZOffset;

		REL::Relocation<void (*)(RE::TESObjectREFR*, RE::ObjectRefHandle&, RE::TESObjectCELL*, RE::TESWorldSpace*, const RE::NiPoint3&, const RE::NiPoint3&)> moveTo{ RELOCATION_ID(56227, 56626) };
		moveTo(objRef, plrHold, cell, ws, sword_data::Temp2, sword_data::Temp3);

		if (inst != nullptr) {
			inst->WaitingEffect = static_cast<unsigned char>(ghost ? 2 : 1);
			inst->CreateTime = now;
			inst->FadedOut = false;
			inst->Goto = go;
			data->forced_grab = inst;
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
			std::scoped_lock lock(SwordPositionLocker);
			for (const auto& key : CachedHeldHandles | std::views::keys) {
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

		sword_data::Temp1.y = ghost ? static_cast<float>(Config::MagicSwordBarrage_PlaceDistance) : static_cast<float>(Config::MagicSwordBlast_PlaceDistance);
		if (ghost) {
			sword_data::Temp1.x = 0.0f;
			sword_data::Temp1.z = 0.0f;
		} else {
			// Some offset?
			sword_data::Temp1.x = 0.0f;
			sword_data::Temp1.z = 0.0f;
		}

		sword_data::Temp2 = Util::Translate(camWt, sword_data::Temp1);

		RE::NiMatrix3 mat;

		if (!REL::Module::IsVR()) {
			auto camRoot = pcam->cameraRoot;

			if (camRoot == nullptr) {
				return false;
			}

			mat = camRoot->world.rotate;
		} else {
			if (CastingLeftHandVR()) {
				mat = plr->GetVRNodeData()->LeftWandNode->world.rotate;
			} else {
				mat = plr->GetVRNodeData()->RightWandNode->world.rotate;
			}
		}

		mat.ToEulerAnglesXYZ(sword_data::Temp3);

		begin = { hpos.x, hpos.y, hpos.z, 0.0f };
		end = { sword_data::Temp2.x, sword_data::Temp2.y, sword_data::Temp2.z, 0.0f };

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

		float frac_extent = extraRadiusOfSword / std::max(1.0f, sword_data::Temp1.y);
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

		sword_data::Temp2.x = end[0];
		sword_data::Temp2.y = end[1];
		sword_data::Temp2.z = end[2];

		return true;
	}

	sword_data* const BetterTelekinesisPlugin::normal_swords = new sword_data();
	sword_data* const BetterTelekinesisPlugin::ghost_swords = new sword_data();

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
			if (ghost_swords->forced_grab == nullptr) {
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

				if (ghost_swords->forced_grab->Handle != handleId) {
					return false;
				}
			}
		}

		if (castingNormal) {
			if (normal_swords->forced_grab == nullptr) {
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

				if (normal_swords->forced_grab->Handle != handleId) {
					return false;
				}
			}
		}

		return true;
	}

	void BetterTelekinesisPlugin::OnFailPickTelekinesisTarget(RE::EffectSetting* efs, const bool failBecauseAlreadyMax)
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
			for (int i = 0; i < static_cast<int>(spell_types::max); i++) {
				auto inf = spellInfos[i];

				if (inf->Effect != nullptr && inf->Effect == ef) {
					switch (static_cast<spell_types>(i)) {
					case spell_types::normal:
						return OurSpellTypes::TelekNormal;
					case spell_types::reach:
						return OurSpellTypes::TelekReach;
					case spell_types::single:
						return OurSpellTypes::TelekOne;
					case spell_types::enemy:
						return OurSpellTypes::None;
					case spell_types::blast:
						return OurSpellTypes::SwordBlast;
					case spell_types::barrage:
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
			for (int i = 0; i < static_cast<int>(spell_types::max); i++) {
				auto inf = spellInfos[i];
				if (!inf->Item.empty() && inf->Item.contains(baseForm->formID)) {
					switch (static_cast<spell_types>(i)) {
					case spell_types::normal:
					case spell_types::reach:
					case spell_types::single:
					case spell_types::enemy:
						return OurItemTypes::None;
					case spell_types::blast:
						return OurItemTypes::IronSword;
					case spell_types::barrage:
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

	void sword_data::AddSword_FormId(unsigned int formId, const std::string& fileName, bool ghost)
	{
		auto form = RE::TESDataHandler::GetSingleton()->LookupForm(formId, fileName);
		if (form == nullptr) {
			return;
		}

		auto refr = form->AsReference();
		if (refr != nullptr) {
			this->AddSword_Obj(refr, ghost);
		}
	}

	void sword_data::AddSword_Obj(RE::TESObjectREFR* obj, const bool ghost)
	{
		if (obj == nullptr) {
			return;
		}

		auto objRef = RE::TESObjectREFR::LookupByHandle(obj->GetHandle().native_handle()).get();
		if (objRef != nullptr) {
			auto sw = std::make_shared<sword_instance>();
			sw->Handle = objRef->GetHandle().native_handle();
			swords.push_back(sw);
			lookup[objRef->GetHandle().native_handle()] = sw;

			int ix = static_cast<int>(swords.size()) - 1;
			auto& allItem = BetterTelekinesisPlugin::spellInfos[ghost ? static_cast<int>(BetterTelekinesisPlugin::spell_types::barrage) : static_cast<int>(BetterTelekinesisPlugin::spell_types::blast)]->Item;
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

	bool sword_instance::IsFreeForSummon(const double now) const
	{
		if (!this->FadedOut || this->Held) {
			return false;
		}

		if (this->IsWaitingEffect(now)) {
			return false;
		}

		if (now - this->LaunchTime < 3.0) {
			return false;
		}

		if (now - this->CreateTime < 3.0) {
			return false;
		}

		return true;
	}

	bool sword_instance::IsWaitingEffect(const double now) const
	{
		return this->WaitingEffect != 0 && now - this->CreateTime < 0.3;
	}

	bool sword_instance::CanPlayFadeout(const double now) const
	{
		if (this->FadedOut || this->Held || this->FadingOut || now - this->HeldTime < getLifetime() || now - this->CreateTime < getLifetime()) {
			return false;
		}

		return true;
	}

	bool sword_instance::IsWaitingInvis() const
	{
		if (this->FadedOut || !this->FadingOut) {
			return false;
		}

		return true;
	}

	double sword_instance::getLifetime()
	{
		return Config::MagicSword_RemoveDelay;
	}

	bool find_nearest_node_helper::inited = false;

	void find_nearest_node_helper::init()
	{
		inited = true;

		Begin = RE::NiPoint3();
		End = RE::NiPoint3();
		Temp1 = RE::NiPoint3();
		Temp2 = RE::NiPoint3();
		Temp3 = RE::NiPoint3();
		Temp4 = RE::NiPoint3();

		Temp1.x = 0.0f;
		Temp1.y = 5000.0f;
		Temp1.z = 0.0f;
	}

	RE::NiNode* find_nearest_node_helper::FindBestNodeInCrosshair(RE::NiNode* root)
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
		Begin.x = wtpos.x;
		Begin.y = wtpos.y;
		Begin.z = wtpos.z;

		End = Util::Translate(wt, Temp1);

		auto r = new temp_calc();
		r->best = root;
		r->dist = GetDistance(root);

		explore_calc(root, r);

		auto ret = r->best;
		delete r;
		return ret;
	}

	void find_nearest_node_helper::explore_calc(const RE::NiNode* current, temp_calc* state)
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

			bool exclude = cn->AsNode() != nullptr;
			if (!exclude) {
				RE::COL_LAYER layer = cn->GetCollisionLayer();
				if (layer == RE::COL_LAYER::kUnidentified) {
					exclude = true;
				}

				if (!exclude && !BetterTelekinesisPlugin::ExcludeActorNodes.empty()) {
					auto& nmb = cn->name;
					if (!nmb.empty() && BetterTelekinesisPlugin::ExcludeActorNodes.contains(nmb.c_str())) {
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

			explore_calc(cn, state);
		}
	}

	float find_nearest_node_helper::GetDistance(const RE::NiNode* n)
	{
		if (auto np = n->parent; np == nullptr) {
			return 999999.0f;
		}

		auto qpos = n->world.translate;

		Temp2 = qpos - Begin;
		Temp3 = qpos - End;
		Temp3 = Temp2.Cross(Temp3);
		float len1 = Temp3.Length();
		Temp3 = End - Begin;
		float len2 = Temp3.Length();

		if (len2 <= 0.0001f) {
			return 999999.0f;
		}

		return len1 / len2;
	}

	float random_move_generator::speed_change = 0.3f;
	float random_move_generator::max_speed = 1.0f;

	float random_move_generator::getExtentMult()
	{
		return static_cast<float>(Config::MultiObjectHoverAmount);
	}

	float random_move_generator::getCurrentX() const
	{
		return this->current_x;
	}

	float random_move_generator::getCurrentY() const
	{
		return this->current_y;
	}

	void random_move_generator::update(const float diff)
	{
		if (this->disable || diff <= 0.0f) {
			return;
		}

		if (Config::MultiObjectHoverAmount <= 0.0) {
			this->disable = true;
			return;
		}

		if (this->has_target == 0) {
			this->select_target();
		}

		this->update_speed(diff);

		this->update_pos(diff);
	}

	void random_move_generator::update_pos(const float diff)
	{
		this->current_x += this->speed_x * diff;
		this->current_y += this->speed_y * diff;

		if ((this->has_target & 1) != 0) {
			if (this->target_x < 0.0f) {
				if (this->current_x <= this->target_x) {
					this->has_target &= 0xFE;
				}
			} else {
				if (this->current_x >= this->target_x) {
					this->has_target &= 0xFE;
				}
			}
		}

		if ((this->has_target & 2) != 0) {
			if (this->target_y < 0.0f) {
				if (this->current_y <= this->target_y) {
					this->has_target &= 0xFD;
				}
			} else {
				if (this->current_y >= this->target_y) {
					this->has_target &= 0xFD;
				}
			}
		}
	}

	void random_move_generator::update_speed(const float diff)
	{
		float mod;
		if (this->current_x < this->target_x) {
			mod = diff * speed_change;
		} else {
			mod = -(diff * speed_change);
		}

		this->speed_x += mod;
		if (std::abs(this->speed_x) > max_speed) {
			this->speed_x = this->speed_x < 0.0f ? (-max_speed) : max_speed;
		}

		if (this->current_y < this->target_y) {
			mod = diff * speed_change;
		} else {
			mod = -(diff * speed_change);
		}

		this->speed_y += mod;
		if (std::abs(this->speed_y) > max_speed) {
			this->speed_y = this->speed_y < 0.0f ? (-max_speed) : max_speed;
		}
	}

	void random_move_generator::select_target()
	{
		thread_local static std::random_device rd;
		thread_local static std::mt19937 generator(rd());
		std::uniform_real_distribution<> distribution(0, 1.0);
		double chosen_x = (distribution(generator) - 0.5) * 2.0 * Config::MultiObjectHoverAmount;
		double chosen_y = (distribution(generator) - 0.5) * 2.0 * Config::MultiObjectHoverAmount;

		int had_q = GetQuadrant(current_x, current_y);
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

		this->target_x = static_cast<float>(chosen_x);
		this->target_y = static_cast<float>(chosen_y);
		this->has_target = 3;
	}

	int random_move_generator::GetQuadrant(float x, float y)
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

	void leveled_list_helper::AddLeveledList(std::vector<RE::TESLeveledList*>& ls, unsigned int id)
	{
		auto form = RE::TESForm::LookupByID<RE::TESLeveledList>(id);
		if (form == nullptr) {
			logger::debug(fmt::runtime("Warning: leveled list {#X} was not found!"), id);
			return;
		}

		ls.push_back(form);
	}

	void leveled_list_helper::FindLeveledLists(const schools school, const levels level, std::vector<RE::TESLeveledList*>& all, std::vector<RE::TESLeveledList*>& one)
	{
		switch (level) {
		case levels::novice:
			{
				AddLeveledList(all, 0xA297A);
				AddLeveledList(one, 0x10FD8C);

				switch (school) {
				case schools::alteration:
					AddLeveledList(all, 0x10F64E);
					AddLeveledList(one, 0x9E2B0);
					break;

				case schools::conjuration:
					AddLeveledList(all, 0x10F64F);
					AddLeveledList(one, 0x9E2B1);
					break;

				case schools::destruction:
					AddLeveledList(all, 0x10F650);
					AddLeveledList(one, 0x9E2B2);
					break;

				case schools::illusion:
					AddLeveledList(all, 0x10F651);
					AddLeveledList(one, 0x9E2B3);
					break;

				case schools::restoration:
					AddLeveledList(all, 0x10F652);
					AddLeveledList(one, 0x9E2B4);
					break;
				}
			}
			break;

		case levels::apprentice:
			{
				AddLeveledList(all, 0x10523F);
				AddLeveledList(one, 0x10FD8D);

				switch (school) {
				case schools::alteration:
					AddLeveledList(all, 0xA297D);
					AddLeveledList(one, 0xA272A);
					break;

				case schools::conjuration:
					AddLeveledList(all, 0xA297E);
					AddLeveledList(one, 0xA272B);
					break;

				case schools::destruction:
					AddLeveledList(all, 0xA297F);
					AddLeveledList(one, 0xA272C);
					break;

				case schools::illusion:
					AddLeveledList(all, 0xA2980);
					AddLeveledList(one, 0xA272D);
					break;

				case schools::restoration:
					AddLeveledList(all, 0xA2981);
					AddLeveledList(one, 0xA272E);
					break;
				}
			}
			break;

		case levels::adept:
			{
				AddLeveledList(one, 0x10FCF0);

				switch (school) {
				case schools::alteration:
					AddLeveledList(all, 0xA298C);
					AddLeveledList(one, 0xA2735);
					break;

				case schools::conjuration:
					AddLeveledList(all, 0xA298D);
					AddLeveledList(one, 0xA2730);
					break;

				case schools::destruction:
					AddLeveledList(all, 0xA298E);
					AddLeveledList(one, 0xA2731);
					break;

				case schools::illusion:
					AddLeveledList(all, 0xA298F);
					AddLeveledList(one, 0xA2732);
					break;

				case schools::restoration:
					AddLeveledList(all, 0xA2990);
					AddLeveledList(one, 0xA2734);
					break;
				}
			}
			break;

		case levels::expert:
		case levels::master:  // add master to expert because they are treated as special by game and don't show up in normal vendors
			{
				AddLeveledList(one, 0x10FCF1);

				switch (school) {
				case schools::alteration:
					AddLeveledList(all, 0xA2982);
					AddLeveledList(one, 0xA272F);
					break;

				case schools::conjuration:
					AddLeveledList(all, 0xA2983);
					AddLeveledList(one, 0xA2736);
					break;

				case schools::destruction:
					AddLeveledList(all, 0xA2984);
					AddLeveledList(one, 0xA2737);
					break;

				case schools::illusion:
					AddLeveledList(all, 0xA2985);
					AddLeveledList(one, 0xA2738);
					break;

				case schools::restoration:
					AddLeveledList(all, 0xA2986);
					AddLeveledList(one, 0xA2739);
					break;
				}
			}
			break;
		}
	}

	void leveled_list_helper::ChangeSpellSchool(RE::SpellItem* spell, RE::TESObjectBOOK* book)
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

	void leveled_list_helper::ActualAdd(RE::TESLeveledList* list, RE::TESObjectBOOK* book)
	{
		if (list != nullptr && book != nullptr) {
			//list->entries.list->addEntry(book, 1, 1, nullptr);

			REL::Relocation<void (*)(RE::TESLeveledList*, int16_t, uint16_t, RE::TESForm*, RE::ContainerItemExtra*)> addEntry{ RELOCATION_ID(14577, 14749).address() };
			addEntry(list, 1, 1, book->As<RE::TESForm>(), nullptr);
		}
	}

	void leveled_list_helper::AddToLeveledList(RE::TESObjectBOOK* spellBook)
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
