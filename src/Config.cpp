#include <BetterTelekinesis/config.h>
#include <SimpleIni.h>

namespace BetterTelekinesis
{
	std::string Config::getDefaultResponsiveHoldParameters()
	{
		std::vector<float> vls;

		vls.push_back(0.75f);  // complex spring elasticity, default: 0.1
		vls.push_back(0.65f);  // normal spring elasticity, default: 0.075

		vls.push_back(0.7f);  // complex spring damping, default: 0.9
		vls.push_back(0.5f);  // normal spring damping, default: 0.5

		vls.push_back(0.5f);  // complex object damping, default: 0.7
		vls.push_back(0.5f);  // normal object damping, default: 0.95

		vls.push_back(1000);  // complex max force, default: 1000
		vls.push_back(1000);  // normal max force, default: 1000

		std::string res;
		for (auto q : vls) {
			try {
			res + fmt::format(fmt::runtime("{:.4f} "), q);
			} catch (fmt::format_error e) {
				logger::error(fmt::runtime("Failed to format string: " + std::string(e.what())));
            }
		}
		return std::regex_replace(res, std::regex(" +$"), "");  // trim trailing
	}

	void ReadBoolSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, bool& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = a_ini.GetBoolValue(a_sectionName, a_settingName);
		}
	}

	void ReadIntSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, int& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = static_cast<int>(a_ini.GetLongValue(a_sectionName, a_settingName));
		}
	}

	void ReadDoubleSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, double& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = a_ini.GetDoubleValue(a_sectionName, a_settingName);
		}
	}

	void ReadStringSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName,
		std::string& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = Util::StringHelpers::trim(a_ini.GetValue(a_sectionName, a_settingName));
		}
	}

	void Config::ReadSettings()
	{
		const auto readIni = [&](auto a_path) {
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiLine();

			if (ini.LoadFile(a_path.data()) >= 0) {
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "BaseDistanceMultiplier", BaseDistanceMultiplier);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "BaseDamageMultiplier", BaseDamageMultiplier);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectPullSpeedBase", ObjectPullSpeedBase);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectPullSpeedAccel", ObjectPullSpeedAccel);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectPullSpeedMax", ObjectPullSpeedMax);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectThrowForce", ObjectThrowForce);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectHoldDistance", ObjectHoldDistance);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ActorPullSpeed", ActorPullSpeed);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ActorThrowForce", ActorThrowForce);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ActorHoldDistance", ActorHoldDistance);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "ResponsiveHold", ResponsiveHold);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "ResponsiveHoldParams", ResponsiveHoldParams);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ThrowActorDamage", ThrowActorDamage);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "HoldActorDamage", HoldActorDamage);
				ReadIntSetting(ini, "BetterTelekinesisConfig", "AbortTelekinesisHotkey", AbortTelekinesisHotkey);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "LaunchIsHotkeyInstead", LaunchIsHotkeyInstead);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "DontLaunchIfRunningOutOfMagicka", DontLaunchIfRunningOutOfMagicka);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "OverwriteTargetPicker", OverwriteTargetPicker);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectTargetPickerRange", ObjectTargetPickerRange);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ActorTargetPickerRange", ActorTargetPickerRange);
				ReadIntSetting(ini, "BetterTelekinesisConfig", "DontPickFriendlyTargets", DontPickFriendlyTargets);
				ReadIntSetting(ini, "BetterTelekinesisConfig", "TelekinesisMaxObjects", TelekinesisMaxObjects);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "TelekinesisObjectSpread", TelekinesisObjectSpread);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "TelekinesisSpells", TelekinesisSpells);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "TelekinesisPrimary", TelekinesisPrimary);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "TelekinesisSecondary", TelekinesisSecondary);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "AutoLearnTelekinesisVariants", AutoLearnTelekinesisVariants);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "TelekinesisGrabObjectSound", TelekinesisGrabObjectSound);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "TelekinesisLaunchObjectSound", TelekinesisLaunchObjectSound);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "OverwriteTelekinesisSpellBaseCost", OverwriteTelekinesisSpellBaseCost);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "GrabActorNodeNearest", GrabActorNodeNearest);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "GrabActorNodeExclude", GrabActorNodeExclude);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "GrabActorNodePriority", GrabActorNodePriority);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "AutoLearnTelekinesisSpell", AutoLearnTelekinesisSpell);
				ReadIntSetting(ini, "BetterTelekinesisConfig", "TelekinesisLabelMode", TelekinesisLabelMode);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "FixDragonsNotBeingTelekinesisable", FixDragonsNotBeingTelekinesisable);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "FixGrabActorHoldHostility", FixGrabActorHoldHostility);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "DontDeactivateHavokHoldSpring", DontDeactivateHavokHoldSpring);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "MultiObjectHoverAmount", MultiObjectHoverAmount);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "FixSuperHugeTelekinesisDistanceBug", FixSuperHugeTelekinesisDistanceBug);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "TelekinesisTargetUpdateInterval", TelekinesisTargetUpdateInterval);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "TelekinesisTargetOnlyUpdateIfWeaponOut", TelekinesisTargetOnlyUpdateIfWeaponOut);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "PointWeaponsAndProjectilesForward", PointWeaponsAndProjectilesForward);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "AddSwordSpellsToLeveledLists", AddSwordSpellsToLeveledLists);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "MakeSwordSpellsAlterationInstead", MakeSwordSpellsAlterationInstead);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "TelekinesisDisarmsEnemies", TelekinesisDisarmsEnemies);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SpellInfo_Reach", SpellInfo_Reach);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SpellInfo_One", SpellInfo_One);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SpellInfo_NPC", SpellInfo_NPC);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SpellInfo_Barr", SpellInfo_Barr);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SpellInfo_Blast", SpellInfo_Blast);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SpellInfo_Normal", SpellInfo_Normal);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "EffectInfo_Forms", EffectInfo_Forms);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "MagicSword_RemoveDelay", MagicSword_RemoveDelay);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "MagicSwordBlast_PlaceDistance", MagicSwordBlast_PlaceDistance);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "MagicSwordBarrage_PlaceDistance", MagicSwordBarrage_PlaceDistance);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "SwordBarrage_FireDelay", SwordBarrage_FireDelay);
				ReadDoubleSetting(ini, "BetterTelekinesisConfig", "ObjectPullSpeedMax", SwordBarrage_SpawnDelay);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "SwordReturn_Marker", SwordReturn_Marker);
				ReadBoolSetting(ini, "BetterTelekinesisConfig", "AlwaysLaunchObjectsEvenWhenNotFinishedPulling", AlwaysLaunchObjectsEvenWhenNotFinishedPulling);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "Barrage_SwordModel", Barrage_SwordModel);
				ReadStringSetting(ini, "BetterTelekinesisConfig", "Blast_SwordModel", Blast_SwordModel);

				// Debug
				ReadBoolSetting(ini, "Debug", "DebugLogMode", DebugLogMode);

				return true;
			}
			return false;
		};

		logger::info("Reading .ini...");
		if (readIni(iniPath)) {
			logger::info("...success");
		} else {
			logger::info("...ini not found, creating a new one");
			WriteSettings();
		}
	}

	void Config::WriteSettings()
	{
		logger::info("Writing .ini...");

		CSimpleIniA ini;
		ini.SetUnicode();

		ini.LoadFile(iniPath.data());

		ini.SetDoubleValue("BetterTelekinesisConfig", "BaseDistanceMultiplier", BaseDistanceMultiplier, ";A multiplier to max base distance where telekinesis can pick stuff from. For example 2.0 would mean 2x base distance. This value may be further modified by perks.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "BaseDamageMultiplier", BaseDamageMultiplier, ";A multiplier to base damage of telekinesis without any perks. Vanilla game value is way too pathetic to be of any use so it's increased here by default.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectPullSpeedBase", ObjectPullSpeedBase, ";The initial speed when pulling objects to caster.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectPullSpeedAccel", ObjectPullSpeedAccel, ";The speed gain per second of pulling objects to caster.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectPullSpeedMax", ObjectPullSpeedMax, ";The max speed of pulling objects to caster.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectThrowForce", ObjectThrowForce, ";The force at which objects are thrown, compared to vanilla. For example 4.0 means 4x force of default vanilla value.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectHoldDistance", ObjectHoldDistance, ";The distance multiplier at which to hold objects in front of you.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ActorPullSpeed", ActorPullSpeed, ";The speed at which actors are pulled to caster when using the grab actor effect archetype.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ActorThrowForce", ActorThrowForce, ";The force at which actors are thrown, compared to vanilla.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ActorHoldDistance", ActorHoldDistance, ";The distance multiplier at which to hold actors in front of you.");
		ini.SetBoolValue("BetterTelekinesisConfig", "ResponsiveHold", ResponsiveHold, ";Make the way objects are held in place more responsive. Vanilla method lags behind and preserves momentum a lot. If you enable this method it will make objects snap immediately to where you are aiming.");
		//ini.SetValue("BetterTelekinesisConfig", "ResponsiveHoldParams", ResponsiveHoldParams.c_str(), ";");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ThrowActorDamage", ThrowActorDamage, ";This setting will cause throwing an actor to cause telekinesis damage to the same actor you just threw. The game has a bug where if you pick up a ragdoll and throw it very far it might not take almost any damage. This will make grabbing actors with telekinesis type effects much more effective way to combat them since you deal the same damage to actor by throwing them as if you would have by throwing an object at them with telekinesis. The value here says the ratio of the telekinesis damage that is done, if you set 0.5 for example then deal half of telekinesis damage to thrown actor. 0 will disable this setting.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "HoldActorDamage", HoldActorDamage, ";This setting will cause holding an actor to cause telekinesis damage per second. The amount of damage done is multiplied by this value so 0.1 would deal 10% of telekinesis damage per second to held actor.");
		ini.SetLongValue("BetterTelekinesisConfig", "AbortTelekinesisHotkey", AbortTelekinesisHotkey, ";When you are holding objects you can press this hotkey to abort and drop the objects to the ground where they are instead of launching them.");
		ini.SetBoolValue("BetterTelekinesisConfig", "LaunchIsHotkeyInstead", LaunchIsHotkeyInstead, ";If set to true then you need to press the hotkey to launch objects instead, otherwise they will be dropped on the ground.");
		ini.SetBoolValue("BetterTelekinesisConfig", "DontLaunchIfRunningOutOfMagicka", DontLaunchIfRunningOutOfMagicka, ";When the spell ends and it's time to launch objects check if you are out of magicka and if yes then don't launch?");
		ini.SetBoolValue("BetterTelekinesisConfig", "OverwriteTargetPicker", OverwriteTargetPicker, ";This will overwrite the method for how targets for telekinesis and grab actor are picked. Vanilla method will raycast your crosshair but this is very inconvenient in combat as you have to precisely always aim on the object and it's quite difficult or even impossible if the object is small or very far. This will overwrite the method to allow much more freedom in how we pick the targets.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectTargetPickerRange", ObjectTargetPickerRange, ";The maximum distance from the line where you are aiming for objects to be picked to be telekinesis targets.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ActorTargetPickerRange", ActorTargetPickerRange, ";The maximum distance from the line where you are aiming for actors to be picked to be telekinesis targets.");
		ini.SetLongValue("BetterTelekinesisConfig", "DontPickFriendlyTargets", DontPickFriendlyTargets, ";If set to 2 then don't pick any actor targets who aren't hostile to you (wouldn't attack you on detection). If set to 1 then don't pick followers marked as team-mates. If set to 0 then pick any actor.");
		ini.SetLongValue("BetterTelekinesisConfig", "TelekinesisMaxObjects", TelekinesisMaxObjects, ";How many objects you can hold with telekinesis at once. These are objects and not actors.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "TelekinesisObjectSpread", TelekinesisObjectSpread, ";When you have multiple objects telekinesised, they will spread out by this much. This is degrees rotation not actual in-game units.");
		ini.SetValue("BetterTelekinesisConfig", "TelekinesisSpells", TelekinesisSpells.c_str(), ";The telekinesis spell forms. These are all the spells affected by cost overwrite or the AutoLearnTelekinesisSpell settings.");
		ini.SetValue("BetterTelekinesisConfig", "TelekinesisPrimary", TelekinesisPrimary.c_str(), ";The telekinesis spell that is learned from the spell book.");
		ini.SetValue("BetterTelekinesisConfig", "TelekinesisSecondary", TelekinesisSecondary.c_str(), ";The telekinesis spells that are variants of the primary spell.");
		ini.SetBoolValue("BetterTelekinesisConfig", "AutoLearnTelekinesisVariants", AutoLearnTelekinesisVariants, ";This will learn the secondary telekinesis spells when you have the primary spell.");
		ini.SetBoolValue("BetterTelekinesisConfig", "TelekinesisGrabObjectSound", TelekinesisGrabObjectSound, ";If you set this false it will disable playing the sound that happens when you grab an object with telekinesis.");
		ini.SetBoolValue("BetterTelekinesisConfig", "TelekinesisLaunchObjectSound", TelekinesisLaunchObjectSound, ";If you set this false it will disable playing the sound that happens when you launch an object with telekinesis.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "OverwriteTelekinesisSpellBaseCost", OverwriteTelekinesisSpellBaseCost, ";If this is not negative then the base spell cost will be set to this value.");
		ini.SetBoolValue("BetterTelekinesisConfig", "GrabActorNodeNearest", GrabActorNodeNearest, ";When grabbing actor, select the nearest node to crosshair. That means if you aim at head and grab actor it will grab by its head. This may be inaccurate if you have any mods that modify the placement of crosshair.");
		ini.SetValue("BetterTelekinesisConfig", "GrabActorNodeExclude", GrabActorNodeExclude.c_str(), ";Exclude these actor nodes. This can be useful to make sure we get more accurate results.");
		ini.SetValue("BetterTelekinesisConfig", "GrabActorNodePriority", GrabActorNodePriority.c_str(), R"(;Decide by which node an actor is grabbed by. This setting does nothing if you have enabled GrabActorNodeNearest!!! If a node is not available on actor it will try the next node in list. For example GrabActorNodePriority = "NPC Neck [Neck];NPC Spine2 [Spn2] The above would make NPCs be held by their necks if they have one, and if not then spine.")");
		ini.SetBoolValue("BetterTelekinesisConfig", "AutoLearnTelekinesisSpell", AutoLearnTelekinesisSpell, ";If set to true it will automatically add the telekinesis spell to player if they don't have it.");
		ini.SetLongValue("BetterTelekinesisConfig", "TelekinesisLabelMode", TelekinesisLabelMode, ";Change how the label works when telekinesis is equipped. Due to the way the label works this option only does anything if you also enabled OverwriteTargetPicker. If you don't know what I mean, it's the thing where if you have telekinesis spell equipped (not necessarily hands out) it shows distant objects labels without the E - Activate part. That's to help you show what you are aiming at with telekinesis. So here's some choices on how that works: 0 = don't change anything, leave vanilla, objects exactly below your crosshair will show label 1 = show nearest object's label, this is the first object that would fly to you if you cast telekinesis now 2 = disable telekinesis label completely, don't show the thing");
		ini.SetBoolValue("BetterTelekinesisConfig", "FixDragonsNotBeingTelekinesisable", FixDragonsNotBeingTelekinesisable, ";WARNING: Don't enable, because once the ragdoll of dragon ends it loses all collision and gets stuck instead of getting up!");
		ini.SetBoolValue("BetterTelekinesisConfig", "FixGrabActorHoldHostility", FixGrabActorHoldHostility, ";Make it so that when you grab actor it's immediately considered a hostile action.");
		ini.SetBoolValue("BetterTelekinesisConfig", "DontDeactivateHavokHoldSpring", DontDeactivateHavokHoldSpring, ";If camera is completely steady the hold object spring will be deactivated, this will not look good when you're holding objects that want to make small movements or rotations.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "MultiObjectHoverAmount", MultiObjectHoverAmount, ";When enabling multi object telekinesis the objects will hover around a bit to make it look cooler. This is the amount of distance they can hover from origin.");
		ini.SetBoolValue("BetterTelekinesisConfig", "FixSuperHugeTelekinesisDistanceBug", FixSuperHugeTelekinesisDistanceBug, ";There is a vanilla bug where telekinesis distance is really big instead of what it's supposed to be. Fix that.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "TelekinesisTargetUpdateInterval", TelekinesisTargetUpdateInterval, ";The interval in seconds between telekinesis target updates. This is only used if overwriting the telekinesis target picker.");
		ini.SetBoolValue("BetterTelekinesisConfig", "TelekinesisTargetOnlyUpdateIfWeaponOut", TelekinesisTargetOnlyUpdateIfWeaponOut, ";Don't update telekinesis targets if we don't have hands out. This is here to avoid unnecessarily updating the elekinesis targets. Only used if overwriting the telekinesis target picker.");
		ini.SetBoolValue("BetterTelekinesisConfig", "PointWeaponsAndProjectilesForward", PointWeaponsAndProjectilesForward, ";When holding objects in telekinesis then if they are weapon or projectile point them forward, for maximum coolness.");
		ini.SetBoolValue("BetterTelekinesisConfig", "AddSwordSpellsToLeveledLists", AddSwordSpellsToLeveledLists, ";Add the sword spell books to leveled lists of NPC vendors.");
		ini.SetBoolValue("BetterTelekinesisConfig", "MakeSwordSpellsAlterationInstead", MakeSwordSpellsAlterationInstead, ";Make the new sword spells alteration instead of conjuration.");
		ini.SetBoolValue("BetterTelekinesisConfig", "TelekinesisDisarmsEnemies", TelekinesisDisarmsEnemies, ";Sometimes telekinesis disarms enemies. Some NPCs can not be disarmed. If you are already holding the maximum amount of objects with telekinesis it will not disarm. If there are multiple NPCs around you need to aim at the NPC you want to disarm or it may choose wrong target. Enemies need to have their weapon out to be able to disarm them.");
		//ini.SetValue("BetterTelekinesisConfig", "SpellInfo_Reach", SpellInfo_Reach.c_str());
		//ini.SetValue("BetterTelekinesisConfig", "SpellInfo_One", SpellInfo_One.c_str());
		//ini.SetValue("BetterTelekinesisConfig", "SpellInfo_NPC", SpellInfo_NPC.c_str());
		//ini.SetValue("BetterTelekinesisConfig", "SpellInfo_Barr", SpellInfo_Barr.c_str());
		//ini.SetValue("BetterTelekinesisConfig", "SpellInfo_Blast", SpellInfo_Blast.c_str());
		//ini.SetValue("BetterTelekinesisConfig", "SpellInfo_Normal", SpellInfo_Normal.c_str());
		//ini.SetValue("BetterTelekinesisConfig", "EffectInfo_Forms", EffectInfo_Forms.c_str());
		ini.SetDoubleValue("BetterTelekinesisConfig", "MagicSword_RemoveDelay", MagicSword_RemoveDelay, ";How long can magic sword exist in world without doing anything with it (in seconds).");
		ini.SetDoubleValue("BetterTelekinesisConfig", "MagicSwordBlast_PlaceDistance", MagicSwordBlast_PlaceDistance, ";How far ahead magic sword to put.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "MagicSwordBarrage_PlaceDistance", MagicSwordBarrage_PlaceDistance, ";How far ahead magic sword to put.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "SwordBarrage_FireDelay", SwordBarrage_FireDelay, ";Delay in seconds before barrage will fire the sword after grabbing it.");
		ini.SetDoubleValue("BetterTelekinesisConfig", "ObjectPullSpeedMax", SwordBarrage_SpawnDelay, ";Delay in seconds before barrage will spawn the next sword.");
		//ini.SetValue("BetterTelekinesisConfig", "SwordReturn_Marker", SwordReturn_Marker.c_str());
		ini.SetBoolValue("BetterTelekinesisConfig", "AlwaysLaunchObjectsEvenWhenNotFinishedPulling", AlwaysLaunchObjectsEvenWhenNotFinishedPulling, ";There's a mechanic where if you pull object to you with telekinesis but release the spell before the object is finished being pulled to you it gets dropped instead of launched. Here you can overwrite the behavior and force it to always to be launched even when not finished pulling yet.");
		ini.SetValue("BetterTelekinesisConfig", "Barrage_SwordModel", Barrage_SwordModel.c_str(), ";Make the barrage spell use these nifs as models. Separate with ;");
		ini.SetValue("BetterTelekinesisConfig", "Blast_SwordModel", Blast_SwordModel.c_str(), ";Make the blast spell use these nifs as models. Separate with ;");

		// Debug
		ini.SetBoolValue("Debug", "Debug-Log-Enable", DebugLogMode, ";Enable additional debug logging.");

		ini.SaveFile(iniPath.data());

		logger::info("...success");
	}
}
