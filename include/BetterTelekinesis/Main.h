#pragma once

#include "BetterTelekinesis/Config.h"
#include "BetterTelekinesis/Logging.h"
#include "BetterTelekinesis/RaycastHelper.h"
#include "CasualLibrary.hpp"
#include "bankersrounding.h"

#include "Shared/Utility/Assembly.h"
#include "Shared/Utility/Memory.h"
#include "Stopwatch.hpp"

namespace BetterTelekinesis
{
	class sword_instance final
	{
	public:
		RE::RefHandle Handle = 0;
		unsigned char WaitingEffect = 0;
		bool Held = false;
		bool FadingOut = false;
		bool FadedOut = true;
		double FadeTime = 0;
		double CreateTime = 0;
		double LaunchTime = 0;
		double HeldTime = 0;
		std::vector<float> Goto;
		unsigned char WaitEffectCounter = 0;

		bool IsFreeForSummon(double now) const;

		bool IsWaitingEffect(double now) const;

		bool CanPlayFadeout(double now) const;

		bool IsWaitingInvis() const;

		static double getLifetime();
	};

	class sword_data final
	{
	public:
		std::vector<std::shared_ptr<sword_instance>> swords = std::vector<std::shared_ptr<sword_instance>>();
		std::unordered_map<RE::RefHandle, std::shared_ptr<sword_instance>> lookup = std::unordered_map<RE::RefHandle, std::shared_ptr<sword_instance>>();

		int next_index = 0;

		std::shared_ptr<sword_instance> forced_grab;

		void AddSword_FormId(unsigned int formId, const std::string& fileName, bool ghost);

		void AddSword_Obj(RE::TESObjectREFR* obj, bool ghost);

		static RE::NiPoint3 Temp1;
		static RE::NiPoint3 Temp2;
		static RE::NiPoint3 Temp3;
		static RE::NiPoint3 Return1;
		static RE::NiPoint3 Return2;
	};

	class random_move_generator final
	{
	private:
		float current_x = 0;
		float current_y = 0;
		float target_x = 0;
		float target_y = 0;
		float speed_x = 0;
		float speed_y = 0;
		unsigned char has_target = 0;
		bool disable = false;

		static float speed_change;
		static float max_speed;
		static float getExtentMult();

	public:
		float getCurrentX() const;

		float getCurrentY() const;

		void update(float diff);

	private:
		void update_pos(float diff);

		void update_speed(float diff);

		void select_target();

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

		static void _try_drop_now();

		static void Initialize();

		static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* self, RE::TESForm* a_form, std::uint32_t count, bool forcePersist, bool initiallyDisabled)
		{
			using func_t = RE::TESObjectREFR*(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::TESObjectREFR*, RE::TESForm*, std::uint32_t, bool, bool);
			RE::VMStackID frame = 0;

			REL::Relocation<func_t> func{ RELOCATION_ID(55672, 56203) };
			auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();

			return func(vm, frame, self, a_form, count, forcePersist, initiallyDisabled);
		}

	protected:
		struct Hooks
		{
			struct LimitTelekinesisSound1
			{
				static void thunk(RE::Actor* actor, intptr_t a_arg, uint32_t b_arg, intptr_t c_arg, int d_arg)
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
					func(actor, a_arg, b_arg, c_arg, d_arg);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct LimitTelekinesisSound2
			{
				static void thunk(RE::Actor* actor, intptr_t a_arg, uint32_t b_arg, intptr_t c_arg, int d_arg)
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
					func(actor, a_arg, b_arg, c_arg, d_arg);
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

						REL::Relocation<int (*)(RE::Actor*)> GetAgression{ RELOCATION_ID(36663, 0) };
						int aggression = GetAgression(victim);
						auto r8 = 0;  //Memory::ReadPointer(ctx::SI + 0x48); Not Used
						REL::Relocation<void (*)(RE::Actor*, RE::Actor*, uintptr_t, int)> OnAttacked{ RELOCATION_ID(37672, 38626) };
						OnAttacked(victim, dynamic_cast<RE::Actor*>(plr), r8, aggression);
					}
					func(a_arg, actor, b_arg, c_arg);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Telekinesis launch
			struct TelekinesisLaunch
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

