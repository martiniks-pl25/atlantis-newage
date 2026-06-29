#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"

#include <sstream>

namespace ut = boost::ut;

// Regression: a unit that moves into a region which has lost its O_DUMMY object
// must NOT be orphaned.
//
// Bug: a dungeon room recycled from a collapsed dungeon had no O_DUMMY (the
// collapse reset cleared all objects but never recreated one, and
// generate_dungeon_cell did not add one). When a unit moved in,
// DoAMoveOrder called MoveUnit(newreg->GetDummy()) with GetDummy() == nullptr,
// detaching the unit from its old object and attaching it to none. The orphan
// (object == nullptr) was never reached by the region->object->unit traversal,
// so it vanished silently on the next save — no death, no battle, no log.
// The dungeon entry room (55,5,4) lost two player units (490, 35) this way.
//
// Fix: the destination must always own a dummy. DoAMoveOrder now recreates one
// when missing (self-heal), and collapse/generate_dungeon_cell/load-time
// migration all keep the invariant. These tests drive the movement path
// directly with a manually dummy-stripped region.

ut::suite<"DungeonDummyOrphan"> dungeon_dummy_orphan_suite = [] {
    using namespace ut;

    // Remove the O_DUMMY object from a region, reproducing the collapsed-and-
    // recycled cell state. Returns true if a dummy was actually removed.
    auto strip_dummy = [](ARegion *r) -> bool {
        for (auto it = r->objects.begin(); it != r->objects.end(); ++it) {
            if ((*it)->type == O_DUMMY) {
                Object *d = *it;
                r->objects.erase(it);
                delete d;
                return true;
            }
        }
        return false;
    };

    // Wire a one-way-and-back passage source -> dest in direction dir.
    auto wire = [](ARegion *src, ARegion *dest, int dir) {
        src->neighbors[dir] = dest;
        dest->neighbors[(dir + 3) % NDIRS] = src;
    };

    // -----------------------------------------------------------------------
    // Test 1: single unit walks into a dummy-less R_DUNGEON room → survives,
    // arrives in the room, and the room gets a fresh dummy.
    // -----------------------------------------------------------------------
    "Unit entering a dummy-less dungeon room is not lost"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *src  = helper.get_region(1, 1, 0);
        ARegion *dest = helper.get_region(0, 2, 0);
        src->type  = R_PLAIN;
        dest->type = R_DUNGEON;
        wire(src, dest, D_SOUTH);

        Faction *f = helper.create_faction("Delvers");
        Unit *u = helper.create_unit(f, src);   // created in src (still has dummy)

        // Reproduce the bug state: dest has no dummy to land in.
        expect(strip_dummy(dest)) << "dest must start with a dummy to remove";
        expect(dest->GetDummy() == nullptr) << "dest must be dummy-less before the move";

        std::stringstream ss;
        ss << "#atlantis " << f->num << "\n";
        ss << "unit " << u->num << "\n";
        ss << "move S\n";
        helper.parse_orders(f->num, ss);
        helper.move_units();

        expect(u->object != nullptr)
            << "unit must not be orphaned (object == nullptr) when entering a dummy-less region";
        expect(u->object != nullptr && u->object->region == dest)
            << "unit must have arrived in the dungeon room";
        expect(dest->GetDummy() != nullptr)
            << "the dungeon room must own a dummy again (self-heal)";
    };

    // -----------------------------------------------------------------------
    // Test 2: the exact live failure — two units enter the SAME dummy-less
    // room in one turn. Both must survive (the first creates the dummy; the
    // second must find it, not be orphaned).
    // -----------------------------------------------------------------------
    "Multiple units entering the same dummy-less room all survive"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *src  = helper.get_region(1, 1, 0);
        ARegion *dest = helper.get_region(0, 2, 0);
        src->type  = R_PLAIN;
        dest->type = R_DUNGEON;
        wire(src, dest, D_SOUTH);

        Faction *f = helper.create_faction("Delvers");
        Unit *u1 = helper.create_unit(f, src);
        Unit *u2 = helper.create_unit(f, src);

        expect(strip_dummy(dest)) << "dest must start with a dummy to remove";

        std::stringstream ss;
        ss << "#atlantis " << f->num << "\n";
        ss << "unit " << u1->num << "\n";
        ss << "move S\n";
        ss << "unit " << u2->num << "\n";
        ss << "move S\n";
        helper.parse_orders(f->num, ss);
        helper.move_units();

        expect(u1->object != nullptr && u1->object->region == dest)
            << "first unit must arrive safely in the dungeon room";
        expect(u2->object != nullptr && u2->object->region == dest)
            << "second unit must arrive safely (not orphaned after the first lands)";
        expect(dest->GetDummy() != nullptr)
            << "the dungeon room must own exactly one shared dummy";
    };

    // -----------------------------------------------------------------------
    // Test 3 (control): a normal move into a region that already has its dummy
    // still works and does not create a duplicate dummy.
    // -----------------------------------------------------------------------
    "Normal move into a region with a dummy is unaffected"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *src  = helper.get_region(1, 1, 0);
        ARegion *dest = helper.get_region(0, 2, 0);
        src->type  = R_PLAIN;
        dest->type = R_PLAIN;
        wire(src, dest, D_SOUTH);

        Faction *f = helper.create_faction("Walkers");
        Unit *u = helper.create_unit(f, src);

        expect(dest->GetDummy() != nullptr) << "dest should already have a dummy";

        std::stringstream ss;
        ss << "#atlantis " << f->num << "\n";
        ss << "unit " << u->num << "\n";
        ss << "move S\n";
        helper.parse_orders(f->num, ss);
        helper.move_units();

        expect(u->object != nullptr && u->object->region == dest)
            << "unit must arrive normally";

        int dummies = 0;
        for (auto o : dest->objects) if (o->type == O_DUMMY) dummies++;
        expect(dummies == 1_i) << "there must be exactly one dummy (no duplicate created)";
    };
};
