#include "mapgen.h"
#include "simplex.h"
#include "logger.hpp"
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <iostream>
#include <string>
#include <cmath>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#ifndef M_E
    #define M_E 2.7182818284590452354
#endif

// #define T_UNKNOWN        0
// #define T_LAKE           2
// #define T_RIVER          3
// #define T_VOLKANO        5

const int MIN_TEMP = -1000;
const int MAX_TEMP = 1000;

const int MAX_RAINFALL = 1000;

const std::vector<Biome> BIOMES = {
    // -10
    { .name = B_TUNDRA, .feritality = 0.8, .temp = { -1000,    0 }, .rainfall = {   0,  50 } },
    { .name = B_FOREST, .feritality = 0.8, .temp = { -1000,    0 }, .rainfall = { 51, 1000 } },

    // 0
    { .name = B_TUNDRA, .feritality = 1.5, .temp = {     1,   10 }, .rainfall = {   0,   50 } },
    { .name = B_PLAINS, .feritality = 0.8, .temp = {     1,   10 }, .rainfall = {  51,  200 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {     1,   10 }, .rainfall = { 201, 1000 } },

    // 10
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    11,   15 }, .rainfall = {   0,  200 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {    11,   15 }, .rainfall = { 201,  800 } },
    { .name = B_SWAMP,  .feritality = 1.0, .temp = {    11,   15 }, .rainfall = { 801, 1000 } },

    // 15
    { .name = B_DESERT, .feritality = 1.2, .temp = {    16,   20 }, .rainfall = {   0,  100 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    16,   20 }, .rainfall = { 101,  250 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {    16,   20 }, .rainfall = { 251,  600 } },
    { .name = B_JUNGLE, .feritality = 1.0, .temp = {    16,   30 }, .rainfall = { 601,  800 } },
    { .name = B_SWAMP,  .feritality = 1.0, .temp = {    16,   20 }, .rainfall = { 801, 1000 } },

    // 20
    { .name = B_DESERT, .feritality = 1.2, .temp = {    21,   30 }, .rainfall = {   0,  150 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    21,   30 }, .rainfall = { 151,  250 } },
    { .name = B_FOREST, .feritality = 1.2, .temp = {    21,   30 }, .rainfall = { 251,  400 } },
    { .name = B_JUNGLE, .feritality = 1.2, .temp = {    21,   30 }, .rainfall = { 401,  800 } },
    { .name = B_SWAMP,  .feritality = 1.2, .temp = {    21,   30 }, .rainfall = { 801, 1000 } },

    // 30
    { .name = B_DESERT, .feritality = 1.0, .temp = {    31,   40 }, .rainfall = {   0,  150 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    31,   40 }, .rainfall = { 151,  250 } },
    { .name = B_FOREST, .feritality = 1.2, .temp = {    31,   40 }, .rainfall = { 251,  400 } },
    { .name = B_JUNGLE, .feritality = 1.2, .temp = {    31,   40 }, .rainfall = { 401,  800 } },
    { .name = B_SWAMP,  .feritality = 1.0, .temp = {    31,   40 }, .rainfall = { 801, 1000 } },

    // 40
    { .name = B_DESERT, .feritality = 1.0, .temp = {    41,   50 }, .rainfall = {   0,  150 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    41,   50 }, .rainfall = { 151,  300 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {    41,   50 }, .rainfall = { 301,  400 } },
    { .name = B_JUNGLE, .feritality = 1.0, .temp = {    41,   50 }, .rainfall = { 401, 1000 } },

    // 50
    { .name = B_DESERT, .feritality = 1.0, .temp = {    51,   60 }, .rainfall = {   0,  300 } },
    { .name = B_PLAINS, .feritality = 1.2, .temp = {    51,   60 }, .rainfall = { 301,  400 } },
    { .name = B_JUNGLE, .feritality = 1.0, .temp = {    51,   60 }, .rainfall = { 401, 1000 } },

    // 60
    { .name = B_DESERT, .feritality = 1.0, .temp = {    61, 1000 }, .rainfall = {   0,  400 } },
    { .name = B_PLAINS, .feritality = 1.2, .temp = {    61, 1000 }, .rainfall = { 401,  500 } },
    { .name = B_JUNGLE, .feritality = 1.0, .temp = {    61, 1000 }, .rainfall = { 501, 1000 } },
};

bool Range::in(const int value) const {
    return value >= min && value <= max;
}

bool Biome::match(const Cell* cell) const {
    return temp.in(cell->temperature) && rainfall.in(cell->rainfall);
}

CellGraph::CellGraph(CellMap* map) {
    this->map = map;
}

CellGraph::~CellGraph() {

}

Cell* CellGraph::get(int id) {
    return map->items[id];
}

std::vector<int> CellGraph::neighbors(int id) {
    Cell* current = map->items[id];

    std::vector<int> list;

    addCell(current,  0, -1, list);
    addCell(current,  1, -1, list);
    addCell(current,  1,  0, list);
    addCell(current,  1,  1, list);
    addCell(current,  0,  1, list);
    addCell(current, -1,  1, list);
    addCell(current, -1,  0, list);
    addCell(current, -1, -1, list);

    return list;
}

double CellGraph::cost(int current, int next) {
    return 1;
}

void CellGraph::setInclusion(CellInclusionFunction includeFn) {
    this->includeFn = includeFn;
}

void CellGraph::addCell(Cell* current, int dx, int dy, std::vector<int>& list) {
    Cell* n = map->get(current->x + dx, current->y + dy);
    if (n && includeFn(current, n)) {
        list.push_back(n->index);
    }
}

CellMap::CellMap(int width, int height) {
    this->width = width;
    this->height = height;

    int len = width * height;
    items.reserve(len);

    for (int i = 0; i < len; i++) {
        int x;
        int y;
        coords(i, x, y);

        items.push_back(new Cell({
            .index = i,
            .x = x,
            .y = y,
            .biome = B_UNKNOWN,
            .elevation = 0,
            .temperature = 0,
            .saturation = 0,
            .evoparation = 0,
            .rainfall = 0,
        }));
    }
}

CellMap::~CellMap() {
    for (auto item : items) {
        delete item;
    }
}

Cell* CellMap::get(int x, int y) {
    if (!normalize(x, y)) {
        return NULL;
    }

    return items[index(x, y)];
}

bool CellMap::normalize(int& x, int& y) {
    if (y < 0 || y >= height) {
        return false;
    }

    if (x < 0) {
        x += width;
    }

    if (x >= width) {
        x = x % width;
    }

    return true;
}

int CellMap::index(int x, int y) {
    return x + y * width;
}

void CellMap::coords(int index, int& x, int& y) {
    y = index / width;
    x = index % width;
}

bool Blob::includes(Cell* cell) {
    return std::find(std::begin(items), std::end(items), cell) != std::end(items);
}

bool Blob::add(Cell* cell) {
    if (includes(cell)) {
        return false;
    }

    items.push_back(cell);

    return true;
}

void Blob::add(Blob* blob) {
    for (auto item : blob->items) {
        items.push_back(item);
    }
}

const double T_K = 273.0;

double saturation(int temperature) {
    double T = temperature + T_K;
    double S = pow(M_E, 77.345 + 0.0057 * T - 7235.0 / T) / pow(T, 8.2);

    // if (temperature < 0) {
    //     S = pow(S, -temperature / 16.0 + 1.0);
    // }

    return S;
}

double density(double saturation, int temperature) {
    double T = temperature + T_K;
    double P = (0.0022 * saturation) / T;

    return P;
}

const double TEMP_ALT_CHANGE = 6.0 / 1000;  // temperature changes by 6°C every 1000m

double temperature(double minTemp, double maxTemp, double axialTiltRad, double latitudeRad, int elevation) {
    double tempRange = maxTemp - minTemp;

    double tR = cos((latitudeRad - axialTiltRad) / 1.5);
    double tE = elevation * TEMP_ALT_CHANGE;
    double t = tempRange * tR - tE + minTemp;

    return t;
}

const double RAD = M_PI / 180.0;

inline double degToRad(double deg) {
    return deg * RAD;
}

// static const Edge LEFT_WIND[MOISTURE_SOURCES] = {
//     {  0,  1, 0.1 },
//     { -1,  1, 0.2 },
//     { -1,  0, 0.7 },
//     { -1, -1, 0.2 },
//     {  0, -1, 0.1 }
// };

// static const Edge RIGHT_WIND[MOISTURE_SOURCES] = {
//     { 0,  1, 0.1 },
//     { 1,  1, 0.2 },
//     { 1,  0, 0.7 },
//     { 1, -1, 0.2 },
//     { 0, -1, 0.1 }
// };

const int MOISTURE_SOURCES = 8;
const Edge WIND[MOISTURE_SOURCES] = {
    { -1, -1, 0.20 },
    { -1,  0, 0.70 },
    { -1,  1, 0.20 },
    {  0, -1, 0.10 },
    {  0,  1, 0.10 },
    {  1, -1, 0.05 },
    {  1,  0, 0.05 },
    {  1,  1, 0.05 }
};

const double RAINFALL = 0.1;

//                  | rainfall
// -----------------+---------
// temperature up   | decrease
// temperature down | increase
// altitude up      | increase
// altitude down    | decrease

void simulateRain(CellMap& map, Cell* target, double windAngle, const Edge* windMatrix) {
    int x = target->x;
    int y = target->y;

    double rainfall = 0;

    const int RANGE = 4;
    const double MOISTURE_FADE = 4.0;
    const double MOISTURE_TO_MM = 20.0;

    for (int dx = -RANGE; dx <= RANGE; dx++) {
        for (int dy = -RANGE; dy <= RANGE; dy++) {
            if (dx == 0 && dy == 0) {
                continue;
            }

            auto cell = map.get(x + dx, y + dy);
            if (cell == NULL) {
                continue;
            }

            int distance = sqrt(dx * dx + dy * dy);
            if (distance > RANGE) {
                continue;
            }

            if (!cell->evoparation) {
                continue;
            }

            double moisture = cell->evoparation * (MOISTURE_FADE / distance);

            int dE = target->elevation <= 0
                ? 0
                : std::max(0, target->elevation) - std::max(0, cell->elevation);

            double rain = 0;
            if (moisture > 0) {
                if (dE > 0) {
                    // Orographic rainfall (mountains force air up, causing rain)
                    rain = (dE * RAINFALL * moisture) / 400.0;
                } else {
                    // Base rainfall on flat terrain (aggressive increase to ensure forests/jungles)
                    // Flat coastal areas now get substantial rainfall from ocean moisture
                    rain = (RAINFALL * moisture) / 800.0;
                }
            }

            rainfall += rain;
        }
    }

    target->rainfall = std::min(MAX_RAINFALL, (int) round(rainfall / MOISTURE_TO_MM));
}

Map::Map(int width, int height) : map(CellMap(width, height)) {
    minTemp = 0;
    maxTemp = 60;

    frequency = 5.0;
    amplitude = 0.5;
    redistribution = 1.0;
    evoparation = 1.0;

    waterPercent = 0.2;
    mountainPercent = 0.2;
    hillPercent = 0.50;
    lakePercent = 0.15;  // 15% chance for lake placement

    // Historical buildings defaults
    generateHistoricalRoads = true;
    generateHistoricalProductionBuildings = true;

    // Polar archipelago defaults
    polarLatitudeStart = 70.0;
    polarIslandBlend = 0.5;
    polarElevationRedux = 0.35;
}

Blob* fillByElevation(CellMap* map, Cell* start, int biome, Range elevation) {
    Blob* blob = new Blob();
    blob->biome = biome;

    CellGraph graph = CellGraph(map);
    graph.setInclusion([ elevation ](Cell* current, Cell* next) {
        return elevation.in(next->elevation) && next->biome == B_UNKNOWN;
    });

    auto result = graphs::breadthFirstSearch(graph, start->index);

    for (auto kv : result) {
        auto cell = graph.get(kv.first);
        cell->biome = biome;
        blob->add(cell);
    }

    return blob;
}

Blob* fillByBiome(CellMap* map, Cell* start, const Biome* biome) {
    Blob* blob = new Blob();
    blob->biome = biome->name;

    CellGraph graph = CellGraph(map);
    graph.setInclusion([ biome ](Cell* current, Cell* next) {
        return biome->match(next) && next->biome == B_UNKNOWN;
    });

    auto result = graphs::breadthFirstSearch(graph, start->index);

    for (auto kv : result) {
        auto cell = graph.get(kv.first);
        cell->biome = biome->name;
        blob->add(cell);
    }

    return blob;
}

void Map::Generate() {
    int len = map.width * map.height;

    SimplexNoise* noise = new SimplexNoise(frequency, amplitude);
    std::vector<Blob*> blobs;

    // Create high-frequency noise for polar archipelago effect
    SimplexNoise* islandNoise = new SimplexNoise(frequency * 4.0, amplitude);

    // 0. elevation
    const int ELEVATION = 16000;    // elevation range is 16km
    int minElevation = ELEVATION;
    int maxElevation = 0;
    std::map<int, int> hist;
    const double halfHeight = map.height / 2.0;

    for (int i = 0; i < len; i++) {
        auto cell = map.items[i];

        double nx = (double) cell->x / map.width;
        double ny = (double) cell->y / map.height;

        // Base elevation from main noise (creates continents)
        double e = pow((noise->cylinderFractal(3, nx, ny) + 1.0) / 2.0, redistribution);

        // Polar archipelago effect: fragment land into islands at high latitudes
        double lat = std::abs(((halfHeight - cell->y) / halfHeight) * 90.0);  // 0-90°

        if (lat > polarLatitudeStart && polarLatitudeStart < 89.0) {
            // Calculate polar blending factor (0.0 at polarLatitudeStart, 1.0 at pole)
            double polarAmount = (lat - polarLatitudeStart) / (90.0 - polarLatitudeStart);

            // High-frequency noise creates small-scale terrain variation (islands)
            double islandDetail = (islandNoise->cylinderFractal(2, nx, ny) + 1.0) / 2.0;

            // Smooth blend between continental and island terrain
            // polarAmount = 0.0 → pure continents
            // polarAmount = 1.0 → archipelago effect at maximum
            double blendFactor = polarAmount * polarIslandBlend;
            e = e * (1.0 - blendFactor) + islandDetail * blendFactor;

            // ADDITIONALLY: Lower polar elevation to increase ocean at poles
            // This creates more water between islands
            double elevationReduction = polarAmount * polarElevationRedux;
            e *= (1.0 - elevationReduction);
        }

        cell->elevation = round(e * ELEVATION);
        minElevation = std::min(minElevation, cell->elevation);
        maxElevation = std::max(maxElevation, cell->elevation);

        ++hist[cell->elevation];
    }

    delete islandNoise;

    // 1. determine sea level
    logger::write("1. determine sea level");

    int maxWaterCells = len * waterPercent;
    int waterCells = 0;
    int seaLevel = minElevation;

    for (auto &kv : hist) {
        if (waterCells >= maxWaterCells) {
            break;
        }
        seaLevel = kv.first;
        waterCells += kv.second;
    }

    // 2. determine hill and mountain levels
    // First, find the total number of cells that should be mountainous (hills + mountains).
    int targetMountainousCells = len * mountainPercent;

    // Find the base level for all elevated terrain (hills and mountains)
    int mountainousCellsFound = 0; // Tracks cells from the top down
    int baseMountainLevel = maxElevation;
    for (auto iter = hist.rbegin(); iter != hist.rend(); ++iter) {
        if (iter->first <= seaLevel) continue; // Skip water cells

        mountainousCellsFound += iter->second;
        if (mountainousCellsFound >= targetMountainousCells) {
            baseMountainLevel = iter->first;
            break;
        }
    }

    // Now, count the ACTUAL number of cells at or above baseMountainLevel (but not water)
    int actualMountainousCellsInBlock = 0;
    for (auto const& [elevation, count] : hist) {
        if (elevation >= baseMountainLevel && elevation > seaLevel) {
            actualMountainousCellsInBlock += count;
        }
    }

    // Then, within this block, find the threshold that separates hills from mountains
    int hillCellTarget = (int)(actualMountainousCellsInBlock * hillPercent);
    int hillCellsFound = 0;
    int mountainLevel = baseMountainLevel; // Initialize with base, will move up if hills exist

    // Iterate from baseMountainLevel upwards to find the split
    for (auto const& [elevation, count] : hist) {
        if (elevation < baseMountainLevel || elevation <= seaLevel) continue; // Only consider within the mountainous block, above sea level

        hillCellsFound += count;
        if (hillCellsFound >= hillCellTarget) {
            mountainLevel = elevation + 1; // The next level up is a mountain
            break;
        }
    }


    for (auto item : map.items) {
        if (item->biome != B_UNKNOWN) {
            continue;
        }

        if (item->elevation <= seaLevel) {
            auto blob = fillByElevation(&map, item, B_WATER, { minElevation, seaLevel });
            blobs.push_back(blob);
        } else if (item->elevation >= mountainLevel) {
            auto blob = fillByElevation(&map, item, B_MOUNTAINS, { mountainLevel, maxElevation });
            blobs.push_back(blob);
        } else if (item->elevation >= baseMountainLevel) {
            auto blob = fillByElevation(&map, item, B_HILLS, { baseMountainLevel, mountainLevel - 1 });
            blobs.push_back(blob);
        }
    }


    // 3. temperature and moisture saturation
    logger::write("3. temperature and moisture saturation");

    for (auto item : map.items) {
        item->elevation -= seaLevel;

        bool isWater = item->biome == B_WATER;
        double lat = ((halfHeight - item->y) / halfHeight) * 90.0;
        int tempElevation = isWater ? 0 : item->elevation;


        item->temperature = round(temperature(minTemp, maxTemp, degToRad(/* 23.5 */ 0), degToRad(lat), tempElevation));
        item->temperature = std::min(MAX_TEMP, std::max(MIN_TEMP, item->temperature));
        item->saturation = round(saturation(item->temperature));

        if (isWater) {
            item->evoparation = round(pow(item->saturation, evoparation));
        }
        else {
            item->evoparation = 0;
        }
    }


    // 4. rain
    logger::write("4. rain");

    /*
        Each cell can produce certain amount of moisture via evaporation.
        Wind moves evoparation from neigbour regions according to the wind direction (see table above).
        Incoming moisture combines with local evoparation.
        Certain amount of moisture will fall of as a rain.
        Remaining moisture will move to the neighbour regions.

        Elevation change impacts wind and rainfall. High mountains will create wind barier, and rain shadow
        from the opostie side of the mountain.
    */

    for (int i = 0; i < 2; i++) {
        for (int x = 0; x < map.width; x++) {
            for (int y = 0; y < map.height; y++) {
                auto target = map.get(x, y);

                simulateRain(map, target, 0.0, WIND);
            }
        }
    }

    // 5. biomes
    logger::write("5. biomes");

    // assign biomes
    for (auto item : map.items) {
        if (item->biome != B_UNKNOWN) {
            continue;
        }

        // find biome
        const Biome* biome = NULL;
        for (auto &b : BIOMES) {
            if (b.match(item)) {
                biome = &b;
                break;
            }
        }

        if (biome == NULL) {
            logger::write(
                "NO BIOME for [" + std::to_string(item->x) + ", " + std::to_string(item->y) + "]" + " " +
                std::to_string(item->elevation) + "m" + ", " + std::to_string(item->temperature) + "°C" +
                ", " + std::to_string(item->rainfall) + "mm"
            );

            exit(1);
        }

        auto blob = fillByBiome(&map, item, biome);
        blobs.push_back(blob);
    }

    int unknown = 0;
    for (auto item : map.items) {
        if (item->biome == B_UNKNOWN) {
            unknown++;

            const Biome* biome = NULL;
            for (auto &b : BIOMES) {
                if (b.match(item)) {
                    biome = &b;
                    break;
                }
            }

            logger::write(
                std::to_string(biome->name) + " [" + std::to_string(item->x) + ", " + std::to_string(item->y) + "]" +
                " " + std::to_string(item->elevation) + "m" + ", " + std::to_string(item->temperature) + "°C" + ", " +
                std::to_string(item->rainfall) + "mm"
            );
        }
    }

    logger::write(std::to_string(unknown) + " B_UNKNOWN");

    logger::write("DONE");
}
