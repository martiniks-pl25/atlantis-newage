#ifdef WIN32
#include <memory.h> // Needed for memcpy on windows
#include "io.h"     // Needed for access() on windows
#define F_OK    0
#endif

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <string.h>
#include <ctime>
#include <cctype>

#include "astring.h"
#include "game.h"
#include "gamedata.h"
#include "indenter.hpp"
#include "text_report_generator.hpp"
#include "quests.h"
#include "unit.h"
#include "rng.hpp"
#include "string_parser.hpp"
#include "strings_util.hpp"

#include "external/nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;

Game::Game()
{
    gameStatus = GAME_STATUS_UNINIT;
    ppUnits = 0;
    maxppunits = 0;
    events = new Events();
    rulesetSpecificData = json::object();

    if (Globals->FACTION_ACTIVITY == FactionActivityRules::DEFAULT)
    {
        FactionTypes->push_back(F_WAR);
        FactionTypes->push_back(F_TRADE);
        FactionTypes->push_back(F_MAGIC);
    }
    else
    {
        FactionTypes->push_back(F_MARTIAL);
        FactionTypes->push_back(F_MAGIC);
    }
}

Game::~Game()
{
    delete[] ppUnits;
    delete events;
    ppUnits = 0;
    maxppunits = 0;
    // Return the global array to it's original state. (needed for unit tests)
    FactionTypes->clear();
}

/**
 * @brief Returns current game turn number (1-based)
 *
 * Converts game year and month to sequential turn number.
 * Formula: (year-1)*12 + month + 1
 *
 * @return Turn number starting from 1 (Year 1, Month 0 = Turn 1)
 * @note Month is 0-indexed (0=January, 11=December)
 * @example Year 1, Month 0 → Turn 1 (first turn)
 * @example Year 2, Month 0 → Turn 13
 */
int Game::TurnNumber()
{
    return (year-1)*12 + month + 1;
}

// ALT, 25-Jul-2000
// Default work order procedure
void Game::DefaultWorkOrder()
{
    for(const auto r : regions) {
        if (r->type == R_NEXUS) continue;
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->monthorders || u->faction->is_npc || (Globals->TAX_PILLAGE_MONTH_LONG && u->taxing != TAX_NONE))
                    continue;
                if (u->GetFlag(FLAG_AUTOTAX) && (Globals->TAX_PILLAGE_MONTH_LONG && u->Taxers(1))) {
                    u->taxing = TAX_AUTO;
                } else if (Globals->DEFAULT_WORK_ORDER) {
                    ProcessWorkOrder(u, 1, 0);
                }
            }
        }
    }
}

std::string Game::GetXtraMap(ARegion *reg, int type)
{
    int i;

    if (!reg) return " ";

    switch (type) {
        case 0:
            return reg->IsStartingCity() ? "!" : (reg->HasShaft() ? "*" : " ");
        case 1:
            i = reg->CountWMons();
            return (i ? std::to_string(i) : " ");
        case 2:
            for(const auto o : reg->objects) {
                if (!(ObjectDefs[o->type].flags & ObjectType::CANENTER)) {
                    return (o->units.front() ? "*" : ".");
                }
            }
            return " ";
        case 3:
            if (reg->gate) return "*";
            return " ";
    }
    return " ";
}

void Game::WriteSurfaceMap(ostream& f, ARegionArray *pArr, int type)
{
    ARegion *reg;
    int yy = 0;
    int xx = 0;

    f << "Map (" << xx*32 << "," << yy*16 << ")\n";
    for (int y=0; y < pArr->y; y+=2) {
        std::string temp;
        int x;
        for (x=0; x< pArr->x; x+=2) {
            reg = pArr->GetRegion(x+xx*32,y+yy*16);
            temp += GetRChar(reg);
            temp += GetXtraMap(reg,type);
            temp += "  ";
        }
        f << temp << "\n";
        temp = "  ";
        for (x=1; x< pArr->x; x+=2) {
            reg = pArr->GetRegion(x+xx*32,y+yy*16+1);
            temp += GetRChar(reg);
            temp += GetXtraMap(reg,type);
            temp += "  ";
        }
        f << temp << "\n";
    }
    f << "\n";
}

void Game::WriteUnderworldMap(ostream& f, ARegionArray *pArr, int type)
{
    ARegion *reg, *reg2;
    int xx = 0;
    int yy = 0;
    f << "Map (" << xx*32 << "," << yy*16 << ")\n";
    for (int y=0; y< pArr->y; y+=2) {
        std::string temp = " ";
        std::string temp2;
        int x;
        for (x=0; x< pArr->x; x+=2) {
            reg = pArr->GetRegion(x+xx*32,y+yy*16);
            reg2 = pArr->GetRegion(x+xx*32+1,y+yy*16+1);
            temp += GetRChar(reg);
            temp += GetXtraMap(reg,type);
            if (reg2 && reg2->neighbors[D_NORTH]) temp += "|";
            else temp += " ";

            temp += " ";
            if (reg && reg->neighbors[D_SOUTHWEST]) temp2 += "/";
            else temp2 += " ";

            temp2 += " ";
            if (reg && reg->neighbors[D_SOUTHEAST]) temp2 += "\\";
            else temp2 += " ";

            temp2 += " ";
        }
        f << temp << "\n" << temp2 << "\n";

        temp = " ";
        temp2 = "  ";
        for (x=1; x< pArr->x; x+=2) {
            reg = pArr->GetRegion(x+xx*32,y+yy*16+1);
            reg2 = pArr->GetRegion(x+xx*32-1,y+yy*16);

            if (reg2 && reg2->neighbors[D_SOUTH]) temp += "|";
            else temp += " ";

            temp += " ";
            temp += GetRChar(reg);
            temp += GetXtraMap(reg,type);

            if (reg && reg->neighbors[D_SOUTHWEST]) temp2 += "/";
            else temp2 += " ";

            temp2 += " ";
            if (reg && reg->neighbors[D_SOUTHEAST]) temp2 += "\\";
            else temp2 += " ";

            temp2 += " ";
        }
        f << temp << "\n" << temp2 << "\n";
    }
    f << "\n";
}

int Game::view_map(const std::string& typestr,const std::string& mapfile)
{
    int type = 0;
    if (typestr == "wmon") type = 1;
    if (typestr == "lair") type = 2;
    if (typestr == "gate") type = 3;
    if (typestr == "cities") type = 4;
    if (typestr == "hex") type = 5;

    std::ofstream f(mapfile, std::ios::out|std::ios::ate);
    if (!f.is_open()) return(0);

    switch (type) {
        case 0:
            f << "Geographical Map\n";
            break;
        case 1:
            f << "Wandering Monster Map\n";
            break;
        case 2:
            f << "Lair Map\n";
            break;
        case 3:
            f << "Gate Map\n";
            break;
        case 4:
            f << "Cities Map\n";
            break;
    }

    // Cities map is a bit special since it is really just a list of all the cities in that region
    if (type == 4) {
        for(const auto reg : regions) {
            // Ignore anything that isn't the surface
            if (reg->level->levelType != ARegionArray::LEVEL_SURFACE) continue;
            // Ignore anything with no city
            if (!reg->town || (reg->town->TownType() != TOWN_CITY)) continue;

            f << "(" << reg->xloc << "," << reg->yloc << "): " << reg->town->name << "\n";
        }
        return(1);
    }

    if (type == 5) {
        json worldmap;

        std::vector<ARegion *> start_regions;
        for (auto i = 0; i < regions.numLevels; i++) {
            ARegionArray *pArr = regions.pRegionArrays[i];
            if (pArr->levelType == ARegionArray::LEVEL_NEXUS) {
                if (!Globals->START_CITIES_EXIST) {
                    ARegion *nexus = pArr->GetRegion(0, 0);
                    for(const auto o : nexus->objects) {
                        if (o->inner != -1) {
                            start_regions.push_back(regions.GetRegion(o->inner));
                        }
                    }
                }
                continue;
            };
            string label = (pArr->strName.empty() ? "surface" : (pArr->strName + to_string(i-1)));
            worldmap[label] = json::array();

            for (int y = 0; y < pArr->y; y++) {
                for (int x = 0; x < pArr->x; x++) {
                    ARegion *reg = pArr->GetRegion(x, y);
                    if (!reg) continue;
                    json data = reg->basic_region_data();
                    json hexout = {
                        { "x", x }, { "y", y }, { "z", i },
                        { "terrain", data["terrain"] },
                        { "yew", reg->produces_item(I_YEW) },
                        { "mithril", reg->produces_item(I_MITHRIL) },
                        { "admantium", reg->produces_item(I_ADMANTIUM) },
                        { "floater", reg->produces_item(I_FLOATER) },
                        { "wing", reg->produces_item(I_WHORSE) },
                        { "gate", reg->gate != 0 },
                        { "exits", json::array() },
                        { "shaft", reg->HasShaft() },
                        { "starting",
                          (start_regions.end() != std::find(start_regions.begin(), start_regions.end(), reg)) ||
                          reg->IsStartingCity()
                        }
                    };
                    // Fill in the exits
                    for (auto d = 0; d < NDIRS; d++) {
                        hexout["exits"].push_back(reg->neighbors[d] != nullptr);
                    }
                    if (reg->town) {
                        hexout["town"] = data["settlement"]["size"];
                    }
                    worldmap[label].push_back(hexout);
                }
            }
        }
        f << worldmap.dump(2);
        return(1);
    }

    int i;
    for (i = 0; i < regions.numLevels; i++) {
        f << '\n';
        ARegionArray *pArr = regions.pRegionArrays[i];
        switch(pArr->levelType) {
            case ARegionArray::LEVEL_NEXUS:
                f << "Level " << i << ": Nexus\n";
                break;
            case ARegionArray::LEVEL_SURFACE:
                f << "Level " << i << ": Surface\n";
                WriteSurfaceMap(f, pArr, type);
                break;
            case ARegionArray::LEVEL_UNDERWORLD:
                f << "Level " << i << ": Underworld\n";
                WriteUnderworldMap(f, pArr, type);
                break;
            case ARegionArray::LEVEL_UNDERDEEP:
                f << "Level " << i << ": Underdeep\n";
                WriteUnderworldMap(f, pArr, type);
                break;
            case ARegionArray::LEVEL_DUNGEON:
                f << "Level " << i << ": Dungeon\n";
                WriteUnderworldMap(f, pArr, type);
                break;
        }
    }
    return(1);
}

int Game::NewGame()
{
    factionseq = 1;
    guardfaction = 0;
    monfaction = 0;
    unitseq = 1;
    SetupUnitNums();
    shipseq = FLEET_NUM_START;
    questseq = 1;
    year = 1;
    month = -1;
    gameStatus = GAME_STATUS_NEW;

    //
    // Seed the random number generator with a different value each time.
    //
    // init_random_seed() is a function reference that can be overwritten by the test suites to control the random seed
    // so that tests are repeatable.
    init_random_seed();

    CreateWorld();
    CreateNPCFactions();

    if (Globals->CITY_MONSTERS_EXIST)
        CreateCityMons();
    if (Globals->WANDERING_MONSTERS_EXIST)
        CreateWMons();
    if (Globals->LAIR_MONSTERS_EXIST)
        CreateLMons();

    if (Globals->LAIR_MONSTERS_EXIST)
        CreateVMons();

    /*
    if (Globals->PLAYER_ECONOMY) {
        Equilibrate();
    }
    */



    return(1);
}

// Valid world id = URL-safe slug: 1-32 chars of [a-z0-9_-].
// Must match the portal world slug (used in URLs, report filenames, JWT).
bool Game::is_valid_world_id(const std::string &id)
{
    if (id.empty() || id.size() > 32) return false;
    for (char c : id) {
        if (!(islower((unsigned char)c) || isdigit((unsigned char)c) || c == '-' || c == '_'))
            return false;
    }
    return true;
}

int Game::OpenGame()
{
    //
    // The order here must match the order in SaveGame
    //
    ifstream f("game.in", ios::in);
    if (!f.is_open()) return(0);
    //
    // Read in Globals
    std::string str;
    std::getline(f >> std::ws, str);
    if (f.eof()) return(0);

    if (str.empty()) return(0);

    if (str != "atlantis_game") return(0);

    ATL_VER eVersion;
    f >> eVersion;
    logger::write("Saved Game Engine Version: " + ATL_VER_STRING(eVersion));
    if (
        ATL_VER_MAJOR(eVersion) != ATL_VER_MAJOR(CURRENT_ATL_VER) ||
        ATL_VER_MINOR(eVersion) != ATL_VER_MINOR(CURRENT_ATL_VER)
    ) {
        logger::write("Incompatible Engine versions!");
        return(0);
    }
    if (ATL_VER_PATCH(eVersion) > ATL_VER_PATCH(CURRENT_ATL_VER)) {
        logger::write("This game was created with a more recent Atlantis Engine!");
        return(0);
    }

    std::string gameName;
    f >> std::ws >> gameName;
    if (f.eof()) return(0);

    // Accept the legacy ruleset name from saves created before the NewOrigins -> NewAge
    // rename. Self-heals on the next WriteGame, which stamps the current RULESET_NAME.
    if (gameName != Globals->RULESET_NAME && gameName != "NewOrigins") {
        logger::write("Incompatible rule-set!");
        return(0);
    }

    ATL_VER gVersion;
    f >> gVersion;
    logger::write("Saved Rule-Set Version: " + ATL_VER_STRING(gVersion));

    if (ATL_VER_MAJOR(gVersion) < ATL_VER_MAJOR(Globals->RULESET_VERSION)) {
        logger::write("Upgrading to " + ATL_VER_STRING(MAKE_ATL_VER(ATL_VER_MAJOR(Globals->RULESET_VERSION), 0, 0)));
        if (!upgrade_major_version(gVersion)) {
            logger::write("Unable to upgrade!  Aborting!");
            return(0);
        }
        gVersion = MAKE_ATL_VER(ATL_VER_MAJOR(Globals->RULESET_VERSION), 0, 0);
    }
    if (ATL_VER_MINOR(gVersion) < ATL_VER_MINOR(Globals->RULESET_VERSION)) {
        logger::write("Upgrading to " + ATL_VER_STRING(
            MAKE_ATL_VER(ATL_VER_MAJOR(Globals->RULESET_VERSION), ATL_VER_MINOR(Globals->RULESET_VERSION), 0)
        ));
        if (!upgrade_minor_version(gVersion)) {
            logger::write("Unable to upgrade!  Aborting!");
            return(0);
        }
        gVersion = MAKE_ATL_VER(ATL_VER_MAJOR(gVersion), ATL_VER_MINOR(Globals->RULESET_VERSION), 0);
    }
    f >> year;
    f >> month;

    int seed;
    f >> seed;
    rng::seed_random(seed);

    f >> factionseq;
    f >> unitseq;
    f >> shipseq;
    // questseq added in engine 5.2.6; for older saves rebuild it after read_quests (see below).
    if (eVersion >= MAKE_ATL_VER(5, 2, 6)) {
        f >> questseq;
    }
    f >> guardfaction;
    f >> monfaction;
    // world_id added in engine 5.2.9; older saves have no field — set via `set-world-id` GM command.
    if (eVersion >= MAKE_ATL_VER(5, 2, 9)) {
        f >> worldId;
    } else {
        worldId = "none";
    }

    //
    // Read in the Factions
    //
    int i;
    f >> i;

    for (int j = 0; j < i; j++) {
        Faction *temp = new Faction;
        temp->Readin(f, eVersion);
        factions.push_back(temp);
    }

    // Patch-level upgrade runs after factions are loaded so deliver_balance_patch
    // can iterate factions and push skill/item descriptions into their show queues.
    if (ATL_VER_PATCH(gVersion) < ATL_VER_PATCH(Globals->RULESET_VERSION)) {
        logger::write("Upgrading to " + ATL_VER_STRING(Globals->RULESET_VERSION));
        if (!upgrade_patch_level(gVersion)) {
            logger::write("Unable to upgrade!  Aborting!");
            return(0);
        }
        gVersion = MAKE_ATL_VER(ATL_VER_MAJOR(gVersion), ATL_VER_MINOR(gVersion), ATL_VER_PATCH(Globals->RULESET_VERSION));
    }

    //
    // Read in the ARegions
    //
    i = regions.ReadRegions(f, factions);
    if (!i) return 0;

    // Migrate: add dungeon level if binary now supports it but save file predates it
    if (Globals->DUNGEON_LEVEL &&
        !regions.get_first_region_array_of_type(ARegionArray::LEVEL_DUNGEON)) {
        logger::write("Migrating: adding dungeon level to existing world...");
        ARegionArray *surface = regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
        regions.add_dungeon_level_to_existing_world(surface->x, surface->y);
    }

    // read in quests
    logger::write("Reading quests...");
    if (!quests.read_quests(f, eVersion))
        return 0;
    // Legacy saves (< 5.2.6) didn't serialise questseq — rebuild it from the quest count.
    if (eVersion < MAKE_ATL_VER(5, 2, 6)) {
        questseq = static_cast<int>(quests.size()) + 1;
    }
    // Pre-5.2.8 saves didn't persist Quest::regionname (used for "mayor of X" gazette
    // text and BUILD_ROAD destination label).  Restore from issuer_region for kill/
    // build-tower/build-inn quests; ROAD destinations are unrecoverable so use a
    // placeholder.  Skips quests whose regionname is already set (shouldn't happen
    // for legacy saves but is harmless).
    if (eVersion < MAKE_ATL_VER(5, 2, 8)) {
        for (auto& q : quests) {
            if (q->scope != Quest::SCOPE_LOCAL) continue;
            if (q->regionname != "-") continue;
            if (q->subtype == Quest::LOCAL_BUILD_ROAD) {
                q->regionname = "a nearby settlement";
                continue;
            }
            ARegion *ir = regions.GetRegion(q->issuer_region);
            if (!ir) continue;
            q->regionname = ir->town ? ir->town->name : ir->name;
        }
    }

    // read dungeon instances (tolerant: missing section = no active dungeons)
    read_dungeons(f);

    logger::write("Setting up unit numbers...");
    SetupUnitNums();

    logger::write("Game file loaded.");
    return(1);
}

