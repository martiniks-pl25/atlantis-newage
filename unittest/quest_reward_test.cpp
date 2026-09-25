// unittest/quest_reward_test.cpp
// Tests for Task F: QUEST order reward bundle.
// Covers: default amount, category parsing, pool filtering, reward issuance,
// token/debt accounting, edge cases (empty pool, debt cap, pool vs budget),
// QUEST ... DISCOUNT and the order parser.

#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "quest_data.h"
#include "orders.h"
#include "items.h"
#include "unit.h"
#include "object.h"
#include "testhelper.hpp"
#include "ruleset_config.h"

#include <sstream>
#include <utility>

namespace ut = boost::ut;
using namespace std;

namespace {

// --- Helpers ------------------------------------------------------------------

Unit *find_mayor_in_hall(ARegion *r) {
    for (const auto o : r->objects) {
        if (o->type != O_TOWN_HALL || o->incomplete > 0) continue;
        for (const auto u : o->units)
            if (u->type == U_MAYOR && u->GetMen() > 0) return u;
    }
    return nullptr;
}

Object *find_town_hall(ARegion *r) {
    for (const auto o : r->objects)
        if (o->type == O_TOWN_HALL && o->incomplete <= 0) return o;
    return nullptr;
}

// Build a completed Town Hall with a mayor in region r.
pair<Object*, Unit*> setup_hall_with_mayor(UnitTestHelper& helper, ARegion *r) {
    helper.create_building(r, nullptr, O_TOWN_HALL);
    Object *hall = find_town_hall(r);
    if (hall) hall->incomplete = 0;
    helper.spawn_mayor(r, hall);
    return {hall, find_mayor_in_hall(r)};
}

// Give unit N bounty tokens and set faction debt for region.
void give_tokens_and_debt(Unit *u, ARegion *r, int tokens) {
    u->items.SetNum(I_BOUNTY, tokens);
    u->faction->quest_debts[r->num] = tokens;
}

// Issue a QUEST order on unit with given amount and category.
// Appends to the unit's order list (multiple QUEST orders per unit are allowed).
void issue_quest_order(Unit *u, int amount, QuestOrder::Category cat = QuestOrder::CAT_ANY,
                       bool discount = false) {
    auto *o = new QuestOrder;
    o->amount   = amount;
    o->category = cat;
    o->discount = discount;
    u->questorders.push_back(o);
}

// Parse one QUEST line for u through the real order parser.
void parse_quest(UnitTestHelper& helper, Unit *u, const string& line) {
    stringstream ss;
    ss << "#atlantis " << u->faction->num << " \"mypassword\"\n";
    ss << "unit " << u->num << "\n" << line << "\n";
    helper.parse_orders(u->faction->num, ss, nullptr);
}

// Possible mithril count for a redemption worth `units` hundredths of a token
// (full token = 100, discounted = discount_pct), from the active ruleset values.
pair<int, int> mithril_range(int units) {
    const QuestRewardRules& qr = ruleset_config().quests.reward;
    const int price = ItemDefs[I_MITHRIL].baseprice;
    const int lo = units * qr.token_value / 100;
    const int hi = lo + units * qr.token_variance / 100;
    return { max(1, lo / price), max(1, hi / price) };
}

// Total items in unit inventory except I_BOUNTY and I_LEADERS/I_MAN.
int count_reward_items(Unit *u) {
    int total = 0;
    for (const auto it : u->items) {
        if (it->type == I_BOUNTY) continue;
        if (it->type == I_LEADERS || it->type == I_MAN) continue;
        total += it->num;
    }
    return total;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

ut::suite<"QuestReward"> quest_reward_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: Default amount = 1 — turns in exactly 1 token
    "Default QUEST order turns in 1 token"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);

        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        give_tokens_and_debt(u, r, 5);
        issue_quest_order(u, 1);  // explicit 1 (matching default)

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 4)) << "1 token consumed";
        expect(eq(fac->quest_debts[r->num], 4)) << "debt reduced by 1";
    };

    // -----------------------------------------------------------------------
    // Test 2: Reward item issued — unit gets something from pool
    "QUEST order issues a reward item"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Ensure pools are populated.
        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);   // 200g
        AddRewardEquipment(I_MBAXE);    // 300g

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);

        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 1);
        issue_quest_order(u, 1, QuestOrder::CAT_EQUIPMENT);

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 0)) << "token consumed";
        expect(gt(count_reward_items(u), 0)) << "reward item issued";
    };

    // -----------------------------------------------------------------------
    // Test 3: RESOURCE category only gives resource items
    "CAT_RESOURCE gives only resource items"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardResource(I_MITHRIL);   // 100g
        AddRewardEquipment(I_MSWORD);   // 200g
        AddRewardMagic(I_SHIELDSTONE);  // 1000g

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 1);
        issue_quest_order(u, 1, QuestOrder::CAT_RESOURCE);

        helper.run_quest_orders();

        expect(gt(u->items.GetNum(I_MITHRIL), 0)) << "got mithril";
        expect(eq(u->items.GetNum(I_MSWORD), 0))  << "no sword";
        expect(eq(u->items.GetNum(I_SHIELDSTONE), 0)) << "no magic item";
    };

    // -----------------------------------------------------------------------
    // Test 4: EQUIPMENT category only gives equipment items
    "CAT_EQUIPMENT gives only equipment items"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardResource(I_MITHRIL);
        AddRewardEquipment(I_MBAXE);    // 300g
        AddRewardMagic(I_SHIELDSTONE);  // 1000g

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 1);
        issue_quest_order(u, 1, QuestOrder::CAT_EQUIPMENT);

        helper.run_quest_orders();

        expect(gt(u->items.GetNum(I_MBAXE), 0))      << "got mithril axe";
        expect(eq(u->items.GetNum(I_MITHRIL), 0))    << "no resource";
        expect(eq(u->items.GetNum(I_SHIELDSTONE), 0)) << "no magic item";
    };

    // -----------------------------------------------------------------------
    // Test 5: Debt cap — can't pay more than debt even with more tokens
    "Tokens paid capped by faction debt"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        // Unit has 10 tokens but debt is only 3.
        u->items.SetNum(I_BOUNTY, 10);
        fac->quest_debts[r->num] = 3;
        issue_quest_order(u, 10);  // try to pay all 10

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 7)) << "only 3 tokens consumed";
        expect(eq(fac->quest_debts.count(r->num), 0u)) << "local debt cleared";
    };

    // -----------------------------------------------------------------------
    // Test 6: No debt → error, tokens preserved, no reward
    "No debt: QUEST order fails with error"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        u->items.SetNum(I_BOUNTY, 5);
        // No quest_debts set.
        issue_quest_order(u, 1);

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 5)) << "tokens unchanged";
        expect(eq(count_reward_items(u), 0))     << "no reward issued";
    };

    // -----------------------------------------------------------------------
    // Test 7: No tokens → error, no reward
    "No tokens: QUEST order fails with error"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        fac->quest_debts[r->num] = 3;
        // No I_BOUNTY tokens.
        issue_quest_order(u, 1);

        helper.run_quest_orders();

        expect(eq(count_reward_items(u), 0)) << "no reward issued";
        expect(eq(fac->quest_debts[r->num], 3)) << "debt unchanged";
    };

    // -----------------------------------------------------------------------
    // Test 8: No Town Hall → error, tokens and debt preserved
    "No Town Hall: QUEST order fails"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        // No hall created.
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 3);
        issue_quest_order(u, 1);

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 3)) << "tokens preserved";
        expect(eq(fac->quest_debts[r->num], 3))  << "debt preserved";
        expect(eq(count_reward_items(u), 0))     << "no reward";
    };

    // -----------------------------------------------------------------------
    // Test 9: UNFRIENDLY mayor → refuses, tokens and debt preserved
    "Unfriendly mayor refuses QUEST order"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 3);

        // Set mayor's attitude to player faction as UNFRIENDLY.
        Faction *guard = helper.get_faction(helper.get_guardfaction());
        guard->set_attitude(fac->num, AttitudeType::UNFRIENDLY);

        issue_quest_order(u, 1);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 3)) << "tokens preserved";
        expect(eq(fac->quest_debts[r->num], 3))  << "debt preserved";
        expect(eq(count_reward_items(u), 0))     << "no reward";
    };

    // -----------------------------------------------------------------------
    // Test 10: Budget threshold — item above budget not selected
    "Items above token budget excluded from pool"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        // Only item: Aegis at 45000g — far above 1-token budget (~1000-1500g).
        AddRewardEquipment(I_AEGIS);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 1);
        issue_quest_order(u, 1, QuestOrder::CAT_EQUIPMENT);

        helper.run_quest_orders();

        // No item fits the budget: the order is refused before anything is spent.
        expect(eq(u->items.GetNum(I_BOUNTY), 1)) << "token kept";
        expect(eq(fac->quest_debts[r->num], 1))  << "debt kept";
        expect(eq(u->items.GetNum(I_AEGIS), 0))  << "aegis not given (too expensive)";
    };

    // -----------------------------------------------------------------------
    // Test 11: Multi-token reward — 5 tokens → budget ~5000-7500, more items
    "5 tokens gives proportionally more items"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);  // 200g

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 5);
        issue_quest_order(u, 5, QuestOrder::CAT_EQUIPMENT);

        helper.run_quest_orders();

        // Budget = 5000-7500, baseprice=200 → 25-37 swords.
        expect(ge(u->items.GetNum(I_MSWORD), 25)) << "at least 25 swords for 5 tokens";
        expect(eq(u->items.GetNum(I_BOUNTY), 0))  << "all tokens consumed";
    };

    // -----------------------------------------------------------------------
    // Test 12: Global + local debt — local consumed first
    "Local debt consumed before global debt"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        u->items.SetNum(I_BOUNTY, 3);
        fac->quest_debts[r->num] = 2;  // local
        fac->quest_debts[-1]     = 5;  // global

        issue_quest_order(u, 3);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 0))    << "3 tokens consumed";
        expect(eq(fac->quest_debts.count(r->num), 0u)) << "local debt cleared";
        expect(eq(fac->quest_debts[-1], 4))         << "1 token taken from global";
    };

    // -----------------------------------------------------------------------
    // Test 13: Magic pool empty for low tokens (items all above budget)
    "Magic pool out of budget falls back to the advanced pool"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        // Magic pool only has expensive items; equipment pool has cheap ones.
        AddRewardMagic(I_AEGIS);       // 45000g — way above 1-token budget
        AddRewardEquipment(I_MITHRIL); // 100g — always fits

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 1);

        // Force CAT_ANY to always pick magic branch by running many times.
        // We verify Aegis never appears (can't afford it with 1 token).
        int aegis_count = 0;
        for (int trial = 0; trial < 20; trial++) {
            // Reset state each trial.
            u->items.SetNum(I_BOUNTY, 1);
            u->items.SetNum(I_AEGIS, 0);
            u->items.SetNum(I_MITHRIL, 0);
            fac->quest_debts[r->num] = 1;
            issue_quest_order(u, 1, QuestOrder::CAT_ANY);
            helper.run_quest_orders();
            aegis_count += u->items.GetNum(I_AEGIS);
        }
        expect(eq(aegis_count, 0)) << "Aegis never given with 1 token";
        // Each trial resets the inventory, so the last one must have been paid
        // whichever pool the roll picked.
        expect(eq(u->items.GetNum(I_BOUNTY), 0))  << "last trial redeemed its token";
        expect(gt(u->items.GetNum(I_MITHRIL), 0)) << "fallback reward from the advanced pool";
    };

    // -----------------------------------------------------------------------
    // Test 14: QuestOrder default amount = 1 via parser parse path
    "QuestOrder default amount field is 1"_test = [] {
        QuestOrder o;
        expect(eq(o.amount, 1))                         << "default amount = 1";
        expect(eq(o.category, QuestOrder::CAT_ANY))     << "default category = CAT_ANY";
        expect(!o.discount)                             << "default: no discount";
    };

    // -----------------------------------------------------------------------
    // Test 15: Mayor outside Town Hall → no redemption
    "Mayor outside Town Hall is not accepted"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        (void)hall;
        expect(mayor != nullptr);

        // Move mayor out of the hall into the dummy (region root) object.
        mayor->MoveUnit(r->GetDummy());
        expect(eq(find_mayor_in_hall(r), nullptr)) << "mayor no longer in hall";

        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 2);
        issue_quest_order(u, 1);

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 2)) << "tokens preserved";
        expect(eq(fac->quest_debts[r->num], 2))  << "debt preserved";
        expect(eq(count_reward_items(u), 0))     << "no reward";
    };

    // -----------------------------------------------------------------------
    // Test 16: Incomplete Town Hall → no redemption
    "Incomplete Town Hall is not accepted"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        // Create hall but leave it incomplete.
        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (hall) hall->incomplete = 5;  // still under construction
        helper.spawn_mayor(r, hall);

        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 2);
        issue_quest_order(u, 1);

        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 2)) << "tokens not consumed";
        expect(eq(count_reward_items(u), 0))     << "no reward";
    };

    // -----------------------------------------------------------------------
    // Test 17: Multiple QUEST orders on one unit are all processed in sequence
    "Multiple QUEST orders on one unit redeem sequentially"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        u->items.SetNum(I_BOUNTY, 5);
        fac->quest_debts[r->num] = 5;

        // Two separate redemptions of 2 tokens each.
        issue_quest_order(u, 2);
        issue_quest_order(u, 2);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 1)) << "4 of 5 tokens consumed across both orders";
        expect(eq(fac->quest_debts[r->num], 1))  << "debt reduced by 4";
    };

    // -----------------------------------------------------------------------
    // Test 18: Later QUEST orders see depleted tokens (no limit on count)
    "Extra QUEST orders are no-ops once tokens are exhausted"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ClearRewardPools();
        AddRewardEquipment(I_MSWORD);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);

        u->items.SetNum(I_BOUNTY, 3);
        fac->quest_debts[r->num] = 10;

        // Three orders of 2: pays 2, then 1, then nothing left.
        issue_quest_order(u, 2);
        issue_quest_order(u, 2);
        issue_quest_order(u, 2);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 0))  << "all 3 tokens consumed";
        expect(eq(fac->quest_debts[r->num], 7))   << "debt reduced by 3";
        expect(u->questorders.empty())            << "all quest orders consumed";
    };

    // =======================================================================
    // Order parser: QUEST [amount] [RESOURCE|EQUIPMENT] [DISCOUNT]
    // =======================================================================

    "Parser: DISCOUNT and category in any order"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 0, 0);
        Unit *u = helper.create_unit(helper.create_faction("Player"), r);

        parse_quest(helper, u, "quest discount 3 eqp");

        expect(eq(u->questorders.size(), 1u)) << "order accepted";
        if (u->questorders.size() != 1) return;
        const QuestOrder *o = u->questorders.front();
        expect(eq(o->amount, 3));
        expect(eq(o->category, QuestOrder::CAT_EQUIPMENT));
        expect(o->discount) << "DISCOUNT flag set";
    };

    "Parser: bare QUEST is 1 token, any category, no discount"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 0, 0);
        Unit *u = helper.create_unit(helper.create_faction("Player"), r);

        parse_quest(helper, u, "quest");

        expect(eq(u->questorders.size(), 1u)) << "order accepted";
        if (u->questorders.size() != 1) return;
        const QuestOrder *o = u->questorders.front();
        expect(eq(o->amount, 1));
        expect(eq(o->category, QuestOrder::CAT_ANY));
        expect(!o->discount) << "DISCOUNT only when written";
    };

    "Parser: category alone is not read as the amount"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 0, 0);
        Unit *u = helper.create_unit(helper.create_faction("Player"), r);

        parse_quest(helper, u, "quest resource");

        expect(eq(u->questorders.size(), 1u)) << "order accepted";
        if (u->questorders.size() != 1) return;
        expect(eq(u->questorders.front()->amount, 1));
        expect(eq(u->questorders.front()->category, QuestOrder::CAT_RESOURCE));
    };

    "Parser: amount 0 means 1"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 0, 0);
        Unit *u = helper.create_unit(helper.create_faction("Player"), r);

        parse_quest(helper, u, "quest 0 discount");

        expect(eq(u->questorders.size(), 1u)) << "order accepted";
        if (u->questorders.size() != 1) return;
        expect(eq(u->questorders.front()->amount, 1));
        expect(u->questorders.front()->discount);
    };

    "Parser: unknown keyword rejects the order"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ARegion *r = helper.get_region(0, 0, 0);
        Unit *u = helper.create_unit(helper.create_faction("Player"), r);

        parse_quest(helper, u, "quest 5 half");

        expect(u->questorders.empty()) << "a typo must not become a plain redemption";
    };

    // =======================================================================
    // DISCOUNT redemption
    // =======================================================================

    "DISCOUNT: tokens covered by debt stay full value"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        give_tokens_and_debt(u, r, 5);

        issue_quest_order(u, 5, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        const auto [lo, hi] = mithril_range(500);
        expect(eq(u->items.GetNum(I_BOUNTY), 0)) << "all tokens turned in";
        expect(eq(fac->quest_debts.count(r->num), 0u)) << "debt spent";
        expect(ge(u->items.GetNum(I_MITHRIL), lo)) << "full value, not discounted";
        expect(le(u->items.GetNum(I_MITHRIL), hi));
    };

    "No debt without DISCOUNT: nothing spent"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 4);

        issue_quest_order(u, 4, QuestOrder::CAT_RESOURCE);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 4)) << "tokens kept";
        expect(eq(u->items.GetNum(I_MITHRIL), 0)) << "no reward";
        expect(fac->quest_debts.empty()) << "no debt appears";
    };

    "DISCOUNT without debt pays discount_pct"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 4);

        issue_quest_order(u, 4, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        const int pct = ruleset_config().quests.reward.discount_pct;
        const auto [lo, hi] = mithril_range(4 * pct);
        expect(eq(u->items.GetNum(I_BOUNTY), 0)) << "tokens turned in";
        expect(ge(u->items.GetNum(I_MITHRIL), lo));
        expect(le(u->items.GetNum(I_MITHRIL), hi)) << "discounted, not full value";
        expect(fac->quest_debts.empty()) << "no debt touched or created";
    };

    "DISCOUNT mixed: local, then global, then discounted"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 10);
        fac->quest_debts[r->num] = 4;
        fac->quest_debts[-1] = 2;

        issue_quest_order(u, 10, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        // 6 full-value tokens + 4 discounted.
        const int pct = ruleset_config().quests.reward.discount_pct;
        const auto [lo, hi] = mithril_range(600 + 4 * pct);
        expect(eq(u->items.GetNum(I_BOUNTY), 0));
        expect(eq(fac->quest_debts.count(r->num), 0u)) << "local debt spent";
        expect(eq(fac->quest_debts.count(-1), 0u))     << "global debt spent";
        expect(ge(u->items.GetNum(I_MITHRIL), lo));
        expect(le(u->items.GetNum(I_MITHRIL), hi));
    };

    "DISCOUNT spends the global pool at full value before discounting"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 5);
        fac->quest_debts[-1] = 3;  // global bounty only, nothing owed by this town

        issue_quest_order(u, 5, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        const int pct = ruleset_config().quests.reward.discount_pct;
        const auto [lo, hi] = mithril_range(300 + 2 * pct);
        expect(eq(u->items.GetNum(I_BOUNTY), 0));
        expect(eq(fac->quest_debts.count(-1), 0u)) << "global debt spent";
        expect(ge(u->items.GetNum(I_MITHRIL), lo));
        expect(le(u->items.GetNum(I_MITHRIL), hi));
    };

    "Plain QUEST keeps uncovered tokens"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 10);
        fac->quest_debts[r->num] = 6;

        issue_quest_order(u, 10, QuestOrder::CAT_RESOURCE);
        helper.run_quest_orders();

        const auto [lo, hi] = mithril_range(600);
        expect(eq(u->items.GetNum(I_BOUNTY), 4)) << "4 uncovered tokens kept";
        expect(eq(fac->quest_debts.count(r->num), 0u));
        expect(ge(u->items.GetNum(I_MITHRIL), lo));
        expect(le(u->items.GetNum(I_MITHRIL), hi)) << "only the covered 6 were paid";
    };

    "DISCOUNT capped by the tokens the unit carries"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 2);
        fac->quest_debts[r->num] = 5;

        issue_quest_order(u, 50, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 0)) << "never below zero";
        expect(eq(fac->quest_debts[r->num], 3))  << "only 2 debt spent; the rest stays owed";
    };

    "DISCOUNT refused by an unfriendly mayor"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 3);

        Faction *guard = helper.get_faction(helper.get_guardfaction());
        guard->set_attitude(fac->num, AttitudeType::UNFRIENDLY);

        issue_quest_order(u, 3, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 3)) << "nothing spent";
        expect(eq(count_reward_items(u), 0))     << "no reward";
    };

    "DISCOUNT with no affordable reward keeps the tokens"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardEquipment(I_AEGIS);  // far above any 1-token budget

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 1);

        issue_quest_order(u, 1, QuestOrder::CAT_EQUIPMENT, true);
        helper.run_quest_orders();

        expect(eq(u->items.GetNum(I_BOUNTY), 1)) << "token kept";
        expect(eq(count_reward_items(u), 0))     << "no reward";
    };

    "Debt owed by another town is not spent here"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        const int other_town = r->num + 1;  // debt key of some other settlement
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *u = helper.create_unit(fac, r);
        u->items.SetNum(I_BOUNTY, 3);
        fac->quest_debts[other_town] = 3;

        issue_quest_order(u, 3, QuestOrder::CAT_RESOURCE);        // plain: refused
        issue_quest_order(u, 3, QuestOrder::CAT_RESOURCE, true);  // DISCOUNT: paid at discount
        helper.run_quest_orders();

        const int pct = ruleset_config().quests.reward.discount_pct;
        const auto [lo, hi] = mithril_range(3 * pct);
        expect(eq(u->items.GetNum(I_BOUNTY), 0)) << "DISCOUNT order took the tokens";
        expect(eq(fac->quest_debts[other_town], 3)) << "other town's debt untouched";
        expect(ge(u->items.GetNum(I_MITHRIL), lo));
        expect(le(u->items.GetNum(I_MITHRIL), hi));
    };

    "Two units of one faction share the debt, never overspend it"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ClearRewardPools();
        AddRewardResource(I_MITHRIL);

        ARegion *r = helper.get_region(0, 0, 0);
        setup_hall_with_mayor(helper, r);
        Faction *fac = helper.create_faction("Player");
        Unit *a = helper.create_unit(fac, r);
        Unit *b = helper.create_unit(fac, r);
        a->items.SetNum(I_BOUNTY, 3);
        b->items.SetNum(I_BOUNTY, 3);
        fac->quest_debts[r->num] = 3;

        issue_quest_order(a, 3, QuestOrder::CAT_RESOURCE, true);
        issue_quest_order(b, 3, QuestOrder::CAT_RESOURCE, true);
        helper.run_quest_orders();

        // Whichever unit runs first takes the debt; the other is paid at discount.
        const int pct = ruleset_config().quests.reward.discount_pct;
        const auto [full_lo, full_hi] = mithril_range(300);
        const auto [disc_lo, disc_hi] = mithril_range(3 * pct);
        const int total = a->items.GetNum(I_MITHRIL) + b->items.GetNum(I_MITHRIL);
        expect(eq(a->items.GetNum(I_BOUNTY) + b->items.GetNum(I_BOUNTY), 0));
        expect(eq(fac->quest_debts.count(r->num), 0u)) << "debt spent once, not twice";
        expect(ge(total, full_lo + disc_lo)) << "3 full + 3 discounted between the two units";
        expect(le(total, full_hi + disc_hi));
    };
};
