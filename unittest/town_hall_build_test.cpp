#include "external/boost/ut.hpp"
#include "external/nlohmann/json.hpp"

using json = nlohmann::json;

#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

using namespace std;

namespace {
    // Find any region without a settlement (wilderness or underworld).
    ARegion *find_wilderness(UnitTestHelper& helper) {
        for (const auto r : helper.get_regions()) {
            if (r->town == nullptr) return r;
        }
        return nullptr;
    }

    // Count Town Hall objects in a region (any state, including incomplete).
    size_t count_town_halls(ARegion *r) {
        size_t n = 0;
        for (const auto o : r->objects) if (o->type == O_TOWN_HALL) n++;
        return n;
    }

    bool any_error_contains(Faction *f, const string& fragment) {
        for (const auto& e : f->errors) {
            if (e.message.find(fragment) != string::npos) return true;
        }
        return false;
    }
}

ut::suite<"Town Hall Build"> town_hall_build_suite = [] {
    using namespace ut;

    "Town Hall builds successfully in a settlement"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(faction);
        ARegion *r = leader->object->region;
        expect(r->town != nullptr) << "starting region must have a settlement";

        leader->items.SetNum(I_WOOD, 30);
        helper.set_skill_level(leader, S_BUILDING, 3);

        size_t before = r->objects.size();

        stringstream ss;
        ss << "#atlantis " << faction->num << " \"mypassword\"\n";
        ss << "unit " << leader->num << "\n";
        ss << "build \"Town Hall\"\n";
        helper.parse_orders(faction->num, ss, nullptr);
        helper.run_month_orders();

        expect(faction->errors.size() == 0_ul) << "no errors expected when building first Town Hall";
        expect(r->objects.size() == before + 1) << "a new building object must be created";
        expect(count_town_halls(r) == 1_ul) << "exactly one Town Hall must exist";
    };

    "Cannot build a second Town Hall in the same region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(faction);
        ARegion *r = leader->object->region;
        Unit *builder = helper.create_unit(faction, r);

        // Pre-existing completed Town Hall.
        helper.create_building(r, nullptr, O_TOWN_HALL);
        expect(count_town_halls(r) == 1_ul);

        builder->items.SetNum(I_WOOD, 30);
        helper.set_skill_level(builder, S_BUILDING, 3);

        stringstream ss;
        ss << "#atlantis " << faction->num << " \"mypassword\"\n";
        ss << "unit " << builder->num << "\n";
        ss << "build \"Town Hall\"\n";
        helper.parse_orders(faction->num, ss, nullptr);

        expect(count_town_halls(r) == 1_ul) << "the second Town Hall must not be created";
        expect(faction->errors.size() >= 1_ul) << "an error must be reported";
        expect(any_error_contains(faction, "can only exist once")) << "error must mention the one-per-region rule";
    };

    "Cannot build a second Town Hall while another is incomplete"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(faction);
        ARegion *r = leader->object->region;
        Unit *builder = helper.create_unit(faction, r);

        helper.create_building(r, nullptr, O_TOWN_HALL);
        // Mark the existing hall as still under construction.
        for (const auto o : r->objects) {
            if (o->type == O_TOWN_HALL) { o->incomplete = ObjectDefs[O_TOWN_HALL].cost; break; }
        }
        expect(count_town_halls(r) == 1_ul);

        builder->items.SetNum(I_WOOD, 30);
        helper.set_skill_level(builder, S_BUILDING, 3);

        stringstream ss;
        ss << "#atlantis " << faction->num << " \"mypassword\"\n";
        ss << "unit " << builder->num << "\n";
        ss << "build \"Town Hall\"\n";
        helper.parse_orders(faction->num, ss, nullptr);

        expect(count_town_halls(r) == 1_ul) << "incomplete hall still counts as the unique one";
        expect(faction->errors.size() >= 1_ul);
        expect(any_error_contains(faction, "can only exist once"));
    };

    "Town Hall cannot be built in a region without a settlement"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        ARegion *wild = find_wilderness(helper);
        expect(wild != nullptr) << "test world must contain at least one wilderness region";
        if (!wild) return;

        Unit *builder = helper.create_unit(faction, wild);
        builder->items.SetNum(I_WOOD, 30);
        helper.set_skill_level(builder, S_BUILDING, 3);

        size_t before = wild->objects.size();

        stringstream ss;
        ss << "#atlantis " << faction->num << " \"mypassword\"\n";
        ss << "unit " << builder->num << "\n";
        ss << "build \"Town Hall\"\n";
        helper.parse_orders(faction->num, ss, nullptr);

        expect(wild->objects.size() == before) << "no new object must be created in wilderness";
        expect(count_town_halls(wild) == 0_ul);
        expect(faction->errors.size() >= 1_ul);
        expect(any_error_contains(faction, "can only be built in settlements"));
    };

    "Other buildings are not auto-named via the Town Hall name generator"_test = [] {
        // Town Hall has its own word lists. Other buildings (Tower, production
        // buildings, etc.) must not use them — this test guards the type-check
        // routing in AddNewBuildings/AutoNameBuildings.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(faction);
        ARegion *r = leader->object->region;
        leader->items.SetNum(I_STONE, 10);
        helper.set_skill_level(leader, S_BUILDING, 1);

        stringstream ss;
        ss << "#atlantis " << faction->num << " \"mypassword\"\n";
        ss << "unit " << leader->num << "\n";
        ss << "build tower\n";
        helper.parse_orders(faction->num, ss, nullptr);
        helper.run_month_orders();

        // Find the freshly-created tower.
        Object *tower = nullptr;
        for (const auto o : r->objects) if (o->type == O_TOWER) { tower = o; break; }
        expect(tower != nullptr) << "tower must be created";
        if (!tower) return;

        // Tower name must not contain Town Hall structural words ("Council",
        // "Civic", "Borough", "Forum", etc.) — these are reserved for the hall.
        const std::vector<std::string> hall_only = {
            "Council", "Civic", "Borough", "Burgher",
            "Forum",   "Manor",  "Court",
            "Diwan",   "Majlis", "Saray", "Qasr"
        };
        for (const auto& w : hall_only) {
            expect(tower->name.find(w) == string::npos)
                << ("tower name '" + tower->name + "' must not include '" + w + "'");
        }
    };

    "Continuing build of an incomplete Town Hall does not violate one-per-region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(faction);
        ARegion *r = leader->object->region;

        helper.create_building(r, leader, O_TOWN_HALL);
        Object *hall = nullptr;
        for (const auto o : r->objects) if (o->type == O_TOWN_HALL) { hall = o; break; }
        expect(hall != nullptr);
        if (!hall) return;
        hall->incomplete = ObjectDefs[O_TOWN_HALL].cost; // 30, fresh start

        leader->items.SetNum(I_WOOD, 30);
        helper.set_skill_level(leader, S_BUILDING, 3);

        // BUILD without an object name = continue current building (the hall the unit sits in).
        stringstream ss;
        ss << "#atlantis " << faction->num << " \"mypassword\"\n";
        ss << "unit " << leader->num << "\n";
        ss << "build\n";
        helper.parse_orders(faction->num, ss, nullptr);

        expect(faction->errors.size() == 0_ul) << "continuing build must not be blocked by ONE_PER_REGION";
        expect(count_town_halls(r) == 1_ul);
    };
};
