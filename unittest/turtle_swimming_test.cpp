#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Tests for SWIMMERS_COASTAL_ONLY deep-ocean restriction.
//
// LIZA (lizardmen) have innate swim capacity and are restricted to coastal ocean
// and lakes. Units mounted on a sea creature (turtle, ride > 0 && swim > 0) must
// bypass this restriction and be allowed into deep ocean.
//
// Bug: CanSwimTo blocked any unit whose SwimmingCapacity >= weight, including those
// riding a turtle. Fix: if RidingCapacity >= weight the unit is mounted, not swimming
// on its own power, and may enter deep ocean.
//
// Verified cases:
//   1. LIZA (no turtle) → coastal ocean: allowed
//   2. LIZA (no turtle) → deep ocean: blocked
//   3. LIZA + turtle   → deep ocean: allowed  (fixed)
//   4. Human + turtle  → deep ocean: allowed  (regression: was also blocked)
ut::suite<"TurtleSwimming"> turtle_swimming_suite = [] {
    using namespace ut;

    // LIZA (no turtle): coastal ocean is allowed.
    "LIZA without turtle can swim to coastal ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test");
        Unit *unit = helper.create_unit(faction, helper.get_region(0, 0, 0));
        unit->items.SetNum(I_LEADERS, 0);
        unit->items.SetNum(I_LIZARDMAN, 5);  // weight 50, swim 75

        int saved = Globals->SWIMMERS_COASTAL_ONLY;
        Globals->SWIMMERS_COASTAL_ONLY = 1;

        ARegion land;
        land.type = R_PLAIN;
        ARegion coastal;
        coastal.type = R_OCEAN;
        coastal.neighbors[0] = &land;  // land neighbor → IsDeepOcean() = false

        expect(unit->CanSwimTo(&coastal) == 1_i)
            << "LIZA without turtle must be able to swim in coastal ocean";

        Globals->SWIMMERS_COASTAL_ONLY = saved;
    };

    // LIZA (no turtle): deep ocean (no land neighbors) is blocked.
    "LIZA without turtle cannot swim to deep ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test");
        Unit *unit = helper.create_unit(faction, helper.get_region(0, 0, 0));
        unit->items.SetNum(I_LEADERS, 0);
        unit->items.SetNum(I_LIZARDMAN, 5);  // weight 50, swim 75

        int saved = Globals->SWIMMERS_COASTAL_ONLY;
        Globals->SWIMMERS_COASTAL_ONLY = 1;

        ARegion deep;
        deep.type = R_OCEAN;  // all neighbors null → IsDeepOcean() = true

        expect(unit->CanSwimTo(&deep) == 0_i)
            << "LIZA without turtle must be blocked from deep ocean";

        Globals->SWIMMERS_COASTAL_ONLY = saved;
    };

    // LIZA + turtle: deep ocean allowed because the turtle mounts the unit.
    "LIZA with turtle can swim to deep ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test");
        Unit *unit = helper.create_unit(faction, helper.get_region(0, 0, 0));
        unit->items.SetNum(I_LEADERS, 0);
        unit->items.SetNum(I_LIZARDMAN, 1);  // weight 10, swim 15
        unit->items.SetNum(I_TURT, 1);       // weight 50, swim 70, ride 70
        // total: weight 60, swim 85, ride 70

        int saved = Globals->SWIMMERS_COASTAL_ONLY;
        Globals->SWIMMERS_COASTAL_ONLY = 1;

        ARegion deep;
        deep.type = R_OCEAN;

        expect(unit->CanSwimTo(&deep) == 1_i)
            << "LIZA with turtle must be able to enter deep ocean";

        Globals->SWIMMERS_COASTAL_ONLY = saved;
    };

    // Human (leader) + turtle: deep ocean allowed — regression for the reported bug.
    "Human with turtle can swim to deep ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test");
        Unit *unit = helper.create_unit(faction, helper.get_region(0, 0, 0));
        // unit already has 1 LEADER (weight 10, swim 0)
        unit->items.SetNum(I_TURT, 1);  // weight 50, swim 70, ride 70
        // total: weight 60, swim 70, ride 70

        int saved = Globals->SWIMMERS_COASTAL_ONLY;
        Globals->SWIMMERS_COASTAL_ONLY = 1;

        ARegion deep;
        deep.type = R_OCEAN;

        expect(unit->CanSwimTo(&deep) == 1_i)
            << "Human with turtle must be able to enter deep ocean";

        Globals->SWIMMERS_COASTAL_ONLY = saved;
    };
};
