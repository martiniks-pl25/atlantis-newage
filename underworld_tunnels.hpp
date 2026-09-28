#pragma once
#ifndef UNDERWORLD_TUNNELS_HPP
#define UNDERWORLD_TUNNELS_HPP

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

/**
 * @brief Carves an underworld level into chambers joined by tunnel corridors.
 *
 * GrowTerrain() leaves the level as patches, each grown from one anchor and carrying
 * that anchor's name id. Here each patch is a chamber; neighbouring chambers are
 * linked by a random spanning tree plus a few extra links, each link carved as a short
 * corridor of tunnel hexes through the wall between the two chambers. Then the walls
 * are planned: links between chambers are mostly cut, links along a corridor never,
 * every coastal chamber keeps one way to the sea, and the cuts are repaired until
 * every piece of land connected before them is connected again.
 *
 * Pure functions over an abstract hex graph: no ARegion, no global RNG. Randomness
 * comes in as a Random callable, so the caller decides exactly what is consumed and
 * a test can drive it with a fixed sequence.
 *
 * @see carve_underworld_tunnels() in neworigins/map.cpp, UnderworldTunnelRules
 */
namespace underworld_tunnels {

/// What a hex is to the carving.
enum class Tile : unsigned char {
    none,  ///< no hex at this index
    land,  ///< cavern or underforest: part of a chamber
    sea,   ///< underground ocean
    lake,  ///< inland lake inside a chamber
};

/// Tunables; see UnderworldTunnelRules in ruleset_config.h for their meaning.
struct Params {
    int max_share_pct  = 18;
    int half_length    = 3;
    int min_chamber    = 6;
    int extra_link_pct = 25;
    int chamber_cut    = 15;
    int wall_cut       = 90;
    int coast_cut      = 60;
};

/// The level as a graph. Index d of nbr[c] is the region's direction d, -1 for none.
struct Level {
    std::vector<Tile> tile;
    std::vector<int> seed;  ///< patch id of a land hex (the name id GrowTerrain spread), -1 otherwise
    std::vector<std::array<int, 6>> nbr;

