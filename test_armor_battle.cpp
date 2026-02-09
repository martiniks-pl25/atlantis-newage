/**
 * Standalone armor balance test
 * Compiles independently to test CARM vs PARM combat balance
 */

#include "game.h"
#include "gamedata.h"
#include "battle.h"
#include "army.h"
#include "aregion.h"
#include "faction.h"
#include "unit.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>

// Skill with level
struct SkillLevel {
    int skill;
    int level;
};

// Configuration for a single unit
struct UnitConfig {
    int men;
    int weapon;
    int shield;  // -1 if no shield
    int armor;
    std::vector<SkillLevel> skills;
    bool behind;  // Position in rear (for archers)
    std::string name;
};

// Trim whitespace from both ends of string
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Map item abbreviations to item IDs
int getItemByAbbr(const std::string& abbr) {
    std::string trimmed = trim(abbr);
    if (trimmed.empty()) return -1;

    for (size_t i = 0; i < ItemDefs.size(); i++) {
        if (ItemDefs[i].abr == trimmed) return i;
    }
    return -1;
}

// Map skill abbreviations to skill IDs
int getSkillByAbbr(const std::string& abbr) {
    std::string trimmed = trim(abbr);
    if (trimmed.empty()) return -1;

    for (size_t i = 0; i < SkillDefs.size(); i++) {
        if (SkillDefs[i].abbr == trimmed) return i;
    }
    return -1;
}

// Parse skills string: "COMB5:TACT2" or "LBOW4"
std::vector<SkillLevel> parseSkills(const std::string& skills_str) {
    std::vector<SkillLevel> result;
    std::string trimmed = trim(skills_str);
    if (trimmed.empty()) return result;

    // Split by colon for multiple skills
    std::stringstream ss(trimmed);
    std::string skill_token;

    while (std::getline(ss, skill_token, ':')) {
        skill_token = trim(skill_token);
        if (skill_token.empty()) continue;

        // Extract skill abbreviation and level
        // Format: COMB5 -> skill=COMB, level=5
        size_t i = 0;
        while (i < skill_token.size() && !isdigit(skill_token[i])) i++;

        if (i == 0 || i >= skill_token.size()) continue;

        std::string skill_abbr = skill_token.substr(0, i);
        int level = std::stoi(skill_token.substr(i));

        int skill_id = getSkillByAbbr(skill_abbr);
        if (skill_id >= 0) {
            result.push_back({skill_id, level});
        }
    }

    return result;
}

// Parse unit config line: "100,BAXE,MSHI,PARM,COMB5:TACT2,BEHIND,Heavy Infantry"
// Format: men,weapon,shield,armor,skills,flags,name
UnitConfig parseUnitLine(const std::string& line) {
    UnitConfig cfg;
    std::stringstream ss(line);
    std::string token;

    // Parse men
    std::getline(ss, token, ',');
    cfg.men = std::stoi(trim(token));

    // Parse weapon
    std::getline(ss, token, ',');
    cfg.weapon = getItemByAbbr(token);

    // Parse shield (optional - can be empty)
    std::getline(ss, token, ',');
    cfg.shield = getItemByAbbr(token);

    // Parse armor
    std::getline(ss, token, ',');
    cfg.armor = getItemByAbbr(token);

    // Parse skills (e.g., "COMB5:TACT2" or "LBOW4")
    std::getline(ss, token, ',');
    cfg.skills = parseSkills(token);

    // Parse flags (e.g., "BEHIND" or empty)
    std::getline(ss, token, ',');
    std::string flags = trim(token);
    cfg.behind = (flags == "BEHIND");

    // Parse name (rest of line)
    std::getline(ss, cfg.name);
    cfg.name = trim(cfg.name);

    return cfg;
}