						if (drop_timer.has_value()) {
							int now = GetTickCount();
							if (now - drop_timer.value() < 200) {
								if (Config::LaunchIsHotkeyInstead) {
									func(a_arg, a_effect);
								}
								return;
							}

							drop_timer.reset();
						}

						if (Config::LaunchIsHotkeyInstead) {
							return;
						}

						if (Config::DontLaunchIfRunningOutOfMagicka) {
							auto plr = RE::PlayerCharacter::GetSingleton();
							if (plr != nullptr && plr->GetActorValue(RE::ActorValue::kMagicka) <= 0.01f) {
								return;
							}
						}
					}
					func(a_arg, a_effect);
				}

				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct GrabActorLaunch
			{
				static void thunk(uintptr_t a_arg, RE::Actor* actor, uintptr_t b_arg, uintptr_t c_arg, float a_float)
				{
					if (Config::DontLaunchIfRunningOutOfMagicka || Config::LaunchIsHotkeyInstead || Config::ThrowActorDamage > 0.0f) {
						if (drop_timer.has_value()) {
							int now = GetTickCount();
							if (now - drop_timer.value() < 200) {
								if (!Config::LaunchIsHotkeyInstead) {
									func(a_arg, actor, b_arg, c_arg, a_float);
									return;
								}

								OnLaunchActor(actor);

								func(a_arg, actor, b_arg, c_arg, a_float);
								return;
							}

							drop_timer.reset();
						}

						if (Config::LaunchIsHotkeyInstead) {
							return;
						}

						if (Config::DontLaunchIfRunningOutOfMagicka) {
							auto plr = RE::PlayerCharacter::GetSingleton();
							if (plr != nullptr && plr->GetActorValue(RE::ActorValue::kMagicka) <= 0.01f) {
								return;
							}
						}

						OnLaunchActor(actor);
					}
					func(a_arg, actor, b_arg, c_arg, a_float);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct ApplyOverwriteTargetPick
			{
				static void thunk(RE::CrosshairPickData* a_arg, RE::bhkWorld* hkWorld, RE::NiPoint3* sourcePoint, RE::NiPoint3* sourceRotation)
				{
					func(a_arg, hkWorld, sourcePoint, sourceRotation);

					RE::RefHandle origTelekinesis = a_arg->grabPickRef.native_handle();
					RE::RefHandle chosenTelekinesis = origTelekinesis;
					{
						std::scoped_lock lock(locker_picked);
						if (ShouldUpdateTelekinesis(GetTickCount())) {
							telekinesis_picked.clear();
							grabactor_picked.clear();
							uintptr_t bgt = 0;

							if (Config::DebugLogMode) {
								if (_profile_timer != nullptr) {
									bgt = _profile_timer->elapsed<>();
								}
							}

							OverwriteTelekinesisTargetPick();

							if (Config::DebugLogMode) {
								if (_profile_timer != nullptr) {
									_total_telek_time += _profile_timer->elapsed<>() - bgt;

									if (_times_telek_time++ % 10 == 1) {
										logger::debug(fmt::runtime("profiler: {.2f} <- {} ; {f}"), static_cast<double>(_total_telek_time / 1000) / static_cast<double>(_times_telek_time), _times_telek_time, RE::PlayerCharacter::GetSingleton()->telekinesisDistance);
									}
								}
							}
						}

						switch (Config::TelekinesisLabelMode) {
						case 0:
							if (chosenTelekinesis != 0 && std::ranges::find(telekinesis_picked.begin(), telekinesis_picked.end(), chosenTelekinesis) == telekinesis_picked.end()) {
								chosenTelekinesis = 0;
							}
							if (chosenTelekinesis != 0 && !HasAnyNormalTelekInHand()) {
								chosenTelekinesis = 0;
							}
							break;

						case 1:
							if (!telekinesis_picked.empty()) {
								chosenTelekinesis = telekinesis_picked[0];

								auto objRef = RE::TESObjectREFR::LookupByHandle(chosenTelekinesis);
								if (!objRef->IsHandleValid() || IsOurItem(objRef->GetBaseObject()) != OurItemTypes::None) {
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
						}
					}

					if (origTelekinesis != chosenTelekinesis) {
						a_arg->grabPickRef = RE::TESObjectREFR::LookupByHandle(chosenTelekinesis).get();
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
						std::scoped_lock lock(locker_picked);
						if (!telekinesis_picked.empty()) {
							std::unordered_set<unsigned int> alreadyChosen;
							bool hasBad = false;
							ForeachHeldHandle([&](const std::shared_ptr<held_obj_data>& dat) {
								if (hasBad) {
									return;
								}
								if (efs == nullptr || efs != dat->Effect) {
									hasBad = true;
								} else {
									if (alreadyChosen.empty()) {
										alreadyChosen = std::unordered_set<unsigned int>();
									}
									alreadyChosen.insert(dat->ObjectHandleId);
									hadObjCount++;
								}
							});

							if (!hasBad) {
								for (auto x : telekinesis_picked) {
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

						if (!grabactor_picked.empty()) {
							actorHandle = *grabactor_picked.begin();
						}
					}

					if (Config::DebugLogMode) {
						if (handleId == 0) {
							logger::debug("Didn't pick any target");
						} else {
							{
								auto objHandler = RE::TESObjectREFR::LookupByHandle(handleId);
								if (!objHandler->IsHandleValid()) {
									logger::debug("Picked invalid handle");
								} else {
									logger::debug(fmt::runtime("Picked " + std::string(objHandler->GetName())));
								}
							}
						}
					}

					if (handleId != 0) {
						{
							auto objRefHold = RE::TESObjectREFR::LookupByHandle(handleId);
							std::vector effects{ efs };
							if (!objRefHold->IsHandleValid() || !CanPickTelekinesisTarget(objRefHold.get(), effects)) {
								handleId = 0;
							}
						}

					}

					effect->grabbedObject = RE::TESObjectREFR::LookupByHandle(handleId).get();
					ForceUpdateTelekinesis();

					if (!failBecauseMax && casting_normal && Config::TelekinesisDisarmsEnemies) {
						if (actorHandle != 0) {
							{
								auto objRef = RE::TESObjectREFR::LookupByHandle(actorHandle);
								if (objRef->IsHandleValid()) {
									auto ac = dynamic_cast<RE::Actor*>(objRef.get());
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
				static void thunk(RE::PlayerCharacter* plr)
				{
					clear_grabindex(true);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Player ::Revert
			struct PlayerRevertClear
			{
				static void thunk(RE::PlayerCharacter* plr)
				{
					clear_grabindex(false);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Called from ActivateHandler, probably to drop grabbed objects.
			struct ActivateHandlerClear
			{
				static void thunk(const RE::PlayerCharacter* plr)
				{
					// plr->grabType = plr + 0xAF4
					if (current_grabindex != 0 || plr->grabType.underlying() != 0) {
						clear_grabindex(false);
					}
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Rotate the normal vector based on current index of telekinesised item to separate them out a bit.
			struct SeperateTelekinesis
			{
				static void thunk(RE::PlayerCharacter* plr, RE::NiPoint3& a_origin, RE::NiPoint3& a_direction, bool a_includeCameraOffset)
				{
					//func(plr, a_origin, a_direction, a_includeCameraOffset);
					plr->GetEyeVector(a_origin, a_direction, a_includeCameraOffset);

					auto pt = current_grabindex;
					if (pt == 0) {
						return;
					}

					int indexOfMe = -1;
					size_t hadCount = 0;
					float extraX = 0.0f;
					float extraY = 0.0f;

					{
						std::scoped_lock lock(normal_locker);
						if (saved_grabindex.contains(pt)) {
							auto& g = saved_grabindex.at(pt);
							indexOfMe = g->index_of_obj;
							if (!g->rng.has_value()) {
								extraX = g->rng->getCurrentX();
								extraY = g->rng->getCurrentY();
							}
						}
						hadCount = saved_grabindex.size();
					}

					if (indexOfMe < 0 || indexOfMe >= 100) {
						return;
					}

					int stepX = 0;
					int stepY = 0;
					_select_rotation_offset(indexOfMe, stepX, stepY);

					if (stepX == 0 && stepY == 0 && extraX == 0.0f && extraY == 0.0f) {
						return;
					}

					// Formula method isn't good because it's too jarringly noticable when it changes.
					/*if (hadCount < 2)
				    hadCount = 1;
				else
				    hadCount--;
	
				double stepAmt = 5.0 + Math.Max(-3.0, (1.0 - hadCount / 10.0) * 10.0);*/

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

					auto targetPos = RE::NiPoint3();
					targetPos.x = position.x + normal.x;
					targetPos.y = position.y + normal.y;
					targetPos.z = position.z + normal.z;

					auto transform = RE::NiTransform();
					auto tpos = RE::NiPoint3();
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
				stl::write_thunk_call<LimitTelekinesisSound1>(RELOCATION_ID(34259, 0).address() + OFFSET(0xE1C - 0xDC0, 0));
				stl::write_thunk_call<LimitTelekinesisSound2>(RELOCATION_ID(34250, 0).address() + OFFSET(0x4C4 - 0x250, 0));

				stl::write_thunk_call<FixGrabActorHoldHostility>(RELOCATION_ID(33564, 0).address() + OFFSET(0xC7C - 0xB40, 0));

				stl::write_thunk_call<TelekinesisLaunch>(RELOCATION_ID(34256, 0).address() + OFFSET(0x1C, 0));

				stl::write_thunk_call<GrabActorLaunch>(RELOCATION_ID(33559, 0).address() + OFFSET(0x8AD - 0x730, 0));

				if (Config::OverwriteTargetPicker) {
					stl::write_thunk_call<ApplyOverwriteTargetPick>(RELOCATION_ID(39534, 0).address() + OFFSET(0x5E4 - 0x3D0, 0));
					stl::write_thunk_call<ApplyOverwriteTargetPick2>(RELOCATION_ID(34259, 0).address() + OFFSET(0x19, 0));
				}

				// Player update func, clears grabbed objects in some cases.
				//Multi-telekinesis
				auto addr = RELOCATION_ID(39375, 0).address() + OFFSET(0xEC86 - 0xE770, 0);
				REL::safe_fill(addr, 0x90, 0xC);

				stl::write_thunk_call<PlayerUpdateClear>(RELOCATION_ID(39375, 0).address() + OFFSET(0x522, 0));
				stl::write_thunk_call<PlayerRevertClear>(RELOCATION_ID(39466, 0).address() + OFFSET(0x9837 - 0x9620, 0));
				stl::write_thunk_call<ActivateHandlerClear>(RELOCATION_ID(41346, 0).address() + OFFSET(0x1E2, 0));

				stl::write_thunk_call<SeperateTelekinesis, 6>(RELOCATION_ID(39479, 0).address() + OFFSET(0xC273 - 0xC0F0, 0));

				bool marketplace = OFFSET(false, REL::Module::get().version() >= SKSE::RUNTIME_1_6_1130);
				stl::write_thunk_call<MainUpdate_Nullsub>(RELOCATION_ID(35565, 36564).address() + OFFSET_3(0x748, (marketplace ? 0xC2b : 0xC26), 0x7EE));
			}
		};

	private:
		inline static int HeldUpdateCounter = 0;

		inline static double Time = 0;

		//private static int _dbg_counter = 0;
	public:
		static uintptr_t addr_TeleDamBase;
		static uintptr_t addr_TeleDamMult;
		static uintptr_t addr_CanBeTelekinesis;
		static uintptr_t addr_PickDistance;

	public:
		inline static float _reach_spell = 0.0f;

		class held_obj_data final
		{
		public:
			RE::RefHandle ObjectHandleId = 0;
			RE::EffectSetting* Effect;
			bool IsActor = false;
			float Elapsed = 0;

			int __update_counter = 0;
		};

	public:
		static std::unordered_map<RE::RefHandle, std::shared_ptr<held_obj_data>> CachedHeldHandles;

		static std::mutex CachedHandlesLocker;

		static void ForeachHeldHandle(const std::function<void(std::shared_ptr<held_obj_data>)>& func);

	private:
		static float CalculateCurrentTelekinesisDamage(RE::PlayerCharacter* ptrPlr, RE::Actor* actorPtr);

		static void OnLaunchActor(RE::Actor* actorPtr);

		static void write_float(unsigned long long vid, float value);

		static void write_float_mult(unsigned long long vid, float value);

		static std::optional<uint32_t> drop_timer;

	public:
		static Util::CachedFormList* Spells;

		static Util::CachedFormList* PrimarySpells;

		static Util::CachedFormList* SecondarySpells;

		static std::vector<std::string> grabActorNodes;

	public:
		static std::unordered_set<std::string, Util::case_insensitive_unordered_set::hash> ExcludeActorNodes;

	private:
		static ULONGLONG _last_check_learn;
		static ULONGLONG _last_check_learn2;

		static bool find_collision_node(RE::NiNode* root, int depth = 0);

		static void apply_good_stuff();

		static RE::TESObjectREFR* sword_ReturnMarker;

		static std::vector<RE::ActiveEffect*> GetCurrentRelevantActiveEffects();

		static uint32_t _last_updated_telek;
		static bool _next_update_telek;
		static bool _last_weap_out;

		static uintptr_t _total_telek_time;
		static uintptr_t _times_telek_time;

	public:
		static void ForceUpdateTelekinesis();

	private:
		static bool ShouldUpdateTelekinesis(uint32_t now);

		static void apply_overwrite_target_pick();

		static RE::NiPoint3 TempPt1;
		static RE::NiPoint3 TempPt2;
		static RE::NiPoint3 TempPt3;
		static RE::NiPoint3 TempPtBegin;
		static RE::NiPoint3 TempPtEnd;

	public:
		enum class spell_types : uint8_t
		{
			normal,

			reach,
			enemy,
			single,
			barrage,
			blast,

			max
		};

		class spell_info final
		{
		public:
			spell_info(spell_types t)
			{
				this->type = t;
				this->SpellBook = nullptr;
				this->Spell = nullptr;
				this->Effect = nullptr;
			};

			RE::TESObjectBOOK* SpellBook;
			RE::SpellItem* Spell;
			RE::EffectSetting* Effect;
			spell_types type = static_cast<spell_types>(0);
			std::unordered_set<RE::FormID> Item;

			spell_info* Load(const std::string& str, const std::string& setting);

		private:
			void ProduceItem(RE::FormID formId, const std::string& formFile, const std::string& model);
		};

	public:
		inline static std::vector<spell_info*> spellInfos = std::vector<spell_info*>((int)spell_types::max);

		static std::vector<RE::TESEffectShader*> EffectInfos;

		//private:
		inline static std::vector<RE::RefHandle> telekinesis_picked;

		inline static std::vector<RE::RefHandle> grabactor_picked;
		static std::mutex locker_picked;

	private:
		inline static int debug_msg = false;
		inline static uint32_t last_debug_pick = 0;
		inline static bool debug_pick = false;

		inline static std::unique_ptr<stopwatch::Stopwatch> _profile_timer;
		inline static std::vector<uint64_t> _profile_times = std::vector<uint64_t>(32);
		inline static uint64_t _profile_last = 0;
		inline static uint32_t _profile_index = 0;
		inline static uint64_t _profile_counter = 0;
		inline static uint32_t _profile_report = 0;

		static void begin_profile();

		static void step_profile();

		static void end_profile();

		static bool is_cell_within_dist(float myX, float myY, int coordX, int coordY, float maxDist);

		static std::vector<RE::EffectSetting*> CalculateCasting();

		inline static bool casting_normal = false;

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
		class telek_obj_data final
		{
		public:
			RE::TESObjectREFR* obj;
			float distFromRay = 0;
			float x = 0;
			float y = 0;
			float z = 0;
		};

		class telek_calc_data final
		{
		public:
			glm::vec4 begin;
			glm::vec4 end;
			std::vector<std::unique_ptr<telek_obj_data>> chls;
			int findMask = 0;
			float maxDistance = 0;
			std::unordered_set<RE::NiNode*> ignore;
			std::unordered_set<unsigned int> ignore_handle;
			std::vector<RE::EffectSetting*> casting;
		};

	public:
		static void process_one_obj(RE::TESObjectREFR* obj, const std::shared_ptr<telek_calc_data>& data, float quickMaxDist);

		static void find_best_telekinesis(RE::TESObjectCELL* cell, const std::shared_ptr<telek_calc_data>& data);

		static int GetCurrentTelekinesisObjectCount(int valueIfActorGrabbed = INT32_MAX);

		static void apply_multi_telekinesis();

		static uint32_t _last_tk_sound;
		static uint32_t _last_tk_sound2;

		class saved_grab_index final
		{
		public:
			saved_grab_index();

			uintptr_t addr;
			unsigned int handle = 0;
			float dist = 0;
			float wgt = 0;
			uint32_t grabtype = 0;
			int index_of_obj = 0;
			std::optional<random_move_generator> rng;

			char spring[0x30];
			char spring_alloc[0x30];
		};

	public:
		static std::recursive_mutex grabindex_locker;

		static std::mutex normal_locker;

		inline static std::unordered_map<uintptr_t, std::shared_ptr<saved_grab_index>> saved_grabindex = std::unordered_map<uintptr_t, std::shared_ptr<saved_grab_index>>();

		inline static bool casting_sword_barrage = false;
		static int _placement_barrage;

		static int unsafe_find_free_index();

		static uintptr_t current_grabindex;

		static void switch_to_grabindex(uintptr_t addr, float diff = 0.0f);

		static int _dont_call_clear;

		static void free_grabindex(uintptr_t addr);

		static void clear_grabindex(bool onlyIfCount);

		static void _select_rotation_offset(int index, int& x, int& y);

		static const std::vector<std::pair<int, int>> _rot_offsets;

		static float rotate_speed(float diff);

		static float adjust_diff(float current, float target);

		static void activate_node(const RE::NiNode* node);

		static void update_point_forward(RE::NiNode* node);

		static void update_held_object(RE::TESObjectREFR* obj, const std::shared_ptr<held_obj_data>& data, const std::vector<RE::ActiveEffect*>& effectList);

		static bool _has_init_sword;

		static void InitSwords();

		// 10e296, fire
		// 10f56b, fire
		// 81180, shadow
		// 8CA2F, light
		// 60db7, cool fire but no membrane
		// 7a296, shadow fast
		// 3fa99, big fire
		static unsigned int ghost_sword_effect;
		static unsigned int normal_sword_effect;

		static void PlaySwordEffect(RE::TESObjectREFR* obj, bool ghost);

		static void StopSwordEffect(RE::TESObjectREFR* obj, bool ghost);

	public:
		static void ReturnSwordToPlace(RE::TESObjectREFR* obj);

		static float first_TeleportZOffset;
		//private static float first_TeleportZOffset = 0.0f;

		static void UpdateSwordEffects();

		static void TryPlaceSwordNow(bool ghost);

		static bool CalculateSwordPlacePosition(float extraRadiusOfSword, bool forcePlaceInBadPosition, bool ghost);

		static sword_data* const normal_swords;
		static sword_data* const ghost_swords;

		static int ShouldLaunchObjectNow(RE::ActiveEffect* ef);

		static bool CanPickTelekinesisTarget(RE::TESObjectREFR* obj, const std::vector<RE::EffectSetting*>& casting);

		static void OnFailPickTelekinesisTarget(RE::EffectSetting* efs, bool failBecauseAlreadyMax);

		static OurSpellTypes IsOurSpell(RE::EffectSetting* ef);

		static OurItemTypes IsOurItem(const RE::TESForm* baseForm);

		static bool HasAnyNormalTelekInHand();
	};

	class find_nearest_node_helper final
	{
	private:
		static bool inited;

		static RE::NiPoint3 Begin;
		static RE::NiPoint3 End;
		static RE::NiPoint3 Temp1;
		static RE::NiPoint3 Temp2;
		static RE::NiPoint3 Temp3;
		static RE::NiPoint3 Temp4;

	public:
		static void init();

		static RE::NiNode* FindBestNodeInCrosshair(RE::NiNode* root);

	private:
		class temp_calc final
		{
		public:
			RE::NiNode* best = nullptr;
			float dist = 0;
		};

		static void explore_calc(const RE::NiNode* current, temp_calc* state);

		static float GetDistance(const RE::NiNode* n);
	};

	class leveled_list_helper final
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