    [[nodiscard]] int size() const { return static_cast<int>(tile.size()); }
};

/// Returns a uniform integer in 0..n-1, n >= 1.
using Random = std::function<int(int)>;

/// One corridor: the two chambers it joins and its hexes, from's side first.
struct Corridor {
    int from = -1;
    int to = -1;
    std::vector<int> cells;
};

/// A link to cut: the hex and the direction of the neighbour it loses.
struct Cut {
    int cell = -1;
    int dir = -1;
};

namespace detail {

/// Union-find over hex or chamber indices.
struct Dsu {
    std::vector<int> parent;
    explicit Dsu(const int n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    int find(int x)
    {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    bool unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a == b) return false;
        parent[b] = a;
        return true;
    }
};

/// Number of chambers: one past the highest id.
[[nodiscard]] inline int chamber_count(const std::vector<int>& chamber)
{
    int count = 0;
    for (const int c : chamber) count = std::max(count, c + 1);
    return count;
}

} // namespace detail

/**
 * @brief Chamber id of every hex: connected land with one seed, small ones folded.
 *
 * A chamber smaller than min_chamber is folded into the neighbouring chamber it shares
 * the most links with, smallest first; one with no land neighbour stays as it is.
 *
 * @return chamber id per hex, -1 for non-land; ids are 0..k-1 in order of lowest hex
 */
[[nodiscard]] inline std::vector<int> chambers(const Level& lv, const Params& p)
{
    const int n = lv.size();
    std::vector<int> id(n, -1);
    int count = 0;
    for (int s = 0; s < n; s++) {
        if (lv.tile[s] != Tile::land || id[s] >= 0) continue;
        std::queue<int> q;
        q.push(s);
        id[s] = count;
        while (!q.empty()) {
            const int c = q.front();
            q.pop();
            for (const int nb : lv.nbr[c]) {
                if (nb < 0 || id[nb] >= 0 || lv.tile[nb] != Tile::land || lv.seed[nb] != lv.seed[s]) continue;
                id[nb] = count;
                q.push(nb);
            }
        }
        count++;
    }

    std::vector<int> size(count, 0);
    for (const int c : id) {
        if (c >= 0) size[c]++;
    }
    std::vector<char> stuck(count, 0);
    for (;;) {
        int small = -1;
        for (int k = 0; k < count; k++) {
            if (size[k] == 0 || stuck[k] || size[k] >= p.min_chamber) continue;
            if (small < 0 || size[k] < size[small]) small = k;
        }
        if (small < 0) break;
        std::map<int, int> contact;
        for (int c = 0; c < n; c++) {
            if (id[c] != small) continue;
            for (const int nb : lv.nbr[c]) {
                if (nb >= 0 && id[nb] >= 0 && id[nb] != small) contact[id[nb]]++;
            }
        }
        if (contact.empty()) {
            stuck[small] = 1;
            continue;
        }
        int into = contact.begin()->first;
        for (const auto& [k, links] : contact) {
            if (links > contact.at(into)) into = k;
        }
        for (int& c : id) {
            if (c == small) c = into;
        }
        size[into] += size[small];
        size[small] = 0;
    }

    std::vector<int> renumber(count, -1);
    int next = 0;
    for (int& c : id) {
        if (c < 0) continue;
        if (renumber[c] < 0) renumber[c] = next++;
        c = renumber[c];
    }
    return id;
}

/**
 * @brief Chooses which chambers to join and carves a corridor for each pair.
 *
 * Every pair of chambers that share a link gets a random weight; a spanning forest
 * over those weights guarantees each chamber a corridor to its land neighbours, and
 * each remaining pair gets one with extra_link_pct. Tree corridors are carved first,
 * so when the budget runs out it is the extras that go without.
 *
 * A corridor starts at a random link on the pair's shared border and walks inland on
 * both sides, one hex at a time towards the chamber's interior, up to per_side hexes:
 * budget / (2 * pairs), clamped to 1..half_length, where budget is max_share_pct of
 * the land. It never takes a hex another corridor holds, nor more than half of a
 * chamber, so every chamber keeps a floor of its own.
 *
 * @param chamber chamber id per hex, from chambers()
 * @return corridors with at least one hex, tree pairs first
 */
[[nodiscard]] inline std::vector<Corridor> plan_corridors(
    const Level& lv, const std::vector<int>& chamber, const Params& p, const Random& rnd)
{
    const int n = lv.size();
    const int count = detail::chamber_count(chamber);
    int land = 0;
    for (const Tile t : lv.tile) land += t == Tile::land;

    // Border links per chamber pair, first chamber's hex first; std::map keeps the
    // order, and so the random draws, independent of memory layout.
    std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> border;
    for (int a = 0; a < n; a++) {
        if (chamber[a] < 0) continue;
        for (const int b : lv.nbr[a]) {
            if (b < 0 || chamber[b] < 0 || chamber[b] <= chamber[a]) continue;
            border[{ chamber[a], chamber[b] }].push_back({ a, b });
        }
    }

    struct Link {
        int x;
        int y;
        int weight;
    };
    std::vector<Link> links;
    for (const auto& entry : border) links.push_back({ entry.first.first, entry.first.second, rnd(1000) });
    std::ranges::stable_sort(links, {}, &Link::weight);
    detail::Dsu forest(count);
    std::vector<Link> chosen;
    std::vector<Link> extra;
    for (const Link& l : links) {
        if (forest.unite(l.x, l.y)) {
            chosen.push_back(l);
        } else if (rnd(100) < p.extra_link_pct) {
            extra.push_back(l);
        }
    }
    chosen.insert(chosen.end(), extra.begin(), extra.end());
    if (chosen.empty()) return {};

    // Depth of each chamber hex below its chamber's edge; walking up it heads inland.
    std::vector<int> depth(n, -1);
    std::queue<int> q;
    for (int c = 0; c < n; c++) {
        if (chamber[c] < 0) continue;
        for (const int nb : lv.nbr[c]) {
            if (nb < 0 || chamber[nb] != chamber[c]) {
                depth[c] = 0;
                q.push(c);
                break;
            }
        }
    }
    while (!q.empty()) {
        const int c = q.front();
        q.pop();
        for (const int nb : lv.nbr[c]) {
            if (nb >= 0 && chamber[nb] == chamber[c] && depth[nb] < 0) {
                depth[nb] = depth[c] + 1;
                q.push(nb);
            }
        }
    }

    std::vector<int> size(count, 0);
    for (const int c : chamber) {
        if (c >= 0) size[c]++;
    }
    std::vector<int> taken(count, 0);
    std::vector<char> used(n, 0);
    int budget = land * p.max_share_pct / 100;
    const int per_side = std::clamp(budget / (2 * static_cast<int>(chosen.size())), 1, p.half_length);

    const auto walk = [&](int cell, std::vector<int>& out) {
        for (int step = 0; step < per_side && budget > 0 && cell >= 0; step++) {
            const int k = chamber[cell];
            if (used[cell] || 2 * (taken[k] + 1) > size[k]) break;
            out.push_back(cell);
            used[cell] = 1;
            taken[k]++;
            budget--;
            int next = -1;
            for (const int nb : lv.nbr[cell]) {
                if (nb >= 0 && chamber[nb] == k && !used[nb] && depth[nb] == depth[cell] + 1) {
                    next = nb;
                    break;
                }
            }
            cell = next;
        }
    };

    std::vector<Corridor> out;
    for (const Link& l : chosen) {
        if (budget <= 0) break;
        const auto& edges = border.at({ l.x, l.y });
        const auto [a, b] = edges[rnd(static_cast<int>(edges.size()))];
        Corridor cor { l.x, l.y, {} };
        walk(a, cor.cells);
        walk(b, cor.cells);
        if (!cor.cells.empty()) out.push_back(std::move(cor));
    }
    return out;
}

/**
 * @brief Plans which links to cut: walls between chambers, open corridors.
 *
 * Each link is rolled once, in hex and direction order: chamber_cut inside a chamber
 * (and between a chamber and its lake), wall_cut between chambers and wherever a
 * corridor touches a chamber or corridor it does not join, coast_cut between land and
 * sea; water to water, and a corridor to itself, its lake or its own two chambers are
 * never cut.
 *
 * Two repairs follow. A chamber on the sea whose every sea link was cut gets one back,
 * at random. Then cut land links are restored - inside-chamber links first, walls only
 * if needed, each group in random order - until every piece of land that was
 * connected before the cuts is connected again.
 *
 * @param corridors from plan_corridors(); their hexes still carry their chamber id
 * @return the links to cut, each once, from its lower hex index
 */
[[nodiscard]] inline std::vector<Cut> plan_walls(
    const Level& lv, const std::vector<int>& chamber, const std::vector<Corridor>& corridors,
    const Params& p, const Random& rnd)
{
    const int n = lv.size();
    std::vector<int> corridor_of(n, -1);
    for (int i = 0; i < static_cast<int>(corridors.size()); i++) {
        for (const int c : corridors[i].cells) corridor_of[c] = i;
    }

    enum class Kind { keep, inside, wall, coast };
    const auto kind = [&](const int a, const int b) {
        const bool la = lv.tile[a] == Tile::land;
        const bool lb = lv.tile[b] == Tile::land;
        if (!la && !lb) return Kind::keep;
        if (!la || !lb) {
            const int l = la ? a : b;
            const Tile water = la ? lv.tile[b] : lv.tile[a];
            if (water == Tile::lake) return corridor_of[l] >= 0 ? Kind::keep : Kind::inside;
            return Kind::coast;
        }
        const int ca = corridor_of[a];
        const int cb = corridor_of[b];
        if (ca >= 0 && cb >= 0) return ca == cb ? Kind::keep : Kind::wall;
        if (ca >= 0 || cb >= 0) {
            const Corridor& cor = corridors[ca >= 0 ? ca : cb];
            const int other = chamber[ca >= 0 ? b : a];
            return other == cor.from || other == cor.to ? Kind::keep : Kind::wall;
        }
        return chamber[a] == chamber[b] ? Kind::inside : Kind::wall;
    };

    struct Link {
        int a;
        int dir;
        int b;
        Kind kind;
    };
    // Land kept connected so far. A cut land link joins two hexes that were connected
    // before the cuts, so restoring cut links until none joins two of these sets leaves
    // every land piece as connected as it was.
    std::vector<Link> cut;
    detail::Dsu now(n);
    for (int a = 0; a < n; a++) {
        if (lv.tile[a] == Tile::none) continue;
        for (int d = 0; d < 6; d++) {
            const int b = lv.nbr[a][d];
            if (b <= a || lv.tile[b] == Tile::none) continue;
            const Kind k = kind(a, b);
            const bool land_link = lv.tile[a] == Tile::land && lv.tile[b] == Tile::land;
            const int pct = k == Kind::inside ? p.chamber_cut
                          : k == Kind::wall   ? p.wall_cut
                          : k == Kind::coast  ? p.coast_cut
                                              : 0;
            if (pct > 0 && rnd(100) < pct) {
                cut.push_back({ a, d, b, k });
            } else if (land_link) {
                now.unite(a, b);
            }
        }
    }
    std::vector<char> restored(cut.size(), 0);

    // Every chamber on the sea keeps one way to it: if all its sea links were cut,
    // one comes back.
    const int count = detail::chamber_count(chamber);
    std::vector<int> sea_links(count, 0);
    std::vector<std::vector<int>> cut_coast(count);
    for (int a = 0; a < n; a++) {
        if (chamber[a] < 0) continue;
        for (const int b : lv.nbr[a]) {
            if (b >= 0 && lv.tile[b] == Tile::sea) sea_links[chamber[a]]++;
        }
    }
    for (int i = 0; i < static_cast<int>(cut.size()); i++) {
        if (cut[i].kind != Kind::coast) continue;
        const int l = lv.tile[cut[i].a] == Tile::land ? cut[i].a : cut[i].b;
        cut_coast[chamber[l]].push_back(i);
    }
    for (int k = 0; k < count; k++) {
        if (sea_links[k] > 0 && sea_links[k] == static_cast<int>(cut_coast[k].size())) {
            restored[cut_coast[k][rnd(static_cast<int>(cut_coast[k].size()))]] = 1;
        }
    }

    // Reconnect the land: inside-chamber links first, walls only if still needed.
    const auto shuffled = [&](std::vector<int> v) {
        for (int i = static_cast<int>(v.size()) - 1; i > 0; i--) std::swap(v[i], v[rnd(i + 1)]);
        return v;
    };
    std::vector<int> inside;
    std::vector<int> walls;
    for (int i = 0; i < static_cast<int>(cut.size()); i++) {
        const bool land_link = lv.tile[cut[i].a] == Tile::land && lv.tile[cut[i].b] == Tile::land;
        if (!land_link) continue;
        if (cut[i].kind == Kind::inside) {
            inside.push_back(i);
        } else if (cut[i].kind == Kind::wall) {
            walls.push_back(i);
        }
    }
    const std::vector<int> inside_order = shuffled(inside);
    const std::vector<int> wall_order = shuffled(walls);
    for (const std::vector<int>* group : { &inside_order, &wall_order }) {
        for (const int i : *group) {
            if (now.unite(cut[i].a, cut[i].b)) restored[i] = 1;
        }
    }

    std::vector<Cut> out;
    for (int i = 0; i < static_cast<int>(cut.size()); i++) {
        if (!restored[i]) out.push_back({ cut[i].a, cut[i].dir });
    }
    return out;
}

} // namespace underworld_tunnels

#endif
