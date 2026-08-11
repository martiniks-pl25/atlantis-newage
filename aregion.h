#pragma once
#ifndef REGION_H
#define REGION_H

class ARegion;
class ARegionList;
class ARegionArray;

#include "gamedefs.h"
#include "logger.hpp"
#include "faction.h"
#include "unit.h"
#include "production.h"
#include "market.h"
#include "object.h"
#include "graphs.h"
#include "mapgen.h"
#include "safe_list.h"

#include "external/nlohmann/json.hpp"
using json = nlohmann::json;

#include <map>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <string>

/* Weather Types */
enum {
    W_NORMAL,
    W_WINTER,
    W_MONSOON,
    W_BLIZZARD
};

struct Product
{
    int product;
    int chance;
    int amount;
};

class TerrainType
{
    public:
        const std::string name;
        const std::string plural;
        const std::string type;
        const char marker;
        int similar_type;

        enum {
            RIDINGMOUNTS  = 0x1,
            FLYINGMOUNTS  = 0x2,
            BARREN        = 0x4,
            SHOW_RULES    = 0x8,
            ANNIHILATED   = 0x10,
            NO_LEADERS    = 0x20,  // No leader recruitment allowed
        };
        int flags;

        int pop;
        int wages;
        int economy;
        int movepoints;
        Product prods[7];
        // Race information
        // A hex near water will have either one of the normal races or one
        // of the coastal races in it.   Non-coastal hexes will only have one
        // of the normal races.
        int races[4];
        int coastal_races[3];
        int wmonfreq;
        int smallmon;
        int bigmon;
        int humanoid;
        int lairChance;
        int lairs[6];
};

 extern std::vector<TerrainType> TerrainDefs;

class Location
{
    public:
        Unit *unit;
        Object *obj;
        ARegion *region;
};

Location *GetUnit(std::list<Location *>& locs, int unitid);

int AGetName(int town, ARegion *r);
const std::string& AGetNameString(int name);

// Name every ARegion starts life with; world generation is expected to replace
// it on every region. ARegionList::NameStatistics() reports any that survive.
inline constexpr const char *UNNAMED_REGION = "Region";

// Generation-time tuning statistics. Set to false - or delete the two report
// functions and their call sites - once map tuning is finished. Everything it
// guards runs only inside `new`, never during turn processing.
inline constexpr bool GENERATION_TUNING_STATS = true;

// Which of the starting-location requirement groups a hex satisfies within two
// moves. Shared by the gateway candidate filter and the generation statistics so
// the rule exists in exactly one place.
struct StartRequirements {
    bool wood     = false;
    bool iron     = false;
    bool stone    = false;
    bool food     = false;  // grain OR livestock
    bool mounts   = false;  // horse OR camel
    bool landmass = false;  // >= 10 non-ocean hexes reachable within 3 moves
    int  reach    = 0;      // non-ocean hexes reachable within 3 moves, the landmass rule's input

    bool all() const { return wood && iron && stone && food && mounts && landmass; }
};

// Integer division that rounds to nearest instead of truncating. The tuning
// report is read to two significant figures, where a truncated 17.8 -> 17 or
// 98.9% -> 98% is enough to send a parameter search the wrong way.
int rounded_div(int numerator, int denominator);
int percent_rounded(int part, int whole);

// Summary of a set of distances in whole hexes. mean_tenths is the mean times ten,
// so a report can print one decimal without floating point.
struct DistanceSummary {
    int count = 0;
    int min = 0;
    int max = 0;
    int median = 0;
    int mean_tenths = 0;
};

DistanceSummary summarise_distances(std::vector<int> distances);

// Hex-step distance on a cylinder: the plain distance, or the distance around
// either edge, whichever is shortest. This is the metric the whole generator uses
// for spacing. GetPlanarDistance() is not interchangeable with it - on an
// icosahedral world that one runs a search rather than arithmetic.
int cylDistance(graphs::Location2D a, graphs::Location2D b, int w);

// True when every distance is at least min_spacing. Settlements closer than that
// let one player sit between two of them and take both in a single move, so no
// placement pass may put them nearer.
bool far_enough(const std::vector<int>& distances, int min_spacing);

// What one round-robin placement pass produced. `order` is the terrains it walked,
// scarcest first; `chosen` is in placement order and carries no size yet - the
// caller decides what each site becomes.
struct SettlementPlacement {
    std::vector<ARegion*> chosen;
    std::vector<int> order;
    std::unordered_map<int, int> placed_by_terrain;
    int rounds = 0;
};

