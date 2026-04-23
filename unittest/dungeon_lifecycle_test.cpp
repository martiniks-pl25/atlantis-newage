#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"
#include <algorithm>

namespace ut = boost::ut;

// Test world layout (surface level 0, 2×4 grid):
//   (0,0) = R_PLAIN  (has starting city — avoid as dungeon room)
//   (0,2) = R_FOREST
//   (1,1) = R_MOUNTAIN
//   (1,3) = R_DESERT
//
// In dungeon tests: (0,2) is used as the dungeon room, (1,1) as the surface
// entrance/evacuation region. They are distinct regions so evacuation can be verified.

static Object *add_entrance_object(ARegion *surf, int obj_num) {
    Object *o = new Object(surf);
    o->num = obj_num;
    o->type = O_DUNGEON_ENTRANCE;
    o->set_name("Dark Burrow");
    o->incomplete = 0;
    surf->objects.push_back(o);
    return o;
}

static DungeonInstance make_dungeon_instance(
    DungeonSlotState state, int phase_turn,
    ARegion *surf, ARegion *room, int entrance_object_num)
{
    DungeonInstance d;
    d.id = 999;
    d.type = DungeonType::DUNGEON_KOBOLD_WARRENS;
    d.state = state;
    d.phase_turn = phase_turn;
    d.surface_region_num = surf->num;
    d.entrance_object_num = entrance_object_num;
    d.entry_region_num = room->num;
    d.exit_object_num = -1;
    d.boss_region_num = room->num;
    d.room_nums = { room->num };
    return d;
}

ut::suite<"DungeonLifecycle"> dungeon_lifecycle_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: ACTIVE dungeon with boss alive → state stays ACTIVE
    // -----------------------------------------------------------------------
    "boss alive keeps dungeon ACTIVE"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);
        ARegion *surf = helper.get_region(1, 1, 0);
        add_entrance_object(surf, 42);

        Faction *mon = helper.get_faction(helper.get_monfaction());
        Unit *boss = helper.create_unit(mon, room);
        boss->items.SetNum(I_ETTIN, 3);  // KOBOLD_WARRENS boss_kill_item = I_ETTIN

        helper.inject_dungeon(
            make_dungeon_instance(DungeonSlotState::ACTIVE, 0, surf, room, 42));

        helper.run_process_dungeons();

        expect(helper.dungeon_count() == 1_ul);
        expect(helper.get_dungeon(0).state == DungeonSlotState::ACTIVE)
            << "state must remain ACTIVE while boss is alive";
    };

    // -----------------------------------------------------------------------
    // Test 2: ACTIVE dungeon, no boss item → ACTIVE → DYING, entrance removed
    // -----------------------------------------------------------------------
    "boss killed transitions dungeon to DYING"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);
        ARegion *surf = helper.get_region(1, 1, 0);
        add_entrance_object(surf, 42);

        // No I_ETTIN in any unit — boss considered dead
        Faction *mon = helper.get_faction(helper.get_monfaction());
        Unit *wanderer = helper.create_unit(mon, room);
        wanderer->items.SetNum(I_KOBOLD, 10);  // non-kill item only

        helper.inject_dungeon(
            make_dungeon_instance(DungeonSlotState::ACTIVE, 0, surf, room, 42));

        helper.run_process_dungeons();

        expect(helper.dungeon_count() == 1_ul) << "dungeon must still exist as DYING";
        expect(helper.get_dungeon(0).state == DungeonSlotState::DYING)
            << "state must become DYING after boss death";
        expect(helper.get_dungeon(0).entrance_object_num == -1)
            << "entrance_object_num must be cleared";

        bool entrance_found = false;
        for (auto *obj : surf->objects)
            if (obj->num == 42) { entrance_found = true; break; }
        expect(!entrance_found) << "entrance object must be removed from surface region";
    };

    // -----------------------------------------------------------------------
    // Test 3: DYING with expired timer → dungeon removed from activeDungeons
    // -----------------------------------------------------------------------
    "DYING timer expired removes dungeon"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);
        ARegion *surf = helper.get_region(1, 1, 0);

        // TurnNumber() == 1 after setup_turn(); dying_turns = 6 for KOBOLD_WARRENS
        int dying_turns = DungeonTypeDefs[(int)DungeonType::DUNGEON_KOBOLD_WARRENS].dying_turns;
        int phase_turn = helper.turn_number() - dying_turns;  // exactly expired

        // DYING state: entrance already removed
        helper.inject_dungeon(
            make_dungeon_instance(DungeonSlotState::DYING, phase_turn, surf, room, -1));

        helper.run_process_dungeons();

        expect(helper.dungeons_empty()) << "dungeon must be removed after collapse";
    };

    // -----------------------------------------------------------------------
    // Test 4: DYING timer expired with player inside → evacuated, room reset
    // -----------------------------------------------------------------------
    "COLLAPSING evacuates player and resets room to BARREN"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);  // dungeon room
        ARegion *surf = helper.get_region(1, 1, 0);  // evacuation target

        Faction *player = helper.create_faction("Adventurers");
        Unit *player_unit = helper.create_unit(player, room);

        int dying_turns = DungeonTypeDefs[(int)DungeonType::DUNGEON_KOBOLD_WARRENS].dying_turns;
        int phase_turn = helper.turn_number() - dying_turns;

        helper.inject_dungeon(
            make_dungeon_instance(DungeonSlotState::DYING, phase_turn, surf, room, -1));

        helper.run_process_dungeons();

        expect(player_unit->object != nullptr) << "player unit must have an object after evacuation";
        expect(player_unit->object->region == surf)
            << "player unit must be in surface region after evacuation";
        expect(room->type == R_BARREN) << "dungeon room must revert to R_BARREN after collapse";
        expect(helper.dungeons_empty()) << "dungeon must be removed after collapse";
    };
};
