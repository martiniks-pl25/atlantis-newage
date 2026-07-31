#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "gamedefs.h"
#include "aregion.h"
#include "orders.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Helper: create a 1-leader apprentice with MANI level and a Bosun's Whistle.
// The unit gets U_APPRENTICE type automatically when MANI is set via set_skill_level
// (which triggers game.cpp:944 apprentice promotion).
// We then give BWHI so GetAvailSkill(S_CALL_PIRATES) returns the MANI level.
static Unit *create_whistle_caster(UnitTestHelper &helper, ARegion *r, int mani_level) {
    Faction *f = helper.create_faction("Caster");
    Unit *u = helper.create_unit(f, r);
    u->SetMen(I_LEADERS, 1);
    helper.set_skill_level(u, S_MANIPULATE, mani_level);
    u->type = U_APPRENTICE;
    u->items.SetNum(I_BOSUN_WHISTLE, 1);
    return u;
}

ut::suite<"PirateWhistle"> pirate_whistle_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Basic summon: fleet within radius gets a SailOrder toward caster.
    // World: (0,0,0) = ocean where caster stands,
    //        (1,1,0) = ocean with pirate fleet (distance 1).
    // MANI 1 → radius 1 → fleet should be redirected.
    // -----------------------------------------------------------------------
    "Whistle summons pirate fleet within radius 1"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;

        ARegion *r_fleet = helper.get_region(1, 1, 0);
        r_fleet->type = R_OCEAN;

        Unit *caster = create_whistle_caster(helper, r_caster, 1);
        Unit *pirates = helper.create_npc_pirate_fleet(r_fleet, 5);

        // Set random sail order so we can confirm it gets replaced.
        auto *random_so = new SailOrder;
        pirates->monthorders = random_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        expect(pirates->monthorders != nullptr) << "fleet owner must have monthorders after whistle";
        expect(pirates->monthorders != random_so) << "original SailOrder must be replaced";
        expect(pirates->monthorders->type == O_SAIL) << "new order must be a SailOrder";

        SailOrder *so = dynamic_cast<SailOrder *>(pirates->monthorders);
        expect(so != nullptr) << "monthorders must be castable to SailOrder";
        expect(!so->dirs.empty()) << "SailOrder must contain at least one direction";
    };

    // -----------------------------------------------------------------------
    // Out of range: fleet beyond radius is NOT redirected.
    // MANI 1 → radius 1. Fleet is at distance 2 → must not be touched.
    // -----------------------------------------------------------------------
    "Whistle does not summon fleet beyond radius"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;

        // In the 2×4 test world: (0,0)→(1,1) and (0,0)→(0,2) are both distance 1.
        // (1,3) has no direct edge to (0,0), so it is at distance 2.
        ARegion *r_hop1 = helper.get_region(1, 1, 0);
        r_hop1->type = R_OCEAN;
        ARegion *r_hop2 = helper.get_region(0, 2, 0);
        r_hop2->type = R_OCEAN;
        ARegion *r_fleet = helper.get_region(1, 3, 0); // truly 2 hops from caster
        r_fleet->type = R_OCEAN;

        Unit *caster = create_whistle_caster(helper, r_caster, 1); // radius 1
        Unit *pirates = helper.create_npc_pirate_fleet(r_fleet, 5);

        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        expect(pirates->monthorders == original_so)
            << "fleet beyond radius 1 must not be redirected";
    };

    // -----------------------------------------------------------------------
    // With radius = (level+1)/2, MANI 2 → radius 1, so distance-2 fleet is
    // out of range. MANI 3 → radius 2 → fleet at distance 2 IS redirected.
    // -----------------------------------------------------------------------
    "MANI 3 gives radius 2, reaches fleet at distance 2"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;
        ARegion *r_hop1 = helper.get_region(1, 1, 0);
        r_hop1->type = R_OCEAN;
        ARegion *r_hop2 = helper.get_region(0, 2, 0);
        r_hop2->type = R_OCEAN;
        ARegion *r_fleet = helper.get_region(1, 3, 0); // 2 hops from caster
        r_fleet->type = R_OCEAN;

        Unit *caster = create_whistle_caster(helper, r_caster, 3); // MANI 3 → radius 2
        Unit *pirates = helper.create_npc_pirate_fleet(r_fleet, 5);

        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        expect(pirates->monthorders != original_so)
            << "fleet at distance 2 must be redirected with MANI 3 (radius 2)";

        SailOrder *so = dynamic_cast<SailOrder *>(pirates->monthorders);
        expect(so != nullptr && !so->dirs.empty())
            << "redirected fleet must have a non-empty SailOrder";
    };

    // -----------------------------------------------------------------------
    // Closest wins: two casters from different factions, fleet goes to closer one.
    // Caster A at (0,0,0): distance to fleet at (1,1,0) = 1.
    // Caster B at (0,2,0): distance to fleet at (1,1,0) = 1 (equal — first wins).
    // Second caster should NOT override first.
    // -----------------------------------------------------------------------
    "Closest wins: second caster at equal distance does not override first"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_a    = helper.get_region(0, 0, 0);
        r_a->type = R_OCEAN;
        ARegion *r_fleet = helper.get_region(1, 1, 0);
        r_fleet->type = R_OCEAN;
        ARegion *r_b    = helper.get_region(0, 2, 0);
        r_b->type = R_OCEAN;

        Unit *caster_a = create_whistle_caster(helper, r_a, 1);
        Unit *caster_b = create_whistle_caster(helper, r_b, 1);
        Unit *pirates  = helper.create_npc_pirate_fleet(r_fleet, 5);

        // Cast A first, capture the SailOrder it sets
        helper.activate_spell(S_CALL_PIRATES, { r_a, caster_a, nullptr, 0, 0 });
        SailOrder *so_after_a = dynamic_cast<SailOrder *>(pirates->monthorders);
        expect(so_after_a != nullptr) << "caster A must redirect fleet";

        // Cast B second (equal distance) — must NOT override
        helper.activate_spell(S_CALL_PIRATES, { r_b, caster_b, nullptr, 0, 0 });
        SailOrder *so_after_b = dynamic_cast<SailOrder *>(pirates->monthorders);

        expect(so_after_b == so_after_a)
            << "equal-distance second caster must not override first (first-wins tie-break)";
    };

    // -----------------------------------------------------------------------
    // Closest wins: closer caster overrides farther one.
    // Caster A at distance 2, caster B at distance 1.
    // B must win even if A ran first.
    // A uses MANI 3 → radius 2 (covers distance 2).
    // -----------------------------------------------------------------------
    "Closest wins: nearer caster overrides farther one"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_far   = helper.get_region(0, 0, 0);
        r_far->type = R_OCEAN;
        ARegion *r_mid   = helper.get_region(1, 1, 0);
        r_mid->type = R_OCEAN;
        ARegion *r_near  = helper.get_region(0, 2, 0); // adjacent to fleet
        r_near->type = R_OCEAN;
        ARegion *r_fleet = helper.get_region(1, 3, 0);
        r_fleet->type = R_OCEAN;

        // Caster A: 2 hops from fleet, MANI 3 → radius 2
        Unit *caster_a = create_whistle_caster(helper, r_far, 3);
        // Caster B: 1 hop from fleet, MANI 1 → radius 1
        Unit *caster_b = create_whistle_caster(helper, r_near, 1);
        Unit *pirates  = helper.create_npc_pirate_fleet(r_fleet, 5);

        // A casts first (farther), B casts second (closer)
        helper.activate_spell(S_CALL_PIRATES, { r_far, caster_a, nullptr, 0, 0 });
        SailOrder *so_after_a = dynamic_cast<SailOrder *>(pirates->monthorders);
        expect(so_after_a != nullptr) << "caster A (farther) must initially redirect fleet";

        helper.activate_spell(S_CALL_PIRATES, { r_near, caster_b, nullptr, 0, 0 });
        SailOrder *so_after_b = dynamic_cast<SailOrder *>(pirates->monthorders);

        expect(so_after_b != so_after_a)
            << "closer caster B must override farther caster A";
        expect(so_after_b != nullptr && !so_after_b->dirs.empty())
            << "B's SailOrder must be valid";
    };

    // -----------------------------------------------------------------------
    // Guard avoidance preserved: fleet in radius but path blocked by guarded
    // TOWN_CITY → fleet produces empty SailOrder (no valid path) and is skipped.
    // -----------------------------------------------------------------------
    "Whistle respects player-guarded town avoidance"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Layout: ocean(0,0,0) [caster] — ocean(1,1,0) [fleet].
        // But (1,1,0) is adjacent to (0,2,0) which has a guarded city.
        // If (0,2,0) is a CITY with player guard, (1,1,0) (coastal land) would be
        // skipped due to adjacent-guarded-town rule.
        // Here we make (1,1,0) ocean so the fleet can be there, but the path
        // from (0,0,0) to (1,1,0) must pass the guard check on (1,1,0) itself.
        // Simplest test: fleet is in a region that is itself a guarded CITY —
        // BFS should not enter it, so fleet is not in dist → not redirected.
        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;

        ARegion *r_city = helper.get_region(1, 1, 0);
        r_city->type = R_PLAIN;
        r_city->add_town(TOWN_CITY);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_city);
        guard->guard = GUARD_GUARD;

        Unit *caster  = create_whistle_caster(helper, r_caster, 2);
        Unit *pirates = helper.create_npc_pirate_fleet(r_city, 5);

        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        expect(pirates->monthorders == original_so)
            << "fleet in player-guarded city must not be redirected (BFS skips guarded towns)";
    };

    // -----------------------------------------------------------------------
    // Parity with Unit::DefaultOrders: the avoidance radius scales with tier, so
    // the land ring around a guarded TOWN is reachable — only a CITY blocks its ring.
    // The rule lives in pirate_avoids_settlement / pirate_avoids_city_ring and both
    // movement paths must agree; this pair of tests pins that agreement.
    //
    // Layout: ocean(0,0,0) [caster] — land(1,1,0) [fleet, no town] — (0,2,0) [settlement].
    // (1,1,0) is adjacent to (0,2,0), so it is the ring hex.
    // -----------------------------------------------------------------------
    "Whistle reaches a fleet in the land ring around a player-guarded TOWN_TOWN"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;

        ARegion *r_ring = helper.get_region(1, 1, 0);
        r_ring->type = R_PLAIN;      // ring hex, no town of its own

        ARegion *r_town = helper.get_region(0, 2, 0);
        r_town->type = R_PLAIN;
        r_town->add_town(TOWN_TOWN);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_town);
        guard->guard = GUARD_GUARD;

        Unit *caster  = create_whistle_caster(helper, r_caster, 2);
        Unit *pirates = helper.create_npc_pirate_fleet(r_ring, 5);

        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        expect(pirates->monthorders != original_so)
            << "a town guards only its own hex, so the ring around it must stay reachable "
               "(CPIR must use the same radius as Unit::DefaultOrders)";
    };

    "Whistle still avoids the land ring around a player-guarded TOWN_CITY"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;

        ARegion *r_ring = helper.get_region(1, 1, 0);
        r_ring->type = R_PLAIN;      // ring hex, no town of its own

        ARegion *r_city = helper.get_region(0, 2, 0);
        r_city->type = R_PLAIN;
        r_city->add_town(TOWN_CITY);

        Faction *player = helper.create_faction("Player");
        Unit *guard = helper.create_unit(player, r_city);
        guard->guard = GUARD_GUARD;

        Unit *caster  = create_whistle_caster(helper, r_caster, 2);
        Unit *pirates = helper.create_npc_pirate_fleet(r_ring, 5);

        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        expect(pirates->monthorders == original_so)
            << "a city projects force onto the land around it, so its ring stays refused";
    };

    // -----------------------------------------------------------------------
    // Non-apprentice without BWHI cannot effectively call pirates:
    // GetSkill(S_CALL_PIRATES) returns 0 → level=0 → BFS has radius 0
    // → no fleet is redirected.
    // -----------------------------------------------------------------------
    "Normal unit without BWHI cannot call pirates"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;
        ARegion *r_fleet  = helper.get_region(1, 1, 0);
        r_fleet->type = R_OCEAN;

        // Normal unit, no MANI, no BWHI
        Faction *f = helper.create_faction("Normal");
        Unit *u = helper.create_unit(f, r_caster);
        u->SetMen(I_LEADERS, 1);

        Unit *pirates = helper.create_npc_pirate_fleet(r_fleet, 5);
        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, u, nullptr, 0, 0 });

        expect(pirates->monthorders == original_so)
            << "unit without BWHI/MANI must not redirect fleet (level=0, radius=0)";
    };

    // -----------------------------------------------------------------------
    // Mage with FORCE 3 (no MANI) + BWHI:
    // CPIR = max(0, 0, 3, 0) = 3 → radius = (3+1)/2 = 2.
    // Fleet at distance 2 must be redirected.
    // -----------------------------------------------------------------------
    "Mage with FORCE 3 + BWHI summons at radius 2"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;
        ARegion *r_hop1 = helper.get_region(1, 1, 0);
        r_hop1->type = R_OCEAN;
        ARegion *r_hop2 = helper.get_region(0, 2, 0);
        r_hop2->type = R_OCEAN;
        ARegion *r_fleet = helper.get_region(1, 3, 0); // 2 hops from caster
        r_fleet->type = R_OCEAN;

        Faction *f = helper.create_faction("Mage");
        Unit *u = helper.create_unit(f, r_caster);
        u->SetMen(I_LEADERS, 1);
        u->type = U_MAGE;
        helper.set_skill_level(u, S_FORCE, 3);
        u->items.SetNum(I_BOSUN_WHISTLE, 1);

        Unit *pirates = helper.create_npc_pirate_fleet(r_fleet, 5);
        auto *original_so = new SailOrder;
        pirates->monthorders = original_so;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, u, nullptr, 0, 0 });

        expect(pirates->monthorders != original_so)
            << "fleet at distance 2 must be redirected by mage with FORCE 3 (radius 2)";

        SailOrder *so = dynamic_cast<SailOrder *>(pirates->monthorders);
        expect(so != nullptr && !so->dirs.empty())
            << "mage-redirected fleet must have a non-empty SailOrder";
    };

    // -----------------------------------------------------------------------
    // MANI 5 → CPIR 5 → radius = (5+1)/2 = 3.
    // Fleet at distance 3 is in range, fleet at distance 4 is not.
    // -----------------------------------------------------------------------
    "MANI 5 gives radius 3, reaches distance 3 but not 4"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Build a chain: (0,0) → (1,1) → (0,2) → (1,3) — distance 3
        // (0,0) → (1,1) → (0,2) → (1,3) → (0,4) — distance 4
        ARegion *r_caster = helper.get_region(0, 0, 0);
        r_caster->type = R_OCEAN;
        ARegion *r_hop1 = helper.get_region(1, 1, 0);
        r_hop1->type = R_OCEAN;
        ARegion *r_hop2 = helper.get_region(0, 2, 0);
        r_hop2->type = R_OCEAN;
        ARegion *r_dist3 = helper.get_region(1, 3, 0); // 3 hops
        r_dist3->type = R_OCEAN;
        ARegion *r_dist4 = helper.get_region(0, 4, 0); // 4 hops
        r_dist4->type = R_OCEAN;

        Unit *caster = create_whistle_caster(helper, r_caster, 5); // MANI 5 → radius 3
        Unit *pirates_dist3 = helper.create_npc_pirate_fleet(r_dist3, 5);
        Unit *pirates_dist4 = helper.create_npc_pirate_fleet(r_dist4, 5);

        auto *so_dist3 = new SailOrder;
        pirates_dist3->monthorders = so_dist3;
        auto *so_dist4 = new SailOrder;
        pirates_dist4->monthorders = so_dist4;

        helper.activate_spell(S_CALL_PIRATES, { r_caster, caster, nullptr, 0, 0 });

        // Distance 3 → within radius 3 → must be redirected
        expect(pirates_dist3->monthorders != so_dist3)
            << "fleet at distance 3 must be redirected with MANI 5 (radius 3)";
        SailOrder *so3 = dynamic_cast<SailOrder *>(pirates_dist3->monthorders);
        expect(so3 != nullptr && !so3->dirs.empty())
            << "distance-3 fleet must have a valid SailOrder";

        // Distance 4 → beyond radius 3 → must NOT be touched
        expect(pirates_dist4->monthorders == so_dist4)
            << "fleet at distance 4 must not be redirected (beyond radius 3)";
    };
};
