#include "external/boost/ut.hpp"
#include "external/nlohmann/json.hpp"

using json = nlohmann::json;

#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "unit.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

using namespace std;

namespace {
    // Find a LIVE mayor (1+ leader). Dead mayors (SetMen(0) leftovers) are skipped
    // — they otherwise persist in object lists until the engine's DeleteEmptyUnits.
    Unit *find_mayor(ARegion *r) {
        for (const auto o : r->objects) {
            for (const auto u : o->units) {
                if (u->type == U_MAYOR && u->GetMen() > 0) return u;
            }
        }
        return nullptr;
    }

    Object *find_town_hall(ARegion *r) {
        for (const auto o : r->objects) {
            if (o->type == O_TOWN_HALL) return o;
        }
        return nullptr;
    }

    // World-gen seeds the starting city with mayor + guards (CreateCityMons in
    // NewGame). Tests need a clean slate to drive the new spawn/flee paths
    // without interference. Kill all NPC city units in r.
    void clear_city_mons(ARegion *r, UnitTestHelper& helper) {
        int gfac = helper.get_guardfaction();
        bool removed_any = true;
        while (removed_any) {
            removed_any = false;
            for (auto o : r->objects) {
                for (auto u : o->units) {
                    if (u->faction->num != gfac) continue;
                    if (u->type == U_MAYOR || u->type == U_GUARD ||
                        u->type == U_GUARDMAGE || u->type == U_GUARDCOMMANDER) {
                        r->Kill(u);
                        removed_any = true;
                        break;
                    }
                }
                if (removed_any) break;
            }
        }
    }

    Unit *seed_city_guard(UnitTestHelper& helper, ARegion *r) {
        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        Unit *g = helper.create_unit(gfac, r);
        g->type = U_GUARD;
        g->SetMen(I_LEADERS, 1000);   // overwhelming count to clear 50% × melee_max
        g->SetFlag(FLAG_BEHIND, 0);
        return g;
    }
}

