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

        void GetSpoils(std::list<Location *>& losers, ItemList& spoils, int ass);

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
