#include "gamedata.h"
#include "game.h"
#include "quests.h"
#include "dungeon.h"
#include "indenter.hpp"
#include "string_parser.hpp"
#include "string_filters.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

const std::vector<std::string> AttitudeStrs = {
    "Hostile",
    "Unfriendly",
    "Neutral",
    "Friendly",
    "Ally"
};

const std::string F_WAR = "War";
const std::string F_TRADE = "Trade";
const std::string F_MAGIC = "Magic";
const std::string F_MARTIAL = "Martial";

std::vector<std::string> ft { };
std::vector<std::string> *FactionTypes = &ft;

const std::vector<std::string> TemplateStrs = {
    "off",
    "short",
    "long",
    "map"
};

// Quit states
static const std::vector<std::string> QuitStrs = {
    "none",
    "quit order",
    "quit by gm",
    "quit and restart",
    "won game",
    "game over"
};

void to_json(json &j, const FactionEvent &e) {
    j = json{{"message", e.message}, {"category", e.category}};
    if (e.unit != nullptr) {
        j["unit"] = e.unit->build_json_descriptor();
    }
    if (e.region != nullptr) {
        j["region"] = e.region->basic_region_data();
    }
};

void to_json(json &j, const FactionError &e) {
    j = json{{"message", e.message}};
    if (e.unit != nullptr) {
        j["unit"] = e.unit->build_json_descriptor();
    }
};

int parse_template_type(const parser::token& str)
{
    for (int i = 0; i < NTEMPLATES; i++)
        if (str == TemplateStrs[i]) return i;
    return -1;
}

std::optional<AttitudeType> parse_attitude(const parser::token& str)
{
    for (int i = 0; i < static_cast<int>(AttitudeType::ATTITUDE_COUNT); i++)
        if (str == AttitudeStrs[i]) return static_cast<AttitudeType>(i);
    return std::nullopt;
}

Faction::Faction()
{
    exists = true;
    for (auto &ft : *FactionTypes) {
        type[ft] = 1;
    }

    lastchange = -6;
    password = "none";
    times = 0;
    showunitattitudes = 0;
    temformat = TEMPLATE_OFF;
    quit = 0;
    defaultattitude = AttitudeType::NEUTRAL;
    unclaimed = 0;
    capital_region = -1;
    pReg = NULL;
    pStartLoc = NULL;
    noStartLeader = 0;
    startturn = 0;
    battleLogFormat = 0;
    guard_attack_this_turn = 0;
}

Faction::Faction(int n)
{
    exists = true;
    num = n;

    for (auto &ft : *FactionTypes) {
        type[ft] = 1;
    }

    lastchange = -6;
    name = "Faction (" + std::to_string(num) + ")";
    address = "NoAddress";
    password = "none";
    times = 1;
    showunitattitudes = 0;
    temformat = TEMPLATE_LONG;
    defaultattitude = AttitudeType::NEUTRAL;
    quit = 0;
    unclaimed = 0;
    capital_region = -1;
    pReg = NULL;
    pStartLoc = NULL;
    noStartLeader = 0;
    startturn = 0;
    battleLogFormat = 0;
    guard_attack_this_turn = 0;
}

Faction::~Faction()
{
    attitudes.clear();
}

void Faction::Writeout(std::ostream& f)
{
    f << num << '\n';

    // Right now, the faction type data in the file is what determines npc/non-npc with -1 in the types
    // signifying NPCs. We should eventually persist the is_npc flag directly, but for now, let's just
    // not break things. Having it based on the faction type data is pretty bogus, but it's what we
    // have for now so if this is an NPC faction, right out -1 for all types.
    for (auto &ft : *FactionTypes) {
        f << (is_npc ? -1 : type[ft]) << '\n';
    }

    f << lastchange << '\n';
    f << lastorders << '\n';
    f << unclaimed << '\n';
    f << name << '\n';
    f << address << '\n';
    f << password << '\n';
    f << times << '\n';
    f << showunitattitudes << '\n';
    f << temformat << '\n';

    skills.Writeout(f);
    items.Writeout(f);
    f << static_cast<int>(defaultattitude) << '\n';
    f << attitudes.size() << '\n';
    for (const auto& attitude: attitudes) f << attitude.factionnum << '\n' << static_cast<int>(attitude.attitude) << '\n';

    f << known_local_quests.size() << '\n';
    for (int qnum : known_local_quests) f << qnum << '\n';
    f << quest_debts.size() << '\n';
    for (const auto& [region_num, tokens] : quest_debts) f << region_num << '\n' << tokens << '\n';

    // capital_region added in engine 5.2.10 (CAPITAL order / Trident victory mechanic).
    f << capital_region << '\n';
}

void Faction::Readin(std::istream& f, ATL_VER engine_version)
{
    f >> num;

    for (auto &ft : *FactionTypes) {
        f >> type[ft];
    }
    // Right now, the faction type data in the file is what determines npc/non-npc.
    // We should eventually persist this flag directly, but for now, let's just not break things.
    // Having it based on the faction type data is pretty bogus, but it's what we have for now.
    // A faction is an NPC if it has -1 for either MARTIAL or WAR (NPCs will actually have -1 for all types)
    is_npc = ((type[F_WAR] == -1) || (type[F_MARTIAL] == -1));

    f >> lastchange;
    f >> lastorders;
    f >> unclaimed;

    std::string str;
    std::getline(f >> std::ws, str);
    set_name(str | filter::strip_number);

    std::getline(f >> std::ws, address);

    std::getline(f >> std::ws, password);
    f >> times;
    f >> showunitattitudes;
    f >> temformat;

    skills.Readin(f);
    items.Readin(f);

    int defaultattitude;
    f >> defaultattitude;
    this->defaultattitude = static_cast<AttitudeType>(defaultattitude);

    int n;
    f >> n;
    for (int i = 0; i < n; i++) {
        int fnum, fattitude;
        f >> fnum >> fattitude;
        if (fnum == num) continue;
        Attitude a = { .factionnum = fnum, .attitude = static_cast<AttitudeType>(fattitude) };
        attitudes.push_back(a);
    }

    // quest-system fields added in engine 5.2.6; legacy saves leave containers empty.
    if (engine_version >= MAKE_ATL_VER(5, 2, 6)) {
        int kq_count;
        f >> kq_count;
        for (int i = 0; i < kq_count; i++) {
            int qnum;
            f >> qnum;
            known_local_quests.insert(qnum);
        }
        int qd_count;
        f >> qd_count;
        for (int i = 0; i < qd_count; i++) {
            int region_num, tokens;
            f >> region_num >> tokens;
            quest_debts[region_num] = tokens;
        }
    }

    // capital_region added in engine 5.2.10; legacy saves default to -1 (no capital).
    if (engine_version >= MAKE_ATL_VER(5, 2, 10)) {
        f >> capital_region;
    } else {
        capital_region = -1;
    }
}

void Faction::View()
{
    logger::write("Faction " + std::to_string(num) + " : " + name);
}

void Faction::set_name(const std::string& newname, bool canonicalize)
{
    if (newname.empty()) return;

    if (canonicalize) {
        std::string str = newname | filter::legal_characters;
        if (str.empty()) return;
        str += " (" + std::to_string(num) + ")";
        name = str;
    } else {
        name = newname;
    }
}