int Game::SaveGame()
{
    ofstream f("game.out", ios::out|ios::ate);
    if (!f.is_open()) return(0);

    //
    // Write out Globals
    //
    f << "atlantis_game\n";
    f << CURRENT_ATL_VER << "\n";
    f << Globals->RULESET_NAME << "\n";
    f << Globals->RULESET_VERSION << "\n";

    f << year << "\n";
    f << month << "\n";
    f << rng::get_random(10000) << "\n";
    f << factionseq << "\n";
    f << unitseq << "\n";
    f << shipseq << "\n";
    f << questseq << "\n";
    f << guardfaction << "\n";
    f << monfaction << "\n";
    f << worldId << "\n";  // world_id (engine 5.2.9+); "none" when unset — never empty (positional read)
    //
    // Write out the Factions
    //
    f << factions.size() << "\n";
    for(const auto fac : factions) {
        fac->Writeout(f);
    }

    //
    // Write out the ARegions
    //
    regions.WriteRegions(f);

    // Write out quests
    quests.write_quests(f);

    // Write out dungeon instances
    write_dungeons(f);

    return(1);
}

void Game::DummyGame()
{
    //
    // No need to set anything up; we're just syntax checking some orders.
    //
}

void Game::InitMinimal()
{
    // Minimal initialization for battle tests without full world generation
    year = 1;
    month = 0;
    gameStatus = GAME_STATUS_RUNNING;

    // Seed RNG for battle randomness
    rng::seed_random(static_cast<unsigned int>(time(nullptr)));

    // Initialize faction/unit sequences
    factionseq = 1;
    unitseq = 1;
    shipseq = FLEET_NUM_START;
    questseq = 1;
    guardfaction = 0;
    monfaction = 0;

    // Initialize unit array for GetNewUnit()
    maxppunits = 1000;
    ppUnits = new Unit*[maxppunits];
    for (unsigned int i = 0; i < maxppunits; i++) {
        ppUnits[i] = nullptr;
    }
}

#define PLAYERS_FIRST_LINE "AtlantisPlayerStatus"

int Game::WritePlayers()
{
    ofstream f("players.out", ios::out|ios::ate);
    if (!f.is_open()) return(0);

    f << PLAYERS_FIRST_LINE << "\n";
    f << "Version: " << CURRENT_ATL_VER << "\n";
    // Ruleset version exposed for the portal (per-world feature gating) from world
    // genesis — players.out exists before turn 1. Matches the report's
    // engine.ruleset_version field (ATL_VER_STRING, with a " (beta)" suffix for even
    // minor versions) so consumers share one parser. ReadPlayers skips it tolerantly.
    f << "RulesetVersion: " << ATL_VER_STRING(Globals->RULESET_VERSION) << "\n";
    f << "TurnNumber: " << TurnNumber() << "\n";
    if (gameStatus == GAME_STATUS_UNINIT)
        return(0);

    if (gameStatus == GAME_STATUS_NEW)
        f << "GameStatus: New\n\n";
    else if (gameStatus == GAME_STATUS_RUNNING)
        f << "GameStatus: Running\n\n";
    else if (gameStatus == GAME_STATUS_FINISHED)
        f << "GameStatus: Finished\n\n";

    for(const auto fac : factions) {
        fac->WriteFacInfo(f);
    }

    return(1);
}

bool Game::ReadPlayers()
{
    ifstream f("players.in", ios::in);
    if (!f.is_open()) return(0);

    parser::string_parser parser;

    //
    // Default: failure.
    //
    bool return_code = false;

    do {
        //
        // The first line of the file should match.
        //
        f >> parser;
        if (parser.get_token() != PLAYERS_FIRST_LINE) break;

        //
        // Get the file version number.
        //
        f >> parser;
        if (parser.get_token() != "Version:") break;

        auto token = parser.get_token();
        if (!token) break;

        int nVer = token.get_number().value_or(0);
        if (
            ATL_VER_MAJOR(nVer) != ATL_VER_MAJOR(CURRENT_ATL_VER) ||
            ATL_VER_MINOR(nVer) != ATL_VER_MINOR(CURRENT_ATL_VER) ||
            ATL_VER_PATCH(nVer) > ATL_VER_PATCH(CURRENT_ATL_VER)
        ) {
            logger::write("The players.in file is not compatible with this version of Atlantis.");
            break;
        }

        //
        // Skip the remaining header lines (turn number, RulesetVersion, and any
        // future header fields) until GameStatus:. A tolerant loop keeps the reader
        // compatible with older players.in files that lack the RulesetVersion line
        // (and with anything the portal appends to the header later).
        //
        do {
            f >> parser;
        } while (!f.eof() && parser.get_token() != "GameStatus:");

        token = parser.get_token();
        if (!token) break;

        if (token == "New") gameStatus = GAME_STATUS_NEW;
        else if (token == "Running") gameStatus = GAME_STATUS_RUNNING;
        else if (token == "Finished") gameStatus = GAME_STATUS_FINISHED;
        else break; // invalid game status

        //
        // Now, we should have a list of factions.
        //
        f >> parser;
        Faction *fac = nullptr;

        bool lastWasNew = false;

        //
        // OK, set our return code to success; we'll set it to fail below
        // if necessary.
        //
        return_code = true;

        while(!f.eof()) {
            auto token = parser.get_token();
            if (!token) {
                f >> parser;
                continue;
            }

            if (token == "Faction:") {
                //
                // Get the new faction
                //
                token = parser.get_token();
                if (!token) {
                    return_code = false;
                    break;
                }

                if (token == "new") {
                    std::string save = parser.str();
                    int noleader = 0;
                    int x, y, z;
                    ARegion *reg = nullptr;

                    /* Check for the noleader flag */
                    token = parser.get_token();
                    if (token == "noleader") {
                        noleader = 1;
                        token = parser.get_token();
                        /* Initialize reg to something useful */
                        reg = regions.GetRegion(0, 0, 0);
                    }

                    x = token.get_number().value_or(-1);
                    y = parser.get_token().get_number().value_or(-1);
                    z = parser.get_token().get_number().value_or(-1);
                    if (x != -1 && y != -1 && z != -1) {
                        reg = regions.GetRegion(x, y, z);
                        if (reg == nullptr) logger::write("Bad faction line: " + save);
                    }

                    fac = AddFaction(noleader, reg);
                    if (!fac) {
                        logger::write("Failed to add a new faction!");
                        return_code = false;
                        break;
                    }

                    lastWasNew = true;
                } else {
                    int nFacNum = token.get_number().value_or(0);
                    lastWasNew = false;
                    fac = GetFaction(factions, nFacNum);
                    if (!fac) continue;

                    fac->startturn = TurnNumber();
                }
            } else if (fac) {
                return_code = ReadPlayersLine(token, parser, fac, lastWasNew);
                if (!return_code) break;
            }

            f >> parser;
        }
    } while(false);

    return(return_code);
}

Unit *Game::parse_gm_unit(std::string tag, Faction *fac)
{
    if (tag.empty()) return nullptr;

    if (tag[0] == 'g' && tag[1] == 'm') {
        auto gma = parser::token(tag.substr(2)).get_number();
        for(const auto reg : regions) {
            for(const auto obj : reg->objects) {
                for(const auto u : obj->units) {
                    if (u->faction->num == fac->num && u->gm_alias == gma) {
                        return u;
                    }
                }
            }
        }
    } else {
        auto v = parser::token(tag).get_number();
        if (!v) return nullptr;
        int id = v.value();
        if (static_cast<unsigned int>(id) >= maxppunits) return nullptr;
        return GetUnit(id);
    }
    return nullptr;
}

bool Game::ReadPlayersLine(parser::token& token, parser::string_parser& parser, Faction *fac, bool new_player)
{
    if (token == "Name:") {
        std::string name = parser.str();
        if (!name.empty()) {
            if (new_player) name += " (" + to_string(fac->num) + ")";
            fac->set_name(name, false);
        }
        return true;
    }

    if (token == "RewardTimes") {
        fac->TimesReward();
        return true;
    }

    if (token == "Email:") {
        std::string str = parser.get_token().get_string();
        if (!str.empty()) {
            fac->address = str;
        }
        return true;
    }

    if (token == "Password:") {
        std::string newpass = parser.get_token().get_string();
        if (newpass.empty()) newpass = "none";
        fac->password = newpass;
        return true;
    }

    if (token == "Battle:") {
        fac->battleLogFormat = 0;
        return true;
    }

    if (token == "Template:") {
        token = parser.get_token();
        int temp = parse_template_type(token);
        fac->temformat = temp == -1 ? TEMPLATE_LONG : temp;
        return true;
    }

    if (token == "Reward:") {
        int amt = parser.get_token().get_number().value_or(0);
        fac->event("Reward of " + to_string(amt) + " silver.", "reward");
        fac->unclaimed += amt;
        return true;
    }

    if (token == "SendTimes:") {
        // get the token, but otherwise ignore it
        fac->times = parser.get_token().get_number().value_or(0);
        return true;
    }

    if (token == "LastOrders:") {
        // Read this line and correctly set the lastorders for this
        // faction if the game itself isn't maintaining them.
        if (Globals->LASTORDERS_MAINTAINED_BY_SCRIPTS)
            fac->lastorders = parser.get_token().get_number().value_or(0);
        return true;
    }

    if (token == "FirstTurn:") {
        fac->startturn = parser.get_token().get_number().value_or(0);
        return true;
    }

    if (token == "Loc:") {
        auto x = parser.get_token().get_number();
        auto y = parser.get_token().get_number();
        auto z = parser.get_token().get_number();

        ARegion *reg = nullptr;
        if (x && y && z) {
            reg = regions.GetRegion(x.value(), y.value(), z.value());
        }
        if (!reg) {
            std::string str = "Invalid Loc: ";
            str += x ? to_string(x.value()) + ", " : "missing x-coord, ";
            str += y ? to_string(y.value()) + ", " : "missing y-coord, ";
            str += z ? to_string(z.value()) : "missing z-coord";
            str += " in faction " + to_string(fac->num);
            logger::write(str);
        }
        fac->pReg = reg;
        return true;
    }

    if (token == "NewUnit:") {
        // Creates a new unit in the location specified by a Loc: line
        // with a gm_alias of whatever is after the NewUnit: tag.
        if (!fac->pReg) {
            logger::write("NewUnit is not valid without a Loc: for faction "+ to_string(fac->num));
            return true;
        }
        int val = parser.get_token().get_number().value_or(0);
        if (!val) {
            logger::write("NewUnit: must be followed by an alias in faction " + to_string(fac->num));
            return true;
        }
        Unit *u = GetNewUnit(fac);
        u->gm_alias = val;
        u->MoveUnit(fac->pReg->GetDummy());
        u->event("Is given to your faction.", "gm_gift");
        return true;
    }

    if (token == "Item:") {
        std::string alias = parser.get_token().get_string();
        if (alias.empty()) {
            logger::write("Item: needs to specify a unit in faction " + to_string(fac->num));
            return true;
        }
        Unit *u = parse_gm_unit(alias, fac);
        if (!u) {
            logger::write("Item: needs to specify a unit in faction " + to_string(fac->num));
            return true;
        }
        if (u->faction->num != fac->num) {
            logger::write("Item: unit "+ to_string(u->num) + " doesn't belong to faction " + to_string(fac->num));
            return true;
        }

        auto val = parser.get_token().get_number();
        if (!val) {
            logger::write("Must specify a number of items to give for Item: in faction " + to_string(fac->num));
            return true;
        }

        token = parser.get_token();
        int it = parse_all_items(token);
        if (it == -1) {
            logger::write("Must specify a valid item to give for Item: in faction " + to_string(fac->num));
            return true;
        }

        int has = u->items.GetNum(it);
        u->items.SetNum(it, has + val.value_or(0));
        if (!u->gm_alias) {
            u->event("Is given " + item_string(it, val.value_or(0)) + " by the gods.", "gm_gift");
        }
        u->faction->DiscoverItem(it, 0, 1);
        return true;
    }

    if (token == "Skill:") {
        std::string alias = parser.get_token().get_string();
        if (alias.empty()) {
            logger::write("Skill: needs to specify a unit in faction " + to_string(fac->num));
            return true;
        }
        Unit *u = parse_gm_unit(alias, fac);
        if (!u) {
            logger::write("Skill: needs to specify a unit in faction " + to_string(fac->num));
            return true;
        }
        if (u->faction->num != fac->num) {
            logger::write("Item: unit "+ to_string(u->num) + " doesn't belong to faction " + to_string(fac->num));
            return true;
        }

        token = parser.get_token();
        int sk = parse_skill(token);
        if (sk == -1) {
            logger::write("Must specify a valid skill for Skill: in faction " + to_string(fac->num));
            return true;
        }

        int days = parser.get_token().get_number().value_or(0) * u->GetMen();
        if (!days) {
            logger::write("Must specify a days for Skill: in faction " + to_string(fac->num));
            return true;
        }

        int odays = u->skills.GetDays(sk);
        u->skills.SetDays(sk, odays + days);
        u->AdjustSkills();
        int lvl = u->GetRealSkill(sk);
        if (lvl > fac->skills.GetDays(sk)) {
            fac->skills.SetDays(sk, lvl);
            fac->shows.push_back({ .skill = sk, .level = lvl });
        }
        if (!u->gm_alias) {
            u->event("Is taught " + to_string(days) + " days of " + SkillStrs(sk) + " by the gods.", "gm_gift");
        }
        /* This is NOT quite the same, but the gods are more powerful than mere mortals */
        int mage = (SkillDefs[sk].flags & SkillType::MAGIC);
        int app = (SkillDefs[sk].flags & SkillType::APPRENTICE);
        if (mage) u->type = U_MAGE;
        if (app && u->type == U_NORMAL) u->type = U_APPRENTICE;
        return true;
    }

    if (token == "Order:") {
        std::string alias = parser.get_token().get_string();
        if (alias == "quit") {
            fac->quit = QUIT_BY_GM;
            return true;
        }
        //Not quit so, handle it as a unit order
        Unit *u = parse_gm_unit(alias, fac);
        if (!u) {
            logger::write("Order: needs to specify a unit in faction " + to_string(fac->num));
            return true;
        }
        if (u->faction->num != fac->num) {
            logger::write("Order: unit "+ to_string(u->num) + " doesn't belong to faction " + to_string(fac->num));
            return true;
        }

        std::string saved = parser.str();
        bool repeating = parser.get_at();
        token = parser.get_token();
        if (!token) {
            logger::write("Order: must provide unit order for faction "+ to_string(fac->num));
            return true;
        }
        int o = Parse1Order(token);
        if (o == -1 || o == O_ATLANTIS || o == O_END || o == O_UNIT || o == O_FORM || o == O_ENDFORM) {
            logger::write("Order: invalid order given for faction "+ to_string(fac->num));
            return true;
        }
        if (repeating) {
            u->oldorders.push_back(saved);
        }
        ProcessOrder(o, u, parser, nullptr, repeating);
        return true;
    }

    std::string temp = parser.original();
    if (temp.empty()) return true;

    fac->extra_player_data.push_back(temp);
    return true;
}

int Game::do_orders_check(const std::string& strOrders, const std::string& strCheck)
{
    std::ifstream ordersFile(strOrders, std::ios::in);
    if (!ordersFile.is_open()) {
        logger::write("No such orders file!");
        return(0);
    }

    std::ofstream checkFile(strCheck, std::ios::out|std::ios::ate);
    if (!checkFile.is_open()) {
        logger::write("Couldn't open the orders check file!");
        return(0);
    }

    orders_check check(checkFile);
    ParseOrders(0, ordersFile, &check);

    return(1);
}

