#include <stdio.h>
#include <string.h>
#include "game.h"
#include "gamedata.h"
#include "mapgen.h"
#include "namegen.h"
#include "indenter.hpp"
#include "rng.hpp"

#include <vector>
#include <algorithm>
#include <random>
#include <ctime>
#include <cassert>
#include <unordered_set>
#include <queue>
#include "scoped_enum.hpp"
#include "strings_util.hpp"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

Location *GetUnit(std::list<Location *>& locs, int unitid)
{
    for(const auto l : locs) {
        if (l->unit->num == unitid) return l;
    }
    return nullptr;
}

Farsight::Farsight()
{
    faction = 0;
    unit = 0;
    level = 0;
    observation = 0;
    for (int i = 0; i < NDIRS; i++)
        exits_used[i] = 0;
}

Farsight *GetFarsight(std::list<Farsight *>& farsees, Faction *fac)
{
    for(const auto f : farsees) {
        if (f->faction == fac) return f;
    }
    return nullptr;
}

std::string TownString(int i)
{
    switch (i) {
        case TOWN_VILLAGE: return "village";
        case TOWN_TOWN: return "town";
        case TOWN_CITY: return "city";
    }
    return "huh?";
}

TownInfo::TownInfo()
{
    pop = 0;
    activity = 0;
    hab = 0;
}

TownInfo::~TownInfo() { }

void TownInfo::Readin(std::istream &f)
{
    std::string temp;
    std::getline(f >> std::ws, temp);
    name = temp;
    f >> pop;
    f >> hab;
}

void TownInfo::Writeout(std::ostream& f)
{
    f << name << '\n' << pop << '\n' << hab << '\n';
}

ARegion::ARegion()
{
    name = UNNAMED_REGION;
    xloc = 0;
    yloc = 0;
    buildingseq = 1;
    gate = 0;
    gatemonth = 0;
    gateopen = 1;
    town = 0;
    development = 0;
    maxdevelopment = 0;
    habitat = 0;
    immigrants = 0;
    emigrants = 0;
    improvement = 0;
    clearskies = 0;
    weather = W_NORMAL;
    earthlore = 0;
    phantasmal_entertainment = 0;
    for (int i=0; i<NDIRS; i++)
        neighbors[i] = 0;
    visited = 0;
}

ARegion::~ARegion()
{
    if (town) delete town;
    std::for_each(objects.begin(), objects.end(), [](Object *o) { delete o; });
    objects.clear();
}

void ARegion::ZeroNeighbors()
{
    for (int i=0; i<NDIRS; i++) {
        neighbors[i] = 0;
    }
}

void ARegion::set_name(const std::string& newname)
{
    name = newname;
}

/**
 * @brief Assigns a region's name during world creation
 *
 * Converts the name seed left in `wages` by the terrain phase into the region's
 * display name. GrowTerrain()/AssignTypes() park an AGetName() index there and
 * propagate it to neighbours, which is what makes adjacent hexes share a name.
 * Water and barren hexes ignore the seed and take a fixed, level-dependent name.
 *
 * Seed conventions in `wages`:
 *   >= 0 : index into the ruleset name table (AGetNameString)
 *     -1 : never seeded -> "The Void"
 *     -2 : keep whatever name the region already has; the sentinel is cleared
 *          to -1 so the economy setup never reads it as a wage value
 *
 * @param levelType one of ARegionArray::LEVEL_*, selects the ocean name
 * @note Must run before SetupEconomy(), which reads `wages`.
 * @see ARegionList::FinalSetup(), economy_underground() in neworigins/map.cpp
 */
void ARegion::assign_generated_name(int levelType)
{
    int similar = TerrainDefs[type].similar_type;

    if ((similar == R_OCEAN) && (type != R_LAKE)) {
        if (levelType == ARegionArray::LEVEL_UNDERWORLD) {
            set_name("The Undersea");
        } else if (levelType == ARegionArray::LEVEL_UNDERDEEP) {
            set_name("The Deep Undersea");
        } else {
            std::string ocean_name = Globals->WORLD_NAME;
            ocean_name += " Ocean";
            set_name(ocean_name);
        }
    } else if (similar == R_BARREN) {
        set_name("The Barrens");
    } else {
        if (wages == -1) set_name("The Void");
        else if (wages != -2) set_name(AGetNameString(wages));
        else wages = -1;
    }
}

int ARegion::produces_item(int item)
{
    if(products.size() == 0) return 0;
    auto p = find_if(products.begin(), products.end(), [item](Production *p) { return p->itemtype == item; });
    return (p != products.end()) ? (*p)->amount : 0;
}

Production *ARegion::get_production_for_skill(int item, int skill) {
    if (products.size() == 0) return nullptr;
    auto p = find_if(
        products.begin(),
        products.end(),
        [item, skill](Production *p) { return p->itemtype == item && p->skill == skill; }
    );
    return (p != products.end()) ? *p : nullptr;
}

int ARegion::IsNativeRace(int item)
{
    // Safety check
    if (type < 0 || type >= (int)TerrainDefs.size()) {
        return 0;
    }

    TerrainType *typer = &(TerrainDefs[type]);
    int coastal = sizeof(typer->coastal_races)/sizeof(typer->coastal_races[0]);
    int noncoastal = sizeof(typer->races)/sizeof(typer->races[0]);
    if (IsCoastal()) {
        for (int i=0; i<coastal; i++) {
            if (item == typer->coastal_races[i]) return 1;
        }
    }
    for (int i=0; i<noncoastal; i++) {
        if (item == typer->races[i]) return 1;
    }
    return 0;
}

std::vector<int> ARegion::GetPossibleLairs() {
    std::vector<int> lairs;
    TerrainType *tt = &TerrainDefs[type];
    const int sz = sizeof(tt->lairs) / sizeof(tt->lairs[0]);

    for (int i = 0; i < sz; i++) {
        int index = tt->lairs[i];
        if (index == -1) continue;

        ObjectType& lair = ObjectDefs[index];
        if (lair.flags & ObjectType::DISABLED) continue;

        lairs.push_back(index);
    }

    return lairs;
}

void ARegion::LairCheck()
{
    // No lair if town in region
    if (town) return;

    TerrainType *tt = &TerrainDefs[type];
    if (!tt->lairChance) return;

    int check = rng::get_random(100);
    if (check >= tt->lairChance) return;

    auto lairs = GetPossibleLairs();
    if (lairs.empty()) return;

    int lair = lairs[rng::get_random(lairs.size())];
    MakeLair(lair);
}

void ARegion::MakeLair(int t)
{
    Object *o = new Object(this);
    o->num = buildingseq++;
    o->type = t;

    // Generate ethnicity-specific lair name based on monster type and region's race
    int monsterType = ObjectDefs[t].monster;
    std::string lairName = getLairName(t, monsterType, this->race);
    o->set_name(lairName);

    o->incomplete = 0;
    o->inner = -1;
    objects.push_back(o);
}

int ARegion::GetPoleDistance(int dir)
{
    int ct = 1;
    ARegion *nreg = neighbors[dir];
    while (nreg) {
        ct++;
        nreg = nreg->neighbors[dir];
    }
    return ct;
}

void ARegion::Setup()
{
    //
    // type and location have been setup, do everything else
    //
    SetupProds(1);

    //
    // Make the dummy object BEFORE SetupPop
    // This is critical because SetupPop -> add_town -> SetTownType -> TownHabitat
    // and TownHabitat iterates over objects. Objects must be initialized first!
    //
    Object *obj = new Object(this);
    objects.push_back(obj);

    SetupPop();

    if (Globals->LAIR_MONSTERS_EXIST) LairCheck();
}

void ARegion::setup_terrain(const RegionSetup& settings) {
    SetupProds(settings.prodWeight);

    habitat = settings.habitat;

    SetupHabitat(settings.terrain);
}

void ARegion::finish_setup(const RegionSetup& settings) {
    SetupEconomy();

    objects.push_back(new Object(this));

    if (Globals->LAIR_MONSTERS_EXIST && settings.addLair) {
        auto lairs = GetPossibleLairs();
        if (!lairs.empty()) {
            int lair = lairs[rng::get_random(lairs.size())];
            MakeLair(lair);
        }
    }
}

void ARegion::ManualSetup(const RegionSetup& settings) {
    setup_terrain(settings);

    if (settings.addSettlement) add_town(settings.settlementSize, settings.settlementName);

    finish_setup(settings);
}

int ARegion::TraceConnectedRoad(int dir, int sum, std::list<ARegion *>& con, int range, int dev)
{
    bool isnew = true;
    for(const auto reg : con) {
        if (reg == this) isnew = false;
    }
    if (!isnew) return sum;
    con.push_back(this);
    // Add bonus for connecting town
    if (town) sum++;
    // Add bonus if development is higher
    if (development > dev + 9) sum++;
    if (development * 2 > dev * 5) sum++;
    // Check further along road
    if (range > 0) {
        for (int d=0; d<NDIRS; d++) {
            if (!HasExitRoad(d)) continue;
            ARegion *r = neighbors[d];
            if (!r) continue;
            if (dir == r->GetRealDirComp(d)) continue;
            if (HasConnectingRoad(d)) sum = r->TraceConnectedRoad(d, sum, con, range-1, dev+2);
        }
    }
    return sum;
}

int ARegion::RoadDevelopmentBonus(int range, int dev)
{
    int bonus = 0;
    std::list<ARegion *> con;
    con.push_back(this);
    for (int d=0; d<NDIRS; d++) {
        if (!HasExitRoad(d)) continue;
        ARegion *r = neighbors[d];
        if (!r) continue;
        if (HasConnectingRoad(d)) bonus = r->TraceConnectedRoad(d, bonus, con, range-1, dev);
    }
    return bonus;
}

// AS
void ARegion::DoDecayCheck()
{
    for(auto o : objects) {
        if (!(ObjectDefs[o->type].flags & ObjectType::NEVERDECAY)) {
            DoDecayClicks(o);
        }
    }
}

// AS
void ARegion::DoDecayClicks(Object *o)
{
    if (ObjectDefs[o->type].flags & ObjectType::NEVERDECAY) return;

    int clicks = rng::get_random(GetMaxClicks());
    clicks += PillageCheck();

    if (clicks > ObjectDefs[o->type].maxMonthlyDecay) clicks = ObjectDefs[o->type].maxMonthlyDecay;

    o->incomplete += clicks;

    if (o->incomplete > 0) {
        // trigger decay event
        RunDecayEvent(o);
    }
}

// AS
void ARegion::RunDecayEvent(Object *o)
{
    std::set<Faction *>factions = PresentFactions();
    for(const auto f : factions) {
        std::string tmp = get_decay_flavor() + " " + ObjectDefs[o->type].name + " in " + short_print() + ".";
        f->event(tmp, "decay", this);
    }
}

// AS
std::string ARegion::get_decay_flavor()
{
    int badWeather = 0;
    if (weather != W_NORMAL && !clearskies) badWeather = 1;
    if (!Globals->WEATHER_EXISTS) badWeather = 0;
    switch (type) {
        case R_PLAIN:
        case R_ISLAND_PLAIN:
        case R_CERAN_PLAIN1:
        case R_CERAN_PLAIN2:
        case R_CERAN_PLAIN3:
        case R_CERAN_LAKE:
            return "Floods have damaged ";
        case R_DESERT:
        case R_CERAN_DESERT1:
        case R_CERAN_DESERT2:
        case R_CERAN_DESERT3:
            return "Flashfloods have damaged ";
        case R_CERAN_WASTELAND:
        case R_CERAN_WASTELAND1:
            return "Magical radiation has damaged ";
        case R_TUNDRA:
        case R_CERAN_TUNDRA1:
        case R_CERAN_TUNDRA2:
        case R_CERAN_TUNDRA3:
            if (badWeather) return "Ground freezing has damaged ";
            return "Ground thaw has damaged ";
        case R_MOUNTAIN:
        case R_ISLAND_MOUNTAIN:
        case R_CERAN_MOUNTAIN1:
        case R_CERAN_MOUNTAIN2:
        case R_CERAN_MOUNTAIN3:
            if (badWeather) return "Avalanches have damaged ";
            return "Rockslides have damaged ";
        case R_CERAN_HILL:
        case R_CERAN_HILL1:
        case R_CERAN_HILL2:
            return "Quakes have damaged ";
        case R_FOREST:
        case R_SWAMP:
        case R_ISLAND_SWAMP:
        case R_JUNGLE:
        case R_CERAN_FOREST1:
        case R_CERAN_FOREST2:
        case R_CERAN_FOREST3:
        case R_CERAN_MYSTFOREST:
        case R_CERAN_MYSTFOREST1:
        case R_CERAN_MYSTFOREST2:
        case R_CERAN_SWAMP1:
        case R_CERAN_SWAMP2:
        case R_CERAN_SWAMP3:
        case R_CERAN_JUNGLE1:
        case R_CERAN_JUNGLE2:
        case R_CERAN_JUNGLE3:
            return "Encroaching vegetation has damaged ";
        case R_CAVERN:
        case R_UFOREST:
        case R_TUNNELS:
        case R_CERAN_CAVERN1:
        case R_CERAN_CAVERN2:
        case R_CERAN_CAVERN3:
        case R_CERAN_UFOREST1:
        case R_CERAN_UFOREST2:
        case R_CERAN_UFOREST3:
        case R_CERAN_TUNNELS1:
        case R_CERAN_TUNNELS2:
        case R_CHASM:
        case R_CERAN_CHASM1:
        case R_GROTTO:
        case R_CERAN_GROTTO1:
        case R_DFOREST:
        case R_CERAN_DFOREST1:
            if (badWeather) return "Lava flows have damaged ";
            return "Quakes have damaged ";
    }
    return "Unexplained phenomena have damaged ";
}

// AS
int ARegion::GetMaxClicks()
{
    int terrainAdd = 0;
    int terrainMult = 1;
    int weatherAdd = 0;
    int badWeather = 0;
    int maxClicks;
    if (weather != W_NORMAL && !clearskies) badWeather = 1;
    if (!Globals->WEATHER_EXISTS) badWeather = 0;
    switch (type) {
        case R_PLAIN:
        case R_ISLAND_PLAIN:
        case R_TUNDRA:
        case R_CERAN_PLAIN1:
        case R_CERAN_PLAIN2:
        case R_CERAN_PLAIN3:
        case R_CERAN_LAKE:
        case R_CERAN_TUNDRA1:
        case R_CERAN_TUNDRA2:
        case R_CERAN_TUNDRA3:
            terrainAdd = -1;
            if (badWeather) weatherAdd = 4;
            break;
        case R_MOUNTAIN:
        case R_ISLAND_MOUNTAIN:
        case R_CERAN_MOUNTAIN1:
        case R_CERAN_MOUNTAIN2:
        case R_CERAN_MOUNTAIN3:
        case R_CERAN_HILL:
        case R_CERAN_HILL1:
        case R_CERAN_HILL2:
            terrainMult = 2;
            if (badWeather) weatherAdd = 4;
            break;
        case R_FOREST:
        case R_SWAMP:
        case R_ISLAND_SWAMP:
        case R_JUNGLE:
        case R_CERAN_FOREST1:
        case R_CERAN_FOREST2:
        case R_CERAN_FOREST3:
        case R_CERAN_MYSTFOREST:
        case R_CERAN_MYSTFOREST1:
        case R_CERAN_MYSTFOREST2:
        case R_CERAN_SWAMP1:
        case R_CERAN_SWAMP2:
        case R_CERAN_SWAMP3:
        case R_CERAN_JUNGLE1:
        case R_CERAN_JUNGLE2:
        case R_CERAN_JUNGLE3:
            terrainAdd = -1;
            terrainMult = 2;
            if (badWeather) weatherAdd = 1;
            break;
        case R_DESERT:
        case R_CERAN_DESERT1:
        case R_CERAN_DESERT2:
        case R_CERAN_DESERT3:
            terrainAdd = -1;
            if (badWeather) weatherAdd = 5;
        case R_CAVERN:
        case R_UFOREST:
        case R_TUNNELS:
        case R_CERAN_CAVERN1:
        case R_CERAN_CAVERN2:
        case R_CERAN_CAVERN3:
        case R_CERAN_UFOREST1:
        case R_CERAN_UFOREST2:
        case R_CERAN_UFOREST3:
        case R_CERAN_TUNNELS1:
        case R_CERAN_TUNNELS2:
        case R_CHASM:
        case R_CERAN_CHASM1:
        case R_GROTTO:
        case R_CERAN_GROTTO1:
        case R_DFOREST:
        case R_CERAN_DFOREST1:
            terrainAdd = 1;
            terrainMult = 2;
            if (badWeather) weatherAdd = 6;
            break;
        default:
            if (badWeather) weatherAdd = 4;
            break;
    }
    maxClicks = terrainMult * (terrainAdd + 2) + (weatherAdd + 1);
    return maxClicks;
}

// AS
int ARegion::PillageCheck()
{
    int pillageAdd = maxwages - wages;
    if (pillageAdd > 0) return pillageAdd;
    return 0;
}

// AS
int ARegion::HasRoad()
{
    for(const auto o : objects) {
        if (o->IsRoad() && o->incomplete < 1) return 1;
    }
    return 0;
}

// AS
int ARegion::HasExitRoad(int realDirection)
{
    for(const auto o : objects) {
        if (o->IsRoad() && o->incomplete < 1) {
            if (o->type == GetRoadDirection(realDirection)) return 1;
        }
    }
    return 0;
}

// AS
int ARegion::CountConnectingRoads()
{
    int connections = 0;
    for (int i = 0; i < NDIRS; i++) {
        if (HasExitRoad(i) && neighbors[i] &&
                HasConnectingRoad(i))
            connections ++;
    }
    return connections;
}

// AS
int ARegion::HasConnectingRoad(int realDirection)
{
    int opposite = GetRealDirComp(realDirection);

    if (neighbors[realDirection] && neighbors[realDirection]->HasExitRoad(opposite)) {
        return 1;
    }

    return 0;
}

// AS
int ARegion::GetRoadDirection(int realDirection)
{
    int roadDirection = 0;
    switch (realDirection) {
        case D_NORTH:
            roadDirection = O_ROADN;
            break;
        case D_NORTHEAST:
            roadDirection = O_ROADNE;
            break;
        case D_NORTHWEST:
            roadDirection = O_ROADNW;
            break;
        case D_SOUTH:
            roadDirection = O_ROADS;
            break;
        case D_SOUTHEAST:
            roadDirection = O_ROADSE;
            break;
        case D_SOUTHWEST:
            roadDirection = O_ROADSW;
            break;
    }
    return roadDirection;
}

// AS
int ARegion::GetRealDirComp(int realDirection)
{
    int complementDirection = 0;

    if (neighbors[realDirection]) {
        ARegion *n = neighbors[realDirection];
        for (int i = 0; i < NDIRS; i++)
            if (n->neighbors[i] == this)
                return i;
    }

    switch (realDirection) {
        case D_NORTH:
            complementDirection = D_SOUTH;
            break;
        case D_NORTHEAST:
            complementDirection = D_SOUTHWEST;
            break;
        case D_NORTHWEST:
            complementDirection = D_SOUTHEAST;
            break;
        case D_SOUTH:
            complementDirection = D_NORTH;
            break;
        case D_SOUTHEAST:
            complementDirection = D_NORTHWEST;
            break;
        case D_SOUTHWEST:
            complementDirection = D_NORTHEAST;
            break;
    }
    return complementDirection;
}

std::string ARegion::short_print()
{
    std::string temp = TerrainDefs[type].name;

    temp += " (" + std::to_string(xloc) + "," + std::to_string(yloc);

    ARegionArray *pArr = this->level;
    if (!pArr->strName.empty()) {
        temp += ",";
        if (Globals->EASIER_UNDERWORLD &&
                (Globals->UNDERWORLD_LEVELS+Globals->UNDERDEEP_LEVELS > 1)) {
            temp += std::to_string(zloc) + " <";
        } else {
            // add less explicit multilevel information about the underworld
            if (zloc > 2 && zloc < Globals->UNDERWORLD_LEVELS+2) {
                for (int i = zloc; i > 3; i--) {
                    temp += "very ";
                }
                temp += "deep ";
            } else if ((zloc > Globals->UNDERWORLD_LEVELS+2) &&
                        (zloc < Globals->UNDERWORLD_LEVELS +
                        Globals->UNDERDEEP_LEVELS + 2)) {
                for (int i = zloc; i > Globals->UNDERWORLD_LEVELS + 3; i--) {
                    temp += "very ";
                }
                temp += "deep ";
            }
        }
        temp += pArr->strName;
        if (Globals->EASIER_UNDERWORLD &&
                (Globals->UNDERWORLD_LEVELS+Globals->UNDERDEEP_LEVELS > 1)) {
            temp += ">";
        }
    }
    temp += ")";

    temp += " in " + name;
    return temp;
}

std::string ARegion::print()
{
    std::string temp = short_print();
    if (town) {
        temp += ", contains " + town->name + " [" + TownString(town->TownType()) + "]";
    }
    return temp;
}

void ARegion::SetLoc(int x, int y, int z)
{
    xloc = x;
    yloc = y;
    zloc = z;
}