void Faction::set_address(const std::string& strNewAddress)
{
    address = strNewAddress;
}

std::vector<FactionStatistic> Faction::compute_faction_statistics(Game *game, size_t **citems) {
    std::vector<FactionStatistic> stats;
    // To allow testing, just return an empty vector if we don't have any item arrays inbound
    if (!citems) return stats;

    auto myfaction = this->num - 1; // offset by 1 since the citems array are 0-based and factions are 1-based.

    for (auto i = 0; i < NITEMS; i++) {
        if (ItemDefs[i].type & IT_SHIP) continue;
        if(citems[myfaction][i] == 0) continue;
        size_t place = 1;
        size_t max = 0;
        size_t total = 0;

        for (int pl = 0; pl < game->factionseq; pl++) {
            if (citems[pl][i] > citems[myfaction][i]) place++;
            if (max < citems[pl][i]) max = citems[pl][i];
            total += citems[pl][i];
        }

        size_t amt = citems[myfaction][i];
        bool illusory = (ItemDefs[i].type & IT_MONSTER) && (ItemDefs[i].type & IT_ILLUSION);
        std::string name = ItemDefs[i].name;
        std::string tag = ItemDefs[i].abr;
        std::string plural = ItemDefs[i].names;
        stats.push_back({
            .name = name, .tag = tag, .plural = plural, .amount = amt,
            .rank = place, .max = max, .total = total, .illusion = illusory
        });
    }
    return stats;
}

inline bool Faction::gets_gm_report(Game *game) {
    return is_npc && num == 1 && (Globals->GM_REPORT || (game->month == 0 && game->year == 1));
}

struct GmData {
    std::vector<ShowSkill> skills;
    std::vector<ShowItem> items;
    std::vector<ShowObject> objects;
};

static inline GmData collect_gm_data() {
    GmData data;
    for (auto i = 0; i < NSKILLS; i++)
        for (auto j = 1; j < 6; j++)
            data.skills.push_back({ .skill = i, .level = j });

    for (auto i = 0; i < NITEMS; i++) {
        data.items.push_back({.item = i, .full = true});
    }

    for (auto i = 1; i < NOBJECTS; i++) data.objects.push_back({.obj = i }); // skip O_NONE
    return data;
}

// Builds a JSON array of items that can be produced using a given skill at a given level.
// Includes normal production (pSkill/pLevel) and magical production (mSkill/mLevel).
// Each entry: { "item": "PARM", "output": 1, "man_months": 2, "materials": [{"item":"IRON","amount":2}] }
// Magical entries also have "magic": true.
static json build_skill_produces(const std::string& abbr, int level) {
    json produces = json::array();
    for (int i = 0; i < NITEMS; i++) {
        const auto& def = ItemDefs[i];
        if (def.flags & ItemType::DISABLED) continue;

        // Normal production
        if (def.pSkill && std::string(def.pSkill) == abbr && def.pLevel == level) {
            json materials = json::array();
            for (int m = 0; m < 4; m++) {
                if (def.pInput[m].item == -1) break;
                materials.push_back({
                    { "item", ItemDefs[def.pInput[m].item].abr },
                    { "amount", def.pInput[m].amt }
                });
            }
            produces.push_back({
                { "item", def.abr },
                { "output", def.pOut },
                { "man_months", def.pMonths },
                { "materials", materials }
            });
        }

        // Magical production
        if (def.mSkill && std::string(def.mSkill) == abbr && def.mLevel == level) {
            json materials = json::array();
            for (int m = 0; m < 4; m++) {
                if (def.mInput[m].item == -1) break;
                materials.push_back({
                    { "item", ItemDefs[def.mInput[m].item].abr },
                    { "amount", def.mInput[m].amt }
                });
            }
            produces.push_back({
                { "item", def.abr },
                { "output", def.mOut },
                { "man_months", 1 },
                { "materials", materials },
                { "magic", true }
            });
        }
    }
    return produces;
}

// Categorizes a structure type using ObjectDefs data.
// Used in both GM and player JSON report generation.
// Categories: fleet, ship, road, lair, military, production, other.
static std::string object_category(int obj) {
    auto& def = ObjectDefs[obj];
    if (def.flags & ObjectType::GROUP)      return "fleet";
    if (ObjectIsShip(obj))                  return "ship";
    if (def.name.rfind("Road ", 0) == 0)    return "road";
    if (def.monster != -1)                  return "lair";
    if (def.protect > 0)                    return "military";
    if (def.productionAided != -1)          return "production";
    return "other";
}

// Build human-readable flags array for object_reports (avoids bitwise ops in JS)
static json object_flags_json(int obj) {
    json flags = json::array();
    auto& def = ObjectDefs[obj];
    int f = def.flags;
    if (f & ObjectType::DISABLED)         flags.push_back("disabled");
    if (f & ObjectType::NOMONSTERGROWTH)  flags.push_back("no_monster_growth");
    if (f & ObjectType::NEVERDECAY)       flags.push_back("never_decay");
    if (f & ObjectType::CANENTER)         flags.push_back("can_enter");
    if (f & ObjectType::CANMODIFY)        flags.push_back("can_modify");
    if (f & ObjectType::TRANSPORT)        flags.push_back("transport");
    if (f & ObjectType::GROUP)            flags.push_back("group");
    if (f & ObjectType::KEYBARRIER)       flags.push_back("key_barrier");
    if (f & ObjectType::SACRIFICE)        flags.push_back("sacrifice");
    if (f & ObjectType::GRANTSKILL)       flags.push_back("grant_skill");
    if (f & ObjectType::NOANNIHILATE)     flags.push_back("no_annihilate");
    if (f & ObjectType::SETTLEMENT_ONLY)  flags.push_back("settlement_only");
    if (f & ObjectType::ONE_PER_REGION)   flags.push_back("one_per_region");
    if (f & ObjectType::CANAL)            flags.push_back("canal");
    return flags;
}

// Build structured build-info for object_reports.
// Returns nullptr (json null) when object cannot be built by players.
// Mirrors object_description() buildability logic in object.cpp.
static json object_build_json(int obj) {
    auto& def = ObjectDefs[obj];
    // item == -1 means no material required; only GROUP objects (Fleet) are
    // buildable in that case (object.cpp line 800 skips "cannot be built" for GROUP).
    if (def.item == -1 && !(def.flags & ObjectType::GROUP)) return nullptr;
    if (def.item == -1 &&  (def.flags & ObjectType::GROUP)) {
        // Fleet — free build, no material
        return json{{"item", nullptr}, {"cost", 0}, {"skill", nullptr}, {"level", 0}};
    }
    json b;
    if (def.item == I_WOOD_OR_STONE) {
        b["item"] = "wood or stone"; // player chooses wood or stone
    } else {
        b["item"] = ItemDefs[def.item].abr;
    }
    b["cost"] = def.cost;
    b["skill"] = (def.skill && def.skill[0]) ? json(def.skill) : json(nullptr);
    b["level"] = def.level;
    return b;
}