// Round-robin settlement placement, shared by the surface and the underground.
// The caller supplies the candidates (having applied its own eligibility policy),
// the spacing roll, how many opening rounds forgive a miss, and an optional ceiling
// on the free stage. See the definition in aregion.cpp for the full rules.
SettlementPlacement place_settlements_round_robin(
    const std::unordered_map<int, std::vector<ARegion*>>& candidates,
    const int w,
    const std::function<int()>& spacing_roll,
    int guaranteed_rounds,
    int ceiling);

std::string describe_placement(const SettlementPlacement& p, int ceiling);

class Farsight
{
    public:
        Farsight();

        Faction *faction;
        Unit *unit;
        int level;
        int observation;
        int exits_used[NDIRS];
};

Farsight *GetFarsight(std::list<Farsight *>& farsees, Faction *);

enum {
    TOWN_VILLAGE,
    TOWN_TOWN,
    TOWN_CITY,
    NTOWNS
};

// Development-weight divisor for entertainment income by town tier (see TownGrowth()).
// village /1 (max help to small settlements), town /4, city /8 (weak development
// lever for large cities, since entertainment costs no faction points).
int entertainment_dev_divisor(int towntype);

// Pirate movement avoidance (defined in npc.cpp, see docs/PIRATE_FLEET_SYSTEM.md).
//
// These model the pirates' own risk assessment, not player defence: a settlement
// holding player troops is where a pirate fleet becomes easy prey. The radius
// scales with tier, because tier predicts how much player force is likely there.
//
// EVERY pirate movement path must use these two and nothing else - the rule used to
// be duplicated as a lambda in Unit::DefaultOrders and Game::RunCallPirates, the two
// copies silently diverged, and summoned fleets obeyed a different radius than
// wandering ones.

// Hex rule: a player-guarded town or city is refused outright (villages never are).
bool pirate_avoids_settlement(const ARegion *r);

// Ring rule: a LAND region adjacent to a player-guarded city is refused too.
// Always false for water at any tier - pirates sail open water freely, which is also
// what guarantees a fleet sitting on land always has an exit (land->land moves are
// forbidden, so water is its only way out and it can never be trapped).
bool pirate_avoids_city_ring(const ARegion *r);

class TownInfo
{
    public:
        TownInfo();
        ~TownInfo();

        void Readin(std::istream& f);
        void Writeout(std::ostream& f);
        int TownType();

        std::string name;
        int pop;
        int activity;
        // achieved settled habitat
        int hab;
        // town's development
        int dev;
};

struct RegionSetup {
    TerrainType* terrain;
    int habitat;
    double prodWeight;
    bool addLair;
    bool addSettlement;
    std::string settlementName;
    int settlementSize;
};

class ARegion
{
    friend class Game;
    friend class ARegionArray;
    friend class ARegionList;
    friend class UnitTestHelper;

    public:
        ARegion();
        //ARegion(int, int);
        ~ARegion();

        void Setup();
        void ManualSetup(const RegionSetup& settings);

        // ManualSetup in two halves, so world generation can roll products for the
        // whole map before choosing settlement sites: the starting-location check
        // reads products of hexes up to three moves away, and those do not exist
        // until their own setup has run. ManualSetup is exactly these two with
        // add_town in between; callers that do not need the split keep using it.
        void setup_terrain(const RegionSetup& settings);
        void finish_setup(const RegionSetup& settings);

        void ZeroNeighbors();
        void set_name(const std::string& newname);
        void assign_generated_name(int levelType);

        void Writeout(std::ostream& f);
        void Readin(std::istream& f, std::list<Faction *>& factions);

        int CanMakeAdv(Faction *, int);
        int HasItem(Faction *, int);
        json basic_region_data();
        void build_json_report(json& j, Faction *fac, int month, ARegionList& regions);

        std::string short_print();
        std::string print();

        void Kill(Unit *);
        void ClearHell();

        Unit *GetUnit(int);
        Unit *GetUnitAlias(int, int); /* alias, faction number */
        Unit *GetUnitId(UnitId *, int);
        void deduplicate_unit_list(std::list<UnitId *>& list, int factionid);
        Location *GetLocation(UnitId *, int);

        void SetLoc(int, int, int);
        bool Present(Faction *f);
        std::set<Faction *> PresentFactions();
        int GetObservation(Faction *, int);
        int GetTrueSight(Faction *, int);

        Object *GetObject(int);
        Object *GetDummy();
        void CheckFleets();