int Game::RunGame()
{
    logger::write("Setting Up Turn...");
    PreProcessTurn();

    logger::write("Reading the Gamemaster File...");
    if (!ReadPlayers()) return(0);

    if (gameStatus == GAME_STATUS_FINISHED) {
        logger::write("This game is finished!");
        return(0);
    }
    gameStatus = GAME_STATUS_RUNNING;

    logger::write("Reading the Orders File...");
    ReadOrders();

    if (Globals->MAX_INACTIVE_TURNS != -1) {
        logger::write("QUITting Inactive Factions...");
        RemoveInactiveFactions();
    }

    logger::write("Running the Turn...");
    RunOrders();

    if (Globals->WORLD_EVENTS) {
        logger::write("Writing world events...");
        WriteWorldEvents();
    }

    logger::write("Writing the Report File...");
    WriteReport();
    logger::write("");

    battles.clear();

    EmptyHell();

    logger::write("Writing Playerinfo File...");
    WritePlayers();

    logger::write("Removing Dead Factions...");
    DeleteDeadFactions();

    logger::write("done");

    return(1);
}

void Game::RecordFact(FactBase* fact) {
    this->events->AddFact(fact);
}

std::vector<std::pair<int,std::string>> Game::CollectWanted() {
    std::vector<std::pair<int,std::string>> result;
    Faction *guard = GetFaction(factions, guardfaction);
    if (!guard) return result;

    for (const auto f : factions) {
        if (!f || f->num == guardfaction || f->num == monfaction || !f->exists) continue;
        if (guard->get_attitude(f->num) == AttitudeType::HOSTILE)
            result.push_back({f->num, f->name});
    }
    return result;
}

std::string Game::GenerateWantedSection() {
    auto wanted = CollectWanted();
    if (wanted.empty()) return "";

    std::string result =
        "\n*** WANTED BY ORDER OF THE CITY GUARD ***\n\n"
        "The following factions are declared enemies of the realm.\n"
        "Their forces may be attacked in any guarded settlement:\n\n";
    for (const auto &w : wanted)
        result += "  * " + w.second + " (" + std::to_string(w.first) + ")\n";
    result += "\nAny who strike them within protected walls commit no crime.\n";
    return result;
}

json Game::BuildRegaliaJson() {
    // Crowns reuse the engine's canonical item census (CountItem sums an item
    // across all of a faction's present_regions, so crowns carried outside towns
    // are still counted). Capitals resolve Faction::capital_region to its
    // town/region. Only factions that actually hold a crown / have a capital
    // appear; empty arrays are a valid "no regalia yet" signal.
    struct CrownHolder { int num; std::string name; int crowns; };
    std::vector<CrownHolder> crown_holders;
    json capitals_arr = json::array();
    for (const auto f : factions) {
        if (f->is_npc) continue;
        int c = CountItem(f, I_CROWN);
        if (c > 0)
            crown_holders.push_back({ f->num, f->name | filter::strip_number, c });
        if (f->capital_region != -1) {
            ARegion *cr = regions.GetRegion(f->capital_region);
            if (cr && cr->town) {
                capitals_arr.push_back({
                    { "faction_num",  f->num },
                    { "faction_name", f->name | filter::strip_number },
                    { "city",         cr->town->name | filter::strip_number },
                    { "region", {
                        { "terrain", TerrainDefs[cr->type].name },
                        { "name",    cr->name }
                    } }
                });
            }
        }
    }
    // Crowns ranked by count desc, faction number asc as a stable tiebreak.
    std::sort(crown_holders.begin(), crown_holders.end(),
        [](const CrownHolder& a, const CrownHolder& b) {
            if (a.crowns != b.crowns) return a.crowns > b.crowns;
            return a.num < b.num;
        });
    json crowns_arr = json::array();
    for (const auto& h : crown_holders)
        crowns_arr.push_back({
            { "faction_num",  h.num },
            { "faction_name", h.name },
            { "crowns",       h.crowns }
        });
    json regalia;
    regalia["crowns"]   = crowns_arr;
    regalia["capitals"] = capitals_arr;
    return regalia;
}

// --- Trident coronation victory predicates (hold_condition components) ---

// Does faction f actively guard region r? (a unit of f on GUARD_GUARD in r)
static bool coronation_guards_region(Faction *f, ARegion *r) {
    for (const auto obj : r->objects)
        for (const auto u : obj->units)
            if (u->faction == f && u->guard == GUARD_GUARD)
                return true;
    return false;
}

// Does faction f own a finished Palace in region r? Mirrors the CAPITAL order
// check (O_PALACE, incomplete <= 0, owner's faction == f). The city-tier
// requirement was enforced when the capital was declared.
static bool coronation_owns_palace(Faction *f, ARegion *r) {
    for (const auto obj : r->objects) {
        if (obj->type != O_PALACE) continue;
        if (obj->incomplete > 0) continue;
        Unit *owner = obj->GetOwner();
        if (owner && owner->faction == f) return true;
    }
    return false;
}

// Total crowns held by faction f's units physically present in region r.
static int coronation_crowns_in_region(Faction *f, ARegion *r) {
    int total = 0;
    for (const auto obj : r->objects)
        for (const auto u : obj->units)
            if (u->faction == f)
                total += u->items.GetNum(I_CROWN);
    return total;
}

Faction *Game::check_coronation() {
    // Dormant unless the world enables the coronation victory (Trident only).
    // On Arcanum this key is left unset → the method is a no-op.
    if (rulesetSpecificData.value("victory_type", std::string()) != "coronation")
        return nullptr;

    int crowns_to_win    = rulesetSpecificData.value("crowns_to_win", 3);
    int coronation_turns = rulesetSpecificData.value("coronation_turns", 5);

    Faction *winner = nullptr;
    for (const auto f : factions) {
        if (f->is_npc) continue;
        if (f->capital_region == -1) { f->coronation = 0; continue; }

        ARegion *cap = regions.GetRegion(f->capital_region);
        bool hold = cap
            && coronation_guards_region(f, cap)
            && coronation_owns_palace(f, cap)
            && coronation_crowns_in_region(f, cap) >= crowns_to_win;

        if (!hold) { f->coronation = 0; continue; }

        // Hold condition met this turn: advance the counter (clamped) and announce.
        if (f->coronation < coronation_turns) f->coronation++;
        std::string city = cap->town ? cap->town->name : cap->name;
        if (f->coronation >= coronation_turns) {
            write_times_article((f->name | filter::strip_number) +
                " has been crowned at " + city +
                "! The Trident is theirs. The game is won.");
            winner = f;
            break;
        }
        int remaining = coronation_turns - f->coronation;
        write_times_article("CORONATION: " + (f->name | filter::strip_number) +
            " holds the crowns at " + city + ". The crowning completes in " +
            std::to_string(remaining) + (remaining == 1 ? " turn" : " turns") +
            " unless stopped.");
    }
    return winner;
}

void Game::WriteWorldEvents() {
    constexpr bool write_legacy_text = true;  // set to false to disable times.N text files

    // --- Compute settlement ownership for newspaper ---
    int total_settlements = 0;
    int surface_settlements = 0;
    int contested_settlements = 0;
    int total_villages = 0, total_towns = 0, total_cities = 0;
    int surface_villages = 0, surface_towns = 0, surface_cities = 0;
    std::map<int, SettlementOwner> ownership;

    for (const auto reg : regions) {
        if (!reg->town) continue;
        total_settlements++;
        bool on_surface = reg->level && reg->level->levelType == ARegionArray::LEVEL_SURFACE;
        if (on_surface) surface_settlements++;

        int tt = reg->town->TownType();
        if (tt == TOWN_VILLAGE) { total_villages++; if (on_surface) surface_villages++; }
        else if (tt == TOWN_TOWN) { total_towns++;  if (on_surface) surface_towns++;  }
        else                      { total_cities++;  if (on_surface) surface_cities++;  }

        // Collect all factions with GUARD_GUARD in this region
        std::set<Faction *> guarders;
        for (const auto obj : reg->objects) {
            for (const auto u : obj->units) {
                if (u->guard == GUARD_GUARD)
                    guarders.insert(u->faction);
            }
        }
        if (guarders.empty()) continue;

        Faction *owner = nullptr;
        // Guard faction takes precedence if present
        bool has_guard_faction = false;
        for (auto f : guarders) {
            if (f->num == guardfaction) { has_guard_faction = true; break; }
        }
        if (has_guard_faction) {
            owner = GetFaction(factions, guardfaction);
        } else {
            // Exactly one non-NPC player faction → that faction owns it
            std::vector<Faction *> players;
            for (auto f : guarders) {
                if (!f->is_npc) players.push_back(f);
            }
            if (players.size() == 1) owner = players[0];
            else if (players.size() > 1) contested_settlements++;
        }
        if (!owner) continue;

        auto& entry = ownership[owner->num];
        entry.faction_num = owner->num;
        // Store name without the "(N)" suffix
        entry.faction_name = owner->name | filter::strip_number;
        if (tt == TOWN_VILLAGE) entry.villages++;
        else if (tt == TOWN_TOWN) entry.towns++;
        else entry.cities++;
        entry.total++;
    }

    // Sort by total desc
    std::vector<SettlementOwner> all_owners;
    for (auto& kv : ownership) all_owners.push_back(kv.second);
    std::sort(all_owners.begin(), all_owners.end(),
        [](const SettlementOwner& a, const SettlementOwner& b) {
            if (a.total   != b.total)   return a.total   > b.total;
            if (a.cities  != b.cities)  return a.cities  > b.cities;
            if (a.towns   != b.towns)   return a.towns   > b.towns;
            return a.villages > b.villages;
        });

    // Top 5 for text newspaper
    std::vector<SettlementOwner> top_owners = all_owners;
    if ((int)top_owners.size() > 5) top_owners.resize(5);

    auto *sf = new SettlementStatsFact();
    sf->total_settlements   = total_settlements;
    sf->surface_settlements = surface_settlements;
    sf->contested_settlements = contested_settlements;
    sf->top_owners          = top_owners;
    this->events->AddFact(sf);
    // --- End settlement ownership ---

    // --- Pirate sightings: report every active elite captain in the gazette ---
    for (const auto reg : regions) {
        for (const auto obj : reg->objects) {
            if (obj->type != O_FLEET) continue;
            for (const auto u : obj->units) {
                if (u->faction->num != monfaction) continue;
                if (u->items.GetNum(I_PIRATE_CAPTAIN) == 0) continue;
                auto *ps = new PirateSightingFact();
                ps->ship_name    = obj->name;
                ps->captain_name = u->name;
                ps->terrain_name = TerrainDefs[TerrainDefs[reg->type].similar_type].name;
                ps->region_name  = reg->name;
                this->events->AddFact(ps);
            }
        }
    }
    // --- End pirate sightings ---

    auto wanted = CollectWanted();

    // Build pirate_context for AI gazette generation:
    // All elite events (named ships) + random fill from regular up to 15 total.
    std::vector<std::string> pirate_context = pirate_context_elite;
    if ((int)pirate_context_regular.size() > 1)
        std::shuffle(pirate_context_regular.begin(), pirate_context_regular.end(),
                     std::default_random_engine(rng::get_random(100000)));
    int fill = std::max(0, 15 - (int)pirate_context.size());
    int take = std::min(fill, (int)pirate_context_regular.size());
    pirate_context.insert(pirate_context.end(),
        pirate_context_regular.begin(), pirate_context_regular.begin() + take);
    pirate_context_elite.clear();
    pirate_context_regular.clear();

    std::string json_str = this->events->WriteJSON(
        Globals->RULESET_NAME, MonthNames[this->month], this->year, wanted, pirate_context);

    // Pick a shared base filename for both outputs
    std::string base;
    do {
        base = "times." + std::to_string(rng::get_random(10000));
    } while (filesystem::exists(base) || filesystem::exists(base + ".json"));

    if (write_legacy_text) {
        std::string text = this->events->Write(Globals->RULESET_NAME, MonthNames[this->month], this->year);
        std::string wanted_text = GenerateWantedSection();
        std::string full = text + wanted_text;
        if (!full.empty()) {
            std::ofstream tf(base, std::ios::out | std::ios::trunc);
            if (tf.is_open()) {
                tf << indent::wrap(78, 70, 0) << full << '\n';
            }
        }
    }

    if (!json_str.empty()) {
        // Add active quests to the JSON newspaper
        json j = json::parse(json_str);
        json questArray = json::array();
        for (const auto& q : quests) {
            json item;
            switch (q->type) {
                case Quest::SLAY:        item["type"] = "slay";        break;
                case Quest::HARVEST:     item["type"] = "harvest";     break;
                case Quest::BUILD:       item["type"] = "build";       break;
                case Quest::VISIT:       item["type"] = "visit";       break;
                case Quest::DEMOLISH:    item["type"] = "demolish";    break;
                case Quest::HUNT_PIRATE: item["type"] = "pirate_hunt"; break;
                default:                 item["type"] = "unknown";     break;
            }
            // Generate roleplay narrative text only — no coordinates or rewards
            // to prevent players from trivially locating quest targets via the JSON.
            std::string text;
            if (q->type == Quest::SLAY) {
                Location *l = regions.FindUnit(q->target);
                if (l) {
                    text = "Quest: In the ";
                    text += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
                    text += " of ";
                    text += l->region->name;
                    text += (l->obj->type == O_DUMMY) ? " roams" : " lurks";
                    text += " the ";
                    text += l->unit->name;
                    text += ".  Free the world from this menace and be rewarded!";
                    delete l;
                }
            } else if (q->type == Quest::HARVEST) {
                ARegion *r = regions.GetRegion(q->regionnum);
                if (r) {
                    text = "Quest: Seek a token of the Ancient Ones legacy amongst the ";
                    text += ItemDefs[q->objective.type].names;
                    text += " of ";
                    text += r->name;
                    text += ".";
                }
            } else if (q->type == Quest::BUILD) {
                text = "Quest: Build a ";
                text += ObjectDefs[q->building].name;
                text += " in ";
                text += q->regionname;
                text += " for the glory of the Gods.";
            } else if (q->type == Quest::VISIT) {
                text = "Quest: Show your devotion by visiting ";
                text += ObjectDefs[q->building].name;
                text += "s in ";
                unsigned ucount = 0;
                for (const auto& dest : q->destinations) {
                    ucount++;
                    if (ucount == q->destinations.size())
                        text += " and ";
                    else if (ucount > 1)
                        text += ", ";
                    text += dest;
                }
                text += ".";
            } else if (q->type == Quest::DEMOLISH) {
                ARegion *r = regions.GetRegion(q->regionnum);
                if (r) {
                    Object *o = r->GetObject(q->target);
                    if (o) {
                        text = "Quest: Tear down the blasphemous ";
                        text += o->name;
                        text += " : ";
                        text += ObjectDefs[o->type].name;
                        text += " in ";
                        text += r->name;
                        text += "!";
                    }
                }
            } else if (q->type == Quest::HUNT_PIRATE) {
                Location *l = regions.FindUnit(q->target);
                if (l) {
                    text = "Quest: ";
                    text += l->unit->name;
                    text += " commands the pirate galley '";
                    text += l->obj->name;
                    text += "', last sighted in the ";
                    text += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
                    text += " of ";
                    text += l->region->name;
                    text += ". Bring proof of their destruction and claim the bounty!";
                    delete l;
                }
            } else if (q->subtype == Quest::GLOBAL_BOSS_HUNT) {
                item["type"] = "pirate_hunt";
                Location *l = regions.FindUnit(q->target);
                if (l) {
                    text = "Quest: ";
                    text += l->unit->name;
                    text += " commands the pirate galley '";
                    text += l->obj->name;
                    text += "', last sighted in the ";
                    text += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
                    text += " of ";
                    text += l->region->name;
                    text += ". Bring proof of their destruction and claim the bounty!";
                    delete l;
                }
            }
            if (!text.empty()) {
                item["text"] = text;
                questArray.push_back(item);
            }
        }
        j["quests"] = questArray;

        // Add settlement stats as structured data
        auto make_owner_json = [](const SettlementOwner& o) {
            return json{
                { "faction_num",  o.faction_num  },
                { "faction_name", o.faction_name },
                { "villages",     o.villages     },
                { "towns",        o.towns        },
                { "cities",       o.cities       },
                { "total",        o.total        }
            };
        };
        int controlled = 0;
        for (const auto& o : all_owners) controlled += o.total;

        json settlement_stats;
        settlement_stats["total"]        = total_settlements;
        settlement_stats["surface"]      = surface_settlements;
        settlement_stats["contested"]    = contested_settlements;
        settlement_stats["uncontrolled"] = total_settlements - controlled - contested_settlements;
        settlement_stats["total_by_type"]   = { {"villages", total_villages},
                                                 {"towns",    total_towns},
                                                 {"cities",   total_cities} };
        settlement_stats["surface_by_type"] = { {"villages", surface_villages},
                                                 {"towns",    surface_towns},
                                                 {"cities",   surface_cities} };
        json top_array = json::array();
        for (const auto& o : top_owners) top_array.push_back(make_owner_json(o));
        settlement_stats["top_owners"] = top_array;
        json all_array = json::array();
        for (const auto& o : all_owners) all_array.push_back(make_owner_json(o));
        settlement_stats["all_owners"] = all_array;
        j["settlement_stats"] = settlement_stats;

        // Regalia: crowns held and declared capitals (Trident victory summary)
        j["regalia"] = BuildRegaliaJson();

        std::ofstream jf(base + ".json", std::ios::out | std::ios::trunc);
        if (jf.is_open()) {
            jf << j.dump(2) << '\n';
        }
    }
}