void Faction::build_gm_json_report(json& j, Game *game) {
    GmData data = collect_gm_data();

    j["engine"] = {
        { "version", ATL_VER_STRING(CURRENT_ATL_VER) },
        { "ruleset", Globals->RULESET_NAME },
        { "ruleset_version", ATL_VER_STRING(Globals->RULESET_VERSION) },
        { "json_report_version", ATL_VER_STR(JSON_REPORT_VERSION) }
    };
    if (game->worldId != "none" && !game->worldId.empty()) {
        j["engine"]["world_id"] = game->worldId;
    }
    j["name"] = name | filter::strip_number;
    j["number"] = num;
    j["date"] = {
        { "month", MonthNames[game->month] },
        { "year", game->year }
    };

    json skills = json::array();
    for (auto &skillshow : data.skills) {
        std::string skill_name = SkillDefs[skillshow.skill].name;
        std::string abbr = SkillDefs[skillshow.skill].abbr;
        std::string description = skillshow.Report(this);
        if (description.empty()) continue;
        // Build human-readable flags array for frontend (avoids bitwise ops in JS)
        json skill_flags = json::array();
        int sflags = SkillDefs[skillshow.skill].flags;
        if (sflags & SkillType::MAGIC)      skill_flags.push_back("magic");
        if (sflags & SkillType::COMBAT)     skill_flags.push_back("combat");
        if (sflags & SkillType::CAST)       skill_flags.push_back("cast");
        if (sflags & SkillType::FOUNDATION) skill_flags.push_back("foundation");
        if (sflags & SkillType::APPRENTICE) skill_flags.push_back("apprentice");
        if (sflags & SkillType::DISABLED)   skill_flags.push_back("disabled");
        if (sflags & SkillType::SLOWSTUDY)  skill_flags.push_back("slow_study");
        if (sflags & SkillType::BATTLEREP)  skill_flags.push_back("battle_report");
        if (sflags & SkillType::NOTIFY)     skill_flags.push_back("notify");
        if (sflags & SkillType::DAMAGE)     skill_flags.push_back("damage");
        if (sflags & SkillType::FEAR)       skill_flags.push_back("fear");
        if (sflags & SkillType::MAGEOTHER)  skill_flags.push_back("mage_other");
        if (sflags & SkillType::NOSTUDY)    skill_flags.push_back("no_study");
        if (sflags & SkillType::NOTEACH)    skill_flags.push_back("no_teach");
        if (sflags & SkillType::NOEXP)      skill_flags.push_back("no_exp");
        if (sflags & SkillType::GRANTED)    skill_flags.push_back("granted");

        // Skill dependencies for study
        auto& sdeps = SkillDefs[skillshow.skill].depends;
        json depends = json::array();
        for (auto& d : sdeps) {
            if (d.skill && d.skill[0])
                depends.push_back({{"skill", d.skill}, {"level", d.level}});
        }

        json produces = build_skill_produces(abbr, skillshow.level);
        json skill_entry = {
            { "name", skill_name }, { "tag", abbr }, { "level", skillshow.level },
            { "flags", skill_flags },
            { "cost", SkillDefs[skillshow.skill].cost },
            { "description", description }
        };
        if (!produces.empty()) skill_entry["produces"] = produces;
        if (!depends.empty()) skill_entry["depends"] = depends;
        if (SkillDefs[skillshow.skill].special)
            skill_entry["special"] = SkillDefs[skillshow.skill].special.value();
        if (SkillDefs[skillshow.skill].range)
            skill_entry["range"] = SkillDefs[skillshow.skill].range.value();
        skills.push_back(skill_entry);
    }
    j["skill_reports"] = skills;

    json items = json::array();
    for (auto &itemshow : data.items) {
        int ii = itemshow.item;
        std::string item_name = itemshow.display_name();
        std::string tag = itemshow.display_tag();
        std::string description = item_description(ii, itemshow.full);
        if (description.empty()) continue;
        // Build human-readable types array for frontend
        json item_types = json::array();
        int itype = ItemDefs[ii].type;
        if (itype & IT_NORMAL)   item_types.push_back("normal");
        if (itype & IT_ADVANCED) item_types.push_back("advanced");
        if (itype & IT_TRADE)    item_types.push_back("trade");
        if (itype & IT_MAN)      item_types.push_back("man");
        if (itype & IT_MONSTER)  item_types.push_back("monster");
        if (itype & IT_MAGIC)    item_types.push_back("magic");
        if (itype & IT_WEAPON)   item_types.push_back("weapon");
        if (itype & IT_ARMOR)    item_types.push_back("armor");
        if (itype & IT_MOUNT)    item_types.push_back("mount");
        if (itype & IT_BATTLE)   item_types.push_back("battle");
        if (itype & IT_TOOL)     item_types.push_back("tool");
        if (itype & IT_FOOD)     item_types.push_back("food");
        if (itype & IT_ILLUSION) item_types.push_back("illusion");
        if (itype & IT_UNDEAD)   item_types.push_back("undead");
        if (itype & IT_DEMON)    item_types.push_back("demon");
        if (itype & IT_LEADER)   item_types.push_back("leader");
        if (itype & IT_MONEY)    item_types.push_back("money");
        if (itype & IT_ANIMAL)   item_types.push_back("animal");
        if (itype & IT_SHIP)     item_types.push_back("ship");

        // Build structured stats from engine data tables (eliminates backend regex parsing).
        json item_stats = json::object();

        if (itype & IT_MONSTER) {
            auto mon_opt = find_monster(ItemDefs[ii].abr, (itype & IT_ILLUSION) ? 1 : 0);
            if (mon_opt) {
                auto &m = mon_opt->get();
                json def_arr = json::array();
                for (int i = 0; i < NUM_ATTACK_TYPES; i++) def_arr.push_back(m.defense[i]);
                json preferred = json::array();
                for (auto t : m.preferredTerrain) preferred.push_back(TerrainDefs[t].name);
                json forbidden = json::array();
                for (auto t : m.forbiddenTerrain) forbidden.push_back(TerrainDefs[t].name);
                item_stats = {
                    {"size",              m.size},
                    {"attack",            m.attackLevel},
                    {"defense",           def_arr},
                    {"hp",                m.hits},
                    {"attacks_per_round", m.numAttacks},
                    {"damage_per_attack", m.hitDamage},
                    {"tactics",           m.tactics},
                    {"stealth",           m.stealth},
                    {"observation",       m.obs},
                    {"preferred_terrain", preferred},
                    {"forbidden_terrain", forbidden},
                    {"free_roamer",       m.preferredTerrain.empty() && m.forbiddenTerrain.empty()},
                    {"regen",             m.regen},
                    {"hostile",           m.hostile},
                    {"silver",            m.silver},
                    {"spoiltype",         m.spoiltype == -1 ? json(nullptr) : json(ItemDefs[m.spoiltype].abr)},
                    {"number",            m.number},
                };
                if (m.special) item_stats["special"]       = m.special;
                if (m.special) item_stats["special_level"]  = m.specialLevel;
            }
        } else if (itype & IT_WEAPON) {
            auto wp_opt = find_weapon(ItemDefs[ii].abr);
            if (wp_opt) {
                auto &w = wp_opt->get();
                static const char* WEAP_CLASS_NAMES[NUM_WEAPON_CLASSES] = {
                    "slashing", "piercing", "crushing", "cleaving",
                    "armor-piercing", "energy", "spirit", "weather"
                };
                item_stats = {
                    {"is_ranged",     (w.flags & WeaponType::RANGED) != 0},
                    {"damage_type",   (w.weapClass >= 0 && w.weapClass < NUM_WEAPON_CLASSES)
                                          ? WEAP_CLASS_NAMES[w.weapClass] : "unknown"},
                    {"attack_bonus",  w.attackBonus},
                    {"defense_bonus", w.defenseBonus},
                    {"num_attacks",   w.numAttacks},
                    {"mount_bonus",   w.mountBonus},
                };
            }
        } else if (itype & IT_ARMOR) {
            auto arm_opt = find_armor(ItemDefs[ii].abr);
            if (arm_opt) {
                auto &a = arm_opt->get();
                json saves = json::array();
                for (int i = 0; i < NUM_WEAPON_CLASSES; i++)
                    saves.push_back(a.from > 0 ? (a.saves[i] * 100 / a.from) : 0);
                // saves order: [slashing%, piercing%, crushing%, cleaving%, armor-piercing%, energy%, spirit%, weather%]
                item_stats = {
                    {"saves",          saves},
                    {"attack_bonus",   a.attackBonus},
                    {"defense_bonus",  a.defenseBonus},
                };
            }
        } else if (itype & IT_BATTLE) {
            // Non-weapon battle items (shields, misc combat items)
            auto bi_opt = find_battle_item(ItemDefs[ii].abr);
            if (bi_opt) {
                auto &b = bi_opt->get();
                item_stats = {
                    {"skill_level",    b.skillLevel},
                    {"attack_penalty", b.attackPenalty},
                };
            }
        }

        // Race stats (IT_MAN can coexist with other flags)
        if (itype & IT_MAN) {
            auto race_opt = find_race(ItemDefs[ii].abr);
            if (race_opt) {
                auto &r = race_opt->get();
                json skills = json::array();
                for (auto &sk : r.skills)
                    if (sk.has_value()) skills.push_back(sk.value());
                item_stats["special_skills"]  = skills;
                item_stats["special_level"]   = r.speciallevel;
                item_stats["default_level"]   = r.defaultlevel;
                item_stats["size"]            = r.size;
            }
        }

        // Universal structured fields (v1.1+)
        json capacity = {{"walk", ItemDefs[ii].walk}, {"ride", ItemDefs[ii].ride},
                          {"fly", ItemDefs[ii].fly}, {"swim", ItemDefs[ii].swim}};

        json item_entry = {
            {"name", item_name}, {"tag", tag}, {"types", item_types}, {"description", description},
            {"weight", ItemDefs[ii].weight},
            {"baseprice", ItemDefs[ii].baseprice},
            {"capacity", capacity},
            {"speed", ItemDefs[ii].speed},
        };
        if (!item_stats.empty()) item_entry["_stats"] = item_stats;
        items.push_back(item_entry);
    }
    j["item_reports"] = items;

    json objects = json::array();
    for (auto &objectshow : data.objects) {
        int oi = objectshow.obj;
        std::string obj_name = ObjectDefs[oi].name;
        std::string description = object_description(oi);
        if (description.empty()) continue;
        auto& def = ObjectDefs[oi];
        json defense = json::array();
        for (int i = 0; i < NUM_ATTACK_TYPES; i++)
            defense.push_back(def.defenceArray[i]);
        json obj_entry = {
            { "name", obj_name },
            { "category", object_category(oi) },
            { "description", description },
            { "flags", object_flags_json(oi) },
            { "protect", def.protect },
            { "max_mages", def.maxMages },
            { "defense", defense },
            { "build", object_build_json(oi) }
        };
        objects.push_back(obj_entry);
    }
    j["object_reports"] = objects;

    present_regions.clear();
    for(const auto reg : game->regions) {
        present_regions.push_back(reg);
    }
    json regions = json::array();
    for (const auto& reg: present_regions) {
        json region;
        reg->build_json_report(region, this, game->month, game->regions);
        regions.push_back(region);
    }
    j["regions"] = regions;
    present_regions.clear();

    errors.clear();
    events.clear();
    battles.clear();
}