void ARegion::SetGateStatus(int month)
{
    if ((type == R_NEXUS) || (Globals->START_GATES_OPEN && IsStartingCity())) {
        gateopen = 1;
        return;
    }
    gateopen = 0;
    for (int i = 0; i < Globals->GATES_NOT_PERENNIAL; i++) {
        int dmon = gatemonth + i;
        if (dmon > 11) dmon = dmon - 12;
        if (dmon == month) gateopen = 1;
    }
}

void ARegion::Kill(Unit *u)
{
    Unit *first = nullptr;
    for(const auto obj : objects) {
        if (obj) {
            for(const auto unit : obj->units) {
                if (unit->faction->num == u->faction->num && unit != u) {
                    first = unit;
                    break;
                }
            }
        }
        if (first) break;
    }

    if (first) {
        // give u's stuff to first
        for(auto i : u->items) {
            if (ItemDefs[i->type].type & IT_SHIP && first->items.GetNum(i->type) > 0) {
                if (first->items.GetNum(i->type) > i->num)
                    first->items.SetNum(i->type, i->num);
                continue;
            }
            if (!IsSoldier(i->type)) {
                first->items.SetNum(i->type, first->items.GetNum(i->type) + i->num);
                // If we're in ocean and not in a structure, make sure that
                // the first unit can actually hold the stuff and not drown
                // If the item would cause them to drown then they won't
                // pick it up.
                if (TerrainDefs[type].similar_type == R_OCEAN) {
                    if (first->object->type == O_DUMMY) {
                        if (!first->CanReallySwim()) {
                            first->items.SetNum(i->type, first->items.GetNum(i->type) - i->num);
                        }
                    }
                }
            }
        }
        // clean up the item list
        std::for_each(u->items.begin(), u->items.end(), [](Item *item) { delete item; });
        u->items.clear();
    }

    u->MoveUnit(0);
    hell.push_back(u);
}

void ARegion::ClearHell()
{
    std::for_each(hell.begin(), hell.end(), [](Unit *unit) { delete unit; });
    hell.clear();
}

Object *ARegion::GetObject(int num)
{
    for(const auto o : objects) {
        if (o->num == num) return o;
    }
    return 0;
}

Object *ARegion::GetDummy()
{
    for(const auto o : objects) {
        if (o->type == O_DUMMY) return o;
    }
    return 0;
}

/* Checks all fleets to see if they are empty.
 * Moves all units out of an empty fleet into the
 * dummy object.
 */
void ARegion::CheckFleets()
{
    for(const auto o : objects) {
        if (o->IsFleet()) {
            int bail = 0;
            if (o->FleetCapacity() < 1) bail = 1;
            int alive = 0;
            for(const auto unit : o->units) {
                if (unit->IsAlive()) alive = 1;
                if (bail > 0) unit->MoveUnit(GetDummy());
            }
            // don't remove fleets when no living units are
            // aboard when they're not at sea.
            if (TerrainDefs[type].similar_type != R_OCEAN) alive = 1;
            if ((alive == 0) || (bail == 1)) {
                std::erase(objects, o);
                delete o;
            }
        }
    }
}

Unit *ARegion::GetUnit(int num)
{
    for(const auto obj : objects) {
        Unit *u = obj->GetUnit(num);
        if (u) return(u);
    }
    return nullptr;
}

Location *ARegion::GetLocation(UnitId *id, int faction)
{
    for(const auto o : objects) {
        Unit *unit = o->GetUnitId(id, faction);
        if (unit) {
            Location *l = new Location;
            l->region = this;
            l->obj = o;
            l->unit = unit;
            return l;
        }
    }
    return nullptr;
}

Unit *ARegion::GetUnitAlias(int alias, int faction)
{
    for(const auto obj : objects) {
        Unit *u = obj->GetUnitAlias(alias, faction);
        if (u) return(u);
    }
    return nullptr;
}

Unit *ARegion::GetUnitId(UnitId *id, int faction)
{
    for(const auto o : objects) {
        Unit *unit = o->GetUnitId(id, faction);
        if (unit) return unit;
    }
    return nullptr;
}

void ARegion::deduplicate_unit_list(std::list<UnitId *>& list, int factionid)
{
    std::unordered_set<Unit *> seen;
    std::unordered_set<UnitId> seen_ids;

    for(auto it = list.begin(); it != list.end();) {
        UnitId *id = *it;
        Unit *unit = GetUnitId(id, factionid);
        if (!unit) { ++it; continue; }
        // now, if we have already seen this unit *or* this exact unit id, skip it.
        if (seen.find(unit) == seen.end() && seen_ids.find(*id) == seen_ids.end()) {
            seen.insert(unit);
            seen_ids.insert(*id);
            ++it;
        } else {
            it = list.erase(it);
            delete id;
        }
    }
}

Location *ARegionList::GetUnitId(UnitId *id, int faction, ARegion *cur)
{
    Location *retval = NULL;
    // Check current region first
    retval = cur->GetLocation(id, faction);
    if (retval) return retval;

    // No? We must be looking for an existing unit.
    if (!id->unitnum) return NULL;

    return this->FindUnit(id->unitnum);
}

bool ARegion::Present(Faction *f)
{
    for(const auto obj : objects) {
        for(const auto u : obj->units)
            if (u->faction == f) return true;
    }
    return false;
}

std::set<Faction *>ARegion::PresentFactions()
{
    std::set<Faction *> facs;
    for(const auto obj : objects) {
        for(const auto u : obj->units) {
            facs.insert(u->faction);
        }
    }
    return facs;
}

void ARegion::Writeout(std::ostream& f)
{
    f << name << '\n';
    f << num << '\n';

    f << (type != -1 ? TerrainDefs[type].type : "NO_TERRAIN") << '\n';

    f << buildingseq << '\n';
    f << gate << '\n';
    if (gate > 0) f << gatemonth << '\n';

    f << (race != -1 ? ItemDefs[race].abr : "NO_RACE") << '\n';
    f << population << '\n';
    f << basepopulation << '\n';
    f << wages << '\n';
    f << maxwages << '\n';
    f << wealth << '\n';

    f << elevation << '\n';
    f << humidity << '\n';
    f << temperature << '\n';
    f << vegetation << '\n';
    f << culture << '\n';

    f << habitat << '\n';
    f << development << '\n';
    f << maxdevelopment << '\n';

    f << (town ? 1 : 0) << '\n';
    if (town) town->Writeout(f);

    f << xloc << '\n' << yloc << '\n' << zloc << '\n';
    f << visited << '\n';

    f << products.size() << '\n';
    for (const auto& product : products) product->write_out(f);
    f << markets.size() << '\n';
    for (const auto& market : markets) market->write_out(f);

    f << objects.size() << '\n';
    for(const auto o : objects) o->Writeout(f);
}

static int lookup_region_type(const std::string& token)
{
    for (int i = 0; i < R_NUM; i++) {
        if (token == TerrainDefs[i].type) return i;
    }
    return -1;
}

void ARegion::Readin(std::istream &f, std::list<Faction *>& facs)
{
    std::getline(f >> std::ws, name);

    f >> num;

    std::string str;
    f >> std::ws >> str;
    type = lookup_region_type(str);

    f >> buildingseq;
    f >> gate;
    if (gate > 0) f >> gatemonth;


    f >> std::ws >> str;
    race = lookup_item(str);

    f >> population;
    f >> basepopulation;
    f >> wages;
    f >> maxwages;
    f >> wealth;

    f >> elevation;
    f >> humidity;
    f >> temperature;
    f >> vegetation;
    f >> culture;

    f >> habitat;
    f >> development;
    f >> maxdevelopment;

    int n;
    f >> n;
    if (n) {
        town = new TownInfo;
        town->Readin(f);
        town->dev = TownDevelopment();
    } else {
        town = 0;
    }

    f >> xloc;
    f >> yloc;
    f >> zloc;
    f >> visited;

    f >> n;
    products.reserve(n);
    for (int i = 0; i < n; i++) {
        Production *p = new Production();
        p->read_in(f);
        products.push_back(p);
    }

    f >> n;
    markets.reserve(n);
    for (int i = 0; i < n; i++) {
        Market *m = new Market();
        m->read_in(f);
        markets.push_back(m);
    }

    f >> n;
    buildingseq = 1;
    for (int j = 0; j < n; j++) {
        Object *temp = new Object(this);
        temp->Readin(f, facs);
        if (temp->num >= buildingseq)
            buildingseq = temp->num + 1;
        objects.push_back(temp);
    }
    fleetalias = 1;
    newfleets.clear();
}

int ARegion::CanMakeAdv(Faction *fac, int item)
{
    std::string skname;
    int sk;

    if (Globals->IMPROVED_FARSIGHT) {
        for(const auto f : farsees) {
            if (f && f->faction == fac && f->unit) {
                sk = lookup_skill(ItemDefs[item].pSkill);
                if (f->unit->GetSkill(sk) >= ItemDefs[item].pLevel)
                    return 1;
            }
        }
    }

    if ((Globals->TRANSIT_REPORT & GameDefs::REPORT_USE_UNIT_SKILLS) &&
            (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_RESOURCES)) {
        for(const auto f : passers) {
            if (f && f->faction == fac && f->unit) {
                sk = lookup_skill(ItemDefs[item].pSkill);
                if (f->unit->GetSkill(sk) >= ItemDefs[item].pLevel)
                    return 1;
            }
        }
    }

    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->faction == fac) {
                sk = lookup_skill(ItemDefs[item].pSkill);
                if (u->GetSkill(sk) >= ItemDefs[item].pLevel) return 1;
            }
        }
    }
    return 0;
}

int ARegion::HasItem(Faction *fac, int item)
{
    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->faction == fac) {
                if (u->items.GetNum(item)) return 1;
            }
        }
    }
    return 0;
}

json ARegion::basic_region_data() {
    json j;
    j["terrain"] = TerrainDefs[type].name;

    std::string label = (this->level->strName.empty() ? "surface" : this->level->strName);
    j["coordinates"] = { { "x", xloc }, { "y", yloc }, { "z", zloc }, { "label", label } };

    // in order to support games with different UW settings, we need to put a bit more information in the JSON to
    // make the text report easier.. exact depth being hidden is really stupid, but that's the way the text report is.
    if (!Globals->EASIER_UNDERWORLD) {
        std::string z_prefix = "";
        if (zloc >= 2 && zloc < Globals->UNDERWORLD_LEVELS + 2) {
            for (int i = zloc; i > 3; i--) {
                z_prefix += "very ";
            }
            z_prefix += "deep ";
        } else if ((zloc > Globals->UNDERWORLD_LEVELS + 2) &&
                    (zloc < Globals->UNDERWORLD_LEVELS +
                    Globals->UNDERDEEP_LEVELS + 2)) {
            for (int i = zloc; i > Globals->UNDERWORLD_LEVELS + 3; i--) {
                z_prefix += "very ";
            }
            z_prefix += "deep ";
        }
        if (!z_prefix.empty()) j["coordinates"]["depth_prefix"] = z_prefix;
    }

    j["province"] = name;
    if (town) {
        j["settlement"] = { { "name", town->name }, { "size", TownString(town->TownType()) } };
    }
    return j;
}

void ARegion::build_json_report(json& j, Faction *fac, int month, ARegionList& regions) {
    Farsight *farsight = GetFarsight(farsees, fac);
    Farsight *passer = GetFarsight(passers, fac);
    bool present = (Present(fac) == 1) || fac->is_npc;

    // this faction cannot see this region, why are we even here?
    if (!farsight && !passer && !present) return;

    j = basic_region_data();
    j["present"] = present && !fac->is_npc;

    if (Population() && (present || farsight || (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_PEASANTS))) {
        j["population"] = { { "amount", Population() } };
        if (Globals->RACES_EXIST) {
            j["population"]["race"] = ItemDefs[race].names;
        } else {
            j["population"]["race"] = "men";
        }
        j["tax"] = (present || farsight || Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_REGION_MONEY) ? wealth : 0;
    }

    if (Globals->WEATHER_EXISTS) {
        std::string weather_name = (Globals->WEATHER_EXISTS >= 2) ? "clear"
                                 : (clearskies ? "unnaturally clear" : SeasonNames[weather]);
        std::string next_weather = (Globals->WEATHER_EXISTS >= 2) ? "clear"
                                 : SeasonNames[regions.GetWeather(this, (month + 1) % 12)];
        j["weather"] = {
            { "current", weather_name }, { "next", next_weather }
        };
    }

    if (type == R_NEXUS) {
        std::stringstream desc;
        desc << Globals->WORLD_NAME << " Nexus is a magical place: the entryway to the world of "
             << Globals->WORLD_NAME << ". Enjoy your stay; the city guards should keep you safe as long "
             << "as you should choose to stay. However, rumor has it that once you have left the Nexus, "
             << "you can never return.";
        j["description"] = desc.str();
    }

    Production *p = get_production_for_skill(I_SILVER, -1);
    double wages = p ? p->productivity / 10.0 : 0;
    auto max_wages = p ? p->amount : 0;
    j["wages"] = (p && ((Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_WAGES) || present || farsight))
        ? json{ { "amount", wages }, { "max", max_wages } }
        : json{ { "amount", 0 } };

    // Find max QUAM level among faction units present in this region.
    // Used to reveal hidden trade goods (M_SELL IT_TRADE) in city markets.
    // QUAM 2 = 1 item visible, QUAM 3 = 2 items, QUAM 4 = 3 items.
    int quam_level = 0;
    if (present) {
        for (const auto o : objects) {
            for (const auto u : o->units) {
                if (u->faction == fac) {
                    int lvl = u->GetSkill(S_QUARTERMASTER);
                    if (lvl > quam_level) quam_level = lvl;
                }
            }
        }
    }

    json wanted = json::array();
    json for_sale = json::array();
    int quam_revealed = 0;
    for (const auto& m : markets) {
        if (!m->amount) continue;
        if (m->item < 0 || m->item >= NITEMS) continue;
        if (!present && !farsight && !(Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_MARKETS)) continue;

        ItemType itemdef = ItemDefs[m->item];
        json item = {
            { "name", itemdef.name }, { "plural", itemdef.names }, { "tag", itemdef.abr }, { "price", m->price }
        };
        if (m->amount != -1) item["amount"] = m->amount;
        else item["unlimited"] = true;

        if (m->type == Market::MarketType::M_SELL) {
            // Trade goods: hidden until player has the item OR a QUAM unit is present.
            // QUAM level 2+ reveals hidden trade goods: quota = quam_level - 1.
            if (ItemDefs[m->item].type & IT_TRADE) {
                if (!HasItem(fac, m->item)) {
                    int quam_quota = (quam_level >= 2) ? (quam_level - 1) : 0;
                    if (quam_revealed >= quam_quota) continue;
                    quam_revealed++;
                }
            }
            if (ItemDefs[m->item].type & IT_ADVANCED) {
                if (!Globals->MARKETS_SHOW_ADVANCED_ITEMS) {
                    if (!HasItem(fac, m->item)) continue;
                }
            }
            wanted.push_back(item);
        } else {
            for_sale.push_back(item);
        }
    }
    j["markets"] = { { "wanted", wanted }, { "for_sale", for_sale } };

    p = get_production_for_skill(I_SILVER, S_ENTERTAINMENT);
    if (p) {
        j["entertainment"] =
            (present || farsight || (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_ENTERTAINMENT)) ? p->amount : 0;
    }

    j["products"] = json::array();
    for (const auto& p : products) {
        ItemType itemdef = ItemDefs[p->itemtype];
        if (p->itemtype == I_SILVER) continue; // wages and entertainment handled seperately.
        // Advanced items have slightly different rules, so call CanMakeAdv (poorly named) to see if we can see them.
        if (itemdef.type & IT_ADVANCED && !(CanMakeAdv(fac, p->itemtype) || fac->is_npc)) continue;
        // If it's a normal resource, and we aren't here or not showing it, skip it.
        if (!present && !farsight && !(Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_RESOURCES)) continue;
        json item = {
            { "name", itemdef.name }, { "plural", itemdef.names }, { "tag", itemdef.abr },
        };
        // I don't think resources can be unlimited, but just in case, we will handle it.
        if (p->amount != -1) item["amount"] = p->amount;
        else item["unlimited"] = true;
        j["products"].push_back(item);
    }

    bool default_state = (present || farsight || (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_ALL_EXITS));
    bool exits_seen[NDIRS];
    std::fill_n(exits_seen, NDIRS, default_state);
    // If we are only showing used exits, we need to walk the list of whomever passed through and update it with ones
    // our units used.
    if (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_USED_EXITS) {
        for(const auto p : passers) {
            if (p->faction == fac) {
                for (auto i = 0; i < NDIRS; i++) {
                    exits_seen[i] |= (bool)p->exits_used[i];
                }
            }
        }
    }

    j["exits"] = json::array();
    for (int i=0; i<NDIRS; i++) {
        if (!exits_seen[i] || !neighbors[i]) continue;
        j["exits"].push_back(
            { { "direction", DirectionStrs[i] }, { "region", neighbors[i]->basic_region_data() } }
        );
    }

    if (Globals->GATES_EXIST && gate && gate != -1) {
        bool can_see_gate = false;
        if (fac->is_npc) can_see_gate = true;
        if (Globals->IMPROVED_FARSIGHT && farsight) {
            for(const auto watcher : farsees) {
                if (watcher && watcher->faction == fac && watcher->unit) {
                    if (watcher->unit->GetSkill(S_GATE_LORE)) can_see_gate = true;
                }
            }
        }
        if (Globals->TRANSIT_REPORT & GameDefs::REPORT_USE_UNIT_SKILLS) {
            for(const auto watcher : passers) {
                if (watcher && watcher->faction == fac && watcher->unit) {
                    if (watcher->unit->GetSkill(S_GATE_LORE)) can_see_gate = true;
                }
            }
        }
        for(const auto o : objects) {
            for(const auto u : o->units) {
                if ((u->faction == fac) && u->GetSkill(S_GATE_LORE)) can_see_gate = true;
            }
        }

        // Ok, someone from this faction can see the gate.
        if (can_see_gate) {
            j["gate"]["open"] = gateopen;
            if (gateopen) {
                j["gate"]["number"] = gate;
                if (!Globals->DISPERSE_GATE_NUMBERS) {
                    j["gate"]["total"] = regions.numberofgates;
                }
            }
        }
    }

    int obs = GetObservation(fac, 0);
    int truesight = GetTrueSight(fac, 0);
    int detfac = 0;

    int passobs = GetObservation(fac, 1);
    int passtrue = GetTrueSight(fac, 1);
    int passdetfac = detfac;

    if (fac->is_npc) {
        obs = 10;
        passobs = 10;
    }

    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->faction == fac && u->GetSkill(S_MIND_READING) > 1) {
                detfac = 1;
            }
        }
    }
    if (Globals->IMPROVED_FARSIGHT && farsight) {
        for(const auto watcher : farsees) {
            if (watcher && watcher->faction == fac && watcher->unit) {
                if (watcher->unit->GetSkill(S_MIND_READING) > 1) {
                    detfac = 1;
                }
            }
        }
    }

    if ((Globals->TRANSIT_REPORT & GameDefs::REPORT_USE_UNIT_SKILLS) &&
            (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_UNITS)) {
        for(const auto watcher : passers) {
            if (watcher && watcher->faction == fac && watcher->unit) {
                if (watcher->unit->GetSkill(S_MIND_READING) > 1) {
                    passdetfac = 1;
                }
            }
        }
    }

    for(const auto o : objects) {
        o->build_json_report(j, fac, obs, truesight, detfac, passobs, passtrue, passdetfac, present || farsight);
    }
}

int ARegion::GetTrueSight(Faction *f, int usepassers)
{
    int truesight = 0;

    if (Globals->IMPROVED_FARSIGHT) {
        for(const auto farsight : farsees) {
            if (farsight && farsight->faction == f && farsight->unit) {
                int t = farsight->unit->GetSkill(S_TRUE_SEEING);
                if (t > truesight) truesight = t;
            }
        }
    }

    if (usepassers &&
            (Globals->TRANSIT_REPORT & GameDefs::REPORT_USE_UNIT_SKILLS) &&
            (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_UNITS)) {
        for(const auto farsight : passers) {
            if (farsight && farsight->faction == f && farsight->unit) {
                int t = farsight->unit->GetSkill(S_TRUE_SEEING);
                if (t > truesight) truesight = t;
            }
        }
    }

    for(const auto obj : objects) {
        for(const auto u : obj->units) {
            if (u->faction == f) {
                int temp = u->GetSkill(S_TRUE_SEEING);
                if (temp>truesight) truesight = temp;
            }
        }
    }
    return truesight;
}

int ARegion::GetObservation(Faction *f, int usepassers)
{
    int obs = 0;

    if (Globals->IMPROVED_FARSIGHT) {
        for(const auto farsight : farsees) {
            if (farsight && farsight->faction == f && farsight->unit) {
                int o = farsight->observation;
                if (o > obs) obs = o;
            }
        }
    }
    // Tower-based farsight (unit=nullptr): contributes reduced observation independently of IMPROVED_FARSIGHT.
    // No TRUE_SEEING, MIND_READING, or GATE_LORE bonuses apply.
    for(const auto farsight : farsees) {
        if (farsight && farsight->faction == f && !farsight->unit && farsight->observation > 0) {
            int o = farsight->observation;
            if (o > obs) obs = o;
        }
    }

    if (usepassers &&
            (Globals->TRANSIT_REPORT & GameDefs::REPORT_USE_UNIT_SKILLS) &&
            (Globals->TRANSIT_REPORT & GameDefs::REPORT_SHOW_UNITS)) {
        for(const auto farsight : passers) {
            if (farsight && farsight->faction == f && farsight->unit) {
                int o = farsight->observation;
                if (o > obs) obs = o;
            }
        }
    }

    for(const auto obj : objects) {
        for(const auto u : obj->units) {
            if (u->faction == f) {
                int temp = u->GetAttribute("observation");
                if (temp>obs) obs = temp;
            }
        }
    }
    return obs;
}