void Game::PreProcessTurn()
{
    month++;
    if (month>11) {
        month = 0;
        year++;
    }
    SetupUnitNums();

    for(const auto f : factions) f->DefaultOrders();

    for(const auto reg : regions) {
        if (Globals->WEATHER_EXISTS == 1)
            reg->SetWeather(regions.GetWeather(reg, month));
        if (Globals->GATES_NOT_PERENNIAL)
            reg->SetGateStatus(month);
        reg->DefaultOrders();
    }
}

void Game::ClearOrders(Faction *f)
{
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->faction == f) {
                    u->ClearOrders();
                }
            }
        }
    }
}

void Game::ReadOrders()
{
    for(const auto fac : factions) {
        if (!fac->is_npc) {
            std::string str = "orders.";
            str += std::to_string(fac->num);

            ifstream file(str, ios::in);
            if(file.is_open()) {
                ParseOrders(fac->num, file, nullptr);
                file.close();
            }
            DefaultWorkOrder();
        }
    }
}

void Game::MakeFactionReportLists()
{
    std::vector<Faction *> facs(factionseq, nullptr);

    for(const auto reg : regions) {
        // clear the temporary
        std::fill(facs.begin(), facs.end(), nullptr);

        for(const auto far : reg->farsees) {
            facs[far->faction->num] = far->faction;
        }
        for(const auto pass : reg->passers) {
            facs[pass->faction->num] = pass->faction;
        }
        for(const auto obj : reg->objects) {
            for(const auto unit : obj->units) {
                facs[unit->faction->num] = unit->faction;
            }
        }

        for(const auto& fac: facs) {
            if (fac) fac->present_regions.push_back(reg);
        }
    }
}

void Game::WriteReport()
{
    MakeFactionReportLists();
    CountAllSpecialists();

    size_t ** citems = nullptr;

    if (Globals->FACTION_STATISTICS) {
        citems = new size_t * [factionseq];
        for (int i = 0; i < factionseq; i++)
        {
            citems [i] = new size_t [NITEMS];
            for (int j = 0; j < NITEMS; j++)
            {
                citems [i][j] = 0;
            }
        }
        CountItems(citems);
    }

    for(const auto fac : factions) {
        string report_file = "report." + to_string(fac->num);
        string template_file = "template." + to_string(fac->num);

        if (!fac->is_npc || fac->gets_gm_report(this)) {
            // Generate the report in JSON format and then write it to whatever formats we want
            json json_report;
            bool show_region_depth = Globals->EASIER_UNDERWORLD
                && (Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS > 1);
            fac->build_json_report(json_report, this, citems);

            if (Globals->REPORT_FORMAT & GameDefs::REPORT_FORMAT_JSON) {
                ofstream jsonf(report_file + ".json", ios::out | ios::ate);
                if (jsonf.is_open()) {
                    jsonf << json_report.dump(2);
                }
            }

            if (Globals->REPORT_FORMAT & GameDefs::REPORT_FORMAT_TEXT) {
                TextReportGenerator text_report;
                ofstream f(report_file, ios::out | ios::ate);
                if (f.is_open()) {
                    text_report.output(f, json_report, show_region_depth);
                }
                if (!fac->is_npc && fac->temformat != TEMPLATE_OFF) {
                    // even factions which get a gm report do not get a template.
                    ofstream f(template_file, ios::out | ios::ate);
                    if (f.is_open()) {
                        text_report.output_template(f, json_report, fac->temformat, show_region_depth);
                    }
                }
            }
        }
        logger::dot();
    }

    // stop leaking memory
    if (Globals->FACTION_STATISTICS) {
        for (auto i = 0; i < factionseq; i++) delete [] citems [i];
        delete [] citems;
    }
}

void Game::DeleteDeadFactions()
{
    for(auto it = factions.begin(); it != factions.end();) {
        auto fac = *it;
        if (!fac->is_npc && !fac->exists) {
            it = factions.erase(it);
            for(const auto fac2 : factions) fac2->remove_attitude(fac->num);
            delete fac;
            continue;
        }
        ++it;
    }
}

Faction *Game::AddFaction(int noleader, ARegion *pStart)
{
    //
    // set up faction
    //
    Faction *temp = new Faction(factionseq);
    temp->set_address("NoAddress");
    temp->lastorders = TurnNumber();
    temp->startturn = TurnNumber();
    temp->pStartLoc = pStart;
    temp->pReg = pStart;
    temp->noStartLeader = noleader;

    if (!SetupFaction(temp)) {
        delete temp;
        return 0;
    }
    factions.push_back(temp);
    factionseq++;
    return temp;
}

void Game::ViewFactions()
{
    for(const auto f : factions) f->View();
}

void Game::SetupUnitSeq()
{
    int max = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u && u->num > max) max = u->num;
            }
        }
    }
    unitseq = max+1;
}

void Game::SetupUnitNums()
{
    if (ppUnits) delete[] ppUnits;

    SetupUnitSeq();

    maxppunits = unitseq+10000;

    ppUnits = new Unit *[maxppunits];

    unsigned int i;
    for (i = 0; i < maxppunits ; i++) ppUnits[i] = 0;

    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                i = u->num;
                if ((i > 0) && (i < maxppunits)) {
                    if (!ppUnits[i])
                        ppUnits[u->num] = u;
                    else {
                        logger::write("Error: Unit number " + std::to_string(i) + " multiply defined.");
                        if ((unitseq > 0) && (unitseq < maxppunits)) {
                            u->num = unitseq;
                            ppUnits[unitseq++] = u;
                        }
                    }
                } else {
                    logger::write("Error: Unit number " + std::to_string(i) + " out of range.");
                    if ((unitseq > 0) && (unitseq < maxppunits)) {
                        u->num = unitseq;
                        ppUnits[unitseq++] = u;
                    }
                }
            }
        }
    }
}

Unit *Game::GetNewUnit(Faction *fac, int an)
{
    unsigned int i;
    for (i = 1; i < unitseq; i++) {
        if (!ppUnits[i]) {
            Unit *pUnit = new Unit(i, fac, an);
            ppUnits[i] = pUnit;
            return(pUnit);
        }
    }

    Unit *pUnit = new Unit(unitseq, fac, an);
    ppUnits[unitseq] = pUnit;
    unitseq++;
    if (unitseq >= maxppunits) {
        Unit **temp = new Unit*[maxppunits+10000];
        memcpy(temp, ppUnits, maxppunits*sizeof(Unit *));
        maxppunits += 10000;
        delete[] ppUnits;
        ppUnits = temp;
    }

    return(pUnit);
}

Unit *Game::GetUnit(int num)
{
    if (num < 0 || (unsigned int)num >= maxppunits) return NULL;
    return(ppUnits[num]);
}


void Game::CountAllSpecialists()
{
    for(const auto f : factions) {
        f->nummages = 0;
        f->numqms = 0;
        f->numtacts = 0;
        f->numapprentices = 0;
    }
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->type == U_MAGE) u->faction->nummages++;
                if (u->GetSkill(S_QUARTERMASTER)) u->faction->numqms++;
                if (u->GetSkill(S_TACTICS) == 5) u->faction->numtacts++;
                if (u->type == U_APPRENTICE) u->faction->numapprentices++;
            }
        }
    }
}

// LLS
void Game::UnitFactionMap()
{
    unsigned int i;
    Unit *u;

    logger::write("Opening units.txt");
    ofstream f("units.txt", ios::out|ios::ate);
    if (!f.is_open()) {
        logger::write("Couldn't open file!");
        return;
    }
    logger::write("Writing " + std::to_string(unitseq) + " units");
    for (i = 1; i < unitseq; i++) {
        u = GetUnit(i);
        if (!u) {
            logger::write("doesn't exist");
        } else {
            logger::write(std::to_string(i) + ":" + std::to_string(u->faction->num));
            f << i << ":" << u->faction->num << endl;
        }
    }
}

//The following function added by Creative PBM February 2000
void Game::RemoveInactiveFactions()
{
    if (Globals->MAX_INACTIVE_TURNS == -1) return;

    int cturn;
    cturn = TurnNumber();
    for(const auto fac : factions) {
        if ((cturn - fac->lastorders) >= Globals->MAX_INACTIVE_TURNS && !fac->is_npc) {
            fac->quit = QUIT_BY_GM;
        }
    }
}

int Game::CountMages(Faction *pFac)
{
    int i = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u :o->units) {
                if (u->faction == pFac && u->type == U_MAGE) i++;
            }
        }
    }
    return(i);
}

int Game::CountQuarterMasters(Faction *pFac)
{
    int i = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->faction == pFac && u->GetSkill(S_QUARTERMASTER)) i++;
            }
        }
    }
    return i;
}

int Game::CountTacticians(Faction *pFac)
{
    int i = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->faction == pFac && u->GetSkill(S_TACTICS) == 5) i++;
            }
        }
    }
    return i;
}

int Game::CountApprentices(Faction *pFac)
{
    int i = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->faction == pFac && u->type == U_APPRENTICE) i++;
            }
        }
    }
    return i;
}

int Game::AllowedMages(Faction *pFac)
{
    int points = pFac->type[F_MAGIC];

    if (points < 0) points = 0;
    if (points > allowedMagesSize - 1) points = allowedMagesSize - 1;

    return allowedMages[points];
}

int Game::AllowedQuarterMasters(Faction *pFac)
{
    int points = std::max(pFac->type[F_TRADE], pFac->type[F_MARTIAL]);

    if (points < 0) points = 0;
    if (points > allowedQuartermastersSize - 1)
        points = allowedQuartermastersSize - 1;

    return allowedQuartermasters[points];
}

int Game::AllowedTacticians(Faction *pFac)
{
    int points = std::max(pFac->type[F_WAR], pFac->type[F_MARTIAL]);

    if (points < 0) points = 0;
    if (points > allowedTacticiansSize - 1)
        points = allowedTacticiansSize - 1;

    return allowedTacticians[points];
}

int Game::AllowedApprentices(Faction *pFac)
{
    int points = pFac->type[F_MAGIC];

    if (points < 0) points = 0;
    if (points > allowedApprenticesSize - 1)
        points = allowedApprenticesSize - 1;

    return allowedApprentices[points];
}

int Game::AllowedTaxes(Faction *pFac)
{
    int points = std::max(pFac->type[F_WAR], pFac->type[F_MARTIAL]);

    if (points < 0) points = 0;
    if (points > allowedTaxesSize - 1) points = allowedTaxesSize - 1;

    return allowedTaxes[points];
}

int Game::AllowedTrades(Faction *pFac)
{
    int points = std::max(pFac->type[F_TRADE], pFac->type[F_MARTIAL]);

    if (points < 0) points = 0;
    if (points > allowedTradesSize - 1) points = allowedTradesSize - 1;

    return allowedTrades[points];
}

int Game::AllowedMartial(Faction *pFac)
{
    int points = pFac->type[F_MARTIAL];

    if (points < 0) points = 0;
    if (points > allowedMartialSize - 1) points = allowedMartialSize - 1;

    return allowedMartial[points];
}

bool Game::upgrade_major_version(int current_version)
{
    return false;
}

bool Game::upgrade_minor_version(int current_version)
{
    return true;
}

/**
 * @brief Re-sends skill/item descriptions to factions when balance values change.
 *
 * Called automatically by ReadGame() when saved RULESET_VERSION patch < current.
 * Iterates patches[] in order — each entry fires once per save that hasn't seen it.
 *
 * To add a new balance notification:
 *   1. Bump RULESET_VERSION patch in neworigins/rules.cpp
 *   2. Add { new_patch, { skills... }, { items... } } to patches below
 * See docs/BALANCE_NOTIFICATION_SYSTEM.md
 */
bool Game::upgrade_patch_level(int current_version)
{
    // Each entry: { patch_number, { skill enums }, { item enums } }
    // Factions that know the skill / have seen the item get updated descriptions.
    //
    // Baselined empty for the NewAge 1.x line: the old 8.1.x notifications (heal, I_BOUNTY,
    // building) and the TMAP->RMAP migration were already delivered to the live world while it
    // ran on 8.1.x. Keeping them here would mis-fire after the move to 1.x, because the patch
    // counter resets on a minor/major version change and the old patch numbers would collide
    // with the fresh 1.x line. Add new 1.x balance notifications below as needed.
    static const std::vector<PatchNotification> patches = {};

    int cur = ATL_VER_PATCH(current_version);

    for (const auto& p : patches)
        if (cur < p.patch)
            deliver_balance_patch(p);

    return true;
}

// Convert every I_TREASURE_MAP held by any unit in the world to I_RESOURCE_MAP.
// Also migrates faction knowledge: factions that knew TMAP now know RMAP.
void Game::migrate_tmap_to_rmap()
{
    for (auto *r : regions) {
        for (auto *obj : r->objects) {
            for (auto *u : obj->units) {
                int n = u->items.GetNum(I_TREASURE_MAP);
                if (n <= 0) continue;
                u->items.SetNum(I_TREASURE_MAP, 0);
                u->items.SetNum(I_RESOURCE_MAP, u->items.GetNum(I_RESOURCE_MAP) + n);
            }
        }
    }
    // Migrate faction item-knowledge so DiscoverItem in deliver_balance_patch works.
    for (auto *fac : factions) {
        int known = fac->items.GetNum(I_TREASURE_MAP);
        if (known <= 0) continue;
        fac->items.SetNum(I_TREASURE_MAP, 0);
        // Mark RMAP as discovered (full = 2) so the faction gets the new description.
        if (fac->items.GetNum(I_RESOURCE_MAP) == 0)
            fac->items.SetNum(I_RESOURCE_MAP, 2);
    }
    logger::write("Patch 3: TMAP → RMAP conversion complete.");
}

void Game::deliver_balance_patch(const PatchNotification& p)
{
    for (auto fac : factions) {
        for (int sk : p.skills) {
            int max_lvl = fac->skills.GetDays(sk);
            for (int lvl = 1; lvl <= max_lvl; lvl++)
                fac->shows.push_back({ .skill = sk, .level = lvl });
        }
        for (int it : p.items) {
            if (fac->items.GetNum(it) > 0)
                fac->DiscoverItem(it, 1, 1);
        }
    }
}

void Game::MidProcessUnitExtra(ARegion *r, Unit *u)
{
    if (Globals->CHECK_MONSTER_CONTROL_MID_TURN) MonsterCheck(r, u);
}

void Game::PostProcessUnitExtra(ARegion *r, Unit *u)
{
    if (!Globals->CHECK_MONSTER_CONTROL_MID_TURN) MonsterCheck(r, u);
}

/**
 * @brief Pirates raid empty production buildings, roads, and inns in non-ocean regions.
 *
 * Each pirate in the unit acts independently: picks a random valid target and deals
 * 1 damage point (same as a 1-man unit with BUIL=0 in the DESTROY order).
 *
 * Target eligibility (re-evaluated per pirate):
 *   - Empty (no units inside)
 *   - Not NEVERDECAY
 *   - Type: production building (productionAided != -1), road (IsRoad()), or inn (O_INN)
 *   - structurePoints > 25% of cost  (don't finish off heavily damaged buildings)
 *   - o->destroyed < 25% of cost     (per-turn damage cap: max 25% of cost per turn)
 *
 * structurePoints = cost - incomplete
 * Damage: o->incomplete += 1, o->destroyed += 1 (destroyed resets each turn on load)
 *
 * @param r  The region where the unit is located.
 * @param u  The unit to check.
 */
