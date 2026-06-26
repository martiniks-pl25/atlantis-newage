#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"
#include <sstream>

namespace ut = boost::ut;
using namespace std;

namespace {
    // Force a region's settlement to a given tier by inflating/deflating town prestige.
    // TownType() = pop * (dev + 220) / 270, bucketed against CITY_POP (economy.cpp:10).
    void force_city(ARegion *r)    { r->town->pop = 1000000; r->town->dev = 100; }
    void force_village(ARegion *r) { r->town->pop = 1;       r->town->dev = 0;   }

    // Issue a single CAPITAL order for a unit and process it (parse-time order).
    void issue_capital(UnitTestHelper &helper, Faction *fac, Unit *u) {
        stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "capital\n";
        helper.parse_orders(fac->num, ss);
    }
}

ut::suite<"CapitalOrder"> capital_order_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Happy path: owner of a finished Palace in a city sets the capital.
    // -----------------------------------------------------------------------
    "CAPITAL in a city Palace you own sets capital_region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        expect(r->town != nullptr) << "starting region must have a settlement";
        if (!r->town) return;
        force_city(r);
        expect(r->town->TownType() == TOWN_CITY) << "region must be a city";

        helper.create_building(r, leader, O_PALACE);  // moves leader into the Palace as owner

        issue_capital(helper, fac, leader);

        expect(fac->errors.size() == 0_ul) << "no errors expected";
        expect(fac->capital_region == r->num) << "capital_region must be set to the city";
    };

    // -----------------------------------------------------------------------
    // Not in a Palace → error, capital unchanged.
    // -----------------------------------------------------------------------
    "CAPITAL outside a Palace fails"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        force_city(r);

        helper.create_building(r, leader, O_TOWER);  // a Tower, not a Palace

        issue_capital(helper, fac, leader);

        expect(fac->errors.size() >= 1_ul) << "error expected when not in a Palace";
        expect(fac->capital_region == -1_i) << "capital must remain unset";
    };

    // -----------------------------------------------------------------------
    // Palace in a non-city settlement → error.
    // -----------------------------------------------------------------------
    "CAPITAL in a non-city settlement fails"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        force_village(r);
        expect(r->town->TownType() == TOWN_VILLAGE) << "region must be a village";

        helper.create_building(r, leader, O_PALACE);

        issue_capital(helper, fac, leader);

        expect(fac->errors.size() >= 1_ul) << "error expected for a non-city";
        expect(fac->capital_region == -1_i) << "capital must remain unset";
    };

    // -----------------------------------------------------------------------
    // Unit is in the Palace but is NOT its owner → error.
    // -----------------------------------------------------------------------
    "CAPITAL by a non-owner in the Palace fails"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Test Faction");
        Unit *owner = helper.get_first_unit(fac);
        ARegion *r = owner->object->region;
        if (!r->town) return;
        force_city(r);

        helper.create_building(r, owner, O_PALACE);   // owner is first → GetOwner() == owner
        Object *palace = owner->object;
        Unit *intruder = helper.create_unit(fac, r);
        intruder->MoveUnit(palace);                   // intruder is inside, but not the owner

        issue_capital(helper, fac, intruder);

        expect(fac->errors.size() >= 1_ul) << "error expected when not the Palace owner";
        expect(fac->capital_region == -1_i) << "capital must remain unset";
    };

    // -----------------------------------------------------------------------
    // Unfinished Palace → error.
    // -----------------------------------------------------------------------
    "CAPITAL in an unfinished Palace fails"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        force_city(r);

        helper.create_building(r, leader, O_PALACE);
        leader->object->incomplete = ObjectDefs[O_PALACE].cost;  // not finished

        issue_capital(helper, fac, leader);

        expect(fac->errors.size() >= 1_ul) << "error expected for an unfinished Palace";
        expect(fac->capital_region == -1_i) << "capital must remain unset";
    };

    // -----------------------------------------------------------------------
    // Re-declaring the same city is a no-op (no error, stays set).
    // -----------------------------------------------------------------------
    "CAPITAL on the current capital is a harmless no-op"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        force_city(r);
        helper.create_building(r, leader, O_PALACE);

        issue_capital(helper, fac, leader);
        expect(fac->capital_region == r->num);

        issue_capital(helper, fac, leader);  // again
        expect(fac->errors.size() == 0_ul) << "re-declaring the same capital must not error";
        expect(fac->capital_region == r->num) << "capital stays the same";
    };

    // -----------------------------------------------------------------------
    // Serialization round-trip: capital_region survives Writeout/Readin.
    // -----------------------------------------------------------------------
    "capital_region survives a save round-trip"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        fac->capital_region = 4242;

        stringstream ss;
        fac->Writeout(ss);

        Faction loaded;
        loaded.Readin(ss, CURRENT_ATL_VER);

        expect(loaded.capital_region == 4242_i) << "capital_region must round-trip through save";
    };
};
