#pragma once
#ifndef QUEST_H
#define QUEST_H

#include "astring.h"
#include "helper.h"
#include "unit.h"
#include "items.h"
#include <set>
#include <string>
#include <algorithm>

class ARegionList;
class Events;

class Quest
{
    public:
        Quest();
        ~Quest();

        // Legacy quest types — kept for back-compat with the existing save format.
        // On load: SLAY/HARVEST/BUILD/VISIT/DEMOLISH/DELIVER are cold-dropped;
        // HUNT_PIRATE converts to GLOBAL_BOSS_HUNT.
        enum {
            SLAY,
            HARVEST,
            BUILD,
            VISIT,
            DELIVER,
            DEMOLISH,
            HUNT_PIRATE
        };

        enum Scope {
            SCOPE_GLOBAL = 0,
            SCOPE_LOCAL  = 1
        };

        enum Subtype {
            SUBTYPE_NONE = -1,
            LOCAL_LAIR_CLEAR = 0,
            LOCAL_HUNT,
            LOCAL_HARVEST,
            GLOBAL_BOSS_HUNT,
            // Reserved subtypes (unimplemented):
            LOCAL_DELIVER,
            LOCAL_VISIT,
            LOCAL_GUARD,
            LOCAL_ESCORT,
            GLOBAL_DESTROY,
            LOCAL_BUILD_ROAD,  // = 9: build a missing road segment from city to neighbor
            LOCAL_BUILD_TOWER, // = 10: build a watchtower in a domain region that lacks one
            LOCAL_BUILD_INN,   // = 11: build an inn in a domain region that lacks one
        };

        // Legacy fields — populated by Quest() ctor in quests.cpp.
        int type;
        int target;
        Item objective;
        int building;
        int regionnum;
        std::string regionname;
        std::set<std::string> destinations;
        std::vector<Item> rewards;
        std::string get_rewards();

        // New-format fields — in-header defaults so the existing ctor stays untouched.
        int num             = 0;            // unique id from Game::questseq
        int scope           = SCOPE_GLOBAL; // SCOPE_GLOBAL / SCOPE_LOCAL
        int subtype         = SUBTYPE_NONE; // see Subtype enum
        int tokens          = 0;            // payout resolved at creation from quest_data candidate arrays
        int issuer_unit     = -1;           // mayor unit num (LOCAL only)
        int issuer_region   = -1;           // mayor region num (LOCAL only; -1 = GLOBAL pool)
        int created_turn    = 0;            // TurnNumber() at creation

        // Per-subtype runtime state — populated when the matching subtype is wired up.
        int target_monster  = -1;           // LOCAL_HUNT race item; GLOBAL_BOSS_HUNT monster type
        int kills_so_far    = 0;            // LOCAL_HUNT accumulator
        int amount_so_far   = 0;            // LOCAL_HARVEST accumulator

        // Absolute turn on which this quest expires (0 = no expiry).
        // LOCAL quests: created_turn + LOCAL_QUEST_TTL.
        // Dungeon LOCAL quests: min(created_turn + TTL, spawn + lifetime + dying).
        // GLOBAL quests: 0 (no TTL).
        int expires_turn = 0;
};

class QuestList
{
    std::list<std::shared_ptr<Quest>> quests;
public:
    using iterator = typename std::list<std::shared_ptr<Quest>>::iterator;

    // read_quests takes engine version to select the right format.
    // Pre-5.2.6 saves use the legacy reader with cold-drop migration
    // (SLAY/HARVEST/BUILD/VISIT/DEMOLISH/DELIVER dropped; HUNT_PIRATE → GLOBAL_BOSS_HUNT).
    // write_quests always emits the new flat-int format.
    int read_quests(std::istream& f, ATL_VER engine_version);
    void write_quests(std::ostream& f);

    // out_quest_num / out_unaware_msg let Army::Win send a different completion event to
    // factions that did NOT know about the quest (awareness checked via known_local_quests).
    int check_kill_target(Unit *u, ItemList& spoils, std::string *quest_rewards,
                          int *out_issuer_region = nullptr, Events *events = nullptr,
                          int *out_quest_num = nullptr, std::string *out_unaware_msg = nullptr);
    int check_harvest_target(ARegion *r,    int item, int harvested, int max, Unit *u, std::string *quest_rewards);
    int check_build_target(ARegion *r, int building, Unit *u, std::string *quest_rewards);
    int check_visit_target(ARegion *r, Unit *u, std::string *quest_rewards);
    int check_demolish_target(ARegion *r, int building, Unit *u, std::string *quest_rewards);
    // Award tokens when builder completes a LOCAL_BUILD_ROAD quest for road_type in region r.
    // regions is used to look up the mayor's settlement name for the event message.
    int check_road_quest(ARegion *r, int road_type, Unit *u, std::string *quest_rewards, ARegionList& regions, Events *events = nullptr);
    // Award tokens when builder completes a LOCAL_BUILD_TOWER quest in region r.
    int check_tower_quest(ARegion *r, Unit *u, std::string *quest_rewards, ARegionList& regions, Events *events = nullptr);
    // Award tokens when builder completes a LOCAL_BUILD_INN quest in region r.
    int check_inn_quest(ARegion *r, Unit *u, std::string *quest_rewards, ARegionList& regions, Events *events = nullptr);

    inline void push_back(std::shared_ptr<Quest> q) { quests.push_back(q); }
    inline iterator begin() { return quests.begin(); }
    inline iterator end() { return quests.end(); }
    inline size_t erase(std::shared_ptr<Quest> q) { return std::erase(quests, q); }
    inline size_t size() { return quests.size(); }

    // Erase a quest and optionally purge its num from every faction's awareness set.
    // Pass factions when the caller has access to the faction list (mayor death,
    // TTL expiry); omit it in battle hooks where factions are not available.
    inline size_t erase_with_cleanup(std::shared_ptr<Quest> q,
                                     std::list<Faction *> *factions = nullptr) {
        if (factions && q->scope == Quest::SCOPE_LOCAL) {
            for (auto f : *factions)
                f->known_local_quests.erase(q->num);
        }
        return erase(q);
    }

    std::string distribute_rewards(Unit *u, std::shared_ptr<Quest> q);
};

extern QuestList quests;

#endif // QUEST_H