void Game::PirateRaidBuildings(ARegion *r, Unit *u)
{
    // Only pirate wandering monsters
    if (u->type != U_WMON) return;
    int pirateCount = u->items.GetNum(I_PIRATES);
    if (pirateCount <= 0) return;

    // Only on non-ocean, non-lake terrain
    if (r->type == R_OCEAN || r->type == R_LAKE) return;

    // Snapshot initial incomplete for each object to determine per-turn cap
    std::map<Object *, int> initialIncomplete;
    for (const auto o : r->objects)
        initialIncomplete[o] = o->incomplete;

    std::set<Object *> damagedObjects;

    for (int i = 0; i < pirateCount; i++) {
        // Collect valid targets for this pirate
        std::vector<Object *> candidates;
        for (const auto o : r->objects) {
            if (o->type == O_DUMMY) continue;
            if (ObjectDefs[o->type].flags & ObjectType::NEVERDECAY) continue;
            if (!o->units.empty()) continue;

            const ObjectType& ot = ObjectDefs[o->type];
            if (ot.productionAided == -1 && !o->IsRoad() && o->type != O_INN) continue;

            int cost = ot.cost;
            int destroyThreshold = cost / 4;  // don't finish off badly damaged buildings
            int structurePoints = cost - o->incomplete;

            // Skip if too damaged (don't finish off)
            if (structurePoints <= destroyThreshold) continue;

            // Per-turn damage cap depends on initial state:
            //   functional (initial < 1): maxMaintenance+1 — enough to disable in one raid
            //   broken     (initial >= 1): 4 — slow additional damage
            int capThreshold = (initialIncomplete[o] < 1) ? ot.maxMaintenance + 1 : 4;
            if (o->destroyed >= capThreshold) continue;

            candidates.push_back(o);
        }

        if (candidates.empty()) break;

        // 50% chance this pirate attempts to raid
        if (rng::get_random(2) == 0) continue;

        // Each pirate picks a random target and deals 2 damage points
        Object *target = candidates[rng::get_random(candidates.size())];
        target->incomplete += 2;
        target->destroyed  += 2;
        damagedObjects.insert(target);
    }

    if (damagedObjects.empty()) return;

    // Build list of damaged building names
    std::string nameList;
    for (const auto o : damagedObjects) {
        if (!nameList.empty()) nameList += ", ";
        nameList += o->name;
    }

    // Notify all factions present in the region
    std::string msg = "Pirates from " + u->object->name + " raided and damaged " + nameList + " in " + r->short_print() + ".";
    std::set<Faction *> presentFactions = r->PresentFactions();
    for (const auto f : presentFactions) {
        f->event(msg, "decay", r, u);
    }

    // Collect for AI gazette context (pirate_context in times.json).
    // Elite fleets have a named captain (I_PIRATE_CAPTAIN); regular fleets do not.
    bool is_elite = u->items.GetNum(I_PIRATE_CAPTAIN) > 0;
    std::string ctx = (is_elite ? u->object->name : "A pirate fleet")
        + " raided buildings in " + r->name + ".";
    if (is_elite)
        pirate_context_elite.push_back(ctx);
    else
        pirate_context_regular.push_back(ctx);
}

void Game::MonsterCheck(ARegion *r, Unit *u)
{
    std::string tmp;
    int skill;
    int linked = 0;
    map< int, int > chances;

    if (u->type != U_WMON) {

        for(auto it = u->items.begin(); it != u->items.end();) {
            Item *i = *it;
            // Since items can be removed below, we will increment the iterator here
            it++;

            if (!i->num) continue;
            if (!ItemDefs[i->type].escape) continue;

            // Okay, check flat loss.
            if (ItemDefs[i->type].escape & ItemType::LOSS_CHANCE) {
                int losses = rng::calculate_losses(i->num, ItemDefs[i->type].esc_val);
                // LOSS_CHANCE and HAS_SKILL together mean the
                // decay rate only applies if you don't have
                // the required skill (this might get used if
                // you made illusions GIVEable, for example).
                if (ItemDefs[i->type].escape & ItemType::HAS_SKILL) {
                    skill = lookup_skill(ItemDefs[i->type].esc_skill);
                    if (u->GetSkill(skill) >= ItemDefs[i->type].esc_val) losses = 0;
                }
                if (losses) {
                    string temp = item_string(i->type, losses) + strings::plural(losses, " decay", " decays") +
                        " into nothingness.";
                    u->event(temp, "decay");
                    u->items.SetNum(i->type,i->num - losses);
                }
            } else if (ItemDefs[i->type].escape & ItemType::HAS_SKILL) {
                skill = lookup_skill(ItemDefs[i->type].esc_skill);
                if (u->GetSkill(skill) < ItemDefs[i->type].esc_val) {
                    if (Globals->WANDERING_MONSTERS_EXIST) {
                        Faction *mfac = GetFaction(factions, monfaction);
                        Unit *mon = GetNewUnit(mfac, 0);
                        auto monster = find_monster(ItemDefs[i->type].abr, (ItemDefs[i->type].type & IT_ILLUSION))->get();
                        mon->MakeWMon(monster.name.c_str(), i->type, i->num);
                        mon->MoveUnit(r->GetDummy());
                        // This will be zero unless these are set. (0 means
                        // full spoils)
                        mon->free = Globals->MONSTER_NO_SPOILS + Globals->MONSTER_SPOILS_RECOVERY;
                        mon->UpdateMonsterDescription();
                    }
                    u->event("Loses control of " + item_string(i->type, i->num) + ".", "escape");
                    u->items.SetNum(i->type, 0);
                }
            } else {
                // ESC_LEV_*
                skill = lookup_skill(ItemDefs[i->type].esc_skill);
                int level = u->GetSkill(skill);
                int chance;

                if (!level) chance = 10000;
                else {
                    int top = (ItemDefs[i->type].escape & ItemType::ESC_NUM_SQUARE) ? i->num * i->num : i->num;
                    int bottom = 0;
                    if (ItemDefs[i->type].escape & ItemType::ESC_LEV_LINEAR) bottom = level;
                    else if (ItemDefs[i->type].escape & ItemType::ESC_LEV_SQUARE) bottom = level * level;
                    else if (ItemDefs[i->type].escape & ItemType::ESC_LEV_CUBE) bottom = level * level * level;
                    else if (ItemDefs[i->type].escape & ItemType::ESC_LEV_QUAD) bottom = level * level * level * level;
                    else bottom = 1;
                    bottom = bottom * ItemDefs[i->type].esc_val;
                    chance = (top * 10000)/bottom;
                }

                if (ItemDefs[i->type].escape & ItemType::LOSE_LINKED) {
                    if (chance > chances[ItemDefs[i->type].type]) chances[ItemDefs[i->type].type] = chance;
                    linked = 1;
                } else if (chance > rng::get_random(10000)) {
                    if (Globals->WANDERING_MONSTERS_EXIST) {
                        Faction *mfac = GetFaction(factions, monfaction);
                        Unit *mon = GetNewUnit(mfac, 0);
                        auto monster = find_monster(ItemDefs[i->type].abr, (ItemDefs[i->type].type & IT_ILLUSION))->get();
                        mon->MakeWMon(monster.name.c_str(), i->type, i->num);
                        mon->MoveUnit(r->GetDummy());
                        // This will be zero unless these are set. (0 means full spoils)
                        mon->free = Globals->MONSTER_NO_SPOILS + Globals->MONSTER_SPOILS_RECOVERY;
                        mon->UpdateMonsterDescription();
                    }
                    u->event("Loses control of " + item_string(i->type, i->num) + ".", "escape");
                    u->items.SetNum(i->type, 0);
                }
            }
        }

        if (linked) {
            for (auto i = chances.begin(); i != chances.end(); i++) {
                // walk the chances list and for each chance, see if
                // escape happens and if escape happens then walk all items
                // and everything that is that type, get rid of it.
                if (i->second < rng::get_random(10000)) continue;
                for(auto iter = u->items.begin(); iter != u->items.end();) {
                    Item *it = *iter;
                    // Since items can be removed below, we will increment the iterator here
                    iter++;
                    if (ItemDefs[it->type].type == i->first) {
                        if (Globals->WANDERING_MONSTERS_EXIST) {
                            Faction *mfac = GetFaction(factions, monfaction);
                            Unit *mon = GetNewUnit(mfac, 0);
                            auto monster = find_monster(ItemDefs[it->type].abr, (ItemDefs[it->type].type & IT_ILLUSION))->get();
                            mon->MakeWMon(monster.name.c_str(), it->type, it->num);
                            mon->MoveUnit(r->GetDummy());
                            // This will be zero unless these are set. (0 means full spoils)
                            mon->free = Globals->MONSTER_NO_SPOILS + Globals->MONSTER_SPOILS_RECOVERY;
                            mon->UpdateMonsterDescription();
                        }
                        u->event("Loses control of " + item_string(it->type, it->num) + ".", "escape");
                        u->items.SetNum(it->type, 0);
                    }
                }
            }
        }

    }
}

void Game::CheckUnitMaintenance(int consume)
{
    CheckUnitMaintenanceItem(I_FOOD, Globals->UPKEEP_FOOD_VALUE, consume);
    CheckUnitMaintenanceItem(I_GRAIN, Globals->UPKEEP_FOOD_VALUE, consume);
    CheckUnitMaintenanceItem(I_LIVESTOCK, Globals->UPKEEP_FOOD_VALUE, consume);
    CheckUnitMaintenanceItem(I_FISH, Globals->UPKEEP_FOOD_VALUE, consume);
}

void Game::CheckFactionMaintenance(int con)
{
    CheckFactionMaintenanceItem(I_FOOD, Globals->UPKEEP_FOOD_VALUE, con);
    CheckFactionMaintenanceItem(I_GRAIN, Globals->UPKEEP_FOOD_VALUE, con);
    CheckFactionMaintenanceItem(I_LIVESTOCK, Globals->UPKEEP_FOOD_VALUE, con);
    CheckFactionMaintenanceItem(I_FISH, Globals->UPKEEP_FOOD_VALUE, con);
}

void Game::CheckAllyMaintenance()
{
    CheckAllyMaintenanceItem(I_FOOD, Globals->UPKEEP_FOOD_VALUE);
    CheckAllyMaintenanceItem(I_GRAIN, Globals->UPKEEP_FOOD_VALUE);
    CheckAllyMaintenanceItem(I_LIVESTOCK, Globals->UPKEEP_FOOD_VALUE);
    CheckAllyMaintenanceItem(I_FISH, Globals->UPKEEP_FOOD_VALUE);
}

void Game::CheckUnitHunger()
{
    CheckUnitHungerItem(I_FOOD, Globals->UPKEEP_FOOD_VALUE);
    CheckUnitHungerItem(I_GRAIN, Globals->UPKEEP_FOOD_VALUE);
    CheckUnitHungerItem(I_LIVESTOCK, Globals->UPKEEP_FOOD_VALUE);
    CheckUnitHungerItem(I_FISH, Globals->UPKEEP_FOOD_VALUE);
}

void Game::CheckFactionHunger()
{
    CheckFactionHungerItem(I_FOOD, Globals->UPKEEP_FOOD_VALUE);
    CheckFactionHungerItem(I_GRAIN, Globals->UPKEEP_FOOD_VALUE);
    CheckFactionHungerItem(I_LIVESTOCK, Globals->UPKEEP_FOOD_VALUE);
    CheckFactionHungerItem(I_FISH, Globals->UPKEEP_FOOD_VALUE);
}

void Game::CheckAllyHunger()
{
    CheckAllyHungerItem(I_FOOD, Globals->UPKEEP_FOOD_VALUE);
    CheckAllyHungerItem(I_GRAIN, Globals->UPKEEP_FOOD_VALUE);
    CheckAllyHungerItem(I_LIVESTOCK, Globals->UPKEEP_FOOD_VALUE);
    CheckAllyHungerItem(I_FISH, Globals->UPKEEP_FOOD_VALUE);
}

char Game::GetRChar(ARegion *r)
{
    int t;

    if (!r)
        return ' ';
    t = r->type;
    if (t < 0 || t > R_NUM) return '?';
    char c = TerrainDefs[r->type].marker;
    if (r->town) {
        c = (c - 'a') + 'A';
    }
    return c;
}

void Game::CreateNPCFactions()
{
    Faction *f;
    if (Globals->CITY_MONSTERS_EXIST) {
        f = new Faction(factionseq++);
        guardfaction = f->num;
        f->set_name("The Guardsmen");
        f->is_npc = true;
        f->lastorders = 0;
        factions.push_back(f);
    } else guardfaction = 0;
    // Only create the monster faction if wandering monsters or lair
    // monsters exist.
    if (Globals->LAIR_MONSTERS_EXIST || Globals->WANDERING_MONSTERS_EXIST) {
        f = new Faction(factionseq++);
        monfaction = f->num;
        f->set_name("Creatures");
        f->is_npc = true;
        f->lastorders = 0;
        factions.push_back(f);
    } else monfaction = 0;

    // Optionally start player faction numbering at a fixed value. Guarded by >,
    // so it only pulls factionseq up (never down): 0 is a no-op and a value <= the
    // current NPC count cannot cause a collision. The lower slots stay empty. This
    // runs only at `new`, and factionseq is baked into game.out, so the shift is
    // permanent for the world and has no effect if changed later.
    if (Globals->PLAYER_FACTION_NUM_START > factionseq)
        factionseq = Globals->PLAYER_FACTION_NUM_START;
}

void Game::CreateGuardMelee(ARegion *region, int percent)
{
    if (!region->town && region->type != R_NEXUS) return;

    int skilllevel;
    int AC = 0;
    int num;

    if (region->type == R_NEXUS || region->IsStartingCity()) {
        skilllevel = TOWN_CITY + 2;
        AC = 1;
        num = Globals->AMT_START_CITY_GUARDS;
    } else {
        skilllevel = region->town->TownType() + 2;
        num = Globals->CITY_GUARD * (region->town->TownType() + 1);
    }
    num = num * percent / 100;

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);

    std::string townname = (region->town) ? region->town->name : "City";
    std::string melee_name;

    if (region->type == R_NEXUS) {
        melee_name = "City Guard";
    } else {
        int tt = region->town ? region->town->TownType() : TOWN_CITY;
        switch(tt) {
            case TOWN_VILLAGE:
                melee_name = townname + " Militia";
                break;
            case TOWN_TOWN:
                melee_name = townname + " Town Guard";
                break;
            default:
                melee_name = townname + " City Guard";
                break;
        }
    }

    if ((Globals->GUARDS_USE_LEADERS) || (region->type == R_NEXUS)) {
        // Leader-type guards
        u->SetMen(I_LEADERS, num);

        // Equipment will be added by AdjustCityMon on next turn
        // (unless starting city)
        if (AC && Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
            u->items.SetNum(I_SWORD, num);
            u->items.SetNum(I_PLATEARMOR, num);
            u->items.SetNum(I_ISHIELD, num);
        }

        if (Globals->SAFE_START_CITIES && AC)
            u->items.SetNum(I_AMULETOFI, num);

        u->SetMoney(num * Globals->GUARD_MONEY);
        u->SetSkill(S_COMBAT, skilllevel);
        u->set_name(melee_name);
        u->type = U_GUARD;
        u->guard = GUARD_GUARD;
        u->reveal = REVEAL_FACTION;
    } else {
        // Non-leader racial guards (melee front line)
        int melee_n = num / 2;

        u->SetMen(region->race, melee_n);
        u->SetSkill(S_COMBAT, skilllevel);

        // Equipment will be added by AdjustCityMon on next turn
        if (AC && Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
            u->items.SetNum(I_SWORD, melee_n);
            u->items.SetNum(I_PLATEARMOR, melee_n);
            u->items.SetNum(I_ISHIELD, melee_n);
        }

        if (Globals->SAFE_START_CITIES && AC)
            u->items.SetNum(I_AMULETOFI, melee_n);

        u->SetMoney(melee_n * Globals->GUARD_MONEY);
        u->set_name(melee_name);
        u->type = U_GUARD;
        u->guard = GUARD_GUARD;
        u->reveal = REVEAL_FACTION;
    }

    if (AC) {
        if (Globals->START_CITY_GUARDS_PLATE && Globals->GUARDS_USE_LEADERS)
            u->items.SetNum(I_PLATEARMOR, num);
        u->SetSkill(S_OBSERVATION, 10);
        if (Globals->START_CITY_TACTICS)
            u->SetSkill(S_TACTICS, Globals->START_CITY_TACTICS);
    } else {
        u->SetSkill(S_OBSERVATION, skilllevel + 2);  // towntype + 3
    }

    u->SetFlag(FLAG_HOLDING, 1);
    u->MoveUnit(region->GetDummy());
}

void Game::CreateGuardRanged(ARegion *region, int percent)
{
    if (!region->town && region->type != R_NEXUS) return;
    if (Globals->GUARDS_USE_LEADERS || region->type == R_NEXUS) return; // Ranged only for non-leader guards

    int skilllevel;
    int AC = 0;
    int num;

    if (region->type == R_NEXUS || region->IsStartingCity()) {
        skilllevel = TOWN_CITY + 2;
        AC = 1;
        num = Globals->AMT_START_CITY_GUARDS;
    } else {
        skilllevel = region->town->TownType() + 2;
        num = Globals->CITY_GUARD * (region->town->TownType() + 1);
    }
    num = num * percent / 100;

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);

    std::string townname = (region->town) ? region->town->name : "City";
    std::string ranged_name;

    if (region->type == R_NEXUS) {
        ranged_name = "City Archers";
    } else {
        ranged_name = townname + " Archers";
    }

    // Non-leader racial guards (ranged rear line)
    int ranged_n = num / 2;
    if (ranged_n < 1) ranged_n = 1;

    u->SetMen(region->race, ranged_n);
    u->SetSkill(S_LONGBOW, skilllevel);
    u->SetFlag(FLAG_BEHIND, 1);

    // Equipment will be added by AdjustCityMon on next turn
    if (AC && Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
        // Starting city archers: longbow + leather armor
        u->items.SetNum(I_LONGBOW, ranged_n);
        u->items.SetNum(I_LEATHERARMOR, ranged_n);
    }

    if (Globals->SAFE_START_CITIES && AC)
        u->items.SetNum(I_AMULETOFI, ranged_n);

    u->SetMoney(ranged_n * Globals->GUARD_MONEY);
    u->set_name(ranged_name);
    u->type = U_GUARD;
    u->guard = GUARD_GUARD;
    u->reveal = REVEAL_FACTION;
    u->SetSkill(S_OBSERVATION, skilllevel + 2);  // towntype + 3
    u->SetFlag(FLAG_HOLDING, 1);
    u->MoveUnit(region->GetDummy());
}

