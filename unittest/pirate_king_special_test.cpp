#include "external/boost/ut.hpp"
#include "gamedata.h"
#include "items.h"
#include "skills.h"

namespace ut = boost::ut;

// Verifies that the Pirate King's pistol_volley special is defined correctly
// and that the I_PIRATE_KING MonType references it at the expected level.
//
// DamageType field order: {type, minnum, value, flags, dclass, effect, hitDamage}
// Formula in battle.cpp: realtimes = minnum + rng(value) + rng(value)
//   → value=1 means rng(1)=0 always → exactly minnum shots per round.
// specialLevel is the attack skill per shot (not a shot-count multiplier
// unless FX_USE_LEV is set; pistol_volley uses FX_DAMAGE only).

ut::suite<"PirateKingSpecial"> pirate_king_special_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: pistol_volley special exists and is well-formed.
    // -----------------------------------------------------------------------
    "pistol_volley special exists"_test = [] {
        auto sp = find_special("pistol_volley");
        expect(sp.has_value()) << "pistol_volley must be registered in SpecialDefs";
    };

    // -----------------------------------------------------------------------
    // Test 2: damage[0] — 10 armor-piercing ranged shots of 5 damage each.
    // -----------------------------------------------------------------------
    "pistol_volley damage[0]: 10 ATTACK_RANGED ARMORPIERCING shots of 5 damage"_test = [] {
        auto sp = find_special("pistol_volley");
        expect(sp.has_value());
        if (!sp) return;

        const auto &d = sp->get().damage[0];
        expect(d.type      == (int)ATTACK_RANGED)  << "must be ATTACK_RANGED";
        expect(d.minnum    == 10_i)                << "exactly 10 shots (minnum=10, value=1 → rng(1)=0)";
        expect(d.value     == 1_i)                 << "value=1 → zero added randomness";
        expect(d.hitDamage == 5_i)                 << "5 damage per shot";
        expect(d.dclass    == (int)ARMORPIERCING)  << "armor-piercing";
        expect((d.flags & WeaponType::ALWAYSREADY) != 0_i) << "must fire every round (ALWAYSREADY)";
        expect((d.flags & WeaponType::RANGED)     != 0_i)  << "must be RANGED (fires from behind)";
    };

    // -----------------------------------------------------------------------
    // Test 3: damage[1] — tornado (Kraken spirit, ATTACK_WEATHER).
    // -----------------------------------------------------------------------
    "pistol_volley damage[1]: ATTACK_WEATHER tornado"_test = [] {
        auto sp = find_special("pistol_volley");
        expect(sp.has_value());
        if (!sp) return;

        const auto &d = sp->get().damage[1];
        expect(d.type      == (int)ATTACK_WEATHER) << "second entry must be ATTACK_WEATHER (tornado)";
        expect(d.hitDamage == 1_i)                 << "1 damage per tornado hit";
        expect((d.flags & WeaponType::ALWAYSREADY) != 0_i) << "tornado must also fire every round";
        expect((d.flags & WeaponType::RANGED)     != 0_i)  << "tornado fires from range";
    };

    // -----------------------------------------------------------------------
    // Test 4: damage[2] and [3] must be unused (type == -1).
    // -----------------------------------------------------------------------
    "pistol_volley has no extra damage entries"_test = [] {
        auto sp = find_special("pistol_volley");
        expect(sp.has_value());
        if (!sp) return;

        expect(sp->get().damage[2].type == -1_i) << "damage[2] must be unused";
        expect(sp->get().damage[3].type == -1_i) << "damage[3] must be unused";
    };

    // -----------------------------------------------------------------------
    // Test 5: I_PIRATE_KING MonType references pistol_volley at level 5.
    // -----------------------------------------------------------------------
    "PIRATE_KING MonType uses pistol_volley at specialLevel 5"_test = [] {
        auto mp = find_monster("PKIN", false);
        expect(mp.has_value()) << "PKIN monster must exist in MonDefs";
        if (!mp) return;

        expect(std::string(mp->get().special) == "pistol_volley")
            << "King's special must be pistol_volley";
        expect(mp->get().specialLevel == 5_i)
            << "specialLevel=5 is the attack skill for each pistol shot";
    };

    // -----------------------------------------------------------------------
    // Test 6: Consistency — the special referenced by PKIN must parse cleanly.
    // Catches typos where MonType names a special that doesn't exist.
    // -----------------------------------------------------------------------
    "PIRATE_KING special name resolves in SpecialDefs"_test = [] {
        auto mp = find_monster("PKIN", false);
        expect(mp.has_value());
        if (!mp) return;

        const char *sname = mp->get().special;
        expect(sname != nullptr) << "special must not be null";
        if (!sname) return;

        auto sp = find_special(sname);
        expect(sp.has_value())
            << std::string("special '") + sname + "' must exist in SpecialDefs";
    };
};
