#include "external/boost/ut.hpp"
#include "testhelper.hpp"
#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "aregion.h"
#include "items.h"
#include "rng.hpp"

#include <sstream>

namespace ut = boost::ut;

// Pirate promotion: a regular fleet matures into officers (bosun, then captain),
// and two captainless fleets meeting in one region merge under a new captain.
// The thresholds are absolute crew counts (officers included), so promoting one
// pirate into an officer never drops a fleet back under its own bar.
//
// The unittest map is five hexes (four surface, one underworld), none of them
// water by default, so the captain ceiling - water_hexes * per_mille / 1000,
// floored at 1 - collapses to 1 unless the tests hand-wire water hexes. Tests
// that need a generous ceiling call make_all_water() and a huge per_mille;
// tests that assert the floor pin the default per_mille.

// Sums an item across every unit in a region.
static int count_item_in_region(ARegion *r, int item)
{
    int n = 0;
    for (auto o : r->objects)
        for (auto u : o->units)
            n += u->items.GetNum(item);
    return n;
}

// Sums an item across every region in the world.
static int count_item_world(UnitTestHelper &helper, int item)
{
    int n = 0;
    for (auto r : helper.get_regions()) n += count_item_in_region(r, item);
    return n;
}

// Counts fleet objects in a region.
static int count_fleets(ARegion *r)
{
    int n = 0;
    for (auto o : r->objects) if (o->IsFleet()) n++;
    return n;
}

// Counts units in a region that hold at least one of the given item.
static int count_units_holding(ARegion *r, int item)
{
    int n = 0;
    for (auto o : r->objects)
        for (auto u : o->units)
            if (u->items.GetNum(item) > 0) n++;
    return n;
}

// Returns the first unit in a region holding at least one of the given item.
static Unit *find_unit_holding(ARegion *r, int item)
{
    for (auto o : r->objects)
        for (auto u : o->units)
            if (u->items.GetNum(item) > 0) return u;
    return nullptr;
}

// Marks every region as ocean so the world's water-hex count is non-zero.
static void make_all_water(UnitTestHelper &helper)
{
    for (auto r : helper.get_regions()) r->type = R_OCEAN;
}

// Pins the promotion tuning. The unittest ruleset has none of these keys, so
// every test sets them all; anything omitted falls back to the in-code default.
// With make_all_water() the world has five water hexes, so
//   cap      = 5 * captain_per_mille / 1000   (floored at 1)
//   deep_max = cap * (100 - surface_share) / 100
static json tune(int captain_per_mille = 100000, int merge = 1, int bosun_chance = 100,
                 int surface_share = 67)
{
    json d;
    d["pirate_promote_bosun_crew"] = 75;
    d["pirate_promote_bosun_chance"] = bosun_chance;
    d["pirate_promote_captain_crew"] = 120;
    d["pirate_elite_captain_per_mille"] = captain_per_mille;
    d["pirate_elite_captain_surface_share_pct"] = surface_share;
    d["pirate_promote_cooldown"] = 6;
    d["pirate_promote_merge"] = merge;
    return d;
}