void Faction::build_json_report(json& j, Game *game, size_t **citems) {
    if (gets_gm_report(game)) {
        build_gm_json_report(j, game);
        return;
    }

    if (Globals->FACTION_STATISTICS) {
        j["statistics"] = compute_faction_statistics(game, citems);
    }

    // This can be better, but for now..
    j["engine"] = {
        { "version", ATL_VER_STRING(CURRENT_ATL_VER) },
        { "ruleset", Globals->RULESET_NAME },
        { "ruleset_version", ATL_VER_STRING(Globals->RULESET_VERSION) },
        { "json_report_version", ATL_VER_STR(JSON_REPORT_VERSION) }
    };
    if (game->worldId != "none" && !game->worldId.empty()) {
        j["engine"]["world_id"] = game->worldId;
    }

    j["name"] = name | filter::strip_number;
    j["number"] = num;
    if (Globals->FACTION_LIMIT_TYPE == GameDefs::FACLIM_FACTION_TYPES) {
        j["type"] = json::object();
        for (auto &ft : *FactionTypes) {
            std::string factype = ft | filter::lowercase;
            if (type[ft]) j["type"][factype] = type[ft];
        }
    }
    j["administrative"]["times_sent"] = (times != 0);
    bool password_unset = (password == "none");
    j["administrative"]["password_unset"] = password_unset;
    j["administrative"]["email"] = address;
    j["administrative"]["show_unit_attitudes"] = (showunitattitudes != 0);

    if(!password_unset) j["administrative"]["password"] = password;
    if(Globals->MAX_INACTIVE_TURNS != -1) {
        int cturn = game->TurnNumber() - lastorders;
        if ((cturn >= (Globals->MAX_INACTIVE_TURNS - 3)) && !is_npc) {
            cturn = Globals->MAX_INACTIVE_TURNS - cturn;
            j["administrative"]["inactivity_deletion_turns"] = cturn;
        }
    }
    if(!exists) {
        j["administrative"]["quit"] = QuitStrs[quit ? quit : QUIT_BY_ORDER];
    }
    j["date"] = {
        { "month", MonthNames[game->month] },
        { "year", game->year }
    };

    if ((Globals->FACTION_LIMIT_TYPE == GameDefs::FACLIM_MAGE_COUNT)
        || (Globals->FACTION_LIMIT_TYPE == GameDefs::FACLIM_FACTION_TYPES)) {
        j["status"]["mages"] = {
            { "current", nummages },
            { "allowed", game->AllowedMages(this) }
        };

        if (Globals->APPRENTICES_EXIST) {
            std::string name = Globals->APPRENTICE_NAME;
            name[0] = toupper(name[0]);
            j["status"]["apprentices"] = {
                { "current", numapprentices },
                { "allowed", game->AllowedApprentices(this) },
                { "name", name }
            };
        }
    }
    if (Globals->FACTION_LIMIT_TYPE == GameDefs::FACLIM_FACTION_TYPES) {
        if (Globals->FACTION_ACTIVITY != FactionActivityRules::DEFAULT) {
            int currentCost = GetActivityCost(FactionActivity::TAX);
            int maxAllowedCost = game->AllowedMartial(this);
            bool isMerged = Globals->FACTION_ACTIVITY == FactionActivityRules::MARTIAL_MERGED;
            j["status"][(isMerged ? "regions" : "activity")] = {
                { "current", currentCost },
                { "allowed", maxAllowedCost }
            };
        } else {
            j["status"]["tax_regions"] = {
                { "current", GetActivityCost(FactionActivity::TAX) },
                { "allowed", game->AllowedTaxes(this) }
            };
            j["status"]["trade_regions"] = {
                { "current", GetActivityCost(FactionActivity::TRADE) },
                { "allowed", game->AllowedTrades(this) }
            };
        }
        if (Globals->TRANSPORT & GameDefs::ALLOW_TRANSPORT) {
            j["status"]["quartermasters"] = {
                { "current", numqms },
                { "allowed", game->AllowedQuarterMasters(this) }
            };
        }
        if (Globals->TACTICS_NEEDS_WAR) {
            j["status"]["tacticians"] = {
                { "current", numtacts },
                { "allowed", game->AllowedTacticians(this) }
            };
        }
    }

    if (!errors.empty()) {
        // Handle errors better.  For now, just put them into an 'errors' vector.
        // We could give nice json'y output like which unit, which region, etc.
        j["errors"] = errors;
    }

    if (!battles.empty()) {
        json jbattles = json::array();
        for (const auto& battle: battles) {
            json jbattle = json::object();
            // we will obviously want to make this into json-y output
            battle->build_json_report(jbattle, this);
            jbattles.push_back(jbattle);
        }
        j["battles"] = jbattles;
    }

    if (!events.empty()) {
        // events can also be handled as json objects rather than just strings.
        j["events"] = events;
    }

    /* Attitudes */
    j["attitudes"] = json::object();
    j["attitudes"]["default"] = AttitudeStrs[static_cast<int>(defaultattitude)] | filter::lowercase;
    for (int i=0; i<static_cast<int>(AttitudeType::ATTITUDE_COUNT); i++) {
        std::string attitude = AttitudeStrs[i] | filter::lowercase;
        j["attitudes"][attitude] = json::array(); // [] = json::array();
        for (const auto& a: attitudes) {
            if (a.attitude == static_cast<AttitudeType>(i)) {
                // Grab that faction so we can get it's number and name, and strip the " (num)" from the name for json
                Faction *fac = GetFaction(game->factions, a.factionnum);
                j["attitudes"][attitude].push_back({
                    { "name", fac->name | filter::strip_number }, { "number", a.factionnum }
                });
            }
        }
        // the array will be empty if this faction has declared no other factions with that specific attitude.
    }

    j["unclaimed_silver"] = unclaimed;

    // --- Bounty quests ---
    {
        // Compass-style offset from anchor (mayor's city) to target region.
        // Strict rule: pure side (north/south/east/west) only when one axis is 0.
        // Empty string if same region, null inputs, or different map level.
        // X axis is cylindrical (wraps); Y axis does not wrap.
        auto describe_offset = [](ARegion *target, ARegion *anchor) -> std::string {
            if (!target || !anchor || target == anchor) return "";
            if (target->zloc != anchor->zloc) return "";

            int w  = anchor->level->x;
            int dx = target->xloc - anchor->xloc;
            if (dx >  w / 2) dx -= w;
            if (dx < -w / 2) dx += w;
            int dy = target->yloc - anchor->yloc;

            int ax = std::abs(dx), ay = std::abs(dy);
            int dist = ax + std::max(0, (ay - ax) / 2);
            if (dist == 0) return "";

            const char *dir;
            if      (dx == 0)          dir = (dy < 0) ? "north" : "south";
            else if (dy == 0)          dir = (dx < 0) ? "west"  : "east";
            else if (dx < 0 && dy < 0) dir = "northwest";
            else if (dx > 0 && dy < 0) dir = "northeast";
            else if (dx < 0 && dy > 0) dir = "southwest";
            else                       dir = "southeast";

            return std::to_string(dist) + " hexes " + dir;
        };

        auto turn_ym = [&](int turn) -> json {
            int year  = (turn - 1) / 12 + 1;
            int month = (turn - 1) % 12;
            return json{{"turn", turn}, {"year", year}, {"month", MonthNames[month]}};
        };

        auto quest_desc = [&](const std::shared_ptr<Quest>& q) -> std::string {
            ARegion *ir = (q->issuer_region >= 0)
                          ? game->regions.GetRegion(q->issuer_region) : nullptr;
            std::string settlement = (ir && ir->town) ? ir->town->name
                                   : (ir ? ir->name : "unknown");
            if (q->subtype == Quest::LOCAL_HUNT) {
                Location *loc = game->regions.FindUnit(q->target);
                ARegion  *tr  = loc ? loc->region
                              : (q->regionnum != -1 ? game->regions.GetRegion(q->regionnum) : ir);
                std::string terrain = tr ? TerrainDefs[TerrainDefs[tr->type].similar_type].name : "region";
                std::string rname   = tr ? tr->name : "unknown";
                // GetMonsterDisplayName() already includes (unit_num) — don't add it again.
                std::string uname   = loc ? loc->unit->GetMonsterDisplayName() : "unknown creature";
                return "In the " + terrain + " of " + rname + " roams the " + uname +
                       ". The mayor of " + settlement + " offers a bounty for its defeat!";
            }
            if (q->subtype == Quest::LOCAL_LAIR_CLEAR) {
                // Dungeon quests store -(DungeonType index + 1) in building (always < 0).
                // Lair quests store ObjectDefs index (always >= 0).
                bool is_dungeon = (q->building < 0);
                if (is_dungeon) {
                    int dtype = -(q->building + 1);
                    std::string ename = DungeonTypeDefs[dtype].entrance_name;
                    Location *bloc = game->regions.FindUnit(q->target);
                    std::string boss = bloc ? bloc->unit->GetMonsterDisplayName() : "";
                    std::string article = (!ename.empty() &&
                        std::string("AEIOUaeiou").find(ename[0]) != std::string::npos)
                        ? "An" : "A";
                    std::string base = article + " " + ename + " has appeared near " + settlement + ".";
                    if (!boss.empty())
                        base += " Somewhere within lurks " + boss + ".";
                    return base + " The mayor of " + settlement + " seeks heroes to storm the dungeon!";
                }
                std::string lair_name = (q->building >= 0 && q->building < NOBJECTS)
                                        ? ObjectDefs[q->building].name : "Lair";
                ARegion *lr = (q->regionnum != -1) ? game->regions.GetRegion(q->regionnum) : ir;
                std::string lname = lr ? lr->name : "unknown";
                Location *loc = game->regions.FindUnit(q->target);
                // Use base name only — unit number would look like a dungeon ID.
                std::string uname = "dangerous creatures";
                if (loc) {
                    std::string full = loc->unit->GetMonsterDisplayName();
                    // Strip trailing " (num)" to avoid confusion with dungeon numbering.
                    auto paren = full.rfind(" (");
                    uname = (paren != std::string::npos) ? full.substr(0, paren) : full;
                }
                return "The " + lair_name + " in " + lname + " harbors " + uname +
                       ". The mayor of " + settlement + " seeks adventurers to clear it!";
            }
            if (q->subtype == Quest::LOCAL_BUILD_TOWER) {
                ARegion *tr = (q->regionnum >= 0)
                              ? game->regions.GetRegion(q->regionnum) : nullptr;
                std::string tname = tr ? tr->name : "unknown";
                std::string terr  = tr ? TerrainDefs[TerrainDefs[tr->type].similar_type].name
                                       : "region";
                std::string text  = "The " + terr + " of " + tname + " lacks a watchtower. The mayor of " +
                                    settlement + " offers a bounty to any faction that builds one there.";
                std::string off = describe_offset(tr, ir);
                if (!off.empty())
                    text += " The site lies " + off + " of " + settlement + ".";
                return text;
            }
            if (q->subtype == Quest::LOCAL_BUILD_INN) {
                ARegion *tr = (q->regionnum >= 0)
                              ? game->regions.GetRegion(q->regionnum) : nullptr;
                std::string tname = tr ? tr->name : "unknown";
                std::string terr  = tr ? TerrainDefs[TerrainDefs[tr->type].similar_type].name
                                       : "region";
                std::string text  = "Travelers passing through the " + terr + " of " + tname +
                                    " have nowhere to rest. The mayor of " + settlement +
                                    " offers a bounty to any faction that builds an Inn there.";
                std::string off = describe_offset(tr, ir);
                if (!off.empty())
                    text += " The site lies " + off + " of " + settlement + ".";
                return text;
            }
            if (q->subtype == Quest::LOCAL_BUILD_ROAD) {
                // regionnum = region to build in; regionname = destination settlement name.
                ARegion *build_r = (q->regionnum >= 0)
                                   ? game->regions.GetRegion(q->regionnum) : nullptr;
                std::string build_name = build_r ? build_r->name : "unknown";
                std::string build_terr = build_r
                    ? TerrainDefs[TerrainDefs[build_r->type].similar_type].name : "region";
                std::string dest_name = q->regionname.empty() ? "unknown" : q->regionname;
                static const struct { int obj; const char *dir; } road_dir_map[] = {
                    { O_ROADN,  "North"     }, { O_ROADNE, "Northeast" },
                    { O_ROADSE, "Southeast" }, { O_ROADS,  "South"     },
                    { O_ROADSW, "Southwest" }, { O_ROADNW, "Northwest" },
                };
                std::string dir_name = "Road";
                for (const auto& m : road_dir_map)
                    if (m.obj == q->building) { dir_name = m.dir; break; }
                return "Build a Road " + dir_name + " in the " + build_terr + " of " +
                       build_name + " toward " + dest_name +
                       ". The mayor of " + settlement + " offers a bounty for completing this road!";
            }
            if (q->subtype == Quest::GLOBAL_BOSS_HUNT) {
                Location *loc = game->regions.FindUnit(q->target);
                if (loc) {
                    Unit    *cap = loc->unit;
                    ARegion *r   = loc->region;
                    // cap->object->name already contains "[num]"; cap->name already
                    // contains "(num)" — don't add them again.
                    std::string sname = cap->object ? cap->object->name : "pirate galley";
                    std::string terr  = TerrainDefs[TerrainDefs[r->type].similar_type].name;
                    return "The pirate galley " + sname + " under " + cap->name +
                           " was last sighted in the " + terr + " of " + r->name + ".";
                }
                return "A dangerous pirate captain (" + std::to_string(q->target) + ") is at large.";
            }
            return "Unknown quest.";
        };

        json local_arr  = json::array();
        json global_arr = json::array();

        for (const auto& q : quests) {
            json entry;
            entry["num"]         = q->num;
            entry["subtype"]     = q->subtype;
            entry["description"] = quest_desc(q);
            entry["tokens"]      = q->tokens;
            entry["posted"]      = turn_ym(q->created_turn);

            if (q->scope == Quest::SCOPE_LOCAL) {
                if (!known_local_quests.count(q->num)) continue;
                ARegion *ir = game->regions.GetRegion(q->issuer_region);
                if (ir && ir->town) entry["settlement"] = ir->town->name;
                else if (ir)        entry["settlement"] = ir->name;
                if (ir) { entry["x"] = ir->xloc; entry["y"] = ir->yloc; }
                entry["expires"] = turn_ym(q->expires_turn);

                // Dungeon quests: also show when entrance closes.
                bool is_dungeon = (q->subtype == Quest::LOCAL_LAIR_CLEAR && q->building < 0);
                if (is_dungeon) {
                    int dtype = -(q->building + 1);
                    // Locate matching dungeon by surface region (regionnum) and dungeon type.
                    for (const auto& d : game->activeDungeons) {
                        if (d.surface_region_num == q->regionnum &&
                            static_cast<int>(d.type) == dtype) {
                            int close_turn = d.spawn_turn +
                                             DungeonTypeDefs[(int)d.type].max_lifetime_turns;
                            entry["entrance_closes"] = turn_ym(close_turn);
                            break;
                        }
                    }
                }
                local_arr.push_back(entry);
            } else if (q->scope == Quest::SCOPE_GLOBAL) {
                global_arr.push_back(entry);
            }
        }

        json bq;
        bq["local"]  = local_arr;
        bq["global"] = global_arr;
        j["bounty_quests"] = bq;

        // Quest debts — always shown so player can plan token logistics.
        json debts = json::array();
        for (const auto& [rnum, tokens] : quest_debts) {
            if (tokens <= 0) continue;
            json debt;
            debt["tokens"] = tokens;
            if (rnum == -1) {
                debt["global"] = true;
            } else {
                ARegion *r = game->regions.GetRegion(rnum);
                if (r && r->town) debt["region"] = r->town->name;
                else if (r)       debt["region"] = r->name;
                if (r) { debt["x"] = r->xloc; debt["y"] = r->yloc; }
            }
            debts.push_back(debt);
        }
        if (!debts.empty()) j["quest_debts"] = debts;
    }

    json skills = json::array();
    for (auto &skillshow : shows) {
        std::string skill_name = SkillDefs[skillshow.skill].name;
        std::string abbr = SkillDefs[skillshow.skill].abbr;
        std::string description = skillshow.Report(this);
        if (description.empty()) continue;
        json skill_flags = json::array();
        int sflags = SkillDefs[skillshow.skill].flags;
        if (sflags & SkillType::MAGIC)      skill_flags.push_back("magic");
        if (sflags & SkillType::COMBAT)     skill_flags.push_back("combat");
        if (sflags & SkillType::CAST)       skill_flags.push_back("cast");
        if (sflags & SkillType::FOUNDATION) skill_flags.push_back("foundation");
        if (sflags & SkillType::APPRENTICE) skill_flags.push_back("apprentice");
        json produces = build_skill_produces(abbr, skillshow.level);
        json skill_entry = {
            { "name", skill_name }, { "tag", abbr }, { "level", skillshow.level },
            { "flags", skill_flags }, { "description", description }
        };
        if (!produces.empty()) skill_entry["produces"] = produces;
        skills.push_back(skill_entry);
    }
    j["skill_reports"] = skills;

    json items = json::array();
    for (auto &itemshow : itemshows) {
        int ii = itemshow.item;
        std::string item_name = itemshow.display_name();
        std::string tag = itemshow.display_tag();
        std::string description = item_description(ii, itemshow.full);
        if (description.empty()) continue;
        json item_types = json::array();
        int itype = ItemDefs[ii].type;
        if (itype & IT_NORMAL)   item_types.push_back("normal");
        if (itype & IT_ADVANCED) item_types.push_back("advanced");
        if (itype & IT_TRADE)    item_types.push_back("trade");
        if (itype & IT_MAN)      item_types.push_back("man");
        if (itype & IT_MONSTER)  item_types.push_back("monster");
        if (itype & IT_MAGIC)    item_types.push_back("magic");
        if (itype & IT_WEAPON)   item_types.push_back("weapon");
        if (itype & IT_ARMOR)    item_types.push_back("armor");
        if (itype & IT_MOUNT)    item_types.push_back("mount");
        if (itype & IT_BATTLE)   item_types.push_back("battle");
        if (itype & IT_TOOL)     item_types.push_back("tool");
        if (itype & IT_FOOD)     item_types.push_back("food");
        if (itype & IT_ILLUSION) item_types.push_back("illusion");
        if (itype & IT_UNDEAD)   item_types.push_back("undead");
        if (itype & IT_DEMON)    item_types.push_back("demon");
        if (itype & IT_LEADER)   item_types.push_back("leader");
        if (itype & IT_MONEY)    item_types.push_back("money");
        if (itype & IT_ANIMAL)   item_types.push_back("animal");
        if (itype & IT_SHIP)     item_types.push_back("ship");

        // Build structured stats from engine data tables (eliminates backend regex parsing).
        json item_stats = json::object();

        if (itype & IT_MONSTER) {
            auto mon_opt = find_monster(ItemDefs[ii].abr, (itype & IT_ILLUSION) ? 1 : 0);
            if (mon_opt) {
                auto &m = mon_opt->get();
                json def_arr = json::array();
                for (int i = 0; i < NUM_ATTACK_TYPES; i++) def_arr.push_back(m.defense[i]);
                json preferred = json::array();
                for (auto t : m.preferredTerrain) preferred.push_back(TerrainDefs[t].name);
                json forbidden = json::array();
                for (auto t : m.forbiddenTerrain) forbidden.push_back(TerrainDefs[t].name);
                item_stats = {
                    {"size",              m.size},
                    {"attack",            m.attackLevel},
                    {"defense",           def_arr},
                    {"hp",                m.hits},
                    {"attacks_per_round", m.numAttacks},
                    {"damage_per_attack", m.hitDamage},
                    {"tactics",           m.tactics},
                    {"stealth",           m.stealth},
                    {"observation",       m.obs},
                    {"preferred_terrain", preferred},
                    {"forbidden_terrain", forbidden},
                    {"free_roamer",       m.preferredTerrain.empty() && m.forbiddenTerrain.empty()},
                    {"regen",             m.regen},
                    {"hostile",           m.hostile},
                    {"silver",            m.silver},
                    {"spoiltype",         m.spoiltype == -1 ? json(nullptr) : json(ItemDefs[m.spoiltype].abr)},
                    {"number",            m.number},
                };
                if (m.special) item_stats["special"]       = m.special;
                if (m.special) item_stats["special_level"]  = m.specialLevel;
            }
        } else if (itype & IT_WEAPON) {
            auto wp_opt = find_weapon(ItemDefs[ii].abr);
            if (wp_opt) {
                auto &w = wp_opt->get();
                static const char* WEAP_CLASS_NAMES[NUM_WEAPON_CLASSES] = {
                    "slashing", "piercing", "crushing", "cleaving",
                    "armor-piercing", "energy", "spirit", "weather"
                };
                item_stats = {
                    {"is_ranged",     (w.flags & WeaponType::RANGED) != 0},
                    {"damage_type",   (w.weapClass >= 0 && w.weapClass < NUM_WEAPON_CLASSES)
                                          ? WEAP_CLASS_NAMES[w.weapClass] : "unknown"},
                    {"attack_bonus",  w.attackBonus},
                    {"defense_bonus", w.defenseBonus},
                    {"num_attacks",   w.numAttacks},
                    {"mount_bonus",   w.mountBonus},
                };
            }
        } else if (itype & IT_ARMOR) {
            auto arm_opt = find_armor(ItemDefs[ii].abr);
            if (arm_opt) {
                auto &a = arm_opt->get();
                json saves = json::array();
                for (int i = 0; i < NUM_WEAPON_CLASSES; i++)
                    saves.push_back(a.from > 0 ? (a.saves[i] * 100 / a.from) : 0);
                // saves order: [slashing%, piercing%, crushing%, cleaving%, armor-piercing%, energy%, spirit%, weather%]
                item_stats = {
                    {"saves",          saves},
                    {"attack_bonus",   a.attackBonus},
                    {"defense_bonus",  a.defenseBonus},
                };
            }
        } else if (itype & IT_BATTLE) {
            // Non-weapon battle items (shields, misc combat items)
            auto bi_opt = find_battle_item(ItemDefs[ii].abr);
            if (bi_opt) {
                auto &b = bi_opt->get();
                item_stats = {
                    {"skill_level",    b.skillLevel},
                    {"attack_penalty", b.attackPenalty},
                };
            }
        }

        // Race stats (IT_MAN can coexist with other flags)
        if (itype & IT_MAN) {
            auto race_opt = find_race(ItemDefs[ii].abr);
            if (race_opt) {
                auto &r = race_opt->get();
                json skills = json::array();
                for (auto &sk : r.skills)
                    if (sk.has_value()) skills.push_back(sk.value());
                item_stats["special_skills"]  = skills;
                item_stats["special_level"]   = r.speciallevel;
                item_stats["default_level"]   = r.defaultlevel;
                item_stats["size"]            = r.size;
            }
        }

        // Universal structured fields (v1.1+)
        json capacity = {{"walk", ItemDefs[ii].walk}, {"ride", ItemDefs[ii].ride},
                          {"fly", ItemDefs[ii].fly}, {"swim", ItemDefs[ii].swim}};

        json item_entry = {
            {"name", item_name}, {"tag", tag}, {"types", item_types}, {"description", description},
            {"weight", ItemDefs[ii].weight},
            {"baseprice", ItemDefs[ii].baseprice},
            {"capacity", capacity},
            {"speed", ItemDefs[ii].speed},
        };
        if (!item_stats.empty()) item_entry["_stats"] = item_stats;
        items.push_back(item_entry);
    }
    j["item_reports"] = items;

    json objects = json::array();
    for(const auto objectshow : objectshows) {
        int oi = objectshow.obj;
        std::string obj_name = ObjectDefs[oi].name;
        std::string description = object_description(oi);
        if(description.empty()) continue;
        auto& def = ObjectDefs[oi];
        json defense = json::array();
        for (int i = 0; i < NUM_ATTACK_TYPES; i++)
            defense.push_back(def.defenceArray[i]);
        json obj_entry = {
            { "name", obj_name },
            { "category", object_category(oi) },
            { "description", description },
            { "flags", object_flags_json(oi) },
            { "protect", def.protect },
            { "max_mages", def.maxMages },
            { "defense", defense },
            { "build", object_build_json(oi) }
        };
        objects.push_back(obj_entry);
    }
    j["object_reports"] = objects;

    // regions
    json regions = json::array();
    for (const auto& reg: present_regions) {
        json region;
        reg->build_json_report(region, this, game->month, game->regions);
        regions.push_back(region);
    }
    j["regions"] = regions;
}

