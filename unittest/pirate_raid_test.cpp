#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Helper: create an empty building (no owner) of given type in region.
// Returns the created Object.
static Object *make_empty_building(ARegion *r, int building_type) {
    Object *obj = new Object(r);
    obj->type = building_type;
    obj->num = r->buildingseq++;
    obj->set_name("Building");
    obj->incomplete = -(ObjectDefs[building_type].maxMaintenance);
    r->objects.push_back(obj);
    return obj;
}

ut::suite<"PirateRaid"> pirate_raid_suite = [] {
    using namespace ut;

    // ---------------------------------------------------------------
    // Basic damage: pirates (moved=0, land region) damage empty farm
    // ---------------------------------------------------------------
    "Pirates damage empty farm when stationary on land"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;  // default region type is R_OCEAN — override for land test

        // 100 pirates, stationary (moved=0 by default)
        Unit *pirates = helper.create_pirate_unit(r, 100);

        Object *farm = make_empty_building(r, O_FARM);
        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->incomplete > initial) << "farm should be damaged";
    };

    // ---------------------------------------------------------------
    // moved > 0: pirates that moved this turn do NOT raid
    // ---------------------------------------------------------------
    "Pirates do not raid if they moved this turn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_pirate_unit(r, 100);
        pirates->moved = 5;  // simulate that they moved this turn

        Object *farm = make_empty_building(r, O_FARM);
        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->incomplete == initial) << "farm must not be damaged when pirates moved this turn";
    };

    // ---------------------------------------------------------------
    // Ocean region: no raid
    // ---------------------------------------------------------------
    "Pirates do not raid in ocean regions"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_OCEAN;

        Unit *pirates = helper.create_pirate_unit(r, 100);

        Object *farm = make_empty_building(r, O_FARM);
        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->incomplete == initial) << "farm must not be damaged in ocean";
    };

    // ---------------------------------------------------------------
    // Lake region: no raid
    // ---------------------------------------------------------------
    "Pirates do not raid in lake regions"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_LAKE;

        Unit *pirates = helper.create_pirate_unit(r, 100);

        Object *farm = make_empty_building(r, O_FARM);
        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->incomplete == initial) << "farm must not be damaged in lake";
    };

    // ---------------------------------------------------------------
    // Unit inside building: building is skipped
    // ---------------------------------------------------------------
    "Pirates skip buildings occupied by units"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_pirate_unit(r, 100);

        // Create a faction and place a unit inside the farm
        Faction *faction = helper.create_faction("Defenders");
        Object *farm = make_empty_building(r, O_FARM);
        Unit *guard = helper.create_unit(faction, r);
        guard->MoveUnit(farm);  // guard is now inside the farm

        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->incomplete == initial) << "occupied farm must not be damaged";
    };

    // ---------------------------------------------------------------
    // Per-turn cap: building takes at most cost/4 damage per turn
    // ---------------------------------------------------------------
    "Pirate raid respects 25% per-turn damage cap"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 1000 pirates guarantees cap is always hit
        Unit *pirates = helper.create_pirate_unit(r, 1000);

        Object *farm = make_empty_building(r, O_FARM);
        int cost = ObjectDefs[O_FARM].cost;   // = 10
        int cap  = cost / 4;                  // = 2
        int initial = farm->incomplete;       // = -maxMaintenance = -5

        helper.run_pirate_raid(r, pirates);

        expect(farm->destroyed <= cap)
            << "o->destroyed must not exceed cost/4 = " << cap;
        expect(farm->incomplete <= initial + cap)
            << "incomplete must not increase by more than " << cap;
    };

    // ---------------------------------------------------------------
    // Low structurePoints: building below 25% threshold is not targeted
    // ---------------------------------------------------------------
    "Pirates do not target buildings below 25% structure points"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_pirate_unit(r, 100);

        Object *farm = make_empty_building(r, O_FARM);
        int cost = ObjectDefs[O_FARM].cost;  // = 10
        // Set structurePoints = cost/4 (at threshold — should be excluded)
        // structurePoints = cost - incomplete => incomplete = cost - cost/4 = 10 - 2 = 8
        farm->incomplete = cost - (cost / 4);

        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->incomplete == initial) << "heavily damaged farm must not be targeted";
    };

    // ---------------------------------------------------------------
    // Road is damaged
    // ---------------------------------------------------------------
    "Pirates damage empty road"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.enable(UnitTestHelper::Type::OBJECT, O_ROADN, true);

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_pirate_unit(r, 100);

        Object *road = make_empty_building(r, O_ROADN);
        int initial = road->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(road->incomplete > initial) << "road should be damaged";
    };

    // ---------------------------------------------------------------
    // Inn is damaged
    // ---------------------------------------------------------------
    "Pirates damage empty inn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_pirate_unit(r, 100);

        Object *inn = make_empty_building(r, O_INN);
        int initial = inn->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(inn->incomplete > initial) << "inn should be damaged";
    };
};
