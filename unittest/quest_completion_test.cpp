// unittest/quest_completion_test.cpp
// Tests for quest completion hooks: check_kill_target, check_road_quest,
// check_tower_quest, check_inn_quest.
// Verifies that completing a quest gives I_BOUNTY, fills quest_rewards string,
// updates faction quest_debts, and removes the quest from the active list.

#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "quest_data.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;
using namespace std;

namespace {

// Build a minimal LOCAL_HUNT quest targeting `target_unit` with given tokens.
// Returns the created quest (already pushed to global `quests`).
shared_ptr<Quest> make_hunt_quest(int target_num, int issuer_region, int tokens,
                                   const string& settlement_name = "Testburg")
{
    auto q = make_shared<Quest>();
    q->num           = 999;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_HUNT;
    q->tokens        = tokens;
    q->issuer_unit   = 0;
    q->issuer_region = issuer_region;
    q->created_turn  = 1;
    q->expires_turn  = 13;
    q->target        = target_num;
    q->regionname    = settlement_name;
    quests.push_back(q);
    return q;
}

// Build a LOCAL_BUILD_ROAD quest for region `regionnum`, road `road_type`.
shared_ptr<Quest> make_road_quest(int regionnum, int road_type, int issuer_region,
                                   int tokens, const string& settlement_name = "Testburg")
{
    auto q = make_shared<Quest>();
    q->num           = 998;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_BUILD_ROAD;
    q->tokens        = tokens;
    q->issuer_unit   = 0;
    q->issuer_region = issuer_region;
    q->created_turn  = 1;
    q->expires_turn  = 13;
    q->target        = -1;
    q->regionnum     = regionnum;
    q->building      = road_type;
    q->regionname    = settlement_name;
    quests.push_back(q);
    return q;
}

// Build a LOCAL_BUILD_TOWER quest for region `regionnum`.
shared_ptr<Quest> make_tower_quest(int regionnum, int issuer_region, int tokens,
                                    const string& settlement_name = "Testburg")
{
    auto q = make_shared<Quest>();
    q->num           = 997;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_BUILD_TOWER;
    q->tokens        = tokens;
    q->issuer_unit   = 0;
    q->issuer_region = issuer_region;
    q->created_turn  = 1;
    q->expires_turn  = 13;
    q->target        = -1;
    q->regionnum     = regionnum;
    q->building      = O_TOWER;
    q->regionname    = settlement_name;
    quests.push_back(q);
    return q;
}

// Build a LOCAL_BUILD_INN quest for region `regionnum`.
shared_ptr<Quest> make_inn_quest(int regionnum, int issuer_region, int tokens,
                                  const string& settlement_name = "Testburg")
{
    auto q = make_shared<Quest>();
    q->num           = 996;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_BUILD_INN;
    q->tokens        = tokens;
    q->issuer_unit   = 0;
    q->issuer_region = issuer_region;
    q->created_turn  = 1;
    q->expires_turn  = 13;
    q->target        = -1;
    q->regionnum     = regionnum;
    q->building      = O_INN;
    q->regionname    = settlement_name;
    quests.push_back(q);
    return q;
}

} // namespace

// ---------------------------------------------------------------------------

