#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "army.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

static Army *build_test_army(Unit *u, ARegion *r) {
    Location loc;
    loc.unit   = u;
    loc.obj    = r->GetDummy();
    loc.region = r;
    std::list<Location *> locs;
    locs.push_back(&loc);
    return new Army(u, locs, r->type);
}

ut::suite<"WeightedTargeting"> weighted_targeting_suite = [] {
    using namespace ut;

    // ---------------------------------------------------------------
    // Weight cache is initialised correctly for a pure front army.
    // 1 dragon (size=5, w=50) + 4 skeletons (size=2, w=2 each = 8).
    // Expected frontWeightTotal == allWeightTotal == 58.
    // ---------------------------------------------------------------
    "Weight totals initialised correctly"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Testers");
        ARegion *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_DRAGON, 1);    // size=5, w=50
        u->items.SetNum(I_SKELETON, 4);  // size=2, w=2 each

        Army *army = build_test_army(u, r);

        // No behind soldiers → frontWeightTotal == allWeightTotal
        expect(army->frontWeightTotal == army->allWeightTotal)
            << "all soldiers in front: front total must equal all total";
        // 50 + 4*2 = 58
        expect(army->allWeightTotal == 58_i)
            << "1 dragon (w=50) + 4 skeletons (w=2 each) = 58";

        delete army;
    };

    // ---------------------------------------------------------------
    // Behind soldiers contribute to allWeightTotal but NOT frontWeightTotal.
    // 1 dragon (size=5) in front + 4 leaders (size=2, IT_MAN) behind.
    // Only IT_MAN soldiers respect FLAG_BEHIND; monsters always go to front.
    // frontWeightTotal == 50, allWeightTotal == 58.
    // ---------------------------------------------------------------
    "Behind soldiers excluded from frontWeightTotal"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Testers");
        ARegion *r       = helper.get_region(0, 0, 0);

        // Dragon unit in front (no FLAG_BEHIND)
        Unit *uFront = helper.create_unit(faction, r);
        uFront->items.SetNum(I_LEADERS, 0);
        uFront->items.SetNum(I_DRAGON, 1);   // size=5, w=50

        // Leader unit behind (FLAG_BEHIND) — IT_MAN respects FLAG_BEHIND
        Unit *uBehind = helper.create_unit(faction, r);
        uBehind->items.SetNum(I_LEADERS, 4); // size=2, w=2 each
        uBehind->SetFlag(FLAG_BEHIND, 1);

        Location locFront, locBehind;
        locFront.unit  = uFront;  locFront.obj  = r->GetDummy(); locFront.region  = r;
        locBehind.unit = uBehind; locBehind.obj = r->GetDummy(); locBehind.region = r;
        std::list<Location *> locs;
        locs.push_back(&locFront);
        locs.push_back(&locBehind);

        Army *army = new Army(uFront, locs, r->type);

        expect(army->frontWeightTotal == 50_i)
            << "only dragon (w=50) is in front zone";
        expect(army->allWeightTotal == 58_i)
            << "dragon (50) + 4 skeletons (8) = 58";

        delete army;
    };

    // ---------------------------------------------------------------
    // Kill() decrements caches correctly.
    // Start: 1 dragon (w=50) + 4 skeletons (w=2 each) = 58.
    // Kill one skeleton (size=2): expected total drops to 56.
    // ---------------------------------------------------------------
    "Kill() decrements weight caches"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Testers");
        ARegion *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_DRAGON, 1);
        u->items.SetNum(I_SKELETON, 4);

        Army *army = build_test_army(u, r);
        expect(army->allWeightTotal == 58_i);

        // Kill soldier at index 0 with enough damage to guarantee death.
        // Skeletons have 1 hit, dragon has 50 hits — index 0 is first placed soldier.
        // We kill whichever is at index 0 with 999 damage.
        int sizeBefore = army->soldiers[0]->size;
        int expectedW  = (sizeBefore == 5) ? 50 : 2;
        army->Kill(0, 999);

        expect(army->allWeightTotal == (58 - expectedW))
            << "allWeightTotal must drop by the killed soldier's weight";
        expect(army->frontWeightTotal == (58 - expectedW))
            << "frontWeightTotal must drop too (all soldiers were in front)";

        delete army;
    };

    // ---------------------------------------------------------------
    // Statistical test: size-5 dragon vs 9 humans (size=2).
    // Weights: dragon=50, 9 humans=2 each → pool = 50+18 = 68.
    // Dragon should be hit 50/68 ≈ 73.5% of 2000 trials.
    // Accept if dragon hits > 1300 (generous lower bound ~65%).
    // ---------------------------------------------------------------
    "Dragon targeted proportionally more often than humans"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Testers");
        ARegion *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_DRAGON, 1);    // size=5, w=50
        u->items.SetNum(I_SKELETON, 9);  // size=2, w=2 each (9 skeletons)

        Army *army = build_test_army(u, r);

        // Determine which index is the dragon (size=5)
        int dragonIdx = -1;
        for (int i = 0; i < army->notbehind; i++) {
            if (army->soldiers[i]->size == 5) { dragonIdx = i; break; }
        }
        expect(dragonIdx >= 0_i) << "dragon must be in army";

        int dragonHits = 0;
        const int N = 2000;
        for (int t = 0; t < N; t++) {
            int target = army->GetTargetNum(nullptr, false);
            if (target == dragonIdx) dragonHits++;
        }

        // Expected ~1470 hits (73.5%), accept anything > 1300 (~65%)
        expect(dragonHits > 1300_i)
            << "dragon (w=50) should be targeted far more than skeletons (w=2)";

        delete army;
    };

    // ---------------------------------------------------------------
    // Large army test (2000 soldiers): GetTargetNum always returns valid index.
    // 1500 skeletons (size=2) + 500 dragons (size=5).
    // ---------------------------------------------------------------
    "Large army (2000 soldiers): GetTargetNum always valid"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Testers");
        ARegion *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_SKELETON, 1500);  // size=2
        u->items.SetNum(I_DRAGON, 500);     // size=5

        Army *army = build_test_army(u, r);

        expect(army->notbehind == 2000_i) << "army must have 2000 soldiers";

        bool allValid = true;
        for (int t = 0; t < 5000; t++) {
            int target = army->GetTargetNum(nullptr, false);
            if (target < 0 || target >= army->notbehind) {
                allValid = false;
                break;
            }
        }
        expect(allValid) << "GetTargetNum must always return valid index";

        delete army;
    };
};
