#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// These tests use the same 2x4 surface-level fixture as
// unittest/dungeon_lifecycle_test.cpp, treating one region as a stand-in for
// a dungeon room (via an injected DungeonInstance, exactly as the other
// dungeon test files do). The unit-test world has no real LEVEL_DUNGEON
// array (see unittest/world.cpp), so what's under test here is the
// mechanism itself — "route through the entrance link, not raw coordinates"
// — using Game::find_dungeon_instance_for_region() to decide when to use
// connected distance, not ARegionArray::levelType.

static DungeonInstance make_instance(int id, std::vector<int> rooms) {
    DungeonInstance d;
    d.id = id;
    d.type = DungeonType::DUNGEON_KOBOLD_WARRENS;
    d.state = DungeonSlotState::ACTIVE;
    d.room_nums = std::move(rooms);
    return d;
}

// Mirrors dungeon.cpp's spawn wiring: entrance on the surface with
// inner -> room, and an Exit inside the room with inner -> surface.
static void wire_entrance(ARegion *surf, ARegion *room, int ent_num, int exit_num) {
    Object *ent = new Object(surf);
    ent->num = ent_num;
    ent->type = O_DUNGEON_ENTRANCE;
    ent->incomplete = 0;
    ent->inner = room->num;
    surf->objects.push_back(ent);

    Object *ex = new Object(room);
    ex->num = exit_num;
    ex->type = O_DUNGEON_ENTRANCE;
    ex->incomplete = 0;
    ex->inner = surf->num;
    room->objects.push_back(ex);
}

// Isolates a region from the ordinary hex grid, exactly like a real dungeon
// room (GenerateDungeonLayout nulls every neighbor, then selectively reopens
// only the carved passages). Here nothing is reopened: the ONLY way in/out
// is whatever inner-linked object is wired separately.
static void isolate(ARegion *r) {
    for (int d = 0; d < NDIRS; d++) r->neighbors[d] = nullptr;
}

// Overwrites a region's own coordinate fields to something absurdly far from
// anywhere else on this tiny test map. GetPlanarDistance() reads these fields
// directly (never re-deriving position from the array), while
// get_connected_distance() routes purely through neighbors[]/inner pointers
// and only uses coordinates as an opaque visited-set key — so this corrupts
// the planar metric without affecting graph-based lookup or routing.
// ARegionList::GetRegion(x,y,z) resolves orders by their ORIGINAL array
// position, not by these fields, so a CAST order already composed with the
// original x/y before this call still finds the right region.
static void fake_far_away(ARegion *r) {
    r->xloc = 1000;
    r->yloc = 1000;
}

ut::suite<"DungeonConnectedDistance"> dungeon_connected_distance_suite = [] {
    using namespace ut;

    "explicit Z matching a real deep level is no longer rejected at parse time"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegionList &regions = helper.get_regions();
        // Pretend the world has 5 levels (0..4), as Trident does with the
        // dungeon level at index 4 — without needing a real level 4 array,
        // since parsing never touches GetRegion(). expand_levels() resizes
        // pRegionArrays safely; the new slot must be nulled explicitly
        // (expand_levels leaves it uninitialized) so ~ARegionList()'s
        // unconditional `delete pRegionArrays[i]` doesn't crash on a wild
        // pointer when this helper is torn down at the end of the test.
        regions.expand_levels(5);
        regions.pRegionArrays[4] = nullptr;

        Faction *faction = helper.create_faction("Test Faction");
        Unit *leader = helper.get_first_unit(faction);
        helper.set_skill_level(leader, S_FARSIGHT, 5);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast farsight REGION 0 0 4\n";
        helper.parse_orders(faction->num, ss);

        bool has_invalid_z = false;
        for (auto &e : faction->errors)
            if (e.message.find("Invalid Z coordinate") != std::string::npos) has_invalid_z = true;
        expect(!has_invalid_z)
            << "z == numLevels-1 (a real, valid deepest level) must parse, not be rejected";
    };

    "teleport into an isolated dungeon room routes through its entrance"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        int room_x = room->xloc, room_y = room->yloc;  // capture before corrupting
        isolate(room);
        fake_far_away(room);
        wire_entrance(surf, room, 42, 43);

        helper.inject_dungeon(make_instance(1, {room->num}));

        Faction *faction = helper.create_faction("Adventurers");
        Unit *leader = helper.create_unit(faction, surf);
        helper.set_skill_level(leader, S_TELEPORTATION, 1);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast teleportation REGION " << room_x << " " << room_y << "\n";
        helper.parse_orders(faction->num, ss);
        helper.activate_spell(S_TELEPORTATION, {
            .region = surf, .unit = leader, .object = nullptr, .val1 = 0, .val2 = 0
        });

        expect(leader->object->region == room)
            << "teleport must succeed via the entrance link even though the room "
               "has no ordinary neighbors and its coordinates are nowhere near "
               "the caster — distance must route through inner, not raw coordinates";
        expect(faction->errors.size() == 0_ul);
    };

    "teleport into an isolated dungeon room fails once the entrance is gone"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        int room_x = room->xloc, room_y = room->yloc;
        isolate(room);
        fake_far_away(room);
        // No entrance wired: room has no neighbors and no inner link at all.

        helper.inject_dungeon(make_instance(1, {room->num}));

        Faction *faction = helper.create_faction("Adventurers");
        Unit *leader = helper.create_unit(faction, surf);
        helper.set_skill_level(leader, S_TELEPORTATION, 5);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast teleportation REGION " << room_x << " " << room_y << "\n";
        helper.parse_orders(faction->num, ss);
        helper.activate_spell(S_TELEPORTATION, {
            .region = surf, .unit = leader, .object = nullptr, .val1 = 0, .val2 = 0
        });

        expect(leader->object->region == surf)
            << "with no path at all, the cast must fail as out of range";
        expect(faction->errors.size() == 1_ul);
    };

    "teleport out of an isolated dungeon room back through its entrance"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *surf = helper.get_region(1, 1, 0);
        ARegion *room = helper.get_region(0, 2, 0);
        int surf_x = surf->xloc, surf_y = surf->yloc;
        isolate(room);
        fake_far_away(room);
        wire_entrance(surf, room, 42, 43);

        helper.inject_dungeon(make_instance(1, {room->num}));

        Faction *faction = helper.create_faction("Adventurers");
        Unit *leader = helper.create_unit(faction, room);
        helper.set_skill_level(leader, S_TELEPORTATION, 1);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << leader->num << "\n";
        ss << "cast teleportation REGION " << surf_x << " " << surf_y << "\n";
        helper.parse_orders(faction->num, ss);
        helper.activate_spell(S_TELEPORTATION, {
            .region = room, .unit = leader, .object = nullptr, .val1 = 0, .val2 = 0
        });

        expect(leader->object->region == surf)
            << "the entrance link must work in both directions, even though room's "
               "(faked) coordinates make the return trip look impossibly far by raw distance";
        expect(faction->errors.size() == 0_ul);
    };
};
