#pragma once

#include <vector>
#include <iosfwd>

// Dynamic Dungeon System — types, constants, dungeon type table.
// See docs/DUNGEON_SYSTEM_DESIGN.md for full design.

// ---------------------------------------------------------------------------
// Dungeon type definition — one entry per dungeon theme.
// ---------------------------------------------------------------------------
struct DungeonMobDef {
    int item;       // monster item ID (I_SKELETON, I_IMP, ...)
    int min, max;   // spawn count range (inclusive)
};

// Layout algorithm: BFS_NETWORK produces open star-shaped rooms; DFS_CORRIDOR produces
// narrow corridors with optional branch_chance% dead ends.
enum class DungeonGenStyle { BFS_NETWORK, DFS_CORRIDOR };

struct DungeonTypeDef {
    const char *name;           // theme name, e.g. "Skeleton Ruins"
    const char *entrance_name;  // O_DUNGEON_ENTRANCE display name on surface

    int rooms_min, rooms_max;   // R_DUNGEON room count range (8×8 cell has 32 hexes)

    // Wandering monster composition per non-entry/non-boss room.
    // First entry is the primary (always spawned); remaining entries are added with 50% chance.
    std::vector<DungeonMobDef> wander_mobs;
    const char *wander_unit_name;   // unit display name, e.g. "Undead"

    // Boss unit composition — all entries always spawned.
    std::vector<DungeonMobDef> boss_mobs;
    const char *boss_unit_name;     // e.g. "Ancient Lich"

    // Boss-alive detection: dungeon closes when no unit in any room has this item.
    int boss_kill_item;             // I_LICH / I_DEVIL / I_DRAGON

    // Generation style and branching probability (0–100 %).
    DungeonGenStyle gen_style;
    int branch_chance;  // % chance to add each additional neighbor as a branch/dead-end

    // Spawn weight at turn 1 and turn 50+. Interpolated linearly between these points.
    // At each turn the weight for this type is: early + (late - early) * min(turn,50)/50
    int weight_early;
    int weight_late;

    // Turns from boss death to collapse (escape window).
    // Short for BFS/small; longer for DFS/large dungeons.
    int dying_turns;
};

// Type table — defined in dungeon.cpp, indexed by DungeonType enum.
extern const std::vector<DungeonTypeDef> DungeonTypeDefs;

// ---------------------------------------------------------------------------
// Dungeon lifecycle
// ---------------------------------------------------------------------------
enum class DungeonType {
    DUNGEON_KOBOLD_WARRENS = 0,
    DUNGEON_SKELETON_RUINS = 1,
    DUNGEON_DEMON_PIT      = 2,
    DUNGEON_DRAGON_LAIR    = 3,
};

enum class DungeonSlotState { FREE, ACTIVE, DYING, COLLAPSING };

struct DungeonInstance {
    int              id          = -1;
    DungeonType      type        = DungeonType::DUNGEON_SKELETON_RUINS;
    DungeonSlotState state       = DungeonSlotState::FREE;
    int              phase_turn  = 0;       // turn when transitioned to DYING

    int cell_x = 0, cell_y = 0;            // grid cell in dungeon level (0..7, 0..5)

    int surface_region_num  = -1;           // LEVEL_SURFACE entrance region
    int entrance_object_num = -1;           // O_DUNGEON_ENTRANCE on surface (-1 after DYING)
    int entry_region_num    = -1;           // entry room region num (dungeon level)
    int exit_object_num     = -1;           // O_DUNGEON_ENTRANCE in entry room (inner→surface)
    int boss_region_num     = -1;           // initial boss spawn location (boss may roam)

    std::vector<int> room_nums;             // all R_DUNGEON region nums in this dungeon
};

// ---------------------------------------------------------------------------
// Tuning constants — all dungeon knobs in one place.
// ---------------------------------------------------------------------------
namespace dungeon {
    // Grid: 8×8 coordinate cell = 32 real hexes per cell
    constexpr int CELL_SIZE = 8;

    // Spawn control
    // MAX_ACTIVE is computed dynamically as total_cells/3 — see ProcessDungeons()
    constexpr int MIN_DISTANCE       = 6;   // min hex-distance between surface entrances
    constexpr int SPAWN_ATTEMPTS     = 5;   // spawn attempts per turn
    constexpr int SPAWN_CHANCE       = 50;  // % chance per attempt
    constexpr int PLACEMENT_ATTEMPTS = 100; // random surface region tries per spawn

    // Monster aggression multiplier inside dungeons
    constexpr int AGGRESSION_MULT    = 2;
}
