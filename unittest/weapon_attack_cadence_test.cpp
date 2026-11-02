// unittest/weapon_attack_cadence_test.cpp
//
// A negative WeaponType::numAttacks means "one attack every N rounds" (items.h).
// Battle::DoAttack used to test `round % N == 1`, which is never true when N == 1,
// so a weapon with -1 - the javelin, the only such weapon in the table - never
// attacked at all. These are end-to-end battles on both sides of the formula: the
// javelin must land hits now, and a weapon with N > 1 must keep attacking.

#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

namespace {

// The unittest ruleset leaves the javelin DISABLED (the live one enables it).
// Enable it for the test and put the table entry back afterwards.
struct ScopedItemEnabled {
    int item;
    int saved_flags;
    explicit ScopedItemEnabled(int item) : item(item), saved_flags(ItemDefs[item].flags) {
        ItemDefs[item].flags &= ~ItemType::DISABLED;
    }
    ~ScopedItemEnabled() { ItemDefs[item].flags = saved_flags; }
};

// 40 soldiers carrying one `weapon` each, with the weapon's skill at level 2.
Unit *armed_unit(UnitTestHelper &helper, ARegion *r, const char *name, int weapon, int skill)
{
    Faction *f = helper.create_faction(name);
    Unit *u = helper.create_unit(f, r);
    u->items.SetNum(I_LEADERS, 40);
    u->items.SetNum(weapon, 40);
    helper.set_skill_level(u, skill, 2);
    return u;
}

// 10 men with no weapon: they fight bare-handed, so they can hurt the attacker
// but can never be the reason the attacker deals damage.
Unit *unarmed_unit(UnitTestHelper &helper, ARegion *r, const char *name)
{
    Faction *f = helper.create_faction(name);
    Unit *u = helper.create_unit(f, r);
    u->items.SetNum(I_LEADERS, 10);
    return u;
}

// A fight against `target` must leave it with fewer men than it started with.
// Under the javelin bug the target takes no damage at all and keeps its men.
void expect_battle_damages(UnitTestHelper &helper, ARegion *r, Unit *attacker,
                           Unit *target, const char *what)
{
    using namespace boost::ut;
    const int before = target->GetMen();
    const auto battles_before = helper.game_object().battles.size();

    helper.run_battle(r, attacker, target);

    expect(helper.game_object().battles.size() > battles_before) << "the battle did not run";
    expect(target->GetMen() < before) << what;
}

// A battle region: not safe, with no NPC units to drag into the fight.
ARegion *battle_region(UnitTestHelper &helper)
{
    using namespace boost::ut;
    ARegion *r = helper.get_region(0, 2, 0);
    expect(r != nullptr) << "test bootstrap needs a region";
    if (!r) return nullptr;
    expect(r->IsSafeRegion() == 0_i) << "battles must be allowed here";
    helper.clear_npc_units_in_region(r);
    return r;
}

} // namespace

ut::suite<"WeaponAttackCadence"> weapon_attack_cadence_suite = [] {
    using namespace ut;

    // The regression: under the old formula the javelin unit dealt no damage at
    // all and the target ended the fight with its full strength.
    "a javelin attacks every round (numAttacks -1)"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ScopedItemEnabled javelin(I_JAVELIN);

        ARegion *r = battle_region(helper);
        if (!r) return;

        Unit *jav = armed_unit(helper, r, "Throwers", I_JAVELIN, S_COMBAT);
        Unit *target = unarmed_unit(helper, r, "Dummies");
        expect(jav->GetMen() == 40_i) << "the tester must carry the javelins";

        expect_battle_damages(helper, r, jav, target,
            "the javelin unit dealt no damage: -1 numAttacks must attack every round");
    };

    // The other branch of the formula: N > 1 must keep its cadence, so the
    // every-third-round crossbow still lands hits.
    "a crossbow still attacks (numAttacks -3)"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = battle_region(helper);
        if (!r) return;

        Unit *xb = armed_unit(helper, r, "Crossbows", I_CROSSBOW, S_CROSSBOW);
        Unit *target = unarmed_unit(helper, r, "Dummies");

        expect_battle_damages(helper, r, xb, target,
            "a crossbow must keep attacking every third round");
    };
};