void Faction::WriteFacInfo(std::ostream &f)
{
    f << "Faction: " << num << '\n';
    f << "Name: " << name << '\n';
    f << "Email: " << address << '\n';
    f << "Password: " << password << '\n';
    f << "LastOrders: " << lastorders << '\n';
    f << "FirstTurn: " << startturn << '\n';
    f << "SendTimes: " << times << '\n';
    f << "Template: " << TemplateStrs[temformat] << '\n';
    f << "Battle: na\n";
    for (const auto& s: extra_player_data) {
        f << s << '\n';
    }
    extra_player_data.clear();
}

void Faction::CheckExist(ARegionList& regs)
{
    if (is_npc) return;
    exists = false;
    for(const auto reg : regs) {
        if (reg->Present(this)) {
            exists = true;
            return;
        }
    }
}

void Faction::error(const std::string& s, Unit* u) {
    if (is_npc) return;
    auto count = errors.size();
    if (count == 1000) errors.push_back({.message = "Too many errors!", .unit = u});
    if (count < 1000) errors.push_back({.message = s, .unit = u});
}

void Faction::event(const std::string& message, const std::string& category, ARegion* r,  Unit *u)
{
    if (is_npc) return;
    events.push_back({.message = message, .category = category, .unit = u, .region = r});
}

