#pragma once
namespace game {
    std::uint32_t fname_key = 0;

    enum class bone_gender { male, female, unknown };
    enum class female_bones : std::uint32_t {
        root = 0,             // 0
        head = 8,             // 1
        pelvis = 3,             // 2
        chest = 6,             // 3
        neck = 7,             // 4
        left_shoulder = 23,     // 5
        left_elbow = 24,     // 6
        left_hand = 25,         // 7
        right_shoulder = 48, // 8
        right_elbow = 49,     // 9
        right_hand = 50,     // 10
        left_thigh = 75,     // 11
        left_knee = 76,         // 12
        left_foot = 78,         // 13
        right_thigh = 82,     // 14
        right_knee = 84,     // 15
        right_foot = 85,     // 16
        bone_max = 101,         // 17
        max                     // 18
    };

    enum class male_bones : std::uint32_t {
        root = 0,             // 0
        head = 8,             // 1
        pelvis = 3,             // 2
        chest = 6,             // 3
        neck = 21,             // 4
        left_shoulder = 23,     // 5
        left_elbow = 24,     // 6
        left_hand = 25,         // 7
        right_shoulder = 49, // 8
        right_elbow = 50,     // 9
        right_hand = 51,     // 10
        left_thigh = 77,     // 11
        left_knee = 78,         // 12
        left_foot = 80,         // 13
        right_thigh = 84,     // 14
        right_knee = 85,     // 15
        right_foot = 87,     // 16
        bone_max = 104,         // 17
        max                     // 18
    };

    static const std::unordered_map<std::string, std::string> weapon_map = {
    { "Ability_Melee_Base_C",  "Knife"   },
    { "BasePistol_C",          "Classic" },
    { "SawedOffShotgun_C",     "Shorty"  },
    { "AutomaticPistol_C",     "Frenzy"  },
    { "LugerPistol_C",         "Ghost"   },
    { "CompactPistol_C",       "Bandit"  },
    { "RevolverPistol_C",      "Sheriff" },
    { "Vector_C",              "Stinger" },
    { "SubMachineGun_MP5_C",   "Spectre" },
    { "PumpShotgun_C",         "Bucky"   },
    { "AutomaticShotgun_C",    "Judge"   },
    { "AssaultRifle_Burst_C",  "Bulldog" },
    { "DMR_C",                 "Guardian"},
    { "AssaultRifle_ACR_C",    "Phantom" },
    { "AssaultRifle_AK_C",     "Vandal"  },
    { "LeverSniperRifle_C",    "Marshal" },
    { "DS_Gun_C",              "Outlaw"  },
    { "BoltSniper_C",          "Operator"},
    { "LightMachineGun_C",     "Ares"    },
    { "HeavyMachineGun_C",     "Odin"    },
    };

    static const std::unordered_map<std::string, std::string> agent_map = {
    { "Clay_PC_C",         "Raze" },
    { "Pandemic_PC_C",     "Viper" }, // vware guy
    { "Wraith_PC_C",       "Omen" },
    { "Hunter_PC_C",       "Sova" },
    { "Thorne_PC_C",       "Sage" },
    { "Phoenix_PC_C",      "Phoenix" },
    { "Wushu_PC_C",        "Jett" },
    { "Gumshoe_PC_C",      "Cypher" },
    { "Sarge_PC_C",        "Brimstone" },
    { "Breach_PC_C",       "Breach" },
    { "Vampire_PC_C",      "Reyna" },
    { "Killjoy_PC_C",      "Killjoy" },
    { "Guide_PC_C",        "Skye" },
    { "Stealth_PC_C",      "Yoru" },
    { "Rift_PC_C",         "Astra" },
    { "Grenadie_PC_C",     "KAY/O" },
    { "Grenadier_PC_C",    "KAY/O" },
    { "Deadeye_PC_C",      "Chamber" },
    { "Sprinter_PC_C",     "Neon" },
    { "BountyHunter_PC_C", "Fade" },
    { "Mage_PC_C",         "Harbor" },
    { "AggroBot_PC_C",     "Gekko" },
    { "Cable_PC_C",        "Deadlock" },
    { "Sequoia_PC_C",      "Iso" },
    { "Smonk_PC_C",        "Clove" },
    { "Nox_PC_C",          "Vyse" },
    { "Cashew_PC_C",       "Tejo" },
    { "Terra_PC_C",        "Waylay" },
    { "Pine_PC_C",         "Veto" },
    { "Iris_PC_C",         "Miks" },
    };
}

#include <core/external/game/engine/offsets.h>
#include <core/external/game/engine/unreal/enums.h>
#include <core/external/game/engine/unreal/structs.h>
#include <core/external/game/engine/unreal/classes.h>
#include <core/external/game/engine/fname/fname.hxx>