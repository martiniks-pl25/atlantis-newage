#pragma once

#include <vector>

// Nexus entry allocation - deciding where a batch of gateway entrants starts.
// See docs/GATEWAY_ENTRY_SYSTEM.md for the ladder and the reasoning behind it.
//
// Takes plain data rather than ARegion or Game: the caller collects the entrants and the
// settlement table and maps the returned indices back to its own regions.

namespace nexus_entry {

// One unit standing in the Nexus with a move order into a gateway. A batch is the
// entrants of a single movement phase: a unit that pauses first arrives at a later phase
// and is allocated with that phase's batch, not with this one.
struct Entrant {
    int id = 0;         // caller's handle, normally Unit::num; the allocator only echoes it
    int gateway = 0;    // index into the gateway terrain list, the gateway it entered
    int men = 2;        // men it brings, added to the settlement it takes
};

// One settlement the ladder may place an entrant in.
struct Settlement {
    int terrain = -1;       // R_PLAIN ... R_TUNDRA
    int town_type = -1;     // TOWN_VILLAGE, TOWN_TOWN, TOWN_CITY
    int occupants = 0;      // player units present
    int men = 0;            // men in those units; the terminal level ranks on this
    bool resources = false; // passes get_starting_region_candidates(terrain, true)
    bool blocked = false;   // monsters present, or an established faction lives here
};

// Levels 1-8 refuse a blocked settlement. Level 9 takes one only after a first pass over
// the whole cycle has found nothing unblocked.

// Where one entrant ended up.
struct Placement {
    int entrant = -1;    // index into the entrants vector
    int settlement = -1; // index into the settlements vector, -1 if nothing was available
    int level = 0;       // rung that placed them, 1..9; 0 when unplaced
};

// Ladder rungs, in the order they are served. Every rung is exhausted for all still
// unplaced entrants before the next one opens — that is what keeps one terrain's overflow
// from reaching another terrain ahead of the players who chose it.
enum class Level {
    OWN_VILLAGE_EMPTY = 1,  // own terrain, village, no players
    OWN_VILLAGE_ONE,        // own terrain, village, one player unit
    OTHER_VILLAGE_EMPTY,    // other terrains, village, no players
    OTHER_VILLAGE_ONE,      // other terrains, village, one player unit
    OWN_VILLAGE_TWO,        // own terrain, village, two player units
    OTHER_VILLAGE_TWO,      // other terrains, village, two player units
    OWN_TOWN,               // own terrain, town or city, up to three player units
    OTHER_TOWN,             // other terrains, town or city, up to three
    LEAST_CROWDED,          // any settlement, fewest men, own terrain then the cycle
};

constexpr int FIRST_LEVEL = 1;
constexpr int LAST_LEVEL = 9;

// Levels 7 and 8 match on a range rather than an exact count, unlike levels 1-6.
constexpr int TOWN_OCCUPANCY_MAX = 3;

// Levels 1-6 require the resource filter; 7-9 do not.
bool level_requires_resources(int level);

/**
 * @brief Allocates this turn's gateway entrants across the settlement ladder.
 *
 * Walks levels 1..9 (see Level). At each level every still-unplaced entrant is offered
 * the settlements that rung accepts; entrants are shuffled within the level so a shortage
 * is resolved by lot rather than by position in the orders file. A placement raises the
 * settlement's occupants and men immediately, so two entrants of the same batch never
 * take the same empty village.
 *
 * Terrain scope: "own" is gateway_terrains[entrant.gateway]; "other" is the remaining
 * gateways walked cyclically from the entered one, matching the engine's existing
 * (startIdx + gi) % size order. Level 9 tries the own terrain first and then the cycle,
 * and inside a terrain takes the settlement with the fewest men, ties broken by lot.
 *
 * @param entrants          this turn's entrants; order does not affect the outcome
 * @param settlements       every settlement on the entry level, occupancy already counted
 * @param gateway_terrains  terrain behind each gateway, indexed by Entrant::gateway
 * @return one Placement per entrant, in entrant order; settlement == -1 only when there is
 *         no settlement at all to be had, which the caller must treat as "do not move".
 * @note Deterministic under rng::seed_random(seed); the tests rely on that.
 * @see docs/GATEWAY_ENTRY_SYSTEM.md
 */
std::vector<Placement> allocate(
    const std::vector<Entrant>& entrants,
    std::vector<Settlement> settlements,
    const std::vector<int>& gateway_terrains
);

} // namespace nexus_entry