void Faction::remove_attitude(int f) {
    attitudes.erase(
        remove_if(attitudes.begin(), attitudes.end(), [f](const Attitude& a) { return a.factionnum == f; }),
        attitudes.end()
    );
}

AttitudeType Faction::get_attitude(int n)
{
    if (n == num) return AttitudeType::ALLY;
    for (const auto& attitude: attitudes) {
        if (attitude.factionnum == n) return attitude.attitude;
    }
    return defaultattitude;
}

void Faction::set_attitude(int faction_id, AttitudeType attitude)
{
    auto place = find_if(
        attitudes.begin(), attitudes.end(), [faction_id](const Attitude& a) { return a.factionnum == faction_id; }
    );
    if (place != attitudes.end()) {
        place->attitude = attitude;
        return;
    }
    // we didn't find it.
    Attitude a = { .factionnum = faction_id, .attitude = attitude };
    attitudes.push_back(a);
}

int Faction::CanCatch(ARegion *r, Unit *t)
{
    if (TerrainDefs[r->type].similar_type == R_OCEAN) return 1;

    int def = t->GetDefenseRiding();

    for(const auto o : r->objects) {
        for(const auto u : o->units) {
            if (u == t && o->type != O_DUMMY) return 1;
            if (u->faction == this && u->GetAttackRiding() >= def) return 1;
        }
    }
    return 0;
}

