#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// A bare region carrying only what assign_generated_name() reads.
// ARegion's constructor leaves `type` and `wages` uninitialised on purpose,
// so every test sets both explicitly.
static void prepare(ARegion &r, int type, int wages)
{
    r.type = type;
    r.wages = wages;
}

ut::suite<"RegionNaming"> region_naming_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Water hexes get a fixed name chosen by the map level they sit on.
    // -----------------------------------------------------------------------
    "ocean is named per level type"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion under;
        prepare(under, R_OCEAN, 0);
        under.assign_generated_name(ARegionArray::LEVEL_UNDERWORLD);
        expect(eq(under.name, std::string("The Undersea")));

        ARegion deep;
        prepare(deep, R_OCEAN, 0);
        deep.assign_generated_name(ARegionArray::LEVEL_UNDERDEEP);
        expect(eq(deep.name, std::string("The Deep Undersea")));

        ARegion surface;
        prepare(surface, R_OCEAN, 0);
        surface.assign_generated_name(ARegionArray::LEVEL_SURFACE);
        expect(eq(surface.name, std::string(Globals->WORLD_NAME) + " Ocean"));
    };

    // -----------------------------------------------------------------------
    // A lake shares R_OCEAN's similar_type but must be named from its seed.
    // -----------------------------------------------------------------------
    "lake is named from its seed, not as ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion lake;
        prepare(lake, R_LAKE, 0);
        lake.assign_generated_name(ARegionArray::LEVEL_UNDERWORLD);
        expect(eq(lake.name, AGetNameString(0)));
    };

    // -----------------------------------------------------------------------
    // Barren hexes (including the dungeon level's empty cells).
    // -----------------------------------------------------------------------
    "barren is named The Barrens"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion barren;
        prepare(barren, R_BARREN, 0);
        barren.assign_generated_name(ARegionArray::LEVEL_DUNGEON);
        expect(eq(barren.name, std::string("The Barrens")));
    };

    // -----------------------------------------------------------------------
    // Land: the AGetName() index parked in `wages` becomes the name.
    // -----------------------------------------------------------------------
    "land takes its name from the wages seed"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion land;
        prepare(land, R_CAVERN, 0);
        land.assign_generated_name(ARegionArray::LEVEL_UNDERWORLD);
        expect(eq(land.name, AGetNameString(0)));
    };

    // -----------------------------------------------------------------------
    // Seed sentinels: -1 means "never seeded", -2 means "keep current name"
    // and is cleared so SetupEconomy() does not read a sentinel as a wage.
    // -----------------------------------------------------------------------
    "seed -1 yields The Void"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion land;
        prepare(land, R_CAVERN, -1);
        land.assign_generated_name(ARegionArray::LEVEL_UNDERWORLD);
        expect(eq(land.name, std::string("The Void")));
    };

    "seed -2 keeps the existing name and clears the sentinel"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion land;
        prepare(land, R_CAVERN, -2);
        land.set_name("Already Named");
        land.assign_generated_name(ARegionArray::LEVEL_UNDERWORLD);
        expect(eq(land.name, std::string("Already Named")));
        expect(eq(land.wages, -1));
    };

    // -----------------------------------------------------------------------
    // The default name is the sentinel the generation-time guard looks for.
    // -----------------------------------------------------------------------
    "a fresh region carries the unnamed sentinel"_test = [] {
        ARegion fresh;
        expect(eq(fresh.name, std::string(UNNAMED_REGION)));
    };
};
