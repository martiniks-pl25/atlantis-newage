#include "external/boost/ut.hpp"
#include "gamedata.h"
#include "nexus_entry.h"
#include "aregion.h"    // TOWN_VILLAGE, TOWN_TOWN, TOWN_CITY
#include "rng.hpp"

#include <set>
#include <vector>

namespace ut = boost::ut;

using nexus_entry::Entrant;
using nexus_entry::Placement;
using nexus_entry::Settlement;

// The allocator takes plain data, so these tests build a world out of struct literals and
// never touch UnitTestHelper. That is the whole point of keeping it free of ARegion.

static Settlement village(int terrain, int occupants = 0, int men = 0)
{
    Settlement s;
    s.terrain = terrain;
    s.town_type = TOWN_VILLAGE;
    s.occupants = occupants;
    s.men = men;
    s.resources = true;
    return s;
}

static Settlement city(int terrain, int occupants, int men)
{
    Settlement s;
    s.terrain = terrain;
    s.town_type = TOWN_CITY;
    s.occupants = occupants;
    s.men = men;
    s.resources = true;
    return s;
}

// `count` entrants all coming through gateway `gateway`, ids numbered from `first_id`.
static void add_entrants(std::vector<Entrant>& out, int gateway, int count, int first_id)
{
    for (int i = 0; i < count; i++) {
        Entrant e;
        e.id = first_id + i;
        e.gateway = gateway;
        e.men = 2;
        out.push_back(e);
    }
}

static int placed_count(const std::vector<Placement>& ps)
{
    int n = 0;
    for (const auto& p : ps) if (p.settlement >= 0) n++;
    return n;
}

// Occupancy after allocation, which the result reports only indirectly.
static std::vector<int> final_occupancy(
    const std::vector<Placement>& ps, const std::vector<Settlement>& before)
{
    std::vector<int> occ;
    for (const auto& s : before) occ.push_back(s.occupants);
    for (const auto& p : ps) if (p.settlement >= 0) occ[p.settlement]++;
    return occ;
}