int Faction::CanSee(ARegion* r, Unit* u, int practice)
{
    int detfac = 0;
    if (u->faction == this) return 2;
    if (u->reveal == REVEAL_FACTION) return 2;

    // If the unit has any items which prevent stealth, then we can see them.
    for(auto item : u->items) {
        if (ItemDefs[item->type].flags & ItemType::NOSTEALTH) return 1;
    }

    int retval = 0;
    if (u->reveal == REVEAL_UNIT) retval = 1;
    if (u->guard == GUARD_GUARD) retval = 1;
    for(const auto obj : r->objects) {
        int dummy = 0;
        if (obj->type == O_DUMMY) dummy = 1;
        for(const auto temp : obj->units) {
            if (u == temp && dummy == 0) retval = 1;

            // penalty of 2 to stealth if assassinating and 1 if stealing
            // TODO: not sure about the reasoning behind the IMPROVED_AMTS part
            int stealpenalty = 0;
            if (Globals->HARDER_ASSASSINATION && u->stealthorders) {
                if (u->stealthorders->type == O_STEAL) {
                    stealpenalty = 1;
                } else if (u->stealthorders->type == O_ASSASSINATE) {
                    if (Globals->IMPROVED_AMTS){
                        stealpenalty = 1;
                    } else {
                        stealpenalty = 2;
                    }
                }
            }

            if (temp->faction == this) {
                if (temp->GetAttribute("observation") >
                        u->GetAttribute("stealth") - stealpenalty) {
                    if (practice) {
                        temp->PracticeAttribute("observation");
                        retval = 2;
                    }
                    else
                        return 2;
                } else {
                    if (temp->GetAttribute("observation") ==
                            u->GetAttribute("stealth") - stealpenalty) {
                        if (practice) temp->PracticeAttribute("observation");
                        if (retval < 1) retval = 1;
                    }
                }
                if (temp->GetSkill(S_MIND_READING) > 1) detfac = 1;
            }
        }
    }
    if (retval == 1 && detfac) return 2;
    return retval;
}

