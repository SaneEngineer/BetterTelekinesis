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
			res += format(fmt::runtime("{:.4f} "), q);
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
				ReadDoubleSetting(ini, "BaseProperties", "fOverwriteTelekinesisSpellBaseCost", OverwriteTelekinesisSpellBaseCost);
				ReadDoubleSetting(ini, "BaseProperties", "fBaseDistanceMultiplier", BaseDistanceMultiplier);
				ReadDoubleSetting(ini, "BaseProperties", "fBaseDamageMultiplier", BaseDamageMultiplier);
				ReadDoubleSetting(ini, "BaseProperties", "fObjectPullSpeedBase", ObjectPullSpeedBase);
				ReadDoubleSetting(ini, "BaseProperties", "fObjectPullSpeedAccel", ObjectPullSpeedAccel);
				ReadDoubleSetting(ini, "BaseProperties", "fObjectPullSpeedMax", ObjectPullSpeedMax);
				ReadDoubleSetting(ini, "BaseProperties", "fObjectThrowForce", ObjectThrowForce);
				ReadDoubleSetting(ini, "BaseProperties", "fObjectHoldDistance", ObjectHoldDistance);
				ReadBoolSetting(ini, "BaseProperties", "bResponsiveHold", ResponsiveHold);
				ReadStringSetting(ini, "BaseProperties", "sResponsiveHoldParams", ResponsiveHoldParams);
				ReadBoolSetting(ini, "BaseProperties", "bDontDeactivateHavokHoldSpring", DontDeactivateHavokHoldSpring);

				ReadBoolSetting(ini, "Tweaks", "bAbortTelekinesisHotkeyEnabled", AbortTelekinesisHotkeyEnabled);
				ReadIntSetting(ini, "Tweaks", "iAbortTelekinesisHotkey", AbortTelekinesisHotkey);
				ReadBoolSetting(ini, "Tweaks", "bLaunchIsHotkeyInstead", LaunchIsHotkeyInstead);
				ReadBoolSetting(ini, "Tweaks", "bTelekinesisGrabObjectSound", TelekinesisGrabObjectSound);
				ReadBoolSetting(ini, "Tweaks", "bTelekinesisLaunchObjectSound", TelekinesisLaunchObjectSound);
				ReadBoolSetting(ini, "Tweaks", "bAutoLearnTelekinesisSpell", AutoLearnTelekinesisSpell);
				ReadBoolSetting(ini, "Tweaks", "bAutoLearnTelekinesisVariants", AutoLearnTelekinesisVariants);
				ReadBoolSetting(ini, "Tweaks", "bAlwaysLaunchObjectsEvenWhenNotFinishedPulling", AlwaysLaunchObjectsEvenWhenNotFinishedPulling);
				ReadBoolSetting(ini, "Tweaks", "bDontLaunchIfRunningOutOfMagicka", DontLaunchIfRunningOutOfMagicka);
				ReadBoolSetting(ini, "Tweaks", "bPointWeaponsAndProjectilesForward", PointWeaponsAndProjectilesForward);

				ReadDoubleSetting(ini, "Enemy", "fActorPullSpeed", ActorPullSpeed);
				ReadDoubleSetting(ini, "Enemy", "fActorThrowForce", ActorThrowForce);
				ReadDoubleSetting(ini, "Enemy", "fActorHoldDistance", ActorHoldDistance);
				ReadDoubleSetting(ini, "Enemy", "fThrowActorDamage", ThrowActorDamage);
				ReadDoubleSetting(ini, "Enemy", "fHoldActorDamage", HoldActorDamage);
				ReadBoolSetting(ini, "Enemy", "bTelekinesisDisarmsEnemies", TelekinesisDisarmsEnemies);
				ReadBoolSetting(ini, "Enemy", "bActorDamageToggleEnabled", ActorDamageToggleEnabled);
				ReadIntSetting(ini, "Enemy", "iToggleActorDamageHotkey", ToggleActorDamageHotkey);

				ReadIntSetting(ini, "MultiSettings", "iTelekinesisMaxObjects", TelekinesisMaxObjects);
				ReadDoubleSetting(ini, "MultiSettings", "fTelekinesisObjectSpread", TelekinesisObjectSpread);
				ReadDoubleSetting(ini, "MultiSettings", "fMultiObjectHoverAmount", MultiObjectHoverAmount);
				
				ReadBoolSetting(ini, "Targeting", "bOverwriteTargetPicker", OverwriteTargetPicker);
				ReadDoubleSetting(ini, "Targeting", "fObjectTargetPickerRange", ObjectTargetPickerRange);
				ReadDoubleSetting(ini, "Targeting", "fActorTargetPickerRange", ActorTargetPickerRange);
				ReadIntSetting(ini, "Targeting", "iDontPickFriendlyTargets", DontPickFriendlyTargets);
				ReadDoubleSetting(ini, "Targeting", "fTelekinesisTargetUpdateInterval", TelekinesisTargetUpdateInterval);
				ReadBoolSetting(ini, "Targeting", "bTelekinesisTargetOnlyUpdateIfWeaponOut", TelekinesisTargetOnlyUpdateIfWeaponOut);
				ReadIntSetting(ini, "Targeting", "iTelekinesisLabelMode", TelekinesisLabelMode);

				ReadBoolSetting(ini, "ActorGrabPoints", "bGrabActorNodeNearest", GrabActorNodeNearest);
				ReadStringSetting(ini, "ActorGrabPoints", "sGrabActorNodeExclude", GrabActorNodeExclude);
				ReadStringSetting(ini, "ActorGrabPoints", "sGrabActorNodePriority", GrabActorNodePriority);

				ReadBoolSetting(ini, "SwordSpells", "bAddSwordSpellsToLeveledLists", AddSwordSpellsToLeveledLists);
				ReadBoolSetting(ini, "SwordSpells", "bMakeSwordSpellsAlterationInstead", MakeSwordSpellsAlterationInstead);
				ReadDoubleSetting(ini, "SwordSpells", "fMagicSword_RemoveDelay", MagicSword_RemoveDelay);
				ReadDoubleSetting(ini, "SwordSpells", "fMagicSwordBlast_PlaceDistance", MagicSwordBlast_PlaceDistance);
				ReadDoubleSetting(ini, "SwordSpells", "fMagicSwordBarrage_PlaceDistance", MagicSwordBarrage_PlaceDistance);
				ReadDoubleSetting(ini, "SwordSpells", "fSwordBarrage_FireDelay", SwordBarrage_FireDelay);
				ReadDoubleSetting(ini, "SwordSpells", "fSwordBarrage_SpawnDelay", SwordBarrage_SpawnDelay);
				ReadStringSetting(ini, "SwordSpells", "sSwordReturn_Marker", SwordReturn_Marker);
				ReadStringSetting(ini, "SwordSpells", "sBarrage_SwordModel", Barrage_SwordModel);
				ReadStringSetting(ini, "SwordSpells", "sBlast_SwordModel", Blast_SwordModel);

				ReadStringSetting(ini, "SpellForms", "sTelekinesisSpells", TelekinesisSpells);
				ReadStringSetting(ini, "SpellForms", "sTelekinesisPrimary", TelekinesisPrimary);
				ReadStringSetting(ini, "SpellForms", "sTelekinesisSecondary", TelekinesisSecondary);
				ReadStringSetting(ini, "SpellForms", "sSpellInfo_Reach", SpellInfo_Reach);
				ReadStringSetting(ini, "SpellForms", "sSpellInfo_One", SpellInfo_One);
				ReadStringSetting(ini, "SpellForms", "sSpellInfo_NPC", SpellInfo_NPC);
				ReadStringSetting(ini, "SpellForms", "sSpellInfo_Barr", SpellInfo_Barr);
				ReadStringSetting(ini, "SpellForms", "sSpellInfo_Blast", SpellInfo_Blast);
				ReadStringSetting(ini, "SpellForms", "sSpellInfo_Normal", SpellInfo_Normal);
				ReadStringSetting(ini, "SpellForms", "sEffectInfo_Forms", EffectInfo_Forms);

				ReadBoolSetting(ini, "BugFixes", "bFixSuperHugeTelekinesisDistanceBug", FixSuperHugeTelekinesisDistanceBug);
				ReadBoolSetting(ini, "BugFixes", "bFixGrabActorHoldHostility", FixGrabActorHoldHostility);
				ReadBoolSetting(ini, "BugFixes", "bFixLaunchAngleVR", FixLaunchAngleVR);

				ReadBoolSetting(ini, "Debug", "bDebugLogMode", DebugLogMode);

				return true;
			}
			return false;
		};

		logger::info("Reading default settings.ini...");
		if (readIni(defaultConfigPath)) {
			logger::info("...success");
		} else {
			logger::info("...ini not found, creating a new one");
			WriteSettings();
		}

		logger::info("Reading user .ini...");
		if (readIni(userConfigPath)) {
			logger::info("...success");
		} else {
			logger::info("...user settings not found, using defaults only");
		}
	}

	void Config::WriteSettings()
	{
		logger::info("Writing default settings.ini...");

		CSimpleIniA ini;
		ini.SetUnicode();

		ini.LoadFile(defaultConfigPath.data());

		ini.SetDoubleValue("BaseProperties", "fOverwriteTelekinesisSpellBaseCost", OverwriteTelekinesisSpellBaseCost, ";If this is not negative then the base spell cost will be set to this value.");
		ini.SetDoubleValue("BaseProperties", "fBaseDistanceMultiplier", BaseDistanceMultiplier, ";A multiplier to max base distance where telekinesis can pick stuff from. For example 2.0 would mean 2x base distance. This value may be further modified by perks.");
		ini.SetDoubleValue("BaseProperties", "fBaseDamageMultiplier", BaseDamageMultiplier, ";A multiplier to base damage of telekinesis without any perks. Vanilla game value is way too pathetic to be of any use so it's increased here by default.");
		ini.SetDoubleValue("BaseProperties", "fObjectPullSpeedBase", ObjectPullSpeedBase, ";The initial speed when pulling objects to caster.");
		ini.SetDoubleValue("BaseProperties", "fObjectPullSpeedAccel", ObjectPullSpeedAccel, ";The speed gain per second of pulling objects to caster.");
		ini.SetDoubleValue("BaseProperties", "fObjectPullSpeedMax", ObjectPullSpeedMax, ";The max speed of pulling objects to caster.");
		ini.SetDoubleValue("BaseProperties", "fObjectThrowForce", ObjectThrowForce, ";The force at which objects are thrown, compared to vanilla. For example 4.0 means 4x force of default vanilla value.");
		ini.SetDoubleValue("BaseProperties", "fObjectHoldDistance", ObjectHoldDistance, ";The distance multiplier at which to hold objects in front of you.");
		ini.SetLongValue("BaseProperties", "bResponsiveHold", ResponsiveHold, ";Make the way objects are held in place more responsive. Vanilla method lags behind and preserves momentum a lot. If you enable this method it will make objects snap immediately to where you are aiming.");
		//ini.SetValue("BaseProperties", "sResponsiveHoldParams", ResponsiveHoldParams.c_str(), ";");
		ini.SetLongValue("BaseProperties", "bDontDeactivateHavokHoldSpring", DontDeactivateHavokHoldSpring, ";If camera is completely steady the hold object spring will be deactivated, this will not look good when you're holding objects that want to make small movements or rotations.");

		ini.SetLongValue("Tweaks", "bAbortTelekinesisHotkeyEnabled", AbortTelekinesisHotkeyEnabled, ";Controls whether the below hotkey will be used.");
		ini.SetLongValue("Tweaks", "iAbortTelekinesisHotkey", AbortTelekinesisHotkey, ";When you are holding objects you can press this hotkey to abort and drop the objects to the ground where they are instead of launching them.");
		ini.SetLongValue("Tweaks", "bLaunchIsHotkeyInstead", LaunchIsHotkeyInstead, ";If set to true then you need to press the hotkey to launch objects instead, otherwise they will be dropped on the ground.");
		ini.SetLongValue("Tweaks", "bTelekinesisGrabObjectSound", TelekinesisGrabObjectSound, ";If you set this false it will disable playing the sound that happens when you grab an object with telekinesis.");
		ini.SetLongValue("Tweaks", "bTelekinesisLaunchObjectSound", TelekinesisLaunchObjectSound, ";If you set this false it will disable playing the sound that happens when you launch an object with telekinesis.");
		ini.SetLongValue("Tweaks", "bAutoLearnTelekinesisSpell", AutoLearnTelekinesisSpell, ";If set to true it will automatically add the telekinesis spell to player if they don't have it.");
		ini.SetLongValue("Tweaks", "bAutoLearnTelekinesisVariants", AutoLearnTelekinesisVariants, ";This will learn the secondary telekinesis spells when you have the primary spell.");
		ini.SetLongValue("Tweaks", "bAlwaysLaunchObjectsEvenWhenNotFinishedPulling", AlwaysLaunchObjectsEvenWhenNotFinishedPulling, ";There's a mechanic where if you pull object to you with telekinesis but release the spell before the object is finished being pulled to you it gets dropped instead of launched. Here you can overwrite the behavior and force it to always to be launched even when not finished pulling yet.");
		ini.SetLongValue("Tweaks", "bDontLaunchIfRunningOutOfMagicka", DontLaunchIfRunningOutOfMagicka, ";When the spell ends and it's time to launch objects check if you are out of magicka and if yes then don't launch?");
		ini.SetLongValue("Tweaks", "bPointWeaponsAndProjectilesForward", PointWeaponsAndProjectilesForward, ";When holding objects in telekinesis then if they are weapon or projectile point them forward, for maximum coolness.");

		ini.SetDoubleValue("Targeting", "fObjectTargetPickerRange", ObjectTargetPickerRange, ";The maximum distance from the line where you are aiming for objects to be picked to be telekinesis targets.");
		ini.SetDoubleValue("Targeting", "fActorTargetPickerRange", ActorTargetPickerRange, ";The maximum distance from the line where you are aiming for actors to be picked to be telekinesis targets.");
		ini.SetLongValue("Targeting", "iDontPickFriendlyTargets", DontPickFriendlyTargets, ";If set to 2 then don't pick any actor targets who aren't hostile to you (wouldn't attack you on detection). If set to 1 then don't pick followers marked as team-mates. If set to 0 then pick any actor.");
		ini.SetLongValue("Targeting", "bOverwriteTargetPicker", OverwriteTargetPicker, ";This will overwrite the method for how targets for telekinesis and grab actor are picked. Vanilla method will raycast your crosshair but this is very inconvenient in combat as you have to precisely always aim on the object and it's quite difficult or even impossible if the object is small or very far. This will overwrite the method to allow much more freedom in how we pick the targets.");
		ini.SetDoubleValue("Targeting", "fTelekinesisTargetUpdateInterval", TelekinesisTargetUpdateInterval, ";The interval in seconds between telekinesis target updates. This is only used if overwriting the telekinesis target picker.");
		ini.SetLongValue("Targeting", "bTelekinesisTargetOnlyUpdateIfWeaponOut", TelekinesisTargetOnlyUpdateIfWeaponOut, ";Don't update telekinesis targets if we don't have hands out. This is here to avoid unnecessarily updating the Telekinesis targets. Only used if overwriting the telekinesis target picker.");
		ini.SetLongValue("Targeting", "iTelekinesisLabelMode", TelekinesisLabelMode, ";Change how the label works when telekinesis is equipped. Due to the way the label works this option only does anything if you also enabled OverwriteTargetPicker. If you don't know what I mean, it's the thing where if you have telekinesis spell equipped (not necessarily hands out) it shows distant objects labels without the E - Activate part. That's to help you show what you are aiming at with telekinesis. So here's some choices on how that works: 0 = don't change anything, leave vanilla, objects exactly below your crosshair will show label 1 = show nearest object's label, this is the first object that would fly to you if you cast telekinesis now 2 = disable telekinesis label completely, don't show the thing");

		ini.SetLongValue("MultiSettings", "iTelekinesisMaxObjects", TelekinesisMaxObjects, ";How many objects you can hold with telekinesis at once. These are objects and not actors.");
		ini.SetDoubleValue("MultiSettings", "fTelekinesisObjectSpread", TelekinesisObjectSpread, ";When you have multiple objects telekinesised, they will spread out by this much. This is degrees rotation not actual in-game units.");
		ini.SetDoubleValue("MultiSettings", "fMultiObjectHoverAmount", MultiObjectHoverAmount, ";When enabling multi object telekinesis the objects will hover around a bit to make it look cooler. This is the amount of distance they can hover from origin.");

		ini.SetDoubleValue("Enemy", "fActorPullSpeed", ActorPullSpeed, ";The speed at which actors are pulled to caster when using the grab actor effect archetype.");
		ini.SetDoubleValue("Enemy", "fActorThrowForce", ActorThrowForce, ";The force at which actors are thrown, compared to vanilla.");
		ini.SetDoubleValue("Enemy", "fActorHoldDistance", ActorHoldDistance, ";The distance multiplier at which to hold actors in front of you.");
		ini.SetDoubleValue("Enemy", "fThrowActorDamage", ThrowActorDamage, ";This setting will cause throwing an actor to cause telekinesis damage to the same actor you just threw. The game has a bug where if you pick up a ragdoll and throw it very far it might not take almost any damage. This will make grabbing actors with telekinesis type effects much more effective way to combat them since you deal the same damage to actor by throwing them as if you would have by throwing an object at them with telekinesis. The value here says the ratio of the telekinesis damage that is done, if you set 0.5 for example then deal half of telekinesis damage to thrown actor. 0 will disable this setting.");
		ini.SetDoubleValue("Enemy", "fHoldActorDamage", HoldActorDamage, ";This setting will cause holding an actor to cause telekinesis damage per second. The amount of damage done is multiplied by this value so 0.1 would deal 10% of telekinesis damage per second to held actor.");
		ini.SetLongValue("Enemy", "bTelekinesisDisarmsEnemies", TelekinesisDisarmsEnemies, ";Sometimes telekinesis disarms enemies. Some NPCs can not be disarmed. If you are already holding the maximum amount of objects with telekinesis it will not disarm. If there are multiple NPCs around you need to aim at the NPC you want to disarm or it may choose wrong target. Enemies need to have their weapon out to be able to disarm them.");
		ini.SetLongValue("Enemy", "bActorDamageToggleEnabled", ActorDamageToggleEnabled, ";Controls whether the below hotkey will be used.");
		ini.SetLongValue("Enemy", "iToggleActorDamageHotkey", ToggleActorDamageHotkey, ";When this hotkey is pressed it will toggle the hold actor damage on or off. Hotkey values are DirectX Scan Codes, the default is caps lock.");

		ini.SetLongValue("ActorGrabPoints", "bGrabActorNodeNearest", GrabActorNodeNearest, ";When grabbing actor, select the nearest node to crosshair. That means if you aim at head and grab actor it will grab by its head. This may be inaccurate if you have any mods that modify the placement of crosshair.");
		ini.SetValue("ActorGrabPoints", "sGrabActorNodeExclude", GrabActorNodeExclude.c_str(), ";Exclude these actor nodes. This can be useful to make sure we get more accurate results.");
		ini.SetValue("ActorGrabPoints", "sGrabActorNodePriority", GrabActorNodePriority.c_str(), R"(;Decide by which node an actor is grabbed by. This setting does nothing if you have enabled GrabActorNodeNearest!!! If a node is not available on actor it will try the next node in list. For example GrabActorNodePriority = "NPC Neck [Neck];NPC Spine2 [Spn2] The above would make NPCs be held by their necks if they have one, and if not then spine.")");
		
		ini.SetLongValue("SwordSpells", "bAddSwordSpellsToLeveledLists", AddSwordSpellsToLeveledLists, ";Add the sword spell books to leveled lists of NPC vendors.");
		ini.SetLongValue("SwordSpells", "bMakeSwordSpellsAlterationInstead", MakeSwordSpellsAlterationInstead, ";Make the new sword spells alteration instead of conjuration.");
		ini.SetDoubleValue("SwordSpells", "fMagicSword_RemoveDelay", MagicSword_RemoveDelay, ";How long can magic sword exist in world without doing anything with it (in seconds).");
		ini.SetDoubleValue("SwordSpells", "fMagicSwordBlast_PlaceDistance", MagicSwordBlast_PlaceDistance, ";How far ahead magic sword to put.");
		ini.SetDoubleValue("SwordSpells", "fMagicSwordBarrage_PlaceDistance", MagicSwordBarrage_PlaceDistance, ";How far ahead magic sword to put.");
		ini.SetDoubleValue("SwordSpells", "fSwordBarrage_FireDelay", SwordBarrage_FireDelay, ";Delay in seconds before barrage will fire the sword after grabbing it.");
		ini.SetDoubleValue("SwordSpells", "fSwordBarrage_SpawnDelay", SwordBarrage_SpawnDelay, ";Delay in seconds before barrage will spawn the next sword.");
		//ini.SetValue("SwordSpells", "sSwordReturn_Marker", SwordReturn_Marker.c_str());
		ini.SetValue("SwordSpells", "sBarrage_SwordModel", Barrage_SwordModel.c_str(), ";Make the barrage spell use these nifs as models. Separate with ;");
		ini.SetValue("SwordSpells", "sBlast_SwordModel", Blast_SwordModel.c_str(), ";Make the blast spell use these nifs as models. Separate with ;");

		ini.SetValue("SpellForms", "sTelekinesisSpells", TelekinesisSpells.c_str(), ";The telekinesis spell forms. These are all the spells affected by cost overwrite or the AutoLearnTelekinesisSpell settings.");
		ini.SetValue("SpellForms", "sTelekinesisPrimary", TelekinesisPrimary.c_str(), ";The telekinesis spell that is learned from the spell book.");
		ini.SetValue("SpellForms", "sTelekinesisSecondary", TelekinesisSecondary.c_str(), ";The telekinesis spells that are variants of the primary spell.");
		//ini.SetValue("SpellForms", "sSpellInfo_Reach", SpellInfo_Reach.c_str());
		ini.SetValue("SpellForms", "sSpellInfo_One", SpellInfo_One.c_str());
		ini.SetValue("SpellForms", "sSpellInfo_NPC", SpellInfo_NPC.c_str());
		//ini.SetValue("SpellForms", "sSpellInfo_Barr", SpellInfo_Barr.c_str());
		//ini.SetValue("SpellForms", "sSpellInfo_Blast", SpellInfo_Blast.c_str());
		ini.SetValue("SpellForms", "sSpellInfo_Normal", SpellInfo_Normal.c_str());
		ini.SetValue("SpellForms", "sEffectInfo_Forms", EffectInfo_Forms.c_str());

		ini.SetLongValue("BugFixes", "bFixSuperHugeTelekinesisDistanceBug", FixSuperHugeTelekinesisDistanceBug, ";There is a vanilla bug where telekinesis distance is really big instead of what it's supposed to be. Fix that.");
		ini.SetLongValue("BugFixes", "bFixGrabActorHoldHostility", FixGrabActorHoldHostility, ";Make it so that when you grab actor it's immediately considered a hostile action.");
		ini.SetLongValue("BugFixes", "bFixLaunchAngleVR", FixLaunchAngleVR, ";Fixes a bug with Telekinesis in VR where it would use the wrong direction causing objects to launch upwards rather than straight at where you are aiming like the flat version. Only affects the VR version.");

		ini.SetLongValue("Debug", "bDebugLogMode", DebugLogMode, ";Enable additional debug logging.");

		ini.SaveFile(defaultConfigPath.data());

		logger::info("...success");
	}
}