void Game::CreateGuardMageESHI(ARegion *region)
{
    if (!region->town && region->type != R_NEXUS) return;

    int skilllevel;
    int AC = 0;
    int IV = 0;

    if (region->type == R_NEXUS || region->IsStartingCity()) {
        skilllevel = TOWN_CITY + 1;
        if (Globals->SAFE_START_CITIES || (region->type == R_NEXUS))
            IV = 1;
        AC = 1;
    } else {
        skilllevel = region->town->TownType() + 1;
    }

    int magelevel = skilllevel + 1;  // ESHI gets +1 bonus level
    if (AC && Globals->START_CITY_MAGES > magelevel)
        magelevel = Globals->START_CITY_MAGES;

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);

    std::string townname = (region->town) ? region->town->name : "City";
    std::string eshi_name;

    if (region->type == R_NEXUS) {
        eshi_name = "Arcane Flameguard";
    } else {
        int tt = region->town ? region->town->TownType() : TOWN_CITY;
        switch(tt) {
            case TOWN_VILLAGE:
                eshi_name = townname + " Flameguard";
                break;
            case TOWN_TOWN:
                eshi_name = townname + " Flame Warden";
                break;
            default:
                eshi_name = townname + " Arcane Flameguard";
                break;
        }
    }

    u->set_name(eshi_name);
    u->type = U_GUARDMAGE;
    u->reveal = REVEAL_FACTION;
    u->SetMen(I_LEADERS, 1);
    if (IV) u->items.SetNum(I_AMULETOFI, 1);
    u->SetMoney(Globals->GUARD_MONEY);
    u->SetSkill(S_FORCE, magelevel);
    u->SetSkill(S_ENERGY_SHIELD, magelevel);
    u->guard = GUARD_NONE;
    u->SetFlag(FLAG_BEHIND, 1);
    u->SetFlag(FLAG_HOLDING, 1);
    u->combat = S_ENERGY_SHIELD;
    u->MoveUnit(region->GetDummy());
}

void Game::CreateGuardMageFSHI(ARegion *region)
{
    if (!region->town && region->type != R_NEXUS) return;

    int skilllevel;
    int AC = 0;
    int IV = 0;

    if (region->type == R_NEXUS || region->IsStartingCity()) {
        skilllevel = TOWN_CITY + 1;
        if (Globals->SAFE_START_CITIES || (region->type == R_NEXUS))
            IV = 1;
        AC = 1;
    } else {
        skilllevel = region->town->TownType() + 1;
    }

    int magelevel = skilllevel;
    if (AC && Globals->START_CITY_MAGES > magelevel)
        magelevel = Globals->START_CITY_MAGES;

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);

    std::string townname = (region->town) ? region->town->name : "City";
    std::string fshi_name;

    if (region->type == R_NEXUS) {
        fshi_name = "Shieldmaster";
    } else {
        int tt = region->town ? region->town->TownType() : TOWN_CITY;
        if (tt == TOWN_TOWN) {
            fshi_name = townname + " Shield Warden";
        } else {
            fshi_name = townname + " Shieldmaster";
        }
    }

    u->set_name(fshi_name);
    u->type = U_GUARDMAGE;
    u->reveal = REVEAL_FACTION;
    u->SetMen(I_LEADERS, 1);
    if (IV) u->items.SetNum(I_AMULETOFI, 1);
    u->SetMoney(Globals->GUARD_MONEY);
    u->SetSkill(S_FORCE, magelevel);
    u->SetSkill(S_FORCE_SHIELD, magelevel);
    u->guard = GUARD_NONE;
    u->SetFlag(FLAG_BEHIND, 1);
    u->SetFlag(FLAG_HOLDING, 1);
    u->combat = S_FORCE_SHIELD;
    u->MoveUnit(region->GetDummy());
}

void Game::CreateGuardMageFIRE(ARegion *region)
{
    if (!region->town && region->type != R_NEXUS) return;

    int skilllevel;
    int AC = 0;
    int IV = 0;

    if (region->type == R_NEXUS || region->IsStartingCity()) {
        skilllevel = TOWN_CITY + 1;
        if (Globals->SAFE_START_CITIES || (region->type == R_NEXUS))
            IV = 1;
        AC = 1;
    } else {
        skilllevel = region->town->TownType() + 1;
    }

    int magelevel = skilllevel;
    if (AC && Globals->START_CITY_MAGES > magelevel)
        magelevel = Globals->START_CITY_MAGES;

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);

    std::string townname = (region->town) ? region->town->name : "City";
    std::string fire_name;

    if (region->type == R_NEXUS) {
        fire_name = "Court Fire Arcanist";
    } else {
        fire_name = townname + " Fire Arcanist";
    }

    u->set_name(fire_name);
    u->type = U_GUARDMAGE;
    u->reveal = REVEAL_FACTION;
    u->SetMen(I_LEADERS, 1);
    if (IV) u->items.SetNum(I_AMULETOFI, 1);
    u->SetMoney(Globals->GUARD_MONEY);
    u->SetSkill(S_FORCE, magelevel);
    u->SetSkill(S_FIRE, magelevel);
    u->guard = GUARD_NONE;
    u->SetFlag(FLAG_BEHIND, 1);
    u->SetFlag(FLAG_HOLDING, 1);
    u->combat = S_FIRE;
    u->MoveUnit(region->GetDummy());
}

void Game::CreateCityMon(ARegion *region, int percent, int needmage)
{
    int skilllevel;
    int AC = 0;
    int IV = 0;
    int num;
    if (region->type == R_NEXUS || region->IsStartingCity()) {
        skilllevel = TOWN_CITY + 1;
        if (Globals->SAFE_START_CITIES || (region->type == R_NEXUS))
            IV = 1;
        AC = 1;
        num = Globals->AMT_START_CITY_GUARDS;
    } else {
        skilllevel = region->town->TownType() + 2;
        num = Globals->CITY_GUARD * (region->town->TownType() + 1);
    }
    num = num * percent / 100;
    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);
    Unit *u2;

    // Determine guard unit names based on town name and type
    std::string townname = (region->town) ? region->town->name : "City";
    std::string melee_name, ranged_name, eshi_name, fshi_name, fire_name;
    if (region->type == R_NEXUS) {
        melee_name = "City Guard";
        ranged_name = "City Archers";
        eshi_name = "Arcane Flameguard";
        fshi_name = "Shieldmaster";
        fire_name = "Court Fire Arcanist";
    } else {
        int tt = region->town ? region->town->TownType() : TOWN_CITY;
        switch(tt) {
            case TOWN_VILLAGE:
                melee_name = townname + " Militia";
                ranged_name = townname + " Archers";
                eshi_name = townname + " Flameguard";
                break;
            case TOWN_TOWN:
                melee_name = townname + " Town Guard";
                ranged_name = townname + " Archers";
                eshi_name = townname + " Flame Warden";
                fshi_name = townname + " Shield Warden";
                break;
            default:
                melee_name = townname + " City Guard";
                ranged_name = townname + " Archers";
                eshi_name = townname + " Arcane Flameguard";
                fshi_name = townname + " Shieldmaster";
                fire_name = townname + " Fire Arcanist";
                break;
        }
    }

    if ((Globals->GUARDS_USE_LEADERS) || (region->type == R_NEXUS)) {
        /* standard Leader-type guards */
        u->SetMen(I_LEADERS,num);

        // Equipment assignment based on GUARDS_EQUIPMENT_BY_TOWN_TYPE flag
        if (Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
            // New system: equipment scales by town type
            if (AC) {
                // Starting city/Nexus: give best equipment immediately
                u->items.SetNum(I_SWORD, num);
                u->items.SetNum(I_PLATEARMOR, num);
                u->items.SetNum(I_ISHIELD, num);
            }
            // Regular towns: no equipment (will be added by AdjustCityMon() next turn)
        } else {
            // Old system: sword only for all guards
            u->items.SetNum(I_SWORD, num);
        }

        if (IV) u->items.SetNum(I_AMULETOFI,num);
        u->SetMoney(num * Globals->GUARD_MONEY);
        u->SetSkill(S_COMBAT,skilllevel);
        u->set_name(melee_name);
        u->type = U_GUARD;
        u->guard = GUARD_GUARD;
        u->reveal = REVEAL_FACTION;
    } else {
        /* non-leader racial guards: melee front + ranged rear (1:1 ratio) */
        int melee_n = num / 2;
        int ranged_n = num / 2;
        if (ranged_n < 1) ranged_n = 1;

        /* Front line: melee unit */
        u->SetMen(region->race, melee_n);
        u->SetSkill(S_COMBAT, skilllevel);

        if (Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
            if (AC) {
                // Starting city/Nexus: give best equipment immediately
                u->items.SetNum(I_SWORD, melee_n);
                u->items.SetNum(I_PLATEARMOR, melee_n);
                u->items.SetNum(I_ISHIELD, melee_n);
            }
            // Regular towns: no equipment (AdjustCityMon assigns next turn)
        } else {
            u->items.SetNum(I_SWORD, melee_n);
        }

        if (IV) u->items.SetNum(I_AMULETOFI, melee_n);
        u->SetMoney(melee_n * Globals->GUARD_MONEY);
        u->set_name(melee_name);
        u->type = U_GUARD;
        u->guard = GUARD_GUARD;
        u->reveal = REVEAL_FACTION;

        /* Rear line: ranged unit (longbow) */
        u2 = GetNewUnit(fac);
        u2->SetMen(region->race, ranged_n);
        u2->SetSkill(S_LONGBOW, skilllevel);
        u2->SetFlag(FLAG_BEHIND, 1);

        if (Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
            if (AC) {
                u2->items.SetNum(I_LONGBOW, ranged_n);
            }
            // Regular towns: no equipment (AdjustCityMon assigns next turn)
        } else {
            u2->items.SetNum(I_LONGBOW, ranged_n);
        }

        if (IV) u2->items.SetNum(I_AMULETOFI, ranged_n);
        u2->SetMoney(ranged_n * Globals->GUARD_MONEY);
        u2->set_name(ranged_name);
        u2->type = U_GUARD;
        u2->guard = GUARD_GUARD;
        u2->reveal = REVEAL_FACTION;
    }

    if (AC) {
        if (Globals->START_CITY_GUARDS_PLATE) {
            if (Globals->GUARDS_USE_LEADERS) u->items.SetNum(I_PLATEARMOR, num);
        }
        u->SetSkill(S_OBSERVATION,10);
        if (Globals->START_CITY_TACTICS)
            u->SetSkill(S_TACTICS, Globals->START_CITY_TACTICS);
    } else {
        u->SetSkill(S_OBSERVATION, skilllevel + 2);  // towntype + 3
    }
    u->SetFlag(FLAG_HOLDING,1);
    u->MoveUnit(region->GetDummy());
    if ((!Globals->GUARDS_USE_LEADERS) && (region->type != R_NEXUS)) {
        u2->SetFlag(FLAG_HOLDING,1);
        u2->MoveUnit(region->GetDummy());
    }

    // Always create a Guard Commander (tactical cavalry leader)
    CreateGuardCommander(region);

    // Always create a Mayor (city administrator, symbolic leader)
    CreateMayor(region);

    if (needmage) {
        int magelevel = skilllevel; // towntype + 1: Village=1, Town=2, City=3
        // For starting cities, use START_CITY_MAGES if higher
        if (AC && Globals->START_CITY_MAGES > magelevel)
            magelevel = Globals->START_CITY_MAGES;

        int tt = (region->type == R_NEXUS) ? TOWN_CITY :
                 (region->town ? region->town->TownType() : TOWN_CITY);

        // ESHI mage (all towns): Energy Shield (+1 bonus), no Tactics
        u = GetNewUnit(fac);
        u->set_name(eshi_name);
        u->type = U_GUARDMAGE;
        u->reveal = REVEAL_FACTION;
        u->SetMen(I_LEADERS, 1);
        if (IV) u->items.SetNum(I_AMULETOFI, 1);
        u->SetMoney(Globals->GUARD_MONEY);
        u->SetSkill(S_FORCE, magelevel + 1);
        u->SetSkill(S_ENERGY_SHIELD, magelevel + 1);  // +1 bonus for ESHI
        u->guard = GUARD_NONE;
        u->SetFlag(FLAG_BEHIND, 1);
        u->SetFlag(FLAG_HOLDING, 1);
        u->combat = S_ENERGY_SHIELD;  // Set combat AFTER all other setup
        u->MoveUnit(region->GetDummy());

        // FSHI mage (Town+): Force Shield, no Tactics
        if (tt >= TOWN_TOWN) {
            u = GetNewUnit(fac);
            u->set_name(fshi_name);
            u->type = U_GUARDMAGE;
            u->reveal = REVEAL_FACTION;
            u->SetMen(I_LEADERS, 1);
            if (IV) u->items.SetNum(I_AMULETOFI, 1);
            u->SetMoney(Globals->GUARD_MONEY);
            u->SetSkill(S_FORCE, magelevel);
            u->SetSkill(S_FORCE_SHIELD, magelevel);
            u->guard = GUARD_NONE;
            u->SetFlag(FLAG_BEHIND, 1);
            u->SetFlag(FLAG_HOLDING, 1);
            u->combat = S_FORCE_SHIELD;  // Set combat AFTER all other setup
            u->MoveUnit(region->GetDummy());
        }

        // FIRE mage (City+): Fire attack, no Tactics
        if (tt >= TOWN_CITY) {
            u = GetNewUnit(fac);
            u->set_name(fire_name);
            u->type = U_GUARDMAGE;
            u->reveal = REVEAL_FACTION;
            u->SetMen(I_LEADERS, 1);
            if (IV) u->items.SetNum(I_AMULETOFI, 1);
            u->SetMoney(Globals->GUARD_MONEY);
            u->SetSkill(S_FORCE, magelevel);
            u->SetSkill(S_FIRE, magelevel);
            u->guard = GUARD_NONE;
            u->SetFlag(FLAG_BEHIND, 1);
            u->SetFlag(FLAG_HOLDING, 1);
            u->combat = S_FIRE;  // Set combat AFTER all other setup
            u->MoveUnit(region->GetDummy());
        }
    }
}

