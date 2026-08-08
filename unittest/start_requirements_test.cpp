#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Give a region a production entry, which is what produces_item() reads.
static void give(ARegion *r, int item)
{
    Production *p = new Production;
    p->itemtype = item;
    p->amount = 10;
    p->baseamount = 10;
    p->skill = -1;
    r->products.push_back(p);
}

// Strip every region of its natural production.
//
// The harness world is four hexes across and start_requirements_at() searches two
// moves out, so its forest and mountains would supply wood, iron and stone to any
// hex under test. Without a clean slate these tests would pass no matter what the
// implementation did.
static void clear_all_products(UnitTestHelper &helper)
{
    for (auto *reg : helper.get_regions()) {
        for (auto *p : reg->products) delete p;
        reg->products.clear();
    }
}

// First non-ocean region of the harness world.
static ARegion *first_land_region(UnitTestHelper &helper)
{
    for (auto *reg : helper.get_regions())
        if (TerrainDefs[reg->type].similar_type != R_OCEAN) return reg;
    return nullptr;
}

static ARegionArray *array_of(UnitTestHelper &helper, ARegion *r)
{
    return helper.get_regions().GetRegionArray(r->zloc);
}

ut::suite<"StartRequirements"> start_requirements_suite = [] {
    using namespace ut;

    "resources in the hex itself are found"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        clear_all_products(helper);

        ARegion *r = first_land_region(helper);
        expect(r != nullptr) << "harness world has no land region";

        for (int item : { I_WOOD, I_IRON, I_STONE, I_GRAIN, I_HORSE }) give(r, item);

        StartRequirements req = array_of(helper, r)->start_requirements_at(r);
        expect(req.wood);
        expect(req.iron);
        expect(req.stone);
        expect(req.food);
        expect(req.mounts);
    };

    // The search is supposed to reach outward, not just look under its own feet.
    "resources in a neighbouring hex are found"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        clear_all_products(helper);

        ARegion *r = first_land_region(helper);
        ARegion *neighbour = nullptr;
        for (int d = 0; d < NDIRS; d++) {
            ARegion *n = r->neighbors[d];
            if (n && TerrainDefs[n->type].similar_type != R_OCEAN) { neighbour = n; break; }
        }
        expect(neighbour != nullptr) << "test region has no land neighbour";

        give(neighbour, I_IRON);

        StartRequirements req = array_of(helper, r)->start_requirements_at(r);
        expect(req.iron) << "iron one hex away must count";
        expect(!req.wood) << "nothing else was given to anyone";
    };

    // Food is grain OR livestock; mounts are horse OR camel.
    "food and mounts accept either alternative"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        clear_all_products(helper);

        ARegion *r = first_land_region(helper);
        for (int item : { I_LIVESTOCK, I_CAMEL }) give(r, item);

        StartRequirements req = array_of(helper, r)->start_requirements_at(r);
        expect(req.food) << "livestock alone must satisfy food";
        expect(req.mounts) << "camel alone must satisfy mounts";
        expect(!req.wood);
        expect(!req.iron);
        expect(!req.stone);
    };

    // The landmass rule wants at least 10 non-ocean hexes within two moves. The
    // harness world is far smaller, so this pins the rule rather than the map:
    // a hex on a sliver of land is never a valid start, however well stocked.
    "a tiny landmass never satisfies the requirements"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        clear_all_products(helper);

        ARegion *r = first_land_region(helper);
        for (int item : { I_WOOD, I_IRON, I_STONE, I_GRAIN, I_HORSE }) give(r, item);

        StartRequirements req = array_of(helper, r)->start_requirements_at(r);
        expect(!req.landmass);
        expect(!req.all()) << "all() must fail while landmass does";
    };

    // reach is the landmass rule's own input, reported so a settlement that fails
    // for want of land can be told apart from one that fails for want of ore.
    "reach carries the hex count the landmass rule tested"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        ARegion *r = first_land_region(helper);
        StartRequirements req = array_of(helper, r)->start_requirements_at(r);

        expect(gt(req.reach, 0)) << "the hex itself is always reachable";
        expect(eq(req.landmass, req.reach >= 10)) << "landmass must be reach >= 10 and nothing else";
    };
};

ut::suite<"rounded arithmetic"> rounding_suite = [] {
    using namespace ut;

    // The tuning report used to truncate: an average crew of 17.8 printed as 17.
    "rounded_div rounds to nearest"_test = [] {
        expect(eq(rounded_div(178, 10), 18));
        expect(eq(rounded_div(174, 10), 17));
        expect(eq(rounded_div(175, 10), 18)) << "exact half rounds away from zero";
        expect(eq(rounded_div(1552, 87), 18)) << "measured 48x48 world: 17.8 crew";
    };

    "rounded_div survives a zero denominator"_test = [] {
        expect(eq(rounded_div(5, 0), 0));
        expect(eq(percent_rounded(5, 0), 0));
    };

    "rounded_div handles negative numerators"_test = [] {
        expect(eq(rounded_div(-178, 10), -18));
        expect(eq(rounded_div(-174, 10), -17));
    };

    "percent_rounded rounds to nearest"_test = [] {
        expect(eq(percent_rounded(551, 557), 99)) << "measured 48x48 world: 98.9% of land";
        expect(eq(percent_rounded(1, 3), 33));
        expect(eq(percent_rounded(2, 3), 67));
    };
};

ut::suite<"DistanceSummary"> distance_summary_suite = [] {
    using namespace ut;

    "odd-sized set"_test = [] {
        DistanceSummary s = summarise_distances({ 5, 3, 7 });
        expect(eq(s.count, 3));
        expect(eq(s.min, 3));
        expect(eq(s.max, 7));
        expect(eq(s.median, 5));
        expect(eq(s.mean_tenths, 50));
    };

    "even-sized set takes the upper middle"_test = [] {
        DistanceSummary s = summarise_distances({ 4, 2, 8, 6 });
        expect(eq(s.count, 4));
        expect(eq(s.min, 2));
        expect(eq(s.max, 8));
        expect(eq(s.median, 6));
        expect(eq(s.mean_tenths, 50));
    };

    "empty set reports zeroes"_test = [] {
        DistanceSummary s = summarise_distances({});
        expect(eq(s.count, 0));
        expect(eq(s.min, 0));
        expect(eq(s.max, 0));
        expect(eq(s.median, 0));
    };

    // 17 + 18 + 18 = 53, mean 17.67: the tenth has to round up, not truncate.
    "mean rounds to the nearest tenth"_test = [] {
        DistanceSummary s = summarise_distances({ 17, 18, 18 });
        expect(eq(s.mean_tenths, 177));
    };

    "mean of a single distance is exact"_test = [] {
        DistanceSummary s = summarise_distances({ 4 });
        expect(eq(s.count, 1));
        expect(eq(s.mean_tenths, 40));
    };
};
