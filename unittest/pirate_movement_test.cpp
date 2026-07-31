#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "gamedefs.h"
#include "aregion.h"
#include "orders.h"
#include "testhelper.hpp"
#include "../rng.hpp"

namespace ut = boost::ut;

// Tests for pirate fleet movement avoidance:
//   Pirates (NPC fleet owners) skip regions with TOWN_TOWN or TOWN_CITY that have at
//   least one player unit on GUARD_GUARD.  Villages and NPC-only (faction 1) guarded
//   towns are always accessible.
//   Additionally, pirates skip coastal regions adjacent to such guarded towns/cities.
//
// Behaviour under test is in Unit::DefaultOrders (unit.cpp) when building the valid[]
// direction list for a pirate fleet owner.
//
// Test world: (0,0,0) = R_OCEAN (pirate start).
//   SE / SW from (0,0,0) both lead to (1,1,0) (wrap-around in 2×4 hex world).
//   S  from (0,0,0) leads to (0,2,0).
//   (1,1,0) SE/SW also lead to (0,2,0) — so (1,1,0) and (0,2,0) are adjacent.

ut::suite<"PirateMovement"> pirate_movement_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Negative: TOWN_CITY + player guard → pirate must not arrive there
    // -----------------------------------------------------------------------
    "Pirate fleet avoids TOWN_CITY with player guard on GUARD_GUARD"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_town = helper.get_region(1, 1, 0);
        r_town->type = R_PLAIN;
        r_town->add_town(TOWN_CITY);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_town);
        guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);
        pirates->DefaultOrders(pirates->object);
        helper.move_units();

        ARegion *current = pirates->object->region;
        expect(current != r_town)
            << "pirate fleet must not sail into TOWN_CITY guarded by a player unit";
    };

    // -----------------------------------------------------------------------
    // Negative: TOWN_TOWN + player guard → pirate must not arrive there
    // -----------------------------------------------------------------------
    "Pirate fleet avoids TOWN_TOWN with player guard on GUARD_GUARD"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_town = helper.get_region(1, 1, 0);
        r_town->type = R_PLAIN;
        r_town->add_town(TOWN_TOWN);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_town);
        guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);
        pirates->DefaultOrders(pirates->object);
        helper.move_units();

        ARegion *current = pirates->object->region;
        expect(current != r_town)
            << "pirate fleet must not sail into TOWN_TOWN guarded by a player unit";
    };

    // -----------------------------------------------------------------------
    // Positive: TOWN_VILLAGE + player guard → direction toward village still
    // appears in the valid set (villages are never blocked).
    // We run DefaultOrders with 50 different seeds to confirm the direction
    // toward (1,1,0) [D_SOUTHEAST / D_SOUTHWEST] is reachable at least once.
    // -----------------------------------------------------------------------
    "Pirate fleet does not skip TOWN_VILLAGE regardless of player guard"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_village = helper.get_region(1, 1, 0);
        r_village->type = R_PLAIN;
        r_village->add_town(TOWN_VILLAGE);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_village);
        guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);

        bool saw_village_dir = false;
        for (int seed = 0; seed < 50 && !saw_village_dir; seed++) {
            rng::seed_random(seed);
            pirates->ClearOrders();
            pirates->DefaultOrders(pirates->object);
            auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
            if (so) {
                for (auto *d : so->dirs) {
                    if (d->dir == D_SOUTHEAST || d->dir == D_SOUTHWEST) {
                        saw_village_dir = true;
                        break;
                    }
                }
            }
        }
        expect(saw_village_dir)
            << "TOWN_VILLAGE must always be accessible (not blocked even with player guard)";
    };

    // -----------------------------------------------------------------------
    // Positive: TOWN_CITY + NPC-only guard (faction 1 / guardfaction) →
    // direction toward town must appear in the valid set.
    // -----------------------------------------------------------------------
    "Pirate fleet can sail to TOWN_CITY with only NPC guard (faction 1)"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_town = helper.get_region(1, 1, 0);
        r_town->type = R_PLAIN;
        r_town->add_town(TOWN_CITY);

        // Faction 1 is guardfaction (NPC city guards)
        Faction *npc_faction = helper.get_faction(1);
        Unit *npc_guard = helper.create_unit(npc_faction, r_town);
        npc_guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);

        bool saw_town_dir = false;
        for (int seed = 0; seed < 50 && !saw_town_dir; seed++) {
            rng::seed_random(seed);
            pirates->ClearOrders();
            pirates->DefaultOrders(pirates->object);
            auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
            if (so) {
                for (auto *d : so->dirs) {
                    if (d->dir == D_SOUTHEAST || d->dir == D_SOUTHWEST) {
                        saw_town_dir = true;
                        break;
                    }
                }
            }
        }
        expect(saw_town_dir)
            << "TOWN_CITY with only NPC guard (faction 1) must be accessible to pirate fleets";
    };

    // -----------------------------------------------------------------------
    // Negative: coastal region adjacent to player-guarded TOWN_CITY → blocked.
    // (1,1,0) has no town but is adjacent to (0,2,0) which has TOWN_CITY + player guard.
    // All coastal destinations from (0,0,0) become blocked → pirate stays in ocean.
    // -----------------------------------------------------------------------
    "Pirate fleet avoids coastal region adjacent to player-guarded TOWN_CITY"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_coastal = helper.get_region(1, 1, 0);
        r_coastal->type = R_PLAIN;   // no town — adjacency rule should block it

        ARegion *r_city = helper.get_region(0, 2, 0);
        r_city->type = R_PLAIN;
        r_city->add_town(TOWN_CITY);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_city);
        guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);

        // Run DefaultOrders with many seeds — pirate should always stay
        for (int seed = 0; seed < 50; seed++) {
            rng::seed_random(seed);
            pirates->ClearOrders();
            pirates->DefaultOrders(pirates->object);
            auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
            if (so) {
                for (auto *d : so->dirs) {
                    expect(d->dir == MOVE_PAUSE)
                        << "pirate must not sail toward coastal region adjacent to player-guarded TOWN_CITY (seed="
                        << seed << ")";
                }
            }
        }
    };

    // -----------------------------------------------------------------------
    // Positive: coastal region adjacent to NPC-only guarded TOWN_CITY → accessible.
    // Same topology but faction 1 guard → adjacency check must NOT block (1,1,0).
    // -----------------------------------------------------------------------
    "Pirate fleet can sail to coastal region adjacent to NPC-only guarded TOWN_CITY"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_coastal = helper.get_region(1, 1, 0);
        r_coastal->type = R_PLAIN;   // no town

        ARegion *r_city = helper.get_region(0, 2, 0);
        r_city->type = R_PLAIN;
        r_city->add_town(TOWN_CITY);

        Faction *npc_faction = helper.get_faction(1);
        Unit *npc_guard = helper.create_unit(npc_faction, r_city);
        npc_guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);

        bool saw_coastal_dir = false;
        for (int seed = 0; seed < 50 && !saw_coastal_dir; seed++) {
            rng::seed_random(seed);
            pirates->ClearOrders();
            pirates->DefaultOrders(pirates->object);
            auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
            if (so) {
                for (auto *d : so->dirs) {
                    if (d->dir == D_SOUTHEAST || d->dir == D_SOUTHWEST) {
                        saw_coastal_dir = true;
                        break;
                    }
                }
            }
        }
        expect(saw_coastal_dir)
            << "coastal region adjacent to NPC-only guarded city must remain accessible";
    };

    // -----------------------------------------------------------------------
    // Positive: ocean region adjacent to player-guarded TOWN_CITY → still accessible.
    // Pirate at (1,1,0) R_OCEAN. (0,0,0) is R_OCEAN adjacent to guarded (0,2,0).
    // Ocean destinations are never blocked by the adjacency rule.
    // -----------------------------------------------------------------------
    "Pirate fleet can sail to ocean region adjacent to player-guarded TOWN_CITY"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_start = helper.get_region(1, 1, 0);
        r_start->type = R_OCEAN;

        ARegion *r_ocean_near_city = helper.get_region(0, 0, 0);
        r_ocean_near_city->type = R_OCEAN;   // ocean neighbor of r_start, adjacent to r_city

        ARegion *r_city = helper.get_region(0, 2, 0);
        r_city->type = R_PLAIN;
        r_city->add_town(TOWN_CITY);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_city);
        guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_start, 3);

        bool saw_ocean_dir = false;
        for (int seed = 0; seed < 50 && !saw_ocean_dir; seed++) {
            rng::seed_random(seed);
            pirates->ClearOrders();
            pirates->DefaultOrders(pirates->object);
            auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
            if (so) {
                for (auto *d : so->dirs) {
                    if (d->dir == D_NORTHEAST || d->dir == D_NORTHWEST) {
                        saw_ocean_dir = true;
                        break;
                    }
                }
            }
        }
        expect(saw_ocean_dir)
            << "ocean region adjacent to player-guarded city must remain accessible (no adjacency block on water)";
    };

    // -----------------------------------------------------------------------
    // Tier-scaled radius: a player-guarded TOWN_TOWN blocks only its own hex.
    // Topology is the same as the TOWN_CITY ring test above: (0,2,0) holds the
    // settlement, (1,1,0) is the plain land ring hex next to it, and SE/SW from
    // (0,0,0) both lead to (1,1,0) while S leads to the settlement itself.
    // Expected: the ring hex is reachable, the town hex is not.
    // -----------------------------------------------------------------------
    "Pirate fleet may enter the land ring around a player-guarded TOWN_TOWN"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ocean = helper.get_region(0, 0, 0);
        r_ocean->type = R_OCEAN;

        ARegion *r_coastal = helper.get_region(1, 1, 0);
        r_coastal->type = R_PLAIN;   // no town — the ring hex

        ARegion *r_town = helper.get_region(0, 2, 0);
        r_town->type = R_PLAIN;
        r_town->add_town(TOWN_TOWN);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_town);
        guard->guard = GUARD_GUARD;

        Unit *pirates = helper.create_npc_pirate_fleet(r_ocean, 3);

        bool saw_ring_dir = false;
        for (int seed = 0; seed < 50; seed++) {
            rng::seed_random(seed);
            pirates->ClearOrders();
            pirates->DefaultOrders(pirates->object);
            auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
            if (!so) continue;
            for (auto *d : so->dirs) {
                if (d->dir == D_SOUTHEAST || d->dir == D_SOUTHWEST) saw_ring_dir = true;
                expect(d->dir != D_SOUTH)
                    << "pirate must not enter the player-guarded TOWN_TOWN itself (seed="
                    << seed << ")";
            }
        }
        expect(saw_ring_dir)
            << "the land ring around a player-guarded TOWN_TOWN must stay reachable "
               "(only a city projects force onto the land around it)";
    };
};