ut::suite<"Mayor Town Hall"> mayor_town_hall_suite = [] {
    using namespace ut;

    "Newly spawned mayor has no equipment and no money"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        clear_city_mons(r, helper);

        helper.spawn_mayor(r);

        Unit *mayor = find_mayor(r);
        expect(mayor != nullptr);
        if (!mayor) return;

        expect(mayor->GetMen() == 1_i);
        expect(mayor->items.GetNum(I_SWORD) == 0_i);
        expect(mayor->items.GetNum(I_MSWORD) == 0_i);
        expect(mayor->items.GetNum(I_ADSWORD) == 0_i);
        expect(mayor->items.GetNum(I_CHAINARMOR) == 0_i);
        expect(mayor->items.GetNum(I_MCHAIN) == 0_i);
        expect(mayor->items.GetNum(I_ADRING) == 0_i);
        expect(mayor->items.GetNum(I_CORNUCOPIA) == 0_i);
        expect(mayor->items.GetNum(I_SHIELDSTONE) == 0_i);
        expect(mayor->GetMoney() == 0_i);
        expect(mayor->guard == GUARD_AVOID);
    };

    "After AdjustCityMons pass, mayor in hall gets full kit and base treasury"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;

        helper.spawn_mayor(r, hall);
        helper.run_adjust_city_mons(r);

        Unit *mayor = find_mayor(r);
        expect(mayor != nullptr);
        if (!mayor) return;

        bool has_weapon = mayor->items.GetNum(I_SWORD) > 0
                       || mayor->items.GetNum(I_MSWORD) > 0
                       || mayor->items.GetNum(I_ADSWORD) > 0;
        bool has_armor = mayor->items.GetNum(I_CHAINARMOR) > 0
                       || mayor->items.GetNum(I_MCHAIN) > 0
                       || mayor->items.GetNum(I_ADRING) > 0;
        expect(has_weapon);
        expect(has_armor);
        expect(mayor->items.GetNum(I_CORNUCOPIA) == 1_i);
        expect(mayor->items.GetNum(I_SHIELDSTONE) == 1_i);
        expect(mayor->GetMoney() >= (int)Globals->GUARD_MONEY);
        expect(mayor->guard == GUARD_AVOID);
    };

    "Mayor moves into empty completed Town Hall at end of turn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        seed_city_guard(helper, r);
        helper.spawn_mayor(r);
        Unit *mayor = find_mayor(r);
        if (!mayor) return;
        expect(mayor->object == r->GetDummy());

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;
        expect(hall->units.empty());

        helper.run_adjust_city_mons(r);

        expect(mayor->object == hall);
    };

    "Mayor does NOT move into incomplete Town Hall"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        seed_city_guard(helper, r);
        helper.spawn_mayor(r);
        Unit *mayor = find_mayor(r);
        if (!mayor) return;

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;
        hall->incomplete = ObjectDefs[O_TOWN_HALL].cost;

        helper.run_adjust_city_mons(r);

        expect(mayor->object == r->GetDummy());
    };

    "Mayor does NOT move into Town Hall while builders inside"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        seed_city_guard(helper, r);
        helper.spawn_mayor(r);
        Unit *mayor = find_mayor(r);
        if (!mayor) return;

        helper.create_building(r, leader, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;
        expect(!hall->units.empty());

        helper.run_adjust_city_mons(r);

        expect(mayor->object == r->GetDummy());
    };

    "Spawn into empty completed Town Hall does not require 75% melee guards"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        leader->guard = GUARD_GUARD;

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;

        expect(find_mayor(r) == nullptr);

        helper.run_adjust_city_mons(r);

        Unit *mayor = find_mayor(r);
        expect(mayor != nullptr);
        if (mayor) expect(mayor->object == hall);
    };

    "Mayor in Town Hall persists when guards drop (no attitude change)"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;

        helper.spawn_mayor(r, hall);
        Unit *mayor = find_mayor(r);
        if (!mayor) return;

        helper.run_adjust_city_mons(r);

        expect(find_mayor(r) != nullptr);
    };

    "Mayor in dummy flees when guards drop below 50%"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.spawn_mayor(r);
        expect(find_mayor(r) != nullptr);

        helper.run_adjust_city_mons(r);

        expect(find_mayor(r) == nullptr);
    };

    "Mayor in hall flees when guard faction is HOSTILE to a guarding player"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;
        helper.spawn_mayor(r, hall);

        leader->guard = GUARD_GUARD;

        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        gfac->set_attitude(f->num, AttitudeType::HOSTILE);

        helper.run_adjust_city_mons(r);

        expect(find_mayor(r) == nullptr);
    };

    "Mayor in hall stays when guard faction is UNFRIENDLY to a guarding player"_test = [] {
        // UNFRIENDLY is the soft penalty: mayor (and quests) survive; only quest
        // redemption is gated. Only HOSTILE causes the mayor to flee. See plan §3.4.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;
        helper.spawn_mayor(r, hall);

        leader->guard = GUARD_GUARD;

        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        gfac->set_attitude(f->num, AttitudeType::UNFRIENDLY);

        helper.run_adjust_city_mons(r);

        expect(find_mayor(r) != nullptr);
    };

    "Mayor spawns into hall when guard faction is UNFRIENDLY to lone guarding player"_test = [] {
        // No city guard, only an UNFRIENDLY player on GUARD: under new rule this
        // is enough to spawn (HOSTILE alone blocks).
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (!hall) return;

        leader->guard = GUARD_GUARD;
        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        gfac->set_attitude(f->num, AttitudeType::UNFRIENDLY);

        expect(find_mayor(r) == nullptr);

        helper.run_adjust_city_mons(r);

        Unit *mayor = find_mayor(r);
        expect(mayor != nullptr);
        if (mayor) expect(mayor->object == hall);
    };

    "Mayor does NOT spawn when guard faction is HOSTILE to lone guarding player"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);

        leader->guard = GUARD_GUARD;
        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        gfac->set_attitude(f->num, AttitudeType::HOSTILE);

        helper.run_adjust_city_mons(r);

        expect(find_mayor(r) == nullptr);
    };

    "Mayor does not spawn when no one stands on guard"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *f = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(f);
        ARegion *r = leader->object->region;
        if (!r->town) return;
        clear_city_mons(r, helper);

        helper.create_building(r, nullptr, O_TOWN_HALL);

        helper.run_adjust_city_mons(r);

        expect(find_mayor(r) == nullptr);
    };
};
