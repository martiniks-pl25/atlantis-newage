#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Tests for the Canal infrastructure building (O_CANAL stone / O_MCANAL rootstone).
//
// A canal lets a fleet sail through a coastal land region (an isthmus) in any
// direction that leads to ocean, bypassing the PREVENT_SAIL_THROUGH directional
// restriction in Object::SailThroughCheck(). Without a canal, a fleet that entered
// a land region may only continue back the way it came or turn to an adjacent
// ocean hex; a straight through-pass (prevdir + 3) is blocked.
//
// The two tiers differ only in the movepoint cost charged for the through-pass
// (resolved in Do1SailOrder by build material): stone = 2, rootstone = 1. That
// data wiring is verified here via the ObjectDefs table; the directional bypass
// itself is verified by driving SailThroughCheck directly.

ut::suite<"CanalMovement"> canal_movement_suite = [] {
    using namespace ut;

    // -------------------------------------------------------------------
    // SailThroughCheck: straight through-pass across an isthmus.
    // mid is land; north & south neighbors are ocean. Fleet entered from the
    // north (prevdir = D_NORTH) and wants to continue south (D_SOUTH = prevdir+3),
    // which is neither "backward" nor an adjacent turn → blocked without a canal.
    // -------------------------------------------------------------------

    auto setup_isthmus = [&](UnitTestHelper &helper) -> Object * {
        // SailThroughCheck only inspects mid->neighbors[dir] and their terrain,
        // so the neighbour regions need not be geographically adjacent — we reuse
        // three known-valid test-world hexes and wire the passage by hand.
        ARegion *mid   = helper.get_region(0, 2, 0);
        ARegion *north = helper.get_region(1, 1, 0);
        ARegion *south = helper.get_region(0, 0, 0);
        mid->type   = R_PLAIN;
        north->type = R_OCEAN;
        south->type = R_OCEAN;

        // Wire only the north/south passage; seal the rest so geometry is exact.
        for (int d = 0; d < NDIRS; d++) mid->neighbors[d] = nullptr;
        mid->neighbors[D_NORTH] = north;
        mid->neighbors[D_SOUTH] = south;

        Faction *f = helper.create_faction("Sailors");
        Unit *u = helper.create_unit(f, mid);
        helper.create_fleet(mid, u, O_LONGSHIP, 1);
        Object *fleet = nullptr;
        for (auto o : mid->objects) if (o->type == O_FLEET) fleet = o;
        fleet->flying = 0;
        fleet->SetPrevDir(D_NORTH);   // entered from the north
        return fleet;
    };

    "Fleet cannot sail straight through an isthmus without a canal"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->PREVENT_SAIL_THROUGH;
        Globals->PREVENT_SAIL_THROUGH = 1;

        Object *fleet = setup_isthmus(helper);
        expect(fleet != nullptr) << "fleet must exist";
        expect(that % fleet->SailThroughCheck(D_SOUTH) == 0)
            << "without a canal, a straight through-pass must be blocked";

        Globals->PREVENT_SAIL_THROUGH = saved;
    };

    "Stone canal lets a fleet sail straight through an isthmus"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->PREVENT_SAIL_THROUGH;
        Globals->PREVENT_SAIL_THROUGH = 1;

        Object *fleet = setup_isthmus(helper);
        ARegion *mid = fleet->region;
        helper.create_building(mid, nullptr, O_CANAL);

        expect(that % fleet->SailThroughCheck(D_SOUTH) == 1)
            << "a completed stone canal must allow the through-pass";

        Globals->PREVENT_SAIL_THROUGH = saved;
    };

    "Mystic canal lets a fleet sail straight through an isthmus"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->PREVENT_SAIL_THROUGH;
        Globals->PREVENT_SAIL_THROUGH = 1;

        Object *fleet = setup_isthmus(helper);
        ARegion *mid = fleet->region;
        helper.create_building(mid, nullptr, O_MCANAL);

        expect(that % fleet->SailThroughCheck(D_SOUTH) == 1)
            << "a completed mystic canal must allow the through-pass";

        Globals->PREVENT_SAIL_THROUGH = saved;
    };

    "Incomplete canal does not allow the through-pass"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->PREVENT_SAIL_THROUGH;
        Globals->PREVENT_SAIL_THROUGH = 1;

        Object *fleet = setup_isthmus(helper);
        ARegion *mid = fleet->region;
        helper.create_building(mid, nullptr, O_CANAL);
        // create_building returns void; find the canal and mark it under construction.
        Object *canal = nullptr;
        for (auto o : mid->objects) if (o->type == O_CANAL) canal = o;
        expect(canal != nullptr) << "canal object must exist";
        canal->incomplete = 100;   // under construction (incomplete > 0)

        expect(that % fleet->SailThroughCheck(D_SOUTH) == 0)
            << "an unfinished canal must not bypass the isthmus restriction";

        Globals->PREVENT_SAIL_THROUGH = saved;
    };

    "Fleet may still sail backward without a canal (regression)"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->PREVENT_SAIL_THROUGH;
        Globals->PREVENT_SAIL_THROUGH = 1;

        Object *fleet = setup_isthmus(helper);
        // prevdir == dir is always allowed (sail back the way it came).
        expect(that % fleet->SailThroughCheck(D_NORTH) == 1)
            << "sailing back the way it came must always be allowed";

        Globals->PREVENT_SAIL_THROUGH = saved;
    };

    // -------------------------------------------------------------------
    // Cost-fix invariant: an ordinary allowed exit from a canal region must
    // NOT be charged the canal through-pass cost. Do1SailOrder applies the
    // canal cost only when the exit would be blocked without a canal; this
    // mirrors SailThroughCheck's "backward or adjacent-ocean turn" allowance.
    // We verify the predicate those exits rely on returns 1 even with NO canal,
    // so the cost branch (gated by !allowed_without_canal) is skipped for them.
    // -------------------------------------------------------------------

    "Adjacent-ocean turn out of a coastal region is allowed without a canal"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->PREVENT_SAIL_THROUGH;
        Globals->PREVENT_SAIL_THROUGH = 1;

        // Land region entered from the north (prevdir = D_NORTH); the NORTHEAST
        // neighbour (adjacent to entry, d1 = prevdir+1) is ocean. Turning into it
        // is a normal allowed exit — no canal needed, hence no canal cost.
        ARegion *mid  = helper.get_region(0, 2, 0);
        ARegion *from = helper.get_region(1, 1, 0);
        ARegion *side = helper.get_region(0, 0, 0);
        mid->type  = R_PLAIN;
        from->type = R_OCEAN;
        side->type = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) mid->neighbors[d] = nullptr;
        mid->neighbors[D_NORTH]     = from;
        mid->neighbors[D_NORTHEAST] = side;

        Faction *f = helper.create_faction("Sailors");
        Unit *u = helper.create_unit(f, mid);
        helper.create_fleet(mid, u, O_LONGSHIP, 1);
        Object *fleet = nullptr;
        for (auto o : mid->objects) if (o->type == O_FLEET) fleet = o;
        fleet->flying = 0;
        fleet->SetPrevDir(D_NORTH);

        expect(that % fleet->SailThroughCheck(D_NORTHEAST) == 1)
            << "an adjacent-ocean turn must be allowed without a canal "
               "(so the canal cost branch is skipped for it)";

        Globals->PREVENT_SAIL_THROUGH = saved;
    };

    // -------------------------------------------------------------------
    // ObjectDefs data integrity: tier wiring (cost/material/skill/flags).
    // The per-pass movepoint cost in Do1SailOrder reads the build material:
    // rootstone (IT_ADVANCED) -> cost 1, stone -> cost 2.
    // -------------------------------------------------------------------

    "Stone canal ObjectDef is wired correctly"_test = [] {
        expect(that % (ObjectDefs[O_CANAL].flags & ObjectType::CANAL) != 0)
            << "O_CANAL must carry the CANAL flag";
        expect(that % ObjectDefs[O_CANAL].item == I_STONE) << "stone canal material";
        expect(that % ObjectDefs[O_CANAL].cost == 300) << "stone canal cost";
        expect(that % ObjectDefs[O_CANAL].level == 4) << "stone canal BUIL level";
        // Stone is not an advanced resource -> through-pass cost 2.
        expect(that % (ItemDefs[I_STONE].type & IT_ADVANCED) == 0)
            << "stone must not be IT_ADVANCED (drives pass cost 2)";
    };

    "Mystic canal ObjectDef is wired correctly"_test = [] {
        expect(that % (ObjectDefs[O_MCANAL].flags & ObjectType::CANAL) != 0)
            << "O_MCANAL must carry the CANAL flag";
        expect(that % ObjectDefs[O_MCANAL].item == I_ROOTSTONE) << "mystic canal material";
        expect(that % ObjectDefs[O_MCANAL].cost == 150) << "mystic canal cost";
        expect(that % ObjectDefs[O_MCANAL].level == 5) << "mystic canal BUIL level";
        // Rootstone is advanced -> through-pass cost 1.
        expect(that % (ItemDefs[I_ROOTSTONE].type & IT_ADVANCED) != 0)
            << "rootstone must be IT_ADVANCED (drives pass cost 1)";
    };
};
