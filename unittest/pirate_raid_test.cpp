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
    // Per-turn cap (functional): cap = maxMaintenance+1, disables in one raid
    // ---------------------------------------------------------------
    "Pirate raid disables functional building in one raid"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 1000 pirates guarantees cap is always hit
        Unit *pirates = helper.create_pirate_unit(r, 1000);

        Object *farm = make_empty_building(r, O_FARM);
        int cap     = ObjectDefs[O_FARM].maxMaintenance + 1;  // = 5+1 = 6
        int initial = farm->incomplete;                        // = -maxMaintenance = -5

        helper.run_pirate_raid(r, pirates);

        expect(farm->destroyed <= cap)
            << "o->destroyed must not exceed maxMaintenance+1 = " << cap;
        expect(farm->incomplete <= initial + cap)
            << "incomplete must not increase by more than " << cap;
        expect(farm->incomplete >= 1)
            << "functional farm must be disabled (incomplete >= 1) after raid";
    };

    // ---------------------------------------------------------------
    // Per-turn cap (broken): cap = 4, slow additional damage
    // ---------------------------------------------------------------
    "Pirate raid on broken building is capped at 4 damage per turn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 1000 pirates guarantees cap is always hit
        Unit *pirates = helper.create_pirate_unit(r, 1000);

        Object *farm = make_empty_building(r, O_FARM);
        farm->incomplete = 1;  // already broken at start of turn
        int initial = farm->incomplete;

        helper.run_pirate_raid(r, pirates);

        expect(farm->destroyed <= 4)
            << "o->destroyed must not exceed 4 for a broken building";
        expect(farm->incomplete <= initial + 4)
            << "incomplete must not increase by more than 4 for a broken building";
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

    // ---------------------------------------------------------------
    // Elite recognition: the gazette line must come from the fleet object, not
    // the crew unit - an elite fleet keeps its captain in his own unit aboard
    // the fleet, so the crew unit can never report the fleet's elite status.
    // ---------------------------------------------------------------
    "An elite fleet's raid names the fleet, not a generic pirate fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *fleet  = pirates->object;
        helper.create_npc_pirate_captain(r, fleet);
        make_empty_building(r, O_FARM);

        helper.run_pirate_raid(r, pirates);

        auto &elite = helper.game_object().pirate_context_elite;
        expect(elite.size() == 1_ul)
            << "an elite fleet must write exactly one elite gazette line";
        expect(elite.front().find(fleet->name) != std::string::npos)
            << "the elite line must name the fleet";
        expect(helper.game_object().pirate_context_regular.empty())
            << "an elite fleet must not write a generic gazette line";
    };

    "A plain fleet's raid reads the generic pirate fleet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        make_empty_building(r, O_FARM);

        helper.run_pirate_raid(r, pirates);

        auto &regular = helper.game_object().pirate_context_regular;
        expect(regular.size() == 1_ul)
            << "a plain fleet must write exactly one generic gazette line";
        expect(regular.front().find("A pirate fleet") != std::string::npos)
            << "the regular line must read 'A pirate fleet'";
        expect(helper.game_object().pirate_context_elite.empty())
            << "a plain fleet must not write an elite gazette line";
    };
};
