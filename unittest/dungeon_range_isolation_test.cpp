#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Test world layout (surface level 0, 2x4 grid) — same fixture as
// unittest/dungeon_lifecycle_test.cpp:
//   (0,0) = R_PLAIN   (starting city)
//   (0,2) = R_FOREST
//   (1,1) = R_MOUNTAIN
//   (1,3) = R_DESERT
//
// Dungeon A "owns" (0,2) and (1,1). Dungeon B "owns" (1,3). (0,0) belongs to
// no dungeon (stands in for "the surface").

static DungeonInstance make_instance(int id, std::vector<int> rooms) {
    DungeonInstance d;
    d.id = id;
    d.type = DungeonType::DUNGEON_KOBOLD_WARRENS;
    d.state = DungeonSlotState::ACTIVE;
    d.room_nums = std::move(rooms);
    return d;
}

ut::suite<"DungeonRangeIsolation"> dungeon_range_isolation_suite = [] {
    using namespace ut;

    "teleport within the same dungeon instance is allowed"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room_a1 = helper.get_region(0, 2, 0);
        ARegion *room_a2 = helper.get_region(1, 1, 0);

        helper.inject_dungeon(make_instance(1, {room_a1->num, room_a2->num}));

        Faction *faction = helper.create_faction("Adventurers");
        Unit *leader = helper.create_unit(faction, room_a1);
        helper.set_skill_level(leader, S_TELEPORTATION, 5); // distance is not what's under test

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast teleportation REGION " << room_a2->xloc << " " << room_a2->yloc << "\n";
        helper.parse_orders(faction->num, ss);
        helper.activate_spell(S_TELEPORTATION, {
            .region = room_a1, .unit = leader, .object = nullptr, .val1 = 0, .val2 = 0
        });

        expect(leader->object->region == room_a2)
            << "teleport must succeed between two rooms of the SAME dungeon instance";
        expect(faction->errors.size() == 0_ul);
    };

    "teleport into a DIFFERENT active dungeon's room is blocked"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room_a1 = helper.get_region(0, 2, 0);
        ARegion *room_b1 = helper.get_region(1, 3, 0);

        helper.inject_dungeon(make_instance(1, {room_a1->num}));
        helper.inject_dungeon(make_instance(2, {room_b1->num}));

        Faction *faction = helper.create_faction("Adventurers");
        Unit *leader = helper.create_unit(faction, room_a1);
        helper.set_skill_level(leader, S_TELEPORTATION, 5);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast teleportation REGION " << room_b1->xloc << " " << room_b1->yloc << "\n";
        helper.parse_orders(faction->num, ss);
        helper.activate_spell(S_TELEPORTATION, {
            .region = room_a1, .unit = leader, .object = nullptr, .val1 = 0, .val2 = 0
        });

        expect(leader->object->region == room_a1)
            << "teleport must be BLOCKED between two DIFFERENT active dungeon instances";
        expect(faction->errors.size() == 1_ul) << "a CAST error must be recorded";
    };

    "teleport from a dungeon room to the surface is unaffected"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room_a1  = helper.get_region(0, 2, 0);
        ARegion *surface  = helper.get_region(0, 0, 0); // not in any dungeon's room_nums

        helper.inject_dungeon(make_instance(1, {room_a1->num}));

        Faction *faction = helper.create_faction("Adventurers");
        Unit *leader = helper.create_unit(faction, room_a1);
        helper.set_skill_level(leader, S_TELEPORTATION, 5);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast teleportation REGION " << surface->xloc << " " << surface->yloc << "\n";
        helper.parse_orders(faction->num, ss);
        helper.activate_spell(S_TELEPORTATION, {
            .region = room_a1, .unit = leader, .object = nullptr, .val1 = 0, .val2 = 0
        });

        expect(leader->object->region == surface)
            << "dungeon -> surface teleport must remain legal (not touched by this fix)";
        expect(faction->errors.size() == 0_ul);
    };
};
