#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "gamedefs.h"
#include "aregion.h"
#include "object.h"
#include "orders.h"
#include "items.h"
#include "testhelper.hpp"
#include "../rng.hpp"

#include <sstream>
#include <vector>

namespace ut = boost::ut;

// Course inertia for NPC pirate fleets, and the prerequisite that makes it possible.
//
// Prerequisite: Object::prevdir (the direction back to where the fleet came from) is
// now stored in the save unconditionally, and the ALLOW_TRIVIAL_PORTAGE waiver is
// applied lazily in Do1SailOrder, at the fleet's first real step of the turn. The
// printed isthmus rule is unchanged: a fleet may still not cross a one-hex land mass
// in a single pass, and an exit that was legal anyway is still not charged a canal.
//
// Inertia itself lives in Unit::DefaultOrders: the heading (prevdir + 3) gets extra
// weight, the two sides get a little, and the reversal is dropped while anywhere else
// is open. The heading is also carried from step to step inside one turn.

ut::suite<"PirateCourseInertia"> pirate_course_inertia_suite = [] {
    using namespace ut;

    // ------------------------------------------------------------------
    // Shared geometry: a one-hex isthmus. north (ocean) - mid (land) - south (ocean),
    // wired by hand so no other exit exists. SailThroughCheck and the canal cost block
    // only look at neighbors[] and terrain, so the hexes need not really be adjacent.
    // ------------------------------------------------------------------
    auto wire_isthmus = [](UnitTestHelper &helper, ARegion **out_north, ARegion **out_mid, ARegion **out_south) {
        ARegion *mid   = helper.get_region(0, 2, 0);
        ARegion *north = helper.get_region(1, 1, 0);
        ARegion *south = helper.get_region(0, 0, 0);
        mid->type   = R_PLAIN;
        north->type = R_OCEAN;
        south->type = R_OCEAN;

        for (int d = 0; d < NDIRS; d++) {
            mid->neighbors[d] = nullptr;
            north->neighbors[d] = nullptr;
            south->neighbors[d] = nullptr;
        }
        mid->neighbors[D_NORTH] = north;
        mid->neighbors[D_SOUTH] = south;
        north->neighbors[D_SOUTH] = mid;
        south->neighbors[D_NORTH] = mid;

        *out_north = north;
        *out_mid = mid;
        *out_south = south;
    };

    // Crew big enough for a longship (weight 200 -> fleet size 4).
    auto crew_a_fleet = [](UnitTestHelper &helper, Faction *f, ARegion *where) -> Unit * {
        Unit *u = helper.create_unit(f, where);
        u->SetMen(I_LEADERS, 6);
        helper.set_skill_level(u, S_SAILING, 2);
        helper.create_fleet(where, u, I_LONGSHIP, 1);
        return u;
    };

    auto issue_sail = [](UnitTestHelper &helper, Faction *f, Unit *u, const char *dirs) {
        std::stringstream ss;
        ss << "#atlantis " << f->num << " \"mypassword\"\n";
        ss << "unit " << u->num << "\n";
        ss << "sail " << dirs << "\n";
        helper.parse_orders(f->num, ss, nullptr);
        helper.move_units();
    };

    // ------------------------------------------------------------------
    // 1. Trivial portage still holds across the turn boundary: a fleet that ended a
    //    turn in a land hex leaves in any direction that reaches water, even though
    //    its stored prevdir now survives the load.
    // ------------------------------------------------------------------
    "Fleet that ended its turn in a land hex may sail straight out"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved_prevent = Globals->PREVENT_SAIL_THROUGH;
        int saved_portage = Globals->ALLOW_TRIVIAL_PORTAGE;
        Globals->PREVENT_SAIL_THROUGH = 1;
        Globals->ALLOW_TRIVIAL_PORTAGE = 1;

        ARegion *north, *mid, *south;
        wire_isthmus(helper, &north, &mid, &south);

        Faction *f = helper.create_faction("Sailors");
        Unit *u = crew_a_fleet(helper, f, mid);
        // Entered from the north on a previous turn; the value now persists.
        u->object->SetPrevDir(D_NORTH);

        issue_sail(helper, f, u, "s");

        expect(u->object->region == south)
            << "trivial portage must let a docked fleet leave along any water side";
        expect(that % u->moved == 1) << "a plain exit costs one movepoint";

        Globals->ALLOW_TRIVIAL_PORTAGE = saved_portage;
        Globals->PREVENT_SAIL_THROUGH = saved_prevent;
    };

    // ------------------------------------------------------------------
    // 2. The isthmus rule still bites inside a single turn: the waiver is spent on the
    //    first step, so the second step is judged against the prevdir just written.
    // ------------------------------------------------------------------
    "One-hex isthmus cannot be crossed in a single turn"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved_prevent = Globals->PREVENT_SAIL_THROUGH;
        int saved_portage = Globals->ALLOW_TRIVIAL_PORTAGE;
        Globals->PREVENT_SAIL_THROUGH = 1;
        Globals->ALLOW_TRIVIAL_PORTAGE = 1;

        ARegion *north, *mid, *south;
        wire_isthmus(helper, &north, &mid, &south);

        Faction *f = helper.create_faction("Sailors");
        Unit *u = crew_a_fleet(helper, f, north);

        issue_sail(helper, f, u, "s s");

        expect(u->object->region == mid)
            << "the fleet must be stopped on the isthmus, not carried through it";
        expect(u->object->region != south) << "a one-pass through-crossing must be refused";
        expect(that % u->object->prevdir == D_NORTH)
            << "the entry direction must be recorded for the blocked second step";

        Globals->ALLOW_TRIVIAL_PORTAGE = saved_portage;
        Globals->PREVENT_SAIL_THROUGH = saved_prevent;
    };

    // ------------------------------------------------------------------
    // 3. Canal cost. On the first step of the turn the exit is legal on its own (the
    //    portage waiver), so the canal enables nothing and charges nothing. On a later
    //    step the through-pass is genuinely canal-only and costs the tier price.
    // ------------------------------------------------------------------
    "No canal cost on the first step of the turn"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved_prevent = Globals->PREVENT_SAIL_THROUGH;
        int saved_portage = Globals->ALLOW_TRIVIAL_PORTAGE;
        Globals->PREVENT_SAIL_THROUGH = 1;
        Globals->ALLOW_TRIVIAL_PORTAGE = 1;

        ARegion *north, *mid, *south;
        wire_isthmus(helper, &north, &mid, &south);
        helper.create_building(mid, nullptr, O_CANAL);

        Faction *f = helper.create_faction("Sailors");
        Unit *u = crew_a_fleet(helper, f, mid);
        u->object->SetPrevDir(D_NORTH);

        issue_sail(helper, f, u, "s");

        expect(u->object->region == south) << "the fleet must leave the isthmus";
        expect(that % u->moved == 1)
            << "an exit the portage waiver already allows must not be charged the canal";

        Globals->ALLOW_TRIVIAL_PORTAGE = saved_portage;
        Globals->PREVENT_SAIL_THROUGH = saved_prevent;
    };

    "Canal through-pass on a later step still costs the tier price"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        int saved_prevent = Globals->PREVENT_SAIL_THROUGH;
        int saved_portage = Globals->ALLOW_TRIVIAL_PORTAGE;
        Globals->PREVENT_SAIL_THROUGH = 1;
        Globals->ALLOW_TRIVIAL_PORTAGE = 1;

        ARegion *north, *mid, *south;
        wire_isthmus(helper, &north, &mid, &south);
        helper.create_building(mid, nullptr, O_CANAL);

        Faction *f = helper.create_faction("Sailors");
        Unit *u = crew_a_fleet(helper, f, north);

        issue_sail(helper, f, u, "s s");

        expect(u->object->region == south)
            << "a stone canal must carry the fleet across on the second step";
        expect(that % u->moved == 3)
            << "one movepoint for the entry plus two for the stone canal pass";

        Globals->ALLOW_TRIVIAL_PORTAGE = saved_portage;
        Globals->PREVENT_SAIL_THROUGH = saved_prevent;
    };

    // ------------------------------------------------------------------
    // 4. prevdir survives serialization - the whole point of the prerequisite.
    // ------------------------------------------------------------------
    "prevdir survives a write/read round trip"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);

        Object src(r);
        src.type = O_FLEET;      // set before naming: a dummy object refuses set_name
        src.num = 4242;
        src.set_name("Pirate Cog");
        src.AddShip(I_COG);
        src.SetPrevDir(D_NORTHEAST);

        std::stringstream ss;
        src.Writeout(ss);

        Object dst(r);
        dst.Readin(ss, helper.get_factions());

        expect(that % dst.prevdir == D_NORTHEAST)
            << "the stored heading must come back unchanged from the save";
    };

    "A never-sailed fleet still round trips as no heading"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);

        Object src(r);
        src.type = O_FLEET;
        src.num = 4243;
        src.set_name("Fresh Cog");
        src.AddShip(I_COG);

        std::stringstream ss;
        src.Writeout(ss);

        Object dst(r);
        dst.Readin(ss, helper.get_factions());

        expect(that % dst.prevdir == -1) << "an unsailed fleet must read back as -1";
    };

    // ------------------------------------------------------------------
    // Cone geometry: four ocean hexes, each wired N / NE / S to another of them, so
    // every hex always offers a heading, a side and a reversal. A reversal is
    // therefore always geometrically available - only the cone can prevent it.
    // ------------------------------------------------------------------
    auto wire_ocean_ring = [](UnitTestHelper &helper) -> std::vector<ARegion *> {
        std::vector<ARegion *> hex = {
            helper.get_region(0, 0, 0),
            helper.get_region(1, 1, 0),
            helper.get_region(0, 2, 0),
            helper.get_region(1, 3, 0)
        };
        for (auto r : hex) {
            r->type = R_OCEAN;
            for (int d = 0; d < NDIRS; d++) r->neighbors[d] = nullptr;
        }
        for (size_t i = 0; i < hex.size(); i++) {
            hex[i]->neighbors[D_NORTH]     = hex[(i + 1) % hex.size()];
            hex[i]->neighbors[D_NORTHEAST] = hex[(i + 2) % hex.size()];
            hex[i]->neighbors[D_SOUTH]     = hex[(i + 3) % hex.size()];
        }
        return hex;
    };

    auto route_of = [](Unit *pirates) -> std::vector<int> {
        std::vector<int> steps;
        auto *so = dynamic_cast<SailOrder *>(pirates->monthorders);
        if (so) for (auto *d : so->dirs) if (d->dir != MOVE_PAUSE) steps.push_back(d->dir);
        return steps;
    };

    // ------------------------------------------------------------------
    // 5. Between turns: a fleet with a stored heading never opens the turn by sailing
    //    straight back, as long as forward or a side is open. Asserted as "not once in
    //    200 draws", which is an impossible outcome rather than an unlikely one.
    // ------------------------------------------------------------------
    "Fleet never reverses on the first step of a turn"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        auto hex = wire_ocean_ring(helper);
        Unit *pirates = helper.create_npc_pirate_fleet(hex[0], 3);
        // Came from the south, so the heading is north and D_SOUTH is the reversal.
        pirates->object->SetPrevDir(D_SOUTH);

        int reversals = 0;
        int forwards = 0;
        int routes = 0;
        for (int seed = 0; seed < 200; seed++) {
            rng::seed_random(seed);
            pirates->DefaultOrders(pirates->object);
            auto steps = route_of(pirates);
            if (steps.empty()) continue;
            routes++;
            if (steps[0] == D_SOUTH) reversals++;
            if (steps[0] == D_NORTH) forwards++;
        }

        expect(that % routes > 0) << "the fleet must produce sailing routes at all";
        expect(that % reversals == 0)
            << "a fleet with a heading must never open the turn by sailing backward";
        expect(that % forwards > 0) << "the heading itself must remain reachable";
    };

    // ------------------------------------------------------------------
    // 6. Inside a turn: a multi-step route never doubles back on itself. Pauses are
    //    skipped when pairing steps, because a pause must not clear the course.
    // ------------------------------------------------------------------
    "Multi-step route never doubles back inside one turn"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        auto hex = wire_ocean_ring(helper);
        Unit *pirates = helper.create_npc_pirate_fleet(hex[0], 3);
        // No stored heading: the cone must be established by the first step alone.
        pirates->object->SetPrevDir(-1);

        int doublebacks = 0;
        int multistep_routes = 0;
        for (int seed = 0; seed < 300; seed++) {
            rng::seed_random(seed);
            pirates->DefaultOrders(pirates->object);
            auto steps = route_of(pirates);
            if (steps.size() > 1) multistep_routes++;
            for (size_t i = 1; i < steps.size(); i++) {
                if (steps[i] == (steps[i - 1] + 3) % NDIRS) doublebacks++;
            }
        }

        expect(that % multistep_routes > 0)
            << "the sample must contain routes of more than one step";
        expect(that % doublebacks == 0)
            << "course inertia must carry from step to step inside the turn";
    };

    // ------------------------------------------------------------------
    // Escape hatch: when the reversal is the only way out it must stay available,
    // or a fleet in a dead end would be stuck forever.
    // ------------------------------------------------------------------
    "Fleet in a dead end may still turn around"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *pocket = helper.get_region(0, 0, 0);
        ARegion *back   = helper.get_region(1, 1, 0);
        pocket->type = R_OCEAN;
        back->type   = R_OCEAN;
        for (int d = 0; d < NDIRS; d++) {
            pocket->neighbors[d] = nullptr;
            back->neighbors[d] = nullptr;
        }
        // The only exit is the way the fleet came in.
        pocket->neighbors[D_SOUTH] = back;
        back->neighbors[D_NORTH] = pocket;

        Unit *pirates = helper.create_npc_pirate_fleet(pocket, 3);
        pirates->object->SetPrevDir(D_SOUTH);

        bool escaped = false;
        for (int seed = 0; seed < 100 && !escaped; seed++) {
            rng::seed_random(seed);
            pirates->DefaultOrders(pirates->object);
            auto steps = route_of(pirates);
            if (!steps.empty() && steps[0] == D_SOUTH) escaped = true;
        }

        expect(escaped)
            << "with no forward or side option the reversal must keep its weight";
    };

    // ------------------------------------------------------------------
    // Shared geometry: a coastal land hex ("dock") the fleet reached from
    // D_NORTH, so SetPrevDir(D_NORTH) gives the heading D_SOUTH. Ocean is
    // wired at south (forward) and the two sides (SE / SW) by default; a
    // test overtypes them as needed. Hexes need not really be adjacent.
    // ------------------------------------------------------------------
    auto wire_dock = [](UnitTestHelper &helper, ARegion **out_dock, ARegion **out_north,
                        ARegion **out_south, ARegion **out_southeast, ARegion **out_southwest) {
        ARegion *dock      = helper.get_region(0, 0, 0);
        ARegion *north     = helper.get_region(1, 1, 0);
        ARegion *south     = helper.get_region(2, 2, 0);
        ARegion *southeast = helper.get_region(3, 3, 0);
        ARegion *southwest = helper.get_region(4, 4, 0);
        dock->type = R_PLAIN;
        north->type = R_OCEAN;
        south->type = R_OCEAN;
        southeast->type = R_OCEAN;
        southwest->type = R_OCEAN;

        for (int d = 0; d < NDIRS; d++) {
            dock->neighbors[d] = nullptr;
            north->neighbors[d] = nullptr;
            south->neighbors[d] = nullptr;
            southeast->neighbors[d] = nullptr;
            southwest->neighbors[d] = nullptr;
        }
        dock->neighbors[D_NORTH] = north;
        dock->neighbors[D_SOUTH] = south;
        dock->neighbors[D_SOUTHEAST] = southeast;
        dock->neighbors[D_SOUTHWEST] = southwest;
        north->neighbors[D_SOUTH] = dock;
        south->neighbors[D_NORTH] = dock;
        southeast->neighbors[D_NORTHWEST] = dock;
        southwest->neighbors[D_NORTHEAST] = dock;

        *out_dock = dock;
        *out_north = north;
        *out_south = south;
        *out_southeast = southeast;
        *out_southwest = southwest;
    };

    // ------------------------------------------------------------------
    // 10. A fleet that ended its turn in a coastal land hex (docked) keeps
    //     the bearing it arrived on: the heading is read from any hex, not
    //     just ocean, so leaving port continues the voyage.
    // ------------------------------------------------------------------
    "Fleet leaving a dock keeps its heading"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *dock, *north, *south, *southeast, *southwest;
        wire_dock(helper, &dock, &north, &south, &southeast, &southwest);

        Unit *pirates = helper.create_npc_pirate_fleet(dock, 3);
        // Reached the dock from the north, so the heading is south.
        pirates->object->SetPrevDir(D_NORTH);

        int backtracks = 0;
        int forwards = 0;
        int routes = 0;
        for (int seed = 0; seed < 200; seed++) {
            rng::seed_random(seed);
            pirates->DefaultOrders(pirates->object);
            auto steps = route_of(pirates);
            if (steps.empty()) continue;
            routes++;
            if (steps[0] == D_NORTH) backtracks++;
            if (steps[0] == D_SOUTH) forwards++;
        }

        expect(that % routes > 0) << "the fleet must produce sailing routes at all";
        expect(that % backtracks == 0)
            << "a docked fleet keeps its bearing and must never sail back the way it came";
        expect(that % forwards > 0) << "the heading itself must remain reachable";
    };

    // ------------------------------------------------------------------
    // 11. With the heading itself on land, the fleet keeps going along the
    //     coast: both coastal sides are open, so the reversal is still
    //     dropped.
    // ------------------------------------------------------------------
    "Heading into land falls back to the coastal sides"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *dock, *north, *south, *southeast, *southwest;
        wire_dock(helper, &dock, &north, &south, &southeast, &southwest);
        // The heading points at land; only the two sides are water.
        south->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(dock, 3);
        pirates->object->SetPrevDir(D_NORTH);

        int backtracks = 0;
        int se = 0;
        int sw = 0;
        for (int seed = 0; seed < 200; seed++) {
            rng::seed_random(seed);
            pirates->DefaultOrders(pirates->object);
            auto steps = route_of(pirates);
            if (steps.empty()) continue;
            if (steps[0] == D_NORTH) backtracks++;
            if (steps[0] == D_SOUTHEAST) se++;
            if (steps[0] == D_SOUTHWEST) sw++;
        }

        expect(that % backtracks == 0)
            << "with a side open the reversal is still dropped even when the heading is land";
        expect(that % se > 0)
            << "the southeast side must be reachable when the heading itself is land";
        expect(that % sw > 0)
            << "the southwest side must be reachable when the heading itself is land";
    };

    // ------------------------------------------------------------------
    // 12. A dock whose only sea exit is the way back still puts to sea: the
    //     escape hatch keeps the reversal's weight when nothing else is open.
    // ------------------------------------------------------------------
    "A dock with one sea exit still puts to sea"_test = [&] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *dock, *north, *south, *southeast, *southwest;
        wire_dock(helper, &dock, &north, &south, &southeast, &southwest);
        // Close every exit but the way in: the heading and both sides are land.
        south->type = R_PLAIN;
        southeast->type = R_PLAIN;
        southwest->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(dock, 3);
        pirates->object->SetPrevDir(D_NORTH);

        bool escaped = false;
        for (int seed = 0; seed < 100 && !escaped; seed++) {
            rng::seed_random(seed);
            pirates->DefaultOrders(pirates->object);
            auto steps = route_of(pirates);
            if (!steps.empty() && steps[0] == D_NORTH) escaped = true;
        }

        expect(escaped)
            << "with the way back the only sea exit the reversal must keep its weight";
    };
};
