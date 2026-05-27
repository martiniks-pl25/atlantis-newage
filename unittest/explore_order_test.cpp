#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "items.h"
#include "testhelper.hpp"
#include <sstream>

namespace ut = boost::ut;

// Sum of baseamount across all products in a region.
static int total_product_baseamount(ARegion *r)
{
    int total = 0;
    for (const auto& p : r->products)
        total += p->baseamount;
    return total;
}

// Find the first R_NEXUS region in the world (returns nullptr if none).
static ARegion *find_nexus_region(UnitTestHelper &helper)
{
    for (auto *r : helper.get_regions())
        if (r->type == R_NEXUS) return r;
    return nullptr;
}

ut::suite<"ExploreOrder"> explore_order_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: EXPLORE RMAP — map is consumed, region production increases.
    // -----------------------------------------------------------------------
    "EXPLORE RMAP consumes map and boosts region production"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Explorer");
        Unit *u = helper.get_first_unit(fac);
        ARegion *r = u->object->region;

        u->items.SetNum(I_RESOURCE_MAP, 1);
        int before = total_product_baseamount(r);

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore rmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        expect(u->items.GetNum(I_RESOURCE_MAP) == 0_i) << "RMAP must be consumed";
        expect(fac->errors.size() == 0_ul) << "no errors expected";
        expect(fac->events.size() == 1_ul) << "one explore event expected";

        int delta = total_product_baseamount(r) - before;
        expect(delta >= 1_i) << "production boost must be at least 1";
        expect(delta <= 2_i) << "production boost must be at most 2 (1d2)";
    };

    // -----------------------------------------------------------------------
    // Test 2: EXPLORE RMAP without map → error, production unchanged.
    // -----------------------------------------------------------------------
    "EXPLORE RMAP without map yields error and no production change"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("NoMap");
        Unit *u = helper.get_first_unit(fac);
        ARegion *r = u->object->region;

        // no RMAP given — unit has only default I_LEADERS=1
        int before = total_product_baseamount(r);

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore rmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        expect(fac->errors.size() == 1_ul) << "one error expected";
        expect(fac->events.size() == 0_ul) << "no events expected";
        expect(total_product_baseamount(r) == before) << "production must not change";
    };

    // -----------------------------------------------------------------------
    // Test 3: EXPLORE TMAP → stub error, TMAP not consumed.
    // -----------------------------------------------------------------------
    "EXPLORE TMAP returns stub error and does not consume map"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("TreasureHunter");
        Unit *u = helper.get_first_unit(fac);
        u->items.SetNum(I_TREASURE_MAP, 1);

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore tmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        expect(fac->errors.size() == 1_ul) << "one error expected for stub TMAP";
        expect(u->items.GetNum(I_TREASURE_MAP) == 1_i) << "TMAP must not be consumed";
    };

    // -----------------------------------------------------------------------
    // Test 4: EXPLORE RMAP in nexus → error "no terrain resources", map consumed.
    // -----------------------------------------------------------------------
    "EXPLORE RMAP in nexus yields error and consumes map"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *nexus = find_nexus_region(helper);
        if (!nexus || !nexus->GetDummy()) return;  // skip if world has no nexus

        Faction *fac = helper.create_faction("NexusExplorer");
        Unit *u = helper.create_unit(fac, nexus);
        u->items.SetNum(I_RESOURCE_MAP, 1);

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore rmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        expect(fac->errors.size() == 1_ul) << "error expected for nexus (empty pool)";
        expect(u->items.GetNum(I_RESOURCE_MAP) == 0_i)
            << "RMAP must be consumed even when pool is empty";
    };

    // -----------------------------------------------------------------------
    // Test 5: add_or_increase_product increases an existing product's baseamount.
    // -----------------------------------------------------------------------
    "add_or_increase_product increases existing product baseamount"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Farmer");
        ARegion *r = helper.get_first_unit(fac)->object->region;
        if (r->products.empty()) return;

        Production *prod = r->products.front();
        int item = prod->itemtype;
        int before = prod->baseamount;

        r->add_or_increase_product(item, 3);

        expect(prod->baseamount == before + 3) << "baseamount must increase by exactly 3";
        expect(prod->amount == prod->baseamount) << "amount must equal baseamount after boost";
    };

    // -----------------------------------------------------------------------
    // Test 6: add_or_increase_product creates a new product when absent.
    // -----------------------------------------------------------------------
    "add_or_increase_product creates new product when item is absent"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Miner");
        ARegion *r = helper.get_first_unit(fac)->object->region;

        TerrainType *typer = &TerrainDefs[r->type];
        int target_item    = -1;
        int terrain_default = 1;
        for (unsigned int c = 0; c < sizeof(typer->prods)/sizeof(typer->prods[0]); c++) {
            int it = typer->prods[c].product;
            if (it == -1 || (ItemDefs[it].flags & ItemType::DISABLED)) continue;
            bool present = false;
            for (const auto& p : r->products)
                if (p->itemtype == it) { present = true; break; }
            if (!present) {
                target_item     = it;
                terrain_default = typer->prods[c].amount;
                break;
            }
        }
        if (target_item == -1) return;  // all terrain products already present — skip

        size_t before_count = r->products.size();
        r->add_or_increase_product(target_item, 2);

        expect(r->products.size() == before_count + 1u) << "products list must grow by 1";

        Production *new_prod = nullptr;
        for (auto *p : r->products)
            if (p->itemtype == target_item) { new_prod = p; break; }

        expect(new_prod != nullptr) << "new product must be findable";
        if (new_prod) {
            expect(new_prod->baseamount == terrain_default + 2)
                << "baseamount must be terrain_default + bonus";
            expect(new_prod->amount == new_prod->baseamount)
                << "amount must equal baseamount";
        }
    };
};
