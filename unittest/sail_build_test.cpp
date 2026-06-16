#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "gamedefs.h"
#include "aregion.h"
#include "object.h"
#include "orders.h"
#include "testhelper.hpp"
#include "../rng.hpp"

namespace ut = boost::ut;

// Regression: a builder boards a ship as a passenger and issues BUILD (a road),
// the fleet captain issues SAIL, and the fleet sails to a neighbouring region.
// At the end of the turn the road must be started in the DESTINATION region, not
// in the origin. Movement (SAIL) runs before the month-long BUILD phase, so the
// passenger is carried to the destination and AddNewBuildings/Run1BuildOrder must
// create and progress the structure where the unit actually ends up.
//
// Test world: default 2x4 wrap hex world.
//   (0,0,0) = R_OCEAN (fleet start). SE from (0,0,0) leads to (1,1,0).
//   (1,1,0) = R_PLAIN, coastal (adjacent to the ocean), buildable.

ut::suite<"SailBuild"> sail_build_suite = [] {
    using namespace ut;

    "Passenger builder sails to a neighbour and starts a road there"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *ocean = helper.get_region(0, 0, 0);
        ocean->type = R_OCEAN;
        ARegion *dest = helper.get_region(1, 1, 0);
        dest->type = R_PLAIN;

        // Geometry sanity: SE from the ocean must reach the destination.
        expect(ocean->neighbors[D_SOUTHEAST] == dest)
            << "test world geometry: SE from (0,0,0) must reach (1,1,0)";

        Faction *f = helper.create_faction("Sailors");

        // Captain owns the raft and crews it (SAIL). High sailing skill + men so
        // the sailor count / fleet speed checks pass comfortably.
        Unit *captain = helper.create_unit(f, ocean);
        helper.create_fleet(ocean, captain, I_RAFT, 1);
        Object *fleet = nullptr;
        for (auto o : ocean->objects) if (o->type == O_FLEET) fleet = o;
        expect(fleet != nullptr) << "fleet must exist";

        captain->SetMen(I_LEADERS, 2);
        captain->SetSkill(S_SAILING, 5);
        SailOrder *so = new SailOrder;
        MoveDir *md = new MoveDir;
        md->dir = D_SOUTHEAST;
        so->dirs.push_back(md);
        captain->monthorders = so;

        // Builder boards as a passenger and orders BUILD <road>, carrying stone.
        // Build power (men*skill) and stone are kept small so the road is started
        // but not finished — proving construction began this turn.
        Unit *builder = helper.create_unit(f, ocean);
        builder->MoveUnit(fleet);
        builder->SetMen(I_LEADERS, 5);
        // Use a skill level that clears the road's required build level whether or
        // not the ruleset's ModifyObjectConstruction (BUIL 2) was applied — the
        // base gamedata road requires BUIL 3.
        builder->SetSkill(S_BUILDING, 5);
        builder->items.SetNum(I_STONE, 5);
        BuildOrder *bo = new BuildOrder;
        bo->new_building = O_ROADN;
        builder->monthorders = bo;

        // Run the turn order: movement (SAIL) then month-long orders (BUILD).
        helper.move_units();
        helper.run_month_orders();

        // 1) Fleet and the passenger builder reached the destination.
        expect(fleet->region == dest)
            << "fleet must have sailed from the ocean to the neighbouring region";
        expect(builder->object->region == dest)
            << "builder must end the turn in the destination region";

        // 2) A road was started in the destination, not yet complete.
        int dest_roads = 0;
        int dest_incomplete = -1;
        for (auto o : dest->objects)
            if (o->type == O_ROADN) { dest_roads++; dest_incomplete = o->incomplete; }
        int road_cost = ObjectDefs[O_ROADN].cost;
        expect(that % dest_roads == 1)
            << "exactly one road object must exist in the destination";
        expect(that % dest_incomplete > 0)
            << "road must be started but not finished";
        expect(that % dest_incomplete < road_cost)
            << "construction progress must have been made on the road";

        // 3) Nothing was built in the origin (ocean) region.
        int origin_roads = 0;
        for (auto o : ocean->objects) if (o->type == O_ROADN) origin_roads++;
        expect(that % origin_roads == 0)
            << "no road must be built in the origin region";
    };
};
