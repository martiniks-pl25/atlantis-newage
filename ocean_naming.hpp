#pragma once
#ifndef OCEAN_NAMING_HPP
#define OCEAN_NAMING_HPP

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numeric>
#include <queue>
#include <vector>

/**
 * @brief Splits the surface water of a generated map into named oceans and seas.
 *
 * The parametric generator names water by connectivity: one name per connected water
 * body. On a cylinder map with a polar sea lane and an equatorial sea every ocean hex
 * joins one network, so a single name covers ~99% of the water. This module cuts that
 * network into basins along its narrows (a watershed):
 *
 * - every water hex gets its distance to the nearest land, counted through water: open
 *   water is "deep", a strait is "shallow";
 * - the water is flooded from the deepest hexes outward, each hex joining the basin of
 *   its deepest already-flooded neighbour, so two basins meet where the passage between
 *   them is narrowest;
 * - two basins that meet merge unless the passage is at least neck_depth hexes
 *   shallower than both of them, or their deepest points lie in different belts
 *   (polar, equatorial, the land bands between); the belt of a basin is the latitude of
 *   its deepest hex, so a border never follows a latitude line;
 * - each polar lane is one ocean: every hex at or above polar_lat, and every basin whose
 *   open water is polar, joins its hemisphere's polar ocean (a lane is a ring around the
 *   pole, so there its latitude line is the natural edge);
 * - a basin holding more than 1.5 * ocean_target hexes is cut by longitude into about
 *   size / ocean_target parts, the cuts on the columns with the least water - the one
 *   place left where open water has no narrows to follow (the equatorial sea);
 * - any part smaller than bay_min is folded into the neighbouring part it shares the
 *   most edges with; a part with no neighbour keeps its old name (dropped here).
 *
 * The result is a pure function of the grid: it draws no random numbers, so the
 * caller controls exactly what the naming step consumes.
 *
 * @see nameSurfaceWater() in aregion.cpp, which feeds the grid and applies the names
 */
namespace ocean_naming {

/// What a hex is to the partition.
enum class Tile : unsigned char {
    none,   ///< not a hex, or water that keeps its old name (rivers)
    land,   ///< any non-water hex; its landmass decides strait / bay / gulf
    water,  ///< ocean hex to be partitioned
};

/// Kind of a named part; decides the name's suffix.
enum class Kind { ocean, polar_ocean, sea, gulf, strait, bay };

/// Tunables; see OceanNamingRules in ruleset_config.h for their meaning.
struct Params {
    double polar_lat = 80.0;
    double edge_lat = 33.0;
    int ocean_target = 180;
    int bay_min = 6;
    int strait_max = 14;
    int neck_depth = 2;
    int ocean_min = 60;
};

/**
 * @brief The same tunables with the latitude belts switched off, for the underground.
 *
 * No hex reaches polar_lat and none lies below edge_lat, so every basin is a band
 * basin: basins merge or split only by their narrows, nothing joins a polar ocean,
 * and each part comes out a sea, gulf, strait or bay - never an ocean.
 */
[[nodiscard]] inline Params without_belts(Params p)
{
    p.polar_lat = 1000.0;
    p.edge_lat = -1.0;
    return p;
}

/// The hex grid in region coordinates: a hex exists where (x + y) is even; x wraps.
struct Grid {
    int width = 0;
    int height = 0;
    std::vector<Tile> tiles;  ///< width * height, index y * width + x

