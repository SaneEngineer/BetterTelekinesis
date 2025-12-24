#pragma once

#include "BetterTelekinesis/Config.h"
#include "BetterTelekinesis/RaycastHelper.h"
#include "CasualLibrary.hpp"
#include "Stopwatch.hpp"

namespace BetterTelekinesis
{
	static std::mutex lockerPicked;
	static std::shared_mutex cachedHandlesLocker;
	static inline bool lockerLocked = false;
	static std::mutex swordPositionLocker;
	static std::mutex updateLocker;
	static std::recursive_mutex grabindexLocker;
	static std::mutex normalLocker;

	class SwordInstance final
	{
	public:
		RE::RefHandle handle = 0;
		unsigned char waitingEffect = 0;
		bool held = false;
		bool fadingOut = false;
		bool fadedOut = true;
		double fadeTime = 0;
		double createTime = 0;
		double launchTime = 0;
		double heldTime = 0;
		std::vector<float> Goto;
		unsigned char waitEffectCounter = 0;

		[[nodiscard]] bool IsFreeForSummon(double now) const;

		[[nodiscard]] bool IsWaitingEffect(double now) const;

		[[nodiscard]] bool CanPlayFadeout(double now) const;

		[[nodiscard]] bool IsWaitingInvis() const;

		static double GetLifetime();
	};

	class SwordData final
	{
	public:
		std::vector<std::shared_ptr<SwordInstance>> swords = std::vector<std::shared_ptr<SwordInstance>>();
		std::unordered_map<RE::RefHandle, std::shared_ptr<SwordInstance>> lookup = std::unordered_map<RE::RefHandle, std::shared_ptr<SwordInstance>>();

		int nextIndex = 0;

		std::shared_ptr<SwordInstance> forcedGrab;

		void AddSwordFormId(unsigned int formId, const std::string& fileName, bool ghost);

		void AddSwordObj(RE::TESObjectREFR* obj, bool ghost);

		inline static RE::NiPoint3 temp1;
		inline static RE::NiPoint3 temp2;
		inline static RE::NiPoint3 temp3;
		inline static RE::NiPoint3 return1;
		inline static RE::NiPoint3 return2;
	};

	class RandomMoveGenerator final
	{
		float currentX = 0;
		float currentY = 0;
		float targetX = 0;
		float targetY = 0;
		float speedX = 0;
		float speedY = 0;
		unsigned char hasTarget = 0;
		bool disable = false;

		static inline float speedChange = 0.3f;
		static inline float maxSpeed = 1.0f;
		static float GetExtentMult();

	public:
		[[nodiscard]] float GetCurrentX() const;

		[[nodiscard]] float GetCurrentY() const;

		void Update(float diff);

	private:
		void UpdatePos(float diff);

		void UpdateSpeed(float diff);

		void SelectTarget();

		static int GetQuadrant(float x, float y);
	};

	class BetterTelekinesisPlugin final
	{
	public:
		static void InstallHooks()
		{
			Hooks::Install();
		}

		static void OnMainMenuOpen();

		static void Update();

		static void TryDropNow();

		static void Initialize();