void ARegion::SetWeather(int newWeather)
{
    weather = newWeather;
}

int ARegion::IsCoastal()
{
    if (type == R_LAKE) {
        if (Globals->LAKESIDE_IS_COASTAL)
            return 1;
    } else if (TerrainDefs[type].similar_type == R_OCEAN)
        return 1;
    int seacount = 0;
    for (int i=0; i<NDIRS; i++) {
        if (!neighbors[i]) continue;

        // Safety check: skip neighbors with invalid types
        if (neighbors[i]->type < 0 || neighbors[i]->type >= (int)TerrainDefs.size())
            continue;

        if (TerrainDefs[neighbors[i]->type].similar_type == R_OCEAN) {
            if (!Globals->LAKESIDE_IS_COASTAL && neighbors[i]->type == R_LAKE) continue;
            seacount++;
        }
    }
    return seacount;
}

int ARegion::IsCoastalOrLakeside()
{
    if (TerrainDefs[type].similar_type == R_OCEAN) return 1;
    int seacount = 0;
    for (int i=0; i<NDIRS; i++) {
        if (!neighbors[i]) continue;

        // Safety check: skip neighbors with invalid types
        if (neighbors[i]->type < 0 || neighbors[i]->type >= (int)TerrainDefs.size())
            continue;

        if (TerrainDefs[neighbors[i]->type].similar_type == R_OCEAN) {
            seacount++;
        }
    }
    return seacount;
}

int ARegion::IsDeepOcean()
{
    // Not ocean - return false
    if (TerrainDefs[type].similar_type != R_OCEAN)
        return 0;

    // Check if any neighbor is land
    for (int i = 0; i < NDIRS; i++) {
        if (!neighbors[i]) continue;

        if (neighbors[i]->type < 0 || neighbors[i]->type >= (int)TerrainDefs.size())
            continue;

        // Found land neighbor - coastal ocean
        if (TerrainDefs[neighbors[i]->type].similar_type != R_OCEAN &&
            neighbors[i]->type != R_LAKE) {
            return 0;
        }
    }

    // All neighbors are water - deep ocean
    return 1;
}

int ARegion::MoveCost(int movetype, ARegion *fromRegion, int dir, std::string *road)
{
    int cost = 1;
    if (Globals->WEATHER_EXISTS == 1) {
        cost = 2;
        if (weather == W_NORMAL || clearskies) {
            cost = 1;
        }
        if (weather == W_BLIZZARD && !clearskies) {
            return 4;
        }
    }
    if (movetype == M_SWIM) {
        cost = (TerrainDefs[type].movepoints * cost);
        // Roads don't help swimming, even if there are any in the ocean
    } else if (movetype == M_WALK || movetype == M_RIDE) {
        cost = (TerrainDefs[type].movepoints * cost);
        if (fromRegion->HasExitRoad(dir) && fromRegion->HasConnectingRoad(dir)) {
            cost -= cost/2;
            if (road)
                *road = "on a road ";
        }
    }
    if (cost < 1) cost = 1;
    return cost;
}

Unit *ARegion::Forbidden(Unit *u)
{
    for(const auto obj : objects) {
        for(const auto u2 : obj->units) {
            if (u2->Forbids(this, u)) return u2;
        }
    }
    return nullptr;
}

Unit *ARegion::ForbiddenByAlly(Unit *u)
{
    for(const auto obj : objects) {
        for(const auto u2 : obj->units) {
            if (u->faction->get_attitude(u2->faction->num) == AttitudeType::ALLY && u2->Forbids(this, u)) return u2;
        }
    }
    return nullptr;
}

int ARegion::HasCityGuard()
{
    for(const auto obj : objects) {
        for(const auto u : obj->units) {
            if (u->type == U_GUARD && u->GetSoldiers() && u->guard == GUARD_GUARD) {
                return 1;
            }
        }
    }
    return 0;
}

bool ARegion::notify_spell_use(Unit *caster, const std::string& spell, ARegionList& regs)
{
    unsigned int i;

    auto skill_def = FindSkill(spell.c_str()).value().get();

    if (!(skill_def.flags & SkillType::NOTIFY)) {
        // Okay, we aren't notifyable, check our prerequisites
        for (i = 0; i < sizeof(skill_def.depends)/sizeof(skill_def.depends[0]); i++) {
            if (skill_def.depends[i].skill == NULL) break;
            if (notify_spell_use(caster, skill_def.depends[i].skill, regs)) return true;
        }
        return false;
    }

    int sp = lookup_skill(spell);
    std::set<Faction *> facs;
    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->faction == caster->faction) continue;
            if (u->GetSkill(sp)) facs.insert(u->faction);
        }
    }

    for(const auto f : facs) {
        std::string tmp = caster->name + " uses " + SkillStrs(sp) + " in " + print() + ".";
        f->event(tmp, "cast", this);
    }
    return true;
}

// ALT, 26-Jul-2000
// Procedure to notify all units in city about city name change
void ARegion::notify_city(Unit *caster, const std::string& oldname, const std::string& newname)
{
    std::set<Faction *> flist;
    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->faction == caster->faction) continue;
            flist.insert(u->faction);
        }
    }
    for(const auto f : flist) {
        std::string tmp = caster->name + " renames " + oldname + " to " + newname + ".";
        f->event(tmp, "rename");
    }
}

int ARegion::CanTax(Unit *u)
{
    for(const auto obj : objects) {
        for(const auto u2 : obj->units) {
            if (u2->guard == GUARD_GUARD && u2->IsAlive())
                if(u2->GetAttitude(this, u) <= AttitudeType::NEUTRAL) return 0;
        }
    }
    return 1;
}

int ARegion::CanGuard(Unit *u)
{
    for(const auto obj : objects) {
        for(const auto u2 : obj->units) {
            if (u2->guard == GUARD_GUARD && u2->IsAlive())
                if (u2->GetAttitude(this, u) < AttitudeType::ALLY) return 0;
        }
    }
    return 1;
}

int ARegion::CanPillage(Unit *u)
{
    for(const auto obj : objects) {
        for(const auto u2 : obj->units) {
            if (u2->guard == GUARD_GUARD && u2->IsAlive() && u2->faction != u->faction) return 0;
        }
    }
    return 1;
}

int ARegion::ForbiddenShip(Object *ship)
{
    for(const auto u : ship->units) {
        if (Forbidden(u)) return 1;
    }
    return 0;
}

void ARegion::DefaultOrders()
{
    for(const auto obj : objects) {
        for(const auto u : obj->units) u->DefaultOrders(obj);
    }
}

//
// This is just used for mapping; just check if there is an inner region.
//
int ARegion::HasShaft()
{
    for(const auto o : objects) {
        if (o->inner != -1) return 1;
    }
    return 0;
}

int ARegion::IsGuarded()
{
    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->guard == GUARD_GUARD) return 1;
        }
    }
    return 0;
}

/**
 * @brief Checks if region has city guards (U_GUARD, U_GUARDMAGE or U_GUARDCOMMANDER)
 *
 * The mayor is deliberately left out: they hold GUARD_AVOID and do not fight for
 * anyone but their own faction, so a town holding nothing but a mayor is not a
 * town defended by guards.
 *
 * @return 1 if at least one alive city guard unit exists, 0 otherwise
 * @see ARegion::GetCityGuard, Game::RunBattle for the reputation penalty
 */
