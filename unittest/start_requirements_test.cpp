#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

#include <climits>
#include <memory>

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
// The harness world is four hexes across and start_requirements_at() searches three
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

    // The landmass rule wants at least 10 non-ocean hexes within three moves. The
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

// The spacing floor both settlement passes share. Distances are supplied, so the
// rule is testable without building a map.
ut::suite<"SettlementSpacing"> settlement_spacing_suite = [] {
    using namespace ut;

    "a site at exactly the minimum is accepted"_test = [] {
        expect(far_enough({ 4, 7, 9 }, 4));
    };

    "one step too close is refused"_test = [] {
        expect(!far_enough({ 3, 7, 9 }, 4));
    };

    // The first settlement placed has nothing to be far from.
    "an empty distance list always passes"_test = [] {
        expect(far_enough({}, 4));
    };

    // Four is the working floor everywhere; the rule itself takes whatever it is given.
    "the floor is the caller's, not the function's"_test = [] {
        expect(!far_enough({ 3 }, 4));
        expect(far_enough({ 3 }, 3));
    };
};

// -------------------------------------------------------------------------------
// Round-robin placement, shared by the surface and the underground.
//
// Bare ARegion objects are enough: the walk reads xloc, yloc and the terrain key it
// was filed under, nothing else. Building them by hand rather than generating a
// world is what makes the rules testable at all - the underground pass used to live
// in neworigins/map.cpp, which the test binary never links.
// -------------------------------------------------------------------------------
namespace {
    constexpr int TEST_MAP_WIDTH = 400;   // wide enough that nothing wraps into anything

    struct RegionPool {
        std::vector<std::unique_ptr<ARegion>> owned;
        std::unordered_map<int, std::vector<ARegion*>> by_terrain;

        // Sites are laid out along one row, `step` apart, so "far" and "close" are
        // unambiguous without reasoning about the hex metric.
        void add(int terrain, int count, int first_x, int step) {
            for (int i = 0; i < count; i++) {
                auto reg = std::make_unique<ARegion>();
                reg->type = terrain;
                reg->xloc = first_x + i * step;
                reg->yloc = 0;
                by_terrain[terrain].push_back(reg.get());
                owned.push_back(std::move(reg));
            }
        }
    };

    auto fixed_roll(int n) { return [n]() { return n; }; }

    int closest_pair(const std::vector<ARegion*>& regs) {
        int best = INT_MAX;
        for (size_t i = 0; i < regs.size(); i++)
            for (size_t j = i + 1; j < regs.size(); j++)
                best = std::min(best, cylDistance({ regs[i]->xloc, regs[i]->yloc },
                                                  { regs[j]->xloc, regs[j]->yloc },
                                                  TEST_MAP_WIDTH));
        return regs.size() < 2 ? INT_MAX : best;
    }
}

ut::suite<"RoundRobinPlacement"> round_robin_suite = [] {
    using namespace ut;

    // The guarantee the whole design exists for: on the surface an empty terrain
    // means a gateway that leads nowhere, underground it means a barren level.
    "every terrain with candidates gets a settlement"_test = [] {
        RegionPool pool;
        pool.add(R_PLAIN,    4, 0,   40);
        pool.add(R_FOREST,   4, 200, 40);
        pool.add(R_MOUNTAIN, 4, 400, 40);

        auto result = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(5), 1, 0);

        // The map only ever gains a terrain when a settlement is placed on it, so
        // presence is the guarantee. .at() would throw and take the binary with it.
        expect(result.placed_by_terrain.count(R_PLAIN) == 1_ul);
        expect(result.placed_by_terrain.count(R_FOREST) == 1_ul);
        expect(result.placed_by_terrain.count(R_MOUNTAIN) == 1_ul);
    };

    // Whoever has the least choice picks while there is still choice to be had.
    "the scarcest terrain is walked first"_test = [] {
        RegionPool pool;
        pool.add(R_PLAIN,    5, 0,   40);
        pool.add(R_FOREST,   1, 200, 40);
        pool.add(R_MOUNTAIN, 3, 400, 40);

        auto result = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(5), 1, 0);

        expect(result.order.size() == 3_ul);
        expect(result.order[0] == R_FOREST);
        expect(result.order[1] == R_MOUNTAIN);
        expect(result.order[2] == R_PLAIN);
    };

    // The ceiling is a density knob, not a correctness one: it must never be the
    // reason a terrain ends up empty.
    "a ceiling below the terrain count does not cut the guaranteed round"_test = [] {
        RegionPool pool;
        pool.add(R_PLAIN,    3, 0,   40);
        pool.add(R_FOREST,   3, 200, 40);
        pool.add(R_MOUNTAIN, 3, 400, 40);

        auto result = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(5), 1, 1);

        expect(result.chosen.size() == 3_ul);
    };

    "the ceiling stops the free stage"_test = [] {
        RegionPool pool;
        pool.add(R_PLAIN,  10, 0,   40);
        pool.add(R_FOREST, 10, 500, 40);

        auto capped = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(5), 1, 6);
        auto uncapped = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(5), 1, 0);

        expect(capped.chosen.size() <= 6_ul);
        expect(uncapped.chosen.size() > capped.chosen.size());
    };

    // Nothing anywhere lowers the roll, guaranteed rounds included.
    "no two settlements land closer than the roll"_test = [] {
        RegionPool pool;
        // Deliberately dense: adjacent sites one step apart, so a pass that ignored
        // spacing would fill the row.
        pool.add(R_PLAIN,  20, 0,   2);
        pool.add(R_FOREST, 20, 100, 2);

        auto result = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(6), 4, 0);

        expect(closest_pair(result.chosen) >= 6_i);
    };

    // A terrain boxed in by its neighbours must retire rather than spin forever.
    // Plain is scarcest so it picks first, and forest's single hex sits right beside
    // the one it takes - so forest can never place, and the walk still has to end.
    "a terrain with no room left retires instead of looping"_test = [] {
        RegionPool pool;
        pool.add(R_PLAIN,  1, 0, 40);
        pool.add(R_FOREST, 1, 2, 40);

        auto result = place_settlements_round_robin(
            pool.by_terrain, TEST_MAP_WIDTH, fixed_roll(8), 0, 0);

        expect(result.order[0] == R_PLAIN);
        expect(result.chosen.size() == 1_ul);
        expect(result.placed_by_terrain.count(R_FOREST) == 0_ul);
    };

    "an empty candidate set yields nothing and terminates"_test = [] {
        std::unordered_map<int, std::vector<ARegion*>> none;
        auto result = place_settlements_round_robin(
            none, TEST_MAP_WIDTH, fixed_roll(5), 4, 0);

        expect(result.chosen.empty());
        expect(result.order.empty());
    };
};
