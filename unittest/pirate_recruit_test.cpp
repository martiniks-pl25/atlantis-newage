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
        r->population = 1000;   // pirates press-gang out of the region's people

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

        r->population = 1000;   // people are available; the cap is what must stop them

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

        r->population = 1000;   // pirates press-gang out of the region's people

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

        r->population = 1000;   // pirates press-gang out of the region's people

        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        make_galley_fleet(pirates);

        helper.run_battle(r, weak_attacker(helper, r), pirates);

        int before = pirates->items.GetNum(I_PIRATES);
        if (before < 1) return;   // wiped out; nothing to recruit into

        helper.run_pirate_recruit_land_crew();

        expect(pirates->items.GetNum(I_PIRATES) > before)
            << "a fleet that fought must still recruit that turn";
    };

    // -----------------------------------------------------------------------
    // Where there is nobody, there is nobody to press-gang.
    // -----------------------------------------------------------------------
    "a region with no people recruits nobody"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        r->population = 0;
        // The test world puts a town in the plains hex, and Population() counts its
        // people too - clear it so the pool below is exactly r->population.
        if (r->town) r->town->pop = 0;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        expect(pirates->items.GetNum(I_PIRATES) == before)
            << "an empty hex must not hand over crew";
    };

    // -----------------------------------------------------------------------
    // A hex gives up one market unit of people per turn - the same pool a
    // player's recruiter draws on. Below the price of a single hand, nothing
    // happens: 49 people are one unit, and one hand costs two of them.
    // -----------------------------------------------------------------------
    "a hamlet that cannot pay for one hand recruits nobody"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        helper.set_ruleset_specific_data(tune);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        r->population = 2 * MEN_PER_MARKET_UNIT - 1;   // one market unit, no more
        // The test world puts a town in the plains hex, and Population() counts its
        // people too - clear it so the pool below is exactly r->population.
        if (r->town) r->town->pop = 0;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        int before = pirates->items.GetNum(I_PIRATES);

        helper.run_pirate_recruit_land_crew();

        expect(pirates->items.GetNum(I_PIRATES) == before)
            << "one market unit cannot pay for a hand that costs two people";
    };

    // -----------------------------------------------------------------------
    // A ship takes on about as many hands per turn as it needs to sail her.
    // A cog needs six, so with a spread of 4 up and 2 down it signs on 5 to 9 -
    // never more, however hungry the crew and however crowded the coast.
    // -----------------------------------------------------------------------
    "one ship takes on about as many hands as it needs to sail"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_recruit_intake_up"] = 4;
        tune["pirate_recruit_intake_down"] = 2;
        tune["pirate_recruit_pop_cost"] = 2;
        helper.set_ruleset_specific_data(tune);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        r->population = 5000;   // never the binding term here
        // The test world puts a town in the plains hex, and Population() counts its
        // people too - clear it so the pool below is exactly r->population.
        if (r->town) r->town->pop = 0;

        // A galleon on purpose: it holds 270 hands but needs only 15 to sail, so a
        // crew of 200 wants 30-50 a turn and the deck is the only thing in the way.
        // On a cog the crew would want 6-12 against a ceiling of 5-9, and the case
        // could pass by luck.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 200);
        Object *fleet = pirates->object;
        fleet->SetNumShips(I_COG, 0);
        fleet->AddShip(I_GALLEON);
        fleet->FleetCapacity();

        int sailors = fleet->GetFleetSize();
        expect(sailors > 0_i) << "the hull must need sailors for this test to mean anything";
        expect(fleet->capacity / ItemDefs[I_PIRATES].weight > 200_i)
            << "and it must have room left, or the gap would be doing the limiting";

        helper.run_pirate_recruit_land_crew();

        int gained = pirates->items.GetNum(I_PIRATES) - 200;
        expect(gained >= 1) << "a crowded coast must yield somebody";
        expect(gained <= sailors + 4 - 1)
            << "intake cannot exceed the sailor requirement plus the upward spread";
    };

    // -----------------------------------------------------------------------
    // One hex, one pool: hulls docked together share what the region gives up,
    // so a stack of ships does not multiply what a village loses.
    // -----------------------------------------------------------------------
    "two fleets in one hex share the same pool"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        helper.set_ruleset_specific_data(tune);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        // 200 people = 8 per turn = 4 hands at two people each, for the whole hex.
        r->population = 200;
        // The test world puts a town in the plains hex, and Population() counts its
        // people too - clear it so the pool below is exactly r->population.
        if (r->town) r->town->pop = 0;

        Unit *first  = helper.create_npc_pirate_fleet(r, 60);
        Unit *second = helper.create_npc_pirate_fleet(r, 60);

        helper.run_pirate_recruit_land_crew();

        int total = (first->items.GetNum(I_PIRATES) - 60)
                  + (second->items.GetNum(I_PIRATES) - 60);
        expect(total >= 1) << "the hex does give somebody up";
        expect(total <= 4)
            << "the hex gives up one pool per turn, not one pool per hull";
    };

    // -----------------------------------------------------------------------
    // The people taken are gone from the region, through the same call a
    // player's men purchase goes through - so both cost a region alike.
    // -----------------------------------------------------------------------
    "press-ganged crew leaves the region's population"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // The test ruleset ships DYNAMIC_POPULATION = 0, which makes
        // ARegion::Recruit() a no-op; the played rulesets have it on.
        int saved = Globals->DYNAMIC_POPULATION;
        Globals->DYNAMIC_POPULATION = 1;

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        helper.set_ruleset_specific_data(tune);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        r->population = 1000;
        // The test world puts a town in the plains hex, and Population() counts its
        // people too - clear it so the pool below is exactly r->population.
        if (r->town) r->town->pop = 0;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        int before_crew = pirates->items.GetNum(I_PIRATES);
        int before_pop  = r->population;

        helper.run_pirate_recruit_land_crew();

        int gained = pirates->items.GetNum(I_PIRATES) - before_crew;
        int lost   = before_pop - r->population;
        expect(gained > 0) << "must have pressed somebody";
        expect(lost == gained * 2 * Globals->RECRUIT_POP_LOSS_PERCENT / 100)
            << "the hex loses two people for every hand the pirates keep";

        Globals->DYNAMIC_POPULATION = saved;
    };
};
