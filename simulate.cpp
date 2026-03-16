// simulate.cpp - Battle simulator subcommand
// Usage: ./neworigins simulate <input.json>
// Output: JSON to stdout

#include "game.h"
#include "battle.h"
#include "army.h"
#include "unit.h"
#include "faction.h"
#include "aregion.h"
#include "object.h"
#include "events.h"
#include "items.h"
#include "skills.h"
#include "gamedata.h"

#include "external/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Terrain lookup by name
// ---------------------------------------------------------------------------
static int lookup_terrain(const std::string& name) {
    for (size_t i = 0; i < TerrainDefs.size(); i++) {
        if (name == TerrainDefs[i].name) return (int)i;
    }
    return R_PLAIN;
}

// ---------------------------------------------------------------------------
// Templates (parsed once from JSON, reused across N battles)
// ---------------------------------------------------------------------------
struct UnitTemplate {
    std::string name       = "Unit";
    std::vector<std::pair<int,int>> items;   // (item_type_index, count)
    std::vector<std::pair<int,int>> skills;  // (skill_index, level)
    bool behind            = false;
    bool is_mage           = false;
    int  combat_skill      = -1;  // skill index for COMBAT spell (-1 = none)
};

// One building (or "no building" when building_type == -1) with its units
struct BuildingGroup {
    int building_type = -1;  // -1 = field (no building bonus)
    std::vector<UnitTemplate> units;
};

struct SideTemplate {
    std::vector<BuildingGroup> groups;

