#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// True when the object pointer is still a member of the region's object list.
// Used to observe that a source fleet was deleted without dereferencing it.
static bool object_present(ARegion *r, Object *o)
{
    for (auto x : r->objects) {
        if (x == o) return true;
    }
    return false;
}

// The shelter branch in combat only engages for a ship whose ObjectDefs entry
// has protect > 0. A Cog has protect 0, so the default pirate fleet would never
// be "armoured" - swap in a Galley (protect 120) for the tests that need one.
static Object *make_galley_fleet(Unit *pirates)
{
    Object *fleet = pirates->object;
    fleet->SetNumShips(I_COG, 0);
    fleet->AddShip(I_GALLEY);
    fleet->FleetCapacity();
    return fleet;
}

ut::suite<"PirateSeize"> pirate_seize_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Merge: an empty ship is absorbed into the pirate's own fleet object, the
    // crew is untouched, and the emptied source is deleted.
    // -----------------------------------------------------------------------
    "Pirates merge an empty ship into their fleet when docked on land"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 40 crew on a Cog: crew_cap = 75, so 40 >= 50% x 75 = 37.5 is crowded.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->items.GetNum(I_PIRATES) == 40_i)
            << "a merge must not split or change the crew";
        expect(pirates->object->GetNumShips(I_COG) == 2_i)
            << "the seized Cog must join the pirate's own fleet";
        expect(!object_present(r, empty))
            << "the emptied source fleet must be deleted";
    };

    // -----------------------------------------------------------------------
    // Armour exception: a hull with armour bypasses the fill gate entirely, so
    // an unarmoured, uncrowded fleet still takes a Galley.
    // -----------------------------------------------------------------------
    "An unarmoured fleet seizes an armoured Galley even when not crowded"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 25 crew on a Cog: crew_cap = 75, 25 < 37.5 - not crowded. The Galley's
        // armour must still be taken because the fleet has none of its own.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 25);
        helper.create_empty_fleet(r, I_GALLEY);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_GALLEY) == 1_i)
            << "an armoured hull must bypass the fill gate";
        expect(pirates->items.GetNum(I_PIRATES) == 25_i)
            << "the crew must be unchanged by a merge";
    };

    // -----------------------------------------------------------------------
    // Fill gate: an already-armoured, crowded fleet takes the biggest hull.
    // -----------------------------------------------------------------------
    "A crowded fleet takes the biggest hull"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // Galley fleet: protect 120, crew_cap = 120; 70 crew is crowded.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 70);
        make_galley_fleet(pirates);

        Object *empty_cog     = helper.create_empty_fleet(r, I_COG);
        helper.create_empty_fleet(r, I_GALLEON);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_GALLEON) == 1_i)
            << "a crowded fleet must take the biggest hull, not just any hull";
        expect(empty_cog->GetNumShips(I_COG) == 1_i)
            << "the smaller Cog must be left behind";
    };

    // -----------------------------------------------------------------------
    // Armour beats size: offered both a Galley and a bigger Galleon, an
    // unarmoured fleet takes the Galley first.
    // -----------------------------------------------------------------------
    "A crowded unarmoured fleet offered a Galley and a Galleon takes the Galley"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        helper.create_empty_fleet(r, I_GALLEY);
        Object *empty_galleon = helper.create_empty_fleet(r, I_GALLEON);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_GALLEY) == 1_i)
            << "armour must beat size when the fleet has none of its own";
        expect(empty_galleon->GetNumShips(I_GALLEON) == 1_i)
            << "the bigger Galleon must be left behind";
    };

    // -----------------------------------------------------------------------
    // Speed filter: a Raft (speed 2) is slower than a Cog fleet (speed 4).
    // -----------------------------------------------------------------------
    "A Raft slower than the fleet is refused"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet(r, I_RAFT);

        helper.run_pirate_seize_empty_ships();

        expect(empty->GetNumShips(I_RAFT) == 1_i)
            << "a slower Raft must not be eligible, even for a crowded fleet";
        expect(pirates->object->GetNumShips(I_COG) == 1_i)
            << "the pirate fleet must not have gained anything";
    };

    // -----------------------------------------------------------------------
    // Per-turn cap: an empty fleet holding three Galleons loses exactly one.
    // -----------------------------------------------------------------------
    "Exactly one ship per turn from an empty fleet of three Galleons"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet_multi(r, {I_GALLEON, I_GALLEON, I_GALLEON});

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_GALLEON) == 1_i)
            << "the pirate fleet must gain exactly one Galleon";
        expect(empty->GetNumShips(I_GALLEON) == 2_i)
            << "the source must lose exactly one Galleon";
    };

    // -----------------------------------------------------------------------
    // pirate_seize_max_per_turn = 0: the per-turn cap loop never runs, so a
    // docked fleet with ample crew leaves an eligible empty ship untouched.
    // -----------------------------------------------------------------------
    "pirate_seize_max_per_turn of zero disables seizure"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_seize_max_per_turn"] = 0;
        helper.set_ruleset_specific_data(tune);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_COG) == 1_i)
            << "a zero per-turn cap must leave the pirate fleet's hull count unchanged";
        expect(object_present(r, empty))
            << "the empty source fleet must survive when seizure is disabled";
    };

    // -----------------------------------------------------------------------
    // Crew below the threshold takes nothing.
    // -----------------------------------------------------------------------
    "Pirates below the minimum crew take nothing"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 19);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(object_present(r, empty))
            << "the empty fleet must survive an under-crewed pirate fleet";
        expect(empty->GetNumShips(I_COG) == 1_i)
            << "the empty ship must not be taken";
        expect(pirates->items.GetNum(I_PIRATES) == 19_i)
            << "the pirate crew must be unchanged";
    };

    // -----------------------------------------------------------------------
    // Growth pause (design 8.2): after absorbing the Galley the fleet is now
    // armoured and uncrowded, so a second iteration leaves a Galleon alone.
    // -----------------------------------------------------------------------
    "After absorbing a Galley an uncrowded fleet ignores a Galleon"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Two seizures per turn, so the second iteration re-runs the selection.
        json data;
        data["pirate_seize_max_per_turn"] = 2;
        helper.set_ruleset_specific_data(data);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        helper.create_empty_fleet(r, I_GALLEY);
        Object *empty_galleon = helper.create_empty_fleet(r, I_GALLEON);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_GALLEY) == 1_i)
            << "the armour exception must take the Galley on the first pass";
        expect(pirates->object->GetNumShips(I_GALLEON) == 0_i)
            << "the fleet must not take the Galleon once it is armoured and uncrowded";
        expect(empty_galleon->GetNumShips(I_GALLEON) == 1_i)
            << "the Galleon must stay with its empty fleet";
    };

    // -----------------------------------------------------------------------
    // Ocean: no seizure
    // -----------------------------------------------------------------------
    "Pirates do not seize empty ships in ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_OCEAN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(empty->GetNumShips(I_COG) == 1_i)
            << "empty fleet must not be seized while in ocean";
        expect(pirates->object->GetNumShips(I_COG) == 1_i)
            << "the pirate fleet must gain nothing in ocean";
    };

    // -----------------------------------------------------------------------
    // Lake: no seizure
    // -----------------------------------------------------------------------
    "Pirates do not seize empty ships on a lake"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_LAKE;

        helper.create_npc_pirate_fleet(r, 40);
        Object *empty = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(empty->GetNumShips(I_COG) == 1_i)
            << "empty fleet must not be seized while on a lake";
    };

    // -----------------------------------------------------------------------
    // Elite recognition: the gazette line must come from the fleet object, not
    // the crew unit - an elite fleet keeps its captain in his own unit aboard
    // the fleet, so the crew unit can never report the fleet's elite status.
    // -----------------------------------------------------------------------
    "An elite fleet's seizure names the fleet, not a generic pirate fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *fleet  = pirates->object;
        helper.create_npc_pirate_captain(r, fleet);
        helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        auto &elite = helper.game_object().pirate_context_elite;
        expect(elite.size() == 1_ul)
            << "an elite fleet must write exactly one elite gazette line";
        expect(elite.front().find(fleet->name) != std::string::npos)
            << "the elite line must name the fleet";
        expect(helper.game_object().pirate_context_regular.empty())
            << "an elite fleet must not write a generic gazette line";
    };

    "A plain fleet's seizure reads the generic pirate fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        helper.create_npc_pirate_fleet(r, 40);
        helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        auto &regular = helper.game_object().pirate_context_regular;
        expect(regular.size() == 1_ul)
            << "a plain fleet must write exactly one generic gazette line";
        expect(regular.front().find("A pirate fleet") != std::string::npos)
            << "the regular line must read 'A pirate fleet'";
        expect(helper.game_object().pirate_context_elite.empty())
            << "a plain fleet must not write an elite gazette line";
    };

    // -----------------------------------------------------------------------
    // Offshore seizure: a fleet standing in the water takes an abandoned ship
    // off a land neighbour's coast, and the merged ship joins the pirate fleet
    // that stays in the water.
    // -----------------------------------------------------------------------
    "a fleet in ocean seizes an eligible ship from a land neighbour"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *coast = helper.get_region(0, 2, 0);
        coast->type = R_PLAIN;
        ocean->neighbors[D_NORTH] = coast;

        Unit *pirates = helper.create_npc_pirate_fleet(ocean, 40);  // crowded Cog
        Object *empty  = helper.create_empty_fleet(coast, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_COG) == 2_i)
            << "the seized Cog must join the pirate's fleet, which stays in the water";
        expect(!object_present(coast, empty))
            << "the emptied source fleet must be deleted";
    };

    // -----------------------------------------------------------------------
    // Avoidance gates movement only: an offshore raid on a guarded coast is a
    // deliberate choice, so a player-guarded city does not stop the seizure.
    // -----------------------------------------------------------------------
    "an ocean fleet seizes from a player-guarded city neighbour"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *city = helper.get_region(0, 2, 0);
        city->type = R_PLAIN;
        city->add_town(TOWN_CITY);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, city);
        guard->guard = GUARD_GUARD;
        ocean->neighbors[D_NORTH] = city;

        expect(pirate_avoids_settlement(city))
            << "the setup must genuinely be a guarded city, or this test proves nothing";

        Unit *pirates = helper.create_npc_pirate_fleet(ocean, 40);
        Object *empty  = helper.create_empty_fleet(city, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_COG) == 2_i)
            << "avoidance gates movement only, not an offshore raid";
        expect(!object_present(city, empty))
            << "the ship must be taken from the guarded city's coast";
    };

    // -----------------------------------------------------------------------
    // pirate_seize_offshore = 0 disables the offshore branch entirely.
    // -----------------------------------------------------------------------
    "pirate_seize_offshore of zero disables offshore seizure"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json tune;
        tune["pirate_seize_offshore"] = 0;
        helper.set_ruleset_specific_data(tune);

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        ARegion *coast = helper.get_region(0, 2, 0);
        coast->type = R_PLAIN;
        ocean->neighbors[D_NORTH] = coast;

        Unit *pirates = helper.create_npc_pirate_fleet(ocean, 40);
        Object *empty  = helper.create_empty_fleet(coast, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_COG) == 1_i)
            << "a disabled offshore branch must leave the pirate fleet as it was";
        expect(object_present(coast, empty))
            << "the empty fleet must survive when offshore seizure is off";
    };

    // -----------------------------------------------------------------------
    // Open ocean: a fleet with no land neighbour does nothing (design edge 5.17).
    // -----------------------------------------------------------------------
    "an open-ocean fleet with no land neighbour seizes nothing"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *ocean = helper.get_region(1, 3, 0);
        ocean->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) ocean->neighbors[d] = nullptr;

        Unit *pirates = helper.create_npc_pirate_fleet(ocean, 40);

        // An abandoned fleet sits on land the water region cannot see.
        ARegion *land = helper.get_region(0, 2, 0);
        land->type = R_PLAIN;
        Object *empty = helper.create_empty_fleet(land, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(pirates->object->GetNumShips(I_COG) == 1_i)
            << "a fleet with no land neighbour must seize nothing";
        expect(empty->GetNumShips(I_COG) == 1_i)
            << "the empty fleet out of reach must survive";
    };
};
