#pragma once
#ifndef BATTLE_H
#define BATTLE_H


class Battle;

#include "astring.h"
#include "army.h"
#include "items.h"
#include "events.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

class Location;

#include "external/nlohmann/json.hpp"
using json = nlohmann::json;

enum {
    ASS_NONE,
    ASS_SUCC,
    ASS_FAIL
};

enum {
    BATTLE_IMPOSSIBLE,
    BATTLE_LOST,
    BATTLE_WON,
    BATTLE_DRAW
};

// How a battle was triggered, for the JSON report's "trigger" field. Every caller of
// Game::AttemptAttack / Game::RunBattle states its own value; it is never inferred.
// Crosses into game.h as a plain int - see the note on RunBattle there.
enum BattleTrigger : int {
    TRIGGER_ATTACK_ORDER = 0,   // explicit ATTACK order from a player
    TRIGGER_AUTO_ATTACK = 1,    // hostile units meeting, or a monster's own aggression roll
    TRIGGER_ADVANCE = 2,        // ADVANCE order / guard advance
    TRIGGER_ASSASSINATION = 3
};

// Turn-scaled pirate map-drop tuning (dormant unless configured via rulesetSpecificData;
// see docs/PIRATE_MAP_CHANCE_RAMP_PLAN.md). Both functions are pure — no RNG, no
// Game/Battle state — so they are testable in isolation from the combat pipeline.
struct MapChanceRamp {
    int crewChance;  // mature crew's per-vessel map-roll chance, "turn N = N%" capped
    int tmapShare;   // 0-100: scheduled % of a successful map roll that becomes TMAP, before supply throttle
};

MapChanceRamp compute_map_chance_ramp(int turnNumber, int cap);

// Suppresses tmapShare toward floorShare as more pirate hideouts are already alive
// in the world, so the turn ramp above can't be tuned into a hideout-spawn feedback
// loop. Never drops below floorShare — a hideout must always stay at least as likely
// as the game's un-ramped baseline. softCap <= 0 disables the throttle (returns
// tmapShare unchanged).
int apply_hideout_supply_throttle(int tmapShare, int activeHideouts, int softCap, int floorShare);

// A vessel entry's flat officer map-roll bonus: captain, bosun and admiral pay
// +20 each, however many of that role died there. The crew is NOT included — its
// chance scales separately as "turn N = N%" (see Battle::crewMapChance). Pure —
// no RNG, no state — so the role rule is testable without a battle.
int pirate_officer_chance(bool had_captain, bool had_bosun, bool had_admiral);

// --- Structured battle data for the JSON report ------------------------------------------
// Captured during Run() (while units are still alive) and emitted by build_json_report().
// The battle prose (Battle::text / "report") is built independently and stays unchanged.
struct BattleUnit {
    std::string name;
    int number = 0;
    std::string faction_name;
    int faction_number = 0;
    bool faction_known = false;  // false when the opposing side could not identify the faction
    bool behind = false;
};

struct BattleLoss {
    std::string tag;     // 4-char item abbr (e.g. "HELF")
    std::string name;    // singular item name, as elsewhere in the report
    std::string plural;  // plural form, so a consumer can reproduce the prose wording
    int count = 0;
    std::vector<std::pair<int, int>> units;  // (unit num, count) — which units lost how many
};

struct BattleCasualty {
    BattleUnit army;   // army leader (the "X loses N" subject)
    int lost = 0;
    std::vector<BattleLoss> losses;   // per-item-type breakdown
};

struct BattleEvent {
    BattleUnit unit;   // acting unit
    std::string kind;  // "special" | "mount" | "shield" | "damage" | "regen" | "overwhelm"
    int killed = -1;   // enemies killed (special/mount)
    int hits = -1;     // hits taken/healed (damage/regen)
    int current = -1;  // HP after (damage/regen)
    int max = -1;      // max HP (damage/regen)
    std::string text;  // original prose line, always present (fallback)
};

struct BattleRound {
    int number = 0;    // 1-based normal round; -1 = free round
    bool has_tactics_bonus = false;
    int tactics_bonus = 0;
    BattleUnit tactics_army;
    std::vector<BattleEvent> events;
    std::vector<BattleCasualty> casualties;
};

struct BattleHeal {
    BattleUnit unit;
    int count = 0;
    std::string source;   // "healing potion" | "magical healing" | "healing"
};

struct BattleTotalCasualty {
    BattleUnit army;
    int lost = 0;
    std::vector<int> damaged_units;   // unit nums (the "Damaged units:" line)
    std::vector<BattleHeal> heals;
};

struct BattleSpoil {
    std::string tag;
    std::string name;    // singular item name
    std::string plural;  // plural form
    int count = 0;
};

class Battle
{
    public:
        Battle();
        ~Battle();

        void build_json_report(json &j, Faction *fac);
        void AddLine(const std::string& line);

        int Run(
            Events* events, ARegion *region, Unit *att, std::list<Location *>& atts,
            Unit *tar, std::list<Location *>& defs, int ass
        );
        void FreeRound(Army *,Army *, int ass = 0);
        void NormalRound(int,Army *,Army *);
        void DoAttack(int round, Soldier *a, Army *attackers, Army *def,
                int behind, int ass = 0, bool canAttackBehind = false, bool canAttackFromBehind = false);