ut::suite<"NexusEntry"> nexus_entry_suite = [] {
    using namespace ut;

    // Two gateways: mountain is gateway 0, forest gateway 1.
    const std::vector<int> two_gateways = { R_MOUNTAIN, R_FOREST };

    "ten entrants and three villages fill them two deep, then spill to the next terrain"_test = [&] {
        rng::seed_random(11);

        std::vector<Settlement> world;
        for (int i = 0; i < 3; i++) world.push_back(village(R_MOUNTAIN));
        for (int i = 0; i < 6; i++) world.push_back(village(R_FOREST));

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 10, 1);

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(placed_count(result) == 10) << "every entrant should have a start";

        int at_level[nexus_entry::LAST_LEVEL + 1] = {0};
        for (const auto& p : result) at_level[p.level]++;

        expect(at_level[1] == 3) << "three mountain villages seat three at level 1";
        expect(at_level[2] == 3) << "the same three seat three more at level 2";
        expect(at_level[3] == 4) << "the rest cross to forest at level 3, not to a third seat";

        auto occ = final_occupancy(result, world);
        for (int i = 0; i < 3; i++)
            expect(occ[i] == 2) << "a mountain village was filled past two";
    };

    "a terrain's own entrants are served before another terrain's overflow"_test = [&] {
        rng::seed_random(12);

        // Three mountain villages against ten mountain entrants; forest has four for five.
        std::vector<Settlement> world;
        for (int i = 0; i < 3; i++) world.push_back(village(R_MOUNTAIN));
        for (int i = 0; i < 4; i++) world.push_back(village(R_FOREST));

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 10, 1);    // mountain
        add_entrants(entrants, 1, 5, 100);   // forest

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(placed_count(result) == 15) << "every entrant should have a start";

        for (const auto& p : result) {
            if (entrants[p.entrant].gateway != 1) continue;
            expect(p.level <= 2) << "a forest entrant was pushed off its own terrain";
            expect(world[p.settlement].terrain == R_FOREST) << "forest entrant left forest";
        }
    };

    "nobody takes a third seat while a village anywhere holds fewer"_test = [&] {
        rng::seed_random(13);

        std::vector<Settlement> world;
        for (int i = 0; i < 2; i++) world.push_back(village(R_MOUNTAIN));
        for (int i = 0; i < 3; i++) world.push_back(village(R_FOREST));

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 9, 1);  // more than the two mountain villages hold

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(placed_count(result) == 9) << "every entrant should have a start";

        auto occ = final_occupancy(result, world);
        int at_two = 0;
        for (const auto& o : occ) {
            expect(o <= 2) << "a village took a third player while others still had room";
            if (o == 2) at_two++;
        }
        expect(at_two == 4) << "nine entrants over five villages: four pairs and a single";
    };

    "a shortage is settled by lot, not by position in the file"_test = [&] {
        // One gateway, three villages, ten entrants: only three can be first.
        const std::vector<int> one_gateway = { R_MOUNTAIN };

        auto winners_with_seed = [&](int seed) {
            rng::seed_random(seed);

            std::vector<Settlement> world;
            for (int i = 0; i < 3; i++) world.push_back(village(R_MOUNTAIN));

            std::vector<Entrant> entrants;
            add_entrants(entrants, 0, 10, 1);

            auto result = nexus_entry::allocate(entrants, world, one_gateway);

            std::set<int> firsts;
            for (const auto& p : result)
                if (p.level == 1) firsts.insert(entrants[p.entrant].id);
            return firsts;
        };

        // Any single pair of seeds can draw the same three entrants by chance — one in
        // 120 — so the assertion is over a spread of seeds, not over one pair.
        std::set<std::set<int>> distinct;
        for (int seed = 1; seed <= 6; seed++) {
            auto winners = winners_with_seed(seed);
            expect(winners.size() == 3u) << "exactly three entrants win level 1";
            distinct.insert(winners);
        }
        expect(distinct.size() > 1u) << "the same three entrants won under every seed";
    };

    "the terminal level ranks by men, not by units"_test = [&] {
        rng::seed_random(14);

        // Both cities hold more than TOWN_OCCUPANCY_MAX, so levels 7 and 8 reject them and
        // only level 9 can place anyone. Index 0 is four units carrying 200 men between
        // them, index 1 five units of ten — fewer men in more units.
        std::vector<Settlement> world = {
            city(R_MOUNTAIN, 4, 200),
            city(R_MOUNTAIN, 5, 50),
        };

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 1, 1);

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(result[0].level == 9) << "the town levels should not accept these";
        expect(result[0].settlement == 1)
            << "took the 200-man city, which only a count of units would call emptier";
    };

    "a lone late entrant lands in the least crowded settlement, never outside one"_test = [&] {
        rng::seed_random(15);

        std::vector<Settlement> world = {
            village(R_MOUNTAIN, 6, 120),
            village(R_MOUNTAIN, 4, 90),
            city(R_FOREST, 8, 400),
        };

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 1, 1);

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(result[0].settlement == 1) << "the emptiest settlement of the own terrain";
        expect(result[0].level == 9) << "a settled map leaves only the terminal level";
    };

    "a blocked settlement is the last resort, never the first"_test = [&] {
        rng::seed_random(16);

        // Index 0 is emptier but holds monsters or a developed faction; index 1 is safe
        // and more crowded. Both are past the earlier levels.
        std::vector<Settlement> world = {
            city(R_MOUNTAIN, 4, 10),
            city(R_MOUNTAIN, 5, 300),
        };
        world[0].blocked = true;

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 1, 1);

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(result[0].settlement == 1)
            << "took the blocked settlement while a safe one was still available";
        expect(result[0].level == 9) << "only the terminal level was open here";
    };

    "leaving the Nexus works even when every settlement is blocked"_test = [&] {
        rng::seed_random(17);

        std::vector<Settlement> world = {
            city(R_MOUNTAIN, 6, 400),
            city(R_MOUNTAIN, 5, 90),
        };
        world[0].blocked = true;
        world[1].blocked = true;

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 1, 1);

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        expect(result[0].settlement == 1) << "the fewest men of the blocked settlements";
        expect(result[0].level == 9) << "placed by the terminal level's second pass";
    };

    "entrants of one batch never share an empty village"_test = [&] {
        rng::seed_random(18);

        std::vector<Settlement> world;
        for (int i = 0; i < 5; i++) world.push_back(village(R_MOUNTAIN));

        std::vector<Entrant> entrants;
        add_entrants(entrants, 0, 5, 1);

        auto result = nexus_entry::allocate(entrants, world, two_gateways);

        std::set<int> used;
        for (const auto& p : result) used.insert(p.settlement);

        expect(used.size() == 5u) << "two entrants were seated in the same empty village";
        for (const auto& p : result)
            expect(p.level == 1) << "five entrants, five empty villages, all at level 1";
    };
};
