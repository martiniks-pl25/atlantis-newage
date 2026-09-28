#include <algorithm>
#include <vector>

#include "../external/boost/ut.hpp"
#include "../ocean_naming.hpp"

namespace ut = boost::ut;
using namespace std;

// Helpers that build a synthetic 64x64 map resembling a real generated world:
// all water, plus two latitude bands (roughly 40-75 degrees) of land, each band
// made of six separate rectangular islands so the water forms one connected
// network through the polar rows, the equatorial belt, and the straits between
// islands. These are plain functions, not namespace-scope globals, so they are
// safe to call from inside the ut suite's static-init-time test bodies (see the
// boost.ut static-init trap noted in this repo's memory).
namespace {

using ocean_naming::Grid;
using ocean_naming::Tile;

constexpr int kSize = 64;

// Six islands, each 6 columns wide, separated by gaps of at least 3 columns
// (a final, larger gap closes the cylinder back to island 0).
bool in_island_column(const int x)
{
    static constexpr int starts[6] = { 0, 9, 18, 27, 36, 45 };
    for (const int s : starts) {
        if (x >= s && x < s + 6) return true;
    }
    return false;
}

// Two land bands, 12 rows each, sitting inside the "band" latitude role
// (edge_lat..polar_lat) with a couple of open, land-free rows on each side
// so the band water still connects into a single network.
bool in_land_row(const int y)
{
    return (y >= 6 && y <= 17) || (y >= 46 && y <= 57);
}

Grid make_grid()
{
    Grid g;
    g.width = kSize;
    g.height = kSize;
    g.tiles.assign(static_cast<size_t>(kSize) * kSize, Tile::none);
    for (int y = 0; y < kSize; y++) {
        for (int x = 0; x < kSize; x++) {
            if ((x + y) % 2 != 0) continue;  // no hex exists here
            const int idx = y * kSize + x;
            g.tiles[idx] = (in_land_row(y) && in_island_column(x)) ? Tile::land : Tile::water;
        }
    }
    return g;
}

size_t count_water(const Grid& g)
{
    size_t n = 0;
    for (const auto t : g.tiles) {
        if (t == Tile::water) n++;
    }
    return n;
}

bool cells_equal(const vector<int>& a, const vector<int>& b)
{
    return a == b;
}

}  // namespace

ut::suite<"Ocean Naming"> ocean_naming_suite = [] {
    using namespace ut;

    "every part is a single connected component"_test = [] {
        const Grid g = make_grid();
        const auto parts = ocean_naming::partition(g, {});
        expect(parts.size() > 0_ul);
        for (const auto& part : parts) {
            vector<char> member(g.tiles.size(), 0);
            for (const int c : part.cells) member[c] = 1;
            const auto comps = ocean_naming::components(g, member);
            expect(eq(comps.size(), size_t{ 1 }));
        }
    };

    "parts do not overlap and hold only water cells"_test = [] {
        const Grid g = make_grid();
        const auto parts = ocean_naming::partition(g, {});
        vector<int> owner(g.tiles.size(), -1);
        for (size_t i = 0; i < parts.size(); i++) {
            for (const int c : parts[i].cells) {
                expect(g.tiles[c] == Tile::water);
                expect(owner[c] == -1);
                owner[c] = static_cast<int>(i);
            }
        }
    };

    "every part has at least bay_min cells"_test = [] {
        const Grid g = make_grid();
        const ocean_naming::Params params{};
        const auto parts = ocean_naming::partition(g, params);
        for (const auto& part : parts) {
            expect(part.cells.size() >= static_cast<size_t>(params.bay_min));
        }
    };

    "no single part holds more than 30 percent of the water"_test = [] {
        const Grid g = make_grid();
        const auto parts = ocean_naming::partition(g, {});
        const size_t water_total = count_water(g);
        size_t largest = 0;
        for (const auto& part : parts) largest = std::max(largest, part.cells.size());
        expect(water_total > 0_ul);
        expect(static_cast<double>(largest) <= 0.30 * static_cast<double>(water_total));
    };

    "both ocean and polar_ocean kinds appear, at least four such parts"_test = [] {
        const Grid g = make_grid();
        const auto parts = ocean_naming::partition(g, {});
        int ocean_count = 0;
        int polar_count = 0;
        for (const auto& part : parts) {
            if (part.kind == ocean_naming::Kind::ocean) ocean_count++;
            if (part.kind == ocean_naming::Kind::polar_ocean) polar_count++;
        }
        expect(ocean_count >= 1);
        expect(polar_count >= 1);
        expect(ocean_count + polar_count >= 4);
    };

    "partition is deterministic across repeated calls"_test = [] {
        const Grid g = make_grid();
        const ocean_naming::Params params{};
        const auto a = ocean_naming::partition(g, params);
        const auto b = ocean_naming::partition(g, params);
        expect(eq(a.size(), b.size()));
        const size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; i++) {
            expect(a[i].kind == b[i].kind);
            expect(cells_equal(a[i].cells, b[i].cells));
        }
    };

    "every water cell on this map belongs to some part"_test = [] {
        const Grid g = make_grid();
        const auto parts = ocean_naming::partition(g, {});
        size_t covered = 0;
        for (const auto& part : parts) covered += part.cells.size();
        expect(eq(covered, count_water(g)));
    };

    // The underground names basins without latitude belts: a part is never an ocean,
    // and the polar rows are split by their narrows like any other water.
    "without belts no part is an ocean and every water cell is still named"_test = [] {
        const Grid g = make_grid();
        const auto parts = ocean_naming::partition(g, ocean_naming::without_belts({}));
        size_t covered = 0;
        for (const auto& part : parts) {
            expect(part.kind != ocean_naming::Kind::ocean);
            expect(part.kind != ocean_naming::Kind::polar_ocean);
            covered += part.cells.size();
        }
        expect(parts.size() > 1_ul);
        expect(eq(covered, count_water(g)));
    };
};