int ARegion::HasCityGuards()
{
    for (const auto o : objects) {
        for (const auto u : o->units) {
            if ((u->type == U_GUARD || u->type == U_GUARDMAGE || u->type == U_GUARDCOMMANDER) &&
                u->IsAlive()) {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Gets the first alive city guard unit in the region
 *
 * Used for checking guard attitudes towards targets in combat.
 *
 * @return Pointer to first city guard unit found, or nullptr if none exist
 */
Unit* ARegion::GetCityGuard()
{
    for (const auto o : objects) {
        for (const auto u : o->units) {
            if ((u->type == U_GUARD || u->type == U_GUARDMAGE || u->type == U_GUARDCOMMANDER) &&
                u->IsAlive()) {
                return u;
            }
        }
    }
    return nullptr;
}

/**
 * @brief Checks if region contains at least one lair object
 *
 * A lair is any object with ObjectDefs[type].monster != -1 (e.g., O_LAIR, O_CAVE, O_RUIN).
 * Used to allow wandering monster spawning in guarded regions that have lairs,
 * since guards cannot prevent monsters from emerging from their lairs.
 *
 * @return 1 if region has at least one lair, 0 otherwise
 * @note Does not check if lair is occupied or if monster type is enabled
 * @see GrowWMons() in npc.cpp for usage in spawn algorithm
 * @see ObjectDefs[].monster for lair definitions
 */
int ARegion::HasLair()
{
    for(const auto o : objects) {
        int montype = ObjectDefs[o->type].monster;
        if (montype != -1) return 1;
    }
    return 0;
}

int ARegion::CountWMons()
{
    int count = 0;
    for(const auto o : objects) {
        for(const auto u : o->units) {
            if (u->type == U_WMON) count ++;
        }
    }
    return count;
}

/* New Fleet objects are stored in the newfleets
 * map for resolving aliases in the Enter NEW phase.
 */
void ARegion::AddFleet(Object *fleet)
{
    objects.push_back(fleet);
    newfleets.insert(std::make_pair(fleetalias++, fleet->num));
}

int ARegion::ResolveFleetAlias(int alias)
{
    auto f = newfleets.find(alias);
    if (f == newfleets.end()) return -1;
    return f->second;
}

ARegionList::ARegionList()
{
    pRegionArrays = 0;
    numLevels = 0;
    numberofgates = 0;
}

ARegionList::~ARegionList()
{
    if (pRegionArrays) {
        int i;
        for (i = 0; i < numLevels; i++) {
            delete pRegionArrays[i];
        }

        delete pRegionArrays;
    }
    std::for_each(regions.begin(), regions.end(), [](ARegion *r) { delete r; });
    regions.clear();
}

void ARegionList::WriteRegions(std::ostream& f)
{
    f << regions.size() << "\n";
    f << numLevels << "\n";
    for (int i = 0; i < numLevels; i++) {
        ARegionArray *pRegs = pRegionArrays[i];
        f << pRegs->x << "\n" << pRegs->y << "\n";
        f << (pRegs->strName.empty() ? "none" : pRegs->strName) << "\n";
        f << pRegs->levelType << "\n";
    }

    f << numberofgates << "\n";
    for(const auto r : regions) r->Writeout(f);

    f << "Neighbors\n";
    for(const auto reg : regions) {
        for (int i = 0; i < NDIRS; i++) {
            f  << (reg->neighbors[i] ? reg->neighbors[i]->num : -1) << '\n';
        }
    }
}

int ARegionList::ReadRegions(std::istream &f, std::list<Faction *>& factions)
{
    int num;
    f >> num;

    f >> numLevels;
    create_levels(numLevels);
    int i;
    for (i = 0; i < numLevels; i++) {
        int curX, curY;
        f >> curX >> curY;
        std::string name;
        std::getline(f >> std::ws, name);
        ARegionArray *pRegs = new ARegionArray(curX, curY);
        if (name == "none") {
            pRegs->strName = "";
        } else {
            pRegs->strName = name;
        }
        f >> pRegs->levelType;
        pRegionArrays[i] = pRegs;
    }

    f >> numberofgates;

    logger::write("Reading the regions...");
    for (i = 0; i < num; i++) {
        ARegion *temp = new ARegion;
        temp->Readin(f, factions);
        regions.push_back(temp);

        pRegionArrays[temp->zloc]->SetRegion(temp->xloc, temp->yloc, temp);
        temp->level = pRegionArrays[temp->zloc];
    }

    logger::write("Setting up the neighbors...");
    std::string temp;
    f >> std::ws >> temp;
    for(const auto reg : regions) {
        for (i = 0; i < NDIRS; i++) {
            int j;
            f >> j;
            if (j != -1) {
                reg->neighbors[i] = regions.at(j);
            } else {
                reg->neighbors[i] = nullptr;
            }
        }
    }
    return 1;
}

ARegion *ARegionList::GetRegion(int n)
{
    return regions.at(n);
}

ARegion *ARegionList::GetRegion(int x, int y, int z)
{

    if (z >= numLevels) return NULL;

    ARegionArray *arr = pRegionArrays[z];

    x = (x + arr->x) % arr->x;
    y = (y + arr->y) % arr->y;

    return(arr->GetRegion(x, y));
}

Location *ARegionList::FindUnit(int i)
{
    for(const auto reg : regions) {
        for(const auto obj : reg->objects) {
            for(const auto u : obj->units) {
                if (u->num == i) {
                    Location *retval = new Location;
                    retval->unit = u;
                    retval->region = reg;
                    retval->obj = obj;
                    return retval;
                }
            }
        }
    }
    return nullptr;
}

void ARegionList::NeighSetup(ARegion *r, ARegionArray *ar)
{
    r->ZeroNeighbors();

    if (r->yloc != 0 && r->yloc != 1) {
        r->neighbors[D_NORTH] = ar->GetRegion(r->xloc, r->yloc - 2);
    }
    if (r->yloc != 0) {
        r->neighbors[D_NORTHEAST] = ar->GetRegion(r->xloc + 1, r->yloc - 1);
        r->neighbors[D_NORTHWEST] = ar->GetRegion(r->xloc - 1, r->yloc - 1);
    }
    if (r->yloc != ar->y - 1) {
        r->neighbors[D_SOUTHEAST] = ar->GetRegion(r->xloc + 1, r->yloc + 1);
        r->neighbors[D_SOUTHWEST] = ar->GetRegion(r->xloc - 1, r->yloc + 1);
    }
    if (r->yloc != ar->y - 1 && r->yloc != ar->y - 2) {
        r->neighbors[D_SOUTH] = ar->GetRegion(r->xloc, r->yloc + 2);
    }
}

void ARegionList::IcosahedralNeighSetup(ARegion *r, ARegionArray *ar)
{
    int scale, x, y, x2, y2, x3, neighX, neighY;

    scale = ar->x / 10;

    r->ZeroNeighbors();

    y = r->yloc;
    x = r->xloc;
    // x2 is the x-coord of this hex inside its "wedge"
    if (y < 5 * scale)
        x2 = x % (2 * scale);
    else
        x2 = (x + 1) % (2 * scale);
    // x3 is the distance of this hex from the right side of its "wedge"
    x3 = (2 * scale - x2) % (2 * scale);
    // y2 is the distance from the SOUTH pole
    y2 = 10 * scale - 1 - y;
    // Always try to connect in the standard way...
    if (y > 1) {
        r->neighbors[D_NORTH] = ar->GetRegion(x, y - 2);
    }
    // but if that fails, use the special icosahedral connections:
    if (!r->neighbors[D_NORTH]) {
        if (y > 0 && y < 3 * scale)
        {
            if (y == 2) {
                neighX = 0;
                neighY = 0;
            }
            else if (y == 3 * x2) {
                neighX = x + 2 * (scale - x2) + 1;
                neighY = y - 1;
            }
            else {
                neighX = x + 2 * (scale - x2);
                neighY = y - 2;
            }
            neighX %= (scale * 10);
            r->neighbors[D_NORTH] = ar->GetRegion(neighX, neighY);
        }
    }
    if (y > 0) {
        neighX = x + 1;
        neighY = y - 1;
        neighX %= (scale * 10);
        r->neighbors[D_NORTHEAST] = ar->GetRegion(neighX, neighY);
    }
    if (!r->neighbors[D_NORTHEAST]) {
        if (y == 0) {
            neighX = 4 * scale;
            neighY = 2;
        }
        else if (y < 3 * scale) {
            if (y == 3 * x2) {
                neighX = x + 2 * (scale - x2) + 1;
                neighY = y + 1;
            }
            else {
                neighX = x + 2 * (scale - x2);
                neighY = y;
            }
        }
        else if (y2 < 1) {
            neighX = x + 2 * scale;
            neighY = y - 2;
        }
        else if (y2 < 3 * scale) {
            neighX = x + 2 * (scale - x2);
            neighY = y - 2;
        }
        neighX %= (scale * 10);
        r->neighbors[D_NORTHEAST] = ar->GetRegion(neighX, neighY);
    }
    if (y2 > 0) {
        neighX = x + 1;
        neighY = y + 1;
        neighX %= (scale * 10);
        r->neighbors[D_SOUTHEAST] = ar->GetRegion(neighX, neighY);
    }
    if (!r->neighbors[D_SOUTHEAST]) {
        if (y == 0) {
            neighX = 2 * scale;
            neighY = 2;
        }
        else if (y2 < 1) {
            neighX = x + 4 * scale;
            neighY = y - 2;
        }
        else if (y2 < 3 * scale) {
            if (y2 == 3 * x2) {
                neighX = x + 2 * (scale - x2) + 1;
                neighY = y - 1;
            }
            else {
                neighX = x + 2 * (scale - x2);
                neighY = y;
            }
        }
        else if (y < 3 * scale) {
            neighX = x + 2 * (scale - x2);
            neighY = y + 2;
        }
        neighX %= (scale * 10);
        r->neighbors[D_SOUTHEAST] = ar->GetRegion(neighX, neighY);
    }
    if (y2 > 1) {
        r->neighbors[D_SOUTH] = ar->GetRegion(x, y + 2);
    }
    if (!r->neighbors[D_SOUTH]) {
        if (y2 > 0 && y2 < 3 * scale)
        {
            if (y2 == 2) {
                neighX = 10 * scale - 1;
                neighY = y + 2;
            }
            else if (y2 == 3 * x2) {
                neighX = x + 2 * (scale - x2) + 1;
                neighY = y + 1;
            }
            else {
                neighX = x + 2 * (scale - x2);
                neighY = y + 2;
            }
            neighX = (neighX + scale * 10) % (scale * 10);
            r->neighbors[D_SOUTH] = ar->GetRegion(neighX, neighY);
        }
    }
    if (y2 > 0) {
        neighX = x - 1;
        neighY = y + 1;
        neighX = (neighX + scale * 10) % (scale * 10);
        r->neighbors[D_SOUTHWEST] = ar->GetRegion(neighX, neighY);
    }
    if (!r->neighbors[D_SOUTHWEST]) {
        if (y == 0) {
            neighX = 8 * scale;
            neighY = 2;
        }
        else if (y2 < 1) {
            neighX = x + 6 * scale;
            neighY = y - 2;
        }
        else if (y2 < 3 * scale) {
            if (y2 == 3 * x3 + 4) {
                neighX = x + 2 * (x3 - scale) + 1;
                neighY = y + 1;
            }
            else {
                neighX = x + 2 * (x3 - scale);
                neighY = y;
            }
        }
        else if (y < 3 * scale) {
            neighX = x - 2 * (scale - x3) + 1;
            neighY = y + 1;
        }
        neighX = (neighX + scale * 10) % (scale * 10);
        r->neighbors[D_SOUTHWEST] = ar->GetRegion(neighX, neighY);
    }
    if (y > 0) {
        neighX = x - 1;
        neighY = y - 1;
        neighX = (neighX + scale * 10) % (scale * 10);
        r->neighbors[D_NORTHWEST] = ar->GetRegion(neighX, neighY);
    }
    if (!r->neighbors[D_NORTHWEST]) {
        if (y == 0) {
            neighX = 6 * scale;
            neighY = 2;
        }
        else if (y < 3 * scale) {
            if (y == 3 * x3 + 4) {
                neighX = x + 2 * (x3 - scale) + 1;
                neighY = y - 1;
            }
            else {
                neighX = x + 2 * (x3 - scale);
                neighY = y;
            }
        }
        else if (y2 < 1) {
            neighX = x + 8 * scale;
            neighY = y - 2;
        }
        else if (y2 < 3 * scale) {
            neighX = x - 2 * (scale - x3) + 1;
            neighY = y - 1;
        }
        neighX = (neighX + scale * 10) % (scale * 10);
        r->neighbors[D_NORTHWEST] = ar->GetRegion(neighX, neighY);
    }
}

void ARegionList::CalcDensities()
{
    logger::write("Densities:");
    int arr[R_NUM];
    int i;
    for (i=0; i<R_NUM; i++)
        arr[i] = 0;
    for(const auto reg : regions) {
        arr[reg->type]++;
    }
    for (i=0; i<R_NUM; i++)
        if (arr[i]) logger::write(TerrainDefs[i].name + " " + std::to_string(arr[i]));

    logger::write("");
}

void ARegionList::TownStatistics()
{
    int villages = 0;
    int towns = 0;
    int cities = 0;
    for(const auto reg : regions) {
        if (reg->town) {
            switch(reg->town->TownType()) {
                case TOWN_VILLAGE:
                    villages++;
                    break;
                case TOWN_TOWN:
                    towns++;
                    break;
                case TOWN_CITY:
                    cities++;
            }
        }
    }
    int tot = villages + towns + cities;

    // A generated map can legitimately end up with no settlements at all: on a small,
    // mostly ocean surface every Poisson sample can be rejected on terrain. Dividing
    // the shares by that count killed world creation outright with SIGFPE, so the
    // report has to survive the case it exists to make visible.
    logger::write("Settlements: " + std::to_string(tot));
    logger::write("Villages: " + std::to_string(villages) + " (" + std::to_string(percent_rounded(villages, tot)) + "%)");
    logger::write("Towns   : " + std::to_string(towns) + " (" + std::to_string(percent_rounded(towns, tot)) + "%)");
    logger::write("Cities  : " + std::to_string(cities) + " (" + std::to_string(percent_rounded(cities, tot)) + "%)");
    if (tot == 0)
        logger::write("WARNING: this world has no settlements on any level and is not playable.");
    logger::write("");
}

/**
 * @brief Reports regions left with the default name after world creation
 *
 * A level whose generation path forgets to name its regions produces hexes that
 * print as "Region" in every player report for the life of the world, and names
 * cannot be repaired afterwards because they are baked into game.out. Run at the
 * end of world creation so the mistake shows up in gen.log immediately.
 *
 * @note Reports only; generation is not aborted.
 * @see ARegion::assign_generated_name()
 */
void ARegionList::NameStatistics()
{
    std::vector<int> unnamed(numLevels > 0 ? numLevels : 1, 0);

    for (const auto reg : regions) {
        if (reg->name != UNNAMED_REGION) continue;
        if (reg->zloc >= 0 && reg->zloc < (int)unnamed.size()) unnamed[reg->zloc]++;
    }

    int total = 0;
    for (size_t level = 0; level < unnamed.size(); level++) {
        if (unnamed[level] == 0) continue;
        total += unnamed[level];
        logger::write(
            "ERROR: level " + std::to_string(level) + " left " + std::to_string(unnamed[level]) +
            " region(s) unnamed - this level's generation path skips region naming"
        );
    }

    if (total == 0) logger::write("Region names: every region named");
    logger::write("");
}

/**
 * @brief Connected land components of one level, with their settlements
 *
 * Flood-fills across hexes that are neither water nor barren. A large landmass
 * carrying no settlement is a slice of the map that is effectively out of play,
 * which no other statistic reveals.
 *
 * @note Part of the generation tuning report; see GENERATION_TUNING_STATS.
 */
void ARegionList::report_landmasses(ARegionArray *arr, int level)
{
    auto is_land = [](ARegion *r) {
        if (!r) return false;
        if (TerrainDefs[r->type].similar_type == R_OCEAN) return false;
        if (r->type == R_BARREN) return false;
        return true;
    };

    std::unordered_set<ARegion *> seen;
    std::vector<std::pair<int, int>> masses;  // size, settlement count
    int total_land = 0, biggest = 0, biggest_empty = 0;

    for (int x = 0; x < arr->x; x++) {
        for (int y = 0; y < arr->y; y++) {
            ARegion *start = arr->GetRegion(x, y);
            if (!is_land(start) || seen.count(start)) continue;

            int size = 0, settlements = 0;
            std::vector<ARegion *> stack = { start };
            seen.insert(start);
            while (!stack.empty()) {
                ARegion *cur = stack.back();
                stack.pop_back();
                size++;
                if (cur->town) settlements++;
                for (int d = 0; d < NDIRS; d++) {
                    ARegion *n = cur->neighbors[d];
                    if (!is_land(n) || seen.count(n)) continue;
                    seen.insert(n);
                    stack.push_back(n);
                }
            }

            total_land += size;
            if (size > biggest) biggest = size;
            if (settlements == 0 && size > biggest_empty) biggest_empty = size;
            masses.push_back({ size, settlements });
        }
    }

    if (masses.empty()) return;

    std::sort(masses.begin(), masses.end(), std::greater<>());

    logger::write(
        "landmasses: " + std::to_string(masses.size()) +
        ", land hexes " + std::to_string(total_land) +
        ", largest " + std::to_string(biggest) +
        " (" + std::to_string(percent_rounded(biggest, total_land)) + "% of land)"
    );

    int shown = 0;
    for (const auto &[size, settlements] : masses) {
        if (shown++ >= 8) break;
        logger::write(
            "  landmass " + std::to_string(size) + " hexes, " +
            std::to_string(settlements) + " settlement(s)" +
            (settlements == 0 && size >= 10 ? "   <-- no settlement" : "")
        );
    }
    if (biggest_empty >= 10)
        logger::write("  largest landmass with no settlement: " + std::to_string(biggest_empty) + " hexes");
}

/**
 * @brief Moves to the nearest other settlement, over the level's real links
 *
 * Breadth-first over ARegion::neighbors, which is the graph units actually walk.
 * GetPlanarDistance() cannot be used here: it measures a straight line across the
 * grid, and MakeUWMaze() severs neighbour links underground - on a 48x48 world the
 * underworld keeps 3.75 of 6 sides open and the underdeep 3.14, so a planar
 * distance there understates the walk by a third and reorders the settlements.
 * On the surface, where almost nothing is severed, both metrics agree.
 *
 * @param from     settlement to measure from
 * @param settled  every settled hex on the level, including from
 * @return moves to the nearest other settlement, or -1 if none is reachable
 * @note Part of the generation tuning report; see GENERATION_TUNING_STATS.
 */
static int hops_to_nearest_settlement(ARegion *from, const std::unordered_set<ARegion *> &settled)
{
    std::unordered_set<ARegion *> seen = { from };
    std::queue<std::pair<ARegion *, int>> queue;
    queue.push({ from, 0 });

    while (!queue.empty()) {
        auto [cur, dist] = queue.front();
        queue.pop();

        if (cur != from && settled.count(cur)) return dist;

        for (int d = 0; d < NDIRS; d++) {
            ARegion *n = cur->neighbors[d];
            if (!n || seen.count(n)) continue;
            seen.insert(n);
            queue.push({ n, dist + 1 });
        }
    }

    return -1;
}

/**
 * @brief Generation-time tuning report for one map level
 *
 * Terrain against settlements, landmasses and settlement spacing for any level.
 * Villages are counted apart from towns and cities because the gateway entry
 * ladder only ever lands a player in a village.
 *
 * The starting-location section - how many settlements meet every requirement,
 * what the failing ones lack, and how much land each can work within three moves -
 * is printed for the surface alone. Nothing leads downward out of the nexus, so
 * an underground settlement is never an entry point and measuring it against the
 * entry rule reports a failure that means nothing.
 *
 * @note Part of the generation tuning report; see GENERATION_TUNING_STATS.
 * @see ARegionList::MapStatistics()
 */
void ARegionList::report_level(ARegionArray *arr, int level)
{
    std::vector<ARegion *> here;
    for (const auto reg : regions)
        if (reg->zloc == level) here.push_back(reg);
    if (here.empty()) return;

    // The surface is created with an empty name on purpose (world.cpp), so it is
    // the one level that has to be named here rather than read off the array.
    std::string level_name =
        arr->levelType == ARegionArray::LEVEL_SURFACE ? "surface" :
        arr->strName.empty() ? ("level " + std::to_string(level)) : arr->strName;
    logger::write("=== TUNING: " + level_name + " (level " + std::to_string(level) + ") ===");
    logger::write("terrain      hexes  village  town  city");

    int land = 0, water = 0;
    for (int type = 0; type < R_NUM; type++) {
        int hexes = 0, villages = 0, towns = 0, cities = 0;
        for (const auto reg : here) {
            if (reg->type != type) continue;
            hexes++;
            if (!reg->town) continue;
            switch (reg->town->TownType()) {
                case TOWN_VILLAGE: villages++; break;
                case TOWN_TOWN:    towns++;    break;
                case TOWN_CITY:    cities++;   break;
            }
        }
        if (hexes == 0) continue;

        if (TerrainDefs[type].similar_type == R_OCEAN) water += hexes; else land += hexes;

        std::string line = TerrainDefs[type].name;
        line.resize(12, ' ');
        line += " " + std::to_string(hexes) + "  " + std::to_string(villages) +
                "  " + std::to_string(towns) + "  " + std::to_string(cities);

        // Only the surface carries gateways, so only there does a terrain without
        // a village break the entry ladder.
        bool gateway_terrain = (arr->levelType == ARegionArray::LEVEL_SURFACE &&
                                type >= R_PLAIN && type <= R_TUNDRA);
        if (gateway_terrain && villages == 0) line += "   <-- NO VILLAGE (gateway terrain)";

        logger::write(line);
    }

    logger::write("land hexes: " + std::to_string(land) + ", water hexes: " + std::to_string(water));

    report_landmasses(arr, level);

    std::vector<ARegion *> settled;
    for (const auto reg : here) if (reg->town) settled.push_back(reg);

    std::unordered_set<ARegion *> settled_set(settled.begin(), settled.end());
    std::vector<int> nearest;
    for (const auto a : settled) {
        int d = hops_to_nearest_settlement(a, settled_set);
        if (d >= 0) nearest.push_back(d);
    }

    DistanceSummary spacing = summarise_distances(nearest);
    if (spacing.count == 0) {
        logger::write("settlement spacing: n/a (" + std::to_string(settled.size()) + " settlement(s))");
    } else {
        logger::write(
            "settlement spacing in moves: count " + std::to_string(spacing.count) +
            ", min " + std::to_string(spacing.min) +
            ", median " + std::to_string(spacing.median) +
            ", mean " + std::to_string(spacing.mean_tenths / 10) + "." +
                        std::to_string(spacing.mean_tenths % 10) +
            ", max " + std::to_string(spacing.max)
        );
    }

    // Starting requirements are a surface question: the nexus has no exit
    // downward, so the entry ladder only ever lands a player on the surface.
    // Underground settlements are reached on foot, from a base that already
    // exists, and measuring them against the entry rule says nothing.
    if (arr->levelType != ARegionArray::LEVEL_SURFACE || settled.empty()) {
        logger::write("");
        return;
    }

    int complete = 0;
    std::vector<int> reaches;
    std::vector<std::string> shortfalls;
    for (const auto reg : settled) {
        StartRequirements req = arr->start_requirements_at(reg);
        reaches.push_back(req.reach);
        if (req.all()) { complete++; continue; }

        std::string missing;
        if (!req.wood)     missing += " wood";
        if (!req.iron)     missing += " iron";
        if (!req.stone)    missing += " stone";
        if (!req.food)     missing += " food";
        if (!req.mounts)   missing += " mounts";
        if (!req.landmass) missing += " landmass";
        shortfalls.push_back(
            "  " + reg->town->name + " (" + std::string(TerrainDefs[reg->type].name) +
            ", reach " + std::to_string(req.reach) + "):" + missing
        );
    }

    logger::write(
        "settlements meeting every starting requirement: " +
        std::to_string(complete) + " of " + std::to_string(settled.size())
    );
    for (const auto &line : shortfalls) logger::write(line);

    // How much land a settlement can work within the three moves the requirement
    // check itself walks. A low minimum is the signature of a settlement pinned
    // against the coast, which no placement quota can rescue - such a hex has to
    // be kept out of the entry pool instead.
    DistanceSummary reach = summarise_distances(reaches);
    logger::write(
        "hexes within three moves: min " + std::to_string(reach.min) +
        ", median " + std::to_string(reach.median) +
        ", mean " + std::to_string(reach.mean_tenths / 10) + "." +
                    std::to_string(reach.mean_tenths % 10) +
        ", max " + std::to_string(reach.max)
    );

    logger::write("");
}

/**
 * @brief Generation-time tuning report on the finished map
 *
 * Walks every playable level, then appends the surface-only sections: how the
 * requested generation parameters actually came out, and the starting-location
 * candidate pool the gateway entry ladder draws from.
 *
 * The requested-versus-delivered section exists because Densities: sums every
 * level into one histogram, so the surface water share is not visible anywhere,
 * and because the water percentage is applied to the noise grid, which is twice
 * the resolution of the hex grid (mapgen.cpp) - the delivered share is expected
 * to differ from the requested one, and this says by how much.
 *
 * @note Runs only from Game::CreateWorld(), guarded by GENERATION_TUNING_STATS.
 */
void ARegionList::MapStatistics()
{
    if constexpr (!GENERATION_TUNING_STATS) return;

    for (int level = 0; level < numLevels; level++) {
        ARegionArray *arr = pRegionArrays[level];
        if (!arr) continue;
        // The nexus is a single hex; the dungeon level is entirely barren until
        // dungeons spawn during play. Neither says anything at creation time.
        if (arr->levelType == ARegionArray::LEVEL_NEXUS) continue;
        if (arr->levelType == ARegionArray::LEVEL_DUNGEON) continue;

        report_level(arr, level);
    }

    ARegionArray *surface = GetRegionArray(ARegionArray::LEVEL_SURFACE);
    if (!surface) return;

    int total = 0, water = 0, mountain = 0, hill = 0;
    for (const auto reg : regions) {
        if (reg->zloc != ARegionArray::LEVEL_SURFACE) continue;
        total++;
        if (TerrainDefs[reg->type].similar_type == R_OCEAN) water++;
        if (reg->type == R_MOUNTAIN || reg->type == R_VOLCANO) mountain++;
        if (reg->type == R_HILL) hill++;
    }

    logger::write("=== TUNING: requested vs delivered (surface) ===");
    logger::write("water:    " + std::to_string(percent_rounded(water, total)) + "% of all surface hexes");
    logger::write("mountain: " + std::to_string(percent_rounded(mountain, total)) + "% of all surface hexes");
    logger::write("hill:     " + std::to_string(percent_rounded(hill, total)) + "% of all surface hexes");
    logger::write(
        "hills as a share of high ground: " +
        std::to_string(percent_rounded(hill, mountain + hill)) + "%"
    );
    logger::write("Compare against the Water / Mountains / Hills values in the generator prompt.");
    logger::write("");

    logger::write("=== TUNING: starting-location candidates per gateway terrain ===");
    logger::write("terrain      candidates  of which villages");

    for (int type = R_PLAIN; type <= R_TUNDRA; type++) {
        auto cands = surface->get_starting_region_candidates(type, true);
        int with_village = 0;
        for (const auto reg : cands)
            if (reg->town && reg->town->TownType() == TOWN_VILLAGE) with_village++;

        std::string line = TerrainDefs[type].name;
        line.resize(12, ' ');
        line += " " + std::to_string(cands.size()) + "  " + std::to_string(with_village);
        if (with_village == 0) line += "   <-- entry ladder Block A cannot be satisfied";
        logger::write(line);
    }
    logger::write("");
}

ARegion *ARegionList::FindGate(int x)
{
    if (x == -1) {
        std::vector<ARegion *> gates;
        int count = 0;

        for(const auto r : regions) {
            if (r->gate) gates.push_back(r);
        }
        if(gates.empty()) return nullptr;

        count = rng::get_random(gates.size());
        return gates[count];
    }
    for(const auto r : regions) {
        if (r->gate == x) return r;
    }
    return nullptr;
}

ARegion *ARegionList::FindConnectedRegions(ARegion *r, ARegion *tail, int shaft)
{
    int i;
    ARegion *inner;

    for (i = 0; i < NDIRS; i++) {
            if (r->neighbors[i] && r->neighbors[i]->distance == -1) {
                    tail->next = r->neighbors[i];
                    tail = tail->next;
                    tail->distance = r->distance + 1;
            }
    }

    if (shaft) {
        for(const auto o : r->objects) {
            if (o->inner != -1) {
                inner = GetRegion(o->inner);
                if (inner && inner->distance == -1) {
                    tail->next = inner;
                    tail = tail->next;
                    tail->distance = r->distance + 1;
                }
            }
        }
    }

    return tail;
}

ARegion *ARegionList::FindNearestStartingCity(ARegion *start, int *dir)
{
    ARegion *r, *queue, *inner;
    int offset, i, valid;

    for(const auto r : regions) {
        r->distance = -1;
        r->next = nullptr;
    }

    start->distance = 0;
    queue = start;
    while (start) {
        queue = FindConnectedRegions(start, queue, 1);
        valid = 0;
        if (start) {
            if (Globals->START_CITIES_EXIST) {
                if (start->IsStartingCity())
                    valid = 1;
            } else {
                // No starting cities?
                // Then any explored settlement will do
                if (start->town && start->visited)
                    valid = 1;
            }
        }
        if (valid) {
            if (dir) {
                offset = rng::get_random(NDIRS);
                for (i = 0; i < NDIRS; i++) {
                    r = start->neighbors[(i + offset) % NDIRS];
                    if (!r)
                        continue;
                    if (r->distance + 1 == start->distance) {
                        *dir = (i + offset) % NDIRS;
                        break;
                    }
                }
                for(const auto o : start->objects) {
                    if (o->inner != -1) {
                        inner = GetRegion(o->inner);
                        if (inner->distance + 1 == start->distance) {
                            *dir = MOVE_IN;
                            break;
                        }
                    }
                }
            }
            return start;
        }
        start = start->next;
    }

    // This should never happen!
    return nullptr;
}

// Some structures for the get_connected_distance function
// structures to allow us to do an efficient search
struct RegionVisited {
    int x, y, z;
    bool operator==(const RegionVisited &v) const { return x == v.x && y == v.y && z == v.z; }
 };
class RegionVisitHash {
public:
    size_t operator()(const RegionVisited v) const {
        return std::hash<uint32_t>()(v.x) ^ std::hash<uint32_t>()(v.y) ^ std::hash<uint32_t>()(v.z);
    }
};
struct QEntry { ARegion *r; int dist; };
class QEntryCompare {
public:
    // We want to sort by min distance
    bool operator()(const QEntry &below, const QEntry &above) const { return above.dist < below.dist; }
};

// This doesn't really need to be on the ARegionList but, it's okay for now.
int ARegionList::get_connected_distance(ARegion *start, ARegion *target, int penalty, int maxdist) {
    std::unordered_set<RegionVisited, RegionVisitHash> visited_regions;
    // We want to search the closest regions first so that as soon as we find one that is too far we know *all* the
    // rest will be too far as well.
    std::priority_queue<QEntry, std::vector<QEntry>, QEntryCompare> q;

    if (start == 0 || target == 0) {
        // We were given some unusual (nonexistant) regions
        return 10000000;
    }
    ARegion *cur = start;
    int cur_dist = 0;

    while (maxdist == -1 || cur_dist <= maxdist) {
        // If we have hit our target, we are done
        if (cur == target) {
            // found our target within range
            return cur_dist;
        }

        // Add my current region to the visited set to make sure we don't loop
        visited_regions.insert({cur->xloc, cur->yloc, cur->zloc});

        // Add all neighbors to the queue as long as we haven't visited them yet
        for (int i = 0; i < NDIRS; i++) {
            ARegion *n = cur->neighbors[i];
            if (n == nullptr) continue; // edge of map has missing neighbors
            // cur and n *should* have the same zloc, but ... let's just future-proof in case that changes sometime
            int cost = (cur->zloc == n->zloc ? 1 : penalty);
            if (n && visited_regions.insert({n->xloc, n->yloc, n->zloc}).second) {
                q.push({n, cur_dist + cost });
            }
        }
        // Add any inner regions to the queue as long as we haven't visited them yet
        for(const auto o : cur->objects) {
            if (o->inner != -1) {
                ARegion *inner = GetRegion(o->inner);
                int cost = (cur->zloc == inner->zloc ? 1 : penalty);
                if (visited_regions.insert({inner->xloc, inner->yloc, inner->zloc}).second) {
                    q.push({inner, cur_dist + cost});
                }
            }
        }

        cur = q.top().r;
        cur_dist = q.top().dist;
        q.pop();
    }

    // Should only be hit if the target is > maxdist from the start location
    return 10000000;
}

int ARegionList::GetPlanarDistance(ARegion *one, ARegion *two, int penalty, int maxdist)
{
    // make sure you cannot teleport into or from the nexus
    if (Globals->NEXUS_EXISTS && (one->zloc == ARegionArray::LEVEL_NEXUS || two->zloc == ARegionArray::LEVEL_NEXUS))
        return 10000000;

    if (Globals->ABYSS_LEVEL) {
        // make sure you cannot teleport into or from the abyss
        int ablevel = Globals->UNDERWORLD_LEVELS +
            Globals->UNDERDEEP_LEVELS + 2;
        if (one->zloc == ablevel || two->zloc == ablevel)
            return 10000000;
    }

    int one_x, one_y, two_x, two_y;
    int maxy;
    ARegionArray *pArr = (Globals->NEXUS_EXISTS ? pRegionArrays[ARegionArray::LEVEL_SURFACE] : pRegionArrays[0]);

    one_x = one->xloc * GetLevelXScale(one->zloc);
    one_y = one->yloc * GetLevelYScale(one->zloc);

    two_x = two->xloc * GetLevelXScale(two->zloc);
    two_y = two->yloc * GetLevelYScale(two->zloc);

    if (Globals->ICOSAHEDRAL_WORLD) {
        int zdist;
        ARegion *start, *target, *queue;

        start = pArr->GetRegion(one_x, one_y);
        if (start == 0) {
            one_x += GetLevelXScale(one->zloc) - 1;
            one_y += GetLevelYScale(one->zloc) - 1;
            start = pArr->GetRegion(one_x, one_y);
        }

        target = pArr->GetRegion(two_x, two_y);
        if (target == 0) {
            two_x += GetLevelXScale(two->zloc) - 1;
            two_y += GetLevelYScale(two->zloc) - 1;
            target = pArr->GetRegion(two_x, two_y);
        }

        if (start == 0 || target == 0) {
            // couldn't find equivalent locations on
            // the surface (this should never happen)
            logger::write("Unable to find ends pathing from (" +
                std::to_string(one->xloc) + "," +
                std::to_string(one->yloc) + "," +
                std::to_string(one->zloc) + ") to (" +
                std::to_string(two->xloc) + "," +
                std::to_string(two->yloc) + "," +
                std::to_string(two->zloc) + ")!");
            return 10000000;
        }

        for(const auto r : regions) {
            r->distance = -1;
            r->next = 0;
        }

        zdist = (one->zloc - two->zloc);
        if (zdist < 0) zdist = -zdist;
        start->distance = zdist * penalty;
        queue = start;
        while (maxdist == -1 || start->distance <= maxdist) {
            if (start->xloc == two_x && start->yloc == two_y) {
                // found our target within range
                return start->distance;
            }
            // add neighbours to the search list
            queue = FindConnectedRegions(start, queue, 0);
            start = start->next;
            if (start == 0)
            {
                // ran out of hexes to search
                // (this should never happen)
                logger::write("Unable to find path from (" +
                    std::to_string(one->xloc) + "," +
                    std::to_string(one->yloc) + "," +
                    std::to_string(one->zloc) + ") to (" +
                    std::to_string(two->xloc) + "," +
                    std::to_string(two->yloc) + "," +
                    std::to_string(two->zloc) + ")!");
                return 10000000;
            }
        }
        // didn't find the target within range
        return start->distance;
    } else {
        maxy = one_y - two_y;
        if (maxy < 0) maxy=-maxy;

        int maxx = one_x - two_x;
        if (maxx < 0) maxx = -maxx;

        int max2 = one_x + pArr->x - two_x;
        if (max2 < 0) max2 = -max2;
        if (max2 < maxx) maxx = max2;

        max2 = one_x - (two_x + pArr->x);
        if (max2 < 0) max2 = -max2;
        if (max2 < maxx) maxx = max2;

        if (maxy > maxx) maxx = (maxx+maxy)/2;

        if (one->zloc != two->zloc) {
            int zdist = (one->zloc - two->zloc);
            if ((two->zloc - one->zloc) > zdist)
                zdist = two->zloc - one->zloc;
            maxx += (penalty * zdist);
        }

        return maxx;
    }
}

ARegionArray *ARegionList::GetRegionArray(int level)
{
    return(pRegionArrays[level]);
}

ARegionArray *ARegionList::get_first_region_array_of_type(int levelType) {
    for (int i = 0; i < numLevels; i++) {
        if (pRegionArrays[i]->levelType == levelType) {
            return pRegionArrays[i];
        }
    }
    return nullptr;
}

void ARegionList::create_levels(int n)
{
    numLevels = n;
    pRegionArrays = new ARegionArray *[n];
}

void ARegionList::expand_levels(int newNumLevels)
{
    auto newArr = new ARegionArray *[newNumLevels];
    for (int i = 0; i < numLevels; i++) newArr[i] = pRegionArrays[i];
    delete[] pRegionArrays;
    pRegionArrays = newArr;
    numLevels = newNumLevels;
}

ARegionArray::ARegionArray(int xx, int yy)
{
    x = xx;
    y = yy;
    regions = new ARegion *[x * y / 2 + 1];
    strName = "";

    int i;
    for (i = 0; i < x * y / 2; i++) regions[i] = 0;
}

ARegionArray::~ARegionArray()
{
    delete [] regions;
}

void ARegionArray::SetRegion(int xx, int yy, ARegion *r)
{
    regions[xx / 2 + yy * x / 2] = r;
}

ARegion *ARegionArray::GetRegion(int xx, int yy)
{
    xx = (xx + x) % x;
    yy = (yy + y) % y;
    if ((xx + yy) % 2) return(0);
    return(regions[xx / 2 + yy * x / 2]);
}

std::vector<ARegion *> ARegionArray::get_starting_region_candidates(int terrain) {
    return get_starting_region_candidates(terrain, true);
}

/**
 * @brief Which starting-location requirements a hex satisfies within three moves
 *
 * Walks outward three moves over non-ocean hexes and records which of the required
 * resource groups are reachable, plus whether the surrounding landmass is big
 * enough to be worth starting on at all.
 *
 * The search is called with maxDistance 2 but reaches three moves: the BFS overload
 * skips a node only when its distance is already GREATER than the limit, so nodes at
 * distance 2 are still expanded and their neighbours recorded. Kept deliberately as
 * of 2026-08-09 - the candidate pools this produces are what the gateway system is
 * tuned against, and tightening the bound would move where players land on worlds
 * that already exist. Do not "fix" it without re-measuring.
 *
 * Split out of get_starting_region_candidates() so the gateway candidate filter
 * and the generation statistics share one definition of the rule instead of
 * keeping two copies that can drift apart.
 *
 * @param reg hex to test
 * @return per-group result; StartRequirements::all() is the filter's own verdict
 * @see ARegionArray::get_starting_region_candidates()
 */
StartRequirements ARegionArray::start_requirements_at(ARegion *reg)
{
    StartRequirements req;

    ARegionGraph graph = ARegionGraph(this);
    graph.setInclusion([](ARegion *current, ARegion *next) { return next->type != R_OCEAN; });

    graphs::Location2D loc = { reg->xloc, reg->yloc };
    auto result = graphs::breadthFirstSearch(graph, loc, 2);

    // A hex on a sliver of land is no place to start, however well stocked.
    req.reach = (int)result.size();
    req.landmass = (req.reach >= 10);

    bool grain = false, livestock = false, horse = false, camel = false;
    for (const auto &kv : result) {
        ARegion *tReg = graph.get(kv.first);
        if (tReg->produces_item(I_WOOD))      req.wood  = true;
        if (tReg->produces_item(I_IRON))      req.iron  = true;
        if (tReg->produces_item(I_STONE))     req.stone = true;
        if (tReg->produces_item(I_GRAIN))     grain     = true;
        if (tReg->produces_item(I_LIVESTOCK)) livestock = true;
        if (tReg->produces_item(I_HORSE))     horse     = true;
        if (tReg->produces_item(I_CAMEL))     camel     = true;
    }
    req.food   = grain || livestock;
    req.mounts = horse || camel;

    return req;
}

std::vector<ARegion *> ARegionArray::get_starting_region_candidates(int terrain, bool require_resources) {
    std::vector<ARegion *> candidates;
    for (int x2 = 0; x2 < x; x2++) {
        for (int y2 = 0; y2 < y; y2++) {
            ARegion *reg = GetRegion(x2, y2);
            if (!reg) continue;
            if (reg->type != terrain) continue;

            StartRequirements req = start_requirements_at(reg);
            if (!req.landmass) continue;
            if (require_resources && !req.all()) continue;

            candidates.push_back(reg);
        }
    }

    return candidates;
}

/**
 * @brief Reduces a set of distances to min, median, mean and max
 *
 * @param distances by value; sorted in place
 * @return summary with mean_tenths = mean x 10, so callers print one decimal
 *         without floating point. An empty input gives all zeroes.
 */
DistanceSummary summarise_distances(std::vector<int> distances)
{
    DistanceSummary s;
    if (distances.empty()) return s;

    std::sort(distances.begin(), distances.end());

    s.count  = (int)distances.size();
    s.min    = distances.front();
    s.max    = distances.back();
    s.median = distances[distances.size() / 2];

    int total = 0;
    for (int d : distances) total += d;
    s.mean_tenths = rounded_div(total * 10, s.count);

    return s;
}

/**
 * @brief Is a candidate site far enough from every settlement already placed?
 *
 * The spacing is rolled fresh for each placement, so the caller passes the roll in.
 * No caller rolls below 4: at 3 one player could sit between two settlements and
 * take both in a single move, since a hex adjacent to two of them exists only at
 * distance 2.
 *
 * @param distances hex-step distances to every settlement already placed
 * @param min_spacing the floor; an empty distance list always passes
 */
bool far_enough(const std::vector<int>& distances, int min_spacing)
{
    for (int d : distances) if (d < min_spacing) return false;
    return true;
}

/**
 * @brief Place settlements by walking the terrains round-robin, scarcest first.
 *
 * Shared by the surface (`economy()`) and the underground (`economy_underground()`
 * in neworigins/map.cpp). Terrain drives the loop rather than the map as a whole:
 * sampling the map blindly leaves whole terrains empty, which on the surface breaks
 * the gateway that terrain owns, and underground leaves a level with nothing on it.
 *
 * Scarcest first is by candidate count, not by area — mountain has fewer usable
 * sites than tundra despite covering more ground. Whoever has the least choice picks
 * while there is still choice to be had.
 *
 * Two stages, differing only in what a failed roll costs. During the first
 * `guaranteed_rounds` a terrain that finds no room simply rolls again next round —
 * a second chance, not a concession, since the distance itself is never lowered.
 * Afterwards a miss retires the terrain for good, and that is what ends the process:
 * as the level fills, high rolls fail more often and terrains fall away one by one.
 *
 * `ceiling` bounds the free stage alone. Cutting a guaranteed round short would cost
 * a terrain its settlements outright, so the guaranteed rounds always run in full
 * and the floor is therefore the number of terrains with candidates.
 *
 * @param candidates eligible hexes grouped by terrain; the caller has already applied
 *                   whatever policy it has (the surface filters by entry requirements,
 *                   the underground by nothing)
 * @param w          map width, for `cylDistance` — the metric the whole generator uses.
 *                   `GetPlanarDistance` would be wrong here as well as slow: on an
 *                   icosahedral world it runs a search rather than arithmetic
 * @param spacing_roll rolled fresh for every placement; nothing ever lowers the result
 * @param guaranteed_rounds rounds in which a miss costs a round rather than the terrain
 * @param ceiling    hard cap on the free stage; 0 = none
 * @see far_enough(), economy()
 */
SettlementPlacement place_settlements_round_robin(
    const std::unordered_map<int, std::vector<ARegion*>>& candidates,
    const int w,
    const std::function<int()>& spacing_roll,
    int guaranteed_rounds,
    int ceiling)
{
    SettlementPlacement out;

    for (const auto& [terrain, hexes] : candidates)
        if (!hexes.empty()) out.order.push_back(terrain);

    std::sort(out.order.begin(), out.order.end(), [&](int a, int b) {
        size_t na = candidates.at(a).size();
        size_t nb = candidates.at(b).size();
        if (na != nb) return na < nb;
        return a < b;   // stable, so a map with ties still generates repeatably
    });

    std::unordered_set<ARegion*> taken;
    std::vector<bool> active(out.order.size(), true);
    int active_terrains = (int) out.order.size();

    auto far_from_chosen = [&](ARegion* reg, int spacing) {
        std::vector<int> dists;
        graphs::Location2D a = { reg->xloc, reg->yloc };
        for (const auto other : out.chosen) {
            graphs::Location2D b = { other->xloc, other->yloc };
            dists.push_back(cylDistance(a, b, w));
        }
        return far_enough(dists, spacing);
    };

    while (active_terrains > 0) {
        bool guaranteed = (out.rounds < guaranteed_rounds);
        out.rounds++;

        for (size_t i = 0; i < out.order.size(); i++) {
            if (!active[i]) continue;

            if (!guaranteed && ceiling > 0 && (int) out.chosen.size() >= ceiling) {
                active[i] = false;
                active_terrains--;
                continue;
            }

            int terrain = out.order[i];
            int spacing = spacing_roll();

            std::vector<ARegion*> eligible;
            for (const auto reg : candidates.at(terrain)) {
                if (taken.count(reg)) continue;
                if (!far_from_chosen(reg, spacing)) continue;
                eligible.push_back(reg);
            }

            if (eligible.empty()) {
                if (!guaranteed) {
                    active[i] = false;
                    active_terrains--;
                }
                continue;
            }

            ARegion* pick = eligible[rng::get_random(eligible.size())];
            out.chosen.push_back(pick);
            taken.insert(pick);
            out.placed_by_terrain[terrain]++;
        }
    }

    return out;
}

/**
 * @brief One-line summary of what a placement pass did, for the generation log.
 */
std::string describe_placement(const SettlementPlacement& p, int ceiling)
{
    std::string line;
    for (int terrain : p.order)
        line += " " + std::string(TerrainDefs[terrain].name) + "=" +
                std::to_string(p.placed_by_terrain.count(terrain)
                               ? p.placed_by_terrain.at(terrain) : 0);
    line += " | total " + std::to_string(p.chosen.size()) +
            ", rounds " + std::to_string(p.rounds);
    if (ceiling > 0) line += ", ceiling " + std::to_string(ceiling);
    return line;
}

int rounded_div(int numerator, int denominator)
{
    if (denominator == 0) return 0;
    if ((numerator < 0) != (denominator < 0))
        return (numerator - denominator / 2) / denominator;
    return (numerator + denominator / 2) / denominator;
}

int percent_rounded(int part, int whole)
{
    return rounded_div(part * 100, whole);
}

void ARegionArray::set_name(const std::string& name)
{
    strName.clear();
    if (!name.empty()) strName = name;
}

ARegionFlatArray::ARegionFlatArray(int s)
{
    size = s;
    regions = new ARegion *[s];
}

ARegionFlatArray::~ARegionFlatArray()
{
    if (regions) delete regions;
}

void ARegionFlatArray::SetRegion(int x, ARegion *r) {
    regions[x] = r;
}

ARegion *ARegionFlatArray::GetRegion(int x) {
    return regions[x];
}

int parse_terrain(const strings::ci_string& token)
{
    for (int i = 0; i < R_NUM; i++) {
        if (token == TerrainDefs[i].type || token == TerrainDefs[i].name) return i;
    }

    return (-1);
}

int mapBiome(int biome) {
    switch (biome) {
        case B_TUNDRA: return R_TUNDRA;
        case B_MOUNTAINS: return R_MOUNTAIN;
        case B_HILLS: return R_HILL;
        case B_SWAMP: return R_SWAMP;
        case B_FOREST: return R_FOREST;
        case B_PLAINS: return R_PLAIN;
        case B_JUNGLE: return R_JUNGLE;
        case B_DESERT: return R_DESERT;
        case B_WATER: return R_OCEAN;
        default: return -1;
    }
}

struct WaterBody {
    int name;
    std::unordered_set<graphs::Location2D> regions;
    std::unordered_set<int> connections;
    bool rivers;

    bool includes(const graphs::Location2D key) {
        return regions.find(key) != regions.end();
    }

    bool includes(ARegion* reg) {
        return regions.find({ reg->xloc, reg->yloc }) != regions.end();
    }

    void add(const graphs::Location2D key) {
        regions.insert(key);
    }

    void add(const ARegion* reg) {
        regions.insert({ reg->xloc, reg->yloc });
    }

    void connect(WaterBody* other) {
        connections.insert(other->name);
    }

    bool connected(WaterBody* other) {
        return connections.find(other->name) != connections.end();
    }
};

int findWaterBody(std::vector<WaterBody*>& waterBodies, ARegion* reg) {
    for (const auto& water : waterBodies) {
        if (water->includes(reg)) {
            return water->name;
        }
    }

    return -1;
}

bool isInnerWater(ARegion* reg) {
    for (int i = 0; i < NDIRS; i++) {
        auto n = reg->neighbors[i];
        if (n && n->type != R_OCEAN) {
            return false;
        }
    }

    return true;
}

bool isNearWater(ARegion* reg) {
    for (int i = 0; i < NDIRS; i++) {
        auto n = reg->neighbors[i];
        if (n && n->type == R_OCEAN) {
            return true;
        }
    }

    return false;
}

bool isNearWaterBody(ARegion* reg, WaterBody* wb) {
    for (int i = 0; i < NDIRS; i++) {
        auto n = reg->neighbors[i];
        if (n && wb->includes(n)) {
            return true;
        }
    }

    return false;
}

bool isNearWaterBody(ARegion* reg, std::vector<WaterBody*>& list) {
    for (const auto& wb : list) {
        if (isNearWaterBody(reg, wb)) {
            return true;
        }
    }

    return false;
}

// makeRivers: Generate rivers connecting separate water bodies (oceans/seas)
// Algorithm:
//   1. Find all water bodies (connected ocean regions) via BFS
//   2. Calculate shortest land distances between all water body pairs
//   3. For each water body, connect to 3-6 closest neighbors with rivers
//   4. Rivers use Dijkstra pathfinding with elevation-based costs
//   5. Rivers alternate R_OCEAN and R_SWAMP terrain every 4 hexes
void makeRivers(
    Map* map, ARegionArray* arr, std::vector<WaterBody*>& waterBodies,
    std::unordered_map<ARegion*, int>& rivers,
    const int w, const int h, const int maxRiverReach
) {
    logger::write("Let's have RIVERS!");

    // Track deep ocean regions (surrounded by water on all sides) - rivers can't start/end here
    std::unordered_set<graphs::Location2D> innerWater;

    // Setup graph to traverse only ocean regions
    ARegionGraph graph = ARegionGraph(arr);
    graph.setInclusion([](ARegion* current, ARegion* next) {
        return next->type == R_OCEAN;
    });

    // === PHASE 1: Identify all separate water bodies (oceans/seas) ===
    logger::write("Find water bodies");

    int waterBodyName = 0;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;  // Skip invalid hex coordinates
            }

            ARegion* reg = arr->GetRegion(x, y);
            if (reg->type != R_OCEAN) {
                continue;  // Only process ocean regions
            }

            graphs::Location2D loc = { reg->xloc, reg->yloc };
            // Track inner water (ocean surrounded by ocean) - can't be river endpoints
            if (isInnerWater(reg)) {
                innerWater.insert(loc);
            }

            // Skip if this ocean already belongs to a water body
            if (findWaterBody(waterBodies, reg) >= 0) {
                continue;
            }

            // BFS to find all connected ocean regions = one water body
            auto result = graphs::breadthFirstSearch(graph, loc);

            WaterBody* wb = new WaterBody();
            wb->name = waterBodyName++;
            waterBodies.push_back(wb);

            // Add all found ocean regions to this water body
            for (const auto& kv : result) {
                wb->add(kv.first);
            }
        }
    }

    // === PHASE 2: Calculate shortest distances between all water body pairs ===
    // This determines which water bodies are close enough to connect with rivers
    logger::write("Distances from water body to water body");

    size_t sz = waterBodies.size();
    int distances[sz][sz];
    // Initialize distance matrix with infinity
    for (size_t i = 0; i < sz; i++) {
        for (size_t j = 0; j < sz; j++) {
            distances[i][j] = INT32_MAX;
        }
    }

    // For each water body, find shortest land path to every other water body
    for (const auto& water : waterBodies) {
        logger::write("WATER BODY " + std::to_string(water->name));

        // Setup graph to traverse land (not this water body, not inner water)
        graph.setInclusion([ water, &innerWater ](ARegion* current, ARegion* next) {
            graphs::Location2D loc = { next->xloc, next->yloc };
            return !water->includes(loc) && innerWater.find(loc) == innerWater.end();
        });

        // BFS from each coastal point of this water body
        for (const auto& loc : water->regions) {
            if (innerWater.find(loc) != innerWater.end()) {
                continue;  // Skip deep ocean regions - can't start rivers there
            }

            auto result = graphs::breadthFirstSearch(graph, loc);
            for (const auto& kv : result) {
                int newDist = kv.second.distance + 1;

                // Check if we reached another water body
                int otherWater = findWaterBody(waterBodies, graph.get(kv.first));
                if (otherWater < 0) {
                    continue;  // Not at water body yet
                }

                // Update distance if we found a shorter path
                int currentDist = distances[water->name][otherWater];
                if (newDist < currentDist ) {
                    distances[water->name][otherWater] = newDist;
                }
            }
        }
    }

    // === PHASE 3: Setup pathfinding costs for river placement ===
    // Rivers prefer: low elevation, existing rivers, avoiding coastlines
    logger::write("Max river reach " + std::to_string(maxRiverReach));

    graph.setCost([ map, &rivers ](ARegion* current, ARegion* next) {
        // Base cost = elevation (higher terrain costs more to cross)
        int cost = std::max(1, map->map.get(next->xloc * 2, next->yloc * 2)->elevation);

        // Rivers prefer to follow existing rivers (cost / 10)
        if (rivers.find(next) != rivers.end()) {
            cost = cost / 10;
        }
        // Rivers avoid coastlines (cost * 10)
        else if (isNearWater(next)) {
            cost = cost * 10;
        }

        return cost;
    });

    // === PHASE 4: Create rivers between close water bodies ===
    int riverName = 0;
    for (size_t i = 0; i < sz; i++) {
        logger::write("Connecting water body " + std::to_string(i));

        // Find all water bodies within maxRiverReach distance
        std::vector<std::pair<int, int>> candidates;
        WaterBody* source = waterBodies[i];

        for (size_t j = 0; j < sz; j++) {
            if (i == j) {
                continue;  // Can't connect to self
            }

            int distance = distances[i][j];
            if (distance <= maxRiverReach) {
                candidates.push_back(std::make_pair(j, distance));
            }
        }

        // Each water body connects to 3-6 random neighbors (or all if fewer)
        int numConnections = std::min(rng::make_roll(3, 4), (int) candidates.size());
        logger::write("There will be " + std::to_string(numConnections) + " rivers");

        if (numConnections > 0) {
            rng::shuffle(candidates);  // Randomize which neighbors to connect

            for (int ci = 0; ci < numConnections; ci++) {
                WaterBody* target = waterBodies[candidates[ci].first];
                logger::write("Planing river to " + std::to_string(target->name));

                if (source->connected(target)) {
                    logger::write("Already connected, moving to next target");
                    continue;  // Don't create duplicate rivers
                }

                // Setup pathfinding rules for this river
                graph.setInclusion([ source, target, &rivers ](ARegion* current, ARegion* next) {
                    if (source->includes(next)) {
                        return false;  // Can't go through source water body
                    }

                    if (target->includes(current) && target->includes(next)) {
                        return false;  // Can touch target edge but not go through it
                    }

                    if (rivers.find(next) != rivers.end()) {
                        return true;  // Rivers can cross each other
                    }

                    if (!target->includes(next) && next->type == R_OCEAN) {
                        return false;  // Can't go through other water bodies
                    }

                    return true;  // Land is traversable
                });

                // Find optimal river path from source to target
                graphs::Location2D riverStart;
                graphs::Location2D riverEnd;
                int riverCost = INT32_MAX;

                // Try all coastal points of source water body
                for (const auto& start : source->regions) {
                    if (innerWater.find(start) != innerWater.end()) {
                        continue;  // Skip deep ocean - can't start river there
                    }

                    // Dijkstra pathfinding from this start point
                    std::unordered_map<graphs::Location2D, graphs::Location2D> cameFrom;
                    std::unordered_map<graphs::Location2D, double> costSoFar;
                    graphs::dijkstraSearch(graph, start, cameFrom, costSoFar);

                    // Find cheapest path to any point in target water body
                    int smallestCost = INT32_MAX;
                    graphs::Location2D end;
                    for(const auto loc : target->regions) {
                        int cost = costSoFar[loc];
                        if (cost > 0 && cost < smallestCost) {
                            end = loc;
                            smallestCost = cost;
                        }
                    }

                    if (smallestCost == INT32_MAX) {
                        continue;  // No path from this start point
                    }

                    // Update best river path if this is cheaper
                    if (smallestCost < riverCost) {
                        riverStart = start;
                        riverEnd = end;
                        riverCost = smallestCost;
                    }
                }

                if (riverCost == INT32_MAX) {
                    logger::write("No path to " + std::to_string(target->name) + " found");
                    continue;  // No valid river path found
                }

                // Reconstruct optimal path using Dijkstra
                std::unordered_map<graphs::Location2D, graphs::Location2D> cameFrom;
                std::unordered_map<graphs::Location2D, double> costSoFar;
                graphs::dijkstraSearch(graph, riverStart, riverEnd, cameFrom, costSoFar);

                // Build path from end to start
                std::vector<ARegion*> path;
                graphs::Location2D current = riverEnd;
                while (current != riverStart) {
                    ARegion* reg = graph.get(current);
                    path.push_back(reg);
                    current = cameFrom[current];
                }

                int riverLen = path.size();
                logger::write("River length is " + std::to_string(riverLen));

                // Check if entire river path is on extreme parallels - skip if so
                bool entirelyOnExtremeParallels = true;
                for (auto reg : path) {
                    if (reg->yloc > 1 && reg->yloc < h - 2) {
                        entirelyOnExtremeParallels = false;
                        break;
                    }
                }

                if (entirelyOnExtremeParallels) {
                    logger::write("Skipping river - entire path on extreme parallels");
                    continue;
                }

                // Mark water bodies as connected (only if river will be created)
                source->connect(target);
                target->connect(source);

                // Place river: alternate R_OCEAN and R_SWAMP (adaptive segmentation)
                bool first = true;
                int counter = 0;
                // Adaptive segment length: shorter rivers have more swamps (%)
                int segmentLen = std::min(riverLen / 2, 6);
                if (segmentLen < 3) segmentLen = 3;
                if (segmentLen > riverLen - 1) segmentLen = riverLen - 1;
                logger::write("River segment length is " + std::to_string(segmentLen));

                for (auto reg : path) {
                    // Skip river hexes on extreme north/south parallels (y=0,1 or y=h-2,h-1)
                    if (reg->yloc <= 1 || reg->yloc >= h - 2) {
                        logger::write("Skipping river hex at extreme parallel y=" + std::to_string(reg->yloc));
                        continue;
                    }

                    if (rivers.find(reg) != rivers.end()) {
                        // Crossing existing river - start new river segment
                        riverName++;
                        counter = 1;
                        first = false;
                        continue;
                    }
                    else {
                        rivers.insert(std::make_pair(reg, riverName));
                    }

                    if (first) {
                        // First hex: R_SWAMP if very short river, else R_OCEAN
                        reg->type = path.size() == 1 ? R_SWAMP : R_OCEAN;
                        first = false;
                        continue;
                    }

                    // Alternate R_SWAMP every 4 hexes, otherwise R_OCEAN
                    reg->type = (counter % segmentLen) == 0 ? R_SWAMP : R_OCEAN;
                    counter++;
                }

                riverName++;  // Next river gets new ID
            }
        }
    }
}

void cleanupIsolatedPlaces(
    ARegionArray* arr, std::vector<WaterBody*>& waterBodies,
    std::unordered_map<ARegion*, int>& rivers, int w, int h
) {
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;
            }

            ARegion* reg = arr->GetRegion(x, y);
            if (reg->type == R_OCEAN) {
                continue;
            }

            std::vector<WaterBody*> wb;
            std::vector<int> nearbyRivers;
            bool clear = true;
            for (int i = 0; i < NDIRS; i++) {
                auto next = reg->neighbors[i];
                if (next == NULL) {
                    continue;
                }

                if (next->type != R_OCEAN) {
                    clear = false;
                    break;
                }

                int name = findWaterBody(waterBodies, next);
                if (name >= 0) {
                    wb.push_back(waterBodies[name]);
                }

                auto r = rivers.find(next);
                if (r != rivers.end()) {
                    nearbyRivers.push_back(r->second);
                }
            }

            if (clear) {
                reg->type = R_OCEAN;

                if (!wb.empty()) {
                    wb[rng::get_random(wb.size())]->add(reg);
                }
                else  {
                    rivers.insert(std::make_pair(reg, nearbyRivers[rng::get_random(nearbyRivers.size())]));
                }
            }
        }
    }
}

