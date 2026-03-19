#include "external/boost/ut.hpp"

#include "game.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Test world layout (2x4 hex grid, valid cells where (x+y)%2==0):
//   (0,0,0) = starting city (has TownInfo)
//   (0,2,0) = plain region, no town
//   (1,1,0) = plain region, no town
//   (1,3,0) = plain region, no town

static ARegion* get_city_region(UnitTestHelper& helper) {
    return helper.get_region(0, 0, 0);
}

static ARegion* get_plain_region(UnitTestHelper& helper) {
    return helper.get_region(0, 2, 0);
}

// ============================================================
// Wages
// ============================================================

ut::suite<"Economy - Wages"> wages_suite = []
{
    using namespace ut;

    // Wages formula uses triangular number brackets:
    // dv in [1,3)  → base $1.0 (wages=10)
    // dv in [3,6)  → base $2.0 (wages=20)
    // dv in [6,10) → base $3.0 (wages=30)
    // dv in [10,15)→ base $4.0 (wages=40)
    // dv in [15,21)→ base $5.0 (wages=50)
    // Fractional part: wages += 10*(dv-last)/(level-last)
    // dv = development + RoadDevelopment() + (earthlore+clearskies)*12

    "Wages returns 0 for zero population"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        TownInfo *saved = r->town;
        r->town = nullptr;
        r->population = 0;

        expect(r->Wages() == 0_i) << "zero population must give zero wages";

        r->town = saved;
    };

    "Wages increases monotonically with development"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        int prev = 0;
        for (int dev = 1; dev <= 50; dev++) {
            r->development = dev;
            int w = r->Wages();
            expect(w >= prev) << "wages must not decrease as development rises (dev=" << dev << ")";
            prev = w;
        }

        r->town = saved;
    };

    "Wages: development=1 → wages=10 ($1.0)"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 1;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        expect(r->Wages() == 10_i) << "dv=1 should give wages=10 ($1.0)";

        r->town = saved;
    };

    "Wages: development=3 → wages=20 ($2.0)"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 3;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        expect(r->Wages() == 20_i) << "dv=3 should give wages=20 ($2.0)";

        r->town = saved;
    };

    "Wages: development=5 → wages=26 ($2.6, fractional)"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 5;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        // dv=5 in bracket [3,6): base=20, frac=10*(5-3)/(6-3)=6 → 26
        expect(r->Wages() == 26_i) << "dv=5 should give wages=26 ($2.6)";

        r->town = saved;
    };

    "Wages: development=6 → wages=30 ($3.0)"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 6;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        expect(r->Wages() == 30_i) << "dv=6 should give wages=30 ($3.0)";

        r->town = saved;
    };

    "Wages: development=10 → wages=40 ($4.0)"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 10;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        expect(r->Wages() == 40_i) << "dv=10 should give wages=40 ($4.0)";

        r->town = saved;
    };

    "Wages: development=15 → wages=50 ($5.0)"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 15;
        r->earthlore = 0;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        expect(r->Wages() == 50_i) << "dv=15 should give wages=50 ($5.0)";

        r->town = saved;
    };

    "Earthlore level 1 adds 12 to effective dv → increases wages"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 1000;
        r->development = 1;
        r->clearskies = 0;
        TownInfo *saved = r->town;
        r->town = nullptr;

        r->earthlore = 0;
        int wages_base = r->Wages();  // dv=1 → 10

        r->earthlore = 1;             // dv=1+12=13
        int wages_spell = r->Wages();

        expect(wages_spell > wages_base)
            << "earthlore level 1 must raise wages above base";

        r->town = saved;
        r->earthlore = 0;
    };
};

// ============================================================
// Population
// ============================================================