ut::suite<"QuestCompletion"> quest_completion_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: check_kill_target — injects I_BOUNTY into spoils and erases quest
    // -----------------------------------------------------------------------
    "check_kill_target injects bounty into spoils and removes quest"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);

        // "Dead" monster unit — the one targeted by the quest.
        Faction *mf = helper.get_faction(helper.get_monfaction());
        Unit *monster = helper.create_unit(mf, r);
        monster->type = U_WMON;
        monster->items.SetNum(I_WOLF, 3);

        // Player unit as the "killer" (not used by check_kill_target directly).
        Faction *fac = helper.create_faction("Hunters");
        Unit *killer = helper.create_unit(fac, r);
        (void)killer;

        // Inject quest targeting the monster.
        make_hunt_quest(monster->num, r->num, 2, "Testburg");
        expect(quests.size() == 1_ul) << "quest must exist before completion";

        ItemList spoils;
        string quest_rewards;
        int issuer_region = -1;

        int result = quests.check_kill_target(monster, spoils, &quest_rewards, &issuer_region);

        expect(result == 1) << "check_kill_target must return 1 on match";
        expect(spoils.GetNum(I_BOUNTY) == 2) << "spoils must contain 2 I_BOUNTY tokens";
        expect(!quest_rewards.empty()) << "quest_rewards must be non-empty";
        expect(quest_rewards.find("Bounty Token") != string::npos)
            << "quest_rewards must mention Bounty Token";
        expect(issuer_region == r->num) << "issuer_region must be filled";
        expect(quests.size() == 0_ul) << "quest must be removed after completion";
    };

    // -----------------------------------------------------------------------
    // Test 2: check_kill_target — no match returns 0 and leaves quest intact
    // -----------------------------------------------------------------------
    "check_kill_target returns 0 on unit num mismatch"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *mf = helper.get_faction(helper.get_monfaction());
        Unit *monster = helper.create_unit(mf, r);
        Unit *other   = helper.create_unit(mf, r);

        // Quest targets `monster`, but we call with `other`.
        make_hunt_quest(monster->num, r->num, 1);
        expect(quests.size() == 1_ul);

        ItemList spoils;
        string quest_rewards;
        int result = quests.check_kill_target(other, spoils, &quest_rewards, nullptr);

        expect(result == 0) << "must not match a different unit";
        expect(spoils.GetNum(I_BOUNTY) == 0) << "no bounty for wrong unit";
        expect(quest_rewards.empty()) << "no rewards message for wrong unit";
        expect(quests.size() == 1_ul) << "quest must remain when no match";
    };

    // -----------------------------------------------------------------------
    // Test 3: check_road_quest — builder receives I_BOUNTY and quest is erased
    // -----------------------------------------------------------------------
    "check_road_quest gives bounty to builder and removes quest"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r);

        make_road_quest(r->num, O_ROADSE, r->num, 1, "Testburg");
        expect(quests.size() == 1_ul) << "road quest must exist";

        string quest_rewards;
        int result = quests.check_road_quest(r, O_ROADSE, builder, &quest_rewards,
                                              helper.get_regions());

        expect(result == 1) << "check_road_quest must return 1 on match";
        expect(builder->items.GetNum(I_BOUNTY) == 1) << "builder must receive 1 I_BOUNTY";
        expect(fac->quest_debts.count(r->num) > 0) << "quest_debts must be updated";
        expect(fac->quest_debts.at(r->num) == 1) << "quest_debts must owe 1 token";
        expect(!quest_rewards.empty()) << "quest_rewards must be non-empty";
        expect(quest_rewards.find("Road quest") != string::npos)
            << "quest_rewards must mention Road quest";
        expect(quests.size() == 0_ul) << "quest must be removed after completion";
    };

    // -----------------------------------------------------------------------
    // Test 4: check_road_quest — wrong road type returns 0
    // -----------------------------------------------------------------------
    "check_road_quest returns 0 on road type mismatch"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r);

        make_road_quest(r->num, O_ROADSE, r->num, 1);

        string quest_rewards;
        // Complete a DIFFERENT road type (O_ROADN ≠ O_ROADSE).
        int result = quests.check_road_quest(r, O_ROADN, builder, &quest_rewards,
                                              helper.get_regions());

        expect(result == 0) << "wrong road type must not complete quest";
        expect(builder->items.GetNum(I_BOUNTY) == 0) << "no bounty for wrong road";
        expect(quests.size() == 1_ul) << "quest must remain";
    };

    // -----------------------------------------------------------------------
    // Test 5: check_tower_quest — builder receives I_BOUNTY and quest is erased
    // -----------------------------------------------------------------------
    "check_tower_quest gives bounty to builder and removes quest"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r);

        make_tower_quest(r->num, r->num, 1, "Testburg");
        expect(quests.size() == 1_ul) << "tower quest must exist";

        string quest_rewards;
        int result = quests.check_tower_quest(r, builder, &quest_rewards,
                                               helper.get_regions());

        expect(result == 1) << "check_tower_quest must return 1 on match";
        expect(builder->items.GetNum(I_BOUNTY) == 1) << "builder must receive 1 I_BOUNTY";
        expect(fac->quest_debts.count(r->num) > 0) << "quest_debts must be updated";
        expect(fac->quest_debts.at(r->num) == 1) << "quest_debts must owe 1 token";
        expect(!quest_rewards.empty()) << "quest_rewards must be non-empty";
        expect(quest_rewards.find("Tower quest") != string::npos)
            << "quest_rewards must mention Tower quest";
        expect(quests.size() == 0_ul) << "quest must be removed after completion";
    };

    // -----------------------------------------------------------------------
    // Test 6: check_tower_quest — wrong region returns 0
    // -----------------------------------------------------------------------
    "check_tower_quest returns 0 on region mismatch"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r0 = helper.get_region(0, 0, 0);
        ARegion *r1 = helper.get_region(1, 1, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r0);

        // Quest is for r1, but builder completes tower in r0.
        if (r1) make_tower_quest(r1->num, r0->num, 1);

        string quest_rewards;
        int result = quests.check_tower_quest(r0, builder, &quest_rewards,
                                               helper.get_regions());

        expect(result == 0) << "different region must not complete quest";
        expect(builder->items.GetNum(I_BOUNTY) == 0) << "no bounty for wrong region";
        if (r1) expect(quests.size() == 1_ul) << "quest must remain";
    };

    // -----------------------------------------------------------------------
    // Test 6.5a: check_inn_quest — builder receives I_BOUNTY and quest is erased
    // -----------------------------------------------------------------------
    "check_inn_quest gives bounty to builder and removes quest"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r);

        make_inn_quest(r->num, r->num, 1, "Testburg");
        expect(quests.size() == 1_ul) << "inn quest must exist";

        string quest_rewards;
        int result = quests.check_inn_quest(r, builder, &quest_rewards,
                                             helper.get_regions());

        expect(result == 1) << "check_inn_quest must return 1 on match";
        expect(builder->items.GetNum(I_BOUNTY) == 1) << "builder must receive 1 I_BOUNTY";
        expect(fac->quest_debts.count(r->num) > 0) << "quest_debts must be updated";
        expect(fac->quest_debts.at(r->num) == 1) << "quest_debts must owe 1 token";
        expect(!quest_rewards.empty()) << "quest_rewards must be non-empty";
        expect(quest_rewards.find("Inn quest") != string::npos)
            << "quest_rewards must mention Inn quest";
        expect(quests.size() == 0_ul) << "quest must be removed after completion";
    };

    // -----------------------------------------------------------------------
    // Test 6.5b: check_inn_quest — wrong region returns 0
    // -----------------------------------------------------------------------
    "check_inn_quest returns 0 on region mismatch"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r0 = helper.get_region(0, 0, 0);
        ARegion *r1 = helper.get_region(1, 1, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r0);

        // Quest is for r1, but builder completes inn in r0.
        if (r1) make_inn_quest(r1->num, r0->num, 1);

        string quest_rewards;
        int result = quests.check_inn_quest(r0, builder, &quest_rewards,
                                             helper.get_regions());

        expect(result == 0) << "different region must not complete quest";
        expect(builder->items.GetNum(I_BOUNTY) == 0) << "no bounty for wrong region";
        if (r1) expect(quests.size() == 1_ul) << "quest must remain";
    };

    // -----------------------------------------------------------------------
    // Test 7: multiple tokens — reward scales correctly
    // -----------------------------------------------------------------------
    "quest reward scales with token count"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r);

        make_tower_quest(r->num, r->num, 5);

        string quest_rewards;
        quests.check_tower_quest(r, builder, &quest_rewards, helper.get_regions());

        expect(builder->items.GetNum(I_BOUNTY) == 5) << "builder must receive 5 I_BOUNTY";
        expect(fac->quest_debts.at(r->num) == 5) << "quest_debts must owe 5 tokens";
        expect(quest_rewards.find("Tokens") != string::npos ||
               quest_rewards.find("tokens") != string::npos)
            << "rewards string must use plural for >1 token";
    };
};
