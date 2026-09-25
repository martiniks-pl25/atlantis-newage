// unittest/quest_battle_debt_test.cpp
// Bounty tokens in battle spoils (Army::Win): only the tokens a quest kill grants
// create debt; tokens looted from a defeated carrier are plain spoils.

#include "external/boost/ut.hpp"
#include "battle.h"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "unit.h"
#include "quests.h"
#include "testhelper.hpp"

#include <memory>
#include <string>

namespace ut = boost::ut;
using namespace std;

namespace {

// Overwhelming force, so the fight resolves without the target surviving on a lucky roll.
Unit *create_attacker(UnitTestHelper &helper, ARegion *r, const string& name)
{
    Faction *player = helper.create_faction(name);
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 200);
    u->items.SetNum(I_MSWORD, 200);
    helper.set_skill_level(u, S_COMBAT, 5);
    return u;
}

// A minimal LOCAL_HUNT quest on `target_num`, pushed to the global list.
void make_hunt_quest(int target_num, int issuer_region, int tokens)
{
    auto q = make_shared<Quest>();
    q->num           = 998;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_HUNT;
    q->tokens        = tokens;
    q->issuer_unit   = 0;
    q->issuer_region = issuer_region;
    q->created_turn  = 1;
    q->expires_turn  = 13;
    q->target        = target_num;
    q->regionname    = "Testburg";
    quests.push_back(q);
}

} // namespace

ut::suite<"QuestBattleDebt"> quest_battle_debt_suite = [] {
    using namespace ut;

    "looted bounty tokens create no debt"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 2, 0);
        helper.clear_npc_units_in_region(r);

        Unit *courier = helper.create_unit(helper.create_faction("Couriers"), r);
        courier->items.SetNum(I_BOUNTY, 5);

        Unit *raider = create_attacker(helper, r, "Raiders");
        expect(helper.run_battle(r, raider, courier) == BATTLE_WON) << "the raider must win";

        expect(eq(raider->items.GetNum(I_BOUNTY), 5)) << "all tokens looted";
        expect(raider->faction->quest_debts.empty()) << "loot is not a completed quest";
    };

    "a quest kill still credits its tokens as debt"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 2, 0);
        helper.clear_npc_units_in_region(r);

        Faction *mf = helper.get_faction(helper.get_monfaction());
        Unit *monster = helper.create_unit(mf, r);
        monster->type = U_WMON;
        monster->items.SetNum(I_WOLF, 2);
        make_hunt_quest(monster->num, r->num, 3);

        Unit *hunter = create_attacker(helper, r, "Hunters");
        expect(helper.run_battle(r, hunter, monster) == BATTLE_WON) << "the hunter must win";

        expect(eq(hunter->items.GetNum(I_BOUNTY), 3)) << "the quest grant is paid out";
        expect(eq(hunter->faction->quest_debts[r->num], 3)) << "and owed by the issuing town";
    };
};