    [[nodiscard]] Tile at(const int x, const int y) const { return tiles[y * width + x]; }
};

/// One named part: its kind and its hexes as y * width + x indices.
struct Part {
    Kind kind = Kind::sea;
    std::vector<int> cells;
};

/**
 * @brief Absolute latitude of a region row, 0 at the equator and 90 at the edges.
 * @param y region row, 0..height-1
 * @param height number of region rows
 */
[[nodiscard]] inline double latitude(const int y, const int height)
{
    if (height < 2) return 0.0;
    return std::abs(90.0 * (height - 1 - 2 * y) / (height - 1));
}

/**
 * @brief The up to six hex neighbours of a cell, x wrapping around the cylinder.
 * @return indices y * width + x of the neighbours that lie on the grid
 */
[[nodiscard]] inline std::vector<int> neighbours(const Grid& g, const int index)
{
    static constexpr int dx[6] = { 0, 0, 1, 1, -1, -1 };
    static constexpr int dy[6] = { -2, 2, -1, 1, -1, 1 };
    const int x = index % g.width;
    const int y = index / g.width;
    std::vector<int> out;
    for (int d = 0; d < 6; d++) {
        const int ny = y + dy[d];
        if (ny < 0 || ny >= g.height) continue;
        const int nx = ((x + dx[d]) % g.width + g.width) % g.width;
        out.push_back(ny * g.width + nx);
    }
    return out;
}

/**
 * @brief Connected components of a cell set, in order of their lowest index.
 * @param in_set membership flag per grid index
 */
[[nodiscard]] inline std::vector<std::vector<int>> components(const Grid& g, const std::vector<char>& in_set)
{
    std::vector<char> seen(in_set.size(), 0);
    std::vector<std::vector<int>> out;
    for (int start = 0; start < static_cast<int>(in_set.size()); start++) {
        if (!in_set[start] || seen[start]) continue;
        std::vector<int> comp;
        std::queue<int> q;
        q.push(start);
        seen[start] = 1;
        while (!q.empty()) {
            const int c = q.front();
            q.pop();
            comp.push_back(c);
            for (const int n : neighbours(g, c)) {
                if (in_set[n] && !seen[n]) {
                    seen[n] = 1;
                    q.push(n);
                }
            }
        }
        out.push_back(std::move(comp));
    }
    return out;
}

/**
 * @brief Distance of every water hex to the nearest land hex, in hex steps through water.
 * @return per grid index; 0 for non-water, INT_MAX / 2 for water with no land in reach
 */
[[nodiscard]] inline std::vector<int> distance_to_land(const Grid& g)
{
    const int n = g.width * g.height;
    std::vector<int> dist(n, 0);
    std::queue<int> q;
    for (int i = 0; i < n; i++) {
        if (g.tiles[i] != Tile::water) continue;
        dist[i] = INT_MAX / 2;
    }
    for (int i = 0; i < n; i++) {
        if (g.tiles[i] != Tile::land) continue;
        for (const int nb : neighbours(g, i)) {
            if (g.tiles[nb] == Tile::water && dist[nb] > 1) {
                dist[nb] = 1;
                q.push(nb);
            }
        }
    }
    while (!q.empty()) {
        const int c = q.front();
        q.pop();
        for (const int nb : neighbours(g, c)) {
            if (g.tiles[nb] == Tile::water && dist[nb] > dist[c] + 1) {
                dist[nb] = dist[c] + 1;
                q.push(nb);
            }
        }
    }
    return dist;
}

/**
 * @brief Cuts one connected stretch of water into k parts by longitude.
 *
 * The stretch's columns are walked west to east starting just after its widest gap
 * (for a stretch that circles the whole map: at its thinnest column). Each cut goes
 * near an even share of the cells, moved within a window to the column holding the
 * least water, so borders follow the narrows between landmasses. Each slice is then
 * split into its connected pieces, since a slice of a winding stretch need not be
 * connected.
 *
 * @param comp the stretch, one connected component
 * @param k number of slices wanted, >= 1
 * @return connected pieces; small ones are left for the caller to fold in
 */
[[nodiscard]] inline std::vector<std::vector<int>> split_by_longitude(
    const Grid& g, const std::vector<int>& comp, const int k)
{
    if (k <= 1) return { comp };

    std::vector<int> per_col(g.width, 0);
    for (const int c : comp) per_col[c % g.width]++;

    // Start column: after the widest run of empty columns, or at the thinnest column
    // when the stretch covers every column.
    int start = 0;
    int best_gap = 0;
    for (int x = 0; x < g.width; x++) {
        if (per_col[x] != 0) continue;
        int len = 0;
        while (len < g.width && per_col[(x + len) % g.width] == 0) len++;
        if (len > best_gap) {
            best_gap = len;
            start = (x + len) % g.width;
        }
    }
    if (best_gap == 0) {
        start = static_cast<int>(std::min_element(per_col.begin(), per_col.end()) - per_col.begin());
    }

    std::vector<int> order;  // occupied columns, west to east from start
    for (int i = 0; i < g.width; i++) {
        const int x = (start + i) % g.width;
        if (per_col[x] > 0) order.push_back(x);
    }

    std::vector<int> cumulative(order.size(), 0);
    int running = 0;
    for (size_t i = 0; i < order.size(); i++) {
        running += per_col[order[i]];
        cumulative[i] = running;
    }

    const int total = running;
    const int window = std::max(1, static_cast<int>(order.size()) / (4 * k));
    std::vector<int> slice_of_col(g.width, 0);
    int prev_cut = -1;
    std::vector<int> cuts;  // positions in order; slice j ends at cuts[j]
    for (int j = 1; j < k; j++) {
        const int want = total * j / k;
        int ideal = static_cast<int>(std::lower_bound(cumulative.begin(), cumulative.end(), want) - cumulative.begin());
        int best = -1;
        for (int p = std::max(prev_cut + 1, ideal - window);
             p <= std::min(static_cast<int>(order.size()) - 2, ideal + window); p++) {
            if (best < 0 || per_col[order[p]] < per_col[order[best]]) best = p;
        }
        if (best < 0) break;
        cuts.push_back(best);
        prev_cut = best;
    }
    int slice = 0;
    for (size_t i = 0; i < order.size(); i++) {
        slice_of_col[order[i]] = slice;
        if (slice < static_cast<int>(cuts.size()) && static_cast<int>(i) == cuts[slice]) slice++;
    }

    std::vector<std::vector<int>> out;
    for (int s = 0; s <= static_cast<int>(cuts.size()); s++) {
        std::vector<char> in_slice(g.tiles.size(), 0);
        for (const int c : comp) {
            if (slice_of_col[c % g.width] == s) in_slice[c] = 1;
        }
        for (auto& piece : components(g, in_slice)) out.push_back(std::move(piece));
    }
    return out;
}

/**
 * @brief Partitions the grid's water into named parts (see the file comment).
 *
 * Every water tile ends in exactly one part, except tiles of a stretch smaller than
 * bay_min that touches no other water part: those are left out and keep the name the
 * generator gave them (lagoons, tarns). Parts are connected and at least bay_min hexes.
 *
 * The kind of a part comes from the latitude of its basin's deepest hex: polar ocean at
 * or above polar_lat, ocean below edge_lat, otherwise a band part. A band part up to
 * strait_max hexes touching two or more landmasses is a strait and one touching a
 * single landmass a bay; larger, a part enclosed by one landmass is a gulf, else a sea.
 *
 * @return parts in a stable order (by the deepest hex of their basin, then west to east)
 */
[[nodiscard]] inline std::vector<Part> partition(const Grid& g, const Params& p)
{
    const int n = g.width * g.height;
    const std::vector<int> dist = distance_to_land(g);

    enum class Belt { polar, equator, band };
    const auto belt_of = [&](const int cell) {
        const double lat = latitude(cell / g.width, g.height);
        return lat >= p.polar_lat ? Belt::polar : lat < p.edge_lat ? Belt::equator : Belt::band;
    };

    // Flood order: deepest first; ties by index so the result is deterministic.
    std::vector<int> order;
    for (int i = 0; i < n; i++) {
        if (g.tiles[i] == Tile::water) order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return dist[a] > dist[b]; });

