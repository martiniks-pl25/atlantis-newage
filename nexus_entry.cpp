#include "nexus_entry.h"

#include "aregion.h"    // TOWN_VILLAGE, TOWN_TOWN, TOWN_CITY
#include "rng.hpp"

#include <algorithm>
#include <numeric>

namespace nexus_entry {

bool level_requires_resources(int level)
{
    // The trailing rungs drop the filter, the way the old phases 3a-6 did: past level 6
    // the question is no longer which start is good but whether one exists at all.
    return level <= static_cast<int>(Level::OTHER_VILLAGE_TWO);
}

namespace {

/**
 * @brief Whether a rung accepts a settlement, ignoring terrain.
 *
 * Terrain scope is handled by the caller, which walks the gateway cycle; this decides
 * only settlement tier, occupancy and the two disqualifiers.
 *
 * @param level rung, 1..9
 * @param s settlement under test, with occupancy already updated for earlier placements
 * @param allow_blocked whether monsters and established factions may be ignored, which
 *        only the terminal level's second pass does
 * @return true if an entrant may be seated here at this level
 */
bool accepts(int level, const Settlement& s, bool allow_blocked)
{
    if (s.blocked && !allow_blocked) return false;
    if (level_requires_resources(level) && !s.resources) return false;

    switch (static_cast<Level>(level)) {
        case Level::OWN_VILLAGE_EMPTY:
        case Level::OTHER_VILLAGE_EMPTY:
            return s.town_type == TOWN_VILLAGE && s.occupants == 0;
        case Level::OWN_VILLAGE_ONE:
        case Level::OTHER_VILLAGE_ONE:
            return s.town_type == TOWN_VILLAGE && s.occupants == 1;
        case Level::OWN_VILLAGE_TWO:
        case Level::OTHER_VILLAGE_TWO:
            return s.town_type == TOWN_VILLAGE && s.occupants == 2;
        case Level::OWN_TOWN:
        case Level::OTHER_TOWN:
            return s.town_type != TOWN_VILLAGE && s.occupants <= TOWN_OCCUPANCY_MAX;
        case Level::LEAST_CROWDED:
            return true;
    }
    return false;
}

/**
 * @brief Gateway offsets a rung searches, as a closed range over the cycle.
 *
 * Offset 0 is the entrant's own gateway; offsets 1..n-1 are the others in the engine's
 * existing cyclic order. Own-terrain rungs search {0}, other-terrain rungs {1..n-1}, and
 * the terminal rung searches everything, own terrain first.
 */
void cycle_range(int level, int gateway_count, int& from, int& to)
{
    switch (static_cast<Level>(level)) {
        case Level::OWN_VILLAGE_EMPTY:
        case Level::OWN_VILLAGE_ONE:
        case Level::OWN_VILLAGE_TWO:
        case Level::OWN_TOWN:
            from = 0;
            to = 0;
            return;
        case Level::OTHER_VILLAGE_EMPTY:
        case Level::OTHER_VILLAGE_ONE:
        case Level::OTHER_VILLAGE_TWO:
        case Level::OTHER_TOWN:
            from = 1;              // empty range when the nexus has a single gateway
            to = gateway_count - 1;
            return;
        case Level::LEAST_CROWDED:
            from = 0;
            to = gateway_count - 1;
            return;
    }
    from = 0;
    to = -1;
}

/**
 * @brief Picks a settlement for one entrant at one rung, or -1 if the rung has nothing.
 *
 * Terrains are tried in cycle order and the first one holding a match wins — an entrant
 * never skips a nearer terrain for a roomier distant one. Within a terrain the choice is
 * a lot, except at the terminal rung, which takes the fewest men and draws lots only
 * among the settlements tied for fewest.
 *
 * @note The terminal rung ranks on Settlement::men, not on Settlement::occupants.
 * @param allow_blocked admits blocked settlements; set only on the terminal level's
 *        second pass.
 */
int pick_for(
    int level,
    const Entrant& e,
    const std::vector<Settlement>& settlements,
    const std::vector<int>& gateway_terrains,
    bool allow_blocked
) {
    const int n = static_cast<int>(gateway_terrains.size());
    if (n <= 0) return -1;

    int start = e.gateway;
    if (start < 0 || start >= n) start = 0;

    int from = 0, to = -1;
    cycle_range(level, n, from, to);

    for (int gi = from; gi <= to; gi++) {
        const int terrain = gateway_terrains[(start + gi) % n];

        std::vector<int> matches;
        for (size_t si = 0; si < settlements.size(); si++) {
            if (settlements[si].terrain != terrain) continue;
            if (!accepts(level, settlements[si], allow_blocked)) continue;
            matches.push_back(static_cast<int>(si));
        }
        if (matches.empty()) continue;

        if (level != LAST_LEVEL) return matches[rng::get_random(static_cast<int>(matches.size()))];

        int fewest = settlements[matches.front()].men;
        for (const int si : matches) fewest = std::min(fewest, settlements[si].men);

        std::vector<int> tied;
        for (const int si : matches)
            if (settlements[si].men == fewest) tied.push_back(si);

        return tied[rng::get_random(static_cast<int>(tied.size()))];
    }

    return -1;
}

} // namespace

std::vector<Placement> allocate(
    const std::vector<Entrant>& entrants,
    std::vector<Settlement> settlements,
    const std::vector<int>& gateway_terrains
) {
    std::vector<Placement> result(entrants.size());
    for (size_t i = 0; i < entrants.size(); i++) result[i].entrant = static_cast<int>(i);

    std::vector<int> unplaced(entrants.size());
    std::iota(unplaced.begin(), unplaced.end(), 0);

    for (int level = FIRST_LEVEL; level <= LAST_LEVEL && !unplaced.empty(); level++) {
        // Shuffled per level: when a rung runs short, who gets in is a lot rather than a
        // position in the orders file.
        rng::shuffle(unplaced);

        std::vector<int> still_unplaced;
        for (const int ei : unplaced) {
            int si = pick_for(level, entrants[ei], settlements, gateway_terrains, false);

            // Second pass of the terminal level: blocked settlements become eligible once
            // the whole cycle has offered nothing unblocked.
            if (si < 0 && level == LAST_LEVEL)
                si = pick_for(level, entrants[ei], settlements, gateway_terrains, true);

            if (si < 0) {
                still_unplaced.push_back(ei);
                continue;
            }

            // Seat taken straight away, so the rest of this level sees it occupied.
            settlements[si].occupants++;
            settlements[si].men += entrants[ei].men;

            result[ei].settlement = si;
            result[ei].level = level;
        }
        unplaced.swap(still_unplaced);
    }

    return result;
}

} // namespace nexus_entry
