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

	class FindNearestNodeHelper final
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
		class tempCalc final
		{
		public:
			RE::NiNode* best = nullptr;
			float dist = 0;
		};

		static void ExploreCalc(const RE::NiNode* current, std::shared_ptr<tempCalc> state);

		static float GetDistance(const RE::NiNode* n);
	};

	class BetterTelekinesisPlugin final
	{
	public:
		static void OnMainMenuOpen();

		static void Update();

		static void TryDropNow();

		static void Initialize();

	private:
		inline static int heldUpdateCounter = 0;

		inline static double time = 0;

	public:
		static inline uintptr_t addr_TeleDamBase = RELOCATION_ID(506190, 376040).address() + 8;
		static inline uintptr_t addr_TeleDamMult = RELOCATION_ID(506186, 376034).address() + 8;
		static inline uintptr_t addr_CanBeTelekinesis = RELOCATION_ID(33822, 34614).address();
		static inline uintptr_t addr_PickDistance = RELOCATION_ID(502526, 370108).address() + 8;

		inline static float reachSpell = 0.0f;

		inline static bool actorDamageToggled = false;
		inline static double origActorDamage = 0.0f;

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

		static void UpdateAutoLearnSpells();

		static float CalculateCurrentTelekinesisDamage(RE::PlayerCharacter* ptrPlr, RE::Actor* actorPtr);

		static void OnLaunchActor(RE::Actor* actorPtr);

		inline static std::optional<uint32_t> dropTimer;

	public:
		inline static Util::CachedFormList* Spells = nullptr;

		inline static Util::CachedFormList* PrimarySpells = nullptr;

		inline static Util::CachedFormList* SecondarySpells = nullptr;

		inline static std::vector<std::string> grabActorNodes;

		inline static std::unordered_set<std::string, Util::case_insensitive_unordered_set::hash> excludeActorNodes;

		inline static uint64_t lastCheckedLearn = 0;

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

		static bool ShouldUpdateTelekinesis(uint32_t now);

		static void ApplyOverwriteTargetPicker();

	 private:
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

		inline static RE::BGSPerk* disarmPerk = nullptr;

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
