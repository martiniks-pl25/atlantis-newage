#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Dungeon identity in the JSON report (format 1.2.0).
//
// Same fixture as dungeon_lifecycle_test.cpp: the unit-test world has no real
// LEVEL_DUNGEON array, so ordinary surface regions stand in for a dungeon room
// and its surface entrance. Injecting a DungeonInstance makes the report treat
// them as parts of one dungeon. The report itself only reads the index, never
// the terrain, so a stand-in room is enough.

static DungeonInstance make_instance(DungeonSlotState state, ARegion *surf, ARegion *room)
{
    DungeonInstance d;
    d.id = 7;
    d.type = DungeonType::DUNGEON_SKELETON_RUINS;
    d.state = state;
    d.surface_region_num = surf->num;
    d.entrance_object_num = 42;
    d.entry_region_num = room->num;
    d.exit_object_num = 43;
    d.room_nums = { room->num };
    return d;
}

static Object *add_object(ARegion *r, int num)
{
    Object *o = new Object(r);
    o->num = num;
    o->type = O_DUNGEON_ENTRANCE;
    o->incomplete = 0;
    o->set_name("Dark Burrow");
    r->objects.push_back(o);
    return o;
}

// Finds a structure entry by its per-region number, or nullptr.
static const json *find_structure(const json &region, int number)
{
    if (!region.contains("structures") || !region["structures"].is_array()) return nullptr;
    for (const auto &s : region["structures"])
        if (s["number"] == number) return &s;
    return nullptr;
}

ut::suite<"DungeonJsonReport"> dungeon_json_report_suite = [] {
    using namespace ut;

    "a room of an open dungeon names the dungeon"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.setup_reports();

        Faction *fac = helper.create_faction("Adventurers");
        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        helper.create_unit(fac, room);  // the faction can see the room

        helper.inject_dungeon(make_instance(DungeonSlotState::ACTIVE, surf, room));
        DungeonReportIndex dungeons(helper.game_object().activeDungeons);

        json j;
        room->build_json_report(j, fac, helper.get_month(), helper.get_regions(), &dungeons);

        expect(j.contains("dungeon")) << "a live dungeon room carries its identity";
        expect(j["dungeon"]["id"] == 7_i);
        expect(j["dungeon"]["type"] == std::string("Skeleton Ruins"));
        expect(j["dungeon"]["state"] == std::string("open"));
    };

    "a closing dungeon reads as closing"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.setup_reports();

        Faction *fac = helper.create_faction("Adventurers");
        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        helper.create_unit(fac, room);

        helper.inject_dungeon(make_instance(DungeonSlotState::DYING, surf, room));
        DungeonReportIndex dungeons(helper.game_object().activeDungeons);

        json j;
        room->build_json_report(j, fac, helper.get_month(), helper.get_regions(), &dungeons);

        expect(j["dungeon"]["id"] == 7_i);
        expect(j["dungeon"]["state"] == std::string("closing"))
            << "a DYING dungeon must read closing, not open";
    };

    "a region outside every dungeon has no dungeon field"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.setup_reports();

        Faction *fac = helper.create_faction("Adventurers");
        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        ARegion *other = helper.get_region(0, 0, 0);
        helper.create_unit(fac, other);

        helper.inject_dungeon(make_instance(DungeonSlotState::ACTIVE, surf, room));
        DungeonReportIndex dungeons(helper.game_object().activeDungeons);

        json j;
        other->build_json_report(j, fac, helper.get_month(), helper.get_regions(), &dungeons);

        expect(j.contains("dungeon") == false)
            << "only rooms of a live dungeon get the field";
    };

    "a recycled cell (COLLAPSING or FREE) stays nameless"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.setup_reports();

        Faction *fac = helper.create_faction("Adventurers");
        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        helper.create_unit(fac, room);

        for (DungeonSlotState state : { DungeonSlotState::COLLAPSING, DungeonSlotState::FREE }) {
            helper.game_object().activeDungeons.clear();
            helper.inject_dungeon(make_instance(state, surf, room));
            DungeonReportIndex dungeons(helper.game_object().activeDungeons);

            json j;
            room->build_json_report(j, fac, helper.get_month(), helper.get_regions(), &dungeons);
            expect(j.contains("dungeon") == false)
                << "a collapsed FREE/COLLAPSING slot must not name a dungeon";
        }
    };

    "the entrance and exit objects carry the dungeon id"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.setup_reports();

        Faction *fac = helper.create_faction("Adventurers");
        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        add_object(surf, 42);  // surface entrance
        add_object(room, 43);  // inner exit (entry room)
        helper.create_unit(fac, surf);
        helper.create_unit(fac, room);

        helper.inject_dungeon(make_instance(DungeonSlotState::ACTIVE, surf, room));
        DungeonReportIndex dungeons(helper.game_object().activeDungeons);

        json surf_report;
        surf->build_json_report(surf_report, fac, helper.get_month(), helper.get_regions(), &dungeons);
        const json *entrance = find_structure(surf_report, 42);
        expect(entrance != nullptr) << "the surface entrance must be reported";
        if (entrance) {
            expect((*entrance)["dungeon"]["id"] == 7_i);
            expect((*entrance)["dungeon"]["type"] == std::string("Skeleton Ruins"));
        }
        expect(surf_report.contains("dungeon") == false)
            << "the surface region is not a room, so the region body gets no dungeon";

        json room_report;
        room->build_json_report(room_report, fac, helper.get_month(), helper.get_regions(), &dungeons);
        const json *exit = find_structure(room_report, 43);
        expect(exit != nullptr) << "the inner exit must be reported";
        if (exit) expect((*exit)["dungeon"]["id"] == 7_i);
    };

    "the faction report plumbs the index into its regions"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Adventurers");
        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        helper.create_unit(fac, room);
        helper.setup_reports();

        helper.inject_dungeon(make_instance(DungeonSlotState::ACTIVE, surf, room));

        json report;
        fac->build_json_report(report, &helper.game_object(), nullptr);

        const json *room_region = nullptr;
        for (const auto &r : report["regions"])
            if (r["coordinates"]["x"] == 0 && r["coordinates"]["y"] == 2)
                room_region = &r;
        expect(room_region != nullptr) << "the room must appear in the faction report";
        if (room_region) {
            expect((*room_region).contains("dungeon"))
                << "Faction::build_json_report must pass the dungeon index through";
            expect((*room_region)["dungeon"]["id"] == 7_i);
            expect((*room_region)["dungeon"]["state"] == std::string("open"));
        }
    };
};
