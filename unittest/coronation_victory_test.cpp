#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "items.h"
#include "testhelper.hpp"
#include <sstream>

namespace ut = boost::ut;
using namespace std;

// ---------------------------------------------------------------------------
// Trident coronation victory (Phase B). A faction wins when the hold condition
// — guards its capital region, owns a finished Palace there, and holds
// CROWNS_TO_WIN crowns in that region — stays true for CORONATION_TURNS
// consecutive turns. The check lives engine-side (Game::check_coronation) so it
// is unit-testable, unlike the ruleset-stubbed CheckVictory. The whole feature
// is dormant unless rulesetSpecificData victory_type == "coronation" (left
// disabled on Arcanum). See docs/TRIDENT_VICTORY_MECHANIC_DESIGN.md.
// ---------------------------------------------------------------------------

namespace {
    void force_city(ARegion *r) { r->town->pop = 1000000; r->town->dev = 100; }

    // Enable the coronation win with the standard 3-crown / 5-turn thresholds.
    void enable_coronation(UnitTestHelper &helper, int crowns = 3, int turns = 5) {
        json data;
        data["victory_type"]     = "coronation";
        data["crowns_to_win"]    = crowns;
        data["coronation_turns"] = turns;
        helper.set_ruleset_specific_data(data);
    }

    // Build a faction whose leader fully satisfies the hold condition in its
    // starting (city) region: owns a finished Palace, guards the region, and
    // holds `crowns` crowns. Returns the faction; out-params expose leader/region.
    Faction *make_holder(UnitTestHelper &helper, const string &name, int crowns,
                         Unit **out_leader = nullptr, ARegion **out_region = nullptr) {
        Faction *fac = helper.create_faction(name);
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        force_city(r);
        helper.create_building(r, leader, O_PALACE);  // leader becomes owner of a finished Palace
        leader->guard = GUARD_GUARD;
        leader->items.SetNum(I_CROWN, crowns);
        fac->capital_region = r->num;
        if (out_leader) *out_leader = leader;
        if (out_region) *out_region = r;
        return fac;
    }
}

ut::suite<"CoronationVictory"> coronation_victory_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // The counter advances once per turn the hold holds, and a win fires
    // exactly at the threshold.
    // -----------------------------------------------------------------------
    "coronation counter advances and wins at the threshold"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        enable_coronation(helper, 3, 5);

        Faction *fac = make_holder(helper, "House Trident", 3);

        // Turns 1..4: hold holds, counter climbs, no winner yet.
        for (int turn = 1; turn <= 4; turn++) {
            Faction *w = helper.run_check_coronation();
            expect(w == nullptr) << "no winner before the threshold (turn " << turn << ")";
            expect(fac->coronation == turn) << "counter must equal turns held";
        }

        // Turn 5: threshold reached → this faction wins.
        Faction *w = helper.run_check_coronation();
        expect(w == fac) << "faction must win on the 5th consecutive hold turn";
        expect(fac->coronation == 5_i) << "counter clamps at the threshold";
    };

    // -----------------------------------------------------------------------
    // Dropping below CROWNS_TO_WIN breaks the hold and resets the counter.
    // -----------------------------------------------------------------------
    "losing a crown resets the counter"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        enable_coronation(helper, 3, 5);

        Unit *leader = nullptr;
        Faction *fac = make_holder(helper, "House Trident", 3, &leader);

        for (int turn = 1; turn <= 3; turn++) helper.run_check_coronation();
        expect(fac->coronation == 3_i) << "counter must have climbed to 3";

        leader->items.SetNum(I_CROWN, 2);   // a rival loots one crown
        Faction *w = helper.run_check_coronation();
        expect(w == nullptr) << "a broken hold yields no winner";
        expect(fac->coronation == 0_i) << "counter must reset when the hold breaks";
    };

    // -----------------------------------------------------------------------
    // Without the Palace the hold never starts (proves owns_palace clause).
    // -----------------------------------------------------------------------
    "no Palace means no coronation progress"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        enable_coronation(helper, 3, 5);

        Faction *fac = helper.create_faction("House Pretender");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        force_city(r);
        leader->guard = GUARD_GUARD;
        leader->items.SetNum(I_CROWN, 3);
        fac->capital_region = r->num;       // declared, but no Palace built

        Faction *w = helper.run_check_coronation();
        expect(w == nullptr) << "no winner without a Palace";
        expect(fac->coronation == 0_i) << "counter must stay at 0 without a Palace";
    };

    // -----------------------------------------------------------------------
    // Dormant unless the world enables the coronation victory: even a full hold
    // does nothing when victory_type is unset (Arcanum's configuration).
    // -----------------------------------------------------------------------
    "coronation is dormant when victory_type is unset"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        // Deliberately do NOT enable coronation.

        Faction *fac = make_holder(helper, "House Trident", 3);

        Faction *w = helper.run_check_coronation();
        expect(w == nullptr) << "no winner when the victory type is not coronation";
        expect(fac->coronation == 0_i) << "the counter must not advance while dormant";
    };

    // -----------------------------------------------------------------------
    // Faction::coronation survives a save round-trip (same 5.2.10 block as
    // capital_region).
    // -----------------------------------------------------------------------
    "coronation counter survives a save round-trip"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("House Trident");
        fac->capital_region = 4242;
        fac->coronation = 3;

        stringstream ss;
        fac->Writeout(ss);

        Faction loaded;
        loaded.Readin(ss, CURRENT_ATL_VER);

        expect(loaded.capital_region == 4242_i) << "capital_region must round-trip";
        expect(loaded.coronation == 3_i) << "coronation counter must round-trip";
    };
};
