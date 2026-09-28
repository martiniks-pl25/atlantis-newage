#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "../external/boost/ut.hpp"
#include "../underworld_tunnels.hpp"

namespace ut = boost::ut;
using namespace std;
using namespace underworld_tunnels;

// Builders are plain functions, never namespace-scope containers: ut suite bodies run
// during static initialisation (see the boost.ut static-init trap in this repo's memory).
namespace {

constexpr int kW = 32;
constexpr int kH = 32;

// Hex grid as MakeRegions()/NeighSetup() build it: a hex where x + y is even, x wraps.
// Direction order N, NE, SE, S, SW, NW as in the engine.
Level make_level()
{
    static constexpr int dx[6] = { 0, 1, 1, 0, -1, -1 };
    static constexpr int dy[6] = { -2, -1, 1, 2, 1, -1 };
    Level lv;
    lv.tile.assign(kW * kH, Tile::none);
    lv.seed.assign(kW * kH, -1);
    lv.nbr.assign(kW * kH, { -1, -1, -1, -1, -1, -1 });
    for (int y = 0; y < kH; y++) {
        for (int x = 0; x < kW; x++) {
            if ((x + y) % 2) continue;
            const int i = y * kW + x;
            // A sea along the bottom; above it 8x8 patches, one seed each, plus a lake.
            if (y >= 24) {
                lv.tile[i] = Tile::sea;
            } else if (x == 4 && y == 4) {
                lv.tile[i] = Tile::lake;
            } else {
                lv.tile[i] = Tile::land;
                lv.seed[i] = (x / 8) + 10 * (y / 8);
            }
            for (int d = 0; d < 6; d++) {
                const int ny = y + dy[d];
                if (ny < 0 || ny >= kH) continue;
                const int nx = ((x + dx[d]) % kW + kW) % kW;
                lv.nbr[i][d] = ny * kW + nx;
            }
        }
    }
    return lv;
}

// Deterministic stand-in for rng::get_random.
Random lcg(const unsigned seed)
{
    auto state = make_shared<unsigned>(seed);
    return [state](const int n) {
        *state = *state * 1103515245u + 12345u;
        return static_cast<int>((*state >> 16) % static_cast<unsigned>(n));
    };
}

// The neighbour table after the cuts, both ends of each link removed.
vector<array<int, 6>> after_cuts(const Level& lv, const vector<Cut>& cuts)
{
    auto nbr = lv.nbr;
    for (const Cut& c : cuts) {
        const int b = nbr[c.cell][c.dir];
        nbr[c.cell][c.dir] = -1;
        for (int& back : nbr[b]) {
            if (back == c.cell) back = -1;
        }
    }
    return nbr;
}

// Connected pieces of land under a neighbour table.
int land_pieces(const Level& lv, const vector<array<int, 6>>& nbr)
{
    vector<char> seen(lv.size(), 0);
    int pieces = 0;
    for (int s = 0; s < lv.size(); s++) {
        if (lv.tile[s] != Tile::land || seen[s]) continue;
        pieces++;
        vector<int> stack { s };
        seen[s] = 1;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            for (const int nb : nbr[c]) {
                if (nb >= 0 && lv.tile[nb] == Tile::land && !seen[nb]) {
                    seen[nb] = 1;
                    stack.push_back(nb);
                }
            }
        }
    }
    return pieces;
}

} // namespace

ut::suite<"Underworld Tunnels"> underworld_tunnels_suite = [] {
    using namespace ut;

    "land connected before the cuts stays connected"_test = [] {
        const Level lv = make_level();
        const Params p {};
        for (unsigned seed = 1; seed <= 30; seed++) {
            const Random rnd = lcg(seed);
            const auto chamber = chambers(lv, p);
            const auto corridors = plan_corridors(lv, chamber, p, rnd);
            const auto cuts = plan_walls(lv, chamber, corridors, p, rnd);
            expect(land_pieces(lv, after_cuts(lv, cuts)) == land_pieces(lv, lv.nbr)) << "seed" << seed;
        }
    };

    "every chamber on the sea keeps a way to it"_test = [] {
        const Level lv = make_level();
        const Params p {};
        for (unsigned seed = 1; seed <= 30; seed++) {
            const Random rnd = lcg(seed);
            const auto chamber = chambers(lv, p);
            const auto corridors = plan_corridors(lv, chamber, p, rnd);
            const auto nbr = after_cuts(lv, plan_walls(lv, chamber, corridors, p, rnd));
            const int count = underworld_tunnels::detail::chamber_count(chamber);
            vector<char> coastal(count, 0);
            vector<char> open(count, 0);
            for (int c = 0; c < lv.size(); c++) {
                if (chamber[c] < 0) continue;
                for (const int nb : lv.nbr[c]) {
                    if (nb >= 0 && lv.tile[nb] == Tile::sea) coastal[chamber[c]] = 1;
                }
                for (const int nb : nbr[c]) {
                    if (nb >= 0 && lv.tile[nb] == Tile::sea) open[chamber[c]] = 1;
                }
            }
            for (int k = 0; k < count; k++) {
                expect(!coastal[k] || open[k]) << "seed" << seed << "chamber" << k;
            }
        }
    };

    "tunnels keep to the budget and leave half of every chamber"_test = [] {
        const Level lv = make_level();
        const Params p {};
        int land = 0;
        for (const Tile t : lv.tile) land += t == Tile::land;
        for (unsigned seed = 1; seed <= 30; seed++) {
            const auto chamber = chambers(lv, p);
            const auto corridors = plan_corridors(lv, chamber, p, lcg(seed));
            const int count = underworld_tunnels::detail::chamber_count(chamber);
            vector<int> size(count, 0);
            vector<int> taken(count, 0);
            for (const int c : chamber) {
                if (c >= 0) size[c]++;
            }
            int carved = 0;
            for (const Corridor& cor : corridors) {
                carved += static_cast<int>(cor.cells.size());
                for (const int cell : cor.cells) taken[chamber[cell]]++;
            }
            expect(carved > 0) << "seed" << seed;
            expect(carved <= land * p.max_share_pct / 100) << "seed" << seed;
            for (int k = 0; k < count; k++) expect(taken[k] <= size[k] / 2) << "chamber" << k;
        }
    };

    "the same random stream carves the same level"_test = [] {
        const Level lv = make_level();
        const Params p {};
        const auto chamber = chambers(lv, p);
        const Random r1 = lcg(7);
        const Random r2 = lcg(7);
        const auto c1 = plan_corridors(lv, chamber, p, r1);
        const auto c2 = plan_corridors(lv, chamber, p, r2);
        const auto w1 = plan_walls(lv, chamber, c1, p, r1);
        const auto w2 = plan_walls(lv, chamber, c2, p, r2);
        expect(c1.size() == c2.size());
        for (size_t i = 0; i < min(c1.size(), c2.size()); i++) expect(c1[i].cells == c2[i].cells);
        expect(w1.size() == w2.size());
        for (size_t i = 0; i < min(w1.size(), w2.size()); i++) {
            expect(w1[i].cell == w2[i].cell && w1[i].dir == w2[i].dir);
        }
    };
};
