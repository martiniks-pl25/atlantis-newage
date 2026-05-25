#pragma once
#ifndef QUEST_DATA_H
#define QUEST_DATA_H

// Engine-level quest-candidate data structures.
// Variant rulesets populate these arrays in their setup hook (neworigins/quest_setup.cpp).
// Empty arrays mean no LOCAL quests are issued for that category.
//
// level_mask: optional ARegionArray::LEVEL_* discriminator; -1 = any level.
// dungeon_type: LairTarget only — DungeonType discriminator for O_DUNGEON_ENTRANCE
//   entries so different dungeon themes pay different amounts; -1 = any dungeon type.

#include <vector>

struct LairTarget {
    int object_type  = -1;  // ObjectDefs index (O_CAVE / O_DUNGEON_ENTRANCE / ...)
    int tokens       = 0;
    int level_mask   = -1;
    int dungeon_type = -1;  // used only when object_type == O_DUNGEON_ENTRANCE
};

struct HuntTarget {
    int race_item = -1;     // ItemDefs index (I_WOLF / I_TROLL / I_DRAGON / ...)
    int tokens    = 0;
    // No level_mask: generator filters candidates by mayor domain level.
    // No kill_threshold: LOCAL_HUNT targets a specific unit by unit.num (like SLAY).
};

struct HarvestTarget {
    int resource_item    = -1;  // ItemDefs index (I_IRON / I_MITHRIL / ...)
    int amount_threshold = 0;   // units produced in region to complete quest
    int tokens           = 0;
    int level_mask       = -1;
};

struct BossTarget {
    int monster_type = -1;  // ItemDefs index (I_PIRATE_CAPTAIN / ...)
    int tokens       = 0;
    int level_mask   = -1;
};

extern std::vector<LairTarget>    lair_targets;
extern std::vector<HuntTarget>    hunt_targets;
extern std::vector<HarvestTarget> harvest_targets;
extern std::vector<BossTarget>    boss_targets;

void AddLairTarget   (int object_type,   int tokens, int level_mask = -1, int dungeon_type = -1);
void AddHuntTarget   (int race_item,     int tokens);
void AddHarvestTarget(int resource_item, int amount_threshold, int tokens, int level_mask = -1);
void AddBossTarget   (int monster_type,  int tokens, int level_mask = -1);
void ClearQuestCandidates();

static constexpr int LOCAL_QUEST_TTL     = 12;  // turns before an unfinished quest expires
static constexpr int ROAD_QUEST_TOKENS   = 1;
static constexpr int TOWER_QUEST_TOKENS  = 1;
static constexpr int INN_QUEST_TOKENS    = 1;

// Returns boss_targets[monster_type].tokens, or `fallback` if the monster is not in the table.
int LookupBossTokens(int monster_type, int fallback);

// ---------------------------------------------------------------------------
// Quest reward pools (Task F).
// Populated by the ruleset setup hook; engine reads them in RunQuestOrders.
// ---------------------------------------------------------------------------

// 1 token = QUEST_TOKEN_VALUE silver; quantity = budget / baseprice.
// budget = tokens * VALUE + rng::get_random(tokens * VARIANCE + 1)
static constexpr int QUEST_TOKEN_VALUE    = 1000;
static constexpr int QUEST_TOKEN_VARIANCE =  500;

// Items explicitly listed in each pool (ItemDefs indices).
// Pool items must be enabled (not DISABLED) and have baseprice > 0.
extern std::vector<int> quest_pool_resource;   // MITH, IRWD, ROOT, FLOA, YEW, ADMT, MUSH
extern std::vector<int> quest_pool_equipment;  // advanced weapons + armor
extern std::vector<int> quest_pool_magic;      // IT_MAGIC items (not SPECIAL)

void AddRewardResource (int item);
void AddRewardEquipment(int item);
void AddRewardMagic    (int item);
void ClearRewardPools();

#endif // QUEST_DATA_H