    bool empty() const {
        for (const auto& g : groups)
            if (!g.units.empty()) return false;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Statistics computed from a vector of survivor counts
// ---------------------------------------------------------------------------
struct SurvivorStats {
    int    count;      // number of samples
    int    min, max;
    double mean;
    double median;
    int    mode;
    double std_dev;
    int    p10, p25, p75, p90;  // percentiles; p25-p75 = "Expected" range
};

// Compute statistics from a sorted or unsorted vector of ints.
// Returns nullopt-equivalent (count=0) if the vector is empty.
static SurvivorStats compute_stats(std::vector<int> v) {
    SurvivorStats s{};
    s.count = (int)v.size();
    if (s.count == 0) return s;

    std::sort(v.begin(), v.end());

    s.min = v.front();
    s.max = v.back();

    // Mean
    double sum = 0;
    for (int x : v) sum += x;
    s.mean = sum / s.count;

    // Median
    int mid = s.count / 2;
    s.median = (s.count % 2 == 1)
        ? v[mid]
        : (v[mid - 1] + v[mid]) / 2.0;

    // Mode (most frequent value; first occurrence if tie)
    {
        std::map<int,int> freq;
        for (int x : v) freq[x]++;
        int best_val = v[0], best_cnt = 0;
        for (const auto& [val, cnt] : freq) {
            if (cnt > best_cnt) { best_cnt = cnt; best_val = val; }
        }
        s.mode = best_val;
    }

    // Standard deviation (population)
    {
        double sq_sum = 0;
        for (int x : v) sq_sum += (x - s.mean) * (x - s.mean);
        s.std_dev = std::sqrt(sq_sum / s.count);
    }

    // Percentiles (nearest rank method)
    auto percentile = [&](double pct) -> int {
        int idx = (int)std::ceil(pct / 100.0 * s.count) - 1;
        if (idx < 0) idx = 0;
        if (idx >= s.count) idx = s.count - 1;
        return v[idx];
    };
    s.p10 = percentile(10);
    s.p25 = percentile(25);
    s.p75 = percentile(75);
    s.p90 = percentile(90);

    return s;
}

// Serialize SurvivorStats to a JSON object.
// Only included in output when count > 0.
static json stats_to_json(const SurvivorStats& s) {
    json j;
    j["count"]   = s.count;
    j["min"]     = s.min;
    j["max"]     = s.max;
    j["mean"]    = std::round(s.mean    * 100.0) / 100.0;
    j["median"]  = std::round(s.median  * 100.0) / 100.0;
    j["mode"]    = s.mode;
    j["std_dev"] = std::round(s.std_dev * 100.0) / 100.0;
    j["p10"]     = s.p10;
    j["p25"]     = s.p25;
    j["p75"]     = s.p75;
    j["p90"]     = s.p90;
    return j;
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------
static UnitTemplate parse_unit_template(const json& j) {
    UnitTemplate t;
    if (j.contains("name")) t.name = j["name"].get<std::string>();

    if (j.contains("items")) {
        for (const auto& ji : j["items"]) {
            int it = lookup_item(ji["abbr"].get<std::string>());
            if (it == -1) {
                std::cerr << "Warning: unknown item '" << ji["abbr"] << "' (skipped)\n";
                continue;
            }
            t.items.emplace_back(it, ji.value("count", 1));
        }
    }

    if (j.contains("skills")) {
        for (const auto& js : j["skills"]) {
            int sk = lookup_skill(js["abbr"].get<std::string>());
            if (sk == -1) {
                std::cerr << "Warning: unknown skill '" << js["abbr"] << "' (skipped)\n";
                continue;
            }
            t.skills.emplace_back(sk, js.value("level", 1));
        }
    }

    t.behind   = j.value("behind", false);
    t.is_mage  = j.value("mage",   false);

    if (j.contains("combat_spell")) {
        std::string abbr = j["combat_spell"].get<std::string>();
        int sk = lookup_skill(abbr);
        if (sk == -1) {
            std::cerr << "Warning: unknown combat_spell '" << abbr << "' (skipped)\n";
        } else if (!(SkillDefs[sk].flags & SkillType::COMBAT)) {
            std::cerr << "Warning: '" << abbr << "' is not a COMBAT skill (skipped)\n";
        } else {
            t.combat_skill = sk;
            t.is_mage = true;  // combat_spell implies mage
        }
    }

    return t;
}

static BuildingGroup parse_building_group(const json& j, int btype) {
    BuildingGroup g;
    g.building_type = btype;
    if (j.contains("units")) {
        for (const auto& ju : j["units"])
            g.units.push_back(parse_unit_template(ju));
    }
    return g;
}

static SideTemplate parse_side(const json& j) {
    SideTemplate s;

    // --- buildings[] : units inside a specific building ---
    if (j.contains("buildings")) {
        for (const auto& jb : j["buildings"]) {
            std::string bname = jb.value("type", "");
            int btype = lookup_object(bname);
            if (btype < 0) {
                std::cerr << "Warning: unknown building '" << bname << "' (skipped)\n";
                continue;
            }
            s.groups.push_back(parse_building_group(jb, btype));
        }
    }

    // --- units[] : units in the field (no building) ---
    if (j.contains("units")) {
        BuildingGroup outside;
        outside.building_type = -1;
        for (const auto& ju : j["units"])
            outside.units.push_back(parse_unit_template(ju));
        s.groups.push_back(outside);
    }

    return s;
}

// Count total combatants in a SideTemplate (used for initial_strength output).
// Counts IT_MAN (people/races) and IT_MONSTER so that monster-only armies
// also report a non-zero initial strength.
static int count_initial_combatants(const SideTemplate& tmpl) {
    int total = 0;
    for (const auto& group : tmpl.groups)
        for (const auto& ut : group.units)
            for (const auto& [it, cnt] : ut.items)
                if (ItemDefs[it].type & (IT_MAN | IT_MONSTER))
                    total += cnt;
    return total;
}

// ---------------------------------------------------------------------------
// Create a Unit from a template
// ---------------------------------------------------------------------------
static Unit* make_unit(const UnitTemplate& tmpl, Faction* fac, Object* obj, Game* game) {
    Unit* u = game->GetNewUnit(fac);
    u->set_name(tmpl.name);
    u->object = obj;  // required: AddBattleFact reads unit->object

    for (const auto& [it, cnt] : tmpl.items)
        u->items.SetNum(it, cnt);

    for (const auto& [sk, level] : tmpl.skills) {
        int men = u->GetMen();
        if (men < 1) men = 1;
        u->skills.SetDays(sk, GetDaysByLevel(level) * men);
    }

    if (tmpl.behind)        u->SetFlag(FLAG_BEHIND, 1);
    if (tmpl.is_mage)       u->type   = U_MAGE;
    if (tmpl.combat_skill != -1) u->combat = tmpl.combat_skill;

    u->canattack = 1;
    u->nomove    = 0;
    return u;
}

// ---------------------------------------------------------------------------
// Single battle result
// ---------------------------------------------------------------------------
struct OneResult {
    int outcome;   // BATTLE_WON / BATTLE_LOST / BATTLE_DRAW / BATTLE_IMPOSSIBLE
    int att_surv;
    int def_surv;
    std::vector<std::string> report;
};

// ---------------------------------------------------------------------------
// Game::SimulateBattle — main entry point
// ---------------------------------------------------------------------------
int Game::SimulateBattle(const std::string& inputFile) {
    std::ifstream f(inputFile);
    if (!f.is_open()) {
        std::cerr << "Cannot open input file: " << inputFile << "\n";
        return 1;
    }

    json j;
    try {
        j = json::parse(f);
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return 1;
    }

    int n_battles = j.value("battles", 1);
    if (n_battles < 1) n_battles = 1;
    if (n_battles > 10000) n_battles = 10000;

    if (j.value("seed", -1) != -1)
        rng::seed_random((unsigned int)j["seed"].get<int>());

    std::string terrain_name = j.value("region_type", "plain");
    int terrain_type = lookup_terrain(terrain_name);

    SideTemplate att_tmpl = parse_side(j.value("attacker", json::object()));
    SideTemplate def_tmpl = parse_side(j.value("defender", json::object()));

    if (att_tmpl.empty()) { std::cerr << "Error: attacker has no units\n"; return 1; }
    if (def_tmpl.empty()) { std::cerr << "Error: defender has no units\n"; return 1; }

    // Initial strength counts (computed once from templates, not from battle results).
    // Counts IT_MAN (races/people) and IT_MONSTER so monster armies report correctly.
    int att_initial = count_initial_combatants(att_tmpl);
    int def_initial = count_initial_combatants(def_tmpl);

    // --- Dummy region ---
    // ARegionArray needed because short_print() dereferences region->level
    ARegionArray dummy_level(1, 1);
    ARegion dummy_reg;
    dummy_reg.type = terrain_type;
    dummy_reg.xloc = 0;
    dummy_reg.yloc = 0;
    dummy_reg.zloc = 0;
    dummy_reg.level = &dummy_level;

    // --- Field object (O_DUMMY, capacity=0 → no building bonus) ---
    Object field_obj(&dummy_reg);

    // --- Simulation factions ---
    Faction att_fac(factionseq++);  att_fac.name = "Attacker";
    Faction def_fac(factionseq++);  def_fac.name = "Defender";

    // --- Helper: free a unit and null its ppUnits slot ---
    auto free_unit = [this](Unit* u) {
        unsigned int n = u->num;
        delete u;
        if (n < maxppunits) ppUnits[n] = nullptr;
    };

    // --- Helper: populate one army side from a SideTemplate ---
    // Returns flat list of created units; fills locs and battle_objs.
    auto build_side = [&](
        const SideTemplate& tmpl,
        Faction* fac,
        std::vector<Unit*>& units,
        std::list<Location*>& locs,
        std::vector<Object*>& battle_objs
    ) {
        for (const auto& group : tmpl.groups) {
            // Create a fresh building Object for this group (if inside a building)
            Object* group_obj;
            if (group.building_type != -1) {
                Object* bobj = new Object(&dummy_reg);
                bobj->type     = group.building_type;
                bobj->capacity = ObjectDefs[group.building_type].protect;
                battle_objs.push_back(bobj);
                group_obj = bobj;
            } else {
                group_obj = &field_obj;
            }

            for (const auto& ut : group.units) {
                Unit* u = make_unit(ut, fac, group_obj, this);
                units.push_back(u);
                Location* loc = new Location();
                loc->unit   = u;
                loc->obj    = group_obj;
                loc->region = &dummy_reg;
                locs.push_back(loc);
            }
        }
    };

    // --- Helper: run one battle ---
    auto run_one = [&](bool capture_report) -> OneResult {
        std::vector<Unit*> att_units, def_units;
        std::list<Location*> att_locs, def_locs;
        std::vector<Object*> battle_objs;  // building Objects to delete after battle

        build_side(att_tmpl, &att_fac, att_units, att_locs, battle_objs);
        build_side(def_tmpl, &def_fac, def_units, def_locs, battle_objs);

        Unit* att_leader = att_units[0];
        Unit* def_leader = def_units[0];

        Battle b;
        b.WriteSides(&dummy_reg, att_leader, def_leader, att_locs, def_locs, 0);

        Events events;
        OneResult res;
        res.outcome = b.Run(&events, &dummy_reg, att_leader, att_locs, def_leader, def_locs, 0);

        if (capture_report) res.report = b.text;

        res.att_surv = 0;
        for (auto* u : att_units) res.att_surv += u->GetMen();
        res.def_surv = 0;
        for (auto* u : def_units) res.def_surv += u->GetMen();

        for (auto* loc : att_locs)  delete loc;
        for (auto* loc : def_locs)  delete loc;
        for (auto* u   : att_units) free_unit(u);
        for (auto* u   : def_units) free_unit(u);
        for (auto* o   : battle_objs) delete o;

        return res;
    };

    // --- Run N battles ---
    // Counters
    int att_wins = 0, def_wins = 0, draws = 0, impossible = 0;

    // Survivor vectors — collected every battle, sorted and analyzed after the loop.
    // Memory: max 6 × 10000 × 4 bytes = 240 KB (negligible).
    std::vector<int> att_all,  def_all;           // all battles
    std::vector<int> att_when_att_wins;            // att survivors in att-win battles
    std::vector<int> def_when_att_wins;            // def survivors in att-win battles (≈0)
    std::vector<int> def_when_def_wins;            // def survivors in def-win battles
    std::vector<int> att_when_def_wins;            // att survivors in def-win battles (≈0)

    att_all.reserve(n_battles);
    def_all.reserve(n_battles);

    std::vector<std::string> win_report, loss_report, draw_report;

    for (int i = 0; i < n_battles; i++) {
        bool need_capture = win_report.empty() || loss_report.empty() || draw_report.empty();
        OneResult r = run_one(need_capture);

        // Collect survivors for distribution statistics — O(1) per battle
        att_all.push_back(r.att_surv);
        def_all.push_back(r.def_surv);

        switch (r.outcome) {
            case BATTLE_WON:
                att_wins++;
                att_when_att_wins.push_back(r.att_surv);
                def_when_att_wins.push_back(r.def_surv);
                if (win_report.empty() && !r.report.empty()) win_report = r.report;
                break;
            case BATTLE_LOST:
                def_wins++;
                def_when_def_wins.push_back(r.def_surv);
                att_when_def_wins.push_back(r.att_surv);
                if (loss_report.empty() && !r.report.empty()) loss_report = r.report;
                break;
            case BATTLE_DRAW:
                draws++;
                if (draw_report.empty() && !r.report.empty()) draw_report = r.report;
                break;
            default:
                impossible++;
                break;
        }
    }

    // --- Compute statistics from collected vectors ---
    // Sorting + arithmetic on max 10000 ints: ~0.1ms total, completely negligible.
    SurvivorStats att_stats         = compute_stats(att_all);
    SurvivorStats def_stats         = compute_stats(def_all);
    SurvivorStats att_stats_on_win  = compute_stats(att_when_att_wins);
    SurvivorStats def_stats_on_loss = compute_stats(def_when_att_wins);
    SurvivorStats def_stats_on_win  = compute_stats(def_when_def_wins);
    SurvivorStats att_stats_on_loss = compute_stats(att_when_def_wins);

    // --- Output JSON ---
    json out;

    // Basic counts (backward-compatible — same fields as before)
    out["total"]          = n_battles;
    out["attacker_wins"]  = att_wins;
    out["defender_wins"]  = def_wins;
    out["draws"]          = draws;
    out["attacker_win_rate"]      = (n_battles > 0) ? (double)att_wins / n_battles : 0.0;
    out["defender_win_rate"]      = (n_battles > 0) ? (double)def_wins / n_battles : 0.0;

    // Legacy average fields (kept for backward compatibility with frontend)
    out["avg_attacker_survivors"] = att_stats.count > 0 ? att_stats.mean : 0.0;
    out["avg_defender_survivors"] = def_stats.count > 0 ? def_stats.mean : 0.0;

    // Initial force strength (count of IT_MAN items before any battle)
    out["attacker_initial"] = att_initial;
    out["defender_initial"] = def_initial;

    // Full distribution stats — all N battles
    out["attacker_stats"] = stats_to_json(att_stats);
    out["defender_stats"] = stats_to_json(def_stats);

    // Per-outcome stats: survivors when THIS side wins
    if (att_stats_on_win.count > 0)
        out["attacker_stats_on_win"]  = stats_to_json(att_stats_on_win);
    if (def_stats_on_win.count > 0)
        out["defender_stats_on_win"]  = stats_to_json(def_stats_on_win);

    // Per-outcome stats: enemy survivors when THIS side wins (usually near 0)
    if (def_stats_on_loss.count > 0)
        out["defender_stats_on_loss"] = stats_to_json(def_stats_on_loss);
    if (att_stats_on_loss.count > 0)
        out["attacker_stats_on_loss"] = stats_to_json(att_stats_on_loss);

    // Per-battle survivor counts — already collected in vectors above, zero extra cost.
    // Frontend uses these to build histograms and any custom distribution analysis.
    // Payload: N integers per vector (e.g. 1000 battles = ~4 KB per vector).
    out["attacker_survivors_list"] = att_all;
    out["defender_survivors_list"] = def_all;

    // Sample battle reports (one per outcome type)
    json reports = json::object();
    if (!win_report.empty())  reports["attacker_win"] = win_report;
    if (!loss_report.empty()) reports["defender_win"] = loss_report;
    if (!draw_report.empty()) reports["draw"]         = draw_report;
    out["sample_reports"] = reports;

    std::cout << out.dump(2) << "\n";
    return 0;
}
