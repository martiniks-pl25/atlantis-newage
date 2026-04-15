#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "orders.h"
#include "testhelper.hpp"

#include <sstream>
#include <string>

namespace ut = boost::ut;
using namespace std;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ARegion *get_city_region(UnitTestHelper &h) { return h.get_region(0, 0, 0); }
static ARegion *get_plain_region(UnitTestHelper &h) { return h.get_region(0, 2, 0); }


/// Give a unit the minimum requirements to found a village.
static void give_founder_items(Unit *u, int mantype, int men_count = 1000, int wagons = 100)
{
    u->items.SetNum(mantype, men_count);
    u->items.SetNum(I_WAGON, wagons);
}

/// Parse a CREATE order for the unit and run month orders.
static void issue_create(UnitTestHelper &h, Faction *f, Unit *u, const string &name)
{
    stringstream ss;
    ss << "#atlantis " << f->num << " \"pw\"\n";
    ss << "unit " << u->num << "\n";
    ss << "create village \"" << name << "\"\n";
    h.parse_orders(f->num, ss, nullptr);
    h.run_month_orders();
}

// ---------------------------------------------------------------------------
// Test suite
// ---------------------------------------------------------------------------

ut::suite<"CREATE VILLAGE order"> create_order_suite = [] {
    using namespace ut;

    // ------------------------------------------------------------------
    // 1. Parsing
    // ------------------------------------------------------------------

    "CREATE VILLAGE parses correctly and sets monthorders"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();
        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        Unit *u = h.create_unit(f, r);
        give_founder_items(u, I_PLAINSMAN);

        stringstream ss;
        ss << "#atlantis " << f->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "create village \"Test Settlement\"\n";
        h.parse_orders(f->num, ss, nullptr);

        expect(u->monthorders != nullptr) << "monthorders must be set";
        expect(u->monthorders->type == O_CREATE) << "order type must be O_CREATE";
        auto *order = dynamic_cast<CreateOrder *>(u->monthorders);
        expect(order != nullptr) << "monthorders must cast to CreateOrder";
        expect(order->settlementType == TOWN_VILLAGE) << "settlement type must be TOWN_VILLAGE";
        expect(order->name == "Test Settlement") << "settlement name must match";
    };

    "CREATE with unknown settlement type reports parse error"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();
        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        Unit *u = h.create_unit(f, r);

        stringstream check_out;
        orders_check checker(check_out);
        stringstream ss;
        ss << "#atlantis " << f->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "create hamlet \"NoPlace\"\n";
        h.parse_orders(f->num, ss, &checker);

        expect(checker.numerrors > 0) << "unknown settlement type must produce a parse error";
    };

    // ------------------------------------------------------------------
    // 2. Success path
    // ------------------------------------------------------------------

    "CREATE VILLAGE succeeds and creates a village with correct race"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        // The test world has a city at (0,0,0). Remove it so distance check
        // does not block founding in nearby regions.
        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);  // (0,2,0) — now no nearby towns
        Unit *u = h.create_unit(f, r);
        give_founder_items(u, I_VIKING, 1200, 150);

        issue_create(h, f, u, "New Haven");

        expect(r->town != nullptr)                              << "region must have a town";
        expect(r->town->TownType() == TOWN_VILLAGE)             << "must be a village";
        expect(r->town->name == "New Haven")                    << "name must match";
        expect(r->race == I_VIKING)                             << "region race must change to founding race";
        expect(u->items.GetNum(I_VIKING) == 200)                << "1000 men must be consumed (1200-1000=200)";
        expect(u->items.GetNum(I_WAGON) == 50)                  << "100 wagons must be consumed (150-100=50)";
        expect(f->errors.size() == 0_ul)                        << "no errors expected";
        expect(f->events.size() > 0_ul)                         << "founding event must be generated";
        // Markets: village should have at least food markets
        expect(r->markets.size() > 0_ul)                        << "markets must be set up";
    };

    "CREATE VILLAGE with leaders as majority keeps original region race"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        int original_race = r->race;
        Unit *u = h.create_unit(f, r);

        // 200 men + 800 leaders = 1000 total; leaders are majority
        u->items.SetNum(I_PLAINSMAN, 200);
        u->items.SetNum(I_LEADERS, 800);
        u->items.SetNum(I_WAGON, 100);

        issue_create(h, f, u, "Leader Town");

        expect(r->town != nullptr)              << "village must be created";
        expect(r->race == original_race)        << "race must NOT change when leaders fill the quota";
    };

    "CREATE VILLAGE with exactly 1000 leaders keeps original race"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        int original_race = r->race;
        Unit *u = h.create_unit(f, r);

        u->items.SetNum(I_LEADERS, 1000);
        u->items.SetNum(I_WAGON, 100);

        issue_create(h, f, u, "Leader Settlement");

        expect(r->town != nullptr)          << "village must be created";
        expect(r->race == original_race)    << "race must NOT change for pure-leader founding";
    };

    // ------------------------------------------------------------------
    // 3. Error cases
    // ------------------------------------------------------------------

    "CREATE VILLAGE fails when region already has a settlement"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_city_region(h);  // already has city
        Unit *u = h.create_unit(f, r);
        give_founder_items(u, I_PLAINSMAN);

        issue_create(h, f, u, "WontWork");

        expect(r->town->name != "WontWork") << "must not overwrite existing settlement";
        expect(f->errors.size() > 0_ul)     << "error must be reported";
    };

    "CREATE VILLAGE fails when too close to another settlement"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        // City at (0,0,0) still exists — plain at (0,2,0) is within 2 hexes
        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        Unit *u = h.create_unit(f, r);
        give_founder_items(u, I_PLAINSMAN);

        issue_create(h, f, u, "TooClose");

        expect(r->town == nullptr)          << "village must NOT be created when too close";
        expect(f->errors.size() > 0_ul)     << "error must be reported";
    };

    "CREATE VILLAGE fails with insufficient men"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        Unit *u = h.create_unit(f, r);
        u->items.SetNum(I_PLAINSMAN, 500);   // not enough
        u->items.SetNum(I_WAGON, 100);

        issue_create(h, f, u, "SmallGroup");

        expect(r->town == nullptr)      << "village must NOT be created";
        expect(f->errors.size() > 0_ul) << "error must be reported";
    };

    "CREATE VILLAGE fails with insufficient wagons"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        Unit *u = h.create_unit(f, r);
        u->items.SetNum(I_PLAINSMAN, 1000);
        u->items.SetNum(I_WAGON, 50);   // not enough

        issue_create(h, f, u, "NoWagons");

        expect(r->town == nullptr)      << "village must NOT be created";
        expect(f->errors.size() > 0_ul) << "error must be reported";
    };

    "CREATE VILLAGE with race-neutral founder keeps original region race"_test = [] {
        // unittest/extra.cpp has I_FAIRY in RACE_NEUTRAL_FOUNDERS
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        int original_race = r->race;
        Unit *u = h.create_unit(f, r);

        u->items.SetNum(I_FAIRY, 1000);
        u->items.SetNum(I_WAGON, 100);

        issue_create(h, f, u, "Fairy Glen");

        expect(r->town != nullptr)          << "village must be created";
        expect(r->race == original_race)    << "race must NOT change for race-neutral founder";
        expect(u->items.GetNum(I_FAIRY) == 0) << "all fairies must be consumed";
    };

    "CREATE VILLAGE: 1000 people with mix of men and leaders succeeds"_test = [] {
        UnitTestHelper h;
        h.initialize_game();
        h.setup_turn();

        ARegion *city_r = get_city_region(h);
        delete city_r->town;
        city_r->town = nullptr;

        Faction *f = h.create_faction("Founders");
        ARegion *r = get_plain_region(h);
        Unit *u = h.create_unit(f, r);

        // 600 men + 400 leaders = 1000 total; men are primary
        u->items.SetNum(I_BARBARIAN, 600);
        u->items.SetNum(I_LEADERS, 400);
        u->items.SetNum(I_WAGON, 100);

        issue_create(h, f, u, "Mixed Colony");

        expect(r->town != nullptr)                      << "village must be created";
        // Men are primary (600 >= 400 leaders), so race changes
        expect(r->race == I_BARBARIAN)                  << "region race must be set to primary man type";
        // 600 men should all be consumed (600 men + 400 leaders, consume 1000 total: 600 from men, 400 from leaders)
        expect(u->items.GetNum(I_BARBARIAN) == 0)       << "all barbarians must be consumed";
        expect(u->items.GetNum(I_LEADERS) == 0)         << "400 leaders must be consumed to make up 1000";
    };
};
