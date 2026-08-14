#include "external/boost/ut.hpp"

#include "battle.h"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "unit.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Guard reputation for attacking the town's civic NPCs.
//
// RunBattle raises Faction::guard_attack_this_turn, and PostProcessTurn spends
// it: one battle or ten, the guards' attitude drops by exactly one step per turn
// and then recovers probabilistically. These tests cover the raising half - the
// ladder itself is a separate stage and is not re-tested here.
//
// The penalty used to be gated on a live U_GUARD/U_GUARDMAGE standing in the
// region, which let a garrison-less town be looted of its officials for free:
// a player who wiped the guards and then held the hex on GUARD kept them from
// regenerating (AdjustCityMons skips regeneration while a player guards), while
// a mayor inside a town hall keeps respawning regardless of guard strength.
// Killing that mayor every turn erased the town's local quests and cost the
// attacker nothing.
// ---------------------------------------------------------------------------

namespace {
    // Overwhelming force, so the battle resolves in one pass and the target dies.
    Unit *create_attacker(UnitTestHelper &helper, ARegion *r, const std::string& name)
    {
        Faction *player = helper.create_faction(name);
        Unit *u = helper.create_unit(player, r);
        u->items.SetNum(I_LEADERS, 500);
        u->items.SetNum(I_MSWORD, 500);
        helper.set_skill_level(u, S_COMBAT, 5);
        return u;
    }

    // A plain city guard of the guard faction, strong enough to be worth a battle.
    Unit *seed_guard(UnitTestHelper &helper, ARegion *r, int type)
    {
        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        Unit *g = helper.create_unit(gfac, r);
        g->type = type;
        g->guard = GUARD_GUARD;
        g->SetMen(I_LEADERS, 10);
        g->SetFlag(FLAG_BEHIND, 0);
        return g;
    }

    Unit *find_type(ARegion *r, int type)
    {
        for (const auto o : r->objects)
            for (const auto u : o->units)
                if (u->type == type && u->GetMen() > 0) return u;
        return nullptr;
    }
}

ut::suite<"Guard Reputation"> guard_reputation_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // The regression: a mayor with no garrison left is still the town's mayor.
    // -----------------------------------------------------------------------
    "killing a mayor with no guards left still costs reputation"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);  // no city, hence no seeded guards
        helper.spawn_mayor(r);

        Unit *mayor = find_type(r, U_MAYOR);
        expect(mayor != nullptr) << "test bootstrap must place a mayor";
        if (!mayor) return;

        expect(r->HasCityGuards() == 0_i) << "the scenario needs an empty garrison";

        Unit *attacker = create_attacker(helper, r, "Raider");
        expect(helper.run_battle(r, attacker, mayor) == BATTLE_WON) << "attacker must win";

        expect(attacker->faction->guard_attack_this_turn == 1_i)
            << "killing the mayor must be recorded against the attacker";
    };

    // -----------------------------------------------------------------------
    // Same for the commander, who does not flee when the rank and file die.
    // -----------------------------------------------------------------------
    "killing a lone guard commander still costs reputation"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        helper.spawn_guard_commander(r);

        Unit *commander = find_type(r, U_GUARDCOMMANDER);
        expect(commander != nullptr) << "test bootstrap must place a commander";
        if (!commander) return;

        Unit *attacker = create_attacker(helper, r, "Raider");
        expect(helper.run_battle(r, attacker, commander) == BATTLE_WON) << "attacker must win";

        expect(attacker->faction->guard_attack_this_turn == 1_i)
            << "killing the commander must be recorded against the attacker";
    };

    // -----------------------------------------------------------------------
    // The commander counts as a garrison in his own right.
    // -----------------------------------------------------------------------
    "a lone commander makes the region count as guarded"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        expect(r->HasCityGuards() == 0_i) << "region must start unguarded";

        // The mayor is not a garrison: GUARD_AVOID, fights only for his own faction.
        helper.spawn_mayor(r);
        expect(r->HasCityGuards() == 0_i) << "a mayor alone does not guard the town";

        helper.spawn_guard_commander(r);
        expect(r->HasCityGuards() == 1_i) << "a commander is a city guard";
        expect(r->GetCityGuard() != nullptr) << "a commander can speak for the guards";
    };

    // -----------------------------------------------------------------------
    // The flag is per turn, not per battle: the attitude ladder drops one step
    // however many officials fall in the same month.
    // -----------------------------------------------------------------------
    "two battles in one turn still cost a single step"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *guard = seed_guard(helper, r, U_GUARD);
        Unit *attacker = create_attacker(helper, r, "Raider");

        expect(helper.run_battle(r, attacker, guard) == BATTLE_WON) << "attacker must win";
        expect(attacker->faction->guard_attack_this_turn == 1_i) << "first battle records the attack";

        // Spawned only now, so the second battle is a real one and not a walkover
        // against a mayor who already fell defending the guard.
        helper.spawn_mayor(r);
        Unit *mayor = find_type(r, U_MAYOR);
        expect(mayor != nullptr) << "test bootstrap must place a mayor";
        if (!mayor) return;

        expect(helper.run_battle(r, attacker, mayor) == BATTLE_WON) << "attacker must win again";
        expect(attacker->faction->guard_attack_this_turn == 1_i)
            << "a second battle in the same turn must not stack";
    };

    // -----------------------------------------------------------------------
    // Control: a monster is nobody's official.
    // -----------------------------------------------------------------------
    "killing a monster in an unguarded region costs nothing"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *mob = helper.create_monster(r, I_KOBOLD, 20);
        Unit *attacker = create_attacker(helper, r, "Raider");

        expect(helper.run_battle(r, attacker, mob) == BATTLE_WON) << "attacker must win";
        expect(attacker->faction->guard_attack_this_turn == 0_i)
            << "monsters carry no civic standing";
    };
};
