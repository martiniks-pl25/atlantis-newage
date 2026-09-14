#include "external/boost/ut.hpp"
#include "external/nlohmann/json.hpp"
#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

#include <algorithm>

namespace ut = boost::ut;

// Helper: a working (fully maintained) building with nobody inside.
static Object *make_building(ARegion *r, int building_type) {
    Object *obj = new Object(r);
    obj->type = building_type;
    obj->num = r->buildingseq++;
    obj->set_name("Building");
    obj->incomplete = -(ObjectDefs[building_type].maxMaintenance);
    r->objects.push_back(obj);
    return obj;
}

// Helper: a behemoth of the given maturity (free 3 = young ... 0 = elder).
static Unit *make_behemoth(UnitTestHelper &helper, ARegion *r, int free) {
    Unit *u = helper.create_monster(r, I_BEHEMOTH, 1);
    u->free = free;
    return u;
}

ut::suite<"MonsterRaid"> monster_raid_suite = [] {
    using namespace ut;

    "Raid targets repeat an empty building by emptyWeight and an occupied one by occupiedWeight"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        Object *mine = make_building(r, O_MINE);
        Unit *miner = helper.create_unit(helper.create_faction("Miners"), r);
        miner->MoveUnit(mine);

        auto targets = raid_targets(r, raid_snapshot(r), RaidProfile{ 3, 1, 1 });

        expect(std::count(targets.begin(), targets.end(), farm) == 3) << "empty farm must appear 3 times";
        expect(std::count(targets.begin(), targets.end(), mine) == 1) << "occupied mine must appear once";
        expect(targets.size() == 4_ul) << "nothing else in the region is a target";
    };

    "Raid targets leave out occupied buildings when occupiedWeight is zero"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        Object *mine = make_building(r, O_MINE);
        Unit *miner = helper.create_unit(helper.create_faction("Miners"), r);
        miner->MoveUnit(mine);

        auto targets = raid_targets(r, raid_snapshot(r), RaidProfile{ 1, 0, 2 });

        expect(targets.size() == 1_ul) << "only one target when occupied buildings weigh 0";
        expect(!targets.empty() && targets.front() == farm) << "that target is the empty farm";
    };

    "Raid targets never include fortifications"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        for (int type : { O_TOWER, O_FORT, O_CASTLE, O_CITADEL })
            make_building(r, type);

        auto targets = raid_targets(r, raid_snapshot(r), RaidProfile{ 3, 1, 1 });

        expect(targets.empty()) << "towers, forts, castles and citadels must never be raided";
    };

    "Elder behemoth stops a lone working farm in one turn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);            // incomplete -5
        Unit *behemoth = make_behemoth(helper, r, 0);       // elder: budget 12

        helper.run_behemoth_trample(r, behemoth);

        expect(farm->incomplete == 1) << "cap maxMaintenance + 1 = 6 takes the farm from -5 to exactly 1";
        expect(farm->destroyed == 6) << "a working farm takes no more than 6 in one turn";
    };

    "Young behemoth spends a budget of 3"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        Unit *behemoth = make_behemoth(helper, r, 3);       // young

        helper.run_behemoth_trample(r, behemoth);

        expect(farm->incomplete == -2) << "young budget 3 moves the farm from -5 to -2, still working";
    };

    "A behemoth older than the table counts as young"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        Unit *behemoth = make_behemoth(helper, r, 5);       // escaped monster: free above 3

        helper.run_behemoth_trample(r, behemoth);

        expect(farm->incomplete == -2) << "free above the table must use the youngest budget";
    };

    "Elder behemoth spreads its budget over two farms"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farmA = make_building(r, O_FARM);
        Object *farmB = make_building(r, O_FARM);
        Unit *behemoth = make_behemoth(helper, r, 0);       // budget 12 = 2 x cap 6

        helper.run_behemoth_trample(r, behemoth);

        expect(farmA->incomplete == 1) << "first farm reaches its cap and stops";
        expect(farmB->incomplete == 1) << "the rest of the budget stops the second farm";
    };

    "Behemoth damages an occupied building"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        Unit *farmer = helper.create_unit(helper.create_faction("Farmers"), r);
        farmer->MoveUnit(farm);
        Unit *behemoth = make_behemoth(helper, r, 0);

        helper.run_behemoth_trample(r, behemoth);

        expect(farm->incomplete == 1) << "an occupied farm is still trampled, only picked less often";
    };

    "Behemoth never takes a building below the quarter-cost floor"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        farm->incomplete = 7;                               // broken, structure points 3
        Unit *behemoth = make_behemoth(helper, r, 0);

        helper.run_behemoth_trample(r, behemoth);

        expect(farm->incomplete == 8) << "one hit to structure points 2, then the cost / 4 floor stops it";
    };

    "Behemoth does not trample when the chance is zero"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        int initial = farm->incomplete;
        Unit *behemoth = make_behemoth(helper, r, 0);

        int saved = behemothTrampleChance;
        behemothTrampleChance = 0;
        helper.run_behemoth_trample(r, behemoth);
        behemothTrampleChance = saved;

        expect(farm->incomplete == initial) << "chance 0 must leave buildings untouched";
    };

    "Other wandering monsters do not trample"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Object *farm = make_building(r, O_FARM);
        int initial = farm->incomplete;
        Unit *lions = helper.create_monster(r, I_LION, 3);

        helper.run_behemoth_trample(r, lions);

        expect(farm->incomplete == initial) << "only a unit holding I_BEHEMOTH tramples";
    };

    "A trample writes one gazette line naming the region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        make_building(r, O_FARM);
        Unit *behemoth = make_behemoth(helper, r, 0);

        helper.run_behemoth_trample(r, behemoth);

        auto &ctx = helper.game_object().monster_raid_context;
        expect(ctx.size() == 1_ul) << "one trample writes exactly one gazette line";
        expect(!ctx.empty() && ctx.front().find(r->name) != std::string::npos) << "the line names the region";
        expect(helper.game_object().pirate_context_regular.empty()) << "a trample is not a pirate raid";
    };

    "Gazette JSON carries the monster raid context"_test = [] {
        Events events;
        std::string out = events.WriteJSON("World", "January", 1, {}, {},
                                           { "A behemoth trampled buildings in Ashford." });
        auto j = nlohmann::json::parse(out);

        expect(j.contains("monster_raid_context")) << "times.json must carry monster_raid_context";
        expect(j["monster_raid_context"].size() == 1_ul);
        expect(j["monster_raid_context"][0] == "A behemoth trampled buildings in Ashford.");
    };

    "Behemoth melee defence is 4 and the other defences are unchanged"_test = [] {
        auto behemoth = find_monster("BEHE", 0);
        expect(behemoth.has_value()) << "BEHE must be defined in MonDefs";
        if (!behemoth) return;

        const MonType &mon = behemoth->get();
        expect(mon.defense[ATTACK_COMBAT]  == 4) << "melee defence raised 3 -> 4";
        expect(mon.defense[ATTACK_ENERGY]  == 4);
        expect(mon.defense[ATTACK_SPIRIT]  == 3);
        expect(mon.defense[ATTACK_WEATHER] == 1);
        expect(mon.defense[ATTACK_RIDING]  == 2);
        expect(mon.defense[ATTACK_RANGED]  == 4);
    };
};