// ---------------------------------------------------------------------------
// The avoidance rule itself, exercised directly rather than through 50 seeds of
// DefaultOrders. Both movement paths (Unit::DefaultOrders and Game::RunCallPirates)
// call these two functions and nothing else, so this suite is the single place
// where the radius is pinned. Defined in npc.cpp, declared in aregion.h.
// ---------------------------------------------------------------------------
ut::suite<"PirateAvoidance"> pirate_avoidance_suite = [] {
    using namespace ut;

    // Puts a player unit on GUARD_GUARD in `r`, so the region counts as guarded.
    auto guard_it = [](UnitTestHelper &helper, ARegion *r, const std::string& name) {
        Faction *f = helper.create_faction(name);
        Unit *u = helper.create_unit(f, r);
        u->guard = GUARD_GUARD;
    };

    "hex rule refuses a guarded town and city but never a village"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_village = helper.get_region(0, 0, 0);
        ARegion *r_town    = helper.get_region(1, 1, 0);
        ARegion *r_city    = helper.get_region(0, 2, 0);
        for (ARegion *r : { r_village, r_town, r_city }) r->type = R_PLAIN;
        r_village->add_town(TOWN_VILLAGE);
        r_town->add_town(TOWN_TOWN);
        r_city->add_town(TOWN_CITY);

        // Unguarded first: nothing is refused regardless of tier.
        expect(!pirate_avoids_settlement(r_village)) << "unguarded village";
        expect(!pirate_avoids_settlement(r_town))    << "unguarded town";
        expect(!pirate_avoids_settlement(r_city))    << "unguarded city";

        guard_it(helper, r_village, "V");
        guard_it(helper, r_town, "T");
        guard_it(helper, r_city, "C");

        expect(!pirate_avoids_settlement(r_village))
            << "a village holds too little force to deter pirates, even when guarded";
        expect(pirate_avoids_settlement(r_town)) << "a guarded town refuses pirates";
        expect(pirate_avoids_settlement(r_city)) << "a guarded city refuses pirates";
    };

    "hex rule ignores NPC guards and a null region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_city = helper.get_region(0, 2, 0);
        r_city->type = R_PLAIN;
        r_city->add_town(TOWN_CITY);

        Faction *npc = helper.get_faction(1);          // guardfaction
        Unit *npc_guard = helper.create_unit(npc, r_city);
        npc_guard->guard = GUARD_GUARD;

        expect(!pirate_avoids_settlement(r_city))
            << "NPC guards never attack pirates, so they deter nothing";
        expect(!pirate_avoids_settlement(nullptr)) << "a null region is never refused";
        expect(!pirate_avoids_city_ring(nullptr))  << "a null region is never refused";
    };

    "ring rule applies to city only, and never to water"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_ring = helper.get_region(1, 1, 0);  // adjacent to (0,2,0)
        ARegion *r_seat = helper.get_region(0, 2, 0);
        r_ring->type = R_PLAIN;
        r_seat->type = R_PLAIN;
        guard_it(helper, r_seat, "Owner");

        r_seat->add_town(TOWN_TOWN);
        expect(!pirate_avoids_city_ring(r_ring))
            << "a town does not project force onto the land around it";

        // Promote the same settlement to a city; the ring must close.
        r_seat->town->hab = 100000;
        r_seat->town->pop = 100000;
        r_seat->town->dev = 100;
        expect(that % r_seat->town->TownType() == TOWN_CITY) << "settlement must now be a city";
        expect(pirate_avoids_city_ring(r_ring))
            << "a city closes the ring of land around it";

        // Water next to the very same city is still open — this is what keeps a
        // fleet on land from ever being trapped.
        r_ring->type = R_OCEAN;
        expect(!pirate_avoids_city_ring(r_ring))
            << "water is never refused at any tier";
    };
};
