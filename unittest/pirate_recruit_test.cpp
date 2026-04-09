#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

ut::suite<"PirateRecruit"> pirate_recruit_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Pirates on land: crew count must increase after PirateRecruitLandCrew
    // -----------------------------------------------------------------------
    "Pirates recruit new crew when fleet is docked on land"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 10);
        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        int after = pirates->items.GetNum(I_PIRATES);
        expect(after > before)
            << "pirate crew must grow when fleet is docked on land";
        // 10% of 10 = 1 minimum, 20% of 10 = 2 maximum
        expect(after >= before + 1)
            << "at least 1 pirate must be recruited";
        expect(after <= before + 2)
            << "at most 20% (2) pirates recruited from 10";
    };

    // -----------------------------------------------------------------------
    // Pirates in ocean: crew count must NOT change
    // -----------------------------------------------------------------------
    "Pirates do not recruit when fleet is at sea"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_OCEAN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 10);
        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        int after = pirates->items.GetNum(I_PIRATES);
        expect(after == before)
            << "pirate crew must not change when fleet is at sea";
    };

    // -----------------------------------------------------------------------
    // Pirates in lake: crew count must NOT change
    // -----------------------------------------------------------------------
    "Pirates do not recruit when fleet is on a lake"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_LAKE;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 10);
        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        int after = pirates->items.GetNum(I_PIRATES);
        expect(after == before)
            << "pirate crew must not change when fleet is on a lake";
    };

    // -----------------------------------------------------------------------
    // Cap: at full ship capacity (Cog = 750/10 = 75) no further recruitment
    // -----------------------------------------------------------------------
    "Pirates do not recruit when at or above cap based on ship capacity"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // Cog capacity = 750, pirate weight = 10 → cap = 75
        Unit *pirates = helper.create_npc_pirate_fleet(r, 75);
        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        int after = pirates->items.GetNum(I_PIRATES);
        expect(after == before)
            << "pirates at ship capacity cap must not recruit any more";
    };

    // -----------------------------------------------------------------------
    // Fleet with captain: pirates unit still recruits (not owner-dependent)
    // -----------------------------------------------------------------------
    "Pirates recruit correctly when fleet also has a captain"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 10);
        Object *fleet  = pirates->object;
        helper.create_npc_pirate_captain(r, fleet);

        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        int after = pirates->items.GetNum(I_PIRATES);
        expect(after > before)
            << "pirates must recruit even when captain is fleet owner";
    };
};
