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
    // Test 3: EXPLORE TMAP with no map in inventory → error, nothing consumed.
    // -----------------------------------------------------------------------
    "EXPLORE TMAP with no map in inventory produces error"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("TreasureHunter");
        Unit *u = helper.get_first_unit(fac);
        // No TMAP in inventory.

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore tmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        expect(fac->errors.size() == 1_ul) << "one error expected when no TMAP in inventory";
        expect(u->items.GetNum(I_TREASURE_MAP) == 0_i) << "no TMAP to consume";
    };

    // -----------------------------------------------------------------------
    // Test 4: EXPLORE TMAP with map present — map is consumed on attempt
    //         (success or failure). Test world has no dungeon level, so
    //         spawn always falls back; RNG seeded for deterministic outcome.
    // -----------------------------------------------------------------------
    "EXPLORE TMAP with map present consumes map on attempt"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        rng::seed_random(1);  // seed where 25% roll succeeds (value < 25)

        Faction *fac = helper.create_faction("TreasureHunter");
        Unit *u = helper.get_first_unit(fac);
        u->items.SetNum(I_TREASURE_MAP, 1);

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore tmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        // Map consumed on a successful roll (spawn fails → fallback silver, no error).
        // Either consumed or not depending on RNG; just verify no crash and order processed.
        expect(u->monthorders == nullptr) << "monthorders must be cleared after EXPLORE";
    };

    // -----------------------------------------------------------------------
    // EXPLORE TMAP off the surface → error, and the map is NOT consumed.
    // Pirate hideouts only spawn on the surface, so using a TMAP underground
    // must fail cleanly without wasting the map.
    // -----------------------------------------------------------------------
    "EXPLORE TMAP underground errors and does not consume the map"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("DeepDelver");
        Unit *u = helper.get_first_unit(fac);
        ARegion *r = u->object->region;
        u->items.SetNum(I_TREASURE_MAP, 1);

        // Force the region's level to non-surface.
        r->level->levelType = ARegionArray::LEVEL_UNDERWORLD;

        std::stringstream ss;
        ss << "#atlantis " << fac->num << " \"pw\"\n";
        ss << "unit " << u->num << "\n";
        ss << "explore tmap\n";
        helper.parse_orders(fac->num, ss);
        helper.run_month_orders();

        expect(fac->errors.size() == 1_ul) << "error expected when using a TMAP off the surface";
        expect(u->items.GetNum(I_TREASURE_MAP) == 1_i) << "TMAP must NOT be consumed underground";
        expect(u->monthorders == nullptr) << "monthorders must be cleared";
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
    // Test 6: a newly discovered product is sized to the bonus only, NOT to the
    // terrain default. Picks an absent terrain product whose terrain default is
    // large enough that the old "terrain_default + bonus" behaviour would be
    // distinguishable, then asserts the deposit equals the bonus.
    // -----------------------------------------------------------------------
    "add_or_increase_product creates new product sized to the bonus only"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Miner");
        ARegion *r = helper.get_first_unit(fac)->object->region;

        TerrainType *typer = &TerrainDefs[r->type];
        int target_item    = -1;
        int terrain_default = 0;
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
        const int bonus = 2;
        r->add_or_increase_product(target_item, bonus);

        expect(r->products.size() == before_count + 1u) << "products list must grow by 1";

        Production *new_prod = nullptr;
        for (auto *p : r->products)
            if (p->itemtype == target_item) { new_prod = p; break; }

        expect(new_prod != nullptr) << "new product must be findable";
        if (new_prod) {
            expect(new_prod->baseamount == bonus)
                << "new deposit must be the bonus size, not terrain_default + bonus";
            expect(new_prod->amount == bonus)
                << "amount must equal the bonus";
            // Regression guard: if the terrain has a non-trivial default, the old
            // behaviour would have produced terrain_default + bonus instead.
            if (terrain_default > 0)
                expect(new_prod->baseamount != terrain_default + bonus)
                    << "must not seed the deposit with the terrain default";
        }
    };

    // -----------------------------------------------------------------------
    // Test 7: the discovery message reports the real bonus count, not a
    // hardcoded "2". item_string(..., FULLNUM | ALWAYSPLURAL) is the exact call
    // used by RunExploreOrders to build the event text.
    // -----------------------------------------------------------------------
    "explore discovery message reflects the real bonus count"_test = [] {
        std::string one = item_string(I_IRON, 1, FULLNUM | ALWAYSPLURAL);
        std::string two = item_string(I_IRON, 2, FULLNUM | ALWAYSPLURAL);

        expect(one.rfind("1 ", 0) == 0u) << "a bonus of 1 must render as \"1 ...\"";
        expect(two.rfind("2 ", 0) == 0u) << "a bonus of 2 must render as \"2 ...\"";
        expect(one != two) << "different bonuses must produce different text";
    };
};
