// quest_generator.cpp — LOCAL quest generation, domain computation, TTL expiry,
// awareness pass.  All new quest system engine logic lives here.
//
// NewOrigins-specific quest creation (GLOBAL_BOSS_HUNT / pirate captains) is in
// neworigins/extra.cpp.  Order execution (QUEST order) is in runorders.cpp.
// Candidate tables and Add* API are in quest_data.cpp.
// Quest data model, check_kill_target, save/load are in quests.cpp.

#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "quest_data.h"
#include "dungeon.h"
#include "rng.hpp"
#include <algorithm>
#include <queue>
#include <set>
#include <unordered_map>

using namespace std;

// ---------------------------------------------------------------------------
// Tuning constants (GM-adjustable; candidates for GameDefs if needed).
// ---------------------------------------------------------------------------
static constexpr int LOCAL_QUESTS_PER_MAYOR  = 5;   // max active quests per mayor
static constexpr int DUNGEON_QUEST_CAP       = 1;   // at most 1 dungeon quest per mayor
static constexpr int HUNT_QUEST_CAP          = 2;   // at most 2 hunt/lair quests per mayor
static constexpr int INFRA_QUEST_CAP         = 1;   // at most 1 infrastructure quest (road / tower / inn)
static constexpr int LOCAL_QUEST_WATER_LIMIT  = 16;  // max water hexes in domain
static constexpr int MIN_LAND_REGIONS         = 24;  // expand radius if domain has fewer land hexes
static constexpr int MAX_DOMAIN_DEPTH         = 4;   // never go deeper than this
static constexpr int ROAD_SEARCH_MIN_DIST     = 3;   // start looking for settlements at this radius
static constexpr int ROAD_SEARCH_MAX_DIST     = 10;  // give up searching beyond this radius
// LOCAL_QUEST_TTL = 12, ROAD_QUEST_TOKENS = 3 — defined in quest_data.h

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// BFS distances from start using neighbors[] — same map level only.
static unordered_map<int,int> region_bfs_dist(ARegion *start) {
    unordered_map<int,int> dist;
    queue<ARegion*> q;
    dist[start->num] = 0;
    q.push(start);
    while (!q.empty()) {
        ARegion *cur = q.front(); q.pop();
        for (int d = 0; d < NDIRS; d++) {
            ARegion *nb = cur->neighbors[d];
            if (!nb || dist.count(nb->num)) continue;
            if (nb->level->levelType != start->level->levelType) continue;
            dist[nb->num] = dist[cur->num] + 1;
            q.push(nb);
        }
    }
    return dist;
}

// BFS from `city` over land regions only (water skipped — roads can't cross water).
// Tracks first direction from city AND parent links for full path reconstruction.
// Returns map: region_num -> {dist, first_dir, parent_num, dir_from_parent}.
struct BfsDirEntry { int dist; int first_dir; int parent_num; int dir_from_parent; };
static unordered_map<int, BfsDirEntry> bfs_with_first_dir(ARegion *city, int level, int max_dist) {
    unordered_map<int, BfsDirEntry> result;
    struct Node { ARegion *r; int dist; int first_dir; };
    queue<Node> q;
    result[city->num] = {0, -1, -1, -1};

    for (int d = 0; d < NDIRS; d++) {
        ARegion *nb = city->neighbors[d];
        if (!nb || nb->level->levelType != level) continue;
        // Skip water — roads cannot be built there.
        if (TerrainDefs[nb->type].similar_type == R_OCEAN || nb->type == R_LAKE) continue;
        if (result.count(nb->num)) continue;
        result[nb->num] = {1, d, city->num, d};
        q.push({nb, 1, d});
    }
    while (!q.empty()) {
        auto [cur, dist, first_dir] = q.front(); q.pop();
        if (dist >= max_dist) continue;
        for (int d = 0; d < NDIRS; d++) {
            ARegion *nb = cur->neighbors[d];
            if (!nb || nb->level->levelType != level) continue;
            if (TerrainDefs[nb->type].similar_type == R_OCEAN || nb->type == R_LAKE) continue;
            if (result.count(nb->num)) continue;
            result[nb->num] = {dist + 1, first_dir, cur->num, d};
            q.push({nb, dist + 1, first_dir});
        }
    }
    return result;
}