ut::suite<"Economy - Population"> population_suite = []
{
    using namespace ut;

    "Population without town returns region population only"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        TownInfo *saved = r->town;
        r->town = nullptr;
        r->population = 1500;

        expect(r->Population() == 1500_i);

        r->town = saved;
    };

    "Population with town sums region pop and town pop"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_city_region(helper);

        expect(r->town != nullptr) << "starting city must have a town";

        int region_pop = r->population;
        int town_pop = r->town->pop;

        expect(r->Population() == region_pop + town_pop);
    };

    "Population changes when town pop is modified"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_city_region(helper);

        int old_total = r->Population();
        r->town->pop += 100;

        expect(r->Population() == (old_total + 100));

        r->town->pop -= 100;  // restore
    };
};

// ============================================================
// TownType
// ============================================================

ut::suite<"Economy - TownType"> towntype_suite = []
{
    using namespace ut;

    // prestige = pop * (dev + 220) / 270
    // TOWN_VILLAGE if prestige < CITY_POP/4       (< 2500 for CITY_POP=10000)
    // TOWN_TOWN   if prestige < CITY_POP*4/5      (< 8000)
    // TOWN_CITY   otherwise

    "TownType: small pop and zero dev → TOWN_VILLAGE"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        TownInfo town;
        town.pop = 500;
        town.dev = 0;
        // prestige = 500 * 220 / 270 = 407 < 2500
        expect(town.TownType() == TOWN_VILLAGE);
    };

    "TownType: medium prestige → TOWN_TOWN"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        // CITY_POP=20000 in unittest rules → thresholds: village<5000, town<16000, city≥16000
        TownInfo town;
        // pop=7000, dev=0: prestige = 7000 * 220 / 270 = 5703 → TOWN
        town.pop = 7000;
        town.dev = 0;
        int prestige = town.pop * (town.dev + 220) / 270;
        expect(prestige >= Globals->CITY_POP / 4 && prestige < Globals->CITY_POP * 4 / 5)
            << "setup: prestige=" << prestige << " must be in [5000, 16000)";
        expect(town.TownType() == TOWN_TOWN);
    };

    "TownType: high pop → TOWN_CITY"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        // CITY_POP=20000 → city threshold: prestige ≥ 16000
        // pop=20000, dev=0: prestige = 20000 * 220 / 270 = 16296 ≥ 16000
        TownInfo town;
        town.pop = 20000;
        town.dev = 0;
        int prestige = town.pop * (town.dev + 220) / 270;
        expect(prestige >= Globals->CITY_POP * 4 / 5)
            << "setup: prestige=" << prestige << " must be ≥ 16000";
        expect(town.TownType() == TOWN_CITY);
    };

    "TownType: starting city in test world is TOWN_CITY"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_city_region(helper);

        expect(r->town != nullptr) << "region (0,0,0) must have a town";
        expect(r->town->TownType() == TOWN_CITY);
    };

    "TownType: adding population can upgrade village to town"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        // CITY_POP=20000 → village threshold: prestige < 5000
        // pop * 220 / 270 < 5000 → pop < 6137
        // pop * 220 / 270 ≥ 5000 → pop ≥ 6137
        TownInfo town;
        town.dev = 0;

        town.pop = 500;    // prestige=407 → VILLAGE
        expect(town.TownType() == TOWN_VILLAGE);

        town.pop = 6200;   // prestige=5051 → TOWN
        expect(town.TownType() == TOWN_TOWN);
    };
};

// ============================================================
// TownDevelopment
// ============================================================

ut::suite<"Economy - TownDevelopment"> towndev_suite = []
{
    using namespace ut;

    // TownDevelopment() = clamp(development - BaseDev(), 0, 100)

    "TownDevelopment is non-negative when development is very low"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        r->development = 0;
        expect(r->TownDevelopment() >= 0_i)
            << "TownDevelopment must never be negative";
    };

    "TownDevelopment is capped at 100"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        r->development = 9999;
        expect(r->TownDevelopment() == 100_i)
            << "TownDevelopment must not exceed 100";
    };

    "TownDevelopment is 0 at basedev and 50 at basedev+50"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        int basedev = r->BaseDev();

        r->development = basedev;
        expect(r->TownDevelopment() == 0_i)
            << "development=basedev should give TownDevelopment=0";

        r->development = basedev + 50;
        expect(r->TownDevelopment() == 50_i)
            << "development=basedev+50 should give TownDevelopment=50";
    };

    "TownDevelopment increases by 1 for each development point above basedev"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        int basedev = r->BaseDev();
        int prev = -1;
        for (int d = 0; d <= 100; d++) {
            r->development = basedev + d;
            int td = r->TownDevelopment();
            if (prev >= 0) {
                expect(td == (prev + 1))
                    << "TownDevelopment should increase by 1 per dev point (d=" << d << ")";
            }
            prev = td;
        }
    };
};

