#include "external/boost/ut.hpp"
#include "battle.h"
#include "testhelper.hpp"
#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "aregion.h"
#include "items.h"

#include <utility>
#include <vector>

namespace ut = boost::ut;

// Cover for the tier-slot rule in Army::GetMonSpoils(): a monster whose spoiltype
// names several tiers fills its opening pool slots in the fixed order
// IT_NORMAL -> IT_ADVANCED -> IT_MAGIC -> IT_TRADE, so a crew that dies in numbers
// always pays one of every tier it names instead of collapsing into the cheapest.
// Trade goods drop one unit at a time, and an unmarketable one never drops at all.
//
// The unit-test ruleset (unittest/extra.cpp) leaves PIRA at its gamedata default of
// a single tier, so each test sets the spoiltype it wants explicitly.

// PIRA's gamedata defaults (gamedata.cpp), restored after a test that changes them.
static const int PIRA_DEFAULT_SILVER = 300;
static const int PIRA_DEFAULT_SPOILS = IT_ADVANCED;

// modify_monster_spoils() writes the global MonDefs table, which outlives the
// UnitTestHelper: without this the bestiary would leak into whichever suite the
// linker happens to run next.
struct PiraSpoilsGuard {
    UnitTestHelper &helper;
    ~PiraSpoilsGuard()
    {
        helper.game_object().modify_monster_spoils("PIRA", PIRA_DEFAULT_SILVER, PIRA_DEFAULT_SPOILS);
    }
};

// item_description() returns an empty string for a disabled item, and the unit-test
// ruleset enables nothing, so a monster has to be switched on before its text can be
// read - and switched back, ItemDefs being global.
struct ItemFlagsGuard {
    std::vector<std::pair<int, int>> saved;
    void enable(UnitTestHelper &helper, int item)
    {
        saved.push_back({item, ItemDefs[item].flags});
        helper.game_object().EnableItem(item);
    }
    ~ItemFlagsGuard()
    {
        for (auto &p : saved) ItemDefs[p.first].flags = p.second;
    }
};

// ItemDefs is global too, so a test that reshapes the trade tier puts it back.
struct TradeFlagsGuard {
    std::vector<std::pair<int, int>> saved;
    TradeFlagsGuard()
    {
        for (int i = 0; i < NITEMS; i++)
            if (ItemDefs[i].type & IT_TRADE) saved.push_back({i, ItemDefs[i].flags});
    }
    ~TradeFlagsGuard()
    {
        for (auto &p : saved) ItemDefs[p.first].flags = p.second;
    }
};

// Sums items of the given tier across a faction's units in a region, skipping
// silver: I_SILVER is IT_NORMAL and can legitimately occupy the normal slot, but it
// also arrives as the separate cash drop, so counting it would not tell the two apart.
static int faction_tier_count(ARegion *r, Faction *f, int tier_mask)
{
    int total = 0;
    for (auto obj : r->objects)
        for (auto u : obj->units)
            if (u->faction == f)
                for (int i = 0; i < NITEMS; i++)
                    if (i != I_SILVER && u->items.GetNum(i) > 0 &&
                        (ItemDefs[i].type & tier_mask))
                        total += u->items.GetNum(i);
    return total;
}

// Sums one specific item across a faction's units in a region.
static int faction_item_count(ARegion *r, Faction *f, int item)
{
    int total = 0;
    for (auto obj : r->objects)
        for (auto u : obj->units)
            if (u->faction == f) total += u->items.GetNum(item);
    return total;
}

// Counts monsters of the given race still standing, ignoring the player's own units.
static int surviving_race(ARegion *r, Faction *player, int race)
{
    int total = 0;
    for (auto obj : r->objects)
        for (auto u : obj->units)
            if (u->faction != player) total += u->items.GetNum(race);
    return total;
}

