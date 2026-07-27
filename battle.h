#pragma once
#ifndef BATTLE_H
#define BATTLE_H


class Battle;

#include "astring.h"
#include "army.h"
#include "items.h"
#include "events.h"
#include <map>
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

// Turn-scaled pirate map-drop tuning (dormant unless configured via rulesetSpecificData;
// see docs/PIRATE_MAP_CHANCE_RAMP_PLAN.md). Both functions are pure — no RNG, no
// Game/Battle state — so they are testable in isolation from the combat pipeline.
struct MapChanceRamp {
    double multiplier;  // multiplies Army::Lose's pirate_tmap_chance; 1.0 = unchanged
    int tmapShare;      // 0-100: scheduled % of a successful map roll that becomes TMAP, before supply throttle
};

MapChanceRamp compute_map_chance_ramp(
    int turnNumber, int rampTurns, double rampBonus, int shareEarly, int shareLate);

// Suppresses tmapShare toward floorShare as more pirate hideouts are already alive
// in the world, so the turn ramp above can't be tuned into a hideout-spawn feedback
// loop. Never drops below floorShare — a hideout must always stay at least as likely
// as the game's un-ramped baseline. softCap <= 0 disables the throttle (returns
// tmapShare unchanged).
int apply_hideout_supply_throttle(int tmapShare, int activeHideouts, int softCap, int floorShare);

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

        // Turn-ramped, hideout-supply-throttled pirate map-drop tuning, copied each
        // battle from Game's per-turn cache (Task 4 wires this in Game::RunBattle).
        // Defaults reproduce pre-ramp behavior exactly, so any direct `new Battle` /
        // `Battle b;` construction that bypasses RunBattle (simulate.cpp,
        // test_armor_battle.cpp) is unaffected.
        double mapChanceMultiplier = 1.0;
        int tmapShare = 10;

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
        };
        std::map<std::string, MountSpecialAccum> mountSpecialAccum;
        void FlushMountSpecials();
};

#endif // BATTLE_H
