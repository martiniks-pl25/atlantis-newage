#include "dungeon.h"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "events.h"
#include "logger.hpp"
#include "rng.hpp"

#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <queue>
#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// Dungeon type table — add new themes here.
// ---------------------------------------------------------------------------
const std::vector<DungeonTypeDef> DungeonTypeDefs = {
    {   // DUNGEON_KOBOLD_WARRENS — easy, open burrow network; dominant early
        "Kobold Warrens", "Dark Burrow",
        6, 10,
        { {I_KOBOLD,10,50}, {I_TROLL,3,10} }, "Kobolds",
        { {I_KOBOLD,30,80}, {I_TROLL,5,15}, {I_ETTIN,2,5} }, "Warden",
        I_ETTIN,
        DungeonGenStyle::BFS_NETWORK, 50,
        75, 25,  // turn 1: 75%;  turn 50+: 25%
        8,       // dying_turns (+2): BFS/small — 5 rooms max dist, 3 moves + margin
        12       // max_lifetime_turns: collapses after 12 turns even if boss alive
    },
    {   // DUNGEON_SKELETON_RUINS — winding crypt corridors
        "Skeleton Ruins", "Ancient Ruins",
        8, 12,
        { {I_SKELETON,10,50}, {I_UNDEAD,2,8} }, "Undead",
        { {I_SKELETON,80,200}, {I_UNDEAD,10,30}, {I_LICH,2,5} }, "Warden",
        I_LICH,
        DungeonGenStyle::DFS_CORRIDOR, 20,
        25, 25,  // turn 1: 25%;  turn 50+: 25%
        12,      // dying_turns (+2): DFS/medium — up to 10 rooms deep
        12       // max_lifetime_turns
    },
    {   // DUNGEON_DEMON_PIT — open pit network
        "Demon Pit", "Burning Gate",
        8, 12,
        { {I_IMP,10,50}, {I_DEMON,2,8} }, "Demons",
        { {I_IMP,50,150}, {I_DEMON,10,25}, {I_DEVIL,1,1} }, "Warden",
        I_DEVIL,
        DungeonGenStyle::BFS_NETWORK, 50,
        0, 25,   // turn 1: 0%;   turn 50+: 25%
        10,      // dying_turns (+2): BFS/medium — open network, shorter paths
        12       // max_lifetime_turns
    },
    {   // DUNGEON_DRAGON_LAIR — meandering lair with side tunnels
        "Dragon Lair", "Dragon's Maw",
        10, 16,
        { {I_LIZARD,2,8}, {I_WYVERN,1,2} }, "Dragonkin",
        { {I_LIZARD,10,20}, {I_WYVERN,3,7}, {I_DRAGON,1,2} }, "Warden",
        I_DRAGON,
        DungeonGenStyle::DFS_CORRIDOR, 30,
        0, 25,   // turn 1: 0%;   turn 50+: 25%
        12,      // dying_turns (+2): DFS/large — up to 14 rooms deep
        12       // max_lifetime_turns
    },
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Hex-coordinate offsets for 6 directions in Atlantis offset-coordinate system.
// Valid hexes satisfy (x+y)%2==0; neighbors of (x,y) are at these offsets.
static const int HEX_DX[6] = { 0,  1,  1,  0, -1, -1 };
static const int HEX_DY[6] = {-2, -1,  1,  2,  1, -1 };

// Returns all real hex neighbors of (rx,ry) together with the direction index (0..5).
// Uses coordinate arithmetic — bypasses neighbors[] so it works after prior dungeon collapse.
static std::vector<std::pair<ARegion*,int>> coord_neighbors_with_dir(ARegionArray *da, int rx, int ry)
{
    std::vector<std::pair<ARegion*,int>> result;
    for (int dir = 0; dir < 6; dir++) {
        ARegion *nb = da->GetRegion(rx + HEX_DX[dir], ry + HEX_DY[dir]);
        if (nb) result.emplace_back(nb, dir);
    }
    return result;
}


// ---------------------------------------------------------------------------
// Game::find_entrance_spot
// Returns a surface region suitable for a new dungeon entrance, or nullptr.
// ---------------------------------------------------------------------------
ARegion* Game::find_entrance_spot()
{
    ARegionArray *surface =
        regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
    if (!surface) return nullptr;

    for (int attempt = 0; attempt < dungeon::PLACEMENT_ATTEMPTS; attempt++) {
        int x = rng::get_random(surface->x);
        int y = rng::get_random(surface->y);
        ARegion *r = surface->GetRegion(x, y);
        if (!r) continue;
        if (r->type == R_OCEAN || r->type == R_LAKE) continue;
        if (r->town) continue;

        bool too_close = false;
        for (const auto &d : activeDungeons) {
            if (d.entrance_object_num < 0) continue;  // DYING — entrance gone
            ARegion *existing = regions.GetRegion(d.surface_region_num);
            if (!existing) continue;
            if (regions.find_distance_between_regions(r, existing) < dungeon::MIN_DISTANCE) {
                too_close = true;
                break;
            }
        }
        if (!too_close) return r;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Game::generate_dungeon_cell
// Activates R_DUNGEON rooms inside the 8×8 cell via DFS or BFS, tracks which
// passages were traversed, then seals ALL hex walls and re-opens only those
// passages.  This gives true wall semantics: adjacent rooms without a passage
// cannot be moved between.
// Fills d.entry_region_num, d.boss_region_num, d.room_nums.
// ---------------------------------------------------------------------------
void Game::generate_dungeon_cell(DungeonInstance &d)
{
    ARegionArray *da =
        regions.get_first_region_array_of_type(ARegionArray::LEVEL_DUNGEON);

    const int CS   = dungeon::CELL_SIZE;
    const int bx   = d.cell_x * CS;
    const int by   = d.cell_y * CS;
    const auto &td = DungeonTypeDefs[(int)d.type];
    int target     = td.rooms_min + rng::get_random(td.rooms_max - td.rooms_min + 1);

    // Collect all real hexes in this cell.
    std::vector<ARegion*> cell_hexes;
    std::unordered_set<int> cell_set;
    for (int dy = 0; dy < CS; dy++)
        for (int dx = 0; dx < CS; dx++) {
            ARegion *r = da->GetRegion(bx + dx, by + dy);
            if (r) { cell_hexes.push_back(r); cell_set.insert(r->num); }
        }
    if (cell_hexes.empty()) return;

    // Pick ENTRY from edge hexes (border of the cell).
    std::vector<ARegion*> edge_hexes;
    for (auto *r : cell_hexes) {
        int rx = r->xloc - bx, ry = r->yloc - by;
        if (rx == 0 || rx == CS-1 || ry == 0 || ry == CS-1)
            edge_hexes.push_back(r);
    }
    ARegion *entry = edge_hexes[rng::get_random((int)edge_hexes.size())];

    // ---------------------------------------------------------------------------
    // Frontier-based room selection with passage tracking.
    // DFS (pop_back) → narrow corridors with dead ends.
    // BFS (pop_front) → star-shaped open network.
    // ---------------------------------------------------------------------------
    bool use_dfs = (td.gen_style == DungeonGenStyle::DFS_CORRIDOR);

    std::vector<ARegion*> active;
    std::unordered_set<int> visited;
    std::deque<ARegion*> frontier;
    // Each passage: (from_num, to_num, dir) where dir is the direction FROM→TO.
    std::vector<std::tuple<int,int,int>> passages;

    // Seed with entry (no incoming passage).
    visited.insert(entry->num);
    active.push_back(entry);
    frontier.push_back(entry);

    while (!frontier.empty() && (int)active.size() < target) {
        ARegion *cur = use_dfs ? frontier.back() : frontier.front();
        if (use_dfs) frontier.pop_back(); else frontier.pop_front();

        auto nbs = coord_neighbors_with_dir(da, cur->xloc, cur->yloc);
        // Keep only in-cell, unvisited candidates.
        std::vector<std::pair<ARegion*,int>> valid;
        for (auto &[nb, dir] : nbs)
            if (cell_set.count(nb->num) && !visited.count(nb->num))
                valid.emplace_back(nb, dir);

        // Shuffle for layout variety.
        for (int i = (int)valid.size()-1; i > 0; i--) {
            int j = rng::get_random(i+1);
            std::swap(valid[i], valid[j]);
        }

        // Always carve the first neighbor (ensures connectivity).
        if (!valid.empty() && (int)active.size() < target) {
            auto [nb, dir] = valid[0];
            visited.insert(nb->num);
            active.push_back(nb);
            frontier.push_back(nb);
            passages.emplace_back(cur->num, nb->num, dir);
        }

        // Each additional candidate gets a branch_chance% shot.
        for (size_t k = 1; k < valid.size() && (int)active.size() < target; k++) {
            if (rng::get_random(100) < td.branch_chance) {
                auto [nb, dir] = valid[k];
                visited.insert(nb->num);
                active.push_back(nb);
                frontier.push_back(nb);
                passages.emplace_back(cur->num, nb->num, dir);
            }
        }
    }

    // ---------------------------------------------------------------------------
    // BOSS room = farthest from ENTRY by BFS through passage graph.
    // Run BFS using passage adjacency so distance reflects actual corridor length.
    // ---------------------------------------------------------------------------
    std::unordered_set<int> active_set;
    for (auto *r : active) active_set.insert(r->num);

    // Build adjacency from passages (bidirectional).
    std::unordered_map<int, std::vector<int>> adj;
    for (auto &[fn, tn, dir] : passages) {
        adj[fn].push_back(tn);
        adj[tn].push_back(fn);
    }

    std::unordered_map<int,int> dist;
    std::queue<int> bfsq;
    dist[entry->num] = 0;
    bfsq.push(entry->num);
    ARegion *boss_room = entry;
    int max_dist = 0;
    while (!bfsq.empty()) {
        int cur_num = bfsq.front(); bfsq.pop();
        for (int nb_num : adj[cur_num]) {
            if (!dist.count(nb_num)) {
                dist[nb_num] = dist[cur_num] + 1;
                if (dist[nb_num] > max_dist) {
                    max_dist = dist[nb_num];
                    boss_room = regions.GetRegion(nb_num);
                }
                bfsq.push(nb_num);
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Activate rooms and apply wall sealing.
    // 1. Set terrain to R_DUNGEON for all active rooms.
    // 2. Null ALL neighbors[] for every active room (seal all walls).
    // 3. Re-open only the passage directions recorded during generation.
    // Nulled pointers persist through WriteRegions/ReadRegions.
    // ---------------------------------------------------------------------------
    for (auto *r : active) {
        r->type = R_DUNGEON;
        r->set_name(td.name);
    }

    for (auto *r : active)
        for (int dir = 0; dir < NDIRS; dir++)
            r->neighbors[dir] = nullptr;

    for (auto &[from_num, to_num, dir] : passages) {
        ARegion *a = regions.GetRegion(from_num);
        ARegion *b = regions.GetRegion(to_num);
        if (a && b) {
            a->neighbors[dir]          = b;
            b->neighbors[(dir+3) % NDIRS] = a;
        }
    }

    d.entry_region_num = entry->num;
    d.boss_region_num  = (boss_room ? boss_room->num : entry->num);
    d.room_nums.clear();
    for (auto *r : active) d.room_nums.push_back(r->num);
}

// ---------------------------------------------------------------------------
// Game::populate_dungeon
// Spawns wandering mobs in corridor rooms and the boss unit in the boss room.
// ---------------------------------------------------------------------------
void Game::populate_dungeon(const DungeonInstance &d)
{
    const auto &td = DungeonTypeDefs[(int)d.type];
    Faction *mfac = GetFaction(factions, monfaction);

    for (int rnum : d.room_nums) {
        ARegion *r = regions.GetRegion(rnum);
        if (!r) continue;

        if (rnum == d.boss_region_num) {
            // Boss unit: all boss_mobs entries always included.
            if (td.boss_mobs.empty()) continue;
            const auto &bm0 = td.boss_mobs[0];
            Unit *boss = GetNewUnit(mfac, 0);
            boss->MakeWMon(td.boss_unit_name, bm0.item,
                           bm0.min + rng::get_random(bm0.max - bm0.min + 1));
            for (size_t i = 1; i < td.boss_mobs.size(); i++) {
                const auto &bm = td.boss_mobs[i];
                boss->items.SetNum(bm.item, bm.min + rng::get_random(bm.max - bm.min + 1));
            }
            boss->guard = GUARD_NONE;
            boss->MoveUnit(r->GetDummy());
            boss->free = 0;  // Elder: full loot immediately
            boss->UpdateMonsterDescription();

        } else {
            // Wander unit (corridor or entry room).
            // Entry room: primary mob only, half count — light guard.
            // Corridor rooms: primary always, secondaries at 50% each.
            if (td.wander_mobs.empty()) continue;
            const auto &wm0 = td.wander_mobs[0];
            Unit *mob = GetNewUnit(mfac, 0);
            if (rnum == d.entry_region_num) {
                int count = std::max(1, (wm0.min + rng::get_random(wm0.max - wm0.min + 1)) / 2);
                mob->MakeWMon(td.wander_unit_name, wm0.item, count);
            } else {
                mob->MakeWMon(td.wander_unit_name, wm0.item,
                              wm0.min + rng::get_random(wm0.max - wm0.min + 1));
                for (size_t i = 1; i < td.wander_mobs.size(); i++) {
                    if (rng::get_random(2)) {
                        const auto &wm = td.wander_mobs[i];
                        mob->items.SetNum(wm.item, wm.min + rng::get_random(wm.max - wm.min + 1));
                    }
                }
            }
            mob->guard = GUARD_NONE;
            mob->MoveUnit(r->GetDummy());
            mob->free = 0;  // Elder: full loot immediately
            mob->UpdateMonsterDescription();
        }
    }
}

// ---------------------------------------------------------------------------
// Game::try_spawn_dungeon
// One spawn attempt: find surface spot + free cell, generate, populate.
// ---------------------------------------------------------------------------
void Game::try_spawn_dungeon()
{
    ARegionArray *da =
        regions.get_first_region_array_of_type(ARegionArray::LEVEL_DUNGEON);
    if (!da) return;

    int grid_x = da->x / dungeon::CELL_SIZE;
    int grid_y = da->y / dungeon::CELL_SIZE;

    // Build occupied cell set.
    std::unordered_set<int> occupied;
    for (const auto &d : activeDungeons)
        occupied.insert(d.cell_x * 1000 + d.cell_y);

    // Collect free cells.
    std::vector<std::pair<int,int>> free_cells;
    for (int cx = 0; cx < grid_x; cx++)
        for (int cy = 0; cy < grid_y; cy++)
            if (!occupied.count(cx * 1000 + cy))
                free_cells.push_back({cx, cy});

    if (free_cells.empty()) return;

    ARegion *surface_r = find_entrance_spot();
    if (!surface_r) return;

    auto [cx, cy] = free_cells[rng::get_random((int)free_cells.size())];

    DungeonInstance d;
    d.id         = nextDungeonId++;

    // Turn-based weighted type selection: interpolate weight_early→weight_late over 40 turns.
    {
        float p = std::min(TurnNumber(), 40) / 40.0f;
        std::vector<int> weights;
        int total_weight = 0;
        for (const auto &td : DungeonTypeDefs) {
            int w = std::max(0, (int)(td.weight_early + (td.weight_late - td.weight_early) * p));
            weights.push_back(w);
            total_weight += w;
        }
        d.type = (DungeonType)0;
        if (total_weight > 0) {
            int roll = rng::get_random(total_weight);
            int acc = 0;
            for (int i = 0; i < (int)weights.size(); i++) {
                acc += weights[i];
                if (roll < acc) { d.type = (DungeonType)i; break; }
            }
        }
    }
    d.state      = DungeonSlotState::ACTIVE;
    d.phase_turn = 0;
    d.spawn_turn = TurnNumber();
    d.cell_x     = cx;
    d.cell_y     = cy;
    d.surface_region_num = surface_r->num;

    generate_dungeon_cell(d);
    if (d.room_nums.empty()) return;  // generation failed (shouldn't happen)

    // TODO: When restarting the server, increase building slot range to 1-199 and
    //       start shipseq at 200 (game.cpp InitMinimal/NewGame: shipseq=200, and
    //       change the BUILD order scan limit from 100 to 200 in monthorders.cpp).
    //       Currently buildings occupy 1-99 and fleets start at 100 (shipseq).

    // Place entrance on surface.
    // Use the same slot-scan as the BUILD order so the entrance gets a number in
    // the building range (1-99) rather than using buildingseq, which is inflated
    // by fleet object numbers during Readin.
    {
        const auto &td = DungeonTypeDefs[(int)d.type];
        int ent_num = 1;
        for (; ent_num < FLEET_NUM_START; ent_num++)
            if (!surface_r->GetObject(ent_num)) break;
        if (ent_num >= FLEET_NUM_START) return;  // no free building slot — skip this spawn

        Object *ent = new Object(surface_r);
        ent->num       = ent_num;
        ent->type      = O_DUNGEON_ENTRANCE;
        ent->set_name(std::string(td.entrance_name) + " of " + surface_r->name);
        ent->incomplete = 0;
        ent->inner      = d.entry_region_num;
        surface_r->objects.push_back(ent);
        d.entrance_object_num = ent->num;
    }

    // Place exit in entry room (dungeon level has no fleets, but use same pattern).
    {
        ARegion *entry_r = regions.GetRegion(d.entry_region_num);
        if (entry_r) {
            int ex_num = 1;
            for (; ex_num < FLEET_NUM_START; ex_num++)
                if (!entry_r->GetObject(ex_num)) break;
            if (ex_num < FLEET_NUM_START) {
                Object *ex = new Object(entry_r);
                ex->num       = ex_num;
                ex->type      = O_DUNGEON_ENTRANCE;
                ex->set_name("Exit");
                ex->incomplete = 0;
                ex->inner      = surface_r->num;
                entry_r->objects.push_back(ex);
                d.exit_object_num = ex->num;
            }
        }
    }

    populate_dungeon(d);
    activeDungeons.push_back(d);

    {
        auto *f = new DungeonFact();
        f->event_type       = DungeonEventType::SPAWN;
        f->dungeon_type_name = DungeonTypeDefs[(int)d.type].name;
        f->region_name      = surface_r->name;
        this->events->AddFact(f);
    }
    logger::write("Dungeon #" + std::to_string(d.id) + " spawned: " +
                  DungeonTypeDefs[(int)d.type].name + " at region " +
                  std::to_string(surface_r->num) + ", " +
                  std::to_string((int)d.room_nums.size()) + " rooms.");
}

// ---------------------------------------------------------------------------
// Game::ProcessDungeons — main entry point called each turn.
// ---------------------------------------------------------------------------
void Game::ProcessDungeons()
{
    if (!Globals->DUNGEON_LEVEL) return;

    // Dynamic cap: ~1/3 of available 8×8 cells, minimum 1.
    ARegionArray *da = regions.get_first_region_array_of_type(ARegionArray::LEVEL_DUNGEON);
    int max_active = 1;
    if (da) {
        int total_cells = (da->x / dungeon::CELL_SIZE) * (da->y / dungeon::CELL_SIZE);
        max_active = std::max(1, total_cells);
    }

    // --- Spawn (not before turn 6 — players need time to develop) ---
    if (TurnNumber() >= dungeon::MIN_SPAWN_TURN) {
        for (int i = 0; i < dungeon::SPAWN_ATTEMPTS; i++) {
            if ((int)activeDungeons.size() >= max_active) break;
            if (rng::get_random(100) < dungeon::SPAWN_CHANCE)
                try_spawn_dungeon();
        }
    }

    // --- Boss death detection + max lifetime: ACTIVE → DYING ---
    for (auto &d : activeDungeons) {
        if (d.state != DungeonSlotState::ACTIVE) continue;

        // Lazy-init spawn_turn for dungeons loaded from old save files.
        if (d.spawn_turn == -1) d.spawn_turn = TurnNumber();

        // Max lifetime: start DYING even if boss is still alive.
        const auto &td_life = DungeonTypeDefs[(int)d.type];
        if (TurnNumber() - d.spawn_turn >= td_life.max_lifetime_turns) {
            d.state      = DungeonSlotState::DYING;
            d.phase_turn = TurnNumber();

            if (d.entrance_object_num >= 0) {
                ARegion *sr = regions.GetRegion(d.surface_region_num);
                if (sr) {
                    for (auto it = sr->objects.begin(); it != sr->objects.end(); ++it) {
                        if ((*it)->num == d.entrance_object_num) {
                            delete *it;
                            sr->objects.erase(it);
                            break;
                        }
                    }
                }
                d.entrance_object_num = -1;
            }

            {
                ARegion *sr = regions.GetRegion(d.surface_region_num);
                auto *f = new DungeonFact();
                f->event_type        = DungeonEventType::DECAYING;
                f->dungeon_type_name = td_life.name;
                f->region_name       = sr ? sr->name : "unknown";
                this->events->AddFact(f);
            }
            logger::write("Dungeon #" + std::to_string(d.id) + " lifetime expired — DYING.");
            continue;
        }

        int kill_item = DungeonTypeDefs[(int)d.type].boss_kill_item;
        bool boss_alive = false;
        for (int rnum : d.room_nums) {
            ARegion *r = regions.GetRegion(rnum);
            if (!r) continue;
            for (auto *obj : r->objects) {
                for (auto *u : obj->units) {
                    if (u->items.GetNum(kill_item) > 0) { boss_alive = true; break; }
                }
                if (boss_alive) break;
            }
            if (boss_alive) break;
        }

        if (!boss_alive) {
            d.state      = DungeonSlotState::DYING;
            d.phase_turn = TurnNumber();

            // Remove surface entrance so no one new can enter.
            if (d.entrance_object_num >= 0) {
                ARegion *sr = regions.GetRegion(d.surface_region_num);
                if (sr) {
                    for (auto it = sr->objects.begin(); it != sr->objects.end(); ++it) {
                        if ((*it)->num == d.entrance_object_num) {
                            delete *it;
                            sr->objects.erase(it);
                            break;
                        }
                    }
                }
                d.entrance_object_num = -1;
            }

            {
                ARegion *sr = regions.GetRegion(d.surface_region_num);
                auto *f = new DungeonFact();
                f->event_type        = DungeonEventType::BOSS_KILLED;
                f->dungeon_type_name = DungeonTypeDefs[(int)d.type].name;
                f->region_name       = sr ? sr->name : "unknown";
                this->events->AddFact(f);
            }
            logger::write("Dungeon #" + std::to_string(d.id) + " boss defeated — DYING.");
        }
    }

    // --- DYING → COLLAPSING ---
    for (auto &d : activeDungeons) {
        if (d.state != DungeonSlotState::DYING) continue;
        int dying_turns = DungeonTypeDefs[(int)d.type].dying_turns;
        if (TurnNumber() - d.phase_turn >= dying_turns) {
            d.state = DungeonSlotState::COLLAPSING;
            ARegion *sr = regions.GetRegion(d.surface_region_num);
            auto *f = new DungeonFact();
            f->event_type        = DungeonEventType::COLLAPSING;
            f->dungeon_type_name = DungeonTypeDefs[(int)d.type].name;
            f->region_name       = sr ? sr->name : "unknown";
            this->events->AddFact(f);
        }
    }

    // --- COLLAPSING: evacuate players, kill monsters, reset cell, free slot ---
    for (int i = (int)activeDungeons.size()-1; i >= 0; i--) {
        DungeonInstance &d = activeDungeons[i];
        if (d.state != DungeonSlotState::COLLAPSING) continue;

        ARegion *surface_r = regions.GetRegion(d.surface_region_num);

        for (int rnum : d.room_nums) {
            ARegion *r = regions.GetRegion(rnum);
            if (!r) continue;

            // Evacuate player units to surface.
            if (surface_r) {
                for (auto *obj : r->objects) {
                    std::vector<Unit*> snap(obj->units.begin(), obj->units.end());
                    for (auto *u : snap) {
                        if (u->faction->num == monfaction) continue;
                        u->MoveUnit(surface_r->GetDummy());
                        u->event("Is expelled from a collapsing dungeon.", "dungeon");
                    }
                }
            }

            // Remove all objects and their monster units.
            for (auto *obj : r->objects) {
                std::vector<Unit*> snap(obj->units.begin(), obj->units.end());
                for (auto *u : snap) {
                    // Null the ppUnits slot before deleting (standard unit teardown).
                    if (u->num >= 0 && u->num < (int)maxppunits) ppUnits[u->num] = nullptr;
                    delete u;
                }
                obj->units.clear();
                delete obj;
            }
            r->objects.clear();

            // Reset terrain and name.
            r->type = R_BARREN;
            r->set_name("The Barrens");
            // Neighbor pointers stay nulled — they will be restored the next time
            // a dungeon spawns in this cell via coord_neighbors (bypasses neighbors[]).
        }

        logger::write("Dungeon #" + std::to_string(d.id) + " collapsed.");
        activeDungeons.erase(activeDungeons.begin() + i);
    }
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

void Game::write_dungeons(std::ostream &f)
{
    f << "DUNGEONS\n" << activeDungeons.size() << "\n";
    for (const auto &d : activeDungeons) {
        f << "BEGIN_DUNGEON\n";
        f << "id "               << d.id                    << "\n";
        f << "type "             << (int)d.type             << "\n";
        f << "state "            << (int)d.state            << "\n";
        f << "phase_turn "       << d.phase_turn            << "\n";
        f << "spawn_turn "       << d.spawn_turn            << "\n";
        f << "cell "             << d.cell_x << " " << d.cell_y << "\n";
        f << "surface_region "   << d.surface_region_num    << "\n";
        f << "entrance_object "  << d.entrance_object_num   << "\n";
        f << "entry_region "     << d.entry_region_num      << "\n";
        f << "exit_object "      << d.exit_object_num       << "\n";
        f << "boss_region "      << d.boss_region_num       << "\n";
        f << "rooms "            << d.room_nums.size();
        for (int rnum : d.room_nums) f << " " << rnum;
        f << "\n";
        f << "END_DUNGEON\n";
    }
    f << "END_DUNGEONS\n";
}

void Game::read_dungeons(std::istream &f)
{
    activeDungeons.clear();
    nextDungeonId = 1;

    std::string kw;
    if (!(f >> kw) || kw != "DUNGEONS") return;  // tolerant: old save file without section

    int count;
    f >> count;

    for (int i = 0; i < count; i++) {
        if (!(f >> kw) || kw != "BEGIN_DUNGEON") break;

        DungeonInstance d;
        std::string tag;
        while (f >> tag && tag != "END_DUNGEON") {
            if      (tag == "id")             f >> d.id;
            else if (tag == "type")           { int t; f >> t; d.type  = (DungeonType)t; }
            else if (tag == "state")          { int s; f >> s; d.state = (DungeonSlotState)s; }
            else if (tag == "phase_turn")     f >> d.phase_turn;
            else if (tag == "spawn_turn")     f >> d.spawn_turn;
            else if (tag == "cell")           f >> d.cell_x >> d.cell_y;
            else if (tag == "surface_region") f >> d.surface_region_num;
            else if (tag == "entrance_object")f >> d.entrance_object_num;
            else if (tag == "entry_region")   f >> d.entry_region_num;
            else if (tag == "exit_object")    f >> d.exit_object_num;
            else if (tag == "boss_region")    f >> d.boss_region_num;
            else if (tag == "rooms") {
                int sz; f >> sz;
                d.room_nums.resize(sz);
                for (int j = 0; j < sz; j++) f >> d.room_nums[j];
            }
        }

        if (d.id >= nextDungeonId) nextDungeonId = d.id + 1;
        activeDungeons.push_back(std::move(d));
    }

    f >> kw;  // consume "END_DUNGEONS"
}

// ---------------------------------------------------------------------------
// Game::migrate_dungeon_boss_counts
// One-time migration: cap I_DEVIL to 1, cap I_DRAGON to 2 in existing bosses.
// Safe to call every turn — no-op once all counts are already in range.
// ---------------------------------------------------------------------------
void Game::migrate_dungeon_boss_counts()
{
    int fixed = 0;

    for (auto &d : activeDungeons) {
        if (d.state == DungeonSlotState::COLLAPSING) continue;

        const auto &td = DungeonTypeDefs[(int)d.type];
        int kill_item = td.boss_kill_item;
        if (kill_item != I_DEVIL && kill_item != I_DRAGON) continue;

        int target = (kill_item == I_DEVIL) ? 1 : 2;

        for (int rnum : d.room_nums) {
            ARegion *r = regions.GetRegion(rnum);
            if (!r) continue;
            for (auto *obj : r->objects) {
                for (auto *u : obj->units) {
                    int cur = u->items.GetNum(kill_item);
                    if (cur > target) {
                        u->items.SetNum(kill_item, target);
                        logger::write("Dungeon #" + std::to_string(d.id) +
                                      ": boss " + ItemDefs[kill_item].names +
                                      " capped " + std::to_string(cur) +
                                      " -> " + std::to_string(target));
                        fixed++;
                    }
                }
            }
        }
    }

    if (fixed > 0)
        logger::write("Dungeon boss count migration: " +
                      std::to_string(fixed) + " unit(s) fixed.");
    else
        logger::write("Dungeon boss count migration: nothing to fix.");
}
