// unittest/quest_setup.cpp — minimal quest candidate tables for unit tests.
// Mirrors the structure of neworigins/quest_setup.cpp but with a small subset
// of entries sufficient to exercise the quest generation logic.

#include "quest_data.h"
#include "../gamedata.h"

void NewOriginsSetupQuests()
{
    ClearQuestCandidates();

    // Minimal hunt_targets for test coverage.
    // Tests use I_WOLF (tier 1) and I_DRAGON (tier 5) to verify token lookup.
    AddHuntTarget(I_WOLF,    1);
    AddHuntTarget(I_DRAGON,  5);
    AddHuntTarget(I_TROLL,   2);
    AddHuntTarget(I_SKELETON,3);

    // Boss targets for pirate captain tests.
    AddBossTarget(I_PIRATE_CAPTAIN, 4);

    // Minimal reward pools — fallback for tests that don't set up their own pools.
    // DISABLED check is absent from pick_quest_reward (pool is GM-curated),
    // so any item with baseprice > 0 works here.
    ClearRewardPools();
    AddRewardMagic    (I_SHIELDSTONE);  // 1,000g
    AddRewardEquipment(I_MBAXE);        // 300g
    AddRewardResource (I_MITHRIL);      // 100g
}