        int MoveCost(int, ARegion *, int, std::string *road);
        Unit *Forbidden(Unit *); /* Returns unit that is forbidding */
        Unit *ForbiddenByAlly(Unit *); /* Returns unit that is forbidding */
        int CanTax(Unit *);
        int CanGuard(Unit *);
        int CanPillage(Unit *);
        void Pillage();
        int ForbiddenShip(Object *);
        int HasCityGuard();
        bool notify_spell_use(Unit *caster, const std::string& spell, ARegionList& regs);
        void notify_city(Unit *, const std::string& oldname, const std::string& newname);

        void DefaultOrders();
        int TownGrowth();
        int food_effective_amount(const Market* m) const;
        void PostTurn();
        void UpdateProducts();
        void add_or_increase_product(int item, int amount);
        void SetWeather(int newWeather);
        int IsCoastal();
        int IsCoastalOrLakeside();
        int IsDeepOcean();  // ocean with no land neighbors
        void MakeStartingCity();
        int IsStartingCity();
        int IsSafeRegion();
        int CanBeStartingCity(ARegionArray *pRA);
        int HasShaft();

        // AS
        int HasRoad();
        int HasExitRoad(int realDirection);
        int CountConnectingRoads();
        int HasConnectingRoad(int realDirection);
        int GetRoadDirection(int realDirection);
        int GetRealDirComp(int realDirection);
        void DoDecayCheck();
        void DoDecayClicks(Object *o);
        void RunDecayEvent(Object *o);
        std::string get_decay_flavor();
        int GetMaxClicks();
        int PillageCheck();

        // JR
        int GetPoleDistance(int dir);
        void SetGateStatus(int month);
        void DisbandInRegion(int, int);
        void Recruit(int);
        int IsNativeRace(int);
        void AdjustPop(int);
        void FindMigrationDestination(int round);
        int MigrationAttractiveness(int, int, int);
        void Migrate();
        void SetTownType(int);
        int DetermineTownSize();
        int TraceConnectedRoad(int dir, int sum, std::list<ARegion *>& con, int range, int dev);
        int RoadDevelopmentBonus(int, int);
        int BaseDev();
        int ProdDev();
        int TownHabitat();
        int RoadDevelopment();
        int TownDevelopment();
        int CheckSea(int, int, int);
        int Slope();
        int SurfaceWater();
        int Soil();
        int Winds();
        int TerrainFactor(int, int);
        int TerrainProbability(int);
        void AddFleet(Object *);
        int ResolveFleetAlias(int);

        int CountWMons();
        int IsGuarded();
        int HasCityGuards();
        Unit* GetCityGuard();
        int HasLair();

        int Wages();
        std::string wages_for_report();
        int Population();

        // ruleset specific movment checks
        const std::optional<std::string> movement_forbidden_by_ruleset(Unit *unit, ARegion *origin, ARegionList& regions);

        std::string name;
        int num;
        int type;
        int buildingseq;
        int weather;
        int gate;
        int gatemonth;
        int gateopen;

        TownInfo *town;
        int race;
        int population;
        int basepopulation;
        int wages;
        int maxwages;
        int wealth;

        /* Economy */
        int habitat;
        int development;
        int maxdevelopment;
        int elevation;
        int humidity;
        int temperature;
        int vegetation;
        int culture;
        // migration origins
        std::list<ARegion *> migfrom;
        // mid-way migration development
        int migdev;
        int immigrants;
        int emigrants;
        // economic improvement
        int improvement;

        /* Potential bonuses to economy */
        int clearskies;
        int earthlore;
        int phantasmal_entertainment;

        ARegion *neighbors[NDIRS];
        safe::list<Object *> objects;
        std::map<int,int> newfleets;
        int fleetalias;
        std::list<Unit *> hell; /* Where dead units go */
        std::list<Farsight *> farsees;
        // List of units which passed through the region
        std::list<Farsight *>passers;
        std::vector<Production *> products;
        std::vector<Market*> markets;
        int xloc, yloc, zloc;
        int visited;

        // Used for calculating distances using an A* search
        int distance;
        ARegion *next;

        // A link to the region's level to make some things easier.
        ARegionArray *level;

        // find a production for a certain skill.
        Production *get_production_for_skill(int item, int skill);
        int produces_item(int item);

        // Editing functions
        void UpdateEditRegion();
        void SetupEditRegion();
        void AddLeadersMarket();
        void AddMenMarket();
        void SetupTradeMarkets(const std::vector<int>& buy_items, const std::vector<int>& sell_items);
        void SetupRandomTradeMarkets();
        // Town creation (public to allow ruleset setup and unit tests)
        void add_town();
        void add_town(int size);
        void add_town(const std::string& name);
        void add_town(int size, const std::string& name);
    private:
        /* Private Setup Functions */
        void SetupPop();
        void SetupProds(double weight);
        void SetIncome();
        void Grow();
        void SetupCityMarket();
        void MakeLair(int);
        void LairCheck();
        std::vector<int> GetPossibleLairs();
        void SetupHabitat(TerrainType* terrain);
        void SetupEconomy();
};

