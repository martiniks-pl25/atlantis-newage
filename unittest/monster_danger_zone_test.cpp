#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// ARegion::IsMonsterDangerZone() decides where wandering monsters skip the
// hostility grace period and ramp-up. Only a real level transition (O_SHAFT)
// makes a surface hex dangerous; a dungeon entrance must not, even though it
// also carries an inner location (HasShaft() is true for both).

ut::suite<"MonsterDangerZone"> monster_danger_zone_suite = [] {
    using namespace ut;

    auto add_object = [](ARegion *r, int num, int type) {
        Object *o = new Object(r);
        o->num = num;
        o->type = type;
        o->inner = 1;
        o->incomplete = 0;
        r->objects.push_back(o);
        return o;
    };

    "dungeon entrance hex keeps the surface grace period"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_PLAIN;
        expect(r->level->levelType == ARegionArray::LEVEL_SURFACE) << "precondition: surface region";
        expect(!r->IsMonsterDangerZone()) << "plain surface hex is safe";

        add_object(r, 100, O_DUNGEON_ENTRANCE);
        expect(r->HasShaft() == 1) << "entrance has an inner location";
        expect(!r->IsMonsterDangerZone()) << "dungeon entrance must not void the grace period";
    };

    "shaft hex is a danger zone"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_PLAIN;
        add_object(r, 100, O_SHAFT);
        expect(r->IsMonsterDangerZone()) << "a real shaft voids the grace period";
    };

    "volcano and underground are danger zones"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_VOLCANO;
        expect(r->IsMonsterDangerZone()) << "volcano";

        r->type = R_PLAIN;
        r->level->levelType = ARegionArray::LEVEL_UNDERWORLD;
        expect(r->IsMonsterDangerZone()) << "underground";
    };
};