void Faction::DefaultOrders()
{
    activity.clear();
    numshows = 0;
}

void Faction::TimesReward()
{
    if (Globals->TIMES_REWARD) {
        event("Times reward of " + std::to_string(Globals->TIMES_REWARD) + " silver.", "reward");
        unclaimed += Globals->TIMES_REWARD;
    }
}

Faction *GetFaction(std::list <Faction *>& facs, int factionid)
{
    for(const auto f : facs)
        if (f->num == factionid) return f;
    return nullptr;
}

void Faction::DiscoverItem(int item, int force, int full)
{
    int seen, skill, i;

    seen = items.GetNum(item);
    if (!seen) {
        if (full) {
            items.SetNum(item, 2);
        } else {
            items.SetNum(item, 1);
        }
        force = 1;
    } else {
        if (seen == 1) {
            if (full) {
                items.SetNum(item, 2);
            }
            force = 1;
        } else {
            full = 1;
        }
    }
    if (force) {
        // This really should be a boolean coming in and just passed through for the full flag.  For now we'll just
        // convert it.
        itemshows.push_back({.item = item, .full = (full != 0) });
        if (!full)
            return;
        // If we've found an item that grants a skill, give a
        // report on the skill granted (if we haven't seen it
        // before)
        skill = lookup_skill(ItemDefs[item].grantSkill);
        if (skill != -1 && !(SkillDefs[skill].flags & SkillType::DISABLED)) {
            for (i = 1; i <= ItemDefs[item].maxGrant; i++) {
                if (i > skills.GetDays(skill)) {
                    skills.SetDays(skill, i);
                    shows.push_back({ .skill = skill, .level = i });
                }
            }
        }
    }
}

int Faction::GetActivityCost(FactionActivity type) {
    int count = 0;
    for (auto &kv : this->activity) {
        auto regionActivity = kv.second;

        if (Globals->FACTION_ACTIVITY == FactionActivityRules::MARTIAL) {
            // do not care on particular activity type, but each activity consumes one point
            count += regionActivity.size();
        } else if (Globals->FACTION_ACTIVITY == FactionActivityRules::MARTIAL_MERGED) {
            // Activity array item can be present due to some logic like trying to do
            // activity unsuccessfully because of different reasons
            if (regionActivity.size() > 0) {
                count++;
            }
        } else {
            // standard logic, each activity is counted separately
            if (regionActivity.find(type) != regionActivity.end()) {
                count++;
            }
        }
    }

    return count;
}

void Faction::RecordActivity(ARegion *region, FactionActivity type) {
    this->activity[region].insert(type);
}

bool Faction::IsActivityRecorded(ARegion *region, FactionActivity type) {
    auto regionActivity = this->activity[region];

    if (Globals->FACTION_ACTIVITY == FactionActivityRules::MARTIAL_MERGED) {
        if (regionActivity.size() > 0) {
            return true;
        }
    }

    return regionActivity.find(type) != std::end(regionActivity);
}
