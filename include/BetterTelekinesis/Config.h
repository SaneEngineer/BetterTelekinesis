#pragma once

#include "BetterTelekinesis/Util.h"
#include <SKSE/SKSE.h>

namespace BetterTelekinesis
{
	class Config
	{
	public:
		static std::string getDefaultResponsiveHoldParameters();

		static void ReadSettings();
		static void WriteSettings();

		static inline double OverwriteTelekinesisSpellBaseCost = -1.0;
		static inline double BaseDistanceMultiplier = 2.0;
		static inline double BaseDamageMultiplier = 5.0;
		static inline double ObjectPullSpeedBase = 200.0;
		static inline double ObjectPullSpeedAccel = 10000.0;
		static inline double ObjectPullSpeedMax = 8000.0;
		static inline double ObjectThrowForce = 4.0f;
		static inline double ObjectHoldDistance = 1.0;
		static inline bool ResponsiveHold = true;
		static inline std::string ResponsiveHoldParams = getDefaultResponsiveHoldParameters();
		static inline bool DontDeactivateHavokHoldSpring = true;

		static inline bool AbortTelekinesisHotkeyEnabled = false;
		static inline int AbortTelekinesisHotkey = 42;
		static inline bool LaunchIsHotkeyInstead = false;
		static inline bool TelekinesisGrabObjectSound = true;
		static inline bool TelekinesisLaunchObjectSound = true;
		static inline bool TelekinesisDisarmsEnemies = false;
		static inline bool AutoLearnTelekinesisSpell = false;
		static inline bool AutoLearnTelekinesisVariants = true;
		static inline bool AlwaysLaunchObjectsEvenWhenNotFinishedPulling = false;
		static inline bool DontLaunchIfRunningOutOfMagicka = true;
		static inline bool PointWeaponsAndProjectilesForward = true;

		static inline bool OverwriteTargetPicker = true;
		static inline double ObjectTargetPickerRange = 500.0;
		static inline double ActorTargetPickerRange = 200.0;
		static inline int DontPickFriendlyTargets = 1;
		static inline double TelekinesisTargetUpdateInterval = 0.2;
		static inline bool TelekinesisTargetOnlyUpdateIfWeaponOut = true;
		static inline int TelekinesisLabelMode = 1;

		static inline int TelekinesisMaxObjects = 10;
		static inline double TelekinesisObjectSpread = 15.0;
		static inline double MultiObjectHoverAmount = 4.0;

		static inline double ActorPullSpeed = 8000.0;
		static inline double ActorThrowForce = 2.0;
		static inline double ActorHoldDistance = 1.5;
		static inline double ThrowActorDamage = 0.0;
		static inline double HoldActorDamage = 0.0;
		static inline bool ActorDamageToggleEnabled = false;
		static inline int ToggleActorDamageHotkey = 58;
		
		static inline bool GrabActorNodeNearest = true;
		static inline std::string GrabActorNodeExclude = "";
		static inline std::string GrabActorNodePriority = "NPC Spine2 [Spn2]";