int countNeighbors(ARegionGraph& graph, ARegion* reg, int ofType, int distance) {
    graphs::Location2D loc = { reg->xloc, reg->yloc };

    int count = 0;

    auto result = graphs::breadthFirstSearch(graph, loc);
    for (auto kv : result) {
        int d = kv.second.distance + 1;
        if (d > distance) {
            continue;
        }

        ARegion* r = graph.get(kv.first);
        if (r->type == ofType) {
            count++;
        }
    }

    return count;
}

void placeVolcanoes(ARegionArray* arr, const int w, const int h) {
    ARegionGraph graph = ARegionGraph(arr);

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;
            }

            ARegion* reg = arr->GetRegion(x, y);
            if (reg->type != R_MOUNTAIN) {
                continue;
            }

            int mountains = countNeighbors(graph, reg, R_MOUNTAIN, rng::make_roll(1, 3) + 1);
            int volcanoes = countNeighbors(graph, reg, R_VOLCANO, 2);

            if (volcanoes == 0 && mountains >= (rng::make_roll(2, 3) + 2)) {
                reg->type = R_VOLCANO;
            }
        }
    }
}

void ARegionList::PlaceVolcanos(ARegionArray *arr) {
    placeVolcanoes(arr, arr->x, arr->y);
}

// Forward declaration for distance function
int distance(graphs::Location2D a, graphs::Location2D b);