// Load configuration from file
bool loadConfig(const std::string& filename,
                std::vector<UnitConfig>& army1,
                std::vector<UnitConfig>& army2,
                int& num_battles) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open config file: " << filename << std::endl;
        return false;
    }

    std::string line;
    std::string current_section = "";

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Remove leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        // Check for section headers
        if (line == "[army1]") {
            current_section = "army1";
            continue;
        } else if (line == "[army2]") {
            current_section = "army2";
            continue;
        }

        // Parse num_battles setting
        if (line.find("num_battles=") == 0) {
            num_battles = std::stoi(line.substr(12));
            continue;
        }

        // Parse unit configuration
        if (current_section == "army1") {
            army1.push_back(parseUnitLine(line));
        } else if (current_section == "army2") {
            army2.push_back(parseUnitLine(line));
        }
    }

    file.close();
    return true;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_file = "battle_config.txt";  // Default config file

    if (argc > 1) {
        if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            std::cout << "Usage: " << argv[0] << " [config_file]" << std::endl;
            std::cout << "  config_file: Configuration file (default: battle_config.txt)" << std::endl;
            std::cout << "\nConfig file format:" << std::endl;
            std::cout << "  num_battles=100" << std::endl;
            std::cout << "  [army1]" << std::endl;
            std::cout << "  men,weapon,shield,armor,skills,flags,name" << std::endl;
            std::cout << "\nExample:" << std::endl;
            std::cout << "  100,BAXE,MSHI,PARM,COMB5:TACT2,,Heavy Infantry" << std::endl;
            std::cout << "  80,LBOW,,LARM,LBOW4,BEHIND,Archers" << std::endl;
            std::cout << "\n  shield: MSHI, ISHI, or empty" << std::endl;
            std::cout << "  skills: COMB5:TACT2 (multiple) or LBOW4 (single)" << std::endl;
            std::cout << "  flags: BEHIND or empty" << std::endl;
            return 0;
        }
        config_file = argv[1];
    }

    // Load configuration from file
    std::vector<UnitConfig> army1_config;
    std::vector<UnitConfig> army2_config;
    int num_battles = 100;  // Default

    if (!loadConfig(config_file, army1_config, army2_config, num_battles)) {
        std::cerr << "Failed to load config. Using defaults." << std::endl;
        // Default configuration
        army1_config = {
            {100, I_SWORD, -1, I_CHAINARMOR, {{S_COMBAT, 3}}, false, "Army 1 Unit 1"}
        };
        army2_config = {
            {100, I_SWORD, -1, I_PLATEARMOR, {{S_COMBAT, 3}}, false, "Army 2 Unit 1"}
        };
    }

    std::cout << "Loaded config from: " << config_file << std::endl;

    // Initialize game
    Game game;

    std::cout << "=== Armor Combat Balance Test ===" << std::endl;
    std::cout << "Number of battles: " << num_battles << std::endl;
    std::cout << "Initializing combat tables..." << std::endl;

    // Minimal initialization without world generation
    game.InitMinimal();
    game.ModifyTablesPerRuleset();

    // Create a test region
    ARegion *region = new ARegion;
    region->type = R_PLAIN;
    region->xloc = 0;
    region->yloc = 0;
    region->zloc = 0;
    region->buildingseq = 1;

    // Create a dummy object for units to be in
    Object *dummy = new Object(region);
    dummy->type = O_DUMMY;
    region->objects.push_back(dummy);

    std::cout << "Battle location: Plains (0,0) surface level" << std::endl;

    // Create two factions
    std::cout << "Creating faction 1..." << std::endl;
    Faction *faction1 = game.AddFaction(1, nullptr);  // noleader=1 to skip region checks
    std::cout << "Faction 1 created" << std::endl;
    faction1->set_name("Light Armor Army (CARM)");

    std::cout << "Creating faction 2..." << std::endl;
    Faction *faction2 = game.AddFaction(1, nullptr);  // noleader=1 to skip region checks
    std::cout << "Faction 2 created" << std::endl;
    faction2->set_name("Heavy Armor Army (PARM)");

    // Create units for Army 1
    std::vector<Unit*> army1_list;
    std::cout << "Creating " << army1_config.size() << " units for Army 1..." << std::endl;
    for (size_t i = 0; i < army1_config.size(); i++) {
        auto& cfg = army1_config[i];
        Unit *u = game.GetNewUnit(faction1);
        u->SetMen(I_MAN, cfg.men);
        u->set_name("Army 1: " + cfg.name);
        u->items.SetNum(cfg.weapon, cfg.men);
        if (cfg.shield >= 0) {
            u->items.SetNum(cfg.shield, cfg.men);
        }
        u->items.SetNum(cfg.armor, cfg.men);
        // Set all skills
        for (const auto& skill : cfg.skills) {
            u->skills.SetDays(skill.skill, 30 * skill.level * cfg.men);
        }
        // Set positioning flag
        if (cfg.behind) {
            u->SetFlag(FLAG_BEHIND, 1);
        }
        u->MoveUnit(dummy);
        army1_list.push_back(u);
    }
    std::cout << "Army 1 created" << std::endl;

    // Create units for Army 2
    std::vector<Unit*> army2_list;
    std::cout << "Creating " << army2_config.size() << " units for Army 2..." << std::endl;
    for (size_t i = 0; i < army2_config.size(); i++) {
        auto& cfg = army2_config[i];
        Unit *u = game.GetNewUnit(faction2);
        u->SetMen(I_MAN, cfg.men);
        u->set_name("Army 2: " + cfg.name);
        u->items.SetNum(cfg.weapon, cfg.men);
        if (cfg.shield >= 0) {
            u->items.SetNum(cfg.shield, cfg.men);
        }
        u->items.SetNum(cfg.armor, cfg.men);
        // Set all skills
        for (const auto& skill : cfg.skills) {
            u->skills.SetDays(skill.skill, 30 * skill.level * cfg.men);
        }
        // Set positioning flag
        if (cfg.behind) {
            u->SetFlag(FLAG_BEHIND, 1);
        }
        u->MoveUnit(dummy);
        army2_list.push_back(u);
    }
    std::cout << "Army 2 created" << std::endl;

    std::cout << "\n=== ARMY SETUP ===" << std::endl;
    std::cout << "Army 1 (" << army1_config.size() << " units):" << std::endl;
    int army1_total = 0;
    for (size_t i = 0; i < army1_config.size(); i++) {
        auto& cfg = army1_config[i];
        std::cout << "  " << (i+1) << ". " << cfg.name << ": " << cfg.men << " men, ";

        // Display weapon + shield
        std::cout << ItemDefs[cfg.weapon].abr;
        if (cfg.shield >= 0) {
            std::cout << "+" << ItemDefs[cfg.shield].abr;
        }
        std::cout << ", " << ItemDefs[cfg.armor].abr << ", ";

        // Display all skills
        for (size_t j = 0; j < cfg.skills.size(); j++) {
            if (j > 0) std::cout << "+";
            std::cout << SkillDefs[cfg.skills[j].skill].abbr << cfg.skills[j].level;
        }

        // Display flags
        if (cfg.behind) {
            std::cout << " [BEHIND]";
        }
        std::cout << std::endl;

        army1_total += cfg.men;
    }
    std::cout << "  Total: " << army1_total << " men" << std::endl;

    std::cout << "\nArmy 2 (" << army2_config.size() << " units):" << std::endl;
    int army2_total = 0;
    for (size_t i = 0; i < army2_config.size(); i++) {
        auto& cfg = army2_config[i];
        std::cout << "  " << (i+1) << ". " << cfg.name << ": " << cfg.men << " men, ";

        // Display weapon + shield
        std::cout << ItemDefs[cfg.weapon].abr;
        if (cfg.shield >= 0) {
            std::cout << "+" << ItemDefs[cfg.shield].abr;
        }
        std::cout << ", " << ItemDefs[cfg.armor].abr << ", ";

        // Display all skills
        for (size_t j = 0; j < cfg.skills.size(); j++) {
            if (j > 0) std::cout << "+";
            std::cout << SkillDefs[cfg.skills[j].skill].abbr << cfg.skills[j].level;
        }

        // Display flags
        if (cfg.behind) {
            std::cout << " [BEHIND]";
        }
        std::cout << std::endl;

        army2_total += cfg.men;
    }
    std::cout << "  Total: " << army2_total << " men" << std::endl;

    // Run multiple battles for statistics
    int carm_wins = 0;
    int parm_wins = 0;
    int draws = 0;
    int total_carm_survivors = 0;
    int total_parm_survivors = 0;

    std::cout << "\n=== Running " << num_battles << " battles ===" << std::endl;

    for (int i = 0; i < num_battles; i++) {
        // Reset all units
        for (size_t j = 0; j < army1_list.size(); j++) {
            auto u = army1_list[j];
            auto& cfg = army1_config[j];
            u->SetMen(I_MAN, cfg.men);
            u->items.SetNum(cfg.weapon, cfg.men);
            if (cfg.shield >= 0) {
                u->items.SetNum(cfg.shield, cfg.men);
            }
            u->items.SetNum(cfg.armor, cfg.men);
            // Reset all skills
            for (const auto& skill : cfg.skills) {
                u->skills.SetDays(skill.skill, 30 * skill.level * cfg.men);
            }
            // Reset positioning flag
            if (cfg.behind) {
                u->SetFlag(FLAG_BEHIND, 1);
            }
        }
        for (size_t j = 0; j < army2_list.size(); j++) {
            auto u = army2_list[j];
            auto& cfg = army2_config[j];
            u->SetMen(I_MAN, cfg.men);
            u->items.SetNum(cfg.weapon, cfg.men);
            if (cfg.shield >= 0) {
                u->items.SetNum(cfg.shield, cfg.men);
            }
            u->items.SetNum(cfg.armor, cfg.men);
            // Reset all skills
            for (const auto& skill : cfg.skills) {
                u->skills.SetDays(skill.skill, 30 * skill.level * cfg.men);
            }
            // Reset positioning flag
            if (cfg.behind) {
                u->SetFlag(FLAG_BEHIND, 1);
            }
        }

        // Setup battle locations
        std::list<Location *> attackers;
        std::list<Location *> defenders;

        for (auto u : army1_list) {
            Location *loc = new Location;
            loc->unit = u;
            loc->obj = dummy;
            loc->region = region;
            attackers.push_back(loc);
        }

        for (auto u : army2_list) {
            Location *loc = new Location;
            loc->unit = u;
            loc->obj = dummy;
            loc->region = region;
            defenders.push_back(loc);
        }

        // Run battle
        Battle battle;
        Events events;
        int result = battle.Run(&events, region, army1_list[0], attackers, army2_list[0], defenders, ASS_NONE);

        // Count survivors
        int army1_survivors = 0;
        for (auto u : army1_list) {
            army1_survivors += u->GetMen();
        }
        int army2_survivors = 0;
        for (auto u : army2_list) {
            army2_survivors += u->GetMen();
        }

        int carm_survivors = army1_survivors;
        int parm_survivors = army2_survivors;

        total_carm_survivors += carm_survivors;
        total_parm_survivors += parm_survivors;

        // Calculate total army sizes
        int army1_size = 0;
        for (auto& cfg : army1_config) army1_size += cfg.men;
        int army2_size = 0;
        for (auto& cfg : army2_config) army2_size += cfg.men;

        std::cout << "Battle " << (i + 1) << ": ";
        if (result == BATTLE_WON) {
            carm_wins++;
            std::cout << "Army 1 wins";
        } else if (result == BATTLE_LOST) {
            parm_wins++;
            std::cout << "Army 2 wins";
        } else {
            draws++;
            std::cout << "DRAW";
        }
        std::cout << " (Army 1: " << carm_survivors << "/" << army1_size << " survivors, Army 2: "
                  << parm_survivors << "/" << army2_size << " survivors)" << std::endl;

        // Cleanup locations
        for (auto loc : attackers) delete loc;
        for (auto loc : defenders) delete loc;
    }

    // Calculate total army sizes for statistics
    int army1_total_size = 0;
    for (auto& cfg : army1_config) army1_total_size += cfg.men;
    int army2_total_size = 0;
    for (auto& cfg : army2_config) army2_total_size += cfg.men;

    std::cout << "\n=== STATISTICS OVER " << num_battles << " BATTLES ===" << std::endl;
    std::cout << "Army 1 wins: " << carm_wins << " (" << (carm_wins * 100 / num_battles) << "%)" << std::endl;
    std::cout << "Army 2 wins: " << parm_wins << " (" << (parm_wins * 100 / num_battles) << "%)" << std::endl;
    std::cout << "Draws: " << draws << " (" << (draws * 100 / num_battles) << "%)" << std::endl;
    std::cout << "Average Army 1 survivors: " << (total_carm_survivors / num_battles) << "/" << army1_total_size << std::endl;
    std::cout << "Average Army 2 survivors: " << (total_parm_survivors / num_battles) << "/" << army2_total_size << std::endl;

    std::cout << "\n=== CONCLUSION ===" << std::endl;
    if (carm_wins > parm_wins * 1.5) {
        std::cout << "Army 1 is significantly stronger" << std::endl;
    } else if (parm_wins > carm_wins * 1.5) {
        std::cout << "Army 2 is significantly stronger" << std::endl;
    } else {
        std::cout << "Armies are reasonably balanced" << std::endl;
    }

    return 0;
}
