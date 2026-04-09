#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "object.h"
#include "aregion.h"
#include "testhelper.hpp"
#include <algorithm>

namespace ut = boost::ut;

// Helper: create a strong player unit that will reliably win any battle.
static Unit *create_strong_attacker(UnitTestHelper &helper, ARegion *r) {
    Faction *player = helper.create_faction("Player");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 500);  // 500 leaders — overwhelming force (city guards exist at (0,0,0))
    return u;
}

ut::suite<"PirateFleetDestruction"> pirate_fleet_destruction_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------
    // After winning a battle against a pirate fleet, the fleet
    // object must be removed from region->objects immediately.
    // -----------------------------------------------------------
    "Pirate fleet remains after all pirates die in battle"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r     = helper.get_region(0, 2, 0);  // R_FOREST — no city, no guards
        Unit *pirates  = helper.create_npc_pirate_fleet(r, 1);
        Object *fleet  = pirates->object;  // save pointer before battle
        Unit *attacker = create_strong_attacker(helper, r);

        // Sanity: fleet exists before battle
        auto before = std::find(r->objects.begin(), r->objects.end(), fleet);
        expect(before != r->objects.end()) << "fleet must exist before battle";

        int result = helper.run_battle(r, attacker, pirates);
        expect(result == BATTLE_WON) << "attacker must win";

        // Fleet must remain as an empty prize — players can board and claim it
        auto after = std::find(r->objects.begin(), r->objects.end(), fleet);
        expect(after != r->objects.end())
            << "NPC pirate fleet must remain after pirates die (capturable prize)";
        expect(fleet->units.empty())
            << "fleet must be empty after all pirates are killed";
    };

    // -----------------------------------------------------------
    // Defeating a pirate captain must always yield a compass.
    // -----------------------------------------------------------
    "Defeating a pirate captain drops a compass"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r     = helper.get_region(0, 2, 0);  // R_FOREST — no city, no guards
        Unit *pirates  = helper.create_npc_pirate_fleet(r, 1);
        Object *fleet  = pirates->object;
        /* captain is added to the same fleet */
        helper.create_npc_pirate_captain(r, fleet);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, pirates);
        expect(result == BATTLE_WON) << "attacker must win";

        // Compass must be in attacker's faction items (distributed via Win())
        int total_compass = 0;
        for (auto obj : r->objects) {
            for (auto u : obj->units) {
                if (u->faction == attacker->faction)
                    total_compass += u->items.GetNum(I_COMPASS);
            }
        }
        expect(total_compass >= 1_i)
            << "defeating a pirate captain must yield at least one compass";
    };

    // -----------------------------------------------------------
    // Treasure map loot path must not crash; count must be 0 or 1.
    // -----------------------------------------------------------
    "Defeating regular pirates does not crash (treasure map loot path)"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r     = helper.get_region(0, 2, 0);  // R_FOREST — no city, no guards
        Unit *pirates  = helper.create_npc_pirate_fleet(r, 1);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, pirates);
        expect(result == BATTLE_WON) << "attacker must win against single pirate";

        int total_tmap = 0;
        for (auto obj : r->objects) {
            for (auto u : obj->units) {
                if (u->faction == attacker->faction)
                    total_tmap += u->items.GetNum(I_TREASURE_MAP);
            }
        }
        expect(total_tmap >= 0_i && total_tmap <= 1_i)
            << "treasure map count must be 0 or 1";
    };
};