void placeLakes(ARegionArray* arr, const int w, const int h, double lakePercent) {
    logger::write("Placing lakes");

    // Create graph for neighbor counting
    ARegionGraph graph = ARegionGraph(arr);

    // Convert lakePercent (0.0-1.0) to percentage (0-100)
    int lakeChance = (int)(lakePercent * 100);

    // Iterate through all regions in map order
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            // Only even coordinates (hex grid structure)
            if ((x + y) % 2) continue;

            ARegion* reg = arr->GetRegion(x, y);
            if (!reg) continue;

            // Skip unsuitable terrain types
            if (reg->type == R_OCEAN || reg->type == R_MOUNTAIN ||
                reg->type == R_VOLCANO || reg->type == R_LAKE) {
                continue;
            }

            // Lakes must be inland (no ocean neighbors)
            int oceanNeighbors = countNeighbors(graph, reg, R_OCEAN, 1);
            if (oceanNeighbors > 0) continue;

            // Random distance constraints for this region
            int minLakeDistance = rng::make_roll(2, 3) + 1; // 3-7 hexes
            int minVolcanoDistance = rng::make_roll(1, 2) + 1;  // 2-3 hexes

            // Check for nearby lakes
            int lakesNearby = countNeighbors(graph, reg, R_LAKE, minLakeDistance);
            if (lakesNearby > 0) continue;

            // Check for nearby volcanoes
            int volcanoesNearby = countNeighbors(graph, reg, R_VOLCANO, minVolcanoDistance);
            if (volcanoesNearby > 0) continue;

            // Random chance (configurable probability)
            if (rng::make_roll(1, 100) > lakeChance) continue;

            // Place the lake
            reg->type = R_LAKE;
            reg->wages = AGetName(0, reg);
            logger::dot();
        }
    }

    logger::write("");
}

int distance(graphs::Location2D a, graphs::Location2D b) {
    int dX = std::abs(a.x - b.x);
    int dY = std::abs(a.y - b.y);

    return dX + std::max(0, (dY - dX) / 2);
}

int cylDistance(graphs::Location2D a, graphs::Location2D b, int w) {
    int d0 = distance(a, b);
    int d1 = distance(a, { b.x + w, b.y });
    int d2 = distance(a, { b.x - w, b.y });

    return std::min(d0, std::min(d1, d2));
}

// Bridson-style Poisson disk sampling on a 2D grid. Returns accepted points.
//
// initialSeeds controls how many independent starting anchors kick off the
// sampling frontier. Default 1 preserves legacy behavior; larger values spread
// seeds on a sqrt(N) x sqrt(N) grid (with random jitter inside each cell), so
// the sampling wave reaches all corners of the map even when minDist is large
// and a single-seed frontier would exhaust before covering the periphery.
std::vector<graphs::Location2D> getPoints(const int w, const int h,
    const int initialMinDist, const int newPointCount,
    const std::function<int(graphs::Location2D)> onPoint,
    const std::function<bool(graphs::Location2D)> onIsIncluded,
    const int initialSeeds = 1) {

    std::vector<graphs::Location2D> output;
    std::vector<graphs::Location2D> processing;

    int minDist = initialMinDist;
    int cellSize = ceil(minDist / sqrt(2));

    // Seed the frontier. initialSeeds >= 1; for N > 1 we spread seeds on a
    // gridSide x gridSide grid (gridSide = ceil(sqrt(N))) with random jitter
    // inside each cell. Seeds themselves are NOT placed as settlements — they
    // only act as anchors for the sampling expansion (same as legacy behavior
    // with a single seed).
    int seeds = std::max(1, initialSeeds);
    int gridSide = (int) ceil(sqrt((double) seeds));
    int cell_w = std::max(1, w / gridSide);
    int cell_h = std::max(1, h / gridSide);

    int placed = 0;
    for (int gy = 0; gy < gridSide && placed < seeds; gy++) {
        for (int gx = 0; gx < gridSide && placed < seeds; gx++) {
            graphs::Location2D loc;
            int tries = 0;
            do {
                int x = gx * cell_w + rng::get_random(cell_w);
                int y = gy * cell_h + rng::get_random(cell_h);
                if (x >= w) x = w - 1;
                if (y >= h) y = h - 1;
                loc = { .x = x, .y = y };
                tries++;
            } while (!onIsIncluded(loc) && tries < 64);
            if (tries >= 64) continue;  // gridcell unusable, skip

            output.push_back(loc);
            processing.push_back(loc);
            placed++;
        }
    }

    // Fallback: if no seed survived (pathological case), fall back to a random
    // valid point — mirrors legacy single-seed behavior.
    if (output.empty()) {
        graphs::Location2D loc;
        do {
            loc = { .x = rng::get_random(w), .y = rng::get_random(h) };
        } while (!onIsIncluded(loc));
        output.push_back(loc);
        processing.push_back(loc);
    }

    while (!processing.empty()) {
        int i = rng::get_random(processing.size());
        auto next = processing.at(i);
        processing.erase(processing.begin() + i);

        int count = 0;
        while (count < newPointCount) {
            int r = rng::get_random(minDist) + minDist + 1;
            double a = rng::get_random(360) / 360.0 * 2 * M_PI;

            int x = ceil(next.x + r * cos(a));
            int y = ceil(next.y + r * sin(a));

            if (x > w) { x = x % w; }
            if (x < 0) { x += w; }

            if (y > h) { y = y % h; }
            if (y < 0) { y += h; }

            if ((x + y) % 2) {
                continue;
            }

            // a new point to check
            count++;
            graphs::Location2D candidate = { x, y };

            if (!onIsIncluded(candidate)) {
                // out of bounds
                continue;
            }

            int gridX = x / cellSize;
            int gridY = y / cellSize;

            // check against all valid points
            bool pointValid = true;
            for (auto p : output) {
                int d = cylDistance(candidate, p, w);
                if (d < minDist) {
                    // too close
                    pointValid = false;
                    break;
                }

                int gpX = p.x / cellSize;
                int gpY = p.y / cellSize;
                if (gridX == gpX && gridY == gpY) {
                    pointValid = false;
                    break;
                }
            }

            if (!pointValid) {
                continue;
            }

            minDist = onPoint(candidate);
            cellSize = ceil(minDist / sqrt(2));

            output.push_back(candidate);
            processing.push_back(candidate);
        }
    }

    return output;
}

std::vector<graphs::Location2D> getPointsFromList(
    const int width,
    const int minDist,
    const int newPointCount,
    const std::vector<graphs::Location2D>& points) {

    std::vector<graphs::Location2D> output;
    std::vector<graphs::Location2D> processing;

    graphs::Location2D loc = points.at(rng::get_random(points.size()));

    output.push_back(loc);
    processing.push_back(loc);

    while (!processing.empty()) {
        int i = rng::get_random(processing.size());
        processing.erase(processing.begin() + i);

        int count = 0;
        while (count < newPointCount) {
            // a new point to check
            graphs::Location2D candidate = points.at(rng::get_random(points.size()));
            count++;

            // check against all valid points
            bool pointValid = true;
            for (auto p : output) {
                int d = cylDistance(candidate, p, width);
                if (d < minDist) {
                    // too close
                    pointValid = false;
                    break;
                }
            }

            if (!pointValid) {
                continue;
            }

            output.push_back(candidate);
            processing.push_back(candidate);
        }
    }

    return output;
}

struct NameArea {
    int name;
    graphs::Location2D center;
    std::unordered_map<int, int> usage;

    int distance(const int w, ARegion* reg) {
        return cylDistance(center, { reg->xloc, reg->yloc }, w);
    }

    int getName(int type) {
        int u = usage[type];
        usage[type]++;

        return name + u;
    }
};

NameArea* getNearestNameArea(std::vector<NameArea*>& nameAnchors, const int w, ARegion* reg) {
    NameArea* na = NULL;
    int distance = INT32_MAX;

    for (auto a : nameAnchors) {
        int d = a->distance(w, reg);
        if (distance > d) {
            distance = d;
            na = a;
        }
    }

    return na;
}

struct River {
    std::string name;
    int length;
    int nameArea;
};

Ethnicity getRegionEtnos(ARegion* reg) {
    Ethnicity etnos = Ethnicity::NONE;
    if (reg->race > 0) {
        auto man = find_race(ItemDefs[reg->race].abr)->get();
        etnos = man.ethnicity;
    }

    return etnos;
}

void subdivideArea(
    const int width, const int height, const int distance,
    const std::vector<graphs::Location2D> &regions, std::vector<std::vector<graphs::Location2D>> &subgraphs
) {
    auto points = getPointsFromList(width, distance, 8, regions);

    std::unordered_map<graphs::Location2D, std::vector<graphs::Location2D>> centers;
    for (auto &p : points) {
        centers[p] = { };
        logger::write("{ x: " + std::to_string(p.x) + ", y: " + std::to_string(p.y) + " }");
    }

    for (auto &reg : regions) {
        graphs::Location2D loc;
        int dist = -1;

        for (auto &kv : centers) {
            int d = cylDistance(kv.first, reg, width);

            if (dist == -1 || d < dist) {
                loc = kv.first;
                dist = d;
            }
        }

        centers[loc].push_back(reg);
    }

    for (auto &kv : centers) {
        subgraphs.push_back(kv.second);
    }
}

void nameArea(
    int width, int height, ARegionGraph &graph, std::unordered_set<std::string> &usedNames,
    std::vector<NameArea*>& nameAnchors, std::vector<graphs::Location2D> &regions, std::unordered_set<ARegion*> &named
) {
    std::string name;
    Ethnicity etnos = Ethnicity::NONE;

    while (name.empty()) {
        for (auto &loc : regions) {
            if (rng::get_random(100) != 99) {
                continue;
            }

            auto r = graph.get(loc);
            etnos = getRegionEtnos(r);

            int type = r->type == R_MOUNTAIN || r->type == R_VOLCANO
                ? R_MOUNTAIN
                : r->type;

            name = getRegionName(etnos, type, regions.size(), false);
            while (usedNames.find(name) != usedNames.end()) {
                logger::write("Searching for better name");
                name = getRegionName(etnos, type, regions.size(), false);
            }
            usedNames.emplace(name);

            logger::write(name);

            break;
        }
    }

    for (auto &loc : regions) {
        auto r = graph.get(loc);

        if (r->type == R_VOLCANO) {
            std::string volcanoName = getRegionName(etnos, r->type, 1, false);
            while (usedNames.find(volcanoName) != usedNames.end()) {
                logger::write("Searching for better name");
                volcanoName = getRegionName(etnos, r->type, 1, false);
            }
            usedNames.emplace(volcanoName);

            logger::write(volcanoName);
            r->set_name(volcanoName);
        }
        else {
            r->set_name(name);
        }

        named.emplace(r);
    }
}

void giveNames(
    ARegionArray* arr, std::vector<WaterBody*>& waterBodies, std::unordered_map<ARegion*, int>& rivers,
    const int w, const int h
) {
    std::unordered_set<ARegion*> named;
    std::unordered_set<std::string> usedNames;

    // generate name areas
    std::vector<NameArea*> nameAnchors;
    std::unordered_set<int> usedNameSeeds;
    auto onPoint = [](graphs::Location2D p) { return 8; };
    auto onIsIncluded = [](graphs::Location2D p) { return true; };
    for (auto p : getPoints(w, h, 8, 16, onPoint, onIsIncluded)) {
        int seed;
        do {
            seed = rng::get_random(w * h) + 1;
        }
        while (usedNameSeeds.find(seed) != usedNameSeeds.end());
        usedNameSeeds.emplace(seed);

        NameArea* na = new NameArea();
        na->center = p;
        na->name = seed;

        nameAnchors.push_back(na);
    }

    // name rivers
    int minLen = 1;
    int maxLen = 0;

    std::unordered_map<int, River> riverNames;
    for (auto &kv : rivers) {
        River& river = riverNames[kv.second];
        river.length++;

        if (river.nameArea == 0) {
            auto na = getNearestNameArea(nameAnchors, w, kv.first);
            river.nameArea = na->getName(R_NUM);
        }
    }

    for (auto &kv : riverNames) {
        minLen = std::min(minLen, kv.second.length);
        maxLen = std::max(maxLen, kv.second.length);
    }

    for (auto &kv : riverNames) {
        River& river = kv.second;

        std::string name = getRiverName(river.length, minLen, maxLen);
        while (usedNames.find(name) != usedNames.end()) {
            logger::write("Searching for better name");
            name = getRiverName(river.length, minLen, maxLen);
        }
        usedNames.emplace(name);

        river.name = name;
        logger::write(river.name);
    }

    for (auto &kv : rivers) {
        River& river = riverNames[kv.second];
        kv.first->set_name(river.name);
        named.insert(kv.first);
    }

    // name water bodies
    for (auto wb : waterBodies) {
        std::string name;

        for (auto loc : wb->regions) {
            ARegion* reg = arr->GetRegion(loc.x, loc.y);

            if (name.empty()) {
                Ethnicity etnos = getRegionEtnos(reg);

                name = getRegionName(etnos, reg->type, wb->regions.size(), false);
                while (usedNames.find(name) != usedNames.end()) {
                    logger::write("Searching for better name");
                    name = getRegionName(etnos, reg->type, wb->regions.size(), false);
                }
                usedNames.emplace(name);

                logger::write(name);
            }

            reg->set_name(name);
            named.insert(reg);
        }
    }

    // name other regions
    ARegionGraph graph = ARegionGraph(arr);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;
            }

            graphs::Location2D loc = { x, y };
            ARegion* reg = graph.get(loc);

            if (named.find(reg) != named.end()) {
                continue;
            }

            graph.setInclusion([ reg ](ARegion* current, ARegion* next) {
                if (reg->type == R_MOUNTAIN || reg->type == R_VOLCANO) {
                    return next->type == R_MOUNTAIN || next->type == R_VOLCANO;
                }

                return next->type == reg->type;
            });

            std::string name;
            auto area = graphs::breadthFirstSearch(graph, loc);

            if (area.size() > 16) {
                logger::write("Large area: " + std::to_string(area.size()) + " regions");

                std::vector<graphs::Location2D> locations;
                for (auto &kv : area) {
                    locations.push_back(kv.first);
                }

                // the are is too big, we must split it
                std::vector<std::vector<graphs::Location2D>> subgraphs;
                subdivideArea(w, h, std::clamp(4, (int) sqrt(area.size()), 8), locations, subgraphs);

                logger::write("  Subdivided into " + std::to_string(subgraphs.size()) + " subgraphs");

                for (auto &regions : subgraphs) {
                    logger::write("    Subgraph with " + std::to_string(regions.size()) + " regions");
                    nameArea(w, h, graph, usedNames, nameAnchors, regions, named);
                }
            }
            else {
                std::vector<graphs::Location2D> regions;
                for (auto &loc : area) {
                    regions.push_back(loc.first);
                }

                nameArea(w, h, graph, usedNames, nameAnchors, regions, named);
            }
        }
    }
}