static bool is_water_region(ARegion *r) {
    return TerrainDefs[r->type].similar_type == R_OCEAN || r->type == R_LAKE;
}

// Returns true if unit_num is already the target of an active kill quest.
static bool is_active_kill_target(int unit_num) {
    for (const auto& q : quests) {
        if ((q->subtype == Quest::LOCAL_HUNT ||
             q->subtype == Quest::LOCAL_LAIR_CLEAR ||
             q->subtype == Quest::GLOBAL_BOSS_HUNT) &&
            q->target == unit_num) return true;
    }
    return false;
}

// Best token payout for a unit's monster races; 0 if not in hunt_targets table.
static int best_hunt_tokens(Unit *u) {
    int best = 0;
    for (const auto& ht : hunt_targets) {
        if (u->items.GetNum(ht.race_item) > 0 && ht.tokens > best)
            best = ht.tokens;
    }
    return best;
}

// ---------------------------------------------------------------------------
// UpdateQuestAwareness
// Any non-NPC unit physically in a LOCAL quest's issuer region learns about
// the quest.  Awareness is additive-only; removal happens in erase_with_cleanup
// when a mayor dies.  HOSTILE factions gain awareness — "anyone can read the notice board".
// ---------------------------------------------------------------------------
void Game::UpdateQuestAwareness() {
    for (const auto& q : quests) {
        if (q->scope != Quest::SCOPE_LOCAL) continue;

        ARegion *r = regions.GetRegion(q->issuer_region);
        if (!r) continue;

        for (const auto o : r->objects) {
            for (const auto u : o->units) {
                if (u->faction->is_npc) continue;
                if (u->GetSoldiers() == 0) continue;  // skip dead units
                u->faction->known_local_quests.insert(q->num);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ComputeMayorDomain — adaptive BFS radius, see plan §4.18.
// Starts at depth 2; expands to MAX_DOMAIN_DEPTH if land count < MIN_LAND_REGIONS.
// ---------------------------------------------------------------------------

// Collect all unique region->name values reachable from city within `depth` hops
// on the same map level, using BFS.
static void collect_names_bfs(ARegion *city, int mayor_level, int depth,
                               set<string>& names)
{
    unordered_map<int,int> visited;
    queue<ARegion*> q;
    visited[city->num] = 0;
    q.push(city);
    while (!q.empty()) {
        ARegion *cur = q.front(); q.pop();
        int dist = visited[cur->num];
        names.insert(cur->name);
        if (dist >= depth) continue;
        for (int d = 0; d < NDIRS; d++) {
            ARegion *nb = cur->neighbors[d];
            if (!nb || nb->level->levelType != mayor_level) continue;
            if (!visited.count(nb->num)) {
                visited[nb->num] = dist + 1;
                q.push(nb);
            }
        }
    }
}

set<int> Game::ComputeMayorDomain(ARegion *city) {
    int mayor_level = city->level->levelType;

    set<int>       domain;
    vector<ARegion*> water_cands;
    int            land_count = 0;
    int            depth_used = 2;

    for (int depth = 2; depth <= MAX_DOMAIN_DEPTH; depth++) {
        depth_used = depth;
        domain.clear();
        water_cands.clear();
        land_count = 0;
        domain.insert(city->num);

        set<string> names;
        collect_names_bfs(city, mayor_level, depth, names);

        for (const auto r : regions) {
            if (r->level->levelType != mayor_level) continue;
            if (!names.count(r->name)) continue;
            if (is_water_region(r)) { water_cands.push_back(r); continue; }
            domain.insert(r->num);
            land_count++;
        }

        if (land_count >= MIN_LAND_REGIONS) break;
        // Too few land regions — expand and try again.
        logger::write("[quest] domain depth " + to_string(depth) +
                      " gives only " + to_string(land_count) +
                      " land hexes for " + city->short_print() +
                      " — expanding radius");
    }

    // Water cap — keep closest LOCAL_QUEST_WATER_LIMIT hexes by BFS distance.
    if (!water_cands.empty()) {
        auto dist = region_bfs_dist(city);
        sort(water_cands.begin(), water_cands.end(),
            [&dist](ARegion *a, ARegion *b) {
                return dist[a->num] < dist[b->num];
            });
        int taken = 0;
        for (auto *wr : water_cands) {
            if (taken >= LOCAL_QUEST_WATER_LIMIT) break;
            domain.insert(wr->num);
            taken++;
        }
    }

    logger::write("[quest] domain for " + city->short_print() +
                  ": depth=" + to_string(depth_used) +
                  " land=" + to_string(land_count) +
                  " total=" + to_string(domain.size()) + " hexes");

    return domain;
}

// ---------------------------------------------------------------------------
// ExpireLocalQuests — remove LOCAL quests past their TTL.
// ---------------------------------------------------------------------------
void Game::ExpireLocalQuests() {
    int turn = TurnNumber();
    vector<shared_ptr<Quest>> to_erase;
    for (const auto& q : quests) {
        if (q->scope != Quest::SCOPE_LOCAL) continue;
        if (q->expires_turn > 0 && turn >= q->expires_turn)
            to_erase.push_back(q);
    }
    for (auto& q : to_erase) {
        ARegion *r = regions.GetRegion(q->issuer_region);
        string rname = r ? r->short_print() : "region#" + to_string(q->issuer_region);
        logger::write("[quest] expire quest #" + to_string(q->num) +
                      " subtype=" + to_string(q->subtype) +
                      " target=" + to_string(q->target) +
                      " region=" + rname +
                      " (TTL=" + to_string(turn - q->created_turn) + " turns)");
        quests.erase_with_cleanup(q, &factions);
    }
}

// ---------------------------------------------------------------------------
// GenerateLocalQuestsForMayor — fill up to LOCAL_QUESTS_PER_MAYOR.
// Picks candidates from wandering monsters (LOCAL_HUNT), simple lairs
// (LOCAL_LAIR_CLEAR), and dungeon entrances (LOCAL_LAIR_CLEAR).
// Adding new quest subtypes: add a new candidate-collection block below and
// a matching completion hook elsewhere.
// ---------------------------------------------------------------------------

struct QuestCandidate {
    Quest::Subtype  subtype;
    ARegion        *region;              // issuer region (surface for dungeons; city for roads)
    Unit           *target     = nullptr; // unit to kill (nullptr for road quests)
    int             tokens;
    int             lair_type      = -1; // kill: lair object type or dungeon type index
                                         // road: road object type (O_ROADN etc.)
    int             target_region  = -1; // region num where target lives or road leads
    int             dungeon_expires =  0; // for dungeon quests: full dungeon lifetime end turn
    bool            is_ocean       = false; // candidate is in a water region
    bool            is_dungeon     = false; // true for LOCAL_LAIR_CLEAR pointing at a dungeon boss
    std::string     dest_name;             // LOCAL_BUILD_ROAD: destination settlement name
};

// Returns the race item with the highest token value in the unit, or -1.
static int best_hunt_tokens_race(Unit *u) {
    int best_tok  = 0;
    int best_race = -1;
    for (const auto& ht : hunt_targets) {
        if (u->items.GetNum(ht.race_item) > 0 && ht.tokens > best_tok) {
            best_tok  = ht.tokens;
            best_race = ht.race_item;
        }
    }
    return best_race;
}

// True for LOCAL_LAIR_CLEAR quests pointing at a dungeon boss.
static bool is_dungeon_slot_quest(const Quest& q) {
    // Dungeon quests store -(DungeonType index + 1) in building (always < 0).
    // Lair quests store ObjectDefs index (always >= 0).
    return q.subtype == Quest::LOCAL_LAIR_CLEAR && q.building < 0;
}

void Game::GenerateLocalQuestsForMayor(ARegion *city, Unit *mayor) {
    // Only an invested mayor speaks for the town. The cornucopia is his seal of
    // office, handed over by AdjustCityMon the turn after he takes the hall, so a
    // mayor who has just replaced a killed predecessor issues nothing until the
    // town has formally installed him.
    if (mayor->items.GetNum(I_CORNUCOPIA) == 0) return;

    // Count active LOCAL quests per slot type for this mayor.
    int active = 0, dungeon_active = 0, hunt_active = 0, infra_active = 0;
    for (const auto& q : quests) {
        if (q->scope != Quest::SCOPE_LOCAL || q->issuer_unit != mayor->num) continue;
        active++;
        if (q->subtype == Quest::LOCAL_BUILD_ROAD ||
            q->subtype == Quest::LOCAL_BUILD_TOWER ||
            q->subtype == Quest::LOCAL_BUILD_INN)
            infra_active++;
        else if (is_dungeon_slot_quest(*q))
            dungeon_active++;
        else
            hunt_active++;  // LOCAL_HUNT or non-dungeon LOCAL_LAIR_CLEAR
    }
    if (active >= LOCAL_QUESTS_PER_MAYOR) return;

    set<int> domain = ComputeMayorDomain(city);

    // --- Collect all valid candidates from the domain ---
    // LOCAL_HUNT: monfaction units in O_DUMMY (wandering).
    // LOCAL_LAIR_CLEAR: monfaction units in any other non-dungeon object
    //   (lairs, fleet objects, ocean caves, etc.) — best unit per object.
    // Dungeon quests: handled separately below.
    vector<QuestCandidate> pool;

    for (int rnum : domain) {
        ARegion *r = regions.GetRegion(rnum);
        if (!r) continue;
        bool r_is_water = is_water_region(r);

        for (const auto o : r->objects) {
            if (o->type == O_DUNGEON_ENTRANCE) continue; // handled below

            if (o->type == O_DUMMY) {
                // Wandering monsters — each unit is a separate candidate.
                for (const auto u : o->units) {
                    if (u->faction->num != monfaction || u->GetSoldiers() == 0) continue;
                    if (is_active_kill_target(u->num)) continue;
                    int tok = best_hunt_tokens(u);
                    if (tok > 0)
                        pool.push_back({ Quest::LOCAL_HUNT, city, u, tok, -1, r->num,
                                         0, r_is_water });
                }
            } else {
                // Skip ships/fleets — their units can sail away, quest becomes unsatisfiable.
                if (ObjectDefs[o->type].flags & ObjectType::TRANSPORT) continue;
                if (o->IsFleet()) continue;
                // Lairs and other stationary objects — best unit per object.
                Unit *best_unit = nullptr;
                int   best_tok  = 0;
                for (const auto u : o->units) {
                    if (u->faction->num != monfaction || u->GetSoldiers() == 0) continue;
                    if (is_active_kill_target(u->num)) continue;
                    int tok = best_hunt_tokens(u);
                    if (tok > best_tok) { best_tok = tok; best_unit = u; }
                }
                if (best_unit)
                    pool.push_back({ Quest::LOCAL_LAIR_CLEAR, city, best_unit, best_tok,
                                     o->type, r->num, 0, r_is_water });
            }
        }

        // LOCAL_LAIR_CLEAR — dungeon entrance (boss in dungeon level).
        for (const auto o : r->objects) {
            if (o->type != O_DUNGEON_ENTRANCE || o->incomplete > 0) continue;

            for (const auto& d : activeDungeons) {
                if (d.entrance_object_num != o->num) continue;
                if (d.surface_region_num  != r->num)  continue;

                const auto& td = DungeonTypeDefs[(int)d.type];
                int turns_to_close = (d.spawn_turn + td.max_lifetime_turns) - TurnNumber();
                if (turns_to_close < 6) break;

                int dtok = 0;
                for (const auto& lt : lair_targets) {
                    if (lt.object_type  != O_DUNGEON_ENTRANCE) continue;
                    if (lt.dungeon_type != static_cast<int>(d.type)) continue;
                    dtok = lt.tokens;
                    break;
                }
                if (dtok == 0) continue;

                Unit *boss = nullptr;
                for (int dn : d.room_nums) {
                    ARegion *dr = regions.GetRegion(dn);
                    if (!dr) continue;
                    for (const auto dobj : dr->objects) {
                        for (const auto du : dobj->units) {
                            if (du->items.GetNum(td.boss_kill_item) > 0 &&
                                !is_active_kill_target(du->num)) {
                                boss = du; break;
                            }
                        }
                        if (boss) break;
                    }
                    if (boss) break;
                }
                if (boss) {
                    int dungeon_expires = d.spawn_turn + td.max_lifetime_turns + td.dying_turns;
                    // Encode dungeon type as -(type+1) to distinguish from ObjectDefs indices.
                    pool.push_back({ Quest::LOCAL_LAIR_CLEAR, city, boss, dtok,
                                     -(static_cast<int>(d.type) + 1), r->num, dungeon_expires,
                                     false, true /* is_dungeon */ });
                }
                break;
            }
        }
        // Future subtypes (LOCAL_DELIVER, etc.) — add a loop here.
    }

    // LOCAL_BUILD_ROAD candidates.
    // BFS finds all settlements at radius ROAD_SEARCH_MIN_DIST..ROAD_SEARCH_MAX_DIST.
    // For each, the full path is reconstructed via parent links. We walk from city and
    // find the FIRST MISSING road segment. Quest: build that specific segment.
    // If all segments are already built toward a settlement, no quest for that direction.
    {
        int mayor_level = city->level->levelType;
        auto bfs = bfs_with_first_dir(city, mayor_level, ROAD_SEARCH_MAX_DIST);

        // Collect (dist, settlement) pairs, nearest first.
        vector<pair<int, ARegion *>> settlement_cands;
        for (auto& [rnum, entry] : bfs) {
            if (entry.dist < ROAD_SEARCH_MIN_DIST) continue;
            ARegion *r = regions.GetRegion(rnum);
            if (r && r->town) settlement_cands.push_back({entry.dist, r});
        }
        sort(settlement_cands.begin(), settlement_cands.end());

        set<int> dirs_proposed;  // one quest per outgoing direction from city
        for (auto& [dist, dest] : settlement_cands) {
            int first_dir = bfs[dest->num].first_dir;
            if (first_dir < 0) continue;
            if (dirs_proposed.count(first_dir)) continue;

            // Reconstruct path: list of (region_arrived_at, direction_used_from_previous).
            vector<pair<ARegion *, int>> path;
            {
                int cur_num = dest->num;
                while (cur_num != city->num) {
                    auto it = bfs.find(cur_num);
                    if (it == bfs.end() || it->second.parent_num < 0) break;
                    ARegion *cr = regions.GetRegion(cur_num);
                    path.push_back({cr, it->second.dir_from_parent});
                    cur_num = it->second.parent_num;
                }
                reverse(path.begin(), path.end());
            }
            if (path.empty()) continue;

            // Walk path from city: find first missing road segment.
            // Each hex-to-hex connection requires TWO road objects: the departure
            // road (O_ROADSE in build_r) AND the arrival road (O_ROADNW in step_r).
            // Check both sides in order so we propose the logically earlier segment.
            ARegion *build_r = city;
            int      build_d = -1;
            for (auto& [step_r, step_d] : path) {
                // Departure side: does build_r have a road toward step_r?
                int rt_dep = build_r->GetRoadDirection(step_d);
                bool dep_has = false;
                for (const auto o : build_r->objects)
                    if (o->type == rt_dep) { dep_has = true; break; }
                if (!dep_has) { build_d = step_d; break; }

                // Arrival side: does step_r have the complementary road back?
                int opp_d   = (step_d + 3) % NDIRS;
                int rt_arr  = step_r->GetRoadDirection(opp_d);
                bool arr_has = false;
                for (const auto o : step_r->objects)
                    if (o->type == rt_arr) { arr_has = true; break; }
                if (!arr_has) { build_r = step_r; build_d = opp_d; break; }

                build_r = step_r;
            }
            if (build_d < 0) continue;  // road fully built to this settlement

            dirs_proposed.insert(first_dir);
            std::string dname = dest->town ? dest->town->name : dest->name;
            int road_type = build_r->GetRoadDirection(build_d);
            QuestCandidate cand;
            cand.subtype       = Quest::LOCAL_BUILD_ROAD;
            cand.region        = city;
            cand.target        = nullptr;
            cand.tokens        = ROAD_QUEST_TOKENS;
            cand.lair_type     = road_type;     // road object type (O_ROADN etc.)
            cand.target_region = build_r->num;  // region where road needs to be built
            cand.dest_name     = dname;         // destination settlement name for description
            pool.push_back(cand);
        }
    }

    // LOCAL_BUILD_TOWER candidates — land regions in domain without any tower (O_TOWER or O_MTOWER),
    // complete or incomplete. One candidate per eligible region.
    for (int rnum : domain) {
        ARegion *r = regions.GetRegion(rnum);
        if (!r || is_water_region(r)) continue;
        bool has_tower = false;
        for (const auto o : r->objects) {
            if (o->type == O_TOWER || o->type == O_MTOWER) { has_tower = true; break; }
        }
        if (has_tower) continue;
        pool.push_back({ Quest::LOCAL_BUILD_TOWER, city, nullptr, TOWER_QUEST_TOKENS,
                         O_TOWER, rnum, 0, false, false });
    }

    // LOCAL_BUILD_INN candidates — land regions in domain without any inn,
    // complete or incomplete. Water regions (ocean/lake) are skipped because
    // BUILD is forbidden in ocean and inns are not allowed in lakes.
    // One candidate per eligible region.
    for (int rnum : domain) {
        ARegion *r = regions.GetRegion(rnum);
        if (!r || is_water_region(r)) continue;
        bool has_inn = false;
        for (const auto o : r->objects) {
            if (o->type == O_INN) { has_inn = true; break; }
        }
        if (has_inn) continue;
        pool.push_back({ Quest::LOCAL_BUILD_INN, city, nullptr, INN_QUEST_TOKENS,
                         O_INN, rnum, 0, false, false });
    }

    // Count what was found for the summary log.
    int dbg_wander = 0, dbg_lair = 0, dbg_dungeon = 0;
    for (int rnum : domain) {
        ARegion *dr = regions.GetRegion(rnum);
        if (!dr) continue;
        for (const auto o : dr->objects) {
            if (o->type == O_DUMMY) {
                for (const auto u : o->units)
                    if (u->faction->num == monfaction && u->GetSoldiers() > 0) dbg_wander++;
            } else if (o->type == O_DUNGEON_ENTRANCE && o->incomplete <= 0) {
                dbg_dungeon++;
            } else if (o->type != O_DUMMY && o->type > 0 && o->type < NOBJECTS) {
                for (const auto u : o->units)
                    if (u->faction->num == monfaction && u->GetSoldiers() > 0) dbg_lair++;
            }
        }
    }
    int dbg_road = 0, dbg_tower = 0, dbg_inn = 0;
    for (const auto& c : pool) {
        if (c.subtype == Quest::LOCAL_BUILD_ROAD)  dbg_road++;
        if (c.subtype == Quest::LOCAL_BUILD_TOWER) dbg_tower++;
        if (c.subtype == Quest::LOCAL_BUILD_INN)   dbg_inn++;
    }

    logger::write("[quest] mayor unit#" + to_string(mayor->num) +
                  " " + city->short_print() +
                  " active=" + to_string(active) +
                  " [dung=" + to_string(dungeon_active) +
                  " hunt=" + to_string(hunt_active) +
                  " infra=" + to_string(infra_active) + "]" +
                  " pool=" + to_string(pool.size()) +
                  " [wander=" + to_string(dbg_wander) +
                  " lair=" + to_string(dbg_lair) +
                  " dungeon=" + to_string(dbg_dungeon) +
                  " road=" + to_string(dbg_road) +
                  " tower=" + to_string(dbg_tower) +
                  " inn=" + to_string(dbg_inn) + "]");

    if (pool.empty()) return;

    // Count ocean quests already issued by this mayor (cap = 1).
    int ocean_active = 0;
    for (const auto& q : quests) {
        if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == mayor->num) {
            ARegion *tr = regions.GetRegion(q->regionnum);
            if (tr && is_water_region(tr)) ocean_active++;
        }
    }

    // If the infra slot is free and several infra types have candidates, pick exactly one
    // type uniformly at random and drop the others — prevents the type with most candidates
    // (typically INN, one per domain region) from drowning out road/tower by sheer count.
    if (infra_active < INFRA_QUEST_CAP) {
        std::vector<Quest::Subtype> present;
        for (const auto& c : pool) {
            if (c.subtype != Quest::LOCAL_BUILD_ROAD &&
                c.subtype != Quest::LOCAL_BUILD_TOWER &&
                c.subtype != Quest::LOCAL_BUILD_INN) continue;
            Quest::Subtype st = static_cast<Quest::Subtype>(c.subtype);
            if (std::find(present.begin(), present.end(), st) == present.end())
                present.push_back(st);
        }
        if (present.size() > 1) {
            Quest::Subtype keep = present[rng::get_random(static_cast<int>(present.size()))];
            pool.erase(remove_if(pool.begin(), pool.end(),
                [keep](const QuestCandidate& c) {
                    bool is_infra = (c.subtype == Quest::LOCAL_BUILD_ROAD ||
                                     c.subtype == Quest::LOCAL_BUILD_TOWER ||
                                     c.subtype == Quest::LOCAL_BUILD_INN);
                    return is_infra && c.subtype != keep;
                }),
                pool.end());
        }
    }

    // --- Pick random candidates until the mayor's budget is full ---
    int new_hunt_this_run = 0;  // at most 1 new hunt slot filled per turn
    while (active < LOCAL_QUESTS_PER_MAYOR && !pool.empty()) {
        int idx = rng::get_random(static_cast<int>(pool.size()));
        QuestCandidate c = pool[idx];   // copy before erase — reference would dangle
        pool.erase(pool.begin() + idx);

        // Slot budget checks.
        if (c.subtype == Quest::LOCAL_BUILD_ROAD ||
            c.subtype == Quest::LOCAL_BUILD_TOWER ||
            c.subtype == Quest::LOCAL_BUILD_INN) {
            if (infra_active >= INFRA_QUEST_CAP) continue;
        } else if (c.is_dungeon) {
            if (dungeon_active >= DUNGEON_QUEST_CAP) continue;
        } else {
            // LOCAL_HUNT or non-dungeon LOCAL_LAIR_CLEAR.
            if (hunt_active >= HUNT_QUEST_CAP) continue;
            if (new_hunt_this_run >= 1) continue;  // fill only 1 hunt slot per turn
        }

        // Re-check kill target (may have been claimed by an earlier iteration).
        if (c.target && is_active_kill_target(c.target->num)) continue;
        // Ocean quest cap: at most 1 per mayor.
        if (c.target && c.is_ocean && ocean_active >= 1) continue;

        int turn = TurnNumber();
        int std_expires = turn + LOCAL_QUEST_TTL;

        auto q           = make_shared<Quest>();
        q->num           = questseq++;
        q->scope         = Quest::SCOPE_LOCAL;
        q->subtype       = c.subtype;
        q->tokens        = c.tokens;
        q->issuer_unit   = mayor->num;
        q->issuer_region = c.region->num;
        q->created_turn  = turn;
        q->building      = c.lair_type;      // road/lair type (>=0), or -(dungeon_type+1) (<0)
        q->regionnum     = c.target_region;  // destination or region where target lives

        // Mayor's settlement name — used in completion event messages.
        std::string mayor_settlement = city->town ? city->town->name : city->name;

        if (c.subtype == Quest::LOCAL_BUILD_ROAD) {
            q->target       = -1;
            q->regionname   = c.dest_name;  // destination settlement (for description)
            q->expires_turn = std_expires;
        } else if (c.subtype == Quest::LOCAL_BUILD_TOWER) {
            q->target       = -1;
            q->regionname   = mayor_settlement;  // issuer settlement (for event)
            q->expires_turn = std_expires;
        } else if (c.subtype == Quest::LOCAL_BUILD_INN) {
            q->target       = -1;
            q->regionname   = mayor_settlement;  // issuer settlement (for event)
            q->expires_turn = std_expires;
        } else if (c.subtype == Quest::LOCAL_HUNT) {
            q->target         = c.target->num;
            q->target_monster = best_hunt_tokens_race(c.target);
            q->regionname     = mayor_settlement;
            q->expires_turn   = std_expires;
        } else {
            // LOCAL_LAIR_CLEAR (dungeon or simple lair).
            q->target       = c.target->num;
            q->regionname   = mayor_settlement;
            q->expires_turn = (c.dungeon_expires > 0)
                              ? min(std_expires, c.dungeon_expires)
                              : std_expires;
        }

        quests.push_back(q);
        active++;

        // Update slot counters.
        if (c.subtype == Quest::LOCAL_BUILD_ROAD ||
            c.subtype == Quest::LOCAL_BUILD_TOWER ||
            c.subtype == Quest::LOCAL_BUILD_INN) {
            infra_active++;
        } else if (c.is_dungeon) {
            dungeon_active++;
        } else {
            hunt_active++;
            new_hunt_this_run++;
        }
        if (c.target && c.is_ocean) ocean_active++;

        static const char *subtype_names[] = {
            "LOCAL_LAIR_CLEAR", "LOCAL_HUNT", "LOCAL_HARVEST", "GLOBAL_BOSS_HUNT",
            "LOCAL_DELIVER", "LOCAL_VISIT", "LOCAL_GUARD", "LOCAL_ESCORT",
            "GLOBAL_DESTROY", "LOCAL_BUILD_ROAD", "LOCAL_BUILD_TOWER", "LOCAL_BUILD_INN"
        };
        const char *sname = (c.subtype >= 0 && c.subtype < 12)
                            ? subtype_names[c.subtype] : "UNKNOWN";
        string target_info = c.target
                             ? ("unit#" + to_string(c.target->num) + " (" + c.target->name + ")")
                             : "build " + (c.lair_type >= 0 && c.lair_type < NOBJECTS
                                           ? string(ObjectDefs[c.lair_type].name) : "?");
        logger::write("[quest] created #" + to_string(q->num) +
                      " " + sname +
                      " target=" + target_info +
                      " tokens=" + to_string(c.tokens) +
                      " in " + c.region->short_print());
    }
}