        void GetSpoils(std::list<Location *>& losers, ItemList& spoils, int ass, Events *events);

        //
        // These functions should be implemented in specials.cpp
        //
        void UpdateShields(Army *);
        void DoSpecialAttack( int round, Soldier *a, Army *attackers,
                Army *def, int behind, int canattackback);

        void WriteSides(
            ARegion * r, Unit * att, Unit * tar, std::list<Location *>& atts, std::list<Location *>& defs, int ass
        );

        // void WriteBattleStats(ArmyStats *);

        int assassination;
        Faction * attacker; /* Only matters in the case of an assassination */
        std::string asstext;
        std::vector<std::string> text;
        // Set by check_kill_target when a quest target dies; read by Army::Win.
        int quest_issuer_region = -1;
        int quest_num = -1;                  // quest->num for awareness lookup; -1 if no quest matched
        std::string quest_rewards;           // event text for factions that already knew the quest
        std::string quest_rewards_unaware;   // event text for factions that did NOT know the quest

        // --- Structured JSON report fields (captured during Run, emitted by build_json_report) ---
        int trigger = TRIGGER_ATTACK_ORDER;  // BattleTrigger — how the fight started
        int phase = -1;                      // -1 = before movement; 0..MAX_SPEED-1 = after that phase
        BattleUnit attacker_unit;            // initiating unit
        BattleUnit defender_unit;            // target unit
        int region_x = 0, region_y = 0, region_z = 0;
        std::string region_terrain;
        std::string region_province;
        std::string region_label;            // "surface" | level name (e.g. "underworld")
        std::vector<BattleUnit> attackers;   // full attacker roster (front then behind)
        std::vector<BattleUnit> defenders;   // full defender roster
        std::vector<BattleRound> rounds;     // normal rounds (number >= 1) and free rounds (number == -1)
        int outcome = BATTLE_IMPOSSIBLE;     // BATTLE_WON / BATTLE_LOST / BATTLE_DRAW
        BattleUnit defeat_unit;
        bool defeat_routed = false;          // routed (free round followed) vs destroyed
        std::vector<BattleTotalCasualty> total_casualties;
        std::vector<BattleSpoil> spoil_items;  // not `spoils`: that name is taken by the
                                               // ItemList locals in Run() and GetSpoils()
        std::vector<std::string> messages;   // post-battle lines (quest, undead rise, loot notes)
        int quest_tokens = -1;               // bounty tokens granted; -1 = no quest target killed
        int quest_tokens_total = 0;          // tokens granted by every quest kill in this battle;
                                             // Army::Win credits debt for this many and no more
        bool quest_global = false;           // GLOBAL_BOSS_HUNT vs local bounty

        // Appends a structured event to the round currently being played. Every call site
        // sits inside NormalRound or FreeRound, so a round always exists; the guard is there
        // so a future caller outside a round drops the event instead of corrupting state.
        void AddEvent(const BattleEvent& ev);
        // Captures a Unit* into a bare BattleUnit reference (name/number, never the faction),
        // for the event and casualty sites that serialize as {name, number}.
        void capture_unit(Unit* u, BattleUnit& bu);
        // Pushes an empty BattleTotalCasualty for `leader` (the "X loses N" subject).
        void push_total_casualty(Unit* leader);

        // Turn-scaled, hideout-supply-throttled pirate map-drop tuning, copied each
        // battle from Game's per-turn cache in Game::RunBattle. crewMapChance is the
        // mature crew's per-vessel map chance ("turn N = N%" on the surface, 10% in a
        // dungeon); tmapShare is the TMAP fraction. Defaults (10/10) reproduce the
        // dungeon-battle baseline, so any direct `new Battle` / `Battle b;` construction
        // that bypasses RunBattle (simulate.cpp, test_armor_battle.cpp) is unaffected.
        int crewMapChance = 10;
        int tmapShare = 10;
        // Turn cooldown a pirate fleet must wait after its captain dies before it
        // can earn another. Copied from rulesetSpecificData by Game::RunBattle and
        // read by Army::Lose, which has no Game pointer of its own.
        int pirate_promote_cooldown = 6;

    private:
        /**
         * @brief Accumulator for mount special effect messages within one round.
         *
         * Mount specials (e.g. camel "spook" that panics enemy horses) are triggered
         * once per attacking soldier, so a unit of 100 camels would produce 100 separate
         * lines per round. To avoid this spam, results are aggregated here during the
         * attack loop and flushed as a single summary line per unit at end of round.
         *
         * Key: "<unit_name>|<special_name>" — groups all soldiers of the same unit
         * with the same mount special.
         * Value: accumulated total hits and message template fields.
         *
         * Zero-result entries (tot == 0, e.g. all spooks deflected) are silently
         * discarded in FlushMountSpecials() and never appear in the report.
         */
        struct MountSpecialAccum {
            std::string unitName;
            std::string spelldesc, spelldesc2, spelltarget;
            int total = 0;
            BattleUnit unit;   // structured actor for the JSON event
        };
        std::map<std::string, MountSpecialAccum> mountSpecialAccum;
        void FlushMountSpecials();
};

#endif // BATTLE_H