/**
 * @brief Places every surface settlement, in four phases
 *
 * Phase 0 rolls products and population for the whole surface. Nothing can be
 * decided before that: the starting-location check reads the products of hexes up
 * to three moves away, and until a hex has been through setup_terrain() it has
 * none. Reading the terrain tables instead is not an option - food is a per-region
 * roll between grain, livestock and fish, and mounts carry a chance below 100.
 *
 * Phase 1 collects the hexes that satisfy the gateway entry requirements. Every
 * settlement must be a viable start, not only the ones a player happens to land in,
 * because the entry ladder falls back to any village. About 86% of land qualifies.
 *
 * Phase 2 places settlements by walking the terrains round-robin, scarcest first,
 * rolling a fresh [4..6] spacing for each one and picking at random among the hexes
 * that clear it. Terrain drives the loop because the gateway system is organised by
 * terrain - one gateway each, and a terrain with no village makes its gateway
 * meaningless. Sampling the map as a whole cannot promise that: measured over ten
 * 48x48 seeds it left 1.6 of the 8 terrains empty. This replaces the former Poisson
 * pass, which had become vestigial once a terrain quota ran ahead of it.
 *
 * Phase 3 turns the chosen sites into towns and finishes every region, after which
 * a share of the villages is upgraded to cities and the trade goods are dealt out.
 *
 * @param arr the surface level
 * @return false if a gateway terrain ended with too few villages to be a usable
 *         start, meaning the caller should discard this world and generate another
 * @see ARegion::setup_terrain(), ARegion::finish_setup(), far_enough()
 */
bool economy(ARegionArray* arr, const int w, const int h) {
    std::unordered_map<int, int> histogram;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;
            }

            ARegion* reg = arr->GetRegion(x, y);
            histogram[reg->type]++;
        }
    }

    logger::write("Setting settlements");

    // -----------------------------------------------------------------------
    // Tunable constants for surface settlement generation.
    // Kept local (not GameDefs) to keep the generation policy next to the code
    // that applies it — change here when tuning density or city ratio.
    // -----------------------------------------------------------------------

    // Spacing between any two settlements, rolled fresh for every placement:
    // 2d2+2 -> [4..6], peaking at 5. The spread is what keeps the map from looking
    // laid out on a grid.
    //
    // Nothing anywhere lowers this roll. Four is the floor on purpose: at 3 a
    // single player can sit between two settlements and take both in one move,
    // and 3 is also the closest a player may found one (CREATE VILLAGE, see
    // monthorders.cpp). Rolling [3..6] instead was measured and rejected - a 3
    // lands somewhere almost every time, so terrains never retired and 48x48 ran
    // to 29-43 settlements against a baseline of 19-24.
    auto settlement_spacing = []() { return rng::make_roll(2, 2) + 2; };

    // Rounds during which a terrain that rolls badly gets another chance next
    // round instead of retiring. Four of them aim for four settlements per gateway
    // terrain, which is what leaves SETTLEMENTS_KEPT standing once the city upgrade
    // has taken its share. Raise for a denser world, lower for a sparser one.
    constexpr int    GUARANTEED_ROUNDS = 4;

    // Entry-capable settlements a terrain must end with. One lone site is a
    // bottleneck rather than a start: every player choosing that terrain arrives in
    // the same hex. Enforced twice, by the city upgrade below and by the verdict at
    // the end, both through gateway_ok() so the two cannot drift apart.
    constexpr int    SETTLEMENTS_KEPT  = 3;

    // Size each placed settlement starts at, and what counts as an entry point.
    //
    // VILLAGES_ONLY is the ruleset's switch for "world generation makes villages
    // only". It is honoured here, and the two rules above follow it rather than
    // assuming villages:
    //
    //   set   -> place villages; a gateway terrain needs SETTLEMENTS_KEPT villages
    //   clear -> place mixed sizes; any settlement of the terrain counts
    //
    // Both readings are defensible against the entry ladder, which is tiered: its
    // phases 1-3 accept villages only, but phase 4 takes towns and cities
    // (monthorders.cpp, "Phase 4: TOWN or CITY"). What the villages buy is start
    // QUALITY rather than a working start - phases 1-3 apply the resource filter and
    // want the hex empty or nearly so, while phase 4 has no filter and tolerates up
    // to three players. So with VILLAGES_ONLY clear the world still plays; players
    // simply land in busier, unvetted places more often.
    auto settlement_size = []() {
        return Globals->VILLAGES_ONLY ? TOWN_VILLAGE : rng::get_random(NTOWNS);
    };
    auto gateway_ok = [](ARegion* reg) {
        if (!reg->town) return false;
        return !Globals->VILLAGES_ONLY || reg->town->TownType() == TOWN_VILLAGE;
    };

    // Fraction of villages to upgrade to cities after initial placement.
    // 0.12 sits in the middle of the requested 10-15% band; adjust here
    // to rebalance the "village-heavy vs city-heavy" feel of a map.
    constexpr double CITY_FRACTION    = 0.12;
    // Floor so tiny maps still get a couple of cities.
    constexpr int    CITY_MIN_COUNT   = 2;
    // Bounds on the auto-derived minimum city-to-city distance. Lower floor
    // guards against mass-placement on tiny maps; upper ceiling keeps cities
    // reachable on huge maps.
    constexpr int    CITY_DIST_MIN    = 8;
    constexpr int    CITY_DIST_MAX    = 20;

    // Track villages created in this pass so we can later pick a spread-out
    // subset and upgrade them to cities.
    std::vector<ARegion*> village_list;

    // -----------------------------------------------------------------------
    // PHASE 0. Roll products and population for the whole surface.
    //
    // start_requirements_at() reads the products of hexes up to three moves out,
    // so nothing can be judged until every hex has been through setup_terrain().
    // Settlement and non-settlement hexes were always set up identically here
    // (habitat = terrain->pop + 1, prodWeight = 1), which is what makes this a
    // split rather than a behaviour change.
    // -----------------------------------------------------------------------
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) continue;
            ARegion* reg = arr->GetRegion(x, y);
            if (!reg) continue;

            TerrainType* terrain = &(TerrainDefs[reg->type]);
            reg->setup_terrain({
                .terrain = terrain,
                .habitat = terrain->pop + 1,
                .prodWeight = 1,
                .addLair = false,
                .addSettlement = false,
                .settlementName = std::string(),
                .settlementSize = 0
            });
        }
    }

    // Sites picked in phase 2, and the size each is to become. Nothing is a town
    // yet - phase 3 does that, once every site is known.
    std::unordered_map<ARegion*, int> chosen_size;

    // -----------------------------------------------------------------------
    // PHASE 1. Which hexes could hold a settlement at all.
    //
    // Every settlement has to be a viable entry point, not only the ones a player
    // happens to land in: the gateway ladder falls back to any village, and one
    // without wood or iron within reach is a dead start. So the requirement check
    // is applied here, once, and everything downstream draws from what it leaves.
    // Roughly 86% of land passes - the check reaches three moves - so this is a
    // filter rather than a bottleneck.
    // -----------------------------------------------------------------------
    std::unordered_map<int, std::vector<ARegion*>> suitable;
    int land_scanned = 0, land_suitable = 0;

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) continue;
            ARegion* reg = arr->GetRegion(x, y);
            if (!reg) continue;
            // R_PLAIN..R_TUNDRA is exactly the settleable surface land; ocean,
            // lake and volcano fall outside it.
            if (reg->type < R_PLAIN || reg->type > R_TUNDRA) continue;
            if (TerrainDefs[reg->type].flags & TerrainType::BARREN) continue;

            land_scanned++;
            if (!arr->start_requirements_at(reg).all()) continue;

            land_suitable++;
            suitable[reg->type].push_back(reg);
        }
    }

    logger::write("Settlement sites: " + std::to_string(land_suitable) + " of " +
                  std::to_string(land_scanned) + " land hexes meet the entry requirements");

    // -----------------------------------------------------------------------
    // PHASE 2. Round-robin placement, scarcest terrain first.
    //
    // There is one gateway per terrain R_PLAIN..R_TUNDRA and the entry ladder only
    // ever lands a player in a village, so a terrain with no village makes its
    // gateway meaningless. Sampling the map as a whole cannot promise that -
    // measured over ten 48x48 seeds it left 1.6 of the 8 terrains empty - so the
    // terrain is what the loop iterates, and each gets a village in the first round.
    //
    // The rules of the walk itself live with place_settlements_round_robin()
    // (this file); what the surface contributes is the candidate set above, the
    // spacing roll and the ceiling.
    // -----------------------------------------------------------------------
    const int cap = Globals->MAX_SURFACE_SETTLEMENTS;   // 0 = none

    SettlementPlacement placement = place_settlements_round_robin(
        suitable, w, settlement_spacing, GUARANTEED_ROUNDS, cap);

    {
        // The walk's own ordering, so the log cannot drift from what it did.
        std::string line = "Placement order (scarcest first):";
        for (int terrain : placement.order)
            line += " " + std::string(TerrainDefs[terrain].name) + "=" +
                    std::to_string(suitable[terrain].size());
        logger::write(line);
    }

    for (const auto reg : placement.chosen) chosen_size[reg] = settlement_size();

    logger::write("Settlements placed:" + describe_placement(placement, cap) +
                  " (" + std::to_string(GUARANTEED_ROUNDS) + " guaranteed)");
    // -----------------------------------------------------------------------
    // PHASE 3. Turn the chosen sites into towns, then finish every region.
    // -----------------------------------------------------------------------
    logger::write("Setting up regions");

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) continue;
            ARegion* reg = arr->GetRegion(x, y);
            if (!reg) continue;

            TerrainType* terrain = &(TerrainDefs[reg->type]);
            auto it = chosen_size.find(reg);
            bool is_settlement = (it != chosen_size.end());

            if (is_settlement) {
                int town_size = it->second;
                Ethnicity etnos = getRegionEtnos(reg);
                std::string name = getEthnicName(etnos);

                reg->add_town(town_size, name);
                if (town_size == TOWN_VILLAGE) village_list.push_back(reg);

                std::string sizeName = town_size == TOWN_VILLAGE ? "Village"
                                     : town_size == TOWN_TOWN    ? "Town" : "City";
                logger::write(sizeName + " " + name);
            }

            // A lair inside a settlement would be removed by add_town anyway.
            bool addLair = !is_settlement && rng::get_random(100) < terrain->lairChance;

            reg->finish_setup({
                .terrain = terrain,
                .habitat = terrain->pop + 1,
                .prodWeight = 1,
                .addLair = addLair,
                .addSettlement = false,
                .settlementName = std::string(),
                .settlementSize = 0
            });
        }
    }

    // -----------------------------------------------------------------------
    // City upgrade phase.
    // Pick ~CITY_FRACTION of the villages and upgrade them to TOWN_CITY using
    // a Poisson-disc re-sample over village positions: iterate villages in
    // shuffled order, accept a candidate only if it is at least
    // `minCityDist` hex-steps away from every already-accepted city.
    //
    // minCityDist is derived as sqrt(land_area / K) and clamped to
    // [CITY_DIST_MIN, CITY_DIST_MAX], giving an even per-area city density
    // that scales across map sizes without a hardcoded magic number.
    //
    // If the initial pass fails to hit K (e.g. villages happened to cluster),
    // the distance is relaxed and the rejected villages are re-tried. In the
    // worst case we emit fewer than K cities — a miss is preferable to
    // breaking the "no two cities adjacent" guarantee.
    // -----------------------------------------------------------------------
    if (!village_list.empty()) {
        int land_hexes = 0;
        for (auto& [terrain_type, count] : histogram) {
            if (terrain_type < 0 || terrain_type >= (int)TerrainDefs.size()) continue;
            int st = TerrainDefs[terrain_type].similar_type;
            if (st == R_OCEAN) continue;
            if (terrain_type == R_LAKE || terrain_type == R_VOLCANO) continue;
            if (TerrainDefs[terrain_type].flags & TerrainType::BARREN) continue;
            land_hexes += count;
        }
        if (land_hexes < 1) land_hexes = 1;

        int target_cities = std::max(
            CITY_MIN_COUNT,
            (int) std::round(village_list.size() * CITY_FRACTION));
        if (target_cities > (int) village_list.size()) target_cities = village_list.size();

        int minCityDist = std::clamp(
            (int) std::sqrt((double) land_hexes / target_cities),
            CITY_DIST_MIN, CITY_DIST_MAX);

        // Fisher-Yates shuffle of village indices (std::shuffle would require
        // wiring rng::get_random into a URBG — simpler to swap in place).
        std::vector<ARegion*> shuffled = village_list;
        for (int i = (int) shuffled.size() - 1; i > 0; i--) {
            int j = rng::get_random(i + 1);
            std::swap(shuffled[i], shuffled[j]);
        }

        // Entry points remaining per terrain. Promoting one away would undo the
        // placement rounds and leave that gateway crowded, so cities come only from
        // terrains with more than SETTLEMENTS_KEPT to spare - in practice the
        // plentiful ones, which is also where a city belongs.
        //
        // Counted through gateway_ok, so with VILLAGES_ONLY clear a promotion costs
        // the terrain nothing (a city is an entry point too) and this guard stands
        // down of its own accord.
        std::unordered_map<int, int> entries_left;
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                if ((x + y) % 2) continue;
                ARegion* reg = arr->GetRegion(x, y);
                if (reg && gateway_ok(reg)) entries_left[reg->type]++;
            }
        }

        std::vector<ARegion*> cities;
        auto try_pick = [&](int dist) {
            for (auto* v : shuffled) {
                if (std::find(cities.begin(), cities.end(), v) != cities.end()) continue;
                if (Globals->VILLAGES_ONLY && entries_left[v->type] <= SETTLEMENTS_KEPT) continue;
                bool ok = true;
                for (auto* c : cities) {
                    graphs::Location2D a = { v->xloc, v->yloc };
                    graphs::Location2D b = { c->xloc, c->yloc };
                    if (cylDistance(a, b, w) < dist) { ok = false; break; }
                }
                if (ok) {
                    cities.push_back(v);
                    entries_left[v->type]--;
                }
                if ((int) cities.size() >= target_cities) break;
            }
        };

        try_pick(minCityDist);

        // Relaxation fallback: if we didn't reach target, loosen the distance
        // requirement (floor at CITY_DIST_MIN) and retry with the remaining
        // villages. Each relaxation step shrinks the gap by ~25%.
        int relaxed = minCityDist;
        while ((int) cities.size() < target_cities && relaxed > CITY_DIST_MIN) {
            relaxed = std::max(CITY_DIST_MIN, relaxed * 3 / 4);
            try_pick(relaxed);
        }

        for (auto* c : cities) {
            c->SetTownType(TOWN_CITY);
            logger::write("City upgrade: " + c->town->name + " (was Village)");
        }
        logger::write(
            "Cities placed: " + std::to_string(cities.size()) +
            "/" + std::to_string(target_cities) +
            " (minCityDist=" + std::to_string(minCityDist) +
            ", relaxed=" + std::to_string(relaxed) + ")");
    }

    // Pass 3: Round-robin trade good assignment.
    // Guarantees all IT_TRADE items appear in both M_BUY and M_SELL globally.
    // Two independent shuffled queues (buy_order / sell_order) cycle through all
    // items round-robin. sell_order starts at offset P/2 to place M_SELL of item X
    // in geographically different cities than its M_BUY (natural arbitrage barrier).
    logger::write("Assigning trade goods");

    // Build pool of all active IT_TRADE items
    std::vector<int> trade_pool;
    for (int i = 0; i < NITEMS; i++) {
        if (ItemDefs[i].flags & ItemType::DISABLED) continue;
        if (ItemDefs[i].flags & ItemType::NOMARKET) continue;
        if (!(ItemDefs[i].type & IT_TRADE)) continue;
        trade_pool.push_back(i);
    }

    std::vector<ARegion*> towns;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) continue;
            ARegion* reg = arr->GetRegion(x, y);
            if (reg && reg->town) towns.push_back(reg);
        }
    }

    int P = (int)trade_pool.size();
    if (P >= 6 && !towns.empty()) {
        const int TRADE_COUNT = 3;

        // Two independent shuffles for M_BUY and M_SELL queues
        std::vector<int> buy_order = trade_pool;
        std::vector<int> sell_order = trade_pool;
        for (int i = P - 1; i > 0; i--) { int j = rng::get_random(i+1); std::swap(buy_order[i],  buy_order[j]);  }
        for (int i = P - 1; i > 0; i--) { int j = rng::get_random(i+1); std::swap(sell_order[i], sell_order[j]); }

        // Shuffle towns for geographic spread
        std::vector<ARegion*> shuffled_towns = towns;
        for (int i = (int)shuffled_towns.size() - 1; i > 0; i--) {
            int j = rng::get_random(i + 1);
            std::swap(shuffled_towns[i], shuffled_towns[j]);
        }

        int buy_idx  = 0;
        int sell_idx = P / 2;  // Offset by half pool: M_SELL and M_BUY of same item land in different cities

        for (ARegion* t : shuffled_towns) {
            // Pick TRADE_COUNT M_BUY items round-robin
            std::vector<int> city_buy;
            std::unordered_set<int> used;
            for (int k = 0; k < TRADE_COUNT; k++) {
                city_buy.push_back(buy_order[buy_idx % P]);
                used.insert(buy_order[buy_idx % P]);
                buy_idx++;
            }

            // Pick TRADE_COUNT M_SELL items, skipping any that overlap with M_BUY
            std::vector<int> city_sell;
            int guard = P * 2;
            for (int k = 0; k < TRADE_COUNT && guard > 0; guard--) {
                int item = sell_order[sell_idx % P];
                sell_idx++;
                if (!used.count(item)) {
                    city_sell.push_back(item);
                    used.insert(item);
                    k++;
                }
            }

            t->SetupTradeMarkets(city_buy, city_sell);
        }
    }

    // -----------------------------------------------------------------------
    // Verdict.
    //
    // Counted off the finished map rather than off the placement tallies, so it
    // reflects whatever the city upgrade left behind.
    //
    // The bar is SETTLEMENTS_KEPT counted through gateway_ok, the same pair the city
    // upgrade respects, so the two cannot disagree about what a usable gateway looks
    // like. With VILLAGES_ONLY set that means villages; with it clear, any settlement
    // of the terrain counts, because the entry ladder reaches towns and cities too.
    //
    // Below the bar a terrain is not a thin start but a crowded one - every player
    // who picks it arrives in the same hex or two - and there is nothing to repair
    // after the fact: the terrain was too scarce on this map to hold its settlements
    // four apart. A world that fails is thrown away rather than shipped, and `new`
    // exits non-zero so a caller can simply roll again:
    //   until ./neworigins new; do :; done
    //
    // Measured over ten seeds each with VILLAGES_ONLY set, 64x48 clears this every
    // time; 48x48 fails it on two to four worlds in ten, mountain and tundra being
    // the terrains that run out of room first.
    // -----------------------------------------------------------------------
    std::unordered_map<int, int> final_entries;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) continue;
            ARegion* reg = arr->GetRegion(x, y);
            if (reg && gateway_ok(reg)) final_entries[reg->type]++;
        }
    }

    bool playable = true;
    std::string verdict = Globals->VILLAGES_ONLY
        ? std::string("Gateway villages:")
        : std::string("Gateway settlements:");
    std::string absent;

    for (int terrain = R_PLAIN; terrain <= R_TUNDRA; terrain++) {
        if (TerrainDefs[terrain].flags & TerrainType::BARREN) continue;

        // A terrain the climate never produced is a refusal, not an exemption: the
        // Nexus builds one gateway per terrain unconditionally (SetACNeighbors,
        // neworigins/map.cpp - the loop runs R_PLAIN..R_TUNDRA with no way to switch
        // an individual gateway off), so a missing terrain means a gateway leading
        // nowhere. That code already refuses such a world, but with a bare exit(1)
        // in the middle of generation; failing here instead makes the refusal clean
        // and says why in one line.
        if (histogram[terrain] == 0) absent += " " + std::string(TerrainDefs[terrain].name);

        int n = final_entries[terrain];
        verdict += " " + std::string(TerrainDefs[terrain].name) + "=" + std::to_string(n);
        if (n < SETTLEMENTS_KEPT) playable = false;
    }
    logger::write(verdict);

    if (!absent.empty())
        logger::write("REJECTED: these gateway terrains do not occur on this map at all:" +
                      absent + " - their gateways would lead nowhere. If a reroll keeps "
                      "producing this, the climate parameters cannot make that terrain.");

    if (!playable)
        logger::write("REJECTED: a gateway terrain has fewer than " +
                      std::to_string(SETTLEMENTS_KEPT) +
                      (Globals->VILLAGES_ONLY ? " villages" : " settlements") +
                      " - regenerate this world.");

    return playable;
}

