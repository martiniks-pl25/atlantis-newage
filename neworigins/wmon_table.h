#pragma once

#include "gamedata.h"

/**
 * @brief One terrain's wandering-monster slots in the NewOrigins ruleset
 *
 * Mirrors the arguments of Game::ModifyTerrainWMons(): the terrain, its spawn
 * frequency and the three monster categories (small, big, humanoid).
 *
 * @see Game::ModifyTerrainWMons
 */
struct WMonSlot
{
    int terrain;    ///< Terrain the slots belong to (R_*)
    int freq;       ///< Spawn frequency (TerrainType::wmonfreq)
    int small;      ///< Small monster item (I_*) or -1
    int big;        ///< Big monster item (I_*) or -1
    int humanoid;   ///< Humanoid monster item (I_*) or -1
};

/**
 * @brief NewOrigins wandering-monster table, one row per terrain
 *
 * Single source of truth: Game::ModifyTablesPerRuleset() applies these rows via
 * Game::ModifyTerrainWMons(), and unittest/wmon_habitat_test.cpp validates them.
 * Rows are grouped by map level: surface, underworld, underdeep.
 *
 * @note Every slot must name a monster that can live on the row's terrain:
 *       either its preferredTerrain list contains the terrain (compared by
 *       TerrainDefs[].similar_type), or the monster has no preferences at all.
 *       Unit::DefaultOrders() only moves a monster into preferred terrain, or
 *       into neutral terrain that touches preferred terrain, so a monster
 *       spawned outside its habitat can never move - and a terrain that exists
 *       only on another map level is unreachable no matter the neighbourhood.
 */
inline constexpr WMonSlot NewOriginsWMons[] = {
    // Surface level
    {R_OCEAN,    5, I_PIRATES,   I_KRAKEN,    I_MERFOLK},

    {R_PLAIN,    3, I_LION,      I_BEHEMOTH,  I_CENTAUR},
    {R_FOREST,   3, I_WOLF,      I_TRENT,     I_KOBOLD},
    {R_MOUNTAIN, 5, I_GBEAR,     I_WYVERN,    I_OGRE},
    {R_HILL,     3, I_GBEAR,     I_ROC,       I_OGRE},
    {R_SWAMP,    4, I_CROCODILE, I_BTHING,    I_TROLL},
    {R_JUNGLE,   3, I_ANACONDA,  I_KONG,      I_WMEN},
    {R_DESERT,   4, I_SCORPION,  I_SPHINX,    I_SANDLING},
    {R_TUNDRA,   4, I_PBEAR,     I_IWURM,     I_YETI},

    {R_VOLCANO, 12, I_IMP,       I_IFRIT,     I_DEMON},
    {R_LAKE,     4, I_MERFOLK,   I_ELEMENTAL, I_MERFOLK},

    // Underworld level
    {R_CAVERN,  12, I_RAT,       I_DRAGON,    I_GOBLIN},
    {R_UFOREST, 12, I_SPIDER,    I_DRAGON,    I_TROLL},
    {R_TUNNELS, 12, I_LIZARD,    I_WYVERN,    I_ETTIN},

    // Underdeep level
    {R_GROTTO,  20, I_DEMON,     I_DRAGON,    I_IFRIT},
    {R_DFOREST, 20, I_LMEN,      I_DRAGON,    I_TROLL},
    {R_CHASM,   20, I_DEMON,     I_DEVIL,     I_ETTIN},
};