    // Basins: union-find over basin ids; a basin's peak is its deepest (first) hex.
    std::vector<int> parent, peak_cell;
    const auto find = [&](int b) {
        while (parent[b] != b) {
            parent[b] = parent[parent[b]];
            b = parent[b];
        }
        return b;
    };
    std::vector<int> basin(n, -1);
    for (const int c : order) {
        int best = -1;  // flooded neighbour with the deepest water
        std::vector<int> roots;
        for (const int nb : neighbours(g, c)) {
            if (basin[nb] < 0) continue;
            if (best < 0 || dist[nb] > dist[best]) best = nb;
            const int r = find(basin[nb]);
            if (std::find(roots.begin(), roots.end(), r) == roots.end()) roots.push_back(r);
        }
        if (best < 0) {
            const int id = static_cast<int>(parent.size());
            parent.push_back(id);
            peak_cell.push_back(c);
            basin[c] = id;
            continue;
        }
        basin[c] = find(basin[best]);
        // First contact between two basins happens at the widest passage between them:
        // merge there unless it is a real neck or the basins lie in different belts.
        for (size_t i = 0; i < roots.size(); i++) {
            for (size_t j = i + 1; j < roots.size(); j++) {
                const int a = find(roots[i]);
                const int b = find(roots[j]);
                if (a == b || belt_of(peak_cell[a]) != belt_of(peak_cell[b])) continue;
                const int shallower = std::min(dist[peak_cell[a]], dist[peak_cell[b]]);
                if (shallower - dist[c] >= p.neck_depth) continue;
                const bool keep_a = dist[peak_cell[a]] > dist[peak_cell[b]]
                    || (dist[peak_cell[a]] == dist[peak_cell[b]] && peak_cell[a] < peak_cell[b]);
                parent[keep_a ? b : a] = keep_a ? a : b;
            }
        }
    }