		static inline bool AddSwordSpellsToLeveledLists = true;
		static inline bool MakeSwordSpellsAlterationInstead = false;
		static inline double MagicSword_RemoveDelay = 6.0;
		static inline double MagicSwordBlast_PlaceDistance = 300.0;
		static inline double MagicSwordBarrage_PlaceDistance = 200.0;
		static inline double SwordBarrage_FireDelay = 0.5;
		static inline double SwordBarrage_SpawnDelay = 0.15;
		static inline std::string SwordReturn_Marker = "80B:BetterTelekinesis.esp";
		static inline std::string Barrage_SwordModel = R"(Weapons\Iron\LongSword.nif;)"s + R"(Weapons\Iron\LongSword.nif;)" + R"(Weapons\Iron\LongSword.nif;)" + R"(Weapons\Iron\LongSword.nif;)" + R"(Weapons\Akaviri\BladesSword.nif;)" + R"(Weapons\Akaviri\BladesSword.nif;)" + R"(Clutter\DummyItems\DummySword01.nif;)" + R"(Weapons\Daedric\DaedricSword.nif;)" + R"(DLC01\Weapons\Dragonbone\Sword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Ebony\EbonySword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Falmer\FalmerLongSword.nif;)" + R"(Weapons\Falmer\FalmerLongSword.nif;)" + R"(Weapons\Falmer\FalmerLongSword.nif;)" + R"(Weapons\Forsworn\ForswornSword.nif;)" + R"(Weapons\Forsworn\ForswornSword.nif;)" + R"(Weapons\Forsworn\ForswornSword.nif;)" + R"(Weapons\Glass\GlassSword.nif;)" + R"(Weapons\Glass\GlassSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\NordHero\NordHeroSword.nif;)" + R"(Weapons\NordHero\NordHeroSword.nif;)" + R"(Weapons\NordHero\NordHeroSword.nif;)" + R"(Weapons\Scimitar\Scimitar.nif;)" + R"(Weapons\Scimitar\Scimitar.nif;)" + R"(Weapons\Silver\SilverSword.nif;)" + R"(Weapons\Silver\SilverSword.nif;)" + R"(Weapons\Silver\SilverSword.nif;)" + R"(Weapons\Steel\SteelSword.nif;)" + R"(Weapons\Steel\SteelSword.nif;)" + R"(Weapons\Steel\SteelSword.nif;)" + R"(Weapons\Steel\SteelSword.nif)";
		static inline std::string Blast_SwordModel = R"(Weapons\Iron\LongSword.nif;)"s + R"(Weapons\Iron\LongSword.nif;)" + R"(Weapons\Iron\LongSword.nif;)" + R"(Weapons\Iron\LongSword.nif;)" + R"(Weapons\Akaviri\BladesSword.nif;)" + R"(Weapons\Akaviri\BladesSword.nif;)" + R"(Clutter\DummyItems\DummySword01.nif;)" + R"(Weapons\Daedric\DaedricSword.nif;)" + R"(DLC01\Weapons\Dragonbone\Sword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Draugr\DraugrSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Dwarven\DwarvenSword.nif;)" + R"(Weapons\Ebony\EbonySword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Elven\ElvenSword.nif;)" + R"(Weapons\Falmer\FalmerLongSword.nif;)" + R"(Weapons\Falmer\FalmerLongSword.nif;)" + R"(Weapons\Falmer\FalmerLongSword.nif;)" + R"(Weapons\Forsworn\ForswornSword.nif;)" + R"(Weapons\Forsworn\ForswornSword.nif;)" + R"(Weapons\Forsworn\ForswornSword.nif;)" + R"(Weapons\Glass\GlassSword.nif;)" + R"(Weapons\Glass\GlassSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Imperial\ImperialSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\Orcish\OrcishSword.nif;)" + R"(Weapons\NordHero\NordHeroSword.nif;)" + R"(Weapons\NordHero\NordHeroSword.nif;)" + R"(Weapons\NordHero\NordHeroSword.nif;)" + R"(Weapons\Scimitar\Scimitar.nif;)" + R"(Weapons\Scimitar\Scimitar.nif;)" + R"(Weapons\Silver\SilverSword.nif;)" + R"(Weapons\Silver\SilverSword.nif;)" + R"(Weapons\Silver\SilverSword.nif;)" + R"(Weapons\Steel\SteelSword.nif;)" + R"(Weapons\Steel\SteelSword.nif;)" + R"(Weapons\Steel\SteelSword.nif;)" + R"(Weapons\Steel\SteelSword.nif)";

		static inline std::string TelekinesisSpells = "1A4CC:Skyrim.esm;873:BetterTelekinesis.esp;874:BetterTelekinesis.esp;876:BetterTelekinesis.esp";
		static inline std::string TelekinesisPrimary = "1A4CC:Skyrim.esm";
		static inline std::string TelekinesisSecondary = "873:BetterTelekinesis.esp;874:BetterTelekinesis.esp;876:BetterTelekinesis.esp";
		static inline std::string SpellInfo_Reach = "876:BetterTelekinesis.esp;875:BetterTelekinesis.esp";
		static inline std::string SpellInfo_One = "873:BetterTelekinesis.esp;806:BetterTelekinesis.esp";
		static inline std::string SpellInfo_NPC = "874:BetterTelekinesis.esp;809:BetterTelekinesis.esp";
		static inline std::string SpellInfo_Barr = "879:BetterTelekinesis.esp;871:BetterTelekinesis.esp;807:BetterTelekinesis.esp";
		static inline std::string SpellInfo_Blast = "87A:BetterTelekinesis.esp;872:BetterTelekinesis.esp;808:BetterTelekinesis.esp";
		static inline std::string SpellInfo_Normal = "A26E5:Skyrim.esm;1A4CC:Skyrim.esm;800:BetterTelekinesis.esp";
		static inline std::string EffectInfo_Forms = "877:BetterTelekinesis.esp;878:BetterTelekinesis.esp";

		static inline bool FixSuperHugeTelekinesisDistanceBug = true;
		static inline bool FixGrabActorHoldHostility = true;
		static inline bool FixLaunchAngleVR = true;

		static inline bool DebugLogMode = false;

		constexpr static inline std::string_view defaultConfigPath = "Data/MCM/Config/BetterTelekinesis/settings.ini";
		constexpr static inline std::string_view userConfigPath = "Data/MCM/Settings/BetterTelekinesis.ini";
	};
}
