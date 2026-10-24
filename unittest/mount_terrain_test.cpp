#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "ruleset_config.h"
#include "unit.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Where a mount gives its riding bonus in combat: mount_bonus_cap() and the battle
// path through Unit::get_mount(). The unittest ruleset keeps the engine defaults, so
// the water mounts are passed in explicitly where they matter.
namespace {
    constexpr MountRules no_water {};
    constexpr MountRules turtle_water { .water = {{ { .tag = "TURT", .max_bonus = 1 } }} };

    static_assert(turtle_water.water_cap("TURT") == 1);
    static_assert(turtle_water.water_cap("HORS") == -1);
    static_assert(no_water.water_cap("TURT") == -1);

    // A unit of one leader with RIDI 5 and one mount of the given kind.
    Unit *rider(UnitTestHelper& helper, const int mount) {
        helper.initialize_game();
        helper.setup_turn();
        Faction *f = helper.create_faction("Riders");
        Unit *u = helper.create_unit(f, helper.get_region(0, 0, 0));
        u->SetSkill(S_RIDING, 5);
        u->items.SetNum(mount, 1);
        return u;
    }
}

ut::suite<"Mount terrain"> mount_terrain_suite = [] {
    using namespace ut;

    "ridden mounts work only where riding is allowed"_test = [] {
        expect(mount_bonus_cap(I_HORSE, R_PLAIN, no_water) == 3_i);
        expect(mount_bonus_cap(I_HORSE, R_DUNGEON, no_water) == 3_i);
        expect(mount_bonus_cap(I_CAMEL, R_DESERT, no_water) == 2_i);
        expect(mount_bonus_cap(I_HORSE, R_FOREST, no_water) == -1_i);
        expect(mount_bonus_cap(I_HORSE, R_OCEAN, no_water) == -1_i);
        expect(mount_bonus_cap(I_HORSE, R_LAKE, no_water) == -1_i);
        expect(mount_bonus_cap(I_HORSE, R_TUNNELS, no_water) == -1_i);
    };

    "a winged horse works over lakes as over oceans"_test = [] {
        expect(mount_bonus_cap(I_WHORSE, R_OCEAN, no_water) == 5_i);
        expect(mount_bonus_cap(I_WHORSE, R_LAKE, no_water) == 5_i);
        expect(mount_bonus_cap(I_WHORSE, R_FOREST, no_water) == 5_i);
        expect(mount_bonus_cap(I_WHORSE, R_TUNNELS, no_water) == -1_i);
        expect(mount_bonus_cap(I_WHORSE, R_CHASM, no_water) == -1_i);
    };

    "a turtle gives its water cap only when listed as a water mount"_test = [] {
        expect(mount_bonus_cap(I_TURT, R_OCEAN, no_water) == -1_i);
        expect(mount_bonus_cap(I_TURT, R_OCEAN, turtle_water) == 1_i);
        expect(mount_bonus_cap(I_TURT, R_LAKE, turtle_water) == 1_i);
        // Land is unchanged by the water rule.
        expect(mount_bonus_cap(I_TURT, R_PLAIN, no_water) == 1_i);
        expect(mount_bonus_cap(I_TURT, R_PLAIN, turtle_water) == 1_i);
        expect(mount_bonus_cap(I_TURT, R_FOREST, turtle_water) == -1_i);
        // Only the listed mount: a horse still gets nothing in water.
        expect(mount_bonus_cap(I_HORSE, R_OCEAN, turtle_water) == -1_i);
    };

    "a non-mount item has no cap"_test = [] {
        expect(mount_bonus_cap(I_SWORD, R_PLAIN, no_water) == -1_i);
        expect(mount_bonus_cap(-1, R_PLAIN, no_water) == -1_i);
    };

    "the rules terrains hold dungeon rooms and each name once"_test = [] {
        const std::vector<int> terrains = mount_rules_terrains();
        expect(std::ranges::find(terrains, R_DUNGEON) != terrains.end());
        expect(std::ranges::find(terrains, R_LAKE) != terrains.end());
        std::vector<std::string> names;
        for (const int t : terrains) names.push_back(TerrainDefs[t].name);
        std::ranges::sort(names);
        expect(std::ranges::adjacent_find(names) == names.end()) << "a terrain name is listed twice";
    };

    "battle caps the rider's skill by the terrain"_test = [] {
        UnitTestHelper helper;
        Unit *u = rider(helper, I_HORSE);
        int bonus = -1;
        expect(u->get_mount("HORS", R_PLAIN, bonus) == I_HORSE);
        expect(bonus == 3_i) << "RIDI 5 on a horse is capped at +3";
        expect(u->items.GetNum(I_HORSE) == 0_i) << "the soldier takes the horse";
    };

    "battle gives no bonus where the mount does not work"_test = [] {
        UnitTestHelper helper;
        Unit *u = rider(helper, I_HORSE);
        int bonus = -1;
        expect(u->get_mount("HORS", R_FOREST, bonus) == -1_i);
        expect(bonus == 0_i);
        expect(u->items.GetNum(I_HORSE) == 1_i) << "an unused horse stays with the unit";
    };

    "battle over a lake gives a winged horse its full bonus"_test = [] {
        UnitTestHelper helper;
        Unit *u = rider(helper, I_WHORSE);
        int bonus = -1;
        expect(u->get_mount("WING", R_LAKE, bonus) == I_WHORSE);
        expect(bonus == 5_i);
    };
};
