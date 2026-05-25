#include "quest_data.h"


std::vector<LairTarget>    lair_targets;
std::vector<HuntTarget>    hunt_targets;
std::vector<HarvestTarget> harvest_targets;
std::vector<BossTarget>    boss_targets;

void AddLairTarget(int object_type, int tokens, int level_mask, int dungeon_type) {
    lair_targets.push_back({ object_type, tokens, level_mask, dungeon_type });
}

void AddHuntTarget(int race_item, int tokens) {
    hunt_targets.push_back({ race_item, tokens });
}

void AddHarvestTarget(int resource_item, int amount_threshold, int tokens, int level_mask) {
    harvest_targets.push_back({ resource_item, amount_threshold, tokens, level_mask });
}

void AddBossTarget(int monster_type, int tokens, int level_mask) {
    boss_targets.push_back({ monster_type, tokens, level_mask });
}

void ClearQuestCandidates() {
    lair_targets.clear();
    hunt_targets.clear();
    harvest_targets.clear();
    boss_targets.clear();
}

int LookupBossTokens(int monster_type, int fallback) {
    for (const auto& bt : boss_targets) {
        if (bt.monster_type == monster_type) return bt.tokens;
    }
    return fallback;
}

// ---------------------------------------------------------------------------
// Reward pools
// ---------------------------------------------------------------------------

std::vector<int> quest_pool_resource;
std::vector<int> quest_pool_equipment;
std::vector<int> quest_pool_magic;

void AddRewardResource (int item) { quest_pool_resource.push_back(item);  }
void AddRewardEquipment(int item) { quest_pool_equipment.push_back(item); }
void AddRewardMagic    (int item) { quest_pool_magic.push_back(item);     }

void ClearRewardPools() {
    quest_pool_resource.clear();
    quest_pool_equipment.clear();
    quest_pool_magic.clear();
}
