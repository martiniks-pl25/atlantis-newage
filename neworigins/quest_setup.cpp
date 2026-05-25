// NewOrigins quest candidate tables.
// Populates engine arrays used by the local quest generator.
// Token values are GM-tunable; see docs/QUEST_BALANCE_TABLES.md.
//
// Three tables:
//   hunt_targets   — monster race → tokens (LOCAL_HUNT + LOCAL_LAIR_CLEAR price lookup)
//   lair_targets   — O_DUNGEON_ENTRANCE × dungeon_type → tokens (LOCAL dungeon-boss quests)
//   boss_targets   — named surface boss → tokens (GLOBAL_BOSS_HUNT)
//
// LOCAL_HUNT uses unit.num targeting (like SLAY) — no kill_threshold needed.

#include "quest_data.h"
#include "../gamedata.h"
#include "../dungeon.h"
#include "../aregion.h"

void NewOriginsSetupQuests()
{
    ClearQuestCandidates();

    // -----------------------------------------------------------------------
    // hunt_targets — used for LOCAL_HUNT (wandering) and LOCAL_LAIR_CLEAR
    // (price lookup: reward = max tokens over all races in the target unit).
    // kill_threshold = 0 (unused; generator targets a specific unit.num).
    // level_mask = -1 (any level); generator filters by mayor domain level.
    // -----------------------------------------------------------------------

    // --- 1 token: common, easy targets ---
    AddHuntTarget(I_RAT,        1);   // Giant Rats — R_CAVERN
    AddHuntTarget(I_SPIDER,     1);   // Giant Spiders — R_UFOREST
    AddHuntTarget(I_LIZARD,     1);   // Giant Lizards — R_TUNNELS
    AddHuntTarget(I_GOBLIN,     1);   // Goblins — R_CAVERN
    AddHuntTarget(I_LMEN,       1);   // Lost Men — R_DFOREST
    AddHuntTarget(I_LION,       1);   // Pride of Lions — plain
    AddHuntTarget(I_WOLF,       1);   // Wolf Pack — forest
    AddHuntTarget(I_CROCODILE,  1);   // Crocodiles — swamp
    AddHuntTarget(I_ANACONDA,   1);   // Anacondas — jungle
    AddHuntTarget(I_SCORPION,   1);   // Scorpions — desert
    AddHuntTarget(I_SANDLING,   1);   // Sandlings — desert
    AddHuntTarget(I_WMEN,       1);   // Wild Men — jungle
    AddHuntTarget(I_SKELETON,   1);   // Skeletons — O_CRYPT lair
    AddHuntTarget(I_IMP,        1);   // Imps — O_DEMONPIT + volcano wanderers
    AddHuntTarget(I_KOBOLD,     1);   // Kobolds — forest wanderers + dungeon mobs
    AddHuntTarget(I_CENTAUR,    1);   // Centaurs — O_RUIN lair + plain wanderers
    AddHuntTarget(I_EAGLE,      1);   // Giant Eagles — mountain/tundra

    // --- 2 tokens: dangerous ---
    AddHuntTarget(I_GBEAR,      2);   // Grizzly Bears — mountain/hill
    AddHuntTarget(I_PBEAR,      2);   // Polar Bears — tundra
    AddHuntTarget(I_OGRE,       2);   // Ogres — mountain/hill
    AddHuntTarget(I_TROLL,      2);   // Trolls — swamp
    AddHuntTarget(I_YETI,       2);   // Yeti — tundra
    AddHuntTarget(I_IWURM,      2);   // Ice Wurms — tundra
    AddHuntTarget(I_ROC,        2);   // Roc — mountain
    AddHuntTarget(I_KONG,       2);   // Great Apes — jungle
    AddHuntTarget(I_BTHING,     2);   // Bog Things — swamp
    AddHuntTarget(I_PIRATES,    2);   // Pirates (regular fleet) — ocean
    AddHuntTarget(I_MERFOLK,    2);   // Merfolk — ocean/lake
    AddHuntTarget(I_UNDEAD,     2);   // Undead — O_CRYPT lair (harder than skeletons)
    AddHuntTarget(I_ELEMENTAL,  2);   // Water Elementals — lake
    AddHuntTarget(I_DEMON,      2);   // Demons — volcano wanderers; O_DEMONPIT upper tier
    AddHuntTarget(I_WARRIORS,   2);   // Evil Warriors — O_MAGETOWER lair (guardian unit)

    // --- 3 tokens: serious opponents ---
    AddHuntTarget(I_TRENT,      3);   // Treants — O_LAIR + forest wanderers
    AddHuntTarget(I_WYVERN,     3);   // Wyverns — mountain
    AddHuntTarget(I_SPHINX,     3);   // Sphinx — desert
    AddHuntTarget(I_BEHEMOTH,   3);   // Behemoths — plain, rare
    AddHuntTarget(I_MAGICIANS,  3);   // Evil Magicians — O_MAGETOWER lair
    AddHuntTarget(I_IFRIT,      3);   // Ifrits — O_IFRITLAIR + volcano wanderers
    AddHuntTarget(I_HYDRA,      3);   // Hydras — O_BOG lair
    AddHuntTarget(I_DARKMAGE,   3);   // Dark Mages — O_DARKTOWER lair
    AddHuntTarget(I_LICH,       3);   // Liches — wanderers + dungeon boss (Skeleton Ruins)
    AddHuntTarget(I_ILLYRTHID,  3);   // Illyrthids — O_ILAIR lair
    AddHuntTarget(I_ETTIN,      3);   // Ettins — R_TUNNELS / R_CHASM
    // --- 4 tokens: elite / bosses ---
    AddHuntTarget(I_BALROG,     4);   // Balrogs — O_DEMONPIT lair (powerful demon)
    AddHuntTarget(I_STORMGIANT, 4);   // Storm Giants — O_GIANTCASTLE lair
    AddHuntTarget(I_CLOUDGIANT, 4);   // Cloud Giants — O_GIANTCASTLE lair (variant)
    AddHuntTarget(I_DRAGON,     4);   // Dragons — O_CAVE lair; wanderers in tundra/mountain
    AddHuntTarget(I_ICEDRAGON,  4);   // Ice Dragons — O_ICECAVE lair
    AddHuntTarget(I_KRAKEN,     4);   // Kraken — rare ocean monster
    AddHuntTarget(I_DEVIL,      4);   // Devil — deep chasm; dungeon boss (Demon Pit)

    // -----------------------------------------------------------------------
    // lair_targets — LOCAL_LAIR_CLEAR quests for dungeon entrances.
    // Issued by the mayor of the surface region containing O_DUNGEON_ENTRANCE.
    // level_mask = LEVEL_SURFACE: generator only creates these for surface mayors.
    // dungeon_type discriminator selects the right entry when multiple dungeons
    // of different types share the same entrance object type.
    // -----------------------------------------------------------------------
    AddLairTarget(O_DUNGEON_ENTRANCE, 6,  ARegionArray::LEVEL_SURFACE,
                  static_cast<int>(DungeonType::DUNGEON_KOBOLD_WARRENS));  // easy, early game
    AddLairTarget(O_DUNGEON_ENTRANCE, 9,  ARegionArray::LEVEL_SURFACE,
                  static_cast<int>(DungeonType::DUNGEON_SKELETON_RUINS));  // medium, always available
    AddLairTarget(O_DUNGEON_ENTRANCE, 12, ARegionArray::LEVEL_SURFACE,
                  static_cast<int>(DungeonType::DUNGEON_DEMON_PIT));       // hard, late game
    AddLairTarget(O_DUNGEON_ENTRANCE, 15, ARegionArray::LEVEL_SURFACE,
                  static_cast<int>(DungeonType::DUNGEON_DRAGON_LAIR));     // hardest, late game

    // -----------------------------------------------------------------------
    // boss_targets — GLOBAL_BOSS_HUNT (one quest per live named unit).
    // -----------------------------------------------------------------------
    AddBossTarget(I_PIRATE_CAPTAIN, 6);  // elite pirate captain; 1:1 with spawn

    // -----------------------------------------------------------------------
    // Reward pools — used by RunQuestOrders (Task F).
    // See docs/ADVANCED_MAGIC_ITEMS.md for full item reference and prices.
    // -----------------------------------------------------------------------
    ClearRewardPools();

    // RESOURCE pool — raw materials (QUEST N RESOURCE)
    AddRewardResource(I_MITHRIL);
    AddRewardResource(I_IRONWOOD);
    AddRewardResource(I_ROOTSTONE);
    AddRewardResource(I_FLOATER);
    AddRewardResource(I_YEW);
    AddRewardResource(I_ADMANTIUM);
    AddRewardResource(I_MUSHROOM);

    // EQUIPMENT pool — advanced weapons + armor (QUEST N EQUIPMENT)
    AddRewardEquipment(I_MSWORD);
    AddRewardEquipment(I_MCROSSBOW);
    AddRewardEquipment(I_LANCE);
    AddRewardEquipment(I_MBAXE);
    AddRewardEquipment(I_MBHAM);
    AddRewardEquipment(I_DOUBLEBOW);
    AddRewardEquipment(I_ADSWORD);
    AddRewardEquipment(I_ABHAM);
    AddRewardEquipment(I_ADBAXE);
    AddRewardEquipment(I_MSHIELD);
    AddRewardEquipment(I_MCHAIN);
    AddRewardEquipment(I_MPLATE);
    AddRewardEquipment(I_ASHIELD);
    AddRewardEquipment(I_ADRING);
    AddRewardEquipment(I_ADPLATE);

    // MAGIC pool — used when no category specified (1/3 chance vs advanced).
    // All IT_MAGIC enabled items except SPECIAL (Amulet of Invulnerability, Portal).
    // Ordered by baseprice ascending so the token-threshold filter is intuitive.
    AddRewardMagic(I_SHIELDSTONE);    //  1,000
    AddRewardMagic(I_AMULETOFP);      //  1,000
    AddRewardMagic(I_MCARPET);        //  1,000
    AddRewardMagic(I_WINDCHIME);      //  3,000
    AddRewardMagic(I_CORNUCOPIA);     //  3,000
    AddRewardMagic(I_BOOKOFEXORCISM); //  3,000
    AddRewardMagic(I_HOLYSYMBOL);     //  3,000
    AddRewardMagic(I_GATE_CRYSTAL);   //  4,000
    AddRewardMagic(I_STAFFOFH);       //  4,000
    AddRewardMagic(I_SCRYINGORB);     //  4,000
    AddRewardMagic(I_RINGOFI);        //  5,000
    AddRewardMagic(I_STAFFOFF);       //  5,000
    AddRewardMagic(I_AMULETOFTS);     //  5,000
    AddRewardMagic(I_CENSER);         //  5,000
    AddRewardMagic(I_RUNESWORD);      //  5,000
    AddRewardMagic(I_FSWORD);         //  5,000
    AddRewardMagic(I_CLOAKOFI);       //  8,000
    AddRewardMagic(I_AEGIS);          // 45,000
}
