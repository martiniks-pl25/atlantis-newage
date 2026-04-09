#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

ut::suite<"PirateSeize"> pirate_seize_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Basic: pirates seize an empty ship docked on land
    // -----------------------------------------------------------------------
    "Pirates seize empty ship when docked on land"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 40 pirates: 10–15% = 4–6, all ≥ 4 so no bonus needed
        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        int before = pirates->items.GetNum(I_PIRATES);
        helper.run_pirate_seize_empty_ships();

        // Empty fleet must now have exactly one unit
        expect(empty->units.size() == 1u)
            << "seized fleet must have a boarding crew unit";

        int seized = empty->units.front()->items.GetNum(I_PIRATES);
        expect(seized >= 4)
            << "boarding crew must be at least 4";
        expect(seized <= 6)
            << "boarding crew must be at most 15% of 40 = 6";

        int remaining = pirates->items.GetNum(I_PIRATES);
        expect(remaining == before - seized)  // split = total (no bonus for 40)
            << "original fleet must have lost exactly the split amount";
    };

    // -----------------------------------------------------------------------
    // Min crew: 20 pirates → 10–15% = 2–3, bonus brings total to 4
    // -----------------------------------------------------------------------
    "Seized ship gets 4 pirates minimum including bonus recruits"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 20 pirates: max split = max(2, 20*15/100=3) = 3 < 4, so bonus kicks in
        Unit *pirates = helper.create_npc_pirate_fleet(r, 20);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(empty->units.size() == 1u)
            << "seized fleet must have a boarding crew";

        int seized = empty->units.front()->items.GetNum(I_PIRATES);
        expect(seized == 4)
            << "boarding crew must be exactly 4 (2-3 split + bonus recruits)";

        // Original fleet loses only split (2-3), not 4
        int remaining = pirates->items.GetNum(I_PIRATES);
        expect(remaining >= 17)
            << "original fleet must retain at least 17 pirates (20 - 3 split)";
        expect(remaining <= 18)
            << "original fleet must not retain more than 18 pirates (20 - 2 split)";
    };

    // -----------------------------------------------------------------------
    // Ocean: no seizure
    // -----------------------------------------------------------------------
    "Pirates do not seize empty ships in ocean"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_OCEAN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(empty->units.empty())
            << "empty fleet must not be seized while in ocean";
        expect(pirates->items.GetNum(I_PIRATES) == 40)
            << "pirate crew must be unchanged in ocean";
    };

    // -----------------------------------------------------------------------
    // Lake: no seizure
    // -----------------------------------------------------------------------
    "Pirates do not seize empty ships on a lake"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_LAKE;

        helper.create_npc_pirate_fleet(r, 40);
        Object *empty = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(empty->units.empty())
            << "empty fleet must not be seized while on a lake";
    };

    // -----------------------------------------------------------------------
    // Below MIN_PIRATES (20): no seizure
    // -----------------------------------------------------------------------
    "Pirates below minimum crew (20) do not seize empty ships"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 19);
        Object *empty  = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        expect(empty->units.empty())
            << "empty fleet must not be seized when pirates < 20";
        expect(pirates->items.GetNum(I_PIRATES) == 19)
            << "pirate crew must be unchanged";
    };

    // -----------------------------------------------------------------------
    // Generic name: renamed after seizure
    // -----------------------------------------------------------------------
    "Seized ship with generic 'Ship' name gets renamed"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        helper.create_npc_pirate_fleet(r, 40);
        Object *empty = helper.create_empty_fleet(r, I_COG, "Ship");

        helper.run_pirate_seize_empty_ships();

        expect(!empty->units.empty())
            << "ship must be seized";
        // Object::set_name stores "Name [N]", so check starts_with
        expect(!empty->name.starts_with("Ship"))
            << "generic 'Ship' name must be replaced with a pirate name";
    };

    // -----------------------------------------------------------------------
    // Custom name: preserved after seizure
    // Object::set_name stores "Name [N]", so compare with starts_with
    // -----------------------------------------------------------------------
    "Seized ship with custom name keeps its name"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        helper.create_npc_pirate_fleet(r, 40);
        Object *empty = helper.create_empty_fleet(r, I_COG, "Serenity");

        helper.run_pirate_seize_empty_ships();

        expect(!empty->units.empty())
            << "ship must be seized";
        expect(empty->name.starts_with("Serenity"))
            << "custom name must be preserved";
    };

    // -----------------------------------------------------------------------
    // Multiple empty ships: each gets its own boarding crew
    // -----------------------------------------------------------------------
    "Each empty ship in region gets seized independently"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        r->type = R_PLAIN;

        // 60 pirates: first split 6–9, second split 5–9 — both well above MIN
        Unit *pirates  = helper.create_npc_pirate_fleet(r, 60);
        Object *empty1 = helper.create_empty_fleet(r, I_COG, "Ship");
        Object *empty2 = helper.create_empty_fleet(r, I_COG, "Ship");

        int before = pirates->items.GetNum(I_PIRATES);
        helper.run_pirate_seize_empty_ships();

        expect(empty1->units.size() == 1u)
            << "first empty fleet must have a boarding crew";
        expect(empty2->units.size() == 1u)
            << "second empty fleet must have a boarding crew";

        int crew1 = empty1->units.front()->items.GetNum(I_PIRATES);
        int crew2 = empty2->units.front()->items.GetNum(I_PIRATES);
        int remaining = pirates->items.GetNum(I_PIRATES);

        expect(crew1 >= 4) << "first boarding crew must be at least 4";
        expect(crew2 >= 4) << "second boarding crew must be at least 4";
        expect(remaining == before - crew1 - crew2)
            << "original fleet must have lost both split amounts";
    };
};
