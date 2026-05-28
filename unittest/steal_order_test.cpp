#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "orders.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Set a STEAL order directly on a unit (bypasses parser).
static void set_steal_order(Unit *thief, Unit *target, int item) {
    delete thief->stealthorders;
    StealOrder *so = new StealOrder();
    UnitId *uid = new UnitId();
    uid->unitnum = target->num;
    uid->alias = 0;
    uid->faction = 0;
    so->target = uid;
    so->item = item;
    thief->stealthorders = so;
}

// Returns stealth days accumulated on a unit (0 = no practice given).
static int stealth_days(Unit *u) {
    return u->skills.GetDays(S_STEALTH);
}

// Kill a faction's starting unit so it doesn't interfere with visibility checks.
// SetupFaction places a starting unit in regions.front(); we zero its men so it
// has no observation and is excluded from CanSee loops.
static void kill_starting_unit(UnitTestHelper &helper, Faction *fac) {
    Unit *u = helper.get_first_unit(fac);
    if (u) u->SetMen(I_LEADERS, 0);
}

ut::suite<"StealOrder"> steal_order_suite = [] {
    using namespace ut;

    // ------------------------------------------------------------------
    // Case 1: steal from own faction — error, no XP
    // ------------------------------------------------------------------
    "Steal from own faction gives error and no stealth XP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Thieves");
        ARegion *r = helper.get_region(0, 0, 0);
        kill_starting_unit(helper, fac);
        helper.clear_npc_units_in_region(r);

        Unit *thief = helper.create_unit(fac, r);
        Unit *target = helper.create_unit(fac, r);
        target->items.SetNum(I_SILVER, 100);

        set_steal_order(thief, target, I_SILVER);
        helper.run_steal_orders();

        expect(fac->errors.size() == 1_ul) << "expected one error";
        if (!fac->errors.empty())
            expect(fac->errors[0].message == "STEAL: Cannot steal from your own units.");
        expect(stealth_days(thief) == 0_i) << "no XP should be awarded";
        expect(target->items.GetNum(I_SILVER) == 100_i) << "silver must not be taken";
    };

    // ------------------------------------------------------------------
    // Case 2: steal from ALLY with visible faction (REVEAL_FACTION) — error, no XP
    // ------------------------------------------------------------------
    "Steal from ALLY with visible faction gives error and no stealth XP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac_a = helper.create_faction("Faction A");
        Faction *fac_b = helper.create_faction("Faction B");
        fac_a->set_attitude(fac_b->num, AttitudeType::ALLY);

        ARegion *r = helper.get_region(0, 0, 0);
        kill_starting_unit(helper, fac_a);
        kill_starting_unit(helper, fac_b);
        helper.clear_npc_units_in_region(r);

        Unit *thief = helper.create_unit(fac_a, r);

        Unit *target = helper.create_unit(fac_b, r);
        target->items.SetNum(I_SILVER, 100);
        target->reveal = REVEAL_FACTION;

        set_steal_order(thief, target, I_SILVER);
        helper.run_steal_orders();

        expect(fac_a->errors.size() == 1_ul) << "expected one error";
        if (!fac_a->errors.empty())
            expect(fac_a->errors[0].message == "STEAL: Cannot steal from units of an allied faction.");
        expect(stealth_days(thief) == 0_i) << "no XP should be awarded";
        expect(target->items.GetNum(I_SILVER) == 100_i) << "silver must not be taken";
    };

    // ------------------------------------------------------------------
    // Case 3: steal from ALLY with hidden faction — caught by seers, XP given.
    // Player didn't know target was ally (faction hidden) → genuine attempt → XP fair.
    // ------------------------------------------------------------------
    "Steal from ALLY with hidden faction is caught and gives stealth XP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac_a = helper.create_faction("Faction A");
        Faction *fac_b = helper.create_faction("Faction B");
        fac_a->set_attitude(fac_b->num, AttitudeType::ALLY);

        ARegion *r = helper.get_region(0, 0, 0);
        kill_starting_unit(helper, fac_a);
        kill_starting_unit(helper, fac_b);
        helper.clear_npc_units_in_region(r);

        Unit *thief = helper.create_unit(fac_a, r);
        // Stealth >= 2 so fac_b's default-obs units can't see the thief.
        // Own faction (fac_a) always sees own unit → in seers → ALLY check fires → caught.
        helper.set_skill_level(thief, S_STEALTH, 2);

        Unit *target = helper.create_unit(fac_b, r);
        target->items.SetNum(I_SILVER, 100);
        target->reveal = REVEAL_UNIT;  // thief can see unit, faction hidden

        set_steal_order(thief, target, I_SILVER);
        helper.run_steal_orders();

        expect(fac_a->errors.size() == 0_ul) << "no error expected";
        expect(fac_a->events.size() >= 1_ul) << "caught event expected";
        expect(stealth_days(thief) > 0_i) << "XP should be awarded for genuine attempt";
        expect(target->items.GetNum(I_SILVER) == 100_i) << "silver must not be taken";
    };

    // ------------------------------------------------------------------
    // Case 4: steal from FRIENDLY, thief visible to target faction — caught.
    // Thief stealth=0; fac_b unit obs=0; obs==stealth → CanSee=1 → fac_b in seers → caught.
    // No prior stealth skill → Practice returns 0 (REQUIRED_EXPERIENCE=0, days<1).
    // ------------------------------------------------------------------
    "Steal from FRIENDLY unit when visible is caught"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac_a = helper.create_faction("Faction A");
        Faction *fac_b = helper.create_faction("Faction B");
        fac_a->set_attitude(fac_b->num, AttitudeType::FRIENDLY);

        ARegion *r = helper.get_region(0, 0, 0);
        kill_starting_unit(helper, fac_a);
        kill_starting_unit(helper, fac_b);
        helper.clear_npc_units_in_region(r);

        Unit *thief = helper.create_unit(fac_a, r);
        // stealth=0: fac_b obs=0 == stealth=0 → CanSee=1 → fac_b in seers → caught

        Unit *target = helper.create_unit(fac_b, r);
        target->items.SetNum(I_SILVER, 100);
        target->reveal = REVEAL_FACTION;

        set_steal_order(thief, target, I_SILVER);
        helper.run_steal_orders();

        expect(fac_a->errors.size() == 0_ul) << "no error expected";
        expect(fac_a->events.size() >= 1_ul) << "caught event expected";
        expect(stealth_days(thief) == 0_i) << "no XP without prior stealth knowledge";
        expect(target->items.GetNum(I_SILVER) == 100_i) << "silver must not be taken";
    };

    // ------------------------------------------------------------------
    // Case 5: steal from neutral, thief invisible to all — succeeds, XP given.
    // Stealth=2 makes the thief invisible to obs=0 units; NPC units cleared from region.
    // ------------------------------------------------------------------
    "Steal from neutral unit when thief invisible succeeds and gives stealth XP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac_a = helper.create_faction("Faction A");
        Faction *fac_b = helper.create_faction("Faction B");

        ARegion *r = helper.get_region(0, 0, 0);
        kill_starting_unit(helper, fac_a);
        kill_starting_unit(helper, fac_b);
        helper.clear_npc_units_in_region(r);

        Unit *thief = helper.create_unit(fac_a, r);
        // Stealth 2 → invisible to obs-0 units. set_skill_level gives days>0 for Practice.
        helper.set_skill_level(thief, S_STEALTH, 2);

        Unit *target = helper.create_unit(fac_b, r);
        target->items.SetNum(I_WOOD, 5);
        target->reveal = REVEAL_UNIT;  // thief can see unit

        set_steal_order(thief, target, I_WOOD);
        helper.run_steal_orders();

        expect(fac_a->errors.size() == 0_ul) << "no error expected";
        // Success also sends an event to seers (thief's own faction is always in seers).
        // The event says "steals X from Y", not "caught". Verify transfer happened.
        expect(thief->items.GetNum(I_WOOD) == 1_i) << "thief should have stolen 1 wood";
        expect(target->items.GetNum(I_WOOD) == 4_i) << "target should have lost 1 wood";
        expect(stealth_days(thief) > 0_i) << "XP should be awarded";
    };

    // ------------------------------------------------------------------
    // Case 6: steal from neutral, thief visible — caught.
    // Thief stealth=0, fac_b obs=0 → obs==stealth → fac_b in seers → caught. No XP.
    // ------------------------------------------------------------------
    "Steal from neutral unit when thief visible is caught"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac_a = helper.create_faction("Faction A");
        Faction *fac_b = helper.create_faction("Faction B");

        ARegion *r = helper.get_region(0, 0, 0);
        kill_starting_unit(helper, fac_a);
        kill_starting_unit(helper, fac_b);
        helper.clear_npc_units_in_region(r);

        Unit *thief = helper.create_unit(fac_a, r);
        // stealth=0, fac_b obs=0 → obs==stealth → CanSee=1 → in seers → caught

        Unit *target = helper.create_unit(fac_b, r);
        target->items.SetNum(I_SILVER, 100);
        target->reveal = REVEAL_FACTION;

        set_steal_order(thief, target, I_SILVER);
        helper.run_steal_orders();

        expect(fac_a->errors.size() == 0_ul) << "no error expected";
        expect(fac_a->events.size() >= 1_ul) << "caught event expected";
        expect(stealth_days(thief) == 0_i) << "no XP without prior stealth knowledge";
        expect(target->items.GetNum(I_SILVER) == 100_i) << "silver must not be taken";
    };
};
