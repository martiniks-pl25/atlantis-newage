#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "army.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Build a single-unit Army from a unit placed in region r.
// Caller owns the returned Army and must delete it.
static Army *build_test_army(Unit *u, ARegion *r) {
    Location loc;
    loc.unit   = u;
    loc.obj    = r->GetDummy();
    loc.region = r;

    std::list<Location *> locs;
    locs.push_back(&loc);

    return new Army(u, locs, r->type);
}

ut::suite<"BehindCapable"> behind_capable_suite = [] {
    using namespace ut;

    // ---------------------------------------------------------------
    // Regression: mage (IT_MAN) + skeletons (IT_MONSTER) + FLAG_BEHIND
    //   Mage must go to back row; skeletons must stay in front.
    //   canfront == 5 (skeletons) verifies skeletons are NOT sent behind.
    // ---------------------------------------------------------------
    "Mage + skeletons with FLAG_BEHIND: skeletons stay in front row"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction  *faction = helper.create_faction("Testers");
        ARegion  *r       = helper.get_region(0, 0, 0);

        // create_unit already adds 1 I_LEADERS (IT_MAN)
        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_SKELETON, 5);
        u->SetFlag(FLAG_BEHIND, 1);

        Army *army = build_test_army(u, r);

        // 5 skeletons in front, 1 mage behind → canfront == 5
        expect(army->canfront == 5_i)
            << "skeletons (IT_MONSTER) must NOT be moved behind when FLAG_BEHIND is set";

        delete army;
    };

    // ---------------------------------------------------------------
    // Control: only IT_MONSTER (no BEHIND_CAPABLE, no IT_MAN) + FLAG_BEHIND
    //   All monsters must go to the front — no behind placement.
    // ---------------------------------------------------------------
    "Pure IT_MONSTER unit with FLAG_BEHIND: all soldiers stay in front row"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction  *faction = helper.create_faction("Testers");
        ARegion  *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);   // remove the default leader
        u->items.SetNum(I_SKELETON, 5);
        u->SetFlag(FLAG_BEHIND, 1);

        Army *army = build_test_army(u, r);

        // NumFront() > 0 (all 5 go front), so the "no-front" fallback is not triggered.
        expect(army->canfront == 5_i)
            << "plain IT_MONSTER must all be in front row regardless of FLAG_BEHIND";

        delete army;
    };

    // ---------------------------------------------------------------
    // New behaviour: PCAP (IT_MONSTER | BEHIND_CAPABLE) + crew (IT_MONSTER)
    //   PCAP must go to back row; crew must stay in front.
    // ---------------------------------------------------------------
    "PCAP (BEHIND_CAPABLE) + crew with FLAG_BEHIND: PCAP goes behind, crew stays in front"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction  *faction = helper.create_faction("Testers");
        ARegion  *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_PIRATE_CAPTAIN, 1);  // BEHIND_CAPABLE
        u->items.SetNum(I_PIRATES, 5);
        u->SetFlag(FLAG_BEHIND, 1);

        Army *army = build_test_army(u, r);

        // 5 pirates in front, 1 PCAP behind → canfront == 5
        expect(army->canfront == 5_i)
            << "PCAP (BEHIND_CAPABLE) must be placed behind; pirates must remain in front";

        delete army;
    };

    // ---------------------------------------------------------------
    // New behaviour: PBOS (IT_MONSTER | BEHIND_CAPABLE) + crew (IT_MONSTER)
    //   Same as PCAP test — PBOS also has BEHIND_CAPABLE.
    // ---------------------------------------------------------------
    "PBOS (BEHIND_CAPABLE) + crew with FLAG_BEHIND: PBOS goes behind, crew stays in front"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction  *faction = helper.create_faction("Testers");
        ARegion  *r       = helper.get_region(0, 0, 0);

        Unit *u = helper.create_unit(faction, r);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_PIRATE_BOSUN, 1);   // BEHIND_CAPABLE
        u->items.SetNum(I_PIRATES, 5);
        u->SetFlag(FLAG_BEHIND, 1);

        Army *army = build_test_army(u, r);

        // 5 pirates in front, 1 PBOS behind → canfront == 5
        expect(army->canfront == 5_i)
            << "PBOS (BEHIND_CAPABLE) must be placed behind; pirates must remain in front";

        delete army;
    };
};