    // Collect basins. Each polar lane is kept whole as one ocean: every hex at or above
    // polar_lat, and every basin whose open water is polar, joins its hemisphere's polar
    // group. The other basins are split by longitude when large.
    constexpr int north = -2;  // group keys below any basin id
    constexpr int south = -1;
    std::map<int, std::vector<int>> cells_of;  // basin ids follow creation = depth order
    for (const int c : order) {
        const int root = find(basin[c]);
        const int anchor = latitude(c / g.width, g.height) >= p.polar_lat ? c
                         : belt_of(peak_cell[root]) == Belt::polar ? peak_cell[root] : -1;
        const int key = anchor < 0 ? root : (anchor / g.width) < g.height / 2 ? north : south;
        cells_of[key].push_back(c);
    }
    std::vector<Part> parts;
    for (auto& [key, cells] : cells_of) {
        const bool polar = key < 0;
        const Kind kind = polar ? Kind::polar_ocean
                        : belt_of(peak_cell[key]) == Belt::equator ? Kind::ocean : Kind::sea;
        // Pulling the polar hexes out can leave a basin in pieces: name each piece.
        std::vector<char> in_group(n, 0);
        for (const int c : cells) in_group[c] = 1;
        for (auto& comp : components(g, in_group)) {
            const double share = double(comp.size()) / p.ocean_target;
            const int k = !polar && share > 1.5 ? static_cast<int>(std::lround(share)) : 1;
            for (auto& piece : split_by_longitude(g, comp, k)) parts.push_back({ kind, std::move(piece) });
        }
    }

    // Fold small parts into a neighbour, smallest first, until every part is big enough
    // or has nowhere to go. Small is under bay_min for any part and under ocean_min for
    // an ocean; an ocean prefers a neighbouring ocean of its kind (the narrow polar lanes
    // otherwise break into a string of little oceans), else the neighbour it shares the
    // most edges with.
    std::vector<int> owner(n, -1);
    for (int i = 0; i < static_cast<int>(parts.size()); i++) {
        for (const int c : parts[i].cells) owner[c] = i;
    }
    const auto too_small = [&](const Part& part) {
        const int size = static_cast<int>(part.cells.size());
        return size < p.bay_min || (part.kind != Kind::sea && size < p.ocean_min);
    };
    std::vector<char> alive(parts.size(), 1);
    for (bool changed = true; changed;) {
        changed = false;
        std::vector<int> small;
        for (int i = 0; i < static_cast<int>(parts.size()); i++) {
            if (alive[i] && too_small(parts[i])) small.push_back(i);
        }
        std::stable_sort(small.begin(), small.end(),
                         [&](int a, int b) { return parts[a].cells.size() < parts[b].cells.size(); });
        for (const int i : small) {
            if (!alive[i] || !too_small(parts[i])) continue;
            std::map<int, int> contact;
            for (const int c : parts[i].cells) {
                for (const int nb : neighbours(g, c)) {
                    if (owner[nb] >= 0 && owner[nb] != i) contact[owner[nb]]++;
                }
            }
            if (contact.empty()) continue;
            int into = -1;
            for (const auto& [j, edges] : contact) {
                const bool same = parts[j].kind == parts[i].kind;
                const bool best_same = into >= 0 && parts[into].kind == parts[i].kind;
                if (into < 0 || (same && !best_same) || (same == best_same && edges > contact[into])) into = j;
            }
            for (const int c : parts[i].cells) owner[c] = into;
            parts[into].cells.insert(parts[into].cells.end(), parts[i].cells.begin(), parts[i].cells.end());
            parts[i].cells.clear();
            alive[i] = 0;
            changed = true;
        }
    }

    // Landmass id per land tile, for the band suffixes.
    std::vector<char> is_land(n, 0);
    for (int i = 0; i < n; i++) is_land[i] = g.tiles[i] == Tile::land;
    std::vector<int> landmass(n, -1);
    int lm = 0;
    for (const auto& comp : components(g, is_land)) {
        for (const int c : comp) landmass[c] = lm;
        lm++;
    }

    std::vector<Part> out;
    for (int i = 0; i < static_cast<int>(parts.size()); i++) {
        if (!alive[i]) continue;
        Part& part = parts[i];
        // An isolated stretch too small to stand as a part keeps its generator name.
        if (static_cast<int>(part.cells.size()) < p.bay_min) continue;
        std::sort(part.cells.begin(), part.cells.end());
        if (part.kind == Kind::sea) {
            std::vector<int> shores;
            for (const int c : part.cells) {
                for (const int nb : neighbours(g, c)) {
                    if (landmass[nb] >= 0) shores.push_back(landmass[nb]);
                }
            }
            std::sort(shores.begin(), shores.end());
            const auto touched = std::unique(shores.begin(), shores.end()) - shores.begin();
            if (static_cast<int>(part.cells.size()) <= p.strait_max) {
                part.kind = touched >= 2 ? Kind::strait : touched == 1 ? Kind::bay : Kind::sea;
            } else if (touched == 1) {
                part.kind = Kind::gulf;
            }
        }
        out.push_back(std::move(part));
    }
    return out;
}

} // namespace ocean_naming

#endif