// An attacker armed far past what the fight needs: a crew that routs leaves
// survivors, and survivors pay no spoils, so an under-armed hunter would make the
// test measure the rout instead of the loot.
static Unit *create_armed_hunter(UnitTestHelper &helper, ARegion *r)
{
    Faction *player = helper.create_faction("Player");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 500);
    u->items.SetNum(I_MSWORD, 500);
    helper.set_skill_level(u, S_COMBAT, 5);
    return u;
}

// Strips the trade tier down to one unmarketable good. If the NOMARKET guard were
// missing, tarot cards would be the only thing the trade slot could reach, so the
// assertion below discriminates instead of passing by luck.
static void leave_only_unmarketable_trade(UnitTestHelper &helper)
{
    for (int i = 0; i < NITEMS; i++)
        if (ItemDefs[i].type & IT_TRADE) helper.game_object().DisableItem(i);
    helper.game_object().ModifyItemFlags(I_TAROTCARDS, ItemType::NOMARKET);
}

ut::suite<"PirateMixedSpoils"> pirate_mixed_spoils_suite = [] {
    using namespace ut;

    "a mixed-tier crew pays one of every tier it names"_test = [] {
        UnitTestHelper helper;
        PiraSpoilsGuard guard{helper};
        helper.initialize_game();
        helper.setup_turn();
        helper.game_object().modify_monster_spoils("PIRA", 250, IT_NORMAL | IT_ADVANCED | IT_TRADE);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        pirates->free = 0;  // Elite: full spoils

        Unit *attacker = create_armed_hunter(helper, r);
        Faction *player = attacker->faction;

        int normal_before = faction_tier_count(r, player, IT_NORMAL);
        int advanced_before = faction_tier_count(r, player, IT_ADVANCED);
        int trade_before = faction_tier_count(r, player, IT_TRADE);

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(faction_tier_count(r, player, IT_NORMAL) > normal_before)
            << "a three-tier pool must yield a normal item";
        expect(faction_tier_count(r, player, IT_ADVANCED) > advanced_before)
            << "a three-tier pool must yield an advanced item";
        expect(faction_tier_count(r, player, IT_TRADE) > trade_before)
            << "a three-tier pool must yield a trade good";
    };

    // Three figures fill exactly the three tiers PIRA names, one figure each, so the
    // trade slot is reached by exactly one pirate. With trade quantity capped at one
    // that pins the drop to a single card; without the cap the same figure would pay
    // val/baseprice of them - several at a trade good's price.
    "a trade good drops one at a time"_test = [] {
        UnitTestHelper helper;
        PiraSpoilsGuard guard{helper};
        helper.initialize_game();
        helper.setup_turn();
        helper.game_object().modify_monster_spoils("PIRA", 250, IT_NORMAL | IT_ADVANCED | IT_TRADE);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 3);
        pirates->free = 0;

        Unit *attacker = create_armed_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);
        expect(surviving_race(r, player, I_PIRATES) == 0_i)
            << "every figure must die, or the slot count is not three";

        expect(faction_tier_count(r, player, IT_TRADE) == 1_i)
            << "one pirate reached the trade slot, so exactly one trade good is owed";
    };

    "an unmarketable trade good never drops"_test = [] {
        UnitTestHelper helper;
        PiraSpoilsGuard guard{helper};
        TradeFlagsGuard flags;
        helper.initialize_game();
        helper.setup_turn();
        helper.game_object().modify_monster_spoils("PIRA", 250, IT_NORMAL | IT_ADVANCED | IT_TRADE);
        leave_only_unmarketable_trade(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        pirates->free = 0;

        Unit *attacker = create_armed_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(faction_item_count(r, player, I_TAROTCARDS) == 0_i)
            << "a trade good no market stocks is dead loot and must never be picked";
        expect(faction_tier_count(r, player, IT_ADVANCED) > 0_i)
            << "the unreachable trade tier must fall through, not swallow the drop";
    };

    // Officers key their own pool and keep their own single-tier spoiltype, so the
    // crew's trade tier must not follow them.
    "an officer keeps its own advanced-only loot"_test = [] {
        UnitTestHelper helper;
        PiraSpoilsGuard guard{helper};
        helper.initialize_game();
        helper.setup_turn();
        helper.game_object().modify_monster_spoils("PIRA", 250, IT_NORMAL | IT_ADVANCED | IT_TRADE);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 1);
        pirates->free = 0;
        Unit *captain = helper.create_npc_pirate_captain(r, pirates->object);
        captain->free = 0;

        Unit *attacker = create_armed_hunter(helper, r);
        Faction *player = attacker->faction;

        int advanced_before = faction_tier_count(r, player, IT_ADVANCED);
        int trade_before = faction_tier_count(r, player, IT_TRADE);

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);
        expect(surviving_race(r, player, I_PIRATE_CAPTAIN) == 0_i)
            << "the captain must fall, or it pays nothing to measure";

        expect(faction_tier_count(r, player, IT_ADVANCED) > advanced_before)
            << "a captain's spoiltype is IT_ADVANCED and it died grown";
        // One pirate reaches only the first slot, and the captain's own pool names a
        // single tier - so nothing here can pay a trade good.
        expect(faction_tier_count(r, player, IT_TRADE) == trade_before)
            << "an officer must not inherit the crew's trade tier";
    };

    // The description is built from the same mask, and its tier chain used to match
    // first-wins - a three-tier pirate advertised "advanced items" and nothing else.
    "the item description names every tier the mask carries"_test = [] {
        UnitTestHelper helper;
        PiraSpoilsGuard guard{helper};
        helper.initialize_game();
        helper.setup_turn();
        helper.game_object().modify_monster_spoils("PIRA", 200, IT_NORMAL | IT_ADVANCED | IT_TRADE);

        ItemFlagsGuard flags;
        flags.enable(helper, I_PIRATES);

        std::string d = item_description(I_PIRATES, 1);
        expect(d.find("normal, advanced or trade items and silver") != std::string::npos)
            << "three-tier mask must advertise all three: " << d;
    };

    // A pure IT_NORMAL monster reaches trade goods only through the substitution in
    // GetMonSpoils, which SPOILS_NO_TRADE turns off - so the text has to follow it.
    "a normal-tier monster promises trade goods only where the substitution is live"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ItemFlagsGuard flags;
        flags.enable(helper, I_MERFOLK);

        int saved = Globals->SPOILS_NO_TRADE;

        Globals->SPOILS_NO_TRADE = 0;
        std::string live = item_description(I_MERFOLK, 1);
        expect(live.find("normal or trade items and silver") != std::string::npos)
            << "with the substitution on, trade goods are reachable: " << live;

        Globals->SPOILS_NO_TRADE = 1;
        std::string off = item_description(I_MERFOLK, 1);
        expect(off.find("normal items and silver") != std::string::npos)
            << "with the substitution off, only the named tier is promised: " << off;
        expect(off.find("trade") == std::string::npos)
            << "no trade good can drop, so none may be advertised: " << off;

        Globals->SPOILS_NO_TRADE = saved;
    };

    "a single-tier crew still draws from its own tier only"_test = [] {
        UnitTestHelper helper;
        PiraSpoilsGuard guard{helper};
        helper.initialize_game();
        helper.setup_turn();
        helper.game_object().modify_monster_spoils("PIRA", 250, IT_ADVANCED);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        pirates->free = 0;

        Unit *attacker = create_armed_hunter(helper, r);
        Faction *player = attacker->faction;

        int normal_before = faction_tier_count(r, player, IT_NORMAL);
        int advanced_before = faction_tier_count(r, player, IT_ADVANCED);
        int trade_before = faction_tier_count(r, player, IT_TRADE);

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(faction_tier_count(r, player, IT_ADVANCED) > advanced_before)
            << "an advanced-tier crew still drops advanced items";
        expect(faction_tier_count(r, player, IT_NORMAL) == normal_before)
            << "a single-tier crew must not reach the normal tier";
        expect(faction_tier_count(r, player, IT_TRADE) == trade_before)
            << "a single-tier crew must not reach the trade tier";
    };
};
