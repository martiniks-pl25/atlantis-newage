#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// A player unit big enough to start a battle but far too small to wipe the
// pirates out - the tests below need the fleet to survive the fight.
static Unit *weak_attacker(UnitTestHelper &helper, ARegion *r)
{
    Faction *player = helper.create_faction("Attacker");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 2);
    return u;
}

// The shelter branch in combat only engages for a ship whose ObjectDefs entry
// has protect > 0. A Cog has protect 0, so the default pirate fleet would never
// reach the code these tests are about - swap in a Galley (protect 120).
static Object *make_galley_fleet(Unit *pirates)
{
    Object *fleet = pirates->object;
    fleet->SetNumShips(I_COG, 0);
    fleet->AddShip(I_GALLEY);
    fleet->FleetCapacity();

    int obid = lookup_object(ItemDefs[I_GALLEY].name);
    ut::expect(obid >= 0 && ObjectDefs[obid].protect > 0)
        << "a galley must shelter men, or these tests prove nothing";
    return fleet;
}

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

    // -----------------------------------------------------------------------
    // Combat borrows nothing from the fleet: it counts shelter places in
    // shelter_left, so the sailing capacity stays valid throughout the turn.
    // This matters because combat (runorders.cpp:33) runs before recruiting
    // (:88) and the next FleetCapacity() call only comes at :94.
    // -----------------------------------------------------------------------
    "a battle leaves the fleet's sailing capacity untouched"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        Object *fleet = make_galley_fleet(pirates);
        int expected = fleet->FleetCapacity();
        expect(expected > 0_i) << "the galley fleet must have a real capacity to begin with";

        helper.run_battle(r, weak_attacker(helper, r), pirates);

        expect(fleet->capacity == expected)
            << "combat must not repurpose Object::capacity on a fleet";
    };

    "a battle leaves the object still a fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        Object *fleet = make_galley_fleet(pirates);

        helper.run_battle(r, weak_attacker(helper, r), pirates);

        // The sheltering ship is recorded in shelter_type, so the object keeps
        // its own type and still reports as a fleet.
        expect(fleet->type == O_FLEET)
            << "combat must not rewrite the object type to the ship type";
        expect(fleet->IsFleet() == 1_i) << "the object must still be a fleet";
    };

    // Fighting and recruiting both happen in one turn, in that order.
    "pirates recruit in the same turn they fought a battle"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        make_galley_fleet(pirates);

        helper.run_battle(r, weak_attacker(helper, r), pirates);

        int before = pirates->items.GetNum(I_PIRATES);
        if (before < 1) return;   // wiped out; nothing to recruit into

        helper.run_pirate_recruit_land_crew();

        expect(pirates->items.GetNum(I_PIRATES) > before)
            << "a fleet that fought must still recruit that turn";
    };
};
