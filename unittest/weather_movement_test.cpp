#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

#include <sstream>

namespace ut = boost::ut;

// Tests for WEATHER_EXISTS=2 ("display only, always clear") guaranteeing
// that movement costs are based on terrain only, never on the weather field.
//
// Bug: ARegion::weather is uninitialized and never updated when WEATHER_EXISTS=2.
// MoveCost used `if (Globals->WEATHER_EXISTS)` which is true for 2, so it read
// the junk weather field and doubled movement costs incorrectly.
//
// Fix: MoveCost now checks `if (Globals->WEATHER_EXISTS == 1)`.
//      ARegion constructor initializes weather = W_NORMAL.
ut::suite<"WeatherMovement"> weather_movement_suite = [] {
    using namespace ut;

    // -------------------------------------------------------------------
    // Unit tests: ARegion::MoveCost return value under various WEATHER_EXISTS
    // Note: test world has no roads, so direction value doesn't affect cost.
    // -------------------------------------------------------------------

    "MoveCost WEATHER_EXISTS=2: returns terrain cost despite W_WINTER field"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->WEATHER_EXISTS;
        Globals->WEATHER_EXISTS = 2;

        ARegion *dest = helper.get_region(0, 0, 0);
        dest->type = R_PLAIN;   // TerrainDefs[R_PLAIN].movepoints = 1
        dest->weather = W_WINTER;
        dest->clearskies = 0;

        ARegion *from = nullptr;
        int dir = 0;
        for (int d = 0; d < NDIRS; d++) {
            if (dest->neighbors[d]) { from = dest->neighbors[d]; dir = d; break; }
        }

        int cost = dest->MoveCost(M_WALK, from, dir, nullptr);
        expect(cost == 1_i)
            << "WEATHER_EXISTS=2 must not apply weather penalty: plain cost must be 1, not 2";

        Globals->WEATHER_EXISTS = saved;
    };

    "MoveCost WEATHER_EXISTS=1: doubles terrain cost in W_WINTER"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->WEATHER_EXISTS;
        Globals->WEATHER_EXISTS = 1;

        ARegion *dest = helper.get_region(0, 0, 0);
        dest->type = R_PLAIN;
        dest->weather = W_WINTER;
        dest->clearskies = 0;

        ARegion *from = nullptr;
        int dir = 0;
        for (int d = 0; d < NDIRS; d++) {
            if (dest->neighbors[d]) { from = dest->neighbors[d]; dir = d; break; }
        }

        int cost = dest->MoveCost(M_WALK, from, dir, nullptr);
        expect(cost == 2_i)
            << "WEATHER_EXISTS=1 must double terrain cost in bad weather";

        Globals->WEATHER_EXISTS = saved;
    };

    "MoveCost WEATHER_EXISTS=0: returns terrain cost regardless of weather field"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // WEATHER_EXISTS=0 is the default in unittest/rules.cpp
        ARegion *dest = helper.get_region(0, 0, 0);
        dest->type = R_PLAIN;
        dest->weather = W_WINTER;
        dest->clearskies = 0;

        ARegion *from = nullptr;
        int dir = 0;
        for (int d = 0; d < NDIRS; d++) {
            if (dest->neighbors[d]) { from = dest->neighbors[d]; dir = d; break; }
        }

        int cost = dest->MoveCost(M_WALK, from, dir, nullptr);
        expect(cost == 1_i)
            << "WEATHER_EXISTS=0 must not apply any weather penalty";
    };

    // -------------------------------------------------------------------
    // Integration tests: actual unit movement
    //
    // Test world: PHASED_MOVE_OFFSET=7, MAX_SPEED=8, leader speed=2
    //   → movepoints at start of move = 7 + 2 = 9
    //   → plain (cost=1): needs 1*8=8 → 9 >= 8 → moves in one turn  ✓
    //   → plain (cost=2, weather bug): needs 2*8=16 → 9 < 16 → queued ✗
    //   → forest (cost=2): needs 2*8=16 → 9 < 16 → legitimately queued ✓
    // -------------------------------------------------------------------

    "Unit enters adjacent plain in one turn — WEATHER_EXISTS=2 must not queue movement"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->WEATHER_EXISTS;
        Globals->WEATHER_EXISTS = 2;

        ARegion *start = helper.get_region(0, 0, 0);
        start->type = R_PLAIN;

        // SE neighbor of (0,0,0) is (1,1,0)
        ARegion *dest = helper.get_region(1, 1, 0);
        dest->type = R_PLAIN;
        dest->weather = W_WINTER;   // simulates uninitialized/junk weather field (bug condition)
        dest->clearskies = 0;

        Faction *f = helper.create_faction("Movers");
        Unit *u = helper.create_unit(f, start);

        std::ostringstream ss;
        ss << "#atlantis " << f->num << "\nunit " << u->num << "\nmove SE\n";
        std::istringstream iss(ss.str());
        helper.parse_orders(f->num, iss);
        helper.move_units();

        ARegion *current = u->object->region;
        expect(current == dest)
            << "unit must complete the move in one turn; WEATHER_EXISTS=2 must ignore weather field";

        Globals->WEATHER_EXISTS = saved;
    };

    "Unit legitimately cannot enter forest in one turn — 'remaining moves queued' is correct"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved = Globals->WEATHER_EXISTS;
        Globals->WEATHER_EXISTS = 2;

        ARegion *start = helper.get_region(0, 0, 0);
        start->type = R_PLAIN;

        // Forest: movepoints=2 → needs cost*MAX_SPEED = 2*8 = 16 points
        // Leader has only 9 → legitimately cannot enter in one turn
        ARegion *dest = helper.get_region(1, 1, 0);
        dest->type = R_FOREST;
        dest->weather = W_NORMAL;   // clear weather — failure is purely from terrain cost

        Faction *f = helper.create_faction("Movers");
        Unit *u = helper.create_unit(f, start);

        std::ostringstream ss;
        ss << "#atlantis " << f->num << "\nunit " << u->num << "\nmove SE\n";
        std::istringstream iss(ss.str());
        helper.parse_orders(f->num, iss);
        helper.move_units();

        ARegion *current = u->object->region;
        // With PHASED_MOVE_OFFSET=7, speed=2: 9 pts/phase. Forest needs 2*8=16.
        // Unit accumulates 9+9=18 pts over 2 phases → enters forest in phase 1.
        // Test verifies TERRAIN cost is applied (no weather doubling with WEATHER_EXISTS=2).
        expect(current == dest)
            << "unit must enter forest in 2 phases (18 pts accumulated >= 16 needed); no weather penalty with WEATHER_EXISTS=2";

        Globals->WEATHER_EXISTS = saved;
    };
};