void addAncientStructure(ARegion* reg, int type, double damage, std::optional<std::string> name = std::nullopt) {
    ObjectType& info = ObjectDefs[type];

    Object * obj = new Object(reg);
    int num = reg->buildingseq++;
    int needs = std::clamp(0, (int) (info.cost * damage), info.cost - 1);
    obj->num = num;
    obj->type = type;

    if (!name) name = getObjectName(type, info);

    obj->set_name(*name);
    logger::write("+ " + obj->name + " : " + info.name + ", needs " + std::to_string(needs));

    obj->incomplete = needs;

    reg->objects.push_back(obj);
}

// ============================================================================
// AddHistoricalBuildings: Generate ancient/ruined structures during world creation
//
// This function creates historical buildings in 4 phases:
// Phase 1: Defensive structures (castles, forts, towers) and inns
// Phase 2: Production buildings for basic resources (optional)
// Phase 3: Calculate shortest paths between cities using Dijkstra
// Phase 4: Build ancient road networks connecting nearby cities (optional)
// ============================================================================
void ARegionList::AddHistoricalBuildings(ARegionArray* arr, const int w, const int h, Map* map) {
    std::vector<ARegion*> cities;

    // ========================================================================
    // PHASE 1: Defensive structures and inns
    // ========================================================================
    // Creates ancient castles, forts, towers based on town/region population
    // Also creates inns in towns based on town type (village/town/city)
    // All structures are partially damaged (need repair to complete)
    // ========================================================================

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            // Skip invalid hex coordinates (only process valid game hexes)
            if ((x + y) % 2) {
                continue;
            }

            ARegion* reg = arr->GetRegion(x, y);

            // Collect all cities for road network generation (Phase 4)
            if (reg->town && reg->town->TownType() == TOWN_CITY) {
                cities.push_back(reg);
            }

            TerrainType* terrain = &(TerrainDefs[reg->type]);

            // Skip uninhabitable terrain (ocean, volcano, barren)
            if (reg->type == R_OCEAN || reg->type == R_VOLCANO || terrain->flags & TerrainType::BARREN) {
                continue;
            }

            // --- Regions with towns ---
            if (reg->town) {
                // Large cities (8000+): 37% chance of ancient castle
                // Probability: 3d6 >= 12 (37% chance)
                // Damage: 0-91% (random 2d6-1 divided by 12)
                if (reg->town->pop > 8000) {
                    if (rng::make_roll(3, 6) >= 12) {
                        addAncientStructure(reg, O_CASTLE, (rng::make_roll(2, 6) - 1) / 12.0);
                    }
                }
                // Medium towns (4000-8000): 16% chance of ancient fort
                // Probability: 3d6 >= 14 (16% chance)
                else if (reg->town->pop > 4000) {
                    if (rng::make_roll(3, 6) >= 14) {
                        addAncientStructure(reg, O_FORT, (rng::make_roll(2, 6) - 1) / 12.0);
                    }
                }
                // Small towns (2000-4000): 16% chance of ancient tower
                // Probability: 3d6 >= 14 (16% chance)
                else if (reg->town->pop > 2000) {
                    if (rng::make_roll(3, 6) >= 14) {
                        addAncientStructure(reg, O_TOWER, (rng::make_roll(2, 6) - 1) / 12.0);
                    }
                }

                // Generate ancient inns based on town type
                // Villages (type=0): 1d6 roll, Cities (type=2): 3d6 roll
                // Higher rolls = more inns, last inn may be more damaged
                int roll = rng::make_roll(reg->town->TownType() + 1, 6);
                int count = ceil(roll / 6);          // Number of inns to create
                int damage = 6 - roll % 6;           // Damage for last inn (0-5)

                for (int i = 0; i < count; i++) {
                    double damagePoints = 0;
                    // Last inn gets additional damage if damage >= 3
                    if (i == count - 1) {
                        if (damage >= 3) {
                            break;  // Too damaged to create
                        }
                        damagePoints = damage / 6.0;  // 0-33% damage
                    }
                    addAncientStructure(reg, O_INN, damagePoints);
                }
            }
            // --- Regions without towns (wilderness) ---
            else {
                // High population wilderness (2000+): 4% chance of fort
                if (reg->population > 2000) {
                    if (rng::make_roll(3, 6) >= 16) {
                        addAncientStructure(reg, O_FORT, (rng::make_roll(2, 6) - 1) / 12.0);
                    }
                }
                // Medium population wilderness (1000-2000): 1% chance of tower
                else if (reg->population > 1000) {
                    if (rng::make_roll(3, 6) >= 17) {
                        addAncientStructure(reg, O_TOWER, (rng::make_roll(2, 6) - 1) / 12.0);
                    }
                }
            }
        }
    }

    // ========================================================================
    // PHASE 2: Production buildings for basic resources (optional)
    // ========================================================================
    // Creates ruined production buildings (farms, mines, etc.) based on
    // resources available in each region. Only one building per region.
    // Buildings are heavily damaged (20-80% damage) requiring repair.
    // Probability: 40-50% for towns, 5-10% for wilderness
    // ========================================================================

    if (map->generateHistoricalProductionBuildings) {
        // Mapping: resource item -> production building type
        std::map<int, int> resourceToBuilding = {
            {I_GRAIN,     O_FARM},          // Grain -> Farm
            {I_LIVESTOCK, O_RANCH},         // Livestock -> Ranch
            {I_WOOD,      O_TIMBERYARD},    // Wood -> Timber Yard
            {I_IRON,      O_MINE},          // Iron -> Mine
            {I_STONE,     O_QUARRY},        // Stone -> Quarry
            {I_FUR,       O_TRAPPINGHUT},   // Fur -> Trapping Hut
            {I_HERBS,     O_TEMPLE},        // Herbs -> Temple
            {I_HORSE,     O_STABLE},        // Horse -> Stable
            {I_CAMEL,     O_OASIS},         // Camel -> Oasis
        };

        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                // Skip invalid hex coordinates
                if ((x + y) % 2) {
                    continue;
                }

                ARegion* reg = arr->GetRegion(x, y);

                // Skip uninhabitable terrain
                if (reg->type == R_OCEAN || reg->type == R_VOLCANO) {
                    continue;
                }

                // Collect all basic resources available in this region
                std::vector<int> availableResources;
                for (const auto& prod : reg->products) {
                    int itemType = prod->itemtype;
                    // Check if this resource has a production building
                    if (resourceToBuilding.find(itemType) != resourceToBuilding.end()) {
                        availableResources.push_back(itemType);
                    }
                }

                // Skip if no basic resources in this region
                if (availableResources.empty()) {
                    continue;
                }

                // Choose one random resource from available resources
                int randomIndex = rng::get_random(availableResources.size());
                int chosenResource = availableResources[randomIndex];
                int buildingType = resourceToBuilding[chosenResource];

                // Determine probability based on whether region has a town
                int probability;
                if (reg->town) {
                    // Towns: 40-50% chance
                    probability = 40 + rng::get_random(21);  // 40-60%
                } else {
                    // Wilderness: 5-10% chance
                    probability = 5 + rng::get_random(16);    // 5-20%
                }

                // Roll for building creation
                if (rng::get_random(100) < probability) {
                    // Generate damage: 30-80% (buildings need significant repair)
                    // Formula: 0.3 + random(0.0-0.6) = 0.3-0.8
                    double damage = 0.3 + (rng::get_random(51) / 100.0);

                    // Generate race-specific, resource-specific building name
                    std::string buildingName = getProductionBuildingName(buildingType, chosenResource, reg->race);

                    addAncientStructure(reg, buildingType, damage, buildingName);
                }
            }
        }
    }

    // ========================================================================
    // PHASE 3 & 4: Ancient road network generation (optional)
    // ========================================================================
    // Phase 3: Calculate shortest paths between all cities using Dijkstra
    // Phase 4: Build ancient roads connecting nearby cities (within 8 hexes)
    // Roads are directional structures placed in both regions they connect
    // ========================================================================

    if (map->generateHistoricalRoads) {
        // --- Phase 3: Setup pathfinding graph ---
        // Configure graph for Dijkstra pathfinding between cities
        ARegionGraph graph = ARegionGraph(arr);

        // Exclude ocean and volcano hexes from road paths
        graph.setInclusion([](ARegion* current, ARegion* next) {
            return next->type != R_OCEAN && next->type != R_VOLCANO;
        });

        // Terrain movement costs for pathfinding
        // Difficult terrain (mountains, forests) = 2 cost
        // Easy terrain (plains, desert) = 1 cost
        graph.setCost([](ARegion* current, ARegion* next) {
            switch (next->type) {
                case R_MOUNTAIN:
                case R_FOREST:
                case R_JUNGLE:
                case R_SWAMP:
                case R_TUNDRA:
                    return 2;  // Difficult terrain

                case R_PLAIN:
                case R_DESERT:
                    return 1;  // Easy terrain

                default:
                    return 0;
            }
        });

        // --- Calculate distances between all city pairs ---
        // Uses Dijkstra's algorithm to find shortest path between each city pair
        // Stores results in symmetric distance matrix for Phase 4
        size_t sz = cities.size();
        int distances[sz][sz];

        // Every cell must start at 0: the fill loop below only visits j > i, so
        // the diagonal and the lower triangle were previously read uninitialised
        // by phase 4.
        for (size_t i = 0; i < sz; i++)
            for (size_t j = 0; j < sz; j++)
                distances[i][j] = 0;

        for (size_t i = 0; i < sz; i++) {
            for (size_t j = i + 1; j < sz; j++) {
                auto start = cities[i];
                auto end = cities[j];

                graphs::Location2D startLoc = { .x = start->xloc, .y = start->yloc };
                graphs::Location2D endLoc = { .x = end->xloc, .y = end->yloc };

                // Run Dijkstra pathfinding from start to end city
                std::unordered_map<graphs::Location2D, graphs::Location2D> cameFrom;
                std::unordered_map<graphs::Location2D, double> costSoFar;
                graphs::dijkstraSearch(graph, startLoc, endLoc, cameFrom, costSoFar);

                // Count hexes in shortest path by backtracking from end to start.
                // Two cities on different landmasses have no path at all - ocean and
                // volcano are excluded from the graph - so dijkstra never records the
                // end hex. Reading it with operator[] used to insert a default {0,0}
                // and return it, which walked to {0,0}, mapped {0,0} to itself and
                // span forever with no output. Look the predecessor up instead.
                int dist = 0;
                bool reachable = true;
                while (endLoc != startLoc) {
                    auto it = cameFrom.find(endLoc);
                    if (it == cameFrom.end()) {
                        reachable = false;
                        break;
                    }
                    endLoc = it->second;
                    dist++;
                }
                if (!reachable) dist = 0;

                // Store distance in symmetric matrix. 0 means "do not connect".
                distances[i][j] = dist;
                distances[j][i] = dist;
            }
        }

        // --- Phase 4: Build ancient roads between nearby cities ---
        // Only connect cities within 8 hexes distance
        // Each city connects to at most one other city (prevents over-connecting)
        // Roads are directional (separate road object in each region)
        std::unordered_set<ARegion*> connected;

        for (size_t i = 0; i < sz; i++) {
            auto start = cities[i];

            // Skip if this city already has a road connection
            if (connected.find(start) != connected.end()) {
                continue;
            }

            for (size_t j = 0; j < sz; j++) {
                auto dist = distances[i][j];

                // Skip if same city, unreachable, or too far (>8 hexes)
                if (!dist || dist > 8) {
                    continue;
                }

                auto end = cities[j];

                // Skip if destination city already has a road connection
                if (connected.find(end) != connected.end()) {
                    continue;
                }

                // Mark both cities as connected
                connected.emplace(start);
                connected.emplace(end);

                // Name the road after its destination
                std::string name = "Road to " + end->town->name;

                // Re-run pathfinding to get exact route for road placement
                graphs::Location2D startLoc = { .x = start->xloc, .y = start->yloc };
                graphs::Location2D endLoc = { .x = end->xloc, .y = end->yloc };

                std::unordered_map<graphs::Location2D, graphs::Location2D> cameFrom;
                std::unordered_map<graphs::Location2D, double> costSoFar;
                graphs::dijkstraSearch(graph, startLoc, endLoc, cameFrom, costSoFar);

                // Place directional road objects along the path
                // Roads are directional: need separate object in each region
                ARegion* endReg = end;
                while (endLoc != startLoc) {
                    endLoc = cameFrom[endLoc];
                    ARegion *current = GetRegion(endLoc.x, endLoc.y, end->zloc);

                    // Find direction from current to endReg
                    int dir;
                    for (dir = 0; dir < NDIRS; dir++) {
                        if (current->neighbors[dir] == endReg) {
                            break;
                        }
                    }

                    // Calculate opposite direction (for return road)
                    int opositeDir = (dir + 3) % NDIRS;

                    // Map directions to road building types
                    int ROAD_BUILDINGS[NDIRS];
                    ROAD_BUILDINGS[D_NORTH] = O_ROADN;
                    ROAD_BUILDINGS[D_NORTHEAST] = O_ROADNE;
                    ROAD_BUILDINGS[D_NORTHWEST] = O_ROADNW;
                    ROAD_BUILDINGS[D_SOUTH] = O_ROADS;
                    ROAD_BUILDINGS[D_SOUTHEAST] = O_ROADSE;
                    ROAD_BUILDINGS[D_SOUTHWEST] = O_ROADSW;

                    // 67% chance to place road section (some gaps for realism)
                    if (rng::get_random(3)) {
                        // Check if road already exists in this direction (current->endReg)
                        bool canBuild = true;
                        for(const auto o : current->objects) {
                            if (o->type == ROAD_BUILDINGS[dir]) {
                                canBuild = false;
                                break;
                            }
                        }

                        if (canBuild) {
                            // Damage: 0-91% (formula: (2d6-6)/6 = range -0.67 to 0.83)
                            // Negative damage becomes 0 (fully intact sections)
                            addAncientStructure(current, ROAD_BUILDINGS[dir],
                                              (rng::make_roll(2, 6) - 6.0) / 6.0, name);
                        }

                        // Also place return road (endReg->current)
                        canBuild = true;
                        for(const auto o : endReg->objects) {
                            if (o->type == ROAD_BUILDINGS[opositeDir]) {
                                canBuild = false;
                                break;
                            }
                        }

                        if (canBuild) {
                            addAncientStructure(endReg, ROAD_BUILDINGS[opositeDir],
                                              (rng::make_roll(2, 6) - 6.0) / 6.0, name);
                        }
                    }

                    // Move to next hex in path
                    endReg = current;
                }
            }
        }
    } // End if (map->generateHistoricalRoads)
}

void assertAllRegionsHaveName(const int w, const int h, ARegionArray* arr) {
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;
            }

            ARegion* reg = arr->GetRegion(x, y);
            assert(!reg->name.empty() && "Region must have name");
        }
    }
}

void ARegionList::create_natural_surface_level(Map* map) {
    static const int level = 1;

    const int w = map->map.width / 2;
    const int h = map->map.height / 2;

    MakeRegions(level, w, h);

    pRegionArrays[level]->set_name("");
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_SURFACE;

    map->Generate();

    ARegionArray* arr = pRegionArrays[level];
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if ((x + y) % 2) {
                continue;
            }

            ARegion* reg = arr->GetRegion(x, y);
            Cell* cell = map->map.get(reg->xloc * 2, reg->yloc * 2);
            reg->type = mapBiome(cell->biome);
        }
    }

    // all water bodies
    std::vector<WaterBody*> waterBodies;

    // all rivers
    std::unordered_map<ARegion*, int> rivers;

    const int maxRiverReach = std::min(w, h) / 4;
    makeRivers(map, arr, waterBodies, rivers, w, h, maxRiverReach);

    cleanupIsolatedPlaces(arr, waterBodies, rivers, w, h);

    placeVolcanoes(arr, w, h);

    placeLakes(arr, w, h, map->lakePercent);

    GrowRaces(arr);

    giveNames(arr, waterBodies, rivers, w, h);
    assertAllRegionsHaveName(w, h, arr);

    // Game::CreateWorld() is a ruleset entry point with a void signature, so the
    // verdict rides on the list rather than up the call chain; NewGame() reads it.
    settlements_ok = economy(arr, w, h);

    AddHistoricalBuildings(arr, w, h, map);
}

ARegionGraph::ARegionGraph(ARegionArray* regions) {
    this->regions = regions;
    this->costFn = [](ARegion* current, ARegion* next) { return 1; };
    this->includeFn = [](ARegion* current, ARegion* next) { return true; };
}

ARegionGraph::~ARegionGraph() {

}

ARegion* ARegionGraph::get(graphs::Location2D id) {
    return regions->GetRegion(id.x, id.y);
}

std::vector<graphs::Location2D> ARegionGraph::neighbors(graphs::Location2D id) {
    ARegion* current = regions->GetRegion(id.x, id.y);

    std::vector<graphs::Location2D> list;
    for (int i = 0; i < NDIRS; i++) {
        ARegion* next = current->neighbors[i];
        if (next == NULL) {
            continue;
        }

        if (!includeFn(current, next)) {
            continue;
        }

        list.push_back({ next->xloc, next->yloc });
    }

    return list;
}

double ARegionGraph::cost(graphs::Location2D current, graphs::Location2D next) {
    return this->costFn(get(current), get(next));
}

void ARegionGraph::setCost(ARegionCostFunction costFn) {
    this->costFn = costFn;
}

void ARegionGraph::setInclusion(ARegionInclusionFunction includeFn) {
    this->includeFn = includeFn;
}

void ARegionList::ResourcesStatistics() {
    std::unordered_map<int, int> resources;
    std::unordered_map<int, int> forSale;
    std::unordered_map<int, int> wanted;

    for(const auto reg : regions) {
        for (const auto& p : reg->products) {
            resources[p->itemtype] += p->amount;
        }

        for (const auto& m : reg->markets) {
            if (m->type == Market::MarketType::M_BUY) {
                forSale[m->item] += m->amount;
            }
            else {
                wanted[m->item] += m->amount;
            }
        }
    }

    logger::write("");
    logger::write("Products:");
    for (auto kv : resources) {
        if (kv.first == I_SILVER || kv.first <= -1) {
            continue;
        }

        ItemType& item = ItemDefs[kv.first];
        logger::write(item.name + " [" + item.abr + "] " + std::to_string(kv.second));
    }
    logger::write("");

    logger::write("Wanted:");
    for (auto kv : wanted) {
        if (kv.first == I_SILVER || kv.first <= -1) {
            continue;
        }

        ItemType& item = ItemDefs[kv.first];
        logger::write(item.name + " [" + item.abr + "] " + std::to_string(kv.second));
    }
    logger::write("");

    logger::write("For Sale:");
    for (auto kv : forSale) {
        if (kv.first == I_SILVER || kv.first <= -1) {
            continue;
        }

        ItemType& item = ItemDefs[kv.first];
        logger::write(item.name + " [" + item.abr + "] " + std::to_string(kv.second));
    }
    logger::write("");
}

const std::unordered_map<ARegion*, graphs::Node<ARegion*>> breadthFirstSearch(ARegion* start, const int maxDistance) {
    std::queue<graphs::Node<ARegion*>> frontier;
    frontier.push({ start, 0 });

    std::unordered_map<ARegion*, graphs::Node<ARegion*>> cameFrom;
    cameFrom[start] = { start, 0 };

    while (!frontier.empty()) {
        graphs::Node<ARegion*> current = frontier.front();
        frontier.pop();

        if (current.distance > maxDistance) {
            continue;
        }

        for (int i = 0; i < NDIRS; i++) {
            auto next = current.key->neighbors[i];
            if (!next) {
                continue;
            }

            if (cameFrom.find(next) == cameFrom.end()) {
                frontier.push({ next, current.distance + 1 });
                cameFrom[next] = current;
            }
        }
    }

    return cameFrom;
}

int ARegionList::FindDistanceToNearestObject(int object_type, ARegion *start)
{
    std::vector<ARegion *> targets;
    // Just find all regions which contain the object, then compute the smallest distance.  I contemplated using
    // the breadth first and dijkstra search, but this is actually simpler and should be faster.
    for(const auto r : regions) {
        // For now, we only care about regions on the same zloc
        if (r->zloc != start->zloc) continue;
        for(const auto o : r->objects) {
            if (o->type == object_type) {
                targets.push_back(r);
                break;
            }
        }
    }
    // compute the nearest linear cylindrical distance to the targets
    int min_dist = 100000000;
    int w = pRegionArrays[start->zloc]->x;
    graphs::Location2D initial = { start->xloc, start->yloc };
    for(const auto target: targets) {
        graphs::Location2D candidate = { target->xloc, target->yloc };
        int dist = cylDistance(initial, candidate, w);
        if (dist < min_dist) min_dist = dist;
    }
    return min_dist;
}

int ARegionList::find_distance_between_regions(ARegion *start, ARegion *end)
{
    if (start->zloc != end->zloc) {
        return -1; // not on the same level
    }

    int w = start->level->x;
    graphs::Location2D a = { start->xloc, start->yloc };
    graphs::Location2D b = { end->xloc, end->yloc };
    return cylDistance(a, b, w);
}
