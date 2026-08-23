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
        // Open water: with no land neighbour to work, an offshore fleet has
        // nobody to press-gang - the coast is the source, not the water hex.
        for (int d = 0; d < NDIRS; d++) r->neighbors[d] = nullptr;

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
        // Open water: with no land neighbour to work, an offshore fleet has
        // nobody to press-gang - the coast is the source, not the water hex.
        for (int d = 0; d < NDIRS; d++) r->neighbors[d] = nullptr;

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

    // -----------------------------------------------------------------------
    // Elite recognition: the gazette line must come from the fleet object, not
    // the crew unit - an elite fleet keeps its captain in his own unit aboard
    // the fleet, so the crew unit can never report the fleet's elite status.
    // -----------------------------------------------------------------------
    "An elite fleet's recruitment names the fleet, not a generic pirate fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        r->population = 1000;   // pirates press-gang out of the region's people

        Unit *pirates = helper.create_npc_pirate_fleet(r, 10);
        Object *fleet  = pirates->object;
        helper.create_npc_pirate_captain(r, fleet);

        helper.run_pirate_recruit_land_crew();

        auto &elite = helper.game_object().pirate_context_elite;
        expect(elite.size() == 1_ul)
            << "an elite fleet must write exactly one elite gazette line";
        expect(elite.front().find(fleet->name) != std::string::npos)
            << "the elite line must name the fleet";
        expect(helper.game_object().pirate_context_regular.empty())
            << "an elite fleet must not write a generic gazette line";
    };

    "A plain fleet's recruitment reads the generic pirate fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;
        r->population = 1000;   // pirates press-gang out of the region's people

        helper.create_npc_pirate_fleet(r, 10);

        helper.run_pirate_recruit_land_crew();

        auto &regular = helper.game_object().pirate_context_regular;
        expect(regular.size() == 1_ul)
            << "a plain fleet must write exactly one generic gazette line";
        expect(regular.front().find("A pirate fleet") != std::string::npos)
            << "the regular line must read 'A pirate fleet'";
        expect(helper.game_object().pirate_context_elite.empty())
            << "a plain fleet must not write an elite gazette line";
    };

    // -----------------------------------------------------------------------
    // Offshore recruitment: a fleet standing in the water works the richest
    // land neighbour and takes half of what a docked fleet would get there.
    // The tests pin the ruleset tuning (pop_cost 2, intake spread 1/1, offshore
    // 50%) and pick populations whose supply (allowance / pop_cost) sits below
    // both the crew's demand (15-25% of 40 = 6-10) and the Cog's ceiling
    // (GetFleetSize() = 6 at the 1/1 spread), so supply is the binding term and
    // the intake is a fixed value without pinning RNG.
    // -----------------------------------------------------------------------
    "offshore intake is the halved land intake for the same fleet and population"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Pin the tuning the played rulesets ship (neworigins/extra.cpp) so the
        // unittest defaults don't leak in: pop_cost 2, intake spread 1/1, and the
        // offshore half. At 1/1 the ceiling is exactly GetFleetSize() (rng(1) is
        // always 0), and supply below demand and ceiling keeps the intake fixed.
        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        tune["pirate_recruit_intake_up"] = 1;
        tune["pirate_recruit_intake_down"] = 1;
        tune["pirate_recruit_offshore_pct"] = 50;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *coast = helper.get_region(1, 1, 0);
        coast->type = R_PLAIN;
        coast->population = 100;   // allowance 4, supply 2 at pop_cost 2
        if (coast->town) coast->town->pop = 0;
        ocean->neighbors[D_NORTH] = coast;

        ARegion *land = helper.get_region(0, 2, 0);
        land->type = R_PLAIN;
        land->population = 100;    // the same pool for the docked fleet
        if (land->town) land->town->pop = 0;

        Unit *docked   = helper.create_npc_pirate_fleet(land, 40);
        Unit *offshore = helper.create_npc_pirate_fleet(ocean, 40);

        helper.run_pirate_recruit_land_crew();

        int docked_gained   = docked->items.GetNum(I_PIRATES) - 40;
        int offshore_gained = offshore->items.GetNum(I_PIRATES) - 40;
        expect(docked_gained == 2) << "the docked fleet takes the full supply of two";
        expect(offshore_gained == 1) << "the offshore fleet takes half the docked intake";
    };

    // -----------------------------------------------------------------------
    // Half of one hand is nobody: an intake of 1 on land halves to 0 offshore.
    // -----------------------------------------------------------------------
    "an intake of one hand on land yields nothing offshore"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        tune["pirate_recruit_intake_up"] = 1;
        tune["pirate_recruit_intake_down"] = 1;
        tune["pirate_recruit_offshore_pct"] = 50;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *coast = helper.get_region(1, 1, 0);
        coast->type = R_PLAIN;
        coast->population = 50;    // allowance 2, supply 1 at pop_cost 2
        if (coast->town) coast->town->pop = 0;
        ocean->neighbors[D_NORTH] = coast;

        ARegion *land = helper.get_region(0, 2, 0);
        land->type = R_PLAIN;
        land->population = 50;     // the same pool for the docked fleet
        if (land->town) land->town->pop = 0;

        Unit *docked   = helper.create_npc_pirate_fleet(land, 40);
        Unit *offshore = helper.create_npc_pirate_fleet(ocean, 40);

        helper.run_pirate_recruit_land_crew();

        int docked_gained   = docked->items.GetNum(I_PIRATES) - 40;
        int offshore_gained = offshore->items.GetNum(I_PIRATES) - 40;
        expect(docked_gained == 1) << "the docked fleet signs on the one hand available";
        expect(offshore_gained == 0) << "half of one hand is nobody";
    };

    // -----------------------------------------------------------------------
    // The argmax: of several land neighbours, the one with the most allowance
    // left is worked; the others keep their people.
    // -----------------------------------------------------------------------
    "with two coasts the richer one is chosen"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->DYNAMIC_POPULATION;
        Globals->DYNAMIC_POPULATION = 1;

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        tune["pirate_recruit_intake_up"] = 1;
        tune["pirate_recruit_intake_down"] = 1;
        tune["pirate_recruit_offshore_pct"] = 50;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *rich = helper.get_region(0, 2, 0);
        rich->type = R_PLAIN;
        rich->population = 250;    // allowance 10 = supply 5 at pop_cost 2
        if (rich->town) rich->town->pop = 0;
        ocean->neighbors[D_NORTH] = rich;

        ARegion *poor = helper.get_region(1, 1, 0);
        poor->type = R_PLAIN;
        poor->population = 100;    // allowance 4
        if (poor->town) poor->town->pop = 0;
        ocean->neighbors[D_SOUTH] = poor;

        Unit *offshore = helper.create_npc_pirate_fleet(ocean, 40);
        int rich_before = rich->population;

        helper.run_pirate_recruit_land_crew();

        expect(offshore->items.GetNum(I_PIRATES) > 40) << "the fleet must recruit somebody";
        expect(rich->population < rich_before) << "the richer coast loses people";
        expect(poor->population == 100) << "the poorer coast is left alone";

        Globals->DYNAMIC_POPULATION = saved;
    };

    // -----------------------------------------------------------------------
    // Stacking is allowed: two offshore fleets draw from one coast in one turn,
    // each seeing the pool the previous one left.
    // -----------------------------------------------------------------------
    "two offshore fleets on one coast share the same pool"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->DYNAMIC_POPULATION;
        Globals->DYNAMIC_POPULATION = 1;

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        tune["pirate_recruit_intake_up"] = 1;
        tune["pirate_recruit_intake_down"] = 1;
        tune["pirate_recruit_offshore_pct"] = 50;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *coast = helper.get_region(0, 2, 0);
        coast->type = R_PLAIN;
        coast->population = 250;   // allowance 10 = supply 5 at pop_cost 2
        if (coast->town) coast->town->pop = 0;
        ocean->neighbors[D_NORTH] = coast;

        Unit *first  = helper.create_npc_pirate_fleet(ocean, 40);
        Unit *second = helper.create_npc_pirate_fleet(ocean, 40);
        int coast_before = coast->population;

        helper.run_pirate_recruit_land_crew();

        // Supply is the binding term in both passes: the first fleet sees supply
        // 5 (below the 6-10 demand and the 6 ceiling), keeps 5*50/100 = 2 and
        // spends 4 people; the second sees 6 people left = supply 3, keeps
        // 3*50/100 = 1 and spends 2. The coast loses 4 + 2 = 6 people in all.
        int first_gained  = first->items.GetNum(I_PIRATES) - 40;
        int second_gained = second->items.GetNum(I_PIRATES) - 40;
        expect(first_gained == 2) << "the first fleet keeps half of the supply of five";
        expect(second_gained == 1) << "the second fleet sees the pool the first left";
        expect(coast_before - coast->population == 6)
            << "the coast loses two people for each of the three hands kept";

        Globals->DYNAMIC_POPULATION = saved;
    };

    // -----------------------------------------------------------------------
    // Order of service: a fleet that took the risk of landing is served before
    // one that stands offshore.
    // -----------------------------------------------------------------------
    "a docked fleet is served before an offshore fleet on the same hex"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        tune["pirate_recruit_intake_up"] = 1;
        tune["pirate_recruit_intake_down"] = 1;
        tune["pirate_recruit_offshore_pct"] = 50;
        helper.set_ruleset_specific_data(tune);

        ARegion *coast = helper.get_region(0, 2, 0);
        coast->type = R_PLAIN;
        coast->population = 100;   // allowance 4, supply 2
        if (coast->town) coast->town->pop = 0;

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;
        ocean->neighbors[D_NORTH] = coast;

        Unit *docked   = helper.create_npc_pirate_fleet(coast, 40);
        Unit *offshore = helper.create_npc_pirate_fleet(ocean, 40);

        helper.run_pirate_recruit_land_crew();

        int docked_gained   = docked->items.GetNum(I_PIRATES) - 40;
        int offshore_gained = offshore->items.GetNum(I_PIRATES) - 40;
        expect(docked_gained == 2) << "the docked fleet takes the full supply of two";
        expect(offshore_gained == 0) << "the offshore fleet finds the pool already gone";
    };

    // -----------------------------------------------------------------------
    // Zero disables offshore recruitment entirely - there is no separate boolean.
    // -----------------------------------------------------------------------
    "pirate_recruit_offshore_pct of zero disables offshore recruitment"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_recruit_offshore_pct"] = 0;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *coast = helper.get_region(0, 2, 0);
        coast->type = R_PLAIN;
        coast->population = 1000;
        ocean->neighbors[D_NORTH] = coast;

        Unit *offshore = helper.create_npc_pirate_fleet(ocean, 40);

        helper.run_pirate_recruit_land_crew();

        expect(offshore->items.GetNum(I_PIRATES) == 40_i)
            << "a zero offshore percentage must disable offshore recruitment";
    };

    // -----------------------------------------------------------------------
    // Equal coasts break ties deterministically to the lowest direction index.
    // -----------------------------------------------------------------------
    "equal coasts break ties to the lowest direction index"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->DYNAMIC_POPULATION;
        Globals->DYNAMIC_POPULATION = 1;

        json tune;
        tune["pirate_recruit_pop_cost"] = 2;
        tune["pirate_recruit_intake_up"] = 1;
        tune["pirate_recruit_intake_down"] = 1;
        tune["pirate_recruit_offshore_pct"] = 50;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *north = helper.get_region(0, 2, 0);
        north->type = R_PLAIN;
        north->population = 100;   // allowance 4, equal to the other coast
        if (north->town) north->town->pop = 0;
        ocean->neighbors[D_NORTH] = north;

        ARegion *south = helper.get_region(1, 1, 0);
        south->type = R_PLAIN;
        south->population = 100;   // allowance 4, equal to the other coast
        if (south->town) south->town->pop = 0;
        ocean->neighbors[D_SOUTH] = south;

        helper.create_npc_pirate_fleet(ocean, 40);
        int north_before = north->population;

        helper.run_pirate_recruit_land_crew();

        expect(north->population < north_before) << "the tie goes to the lowest direction index";
        expect(south->population == 100) << "the other equal coast is left alone";

        Globals->DYNAMIC_POPULATION = saved;
    };
};