void Game::AdjustCityMons(ARegion *r)
{
    if (!r->town && r->type != R_NEXUS) return;

    int towntype = r->town ? r->town->TownType() : TOWN_CITY;

    // Determine what SHOULD exist based on towntype
    bool should_have_melee = true;
    bool should_have_ranged = !Globals->GUARDS_USE_LEADERS && (r->type != R_NEXUS);
    bool should_have_eshi = true;
    bool should_have_fshi = (towntype >= TOWN_TOWN);
    bool should_have_fire = (towntype >= TOWN_CITY);
    bool should_have_commander = true;

    // Check what DOES exist and collect player factions on guard
    bool has_melee = false;
    bool has_ranged = false;
    bool has_eshi = false;
    bool has_fshi = false;
    bool has_fire = false;
    bool has_commander = false;
    bool has_mayor = false;
    Unit *mayor_unit = nullptr;
    std::set<int> guarding_player_facs;       // faction nums of non-guard players on GUARD

    for(const auto o : r->objects) {
        for(const auto u : o->units) {
            // Collect all non-guardfaction players standing on GUARD in the region
            if (u->guard == GUARD_GUARD && u->faction->num != guardfaction) {
                guarding_player_facs.insert(u->faction->num);
            }

            if (u->type == U_GUARD) {
                AdjustCityMon(r, u);
                if (u->GetFlag(FLAG_BEHIND))
                    has_ranged = true;
                else
                    has_melee = true;
            }

            if (u->type == U_GUARDMAGE) {
                AdjustCityMon(r, u);
                if (u->combat == S_ENERGY_SHIELD)
                    has_eshi = true;
                else if (u->combat == S_FORCE_SHIELD)
                    has_fshi = true;
                else if (u->combat == S_FIRE)
                    has_fire = true;
            }

            if (u->type == U_GUARDCOMMANDER) {
                AdjustCityMon(r, u);
                has_commander = true;
            }

            if (u->type == U_MAYOR && u->GetMen() > 0) {
                has_mayor = true;
                mayor_unit = u;
            }

            if (u->type == U_MAYOR && u->GetMen() == 0) {
                // Mayor killed in combat this turn — clean up their quests now.
                std::vector<std::shared_ptr<Quest>> to_purge;
                for (const auto& q : quests) {
                    if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == u->num)
                        to_purge.push_back(q);
                }
                for (auto& q : to_purge) quests.erase_with_cleanup(q, &factions);
            }
        }
    }

    bool player_on_guard = !guarding_player_facs.empty();

    // Two attitude thresholds for mayor logic:
    // mayor_can_stay  — existing mayor flees only at HOSTILE (UNFRIENDLY is a soft
    //                   penalty; the mayor stays but quest redemption is gated).
    // mayor_can_spawn — new mayor requires NEUTRAL or better; UNFRIENDLY blocks spawn
    //                   (e.g. attacker killed the previous mayor this very turn).
    bool mayor_can_stay  = true;
    bool mayor_can_spawn = true;
    if (player_on_guard) {
        Faction *gfac = GetFaction(factions, guardfaction);
        for (int fnum : guarding_player_facs) {
            AttitudeType att = gfac->get_attitude(fnum);
            if (att == AttitudeType::HOSTILE) {
                mayor_can_stay  = false;
                mayor_can_spawn = false;
                break;
            }
            if (att == AttitudeType::UNFRIENDLY)
                mayor_can_spawn = false;
        }
    }

    // Calculate melee men count (after AdjustCityMon top-up) and expected max
    int melee_men = 0;
    int melee_max = 0;
    if (r->type == R_NEXUS || r->IsStartingCity()) {
        int base = Globals->AMT_START_CITY_GUARDS;
        melee_max = Globals->GUARDS_USE_LEADERS ? base : base / 2;
    } else {
        int base = Globals->CITY_GUARD * (towntype + 1);
        melee_max = Globals->GUARDS_USE_LEADERS ? base : base / 2;
    }
    if (has_melee) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->type == U_GUARD && !u->GetFlag(FLAG_BEHIND)) {
                    melee_men = u->GetMen();
                    break;
                }
            }
        }
    }

    // --- Mayor disappearance: split by location (plan §3.4) ---
    // Mayor in dummy: flee at <50% melee_max OR attitude break.
    // Mayor in Town Hall: ONLY attitude break (hall acts as a civic refuge,
    // decoupled from guard health to avoid yo-yo with no-melee-gate spawn).
    if (has_mayor && mayor_unit) {
        bool mayor_in_hall = (mayor_unit->object && mayor_unit->object->type == O_TOWN_HALL);
        bool flee_by_guards = !mayor_in_hall && melee_men < melee_max * 50 / 100;
        bool flee_by_attitude = !mayor_can_stay;
        if (flee_by_guards || flee_by_attitude) {
            mayor_unit->SetMen(I_LEADERS, 0);   // remove the mayor (men + skills)
            // Mayor flees taking ALL his possessions with him — leave nothing
            // behind. Otherwise ARegion::Kill() would hand his gear (mithril
            // weapon/armor, shieldstone, etc.) to the first fellow guard unit
            // in the region, leaking artifacts into the city guards.
            for (auto i : mayor_unit->items) i->num = 0;
            // Erase all LOCAL quests issued by this mayor.
            // Faction debts are preserved — players keep their earned rewards.
            std::vector<std::shared_ptr<Quest>> to_purge;
            for (const auto& q : quests) {
                if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == mayor_unit->num)
                    to_purge.push_back(q);
            }
            for (auto& q : to_purge) quests.erase_with_cleanup(q, &factions);
            has_mayor = false;
            mayor_unit = nullptr;
        }
    }

    // Mayor adjust (equipment, treasury payment, name upgrade)
    if (has_mayor && mayor_unit) {
        AdjustCityMon(r, mayor_unit);
    }

    // Determine what needs to be created
    bool need_melee = should_have_melee && !has_melee;
    bool need_ranged = should_have_ranged && !has_ranged;
    bool need_eshi = should_have_eshi && !has_eshi;
    bool need_fshi = should_have_fshi && !has_fshi;
    bool need_fire = should_have_fire && !has_fire;
    bool need_commander = should_have_commander && !has_commander;

    bool need_something = need_melee || need_ranged || need_eshi || need_fshi || need_fire || need_commander;
    bool has_any_guards = has_melee || has_ranged || has_eshi || has_fshi || has_fire || has_commander;

    // Regenerate guards if: no player on guard AND something is missing
    if (!player_on_guard && need_something) {
        bool should_regenerate = false;

        if (has_any_guards) {
            should_regenerate = true;
        } else {
            should_regenerate = (rng::get_random(100) < Globals->GUARD_REGEN);
        }

        if (should_regenerate) {
            if (need_melee) CreateGuardMelee(r, 20);
            if (need_ranged) CreateGuardRanged(r, 20);
            if (need_eshi) CreateGuardMageESHI(r);
            if (need_fshi) CreateGuardMageFSHI(r);
            if (need_fire) CreateGuardMageFIRE(r);
            if (need_commander) CreateGuardCommander(r);
        }
    }

    // --- Find an empty completed Town Hall (plan §3.2/§3.3) ---
    // "Completed" = incomplete <= 0 (negative values track decay/maintenance reserve).
    Object *empty_hall = nullptr;
    for (const auto o : r->objects) {
        if (o->type == O_TOWN_HALL && o->incomplete <= 0 && o->units.empty()) {
            empty_hall = o;
            break;
        }
    }

    // --- Mayor spawn (plan §3.2) ---
    // Need: someone on guard (city guard OR player) AND attitude ≥ NEUTRAL to all guarding players.
    // Branch: empty completed hall → spawn into hall, no melee% gate.
    //         No hall → spawn in dummy, requires melee_men ≥ 75% × melee_max.
    bool someone_on_guard = has_melee || player_on_guard;
    if (!has_mayor && someone_on_guard && mayor_can_spawn) {
        if (empty_hall) {
            CreateMayor(r, empty_hall);
        } else if (has_melee && melee_men >= melee_max * 75 / 100) {
            CreateMayor(r);
        }
    }

    // --- Move existing mayor from dummy into a freshly-built Town Hall (plan §3.3) ---
    if (has_mayor && mayor_unit && mayor_can_stay && empty_hall &&
        mayor_unit->object == r->GetDummy())
    {
        mayor_unit->MoveUnit(empty_hall);
    }
}

// ---------------------------------------------------------------------------
// Mayor treasury constants — edit here to tune mayor starting funds / income
// ---------------------------------------------------------------------------
namespace {
    /// Silver granted per town tier on spawn (village=1×, town=2×, city=3×)
    /// and also added as a bonus when the town levels up.
    constexpr int MAYOR_TREASURY_PER_LEVEL = 1000;
    /// Percent of region taxable income added to mayor's treasury each turn.
    constexpr int MAYOR_INCOME_PCT = 10;
}

/**
 * @brief Helper function: Determine weapon tier level
 * @return 1-3 for standard guard weapons, 0 for unknown
 */
int GetWeaponTier(int itemtype) {
    if (itemtype == I_SPEAR) return 1;
    if (itemtype == I_PIKE) return 2;
    if (itemtype == I_SWORD) return 3;
    if (itemtype == I_LONGBOW) return 1; // Longbow doesn't upgrade by town type
    return 0; // Unknown weapon
}

/**
 * @brief Helper function: Determine armor tier level
 * @return 1-3 for standard guard armor, 0 for none/unknown
 */
int GetArmorTier(int itemtype) {
    if (itemtype == I_LEATHERARMOR) return 1;
    if (itemtype == I_CHAINARMOR) return 2;
    if (itemtype == I_PLATEARMOR) return 3;
    return 0; // No armor or unknown
}

/**
 * @brief Helper function: Determine shield tier level
 * @return 1-2 for standard guard shields, 0 for none/unknown
 */
int GetShieldTier(int itemtype) {
    if (itemtype == I_WSHIELD) return 1; // Wooden shield
    if (itemtype == I_ISHIELD) return 2; // Iron shield
    return 0; // No shield or unknown
}


