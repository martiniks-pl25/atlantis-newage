#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Monsters start with GUARD_AVOID, which keeps a unit out of an attack its
// neighbour starts. Lair pirates must lose it, or the crew and the bosun
// attack one by one. A fleet keeps it and relies on the crew rule.

ut::suite<"PirateLairCrew"> pirate_lair_crew_suite = [] {
    using namespace ut;

    "pirates in a lair fight as one crew"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        const auto make_lair = [&] {
            Object *lair = new Object(r);
            lair->type = O_ISLE;
            lair->num = r->buildingseq++;
            r->objects.push_back(lair);
            return lair;
        };
        const auto pirate_in = [&](Object *obj, int item, int count) {
            Unit *u = helper.create_monster(r, item, count);
            u->guard = GUARD_AVOID;
            u->MoveUnit(obj);
            return u;
        };

        Object *crewed = make_lair();
        Unit *crew = pirate_in(crewed, I_PIRATES, 40);
        Unit *bosun = pirate_in(crewed, I_PIRATE_BOSUN, 1);
        Unit *lone_crew = pirate_in(make_lair(), I_PIRATES, 30);
        Object *fleet = helper.create_empty_fleet(r, I_COG);
        Unit *fleet_crew = pirate_in(fleet, I_PIRATES, 20);
        Unit *fleet_bosun = pirate_in(fleet, I_PIRATE_BOSUN, 1);

        helper.run_rally_pirate_lairs();

        expect(crew->guard == GUARD_NONE) << "lair crew must not avoid";
        expect(bosun->guard == GUARD_NONE) << "lair bosun must not avoid";
        expect(lone_crew->guard == GUARD_NONE) << "a crew-only lair must not avoid";
        expect(fleet_crew->guard == GUARD_AVOID) << "fleets keep AVOID and the crew rule";
        expect(fleet_bosun->guard == GUARD_AVOID) << "fleets keep AVOID and the crew rule";
    };
};