// ============================================================
// TownGrowth
// ============================================================

ut::suite<"Economy - TownGrowth"> towngrowth_suite = []
{
    using namespace ut;

    "TownGrowth for starting city returns current pop unchanged"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_city_region(helper);

        expect(r->town != nullptr);
        int town_pop = r->town->pop;
        r->improvement = 0;

        // Starting city → IsStartingCity()=true → tarpop = town->pop, no market loop
        expect(r->TownGrowth() == town_pop)
            << "starting city TownGrowth must return current pop";
    };

    // NOTE: In the unittest world, IsStartingCity() returns (town != nullptr).
    // This means any region with a town skips the market growth loop in TownGrowth().
    // The tests below validate this contract.

    "TownGrowth: any town in unittest world acts as starting city"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);
        r->population = 5000;

        delete r->town;
        r->town = new TownInfo();
        r->town->pop = 500;
        r->town->dev = 10;
        r->town->hab = 300;
        r->improvement = 0;

        // In unittest IsStartingCity() == (town != nullptr) == true
        // → TownGrowth returns town->pop unchanged regardless of markets
        expect(r->TownGrowth() == 500_i)
            << "unittest IsStartingCity() returns true for any town → no market growth";
    };

    "TownGrowth: result is non-negative"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_city_region(helper);

        expect(r->town != nullptr);
        r->improvement = 0;

        expect(r->TownGrowth() >= 0_i) << "TownGrowth must never return negative";
    };
};

// ============================================================
// Grow (population growth direction)
// ============================================================

ut::suite<"Economy - Grow"> grow_suite = []
{
    using namespace ut;

    "Grow skips region with basepopulation=0"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        int saved_basepop = r->basepopulation;
        int saved_pop = r->population;
        r->basepopulation = 0;

        helper.run_grow(r);

        expect(r->population == saved_pop)
            << "Grow() must return immediately when basepopulation=0";

        r->basepopulation = saved_basepop;
    };

    "Grow moves population upward when below habitat"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        TownInfo *saved_town = r->town;
        r->town = nullptr;

        r->basepopulation = 200;
        r->habitat = 3000;
        r->population = 200;   // at floor, well below habitat
        r->immigrants = 0;
        r->emigrants = 0;

        int pop_before = r->population;
        helper.run_grow(r);

        expect(r->population >= pop_before)
            << "population below habitat must not shrink in Grow()";

        r->town = saved_town;
    };

    "Grow reduces population when above habitat"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        TownInfo *saved_town = r->town;
        r->town = nullptr;

        r->basepopulation = 200;
        r->habitat = 200;
        r->population = 3000;  // far above habitat
        r->immigrants = 0;
        r->emigrants = 0;

        int pop_before = r->population;
        helper.run_grow(r);

        expect(r->population <= pop_before)
            << "population above habitat must not grow in Grow()";

        r->town = saved_town;
    };

    "Grow: population at equilibrium changes minimally"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = get_plain_region(helper);

        TownInfo *saved_town = r->town;
        r->town = nullptr;

        // Set population exactly at habitat with no production bonus
        r->basepopulation = 1000;
        r->habitat = 1000;
        r->population = 1000;
        r->immigrants = 0;
        r->emigrants = 0;

        int pop_before = r->population;
        helper.run_grow(r);
        int pop_after = r->population;

        // At exact equilibrium: tarpop = habitat - population + basepopulation = 1000
        // diff = 0, dgrow = 0 → no change
        expect(pop_after == pop_before)
            << "population at exact equilibrium should not change";

        r->town = saved_town;
    };
};
