#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "unit.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// PILLAGE: the two rules that keep a region from being looted to death, and
// the arithmetic of the damage itself.
//
// The mechanic had no test coverage at all, which is how a dead safety net
// survived for twenty years: ARegion::Pillage() ended with
//
//     while (Wages() < Globals->MAINTENANCE_COST / 20) development += ...
//
// meant as "pump development back up if the region got too poor". Wages() is
// in TENTHS of silver while MAINTENANCE_COST is whole silver, so the threshold
// was an integer-divided zero and the loop never ran. It was also unreachable
// by construction for any MAINTENANCE_COST up to 200, and the only state that
// could enter it - a region whose population reached zero - would never leave,
// because Wages() returns 0 before it looks at development. The line is gone;
// what actually protects a region is the eligibility gate below plus the
// recovery toward maxdevelopment in PostTurn().
// ---------------------------------------------------------------------------

namespace {
    // A combat-ready unit is what Taxers(1) counts under WHO_CAN_TAX=TAX_NORMAL.
    Unit *create_pillager(UnitTestHelper &helper, ARegion *r, const std::string& name, int men)
    {
        Faction *f = helper.create_faction(name);
        Unit *u = helper.create_unit(f, r);
        u->items.SetNum(I_LEADERS, men);
        u->items.SetNum(I_MSWORD, men);
        helper.set_skill_level(u, S_COMBAT, 3);
        return u;
    }

    // Put the region at a chosen wage by setting development directly, so the
    // tests do not depend on what the generated world happened to roll.
    void set_wage_above_gate(ARegion *r)
    {
        r->development = 120;
        r->maxdevelopment = 120;
    }

    void set_wage_below_gate(ARegion *r)
    {
        r->development = 20;
        r->maxdevelopment = 20;
    }
}

ut::suite<"Pillage"> pillage_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Guard rule: one live foreign guard is enough to refuse the order, which
    // is what makes a settlement unpillageable while its garrison stands.
    // -----------------------------------------------------------------------
    "a live foreign guard blocks pillage"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *raider = create_pillager(helper, r, "Raider", 10);

        expect(r->CanPillage(raider) == 1_i) << "an unguarded region can be pillaged";

        Faction *other = helper.create_faction("Defender");
        Unit *guard = helper.create_unit(other, r);
        guard->SetMen(I_LEADERS, 5);
        guard->guard = GUARD_GUARD;

        expect(r->CanPillage(raider) == 0_i) << "a foreign unit on GUARD refuses the pillage";

        guard->guard = GUARD_NONE;
        expect(r->CanPillage(raider) == 1_i) << "standing down lifts the block";
    };

    "your own guard does not block your pillage"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *raider = create_pillager(helper, r, "Raider", 10);

        Unit *mine = helper.create_unit(raider->faction, r);
        mine->SetMen(I_LEADERS, 5);
        mine->guard = GUARD_GUARD;

        expect(r->CanPillage(raider) == 1_i) << "the rule is about foreign guards only";
    };

    // -----------------------------------------------------------------------
    // Eligibility gate: a region whose wage is already down at the cost of
    // feeding a man is left alone. This is the protection that replaces the
    // removed floor - without it a region could be driven to nothing.
    // -----------------------------------------------------------------------
    "a region at or below the wage gate is not pillaged"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        set_wage_below_gate(r);
        r->wealth = 500;

        expect(r->Wages() <= 10 * Globals->MAINTENANCE_COST)
            << "test bootstrap must put the region under the gate";

        Unit *raider = create_pillager(helper, r, "Raider", 10);
        int purse = raider->GetMoney();
        raider->taxing = TAX_PILLAGE;

        helper.run_pillage_region(r);

        expect(r->wealth == 500_i) << "wealth must survive untouched";
        expect(r->development == 20_i) << "development must survive untouched";
        expect(that % raider->GetMoney() == purse) << "the raider must get nothing";
    };

    // -----------------------------------------------------------------------
    // Positive control: above the gate the order resolves, pays out twice the
    // region's wealth and takes exactly a third of development.
    // -----------------------------------------------------------------------
    "above the gate the region is looted for twice its wealth"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        set_wage_above_gate(r);
        r->wealth = 400;   // 10 pillagers cover 10 * 2 * TAX_BASE_INCOME = 1000

        expect(r->Wages() > 10 * Globals->MAINTENANCE_COST)
            << "test bootstrap must put the region above the gate";

        Unit *raider = create_pillager(helper, r, "Raider", 10);
        int purse = raider->GetMoney();
        raider->taxing = TAX_PILLAGE;

        helper.run_pillage_region(r);

        expect(that % raider->GetMoney() == purse + 800) << "loot is twice the region's wealth";
        expect(r->wealth == 0_i) << "pillaging empties the region";
        expect(r->development == 80_i) << "damage is exactly development / 3";
    };

    "too few pillagers for the prize cancels the order"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        set_wage_above_gate(r);
        r->wealth = 5000;   // needs 50 combat-ready men, we bring one

        Unit *raider = create_pillager(helper, r, "Raider", 1);
        int purse = raider->GetMoney();
        raider->taxing = TAX_PILLAGE;

        helper.run_pillage_region(r);

        expect(that % raider->GetMoney() == purse) << "the raider must get nothing";
        expect(r->wealth == 5000_i) << "wealth must survive untouched";
        expect(r->development == 120_i) << "development must survive untouched";
        expect(that % raider->taxing == TAX_NONE) << "the order is cleared";
    };

    // -----------------------------------------------------------------------
    // The damage itself: a third of development, never all of it, and the call
    // always returns. The last part is the regression guard for the removed
    // loop, whose only reachable state was a non-terminating one.
    // -----------------------------------------------------------------------
    "pillage damage never empties development and always terminates"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);

        r->development = 3;
        r->wealth = 100;
        r->Pillage();
        expect(r->development == 2_i) << "3 - 3/3 leaves 2, not 0";
        expect(r->wealth == 0_i) << "pillage empties wealth";

        r->development = 1;
        r->Pillage();
        expect(r->development == 1_i) << "a third of 1 is 0, so nothing is lost";

        // A depopulated region is the state the old floor loop could not leave:
        // Wages() short-circuits to 0 on empty population, so a floor above
        // zero would spin forever. Pillage() must simply return.
        r->development = 30;
        r->population = 0;
        if (r->town) r->town->pop = 0;
        expect(r->Wages() == 0_i)
            << "an empty region pays no wages - exactly the state the old floor could not leave";
        r->Pillage();
        expect(r->development == 20_i) << "damage still applies, and the call returns";
    };
};