void Game::AdjustCityMon(ARegion *r, Unit *u)
{
    // Mayor has its own dedicated upgrade logic
    if (u->type == U_MAYOR) {
        bool AC = r->IsStartingCity() || r->type == R_NEXUS;
        int tt = AC ? TOWN_CITY : (r->town ? r->town->TownType() : TOWN_CITY);
        bool IV = AC && (Globals->SAFE_START_CITIES || r->type == R_NEXUS);

        // Weapon tier: SWOR(1) → MSWO(2) → ASWR(3)
        auto get_sword_tier = [](int item) -> int {
            if (item == I_SWORD)   return 1;
            if (item == I_MSWORD)  return 2;
            if (item == I_ADSWORD) return 3;
            return 0;
        };
        // Armor tier: CARM(1) → MCAR(2) → ARNG(3)
        auto get_chain_tier = [](int item) -> int {
            if (item == I_CHAINARMOR) return 1;
            if (item == I_MCHAIN)     return 2;
            if (item == I_ADRING)     return 3;
            return 0;
        };

        int req_weapon, req_armor;
        switch (tt) {
            case TOWN_VILLAGE: req_weapon = I_SWORD;   req_armor = I_CHAINARMOR; break;
            case TOWN_TOWN:    req_weapon = I_MSWORD;  req_armor = I_MCHAIN;     break;
            default:           req_weapon = I_ADSWORD; req_armor = I_ADRING;     break;
        }

        int cur_weapon = -1, cur_armor = -1;
        for (int i = 0; i < NITEMS; i++) {
            if (u->items.GetNum(i) == 0) continue;
            if (i == I_SWORD || i == I_MSWORD || i == I_ADSWORD)         cur_weapon = i;
            if (i == I_CHAINARMOR || i == I_MCHAIN || i == I_ADRING)     cur_armor  = i;
        }

        // Upgrade weapon (never downgrade)
        if (cur_weapon == -1) {
            cur_weapon = req_weapon;
        } else if (get_sword_tier(req_weapon) > get_sword_tier(cur_weapon)) {
            u->items.SetNum(cur_weapon, 0);
            cur_weapon = req_weapon;
        }
        // Upgrade armor (never downgrade)
        if (cur_armor == -1) {
            cur_armor = req_armor;
        } else if (get_chain_tier(req_armor) > get_chain_tier(cur_armor)) {
            u->items.SetNum(cur_armor, 0);
            cur_armor = req_armor;
        }

        u->SetMen(I_LEADERS, 1);
        u->items.SetNum(cur_weapon, 1);
        u->items.SetNum(cur_armor, 1);
        u->items.SetNum(I_CORNUCOPIA, 1);    // City key: enables EART casting
        u->items.SetNum(I_SHIELDSTONE, 1);   // Mayor's protective artifact
        if (IV) u->items.SetNum(I_AMULETOFI, 1);
        if (u->GetMoney() < (int)Globals->GUARD_MONEY)
            u->SetMoney(Globals->GUARD_MONEY);

        // S_OBSERVATION 3/4/5 — rank indicator (upgrade only)
        int cur_obs = u->GetRealSkill(S_OBSERVATION);
        u->SetSkill(S_OBSERVATION, std::max(cur_obs, tt + 3));

        u->SetFlag(FLAG_BEHIND, 1);
        u->SetFlag(FLAG_HOLDING, 1);
        u->guard = GUARD_AVOID;            // doesn't attack, defends same-faction only

        // City treasury: MAYOR_INCOME_PCT% of region taxable income per turn
        int income = r->Population() * (r->Wages() - 10 * Globals->MAINTENANCE_COST) / 50;
        if (income > 0) {
            u->SetMoney(u->GetMoney() + income * MAYOR_INCOME_PCT / 100);
        }

        // Name + level-up bonus: fires on town tier upgrade (obs rank tracks tier)
        if (tt + 3 > cur_obs) {
            // Town leveled up: add 1000 silver bonus (same as per-level step)
            u->SetMoney(u->GetMoney() + MAYOR_TREASURY_PER_LEVEL);
            std::string tn = r->town ? r->town->name : "City";
            std::string nm;
            if (r->type == R_NEXUS)         nm = "City Lord Mayor";
            else if (tt == TOWN_VILLAGE)    nm = tn + " Elder";
            else if (tt == TOWN_TOWN)       nm = tn + " Mayor";
            else                            nm = tn + " Lord Mayor";
            u->set_name(nm);
        }
        return;
    }

    // Commander has its own dedicated upgrade logic
    if (u->type == U_GUARDCOMMANDER) {
        bool AC = r->IsStartingCity() || r->type == R_NEXUS;
        int tt = AC ? TOWN_CITY : (r->town ? r->town->TownType() : TOWN_CITY);
        bool IV = AC && (Globals->SAFE_START_CITIES || r->type == R_NEXUS);

        // Equipment by town type (upgrade only, never downgrade)
        int req_weapon, req_armor, req_shield, req_horse;
        int tact, ridi, comb;
        switch (tt) {
            case TOWN_VILLAGE:
                req_weapon = I_BAXE; req_armor = I_PLATEARMOR;
                req_shield = I_ISHIELD; req_horse = I_HORSE;
                tact = 2; ridi = 2; comb = 3;
                break;
            case TOWN_TOWN:
                req_weapon = I_MBAXE; req_armor = I_MPLATE;
                req_shield = I_MSHIELD; req_horse = I_HORSE;
                tact = 3; ridi = 3; comb = 4;
                break;
            default: // TOWN_CITY and Nexus
                req_weapon = I_ADBAXE; req_armor = I_ADPLATE;
                req_shield = I_ASHIELD; req_horse = I_WHORSE;
                tact = 4; ridi = 4; comb = 5;
                break;
        }

        // Detect current weapon (to support upgrade logic)
        auto get_baxe_tier = [](int item) -> int {
            if (item == I_BAXE)  return 1;
            if (item == I_MBAXE) return 2;
            if (item == I_ADBAXE) return 3;
            return 0;
        };
        auto get_ashield_tier = [](int item) -> int {
            if (item == I_ISHIELD) return 1;
            if (item == I_MSHIELD) return 2;
            if (item == I_ASHIELD) return 3;
            return 0;
        };
        auto get_parm_tier = [](int item) -> int {
            if (item == I_PLATEARMOR) return 1;
            if (item == I_MPLATE)     return 2;
            if (item == I_ADPLATE)    return 3;
            return 0;
        };

        int cur_weapon = -1, cur_armor = -1, cur_shield = -1, cur_horse = -1;
        for (int i = 0; i < NITEMS; i++) {
            if (u->items.GetNum(i) == 0) continue;
            if (i == I_BAXE || i == I_MBAXE || i == I_ADBAXE) cur_weapon = i;
            if (i == I_PLATEARMOR || i == I_MPLATE || i == I_ADPLATE) cur_armor = i;
            if (i == I_ISHIELD || i == I_MSHIELD || i == I_ASHIELD) cur_shield = i;
            if (i == I_HORSE || i == I_WHORSE) cur_horse = i;
        }

        // Assign weapon (upgrade only)
        if (cur_weapon == -1) {
            cur_weapon = req_weapon;
        } else if (get_baxe_tier(req_weapon) > get_baxe_tier(cur_weapon)) {
            u->items.SetNum(cur_weapon, 0);
            cur_weapon = req_weapon;
        }
        // Assign armor (upgrade only)
        if (cur_armor == -1) {
            cur_armor = req_armor;
        } else if (get_parm_tier(req_armor) > get_parm_tier(cur_armor)) {
            u->items.SetNum(cur_armor, 0);
            cur_armor = req_armor;
        }
        // Assign shield (upgrade only)
        if (cur_shield == -1) {
            cur_shield = req_shield;
        } else if (get_ashield_tier(req_shield) > get_ashield_tier(cur_shield)) {
            u->items.SetNum(cur_shield, 0);
            cur_shield = req_shield;
        }
        // Assign horse (upgrade only: regular → warhorse)
        if (cur_horse == -1) {
            cur_horse = req_horse;
        } else if (req_horse == I_WHORSE && cur_horse == I_HORSE) {
            u->items.SetNum(I_HORSE, 0);
            cur_horse = I_WHORSE;
        }

        u->SetMen(I_LEADERS, 1);
        u->items.SetNum(cur_weapon, 1);
        u->items.SetNum(cur_armor, 1);
        u->items.SetNum(cur_shield, 1);
        u->items.SetNum(cur_horse, 1);
        if (IV) u->items.SetNum(I_AMULETOFI, 1);
        u->SetMoney(Globals->GUARD_MONEY);
        // Skills: never downgrade
        u->SetSkill(S_TACTICS, std::max(u->GetRealSkill(S_TACTICS), tact));
        u->SetSkill(S_RIDING,  std::max(u->GetRealSkill(S_RIDING),  ridi));
        u->SetSkill(S_COMBAT,  std::max(u->GetRealSkill(S_COMBAT),  comb));
        // Observation 3/4/5 — also serves as rank indicator for name protection
        int cur_obs = u->GetRealSkill(S_OBSERVATION);
        u->SetSkill(S_OBSERVATION, std::max(cur_obs, tt + 3));
        u->SetFlag(FLAG_BEHIND, 1);
        u->SetFlag(FLAG_HOLDING, 1);
        u->guard = GUARD_GUARD;
        // Update name only if town rank increased (obs-based, never downgrade)
        if (tt + 3 > cur_obs) {
            std::string tn = r->town ? r->town->name : "City";
            std::string nm;
            if (r->type == R_NEXUS)         nm = "City General";
            else if (tt == TOWN_VILLAGE)    nm = tn + " Chieftain";
            else if (tt == TOWN_TOWN)       nm = tn + " Marshal";
            else                            nm = tn + " General";
            u->set_name(nm);
        }
        return;
    }

    int towntype;
    int AC = 0;
    int men;
    int IV = 0;
    int mantype;
    int maxmen;
    int weapon = -1;
    int maxweapon = 0;
    int armor = -1;
    int maxarmor = 0;
    int shield = -1;
    int maxshield = 0;
    for (int i=0; i<NITEMS; i++) {
        int num = u->items.GetNum(i);
        if (num == 0) continue;
        if (ItemDefs[i].type & IT_MAN) mantype = i;
        // Only standard rank-issued guard equipment occupies a slot, so the
        // sync below (SetNum(..., men)) restores/replicates exactly what the
        // guard is owed by town rank. Stray artifacts (e.g. a mayor's mithril
        // weapon/armor or shieldstone) have tier 0 and are ignored — they are
        // neither multiplied to men-count nor used as the unit's weapon/shield.
        if ((ItemDefs[i].type & IT_WEAPON)
            && GetWeaponTier(i) > 0
            && (num > maxweapon)) {
            weapon = i;
            maxweapon = num;
        }
        if ((ItemDefs[i].type & IT_ARMOR)
            && GetArmorTier(i) > 0
            && (num > maxarmor)) {
            armor = i;
            maxarmor = num;
        }
        if ((ItemDefs[i].type & IT_BATTLE)
            && GetShieldTier(i) > 0
            && (num > maxshield)) {
            shield = i;
            maxshield = num;
        }
    }
    if (r->type == R_NEXUS || r->IsStartingCity()) {
        towntype = TOWN_CITY;
        AC = 1;
        if (Globals->SAFE_START_CITIES || (r->type == R_NEXUS))
            IV = 1;
        if (u->type == U_GUARDMAGE) {
            men = 1;
        } else {
            maxmen = Globals->AMT_START_CITY_GUARDS;
            if ((!Globals->GUARDS_USE_LEADERS) && (r->type != R_NEXUS)) {
                maxmen = maxmen / 2;   // 50/50 melee/ranged split
            }
            int current_men = u->GetMen();
            men = current_men + (Globals->AMT_START_CITY_GUARDS/5);
            if (men > maxmen)
                men = std::max(current_men, maxmen);  // Don't lower count, only grow
        }
    } else {
        towntype = r->town->TownType();
        if (u->type == U_GUARDMAGE) {
            men = 1;
        } else {
            maxmen = Globals->CITY_GUARD * (towntype+1);
            if (!Globals->GUARDS_USE_LEADERS) {
                maxmen = maxmen / 2;   // 50/50 melee/ranged split
            }
            int current_men = u->GetMen();
            men = current_men + (maxmen/5);
            if (men > maxmen)
                men = std::max(current_men, maxmen);  // Don't lower count, only grow
        }
    }

    // Assign/upgrade equipment based on town type
    if (Globals->GUARDS_EQUIPMENT_BY_TOWN_TYPE) {
        // Determine required equipment for current town type
        int req_weapon = -1;
        int req_armor = -1;
        int req_shield = -1;

        if (u->GetFlag(FLAG_BEHIND)) {
            // Ranged guard unit: longbow + leather armor (no shield, all town types)
            req_weapon = I_LONGBOW;
            req_armor = I_LEATHERARMOR;
        } else {
            // Melee guard unit: equipment scales by town type
            switch(towntype) {
                case TOWN_VILLAGE:
                    req_weapon = I_PIKE;
                    req_armor = I_LEATHERARMOR;
                    req_shield = I_WSHIELD;
                    break;
                case TOWN_TOWN:
                    req_weapon = I_SWORD;
                    req_armor = I_CHAINARMOR;
                    req_shield = I_WSHIELD;
                    break;
                case TOWN_CITY:
                    req_weapon = I_SWORD;
                    req_armor = I_PLATEARMOR;
                    req_shield = I_ISHIELD;
                    break;
            }
        }

        // Upgrade weapon if required tier is higher than current tier
        if (weapon == -1) {
            // New guard: assign required weapon
            weapon = req_weapon;
        } else {
            // Existing guard: upgrade only (never downgrade)
            int current_tier = GetWeaponTier(weapon);
            int required_tier = GetWeaponTier(req_weapon);
            if (required_tier > current_tier) {
                // Remove old weapon, assign new weapon
                u->items.SetNum(weapon, 0);
                weapon = req_weapon;
            }
        }

        // Upgrade armor if required tier is higher than current tier
        if (armor == -1) {
            // New guard: assign required armor
            armor = req_armor;
        } else {
            // Existing guard: upgrade only (never downgrade)
            int current_tier = GetArmorTier(armor);
            int required_tier = GetArmorTier(req_armor);
            if (required_tier > current_tier) {
                // Remove old armor, assign new armor
                u->items.SetNum(armor, 0);
                armor = req_armor;
            }
        }

        // Upgrade shield if required tier is higher than current tier
        if (req_shield != -1) {  // Only for melee guards
            if (shield == -1) {
                // New guard: assign required shield
                shield = req_shield;
            } else {
                // Existing guard: upgrade only (never downgrade)
                int current_tier = GetShieldTier(shield);
                int required_tier = GetShieldTier(req_shield);
                if (required_tier > current_tier) {
                    // Remove old shield, assign new shield
                    u->items.SetNum(shield, 0);
                    shield = req_shield;
                }
            }
        }
    }

    // Determine combat skill from weapon type (after weapon assignment)
    int skill = S_COMBAT;
    if (weapon != -1) {
        auto weapon_def = find_weapon(ItemDefs[weapon].abr)->get();
        auto pS = FindSkill(weapon_def.baseSkill)->get();
        if (pS == FindSkill("XBOW")->get()) skill = S_CROSSBOW;
        if (pS == FindSkill("LBOW")->get()) skill = S_LONGBOW;
    }
    int sl = u->GetRealSkill(skill);

    u->SetMen(mantype,men);
    if (IV) u->items.SetNum(I_AMULETOFI,men);

    if (u->type == U_GUARDMAGE) {
        int magelevel = towntype + 1;
        // For starting cities, use START_CITY_MAGES if higher
        if (AC && Globals->START_CITY_MAGES > magelevel)
            magelevel = Globals->START_CITY_MAGES;

        // Don't lower skills - keep maximum level
        int current_force = u->GetRealSkill(S_FORCE);
        // ESHI gets +1 bonus to all magic skills
        int force_level = (u->combat == S_ENERGY_SHIELD) ? magelevel + 1 : magelevel;
        u->SetSkill(S_FORCE, std::max(current_force, force_level));

        // Update skills based on combat spell (already set in CreateCityMon)
        if (u->combat == S_FIRE) {
            // Fire Arcanist: fire attack
            int current_fire = u->GetRealSkill(S_FIRE);
            u->SetSkill(S_FIRE, std::max(current_fire, magelevel));
        } else if (u->combat == S_FORCE_SHIELD) {
            // Shieldmaster: force shield
            int current_fshi = u->GetRealSkill(S_FORCE_SHIELD);
            u->SetSkill(S_FORCE_SHIELD, std::max(current_fshi, magelevel));
        } else {
            // Flameguard (ESHI): energy shield gets +1 bonus
            int eshi_level = magelevel + 1;
            int current_eshi = u->GetRealSkill(S_ENERGY_SHIELD);
            u->SetSkill(S_ENERGY_SHIELD, std::max(current_eshi, eshi_level));
            if (u->combat != S_ENERGY_SHIELD) u->combat = S_ENERGY_SHIELD;
        }
        // Observation 3/4/5 — also serves as rank indicator for name protection
        int cur_obs = u->GetRealSkill(S_OBSERVATION);
        u->SetSkill(S_OBSERVATION, std::max(cur_obs, towntype + 3));
        u->SetFlag(FLAG_BEHIND, 1);
        u->SetMoney(Globals->GUARD_MONEY);
        // Update name only if town rank increased (obs-based, never downgrade)
        if (towntype + 3 > cur_obs) {
            std::string tn = r->town ? r->town->name : "City";
            std::string nm;
            if (r->type == R_NEXUS) {
                if (u->combat == S_ENERGY_SHIELD)     nm = "Arcane Flameguard";
                else if (u->combat == S_FORCE_SHIELD) nm = "Shieldmaster";
                else if (u->combat == S_FIRE)         nm = "Court Fire Arcanist";
            } else if (towntype == TOWN_VILLAGE) {
                nm = tn + " Flameguard";
            } else if (towntype == TOWN_TOWN) {
                if (u->combat == S_ENERGY_SHIELD)     nm = tn + " Flame Warden";
                else if (u->combat == S_FORCE_SHIELD) nm = tn + " Shield Warden";
            } else {
                if (u->combat == S_ENERGY_SHIELD)     nm = tn + " Arcane Flameguard";
                else if (u->combat == S_FORCE_SHIELD) nm = tn + " Shieldmaster";
                else if (u->combat == S_FIRE)         nm = tn + " Fire Arcanist";
            }
            if (!nm.empty()) u->set_name(nm);
        }
    } else {
        int money = men * (Globals->GUARD_MONEY * men / maxmen);
        u->SetMoney(money);
        // Upgrade combat skill by town type (never downgrade): village=2, town=3, city=4
        u->SetSkill(skill, std::max(sl, towntype + 2));
        int current_obs = u->GetRealSkill(S_OBSERVATION);
        if (AC) {
            u->SetSkill(S_OBSERVATION,10);
            if (Globals->START_CITY_TACTICS)
                u->SetSkill(S_TACTICS, Globals->START_CITY_TACTICS);
        } else {
            // Don't lower Observation - keep maximum level
            u->SetSkill(S_OBSERVATION, std::max(current_obs, towntype + 3));
        }
        // Rank-issued kit is replicated to the current men count uniformly for
        // weapon, armor and shield. START_CITY_GUARDS_PLATE governs which armor a
        // start-city guard is owed (its tier via req_armor), not whether the count
        // is maintained — gating the count-sync here previously left start-city
        // archers' leather (and any non-plate armor) frozen while their weapon grew.
        if (armor != -1) {
            u->items.SetNum(armor,men);
        }
        if (weapon!= -1) {
            u->items.SetNum(weapon,men);
        }
        if (shield != -1) {
            u->items.SetNum(shield,men);
        }
        // Update name only if town rank increased (obs-based, never downgrade)
        if (towntype + 3 > current_obs) {
            std::string tn = r->town ? r->town->name : "City";
            std::string nm;
            if (r->type == R_NEXUS) {
                nm = u->GetFlag(FLAG_BEHIND) ? "City Archers" : "City Guard";
            } else if (u->GetFlag(FLAG_BEHIND)) {
                nm = tn + " Archers";
            } else if (towntype == TOWN_VILLAGE) {
                nm = tn + " Militia";
            } else if (towntype == TOWN_TOWN) {
                nm = tn + " Town Guard";
            } else {
                nm = tn + " City Guard";
            }
            u->set_name(nm);
        }
    }
    // Restore guard status (lost when guards are defeated in battle)
    // Mages (U_GUARDMAGE) use GUARD_NONE - they fight but don't block taxation
    if (u->type == U_GUARD || u->type == U_GUARDCOMMANDER) {
        u->guard = GUARD_GUARD;
    } else if (u->type == U_GUARDMAGE) {
        u->guard = GUARD_NONE;
    }
}

/**
 * @brief Creates a Guard Commander unit for a settlement.
 *
 * Spawns 1 leader with TACTICS/RIDING/COMBAT skills scaled by town type.
 * No equipment on creation — AdjustCityMon delivers it next turn.
 * Names: Village="X Chieftain", Town="X Marshal", City/Nexus="X General".
 *
 * @param r  Region containing the settlement
 */
void Game::CreateGuardCommander(ARegion *r)
{
    bool AC = r->IsStartingCity() || r->type == R_NEXUS;
    int tt = AC ? TOWN_CITY : (r->town ? r->town->TownType() : TOWN_CITY);
    bool IV = AC && (Globals->SAFE_START_CITIES || r->type == R_NEXUS);

    int tact, ridi, comb;
    switch (tt) {
        case TOWN_VILLAGE: tact = 2; ridi = 2; comb = 3; break;
        case TOWN_TOWN:    tact = 3; ridi = 3; comb = 4; break;
        default:           tact = 4; ridi = 4; comb = 5; break;
    }

    std::string townname = r->town ? r->town->name : "City";
    std::string name;
    if (r->type == R_NEXUS) {
        name = "City General";
    } else {
        switch (tt) {
            case TOWN_VILLAGE: name = townname + " Chieftain"; break;
            case TOWN_TOWN:    name = townname + " Marshal";   break;
            default:           name = townname + " General";   break;
        }
    }

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);
    u->set_name(name);
    u->type = U_GUARDCOMMANDER;
    u->guard = GUARD_GUARD;
    u->reveal = REVEAL_FACTION;
    u->SetMen(I_LEADERS, 1);
    if (IV) u->items.SetNum(I_AMULETOFI, 1);
    u->SetMoney(Globals->GUARD_MONEY);
    u->SetSkill(S_TACTICS,      tact);
    u->SetSkill(S_RIDING,       ridi);
    u->SetSkill(S_COMBAT,       comb);
    u->SetSkill(S_OBSERVATION,  tt + 3);
    u->SetFlag(FLAG_BEHIND, 1);
    u->SetFlag(FLAG_HOLDING, 1);
    // Equipment (axe/armor/shield/horse) delivered by AdjustCityMon next turn
    u->MoveUnit(r->GetDummy());
}

/**
 * @brief Creates the city Mayor (symbolic administrator NPC).
 *
 * Mayor participates in combat (rear line), carries I_CORNUCOPIA (city key),
 * accumulates city treasury (5% of region income per turn via AdjustCityMon).
 * Flees when melee guards < 50% of max. Respawns when melee >= 75% of max.
 *
 * Names: Village Elder / Town Mayor / City Lord Mayor / Nexus City Lord Mayor
 * Weapons: SWOR → MSWO → ASWR (by town type, upgrade only)
 * Armor:   CARM → MCAR → ARNG (by town type, upgrade only)
 *
 * @see AdjustCityMon() for equipment/treasury, AdjustCityMons() for flee/spawn logic
 */
void Game::CreateMayor(ARegion *r, Object *target)
{
    // Bare spawn — no equipment, no money. Equipment + GUARD_MONEY arrive
    // via the next AdjustCityMons pass (anti-farm: see docs/TOWN_HALL_AND_MAYOR_QUESTS_PLAN.md §3.8).
    int tt = (r->IsStartingCity() || r->type == R_NEXUS)
             ? TOWN_CITY
             : (r->town ? r->town->TownType() : TOWN_CITY);

    std::string townname = r->town ? r->town->name : "City";
    std::string name;
    if (r->type == R_NEXUS) {
        name = "City Lord Mayor";
    } else {
        switch (tt) {
            case TOWN_VILLAGE: name = townname + " Elder";      break;
            case TOWN_TOWN:    name = townname + " Mayor";      break;
            default:           name = townname + " Lord Mayor"; break;
        }
    }

    Faction *fac = GetFaction(factions, guardfaction);
    Unit *u = GetNewUnit(fac);
    u->set_name(name);
    u->type = U_MAYOR;
    u->guard = GUARD_AVOID;            // doesn't attack, but defends same-faction (see GetSides)
    u->reveal = REVEAL_FACTION;
    u->SetMen(I_LEADERS, 1);
    u->SetSkill(S_OBSERVATION, tt + 3);
    u->SetFlag(FLAG_BEHIND, 1);
    u->SetFlag(FLAG_HOLDING, 1);
    u->MoveUnit(target ? target : r->GetDummy());
}

void Game::Equilibrate()
{
    logger::write("Initialising the economy");
    for (int a=0; a<25; a++) {
        logger::dot();
        ProcessMigration();
        for(const auto r : regions) {
            r->PostTurn();
        }
    }
    logger::write("");
}

void Game::write_times_article(std::string article)
{
    std::string fname;

    do { fname = "times." + std::to_string(rng::get_random(10000)); } while (filesystem::exists(fname));
    std::ofstream f(fname, std::ios::out | std::ios::trunc);
    if (f.is_open()) {
        f << indent::wrap(78,70,0) << article << '\n';
    }
}


void Game::CountItems(size_t ** citems)
{
    for(const auto fac : factions) {
        if (!fac->is_npc) {
            int i = fac->num - 1; // faction numbers are 1-based
            for (int j = 0; j < NITEMS; j++){
                citems[i][j] = CountItem (fac, j);
            }
        }
    }
}

int Game::CountItem (Faction *fac, int item)
{
    if (ItemDefs[item].type & IT_SHIP) return 0;

    size_t all = 0;
    for (const auto& r : fac->present_regions) {
        for(const auto obj : r->objects) {
            for(const auto unit : obj->units) {
                if (unit->faction == fac) all += unit->items.GetNum(item);
            }
        }
    }
    return all;
}
