#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "army.h"
#include "object.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Ocean lairs shelter their garrison like a ship's hull (see
// docs/BUILDING_DEFENSE_ANALYSIS.md). The shelter bonus in combat is not gated
// on unit type, so a monster inside a protected lair receives the building
// defence. These tests pin that down: the physical columns (combat/riding/
// ranged) stack onto the monster's own defence, while the magic columns
// (energy/spirit/weather) go through max(monster, -2 + bonus), so a bonus only
// registers when it beats the monster's native defence.
//
// The unittest build has its own extra.cpp and never runs the Trident ruleset's
// ModifyObjectManpower/ModifyObjectDefence calls, so the tests set the lair
// defence up themselves and restore ObjectDefs on the way out.

namespace {
    // Apply the same lair defence extra.cpp uses, then build a Soldier for a
    // monster standing inside that lair (shelter fields set the way GetSides
    // sets them for a building).
    Soldier lair_soldier(UnitTestHelper &helper, ARegion *r, int lair_type,
                         int monster_item, int prot, int co, int en, int sp,
                         int we, int ri, int ra, int region_type)
    {
        helper.game_object().ModifyObjectManpower(lair_type, prot, 0, 0, 0);
        helper.game_object().ModifyObjectDefence(lair_type, co, en, sp, we, ri, ra);

        Object *lair = new Object(r);
        lair->type = lair_type;
        lair->num = r->buildingseq++;
        r->objects.push_back(lair);

        Unit *monster = helper.create_monster(r, monster_item, 1);
        monster->MoveUnit(lair);

        lair->shelter_left = ObjectDefs[lair_type].protect;
        lair->shelter_type = lair_type;

        return Soldier(monster, lair, region_type, monster_item);
    }
}

ut::suite<"PirateLairDefense"> pirate_lair_defense_suite = [] {
    using namespace ut;

    "a pirate in a protected lair gains the building defence"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Soldier s = lair_soldier(helper, r, O_ISLE, I_PIRATES,
                                 120, 2, 3, 3, 3, 2, 2, R_PLAIN);

        expect(s.dskill[ATTACK_COMBAT] == 5_i) << "pirate combat 3 + lair 2";
        expect(s.dskill[ATTACK_ENERGY] == 1_i)  << "max(0, -2 + 3) = 1";
        expect(s.dskill[ATTACK_SPIRIT] == 1_i);
        expect(s.dskill[ATTACK_WEATHER] == 1_i);
        expect(s.dskill[ATTACK_RIDING] == 2_i)  << "0 + 2";
        expect(s.dskill[ATTACK_RANGED] == 2_i);

        helper.game_object().ModifyObjectManpower(O_ISLE, 0, 0, 0, 0);
        helper.game_object().ModifyObjectDefence(O_ISLE, 0, 0, 0, 0, 0, 0);
    };

    "a merfolk's weather stays native under a lair with no weather bonus"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        // Lair profile {2,3,3,0,2,2}: weather is left 0 because merfolk already
        // have weather 2 and a +3 would be a no-op.
        Soldier s = lair_soldier(helper, r, O_OCAVE, I_MERFOLK,
                                 100, 2, 3, 3, 0, 2, 2, R_PLAIN);

        expect(s.dskill[ATTACK_COMBAT] == 4_i) << "merfolk combat 2 + lair 2";
        expect(s.dskill[ATTACK_ENERGY] == 1_i)  << "max(0, -2 + 3) = 1";
        expect(s.dskill[ATTACK_SPIRIT] == 1_i);
        expect(s.dskill[ATTACK_WEATHER] == 2_i) << "native 2 beats max(2, -2 + 0)";
        expect(s.dskill[ATTACK_RIDING] == 2_i);
        expect(s.dskill[ATTACK_RANGED] == 2_i);

        helper.game_object().ModifyObjectManpower(O_OCAVE, 0, 0, 0, 0);
        helper.game_object().ModifyObjectDefence(O_OCAVE, 0, 0, 0, 0, 0, 0);
    };

    "a kraken's magic defence is not raised by the lair"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        // Lair profile {2,0,0,0,2,2}: magic columns stay 0 because the kraken's
        // native 5 already beats any reasonable bonus.
        Soldier s = lair_soldier(helper, r, O_DERELICT, I_KRAKEN,
                                 10, 2, 0, 0, 0, 2, 2, R_OCEAN);

        expect(s.dskill[ATTACK_COMBAT] == 8_i) << "kraken combat 6 + lair 2";
        expect(s.dskill[ATTACK_ENERGY] == 5_i) << "native 5 beats max(5, -2 + 0)";
        expect(s.dskill[ATTACK_SPIRIT] == 5_i);
        expect(s.dskill[ATTACK_WEATHER] == 5_i);
        expect(s.dskill[ATTACK_RIDING] == 5_i)  << "3 + 2";
        expect(s.dskill[ATTACK_RANGED] == 5_i);

        helper.game_object().ModifyObjectManpower(O_DERELICT, 0, 0, 0, 0);
        helper.game_object().ModifyObjectDefence(O_DERELICT, 0, 0, 0, 0, 0, 0);
    };

    "MakePirateLair scales the crew by the ruleset multiplier"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_lair_spawn_mult"] = 200;
        helper.set_ruleset_specific_data(tune);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_LAKE;   // no bosun, so the crew count is deterministic in range

        Object *lair = new Object(r);
        lair->type = O_ISLE;
        lair->num = r->buildingseq++;
        r->objects.push_back(lair);

        helper.run_make_pirate_lair(lair);

        Unit *pirates = lair->units.front();
        int n = pirates->items.GetNum(I_PIRATES);
        // base (20 + rand(20) + 1) / 2 is 10..20; ×2 -> 20..40.
        expect(n >= 20_i && n <= 40_i)
            << "a ×2 lair crew must be 20-40 pirates, got " << n;
    };
};
