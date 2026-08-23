#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "orders.h"
#include "testhelper.hpp"
#include "../rng.hpp"

namespace ut = boost::ut;

// Tests that COASTAL_ONLY pirates (I_PIRATES, I_PIRATE_BOSUN, I_PIRATE_CAPTAIN,
// I_PIRATE_KING) do not move when placed inside R_DUNGEON rooms.
//
// Without the fix in unit.cpp (isLost check skips R_DUNGEON), COASTAL_ONLY
// monsters enter escape mode and wander via Priority-3 fallback to any reachable
// neighbor — destroying carefully placed hideout composition.
//
// With the fix, they enter NORMAL MODE, but the COASTAL_ONLY direction filter
// discards all R_DUNGEON neighbors (non-coastal), leaving only stay entries.
// The stay-cap code then removes them too (move_count == 0 → cap loop removes
// all -1 entries → directions is empty → DefaultOrders returns without setting
// monthorders). Observable result: monthorders == nullptr in all cases.

ut::suite<"PirateDungeonMovement"> pirate_dungeon_movement_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Helper: create a wandering monster unit of the given item type in region.
    // Uses the public create_monster() helper from UnitTestHelper.
    // -----------------------------------------------------------------------
    auto make_wmon = [](UnitTestHelper &helper, ARegion *r, int item) -> Unit * {
        Unit *u = helper.create_monster(r, item, 5);
        u->guard = GUARD_NONE;
        return u;
    };

    // -----------------------------------------------------------------------
    // Test 1: I_PIRATES in isolated R_DUNGEON room (no neighbors) → stays.
    // -----------------------------------------------------------------------
    "Pirates do not move in isolated R_DUNGEON room"_test = [&make_wmon] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);
        room->type = R_DUNGEON;
        // Seal all neighbors (isolated room).
        for (int d = 0; d < NDIRS; d++) room->neighbors[d] = nullptr;

        Unit *u = make_wmon(helper, room, I_PIRATES);

        for (int seed = 0; seed < 20; seed++) {
            rng::seed_random(seed);
            u->ClearOrders();
            u->DefaultOrders(u->object);
            expect(u->monthorders == nullptr)
                << "pirates must not generate a move order in isolated R_DUNGEON (seed=" << seed << ")";
        }
    };

    // -----------------------------------------------------------------------
    // Test 2: I_PIRATES in R_DUNGEON room connected to another dungeon room
    //         via passage → still stays (COASTAL_ONLY filter discards dungeon neighbor).
    // -----------------------------------------------------------------------
    "Pirates do not move in R_DUNGEON room with adjacent dungeon passage"_test = [&make_wmon] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room1 = helper.get_region(0, 2, 0);
        ARegion *room2 = helper.get_region(1, 1, 0);
        room1->type = R_DUNGEON;
        room2->type = R_DUNGEON;

        // Seal all, then open one passage between the two dungeon rooms.
        for (int d = 0; d < NDIRS; d++) { room1->neighbors[d] = nullptr; room2->neighbors[d] = nullptr; }
        room1->neighbors[D_SOUTH]  = room2;
        room2->neighbors[D_NORTH]  = room1;

        Unit *u = make_wmon(helper, room1, I_PIRATES);

        for (int seed = 0; seed < 20; seed++) {
            rng::seed_random(seed);
            u->ClearOrders();
            u->DefaultOrders(u->object);
            expect(u->monthorders == nullptr)
                << "pirates must not move into adjacent dungeon room (seed=" << seed << ")";
        }
    };

    // -----------------------------------------------------------------------
    // Test 3: I_PIRATE_BOSUN in R_DUNGEON room with passage → stays.
    // -----------------------------------------------------------------------
    "Pirate Bosun does not move in R_DUNGEON room with adjacent dungeon passage"_test = [&make_wmon] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room1 = helper.get_region(0, 2, 0);
        ARegion *room2 = helper.get_region(1, 1, 0);
        room1->type = R_DUNGEON;
        room2->type = R_DUNGEON;
        for (int d = 0; d < NDIRS; d++) { room1->neighbors[d] = nullptr; room2->neighbors[d] = nullptr; }
        room1->neighbors[D_SOUTH] = room2;
        room2->neighbors[D_NORTH] = room1;

        Unit *u = make_wmon(helper, room1, I_PIRATE_BOSUN);

        for (int seed = 0; seed < 20; seed++) {
            rng::seed_random(seed);
            u->ClearOrders();
            u->DefaultOrders(u->object);
            expect(u->monthorders == nullptr)
                << "pirate bosun must not move in R_DUNGEON (seed=" << seed << ")";
        }
    };

    // -----------------------------------------------------------------------
    // Test 4: I_PIRATE_CAPTAIN in R_DUNGEON room with passage → stays.
    // -----------------------------------------------------------------------
    "Pirate Captain does not move in R_DUNGEON room with adjacent dungeon passage"_test = [&make_wmon] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room1 = helper.get_region(0, 2, 0);
        ARegion *room2 = helper.get_region(1, 1, 0);
        room1->type = R_DUNGEON;
        room2->type = R_DUNGEON;
        for (int d = 0; d < NDIRS; d++) { room1->neighbors[d] = nullptr; room2->neighbors[d] = nullptr; }
        room1->neighbors[D_SOUTH] = room2;
        room2->neighbors[D_NORTH] = room1;

        Unit *u = make_wmon(helper, room1, I_PIRATE_CAPTAIN);

        for (int seed = 0; seed < 20; seed++) {
            rng::seed_random(seed);
            u->ClearOrders();
            u->DefaultOrders(u->object);
            expect(u->monthorders == nullptr)
                << "pirate captain must not move in R_DUNGEON (seed=" << seed << ")";
        }
    };

    // -----------------------------------------------------------------------
    // Test 5: I_PIRATE_KING in R_DUNGEON room with passage → stays.
    // -----------------------------------------------------------------------
    "Pirate King does not move in R_DUNGEON room with adjacent dungeon passage"_test = [&make_wmon] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room1 = helper.get_region(0, 2, 0);
        ARegion *room2 = helper.get_region(1, 1, 0);
        room1->type = R_DUNGEON;
        room2->type = R_DUNGEON;
        for (int d = 0; d < NDIRS; d++) { room1->neighbors[d] = nullptr; room2->neighbors[d] = nullptr; }
        room1->neighbors[D_SOUTH] = room2;
        room2->neighbors[D_NORTH] = room1;

        Unit *u = make_wmon(helper, room1, I_PIRATE_KING);

        for (int seed = 0; seed < 20; seed++) {
            rng::seed_random(seed);
            u->ClearOrders();
            u->DefaultOrders(u->object);
            expect(u->monthorders == nullptr)
                << "pirate king must not move in R_DUNGEON (seed=" << seed << ")";
        }
    };

    // -----------------------------------------------------------------------
    // Test 6: Regression — I_PIRATES in R_OCEAN still moves normally.
    //         Run 30 seeds; at least one must produce a non-null monthorders.
    // -----------------------------------------------------------------------
    "Pirates still move normally in R_OCEAN (regression)"_test = [&make_wmon] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean  = helper.get_region(0, 0, 0);
        ARegion *r_ocean2 = helper.get_region(1, 1, 0);
        r_ocean->type  = R_OCEAN;
        r_ocean2->type = R_OCEAN;

        Unit *u = make_wmon(helper, r_ocean, I_PIRATES);

        bool saw_move = false;
        for (int seed = 0; seed < 30 && !saw_move; seed++) {
            rng::seed_random(seed);
            u->ClearOrders();
            u->DefaultOrders(u->object);
            if (u->monthorders != nullptr) saw_move = true;
        }
        expect(saw_move) << "pirates must still generate move orders on R_OCEAN";
    };
};