	protected:
		struct Hooks
		{
			struct LimitTelekinesisSound1
			{
				static void thunk(uintptr_t a_arg, intptr_t b_arg, uint32_t c_arg, intptr_t d_arg, int e_arg)
				{
					if (Config::TelekinesisMaxObjects > 1 || !Config::TelekinesisGrabObjectSound) {
						// Probably don't need the grab object timer check here since it's spaced out anyway, but..
						if (!Config::TelekinesisGrabObjectSound) {
							return;
						}
						auto now = GetTickCount();
						if (now - _last_tk_sound2 < 100) {
							return;
						}

						_last_tk_sound2 = now;
					}
					func(a_arg, b_arg, c_arg, d_arg, e_arg);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct LimitTelekinesisSound2
			{
				static void thunk(uintptr_t a_arg, intptr_t b_arg, uint32_t c_arg, intptr_t d_arg, int e_arg)
				{
					if (Config::TelekinesisMaxObjects > 1 || !Config::TelekinesisLaunchObjectSound) {
						// Don't play telekinesis launch sound if we just played it, otherwise it ends up being played 10 times and becomes super loud.
						if (!Config::TelekinesisLaunchObjectSound) {
							return;
						}
						auto now = GetTickCount();
						if (now - _last_tk_sound < 200) {
							return;
						}
						_last_tk_sound = now;
					}
					func(a_arg, b_arg, c_arg, d_arg, e_arg);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct FixGrabActorHoldHostility
			{
				static void thunk(intptr_t a_arg, RE::Actor* actor, float* b_arg, float c_arg)
				{
					if (Config::FixGrabActorHoldHostility) {
						auto victim = actor;
						auto plr = RE::PlayerCharacter::GetSingleton();

						if (victim == nullptr || plr == nullptr) {
							return;
						}

						REL::Relocation<int (*)(RE::Actor*)> GetAgression{ RELOCATION_ID(36663, 37671) };
						int aggression = GetAgression(victim);
						auto r8 = 0;                                                                                                  //Memory::ReadPointer(ctx::SI + 0x48); Not Used
						REL::Relocation<void (*)(RE::Actor*, RE::Actor*, uintptr_t, int)> OnAttacked{ RELOCATION_ID(37672, 38626) };  //VR verified
						OnAttacked(victim, dynamic_cast<RE::Actor*>(plr), r8, aggression);
					}
					func(a_arg, actor, b_arg, c_arg);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Telekinesis launch
			struct TelekinesisLaunchSE
			{
				static void thunk(uintptr_t a_arg, RE::TelekinesisEffect** a_effect)
				{
					if (Config::DontLaunchIfRunningOutOfMagicka || Config::LaunchIsHotkeyInstead || Config::ThrowActorDamage > 0.0f) {
						// Always launch sword barrage.
						if (a_effect != nullptr) {
							RE::TelekinesisEffect* effect = *a_effect;
							auto ef = skyrim_cast<RE::ActiveEffect*>(effect);
							if (ef != nullptr && IsOurSpell(ef->GetBaseObject()) == OurSpellTypes::SwordBarrage) {
								func(a_arg, a_effect);
								return;
							}
						}

						if (dropTimer.has_value()) {
							if (GetTickCount() - dropTimer.value() < 200) {
								if (Config::LaunchIsHotkeyInstead) {
									func(a_arg, a_effect);
								}
								return;
							}

							dropTimer.reset();
						}

						if (Config::LaunchIsHotkeyInstead) {
							return;
						}

						if (Config::DontLaunchIfRunningOutOfMagicka) {
							auto plr = RE::PlayerCharacter::GetSingleton();
							if (plr != nullptr && plr->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) <= 0.01f) {
								return;
							}
						}
					}
					func(a_arg, a_effect);
				}

				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct TelekinesisLaunchAE
			{
				static void thunk(RE::TelekinesisEffect** a_effect, uintptr_t a_arg)
				{
					if (Config::DontLaunchIfRunningOutOfMagicka || Config::LaunchIsHotkeyInstead || Config::ThrowActorDamage > 0.0f) {
						// Always launch sword barrage.
						if (a_effect != nullptr) {
							RE::TelekinesisEffect* effect = *a_effect;
							auto ef = skyrim_cast<RE::ActiveEffect*>(effect);
							if (ef != nullptr && IsOurSpell(ef->GetBaseObject()) == OurSpellTypes::SwordBarrage) {
								func(a_effect, a_arg);
								return;
							}
						}

						if (dropTimer.has_value()) {
							if (GetTickCount() - dropTimer.value() < 200) {
								if (Config::LaunchIsHotkeyInstead) {
									func(a_effect, a_arg);
								}
								return;
							}

							dropTimer.reset();
						}

						if (Config::LaunchIsHotkeyInstead) {
							return;
						}

						if (Config::DontLaunchIfRunningOutOfMagicka) {
							auto plr = RE::PlayerCharacter::GetSingleton();
							if (plr != nullptr && plr->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) <= 0.01f) {
								return;
							}
						}
					}
					func(a_effect, a_arg);
				}

				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct GrabActorLaunch
			{
				static void thunk(uintptr_t a_arg, RE::Actor* actor, float* b_arg, float a_float)
				{
					if (Config::DontLaunchIfRunningOutOfMagicka || Config::LaunchIsHotkeyInstead || Config::ThrowActorDamage > 0.0f) {
						if (dropTimer.has_value()) {
							if (GetTickCount() - dropTimer.value() < 200) {
								if (!Config::LaunchIsHotkeyInstead) {
									func(a_arg, actor, b_arg, a_float);
									return;
								}

								OnLaunchActor(actor);

								func(a_arg, actor, b_arg, a_float);
								return;
							}

							dropTimer.reset();
						}

						if (Config::LaunchIsHotkeyInstead) {
							return;
						}

						if (Config::DontLaunchIfRunningOutOfMagicka) {
							auto plr = RE::PlayerCharacter::GetSingleton();
							if (plr != nullptr && plr->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) <= 0.01f) {
								return;
							}
						}

						OnLaunchActor(actor);
					}
					func(a_arg, actor, b_arg, a_float);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct ApplyOverwriteTargetPick
			{
				static void thunk(RE::CrosshairPickData* a_arg, RE::bhkWorld* hkWorld, RE::NiPoint3* sourcePoint, RE::NiPoint3* sourceRotation)
				{
					func(a_arg, hkWorld, sourcePoint, sourceRotation);

					auto origTelekinesis = Memory::Internal::read<RE::ObjectRefHandle>(a_arg->grabPickRef + 0xC).native_handle();

					RE::RefHandle chosenTelekinesis = origTelekinesis;
					{
						std::scoped_lock lock(lockerPicked);
						if (ShouldUpdateTelekinesis(GetTickCount())) {
							telekinesisPicked.clear();
							grabactorPicked.clear();
							uintptr_t bgt = 0;

							if (Config::DebugLogMode) {
								if (profileTimer != nullptr) {
									bgt = profileTimer->elapsed<>();
								}
							}

							OverwriteTelekinesisTargetPick();

							if (Config::DebugLogMode) {
								if (profileTimer != nullptr) {
									totalTelekTime += profileTimer->elapsed<>() - bgt;

									if (timesTelekTime++ % 10 == 1) {
										float telekDistance;
										if (!REL::Module::IsVR()) {
											telekDistance = RE::PlayerCharacter::GetSingleton()->GetPlayerRuntimeData().telekinesisDistance;
										} else {
											telekDistance = RE::PlayerCharacter::GetSingleton()->GetVRPlayerRuntimeData().telekinesisDistance;
										}
										logger::debug(fmt::runtime("profiler: {.2f} <- {} ; {f}"), static_cast<double>(totalTelekTime) / 100 / static_cast<double>(timesTelekTime), timesTelekTime, telekDistance);
									}
								}
							}
						}

						switch (Config::TelekinesisLabelMode) {
						case 0:
							if (chosenTelekinesis != 0 && std::ranges::find(telekinesisPicked.begin(), telekinesisPicked.end(), chosenTelekinesis) == telekinesisPicked.end()) {
								chosenTelekinesis = 0;
							}
							if (chosenTelekinesis != 0 && !HasAnyNormalTelekInHand()) {
								chosenTelekinesis = 0;
							}
							break;

						case 1:
							if (!telekinesisPicked.empty()) {
								chosenTelekinesis = telekinesisPicked[0];

								auto objRef = RE::TESObjectREFR::LookupByHandle(chosenTelekinesis).get();
								if (objRef == nullptr || IsOurItem(objRef->GetBaseObject()) != OurItemTypes::None) {
									chosenTelekinesis = 0;
								}

								if (chosenTelekinesis != 0 && !HasAnyNormalTelekInHand()) {
									chosenTelekinesis = 0;
								}
							} else {
								chosenTelekinesis = 0;
							}
							break;

						case 2:
							chosenTelekinesis = 0;
							break;
						default:
							break;
						}
					}

					if (origTelekinesis != chosenTelekinesis) {
						Memory::Internal::write(reinterpret_cast<uintptr_t>(a_arg) + 0xC, RE::TESObjectREFR::LookupByHandle(chosenTelekinesis).get());
					}
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct ApplyOverwriteTargetPickVR
			{
				static void thunk(RE::CrosshairPickData* a_arg, RE::bhkWorld* hkWorld, RE::NiPoint3* sourcePoint, RE::NiPoint3* sourceRotation, int b_arg)
				{
					func(a_arg, hkWorld, sourcePoint, sourceRotation, b_arg);

					auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;
					auto origTelekinesis = a_arg->grabPickRef[hand].native_handle();

					RE::RefHandle chosenTelekinesis = origTelekinesis;
					{
						std::scoped_lock lock(lockerPicked);
						if (ShouldUpdateTelekinesis(GetTickCount())) {
							telekinesisPicked.clear();
							grabactorPicked.clear();
							uintptr_t bgt = 0;

							if (Config::DebugLogMode) {
								if (profileTimer != nullptr) {
									bgt = profileTimer->elapsed<>();
								}
							}

							OverwriteTelekinesisTargetPick();

							if (Config::DebugLogMode) {
								if (profileTimer != nullptr) {
									totalTelekTime += profileTimer->elapsed<>() - bgt;

									if (timesTelekTime++ % 10 == 1) {
										logger::debug(fmt::runtime("profiler: {.2f} <- {} ; {f}"), static_cast<double>(totalTelekTime) / 100 / static_cast<double>(timesTelekTime), timesTelekTime, RE::PlayerCharacter::GetSingleton()->GetPlayerRuntimeData().telekinesisDistance);
									}
								}
							}
						}

						switch (Config::TelekinesisLabelMode) {
						case 0:
							if (chosenTelekinesis != 0 && std::ranges::find(telekinesisPicked.begin(), telekinesisPicked.end(), chosenTelekinesis) == telekinesisPicked.end()) {
								chosenTelekinesis = 0;
							}
							if (chosenTelekinesis != 0 && !HasAnyNormalTelekInHand()) {
								chosenTelekinesis = 0;
							}
							break;

						case 1:
							if (!telekinesisPicked.empty()) {
								chosenTelekinesis = telekinesisPicked[0];

								auto objRef = RE::TESObjectREFR::LookupByHandle(chosenTelekinesis).get();
								if (objRef == nullptr || IsOurItem(objRef->GetBaseObject()) != OurItemTypes::None) {
									chosenTelekinesis = 0;
								}

								if (chosenTelekinesis != 0 && !HasAnyNormalTelekInHand()) {
									chosenTelekinesis = 0;
								}
							} else {
								chosenTelekinesis = 0;
							}
							break;

						case 2:
							chosenTelekinesis = 0;
							break;
						default:
							break;
						}
					}

					if (origTelekinesis != chosenTelekinesis) {
						auto refr = RE::TESObjectREFR::LookupByHandle(chosenTelekinesis);
						if (refr) {
							a_arg->grabPickRef[hand] = refr->GetHandle();
						}
					}
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct ApplyOverwriteTargetPick2
			{
				static void thunk(RE::TelekinesisEffect* effect)
				{
					auto ef = effect;
					RE::EffectSetting* efs = ef->GetBaseObject();

					bool failBecauseMax = false;
					unsigned int handleId = 0;
					int hadObjCount = 0;
					unsigned int actorHandle = 0;
					{
						std::scoped_lock lock(lockerPicked);
						if (!telekinesisPicked.empty()) {
							std::unordered_set<unsigned int> alreadyChosen;
							bool hasBad = false;
							ForeachHeldHandle([&](const std::shared_ptr<HeldObjectData>& dat) {
								if (hasBad) {
									return;
								}
								if (efs == nullptr || efs != dat->effect) {
									hasBad = true;
								} else {
									if (alreadyChosen.empty()) {
										alreadyChosen = std::unordered_set<unsigned int>();
									}
									alreadyChosen.insert(dat->objectHandleId);
									hadObjCount++;
								}
							});

							if (!hasBad) {
								for (auto x : telekinesisPicked) {
									if (!alreadyChosen.empty() && std::ranges::find(alreadyChosen.begin(), alreadyChosen.end(), x) != alreadyChosen.end()) {
										continue;
									}

									handleId = x;
									break;
								}

								if (handleId == 0 && !alreadyChosen.empty() && alreadyChosen.size() >= Config::TelekinesisMaxObjects) {
									failBecauseMax = true;
								}
							}
						}

						if (!grabactorPicked.empty()) {
							actorHandle = *grabactorPicked.begin();
						}
					}

					if (Config::DebugLogMode) {
						if (handleId == 0) {
							logger::debug("Didn't pick any target");
						} else {
							{
								auto objHandler = RE::TESObjectREFR::LookupByHandle(handleId).get();
								if (objHandler == nullptr) {
									logger::debug("Picked invalid handle");
								} else {
									logger::debug(fmt::runtime("Picked " + std::string(objHandler->GetName())));
								}
							}
						}
					}

					if (handleId != 0) {
						{
							auto objRefHold = RE::TESObjectREFR::LookupByHandle(handleId).get();
							std::vector effects{ efs };
							if (objRefHold == nullptr || !CanPickTelekinesisTarget(objRefHold, effects)) {
								handleId = 0;
							}
						}
					}

					effect->grabbedObject = RE::TESObjectREFR::LookupByHandle(handleId).get();
					ForceUpdateTelekinesis();

					if (!failBecauseMax && castingNormal && Config::TelekinesisDisarmsEnemies) {
						if (actorHandle != 0) {
							{
								auto objRef = RE::TESObjectREFR::LookupByHandle(actorHandle).get();
								if (objRef != nullptr) {
									auto ac = objRef->As<RE::Actor>();
									if (ac != nullptr) {
										DisarmActor(ac);
									}
								}
							}
						}
					}

					if (handleId == 0) {
						OnFailPickTelekinesisTarget(efs, failBecauseMax);
					}

					func(effect);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Player update func, clears grabbed objects in some cases.
			struct PlayerUpdateClear
			{
				static void thunk()
				{
					ClearGrabIndex(true);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Player ::Revert
			struct PlayerRevertClear
			{
				static void thunk()
				{
					ClearGrabIndex(false);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Called from ActivateHandler, probably to drop grabbed objects.
			struct ActivateHandlerClear
			{
				static void thunk(RE::PlayerCharacter* plr)
				{
					RE::PlayerCharacter::GrabbingType grabType;

					if (!REL::Module::IsVR()) {
						grabType = plr->GetPlayerRuntimeData().grabType.get();
					} else {
						auto hand = CastingLeftHandVR() ? RE::VR_DEVICE::kLeftController : RE::VR_DEVICE::kRightController;

						grabType = plr->GetVRPlayerRuntimeData().grabbedObjectData[hand].grabType;
					}
					if (currentGrabindex != 0 || grabType != RE::PlayerCharacter::GrabbingType::kNone) {
						ClearGrabIndex(false);
					}
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Rotate the normal vector based on current index of telekinesised item to separate them out a bit.
			struct SeperateTelekinesis
			{
				static void thunk(int a_arg, RE::NiPoint3& a_origin, RE::NiPoint3& a_direction, bool a_includeCameraOffset)
				{
					auto plr = RE::PlayerCharacter::GetSingleton();

					if (!REL::Module::IsVR()) {
						plr->GetEyeVector(a_origin, a_direction, a_includeCameraOffset);
					} else {
						//Unique? VR function 0x1406E96C0 that is different for each hand
						func(a_arg, a_origin, a_direction, a_includeCameraOffset);
					}

					auto pt = currentGrabindex;
					if (pt == 0) {
						return;
					}

					int indexOfMe = -1;
					float extraX = 0.0f;
					float extraY = 0.0f;

					{
						std::scoped_lock lock(normalLocker);
						if (savedGrabindex.contains(pt)) {
							auto& g = savedGrabindex.at(pt);
							indexOfMe = g->objIndex;
							if (g->rng) {
								extraX = g->rng->GetCurrentX();
								extraY = g->rng->GetCurrentY();
							}
						}
					}

					if (indexOfMe < 0 || indexOfMe >= 100) {
						return;
					}

					int stepX = 0;
					int stepY = 0;
					SelectRotationOffset(indexOfMe, stepX, stepY);

					if (stepX == 0 && stepY == 0 && extraX == 0.0f && extraY == 0.0f) {
						return;
					}

					double stepAmt = Config::TelekinesisObjectSpread;
					double rotX = stepX * stepAmt;
					double rotY = stepY * stepAmt;

					rotX += extraX;
					rotY += extraY;

					//auto positionPtr = rsp + (0x6B8 - 0x670); //rdx
					//auto normalPtr = rsp + (0x6B8 - 0x690); //r8

					//auto position = Memory::Internal::read<RE::NiPoint3>(positionPtr);
					auto& position = a_origin;
					//auto normal = Memory::Internal::read<RE::NiPoint3>(normalPtr);
					auto& normal = a_direction;

					RE::NiPoint3 targetPos;
					targetPos.x = position.x + normal.x;
					targetPos.y = position.y + normal.y;
					targetPos.z = position.z + normal.z;

					RE::NiTransform transform;
					RE::NiPoint3 tpos;
					tpos.x = position.x;
					tpos.y = position.y;
					tpos.z = position.z;
					transform.scale = 1.0f;
					transform.translate = tpos;

					transform.rotate = Util::GlmToNiMatrix3(Util::ViewMatrixToRotationMatrix(mmath::LookAt({ tpos.x, tpos.y, tpos.z }, { targetPos.x, targetPos.y, targetPos.z })));  //transform.LookAt(targetPos);

					transform.rotate = Util::GlmToNiMatrix3(rotate(Util::NiTransformToGlm(transform), static_cast<float>(rotX / 180.0 * M_PI), { 0.0f, 0.0f, 1.0f }));  //transform->rotate->RotateZ(static_cast<float>(rotX / 180.0 * M_PI), transform->rotate);

					auto srotTransform = RE::NiTransform();
					auto srot = rotate(Util::NiTransformToGlm(srotTransform), static_cast<float>(rotY / 180.0 * M_PI), { 1.0f, 0.0f, 0.0f });  //srot->RotateX(static_cast<float>(rotY / 180.0 * M_PI), srot);
					transform.rotate = transform.rotate * Util::GlmToNiMatrix3(Util::ViewMatrixToRotationMatrix(srot));

					targetPos.x = 0.0f;
					targetPos.y = 1.0f;
					targetPos.z = 0.0f;

					tpos = Util::Translate(transform, targetPos);

					normal.x = tpos.x - position.x;
					normal.y = tpos.y - position.y;
					normal.z = tpos.z - position.z;
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct MainUpdate_Nullsub
			{
				static void thunk()
				{
					Update();
					func();
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			static void Install()
			{
				stl::write_thunk_call<LimitTelekinesisSound1>(RELOCATION_ID(34259, 35046).address() + REL::Relocate(0xE1C - 0xDC0, 0x51, 0xAC));
				stl::write_thunk_call<LimitTelekinesisSound2>(RELOCATION_ID(34250, 35052).address() + REL::Relocate(0x4C4 - 0x250, 0x243, 0x360));
				stl::write_thunk_call<FixGrabActorHoldHostility>(RELOCATION_ID(33564, 34333).address() + REL::Relocate(0xC7C - 0xB40, 0x135, 0x13C));
				if (REL::Module::IsAE()) {
					stl::write_thunk_call<TelekinesisLaunchAE>(RELOCATION_ID(34256, 35048).address() + REL::Relocate(0x1C, 0x58));
				} else {
					stl::write_thunk_call<TelekinesisLaunchSE>(RELOCATION_ID(34256, 35048).address() + REL::Relocate(0x1C, 0x58, 0x78));
				}
				stl::write_thunk_call<GrabActorLaunch>(RELOCATION_ID(33559, 34335).address() + REL::Relocate(0x8AD - 0x730, 0x17D, 0x193));

				if (Config::OverwriteTargetPicker) {
					if (!REL::Module::IsVR()) {
						stl::write_thunk_call<ApplyOverwriteTargetPick>(RELOCATION_ID(39534, 40620).address() + REL::Relocate(0x5E4 - 0x3D0, 0x1D5));
					} else {
						stl::write_thunk_call<ApplyOverwriteTargetPickVR>(RELOCATION_ID(39534, 40620).address() + REL::Relocate(0x5E4 - 0x3D0, 0x1D5, 0x44F));
					}
					stl::write_thunk_call<ApplyOverwriteTargetPick2>(RELOCATION_ID(34259, 35046).address() + REL::Relocate(0x19, 0x19, 0x69));
				}

				//Multi-telekinesis
				if (Config::TelekinesisMaxObjects > 1) {
					auto addr = RELOCATION_ID(39375, 40447).address() + REL::Relocate(0xEC86 - 0xE770, 0xA67, 0x522);
					REL::safe_fill(addr, 0x90, REL::Relocate(0xC, 0xC, 0xA));
					// Player update func, clears grabbed objects in some cases.
					stl::write_thunk_call<PlayerUpdateClear>(RELOCATION_ID(39375, 40447).address() + REL::Relocate(0x522, 0xA73, 0x52C));
					stl::write_thunk_call<PlayerRevertClear>(RELOCATION_ID(39466, 40543).address() + REL::Relocate(0x9837 - 0x9620, 0x3C6, 0x385));
					stl::write_thunk_call<ActivateHandlerClear>(RELOCATION_ID(41346, 42420).address() + REL::Relocate(0x1E2, 0x1B0, 0x417));
					if (REL::Module::IsVR()) {
						stl::write_thunk_call<ActivateHandlerClear>(RELOCATION_ID(41346, 42420).address() + REL::Relocate(0x1E2, 0x1B0, 0x497));
						stl::write_thunk_call<SeperateTelekinesis>(RELOCATION_ID(39479, 40556).address() + REL::Relocate(0xC273 - 0xC0F0, 0x176, 0x223));
					} else {
						stl::write_thunk_call<SeperateTelekinesis, 6>(RELOCATION_ID(39479, 40556).address() + REL::Relocate(0xC273 - 0xC0F0, 0x176, 0x223));
					}
				}

				bool marketplace = REL::Relocate(false, REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_6_1130);
				stl::write_thunk_call<MainUpdate_Nullsub>(RELOCATION_ID(35565, 36564).address() + REL::Relocate(0x748, (marketplace ? 0xC2b : 0xC26), 0x7EE));
			}
		};

	private:
		inline static int heldUpdateCounter = 0;

		inline static double time = 0;

	public:
		static inline uintptr_t addr_TeleDamBase = RELOCATION_ID(506190, 376040).address() + 8;
		static inline uintptr_t addr_TeleDamMult = RELOCATION_ID(506186, 376034).address() + 8;
		static inline uintptr_t addr_CanBeTelekinesis = RELOCATION_ID(33822, 34614).address();
		static inline uintptr_t addr_PickDistance = RELOCATION_ID(502526, 370108).address() + 8;

		inline static float reachSpell = 0.0f;

		class HeldObjectData final
		{
		public:
			RE::RefHandle objectHandleId = 0;
			RE::EffectSetting* effect = nullptr;
			bool isActor = false;
			float elapsed = 0;

			int updateCounter = 0;
		};

		static inline std::unordered_map<RE::RefHandle, std::shared_ptr<HeldObjectData>> cachedHeldHandles;

		static void ForeachHeldHandle(const std::function<void(std::shared_ptr<HeldObjectData>)>& func);

	private:
		static float CalculateCurrentTelekinesisDamage(RE::PlayerCharacter* ptrPlr, RE::Actor* actorPtr);

		static void OnLaunchActor(RE::Actor* actorPtr);

		inline static std::optional<uint32_t> dropTimer;

	public:
		inline static Util::CachedFormList* Spells = nullptr;

		inline static Util::CachedFormList* PrimarySpells = nullptr;

		inline static Util::CachedFormList* SecondarySpells = nullptr;

		inline static std::vector<std::string> grabActorNodes;

		inline static std::unordered_set<std::string, Util::case_insensitive_unordered_set::hash> excludeActorNodes;

	private:
		inline static uint64_t lastCheckedLearn = 0;
		inline static uint64_t lastCheckedLearnTwo = 0;

		static void ApplyTelekinesisSettings();

		static inline RE::TESObjectREFR* swordReturnMarker = nullptr;

		static std::vector<RE::ActiveEffect*> GetCurrentRelevantActiveEffects();

		inline static uint32_t lastUpdateTelek = 0;
		inline static bool nextUpdateTelek = false;
		inline static bool lastWeaponOut = false;

		static inline uintptr_t totalTelekTime = 0;
		static inline uintptr_t timesTelekTime = 0;

	public:
		static void ForceUpdateTelekinesis();

	private:
		static bool ShouldUpdateTelekinesis(uint32_t now);

		static void ApplyOverwriteTargetPicker();

		inline static RE::NiPoint3 tempPt1;
		inline static RE::NiPoint3 tempPt2;
		inline static RE::NiPoint3 tempPt3;
		inline static RE::NiPoint3 tempPtBegin;
		inline static RE::NiPoint3 tempPtEnd;

	public:
		enum class SpellTypes : uint8_t
		{
			normal,

			reach,
			enemy,
			single,
			barrage,
			blast,

			max
		};

		class SpellInfo final
		{
		public:
			SpellInfo(SpellTypes t)
			{
				this->type = t;
				this->spellBook = nullptr;
				this->spell = nullptr;
				this->effect = nullptr;
			};

			RE::TESObjectBOOK* spellBook;
			RE::SpellItem* spell;
			RE::EffectSetting* effect;
			SpellTypes type = static_cast<SpellTypes>(0);
			std::unordered_set<RE::FormID> item;

			SpellInfo* Load(const std::string& str, const std::string& setting);

		private:
			void ProduceItem(RE::FormID formId, const std::string& formFile, const std::string& model);
		};

	public:
		inline static auto spellInfos = std::vector<SpellInfo*>(static_cast<int>(SpellTypes::max));

		inline static std::vector<RE::TESEffectShader*> effectInfos;

		//private:
		inline static std::vector<RE::RefHandle> telekinesisPicked;

		inline static std::vector<RE::RefHandle> grabactorPicked;

	private:
		inline static uint32_t lastDebugPick = 0;
		inline static bool debugPick = false;

		inline static std::unique_ptr<stopwatch::Stopwatch> profileTimer;
		inline static auto profileTimes = std::vector<uint64_t>(32);
		inline static uint64_t profileLast = 0;
		inline static uint32_t profileIndex = 0;
		inline static uint64_t profileCounter = 0;
		inline static uint32_t profileReport = 0;

		static void BeginProfile();

		static void StepProfile();

		static void EndProfile();

		static bool IsCellWithinDistance(float myX, float myY, int coordX, int coordY, float maxDist);

		static std::vector<RE::EffectSetting*> CalculateCasting();

		inline static bool castingNormal = false;

		static void DisarmActor(RE::Actor* who);

	public:
		static void OverwriteTelekinesisTargetPick();

		enum class OurItemTypes : uint8_t
		{
			None = 0,

			GhostSword,
			IronSword
		};

		enum class OurSpellTypes : uint8_t
		{
			None,

			SwordBarrage,
			SwordBlast,
			TelekOne,
			TelekNormal,
			TelekReach
		};

	private:
		class TelekObjData final
		{
		public:
			RE::TESObjectREFR* obj;
			float distFromRay = 0;
			float x = 0;
			float y = 0;
			float z = 0;
		};

		class TelekCalcData final
		{
		public:
			glm::vec4 begin;
			glm::vec4 end;
			std::vector<std::unique_ptr<TelekObjData>> chls;
			int findMask = 0;
			float maxDistance = 0;
			std::unordered_set<RE::NiNode*> ignore;
			std::unordered_set<unsigned int> ignoreHandle;
			std::vector<RE::EffectSetting*> casting;
		};

	public:
		static void ProcessOneObj(RE::TESObjectREFR* obj, const std::shared_ptr<TelekCalcData>& data, float quickMaxDist);

		static void FindBestTelekinesis(RE::TESObjectCELL* cell, const std::shared_ptr<TelekCalcData>& data);

		static int GetCurrentTelekinesisObjectCount(int valueIfActorGrabbed = INT32_MAX);

		static void ApplyMultiTelekinesis();

		inline static uint32_t _last_tk_sound = 0;
		inline static uint32_t _last_tk_sound2 = 0;

		class SavedGrabIndex final
		{
		public:
			SavedGrabIndex();

			uintptr_t addr = 0;
			unsigned int handle = 0;
			float dist = 0;
			float wgt = 0;
			RE::PlayerCharacter::GrabbingType grabtype = RE::PlayerCharacter::GrabbingType::kNone;
			int objIndex = 0;
			std::unique_ptr<RandomMoveGenerator> rng;
			//RE::BSTSmallArray<RE::hkRefPtr<RE::bhkMouseSpringAction>, 4> spring = {};
			char spring[0x30] = {};
		};

		inline static auto savedGrabindex = std::unordered_map<uintptr_t, std::shared_ptr<SavedGrabIndex>>();

		inline static bool castingSwordBarrage = false;
		inline static int placementBarrage = 0;

		static int UnsafeFindFreeIndex();

		inline static uintptr_t currentGrabindex = 0;

		static void SwitchToGrabIndex(uintptr_t addr, const std::string& reason, float diff = 0.0f);

		static inline int dontCallClear = 0;

		static void FreeGrabIndex(uintptr_t addr, const std::string& reason);

		static void ClearGrabIndex(bool onlyIfCount);

		static void SelectRotationOffset(int index, int& x, int& y);

		static const std::vector<std::pair<int, int>> _rot_offsets;

		static void ActivateNode(const RE::NiNode* node);

		static void UpdatePointForward(RE::TESObjectREFR* obj);

		static void UpdateHeldObject(RE::TESObjectREFR* obj, const std::shared_ptr<HeldObjectData>& data, const std::vector<RE::ActiveEffect*>& effectList);

		static inline bool hasIntializedSwords = false;

		static void InitSwords();

		// 10e296, fire
		// 10f56b, fire
		// 81180, shadow
		// 8CA2F, light
		// 60db7, cool fire but no membrane
		// 7a296, shadow fast
		// 3fa99, big fire
		static inline unsigned int ghostSwordEffect = 0;
		static inline unsigned int normalSwordEffect = 0;

		static void PlaySwordEffect(RE::TESObjectREFR* obj, bool ghost);

		static void StopSwordEffect(RE::TESObjectREFR* obj, bool ghost);

		static void ReturnSwordToPlace(RE::TESObjectREFR* obj);

		static inline float first_TeleportZOffset = -2000.0f;

		static void UpdateSwordEffects();

		static void TryPlaceSwordNow(bool ghost);

		static bool CalculateSwordPlacePosition(float extraRadiusOfSword, bool forcePlaceInBadPosition, bool ghost);

		static inline auto normalSwords = std::make_shared<SwordData>();
		static inline auto ghostSwords = std::make_shared<SwordData>();

		static int ShouldLaunchObjectNow(RE::ActiveEffect* ef);

		static bool CanPickTelekinesisTarget(RE::TESObjectREFR* obj, const std::vector<RE::EffectSetting*>& casting);

		static void OnFailPickTelekinesisTarget(const RE::EffectSetting* efs, bool failBecauseAlreadyMax);

		static OurSpellTypes IsOurSpell(const RE::EffectSetting* ef);

		static OurItemTypes IsOurItem(const RE::TESForm* baseForm);

		static bool HasAnyNormalTelekInHand();

		static bool CastingLeftHandVR();
	};

	class find_nearest_node_helper final
	{
		static inline bool inited = false;

		inline static RE::NiPoint3 begin;
		inline static RE::NiPoint3 end;
		inline static RE::NiPoint3 temp1;
		inline static RE::NiPoint3 temp2;
		inline static RE::NiPoint3 temp3;
		inline static RE::NiPoint3 temp4;

	public:
		static void Init();

		static RE::NiNode* FindBestNodeInCrosshair(RE::NiNode* root);

	private:
		class temp_calc final
		{
		public:
			RE::NiNode* best = nullptr;
			float dist = 0;
		};

		static void ExploreCalc(const RE::NiNode* current, temp_calc* state);

		static float GetDistance(const RE::NiNode* n);
	};

	class LeveledListHelper final
	{
	public:
		enum class schools : uint8_t
		{
			alteration,
			conjuration,
			destruction,
			illusion,
			restoration
		};

		enum class levels : uint8_t
		{
			novice,
			apprentice,
			adept,
			expert,
			master
		};

	private:
		static void AddLeveledList(std::vector<RE::TESLeveledList*>& ls, unsigned int id);

		static void FindLeveledLists(schools school, levels level, std::vector<RE::TESLeveledList*>& all, std::vector<RE::TESLeveledList*>& one);

		static void ChangeSpellSchool(RE::SpellItem* spell, RE::TESObjectBOOK* book);

		static void ActualAdd(RE::TESLeveledList* list, RE::TESObjectBOOK* book);

	public:
		static void AddToLeveledList(RE::TESObjectBOOK* spellBook);
	};
}
