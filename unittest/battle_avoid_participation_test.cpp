// unittest/battle_avoid_participation_test.cpp
// Who joins the defending side when a unit of the target's faction has AVOID set.
//
// Rule (Game::GetSides): a player unit joins only if it is the target, or the
// attacker can identify and catch it (Game::CanAttack). Where it stands does not
// matter - the open field, where every unit shares the region's dummy object, and
// a ship shared with the target follow the same rule. Monsters sharing the
// target's object always join; that side is covered by the pirate suites.
// See docs/BATTLE_PARTICIPATION_RULES.md.
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

Unit *create_attacker(UnitTestHelper &helper, ARegion *r, int observation)
{
    Faction *f = helper.create_faction("Raiders");
    Unit *u = helper.create_unit(f, r);
    u->items.SetNum(I_LEADERS, 20);
    u->items.SetNum(I_SWORD, 20);
    helper.set_skill_level(u, S_COMBAT, 2);
    if (observation > 0) helper.set_skill_level(u, S_OBSERVATION, observation);
    return u;
}

Unit *create_avoiding_unit(UnitTestHelper &helper, Faction *f, ARegion *r)
{
    Unit *u = helper.create_unit(f, r);
    u->guard = GUARD_AVOID;
    return u;
}

bool on_defending_side(const Battle *b, const Unit *u)
{
    return std::any_of(b->defenders.begin(), b->defenders.end(),
                       [u](const BattleUnit &bu) { return bu.number == u->num; });
}

}  // namespace

ut::suite<"BattleAvoidParticipation"> battle_avoid_participation_suite = [] {
    using namespace ut;

    "an unidentified AVOID unit in the open field stays out of the battle"_test = [] {
        // The attacker's observation 0 does not beat stealth 0, so the faction of
        // the bystander is unknown. Both stand in the region's dummy object.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Faction *defender = helper.create_faction("Defender");
        Unit *target = create_avoiding_unit(helper, defender, r);
        Unit *bystander = create_avoiding_unit(helper, defender, r);
        Unit *attacker = create_attacker(helper, r, 0);

        expect(bystander->object == target->object) << "test premise: both in the dummy object";
        expect(helper.run_battle(r, attacker, target) != BATTLE_IMPOSSIBLE);

        const Battle *b = helper.game_object().battles.back();
        expect(on_defending_side(b, target)) << "the target always fights";
        expect(!on_defending_side(b, bystander))
            << "an AVOID unit the attacker cannot identify stays out of the battle";
    };

    "an identified AVOID unit in the open field joins the battle"_test = [] {
        // Control for the case above: observation 1 beats stealth 0, and a unit
        // without mounts can always be caught, so CanAttack lets it in.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Faction *defender = helper.create_faction("Defender");
        Unit *target = create_avoiding_unit(helper, defender, r);
        Unit *bystander = create_avoiding_unit(helper, defender, r);
        Unit *attacker = create_attacker(helper, r, 1);

        expect(helper.run_battle(r, attacker, target) != BATTLE_IMPOSSIBLE);

        const Battle *b = helper.game_object().battles.back();
        expect(on_defending_side(b, bystander))
            << "an AVOID unit the attacker identifies and can catch joins the battle";
    };

    "an unidentified AVOID passenger on the target's ship stays out of the battle"_test = [] {
        // Same rule aboard a ship: being in the target's object is not enough on
        // its own for a player unit.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Faction *defender = helper.create_faction("Defender");
        Unit *target = create_avoiding_unit(helper, defender, r);
        helper.create_fleet(r, target, I_LONGSHIP, 1);
        Unit *passenger = create_avoiding_unit(helper, defender, r);
        passenger->MoveUnit(target->object);
        Unit *attacker = create_attacker(helper, r, 0);

        expect(target->object->IsFleet()) << "test premise: the target is aboard a fleet";
        expect(passenger->object == target->object) << "test premise: the passenger shares it";
        expect(helper.run_battle(r, attacker, target) != BATTLE_IMPOSSIBLE);

        const Battle *b = helper.game_object().battles.back();
        expect(!on_defending_side(b, passenger))
            << "an AVOID passenger the attacker cannot identify stays out of the battle";
    };
};