class ARegionArray
{
    public:
        ARegionArray(int, int);
        ~ARegionArray();

        void SetRegion(int, int, ARegion *);
        ARegion *GetRegion(int, int);
        void set_name(const std::string& name);

        std::vector<ARegion *> get_starting_region_candidates(int terrain);
        std::vector<ARegion *> get_starting_region_candidates(int terrain, bool require_resources);
        StartRequirements start_requirements_at(ARegion *reg);

        int x;
        int y;
        ARegion **regions;
        std::string strName;

        enum {
            LEVEL_NEXUS,
            LEVEL_SURFACE,
            LEVEL_UNDERWORLD,
            LEVEL_UNDERDEEP,
            LEVEL_DUNGEON,
        };
        int levelType;
};

class ARegionFlatArray
{
    public:
        ARegionFlatArray(int);
        ~ARegionFlatArray();

        void SetRegion(int, ARegion *);
        ARegion *GetRegion(int);

        int size;
        ARegion **regions;
};

struct Geography
{
    int elevation;
    int humidity;
    int temperature;
    int vegetation;
    int culture;
};

class GeoMap
{
    public:
        GeoMap(int, int);
        void Generate(int spread, int smoothness);
        int GetElevation(int, int);
        int GetHumidity(int, int);
        int GetTemperature(int, int);
        int GetVegetation(int, int);
        int GetCulture(int, int);
        void ApplyGeography(ARegionArray *pArr);

        int size, xscale, yscale, xoff, yoff;
        std::map<long int, Geography> geomap;

};

class ARegionList
{
    std::vector<ARegion *> regions;

    public:
        using iterator = typename std::vector<ARegion *>::iterator;

        ARegionList();
        ~ARegionList();

        ARegion *GetRegion(int);
        ARegion *GetRegion(int, int, int);
        int ReadRegions(std::istream &f, std::list<Faction *>& facs);
        void WriteRegions(std::ostream&  f);
        Location *FindUnit(int);
        Location *GetUnitId(UnitId *id, int faction, ARegion *cur);

        void ChangeStartingCity(ARegion *, int);
        ARegion *GetStartingCity(ARegion *AC, int num, int level, int maxX,
                int maxY);

        ARegion *FindGate(int);
        int GetPlanarDistance(ARegion *one, ARegion *two, int penalty, int maxdist = -1);
        int get_connected_distance(ARegion *start, ARegion *target, int penalty, int maxdist = -1);
        int GetWeather(ARegion *pReg, int month);

        ARegionArray *GetRegionArray(int level);
        ARegionArray *get_first_region_array_of_type(int type);

        int numberofgates;
        int numLevels;
        ARegionArray **pRegionArrays;

        inline iterator begin() { return regions.begin(); }
        inline iterator end() { return regions.end(); }
        inline iterator erase(iterator it) { return regions.erase(it); }
        inline size_t size() { return regions.size(); }
        inline void clear() { regions.clear(); }
        inline ARegion *front() { return regions.front(); }

        //
        // Public world creation stuff
        //
        void create_levels(int numLevels);
        void create_abyss_level(int level, const std::string& name);
        void create_nexus_level(int level, int xSize, int ySize, const std::string& name);
        void create_surface_level(int level, int xSize, int ySize, const std::string& name);
        void create_natural_surface_level(Map* map);

        // Set by create_natural_surface_level(): false when the surface came out
        // with a gateway terrain too short of villages to start players in, so the
        // world is not worth playing and Game::NewGame() throws it away. Stays true
        // on levels that never run the surface generator, so it is safe to read
        // unconditionally.
        bool settlements_ok = true;

        void create_island_ring_level(int level, int xSize, int ySize, const std::string& name);
        void create_island_level(int level, int nPlayers, const std::string& name);
        void create_underworld_level(int level, int xSize, int ySize, const std::string& name);
        void create_underworld_ring_level(int level, int xSize, int ySize, const std::string& name);
        void create_underdeep_level(int level, int xSize, int ySize, const std::string& name);
        void create_dungeon_level(int level, int xSize, int ySize, const std::string& name);
        void expand_levels(int newNumLevels);
        void add_dungeon_level_to_existing_world(int xSize, int ySize);