ut::suite<"PiratePromotion"> pirate_promotion_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Bosun: absolute threshold, conserved bodies.
    // -----------------------------------------------------------------------
    "a fleet earns a bosun at exactly the threshold and conserves bodies"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 75);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 1_i)
            << "a 75-pirate crew is exactly at the bosun bar";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "75 crew is far below the captain bar";
        expect(pirates->items.GetNum(I_PIRATES) == 74_i)
            << "one crew member became the bosun";
    };

    "nothing below the bosun threshold promotes"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 74);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 0_i)
            << "74 pirates is one short of the bar";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "no officer at all below the bar";
        expect(pirates->items.GetNum(I_PIRATES) == 74_i)
            << "bodies are untouched when nothing promotes";
    };

    "the bosun roll actually gates promotion"_test = [] {
        bool promoted = false;
        bool not_promoted = false;
        for (int seed = 0; seed < 40 && !(promoted && not_promoted); seed++) {
            UnitTestHelper helper;
            helper.initialize_game();
            helper.setup_turn();
            helper.set_ruleset_specific_data(tune(100000, 1, 50));
            make_all_water(helper);

            ARegion *r = helper.get_region(0, 2, 0);
            helper.create_npc_pirate_fleet(r, 75);
            rng::seed_random(seed);
            helper.run_pirate_promote_fleets();

            if (count_item_world(helper, I_PIRATE_BOSUN) > 0) promoted = true;
            else not_promoted = true;
        }
        expect(promoted) << "some seeds must promote a bosun";
        expect(not_promoted) << "some seeds must not promote a bosun";
    };

    "the bosun chance boundary still holds at 0 and 100"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(100000, 1, 0));
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        helper.create_npc_pirate_fleet(r, 75);
        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 0_i)
            << "a 0 chance must never promote a bosun";

        UnitTestHelper helper2;
        helper2.initialize_game();
        helper2.setup_turn();
        helper2.set_ruleset_specific_data(tune(100000, 1, 100));
        make_all_water(helper2);

        ARegion *r2 = helper2.get_region(0, 2, 0);
        helper2.create_npc_pirate_fleet(r2, 75);
        helper2.run_pirate_promote_fleets();

        expect(count_item_in_region(r2, I_PIRATE_BOSUN) == 1_i)
            << "a 100 chance must always promote a bosun";
    };

    "a fleet that already has a bosun does not earn a second"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 100);
        helper.create_npc_pirate_bosun(r, pirates->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 1_i)
            << "a bosun already aboard, no second is earned";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "100 effective crew is still below the captain bar";
    };

    // -----------------------------------------------------------------------
    // Captain: requires a bosun, at the absolute threshold.
    // -----------------------------------------------------------------------
    "no captain without a bosun however large the crew"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(100000, 1, 0));
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        helper.create_npc_pirate_fleet(r, 200);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 0_i)
            << "the bosun roll is pinned off";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "a captain requires a bosun first, whatever the crew size";
    };

    "a fleet with a bosun and 120 effective crew earns a captain"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 119);
        helper.create_npc_pirate_bosun(r, pirates->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "119 pirates plus one bosun is exactly 120 effective crew";
        expect(pirates->items.GetNum(I_PIRATES) == 118_i)
            << "one crew member became the captain";
    };

    "a 120-pirate fleet with no bosun climbs one rank per pass"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 120);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 1_i)
            << "120 crew is over the bosun bar, so a bosun is earned";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "a fleet that earned a bosun this turn does not also earn a captain";
        expect(pirates->items.GetNum(I_PIRATES) == 119_i)
            << "one crew member became the bosun";

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "the next pass promotes the bosun-led 120-crew fleet to captain";
        expect(pirates->items.GetNum(I_PIRATES) == 118_i)
            << "one more crew member became the captain";
    };

    // -----------------------------------------------------------------------
    // Captain cap: scales with water, blocks earned promotion, leaves born alone.
    // -----------------------------------------------------------------------
    "the captain cap blocks an earned captain and leaves a born captain untouched"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(10, 1, 0));
        make_all_water(helper);  // five water hexes * 10 / 1000 floors the cap to 1

        ARegion *born_r = helper.get_region(0, 0, 0);
        Unit *born = helper.create_npc_pirate_fleet(born_r, 75);
        helper.create_npc_pirate_captain(born_r, born->object);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 119);
        helper.create_npc_pirate_bosun(r, pirates->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_world(helper, I_PIRATE_CAPTAIN) == 1_i)
            << "the born captain is the one captain the cap allows";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "the eligible fleet is blocked by the full cap";
    };

    "the captain ceiling collapses to one on a small world"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(10));
        make_all_water(helper);  // the tiny map has five water hexes, far below 100

        ARegion *r1 = helper.get_region(0, 0, 0);
        ARegion *r2 = helper.get_region(0, 2, 0);
        ARegion *r3 = helper.get_region(1, 1, 0);
        Unit *p1 = helper.create_npc_pirate_fleet(r1, 119);
        Unit *p2 = helper.create_npc_pirate_fleet(r2, 119);
        Unit *p3 = helper.create_npc_pirate_fleet(r3, 119);
        helper.create_npc_pirate_bosun(r1, p1->object);
        helper.create_npc_pirate_bosun(r2, p2->object);
        helper.create_npc_pirate_bosun(r3, p3->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_world(helper, I_PIRATE_CAPTAIN) == 1_i)
            << "three eligible fleets, but a small world permits exactly one captain";
    };

    // -----------------------------------------------------------------------
    // Deep sub-ceiling: non-surface levels may hold at most the remainder of
    // the captain ceiling; the surface keeps the whole ceiling when deep is
    // empty, and is processed first.
    // -----------------------------------------------------------------------
    "a full deep quota blocks a deep captain but not a surface one"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(1200));  // cap 6, deep_max 1
        make_all_water(helper);

        ARegion *deep_r = helper.get_region(0, 0, 1);
        Unit *born = helper.create_npc_pirate_fleet(deep_r, 75);
        helper.create_npc_pirate_captain(deep_r, born->object);  // spends the deep quota

        Unit *deep = helper.create_npc_pirate_fleet(deep_r, 119);
        helper.create_npc_pirate_bosun(deep_r, deep->object);

        ARegion *surface_r = helper.get_region(0, 2, 0);
        Unit *surf = helper.create_npc_pirate_fleet(surface_r, 119);
        helper.create_npc_pirate_bosun(surface_r, surf->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(deep_r, I_PIRATE_CAPTAIN) == 1_i)
            << "the deep quota is already spent by the born captain";
        expect(count_item_in_region(surface_r, I_PIRATE_CAPTAIN) == 1_i)
            << "the surface fleet still earns its captain";
    };

    "with the deep empty the surface holds the whole ceiling"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(400));  // cap 2, deep_max 0
        make_all_water(helper);

        ARegion *r1 = helper.get_region(0, 0, 0);
        ARegion *r2 = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r1, 119);
        helper.create_npc_pirate_bosun(r1, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r2, 119);
        helper.create_npc_pirate_bosun(r2, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_world(helper, I_PIRATE_CAPTAIN) == 2_i)
            << "two surface captains fit under the full ceiling (no surface throttle)";
    };

    "on a tiny world the deep never promotes a captain"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(10));  // cap 1, deep_max 0
        make_all_water(helper);

        ARegion *deep_r = helper.get_region(0, 0, 1);
        Unit *deep = helper.create_npc_pirate_fleet(deep_r, 119);
        helper.create_npc_pirate_bosun(deep_r, deep->object);

        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(deep_r, I_PIRATE_CAPTAIN) == 0_i)
            << "deep_max is zero, so a deep fleet is always blocked";
    };

    "the rendezvous also respects the deep limit"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(1200));  // cap 6, deep_max 1
        make_all_water(helper);

        ARegion *deep_r = helper.get_region(0, 0, 1);
        Unit *born = helper.create_npc_pirate_fleet(deep_r, 75);
        helper.create_npc_pirate_captain(deep_r, born->object);  // spends the deep quota

        Unit *a = helper.create_npc_pirate_fleet(deep_r, 75);
        helper.create_npc_pirate_bosun(deep_r, a->object);
        Unit *b = helper.create_npc_pirate_fleet(deep_r, 75);
        helper.create_npc_pirate_bosun(deep_r, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(deep_r) == 3_i)
            << "the two eligible fleets do not merge when the deep quota is full";
        expect(count_item_in_region(deep_r, I_PIRATE_CAPTAIN) == 1_i)
            << "only the born deep captain remains";
    };

    // -----------------------------------------------------------------------
    // Cooldown: blocks re-promotion after a captain dies, then expires.
    // -----------------------------------------------------------------------
    "the cooldown lasts exactly the configured number of turns"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 119);
        helper.create_npc_pirate_bosun(r, pirates->object);
        Object *fleet = pirates->object;

        // A captain died this turn: Army::Lose set the timer to the cooldown.
        fleet->pirate_promote_timer = 6;

        // Six blocked passes - the death turn plus five more. The tick runs at
        // the end of each pass, so the death turn does not consume a tick.
        for (int i = 0; i < 6; i++) {
            helper.run_pirate_promote_fleets();
            expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
                << "the fleet is still inside its cooldown";
        }
        expect(that % fleet->pirate_promote_timer == 0)
            << "six passes drain a six-turn cooldown to zero";

        // The seventh pass: the cooldown has expired, so the fleet re-promotes.
        helper.run_pirate_promote_fleets();
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "the turn after the cooldown expires the fleet earns a captain";
    };

    // -----------------------------------------------------------------------
    // Quest wiring: a promoted captain mints exactly one hunt-quest attempt.
    // -----------------------------------------------------------------------
    "a captain promotion calls TryCreatePirateHuntQuest exactly once"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 119);
        helper.create_npc_pirate_bosun(r, pirates->object);

        unittest_reset_pirate_hunt_quest_calls();
        helper.run_pirate_promote_fleets();

        Unit *captain = find_unit_holding(r, I_PIRATE_CAPTAIN);
        expect(captain != nullptr) << "the captain must exist";
        expect(unittest_pirate_hunt_quest_calls() == 1_i)
            << "one captain, one quest attempt";
        expect(unittest_pirate_hunt_quest_last() == captain)
            << "the quest was asked for the promoted captain";
    };

    "a blocked promotion does not call TryCreatePirateHuntQuest"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 119);
        helper.create_npc_pirate_bosun(r, pirates->object);
        pirates->object->pirate_promote_timer = 6;

        unittest_reset_pirate_hunt_quest_calls();
        helper.run_pirate_promote_fleets();

        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "the cooldown blocks the promotion";
        expect(unittest_pirate_hunt_quest_calls() == 0_i)
            << "a blocked promotion mints no quest";
    };

    // -----------------------------------------------------------------------
    // Rendezvous: two fleets merge under a new captain.
    // -----------------------------------------------------------------------
    "two eligible fleets merge and one captain is chosen"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r, 80);
        a->free = 0;  // mature
        helper.create_npc_pirate_bosun(r, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r, 75);
        b->free = 2;  // less mature
        helper.create_npc_pirate_bosun(r, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 1_i) << "the source fleet object is deleted";
        expect(a->object->GetNumShips(I_COG) == 2_i)
            << "both hulls combine on the receiver";
        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 2_i)
            << "all officers are kept, so both bosuns survive";
        expect(count_units_holding(r, I_PIRATES) == 1_i)
            << "the two crew units become one";
        expect(a->items.GetNum(I_PIRATES) == 154_i)
            << "80 + 75 crew, minus the one that became captain";
        expect(that % a->free == 2)
            << "the merged crew takes the less mature free";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "the merged fleet earns a captain";
    };

    "two fleets that both earn a bosun this turn do not merge until the next pass"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_fleet(r, 75);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 2_i)
            << "a fleet that earned a bosun this turn is not a rendezvous partner";
        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 2_i)
            << "each fleet earned its bosun";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "no merge, so no captain this turn";

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 1_i)
            << "with a turn-old bosun each, the pair merges on the next pass";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "the merge grants one captain";
    };

    "a fleet that just merged earns nothing else this turn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r, 80);
        helper.create_npc_pirate_bosun(r, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r, 80);
        helper.create_npc_pirate_bosun(r, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 1_i)
            << "the pair merges into one fleet";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "the merge grants exactly one captain";
        expect(count_item_in_region(r, I_PIRATE_BOSUN) == 2_i)
            << "the two bosuns it already carried are the only other officers";
        expect(a->items.GetNum(I_PIRATES) == 159_i)
            << "80 + 80 crew, minus exactly the one body that became captain";
    };

    "three fleets in one hex produce exactly one merge, on the second pass"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *p1 = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_bosun(r, p1->object);
        helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_fleet(r, 75);

        // First pass: the two bosun-less fleets each earn a bosun, and a fleet
        // that earned a bosun this turn is not a rendezvous partner, so nothing
        // merges yet.
        helper.run_pirate_promote_fleets();
        expect(count_fleets(r) == 3_i)
            << "a fleet that earned a bosun this turn cannot merge this turn";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "no merge, so no captain this turn";

        // Second pass: the bosuns are a turn old, and the first eligible pair merges.
        helper.run_pirate_promote_fleets();
        expect(count_fleets(r) == 2_i)
            << "exactly one pair merges; the third is left alone";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "one merge grants one captain";
    };

    "no merge when the captain cap is full"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(10, 1, 0));
        make_all_water(helper);

        ARegion *born_r = helper.get_region(0, 0, 0);
        Unit *born = helper.create_npc_pirate_fleet(born_r, 75);
        helper.create_npc_pirate_captain(born_r, born->object);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_bosun(r, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_bosun(r, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 2_i)
            << "a full cap means no clumping";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "no new captain is granted";
    };

    "no merge when one fleet already has a captain"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *elite = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_captain(r, elite->object);
        Unit *b = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_bosun(r, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 2_i)
            << "a captainled fleet is not a rendezvous partner";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "only the existing captain remains";
    };

    "no merge when neither fleet carries a bosun"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(100000, 1, 0));
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_fleet(r, 75);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 2_i)
            << "a bosun-less pair does not merge";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "and no captain appears";
    };

    "no merge when the rendezvous trigger is disabled"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune(100000, 0));
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_bosun(r, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r, 75);
        helper.create_npc_pirate_bosun(r, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 2_i)
            << "pirate_promote_merge = 0 disables the merge";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "and no captain is granted";
    };

    "a fleet on cooldown cannot take part in a rendezvous until it expires"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r, 80);
        helper.create_npc_pirate_bosun(r, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r, 80);
        helper.create_npc_pirate_bosun(r, b->object);

        // One partner is still inside its cooldown: no rendezvous this turn.
        a->object->pirate_promote_timer = 1;
        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 2_i)
            << "a fleet inside its cooldown is not a rendezvous partner";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 0_i)
            << "no merge, so no captain";

        // The timer drains at the end of the pass, so next turn the pair merges.
        helper.run_pirate_promote_fleets();

        expect(count_fleets(r) == 1_i)
            << "once the cooldown expires the pair merges";
        expect(count_item_in_region(r, I_PIRATE_CAPTAIN) == 1_i)
            << "the merge grants a captain";
    };

    "the rendezvous is refused where a player is present and happens where none is"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(tune());
        make_all_water(helper);

        // Occupied region: create_faction drops a player unit in regions.front()
        // (0,0,0), so the two eligible fleets there must not merge.
        ARegion *r_held = helper.get_region(0, 0, 0);
        helper.create_faction("Player");
        Unit *c = helper.create_npc_pirate_fleet(r_held, 75);
        helper.create_npc_pirate_bosun(r_held, c->object);
        Unit *d = helper.create_npc_pirate_fleet(r_held, 75);
        helper.create_npc_pirate_bosun(r_held, d->object);

        // Empty region: no player anywhere, so the pair merges.
        ARegion *r_empty = helper.get_region(0, 2, 0);
        Unit *a = helper.create_npc_pirate_fleet(r_empty, 75);
        helper.create_npc_pirate_bosun(r_empty, a->object);
        Unit *b = helper.create_npc_pirate_fleet(r_empty, 75);
        helper.create_npc_pirate_bosun(r_empty, b->object);

        helper.run_pirate_promote_fleets();

        expect(count_fleets(r_held) == 2_i)
            << "a player's presence refuses the rendezvous";
        expect(count_item_in_region(r_held, I_PIRATE_CAPTAIN) == 0_i)
            << "no captain is chosen under a player's guns";
        expect(count_fleets(r_empty) == 1_i)
            << "an empty region still lets two eligible fleets merge";
        expect(count_item_in_region(r_empty, I_PIRATE_CAPTAIN) == 1_i)
            << "the empty-region merge mints a captain";
    };

    // -----------------------------------------------------------------------
    // Born-elite spawn share: MakePirateFleet() rolls the key at generation.
    // -----------------------------------------------------------------------
    "the born-elite spawn share: 0 spawns no captain, 100 spawns every captain"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        helper.set_ruleset_specific_data(json{ { "pirate_elite_spawn_pct", 0 } });
        ARegion *r_plain = helper.get_region(0, 2, 0);
        r_plain->type = R_OCEAN;
        helper.run_make_pirate_fleet(r_plain);

        expect(count_fleets(r_plain) == 1_i)
            << "the fleet still spawns at a zero share";
        expect(count_item_in_region(r_plain, I_PIRATE_CAPTAIN) == 0_i)
            << "a zero share must never spawn a born captain";

        helper.set_ruleset_specific_data(json{ { "pirate_elite_spawn_pct", 100 } });
        ARegion *r_elite = helper.get_region(1, 3, 0);
        r_elite->type = R_OCEAN;
        helper.run_make_pirate_fleet(r_elite);

        expect(count_fleets(r_elite) == 1_i)
            << "exactly one fleet spawns";
        expect(count_item_in_region(r_elite, I_PIRATE_CAPTAIN) == 1_i)
            << "a 100 share must spawn every fleet with a captain";
    };

    // -----------------------------------------------------------------------
    // Save format: the cooldown field is version-gated.
    // -----------------------------------------------------------------------
    "the promotion cooldown survives a write/read round trip"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);

        Object src(r);
        src.type = O_FLEET;
        src.num = 4242;
        src.set_name("Pirate Cog");
        src.AddShip(I_COG);
        src.pirate_promote_timer = 5;

        std::stringstream ss;
        src.Writeout(ss);

        Object dst(r);
        dst.Readin(ss, helper.get_factions(), CURRENT_ATL_VER);

        expect(that % dst.pirate_promote_timer == 5)
            << "the cooldown must come back unchanged from the save";
    };

    "a save written at the older version reads back a zero cooldown"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);

        // An object block as the pre-5.2.11 engine wrote it: no
        // pirate_promote_timer line between runes and the unit count.
        std::stringstream ss;
        ss << "4242\n"
           << "Fleet\n"
           << "0\n"
           << "Pirate Cog\n"
           << "none\n"
           << "-1\n"
           << "-1\n"
           << "0\n"
           << "0\n"
           << "1\n"
           << "1 COG\n";

        Object dst(r);
        dst.Readin(ss, helper.get_factions(), MAKE_ATL_VER(5, 2, 10));

        expect(that % dst.pirate_promote_timer == 0)
            << "a save without the field must read back as zero, not desynchronise";
        expect(dst.type == O_FLEET) << "the object parses correctly past the absent field";
        expect(dst.GetNumShips(I_COG) == 1_i) << "the hull list still parses";
    };
};