// ---------------------------------------------------------------------------
// Kraken attraction: an elite pirate fleet (a named captain aboard) draws a
// kraken more strongly than a plain crew. In Unit::DefaultOrders each water
// neighbor holding an NPC pirate fleet adds direction weight — +2 for an elite
// fleet, +1 for a plain one — so over many seeded draws the elite fleet must
// win more often. The elite test is the fleet_has_captain predicate, the same
// one recruitment, seizure and the building raid all use.
// ---------------------------------------------------------------------------
ut::suite<"KrakenPirateAttraction"> kraken_pirate_attraction_suite = [] {
    using namespace ut;

    "Kraken is drawn to an elite fleet more strongly than to a plain fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_kraken = helper.get_region(0, 0, 0);
        ARegion *r_elite  = helper.get_region(1, 1, 0);
        ARegion *r_plain  = helper.get_region(2, 2, 0);
        r_kraken->type = R_OCEAN;
        r_elite->type  = R_OCEAN;
        r_plain->type  = R_OCEAN;

        for (ARegion *r : { r_kraken, r_elite, r_plain })
            for (int d = 0; d < NDIRS; d++) r->neighbors[d] = nullptr;
        r_kraken->neighbors[D_NORTH] = r_elite;
        r_elite->neighbors[D_SOUTH]  = r_kraken;
        r_kraken->neighbors[D_SOUTH] = r_plain;
        r_plain->neighbors[D_NORTH]  = r_kraken;

        // Drop any world-gen NPC units so only the fleets built below weigh on
        // the kraken; a stray pre-existing fleet in either hex would skew the draw.
        helper.clear_npc_units_in_region(r_elite);
        helper.clear_npc_units_in_region(r_plain);

        // Plain fleet to the south; elite fleet (crew plus captain) to the north.
        helper.create_npc_pirate_fleet(r_plain, 3);
        Unit *elite_crew = helper.create_npc_pirate_fleet(r_elite, 3);
        helper.create_npc_pirate_captain(r_elite, elite_crew->object);

        Unit *kraken = helper.create_monster(r_kraken, I_KRAKEN, 1);
        kraken->guard = GUARD_NONE;

        int elite_choices = 0;
        int plain_choices = 0;
        int moves = 0;
        for (int seed = 0; seed < 500; seed++) {
            rng::seed_random(seed);
            kraken->ClearOrders();
            kraken->DefaultOrders(kraken->object);
            auto *mo = dynamic_cast<MoveOrder *>(kraken->monthorders);
            if (!mo || mo->dirs.empty()) continue;
            moves++;
            if (mo->dirs.front()->dir == D_NORTH) elite_choices++;
            else if (mo->dirs.front()->dir == D_SOUTH) plain_choices++;
        }

        expect(that % moves > 0)
            << "a kraken with pirate fleets on both sides must always pick a direction";
        expect(that % elite_choices > 0) << "the elite fleet must attract the kraken";
        expect(that % plain_choices > 0) << "a plain fleet must still attract the kraken";
        expect(that % elite_choices > plain_choices)
            << "the elite fleet's captain must draw the kraken more strongly than a plain crew";
    };
};
