// unittest/battle_city_guard_participation_test.cpp
// Which city garrison units join the defending side (Game::GetSides).
//
// The garrison is every type Unit::is_city_guard() names: the rank and file,
// the guard mages and the guard commander. Each of them must follow the same
// two rules, so a guard type added later cannot silently sit a battle out:
//   - an attack on a player the guard faction holds at NEUTRAL or better
//     brings the whole garrison standing in that hex to the player's side;
//   - an ADVANCE attack on any garrison unit brings in the rest of the garrison.
//
// Only the battle roster is asserted, never the outcome, so the dice do not matter.

#include "external/boost/ut.hpp"

#include "battle.h"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "unit.h"
#include "testhelper.hpp"

#include <algorithm>

namespace ut = boost::ut;

namespace {

Unit *create_attacker(UnitTestHelper &helper, ARegion *r)
{
    Faction *f = helper.create_faction("Raiders");
    Unit *u = helper.create_unit(f, r);
    u->items.SetNum(I_LEADERS, 20);
    u->items.SetNum(I_SWORD, 20);
    helper.set_skill_level(u, S_COMBAT, 2);
    return u;
}

// One unit of the guard faction of the given garrison type, standing in the
// region's open field like a real garrison does.
Unit *seed_garrison_unit(UnitTestHelper &helper, ARegion *r, int type)
{
    Faction *gfac = helper.get_faction(helper.get_guardfaction());
    Unit *g = helper.create_unit(gfac, r);
    g->type = type;
    g->guard = (type == U_GUARDMAGE) ? GUARD_NONE : GUARD_GUARD;
    g->SetMen(I_LEADERS, 1);
    return g;
}

bool on_defending_side(const Battle *b, const Unit *u)
{
    return std::any_of(b->defenders.begin(), b->defenders.end(),
                       [u](const BattleUnit &bu) { return bu.number == u->num; });
}

}  // namespace

ut::suite<"BattleCityGuardParticipation"> battle_city_guard_participation_suite = [] {
    using namespace ut;

    "the whole garrison, commander included, defends a NEUTRAL player"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *guard = seed_garrison_unit(helper, r, U_GUARD);
        Unit *mage = seed_garrison_unit(helper, r, U_GUARDMAGE);
        Unit *commander = seed_garrison_unit(helper, r, U_GUARDCOMMANDER);

        Faction *victim_faction = helper.create_faction("Victim");
        Unit *victim = helper.create_unit(victim_faction, r);
        Unit *attacker = create_attacker(helper, r);

        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        expect(gfac->get_attitude(victim_faction->num) == AttitudeType::NEUTRAL)
            << "test premise: the guard faction holds the victim at NEUTRAL";
        expect(helper.run_battle(r, attacker, victim) != BATTLE_IMPOSSIBLE);

        const Battle *b = helper.game_object().battles.back();
        expect(on_defending_side(b, victim)) << "the target always fights";
        expect(on_defending_side(b, guard)) << "the guardsman defends a NEUTRAL player";
        expect(on_defending_side(b, mage)) << "the guard mage defends a NEUTRAL player";
        expect(on_defending_side(b, commander)) << "the guard commander defends a NEUTRAL player";
    };

    "an ADVANCE attack on the commander brings in the rest of the garrison"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *guard = seed_garrison_unit(helper, r, U_GUARD);
        Unit *mage = seed_garrison_unit(helper, r, U_GUARDMAGE);
        Unit *commander = seed_garrison_unit(helper, r, U_GUARDCOMMANDER);
        Unit *attacker = create_attacker(helper, r);

        int result = helper.game_object().RunBattle(r, attacker, commander, 0, 1, TRIGGER_ADVANCE);
        expect(result != BATTLE_IMPOSSIBLE);

        const Battle *b = helper.game_object().battles.back();
        expect(on_defending_side(b, commander)) << "the target always fights";
        expect(on_defending_side(b, guard)) << "the guardsman joins when the commander is attacked";
        expect(on_defending_side(b, mage)) << "the guard mage joins when the commander is attacked";
    };
};
