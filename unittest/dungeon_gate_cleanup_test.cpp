#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Same fixture as unittest/dungeon_lifecycle_test.cpp:
//   (0,2) = dungeon room, (1,1) = surface evacuation target.

static DungeonInstance make_dying_instance(ARegion *surf, ARegion *room, int phase_turn) {
    DungeonInstance d;
    d.id = 999;
    d.type = DungeonType::DUNGEON_KOBOLD_WARRENS;
    d.state = DungeonSlotState::DYING;
    d.phase_turn = phase_turn;
    d.surface_region_num = surf->num;
    d.entrance_object_num = -1;
    d.entry_region_num = room->num;
    d.exit_object_num = -1;
    d.boss_region_num = room->num;
    d.room_nums = { room->num };
    return d;
}

ut::suite<"DungeonGateCleanup"> dungeon_gate_cleanup_suite = [] {
    using namespace ut;

    "gate built inside a dungeon room is cleared on collapse"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);
        ARegion *surf = helper.get_region(1, 1, 0);

        // Simulate a completed CGAT inside the dungeon room.
        ARegionList &regions = helper.get_regions();
        regions.numberofgates = 1;
        room->gate = 1;
        room->gateopen = 1;

        int dying_turns = DungeonTypeDefs[(int)DungeonType::DUNGEON_KOBOLD_WARRENS].dying_turns;
        int phase_turn = helper.turn_number() - dying_turns; // exactly expired

        helper.inject_dungeon(make_dying_instance(surf, room, phase_turn));

        helper.run_process_dungeons();

        expect(helper.dungeons_empty()) << "dungeon must have collapsed";
        expect(room->type == R_BARREN) << "sanity check: collapse ran";
        expect(room->gate == 0) << "gate built inside the dungeon must be cleared on collapse";
        expect(regions.numberofgates == 0)
            << "numberofgates must be decremented, not just the region's gate field";
    };
};