        // cap_by_destination: derive maxShafts from the smaller of the two levels
        // rather than the source. Wanted for underground transitions, where what
        // matters is how permeable the level below is; wrong for the surface link,
        // whose shaft count is deliberately tied to its own settlement density.
        void CreateSmartShafts(int levelFrom, int levelTo, int minDistanceSame,
                               int minDistanceStair, int seeds = 1,
                               bool cap_by_destination = false);
        void CreateLairsAtShafts(int level);

        void MakeShaftLinks(int levelFrom, int levelTo, int odds);
        void SetACNeighbors(int levelSrc, int levelTo, int maxX, int maxY);
        ARegion *FindConnectedRegions(ARegion *r, ARegion *tail, int shaft);
        ARegion *FindNearestStartingCity(ARegion *r, int *dir);
        int FindDistanceToNearestObject(int object, ARegion *r);
        int find_distance_between_regions(ARegion *start, ARegion *target);
        void FixUnconnectedRegions();
        void InitSetupGates(int level);
        void FinalSetupGates();

        // JR
        void InitGeographicMap(ARegionArray *pRegs);
        void CleanUpWater(ARegionArray *pRegs);
        void RemoveCoastalLakes(ARegionArray *pRegs);
        void SeverLandBridges(ARegionArray *pRegs);
        void RescaleFractalParameters(ARegionArray *pArr);
        void SetFractalTerrain(ARegionArray *pArr);
        void NameRegions(ARegionArray *pArr);
        void UnsetRace(ARegionArray *pRegs);
        void RaceAnchors(ARegionArray *pRegs);
        void GrowRaces(ARegionArray *pRegs);

        void TownStatistics();
        void ResourcesStatistics();
        void NameStatistics();

        // Generation-time tuning report; see GENERATION_TUNING_STATS.
        void MapStatistics();
        void report_level(ARegionArray *arr, int level);
        void report_landmasses(ARegionArray *arr, int level);

        void CalcDensities();
        int GetLevelXScale(int level);
        int GetLevelYScale(int level);

        void AddHistoricalBuildings(ARegionArray* arr, const int w, const int h, Map* map);

    private:
        //
        // Private world creation stuff
        //
        void MakeRegions(int level, int xSize, int ySize);
        void SetupNeighbors(ARegionArray *pRegs);
        void NeighSetup(ARegion *r, ARegionArray *ar);
        void MakeIcosahedralRegions(int level, int xSize, int ySize);
        void SetupIcosahedralNeighbors(ARegionArray *pRegs);
        void IcosahedralNeighSetup(ARegion *r, ARegionArray *ar);

        void SetRegTypes(ARegionArray *pRegs, int newType);
        void MakeLand(ARegionArray *pRegs, int percentOcean, int continentSize);
        void MakeCentralLand(ARegionArray *pRegs);
        void MakeRingLand(ARegionArray *pRegs, int minDistance, int maxDistance);

        void SetupAnchors(ARegionArray *pArr);
        void GrowTerrain(ARegionArray *pArr, int growOcean, bool generateLakes = true);
        void RandomTerrain(ARegionArray *pArr);
        void MakeUWMaze(ARegionArray *pArr);
        void PlaceVolcanos(ARegionArray *pArr);
        void MakeIslands(ARegionArray *pArr, int nPlayers);
        void MakeOneIsland(ARegionArray *pRegs, int xx, int yy);

        void AssignTypes(ARegionArray *pArr);
        void FinalSetup(ARegionArray *);

        void MakeShaft(ARegion *reg, ARegionArray *pFrom, ARegionArray *pTo);

        //
        // Game-specific world stuff (see world.cpp)
        //
        int GetRegType(ARegion *pReg);
        int CheckRegionExit(ARegion *pFrom, ARegion *pTo);

};

int parse_terrain(const strings::ci_string& token);

using ARegionCostFunction = std::function<double(ARegion*, ARegion*)>;
using ARegionInclusionFunction = std::function<bool(ARegion*, ARegion*)>;

class ARegionGraph : public graphs::Graph<graphs::Location2D, ARegion*> {
public:
    ARegionGraph(ARegionArray* regions);
    ~ARegionGraph();

    ARegion* get(graphs::Location2D id);
    std::vector<graphs::Location2D> neighbors(graphs::Location2D id);
    double cost(graphs::Location2D current, graphs::Location2D next);

    void setCost(ARegionCostFunction costFn);
    void setInclusion(ARegionInclusionFunction includeFn);

private:
    ARegionArray* regions;
    ARegionCostFunction costFn;
    ARegionInclusionFunction includeFn;
};

const std::unordered_map<ARegion*, graphs::Node<ARegion*>> breadthFirstSearch(ARegion* start, const int maxDistance);

#endif // REGION_H
