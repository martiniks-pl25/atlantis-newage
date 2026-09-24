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

// Rainfall is not spread evenly over its 0..1000 scale, so a window's width is a
// poor guide to how much land it will take. Measured over 389 worlds: a third of
// all lowland sits below rainfall 100, and 11% is pinned at the 1000 clamp
// (mapgen.cpp, simulateRain caps at MAX_RAINFALL). The desert/plains boundary in
// the three hot bands therefore decides a very large share of the map, and at 50
// it cut at the 16th percentile - desert came out at half the share of the other
// terrains. Moved to 80 (the 27th percentile), which balances the two against
// each other; the wetter windows are untouched because moving them costs far
// less ground per unit.
const std::vector<Biome> BIOMES = {
    // -10
    { .name = B_TUNDRA, .feritality = 0.8, .temp = { -1000,    0 }, .rainfall = {   0,  50 } },
    { .name = B_FOREST, .feritality = 0.8, .temp = { -1000,    0 }, .rainfall = { 51, 1000 } },

    // 0
    { .name = B_TUNDRA, .feritality = 1.5, .temp = {     1,   10 }, .rainfall = {   0,  120 } },
    { .name = B_PLAINS, .feritality = 0.8, .temp = {     1,   10 }, .rainfall = { 121,  200 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {     1,   10 }, .rainfall = { 201, 1000 } },

    // 10
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    11,   15 }, .rainfall = {   0,  200 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {    11,   15 }, .rainfall = { 201,  870 } },
    { .name = B_SWAMP,  .feritality = 1.0, .temp = {    11,   15 }, .rainfall = { 871, 1000 } },

    // 15
    { .name = B_DESERT, .feritality = 1.2, .temp = {    16,   20 }, .rainfall = {   0,   60 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    16,   20 }, .rainfall = {  61,  250 } },
    { .name = B_FOREST, .feritality = 1.0, .temp = {    16,   20 }, .rainfall = { 251,  600 } },
    { .name = B_JUNGLE, .feritality = 1.0, .temp = {    16,   30 }, .rainfall = { 601,  870 } },
    { .name = B_SWAMP,  .feritality = 1.0, .temp = {    16,   20 }, .rainfall = { 871, 1000 } },

    // 20
    { .name = B_DESERT, .feritality = 1.2, .temp = {    21,   30 }, .rainfall = {   0,   80 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    21,   30 }, .rainfall = {  81,  250 } },
    { .name = B_FOREST, .feritality = 1.2, .temp = {    21,   30 }, .rainfall = { 251,  400 } },
    { .name = B_JUNGLE, .feritality = 1.2, .temp = {    21,   30 }, .rainfall = { 401,  870 } },
    { .name = B_SWAMP,  .feritality = 1.2, .temp = {    21,   30 }, .rainfall = { 871, 1000 } },

    // 30
    { .name = B_DESERT, .feritality = 1.0, .temp = {    31,   40 }, .rainfall = {   0,   80 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    31,   40 }, .rainfall = {  81,  250 } },
    { .name = B_FOREST, .feritality = 1.2, .temp = {    31,   40 }, .rainfall = { 251,  400 } },
    { .name = B_JUNGLE, .feritality = 1.2, .temp = {    31,   40 }, .rainfall = { 401,  870 } },
    { .name = B_SWAMP,  .feritality = 1.0, .temp = {    31,   40 }, .rainfall = { 871, 1000 } },

    // 40
    { .name = B_DESERT, .feritality = 1.0, .temp = {    41,   50 }, .rainfall = {   0,   80 } },
    { .name = B_PLAINS, .feritality = 1.0, .temp = {    41,   50 }, .rainfall = {  81,  300 } },
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

static const char* biomeName(int biome) {
    switch (biome) {
        case B_TUNDRA:    return "tundra";
        case B_MOUNTAINS: return "mountains";
        case B_SWAMP:     return "swamp";
        case B_FOREST:    return "forest";
        case B_PLAINS:    return "plains";
        case B_JUNGLE:    return "jungle";
        case B_DESERT:    return "desert";
        case B_WATER:     return "water";
        case B_HILLS:     return "hills";
        default:          return "unknown";
    }
}

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

/**
 * @brief Target elevation for the latitude-profile mask at an absolute latitude.
 *
 * A designer-controlled bias blended into the noise field before sea level is
 * chosen. The shape is a single-edge land plateau: 1.0 for every latitude at or
 * above landEdgeLatitude, running all the way to the pole, then falling with a
 * cosine smoothstep toward the equator to (1 - equatorSeaDepth) at the equator.
 * The poles are not shaped here at all; polar drowning belongs to the polar
 * block's polarElevationRedux, which stays independent of maskStrength and
 * multiplies the elevation rather than pulling it toward a flat target. The
 * smoothstep has zero slope at the land edge, so there is no hard step or kink
 * where the plateau meets the transition.
 *
 * landEdgeLatitude sets both WHERE the land ends on the equatorial side and HOW
 * LONG the slope to the equator is: the slope spans the whole 0..landEdgeLatitude
 * range, so a lower edge makes the descent to the equator steeper as well as
 * wider.
 *
 * Formula (lat is absolute 0..90):
 *   lat >= landEdgeLatitude : profile = 1.0
 *   lat <  landEdgeLatitude : profile = (1 - equatorSeaDepth) + equatorSeaDepth * s(lat / landEdgeLatitude)
 *   where s(t) = 0.5 - 0.5 * cos(pi * t) is the cosine smoothstep (0 at t=0, 1 at t=1).
 *
 * @param lat Absolute latitude in degrees, 0..90.
 * @return Target elevation in 0..1.
 * @note Depends on latitude only, so the cylinder wrap in X is preserved by
 *       construction. The caller (Map::Generate) may pass a longitude-warped
 *       latitude; this function itself stays a pure function of its argument.
 */
double Map::latitude_profile(double lat) const {
    const double edge = std::max(0.0, std::min(90.0, landEdgeLatitude));

    if (lat < edge) {
        double t = (edge > 0.0) ? lat / edge : 1.0;
        double s = 0.5 - 0.5 * cos(M_PI * t);
        return (1.0 - equatorSeaDepth) + equatorSeaDepth * s;
    }
    return 1.0;
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
    // Not prompted. Amplitude cancels out: the fractal sum is divided by the sum
    // of the octave amplitudes. Redistribution is the exponent applied to the
    // noise before the mask blend: it leaves terrain shares alone (sea level and
    // mountains are percentiles) but sets how strongly the mask out-competes the
    // noise, so it stays fixed at the value every measurement was taken with.
    amplitude = 0.6;
    redistribution = 2.0;
    octaves = 3;
    lacunarity = 2.0;
    persistence = 0.5;
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
    polarElevationRedux = 0.35;

    // Latitude-profile mask defaults (maskStrength 0.0 = mask disabled)
    landEdgeLatitude = 33.0;
    equatorSeaDepth = 0.45;
    maskStrength = 0.0;
    maskLatJitter = 0.0;
    maskBandRelease = 0.0;
    noiseAspectCorrection = 1.0;
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

    SimplexNoise* noise = new SimplexNoise(frequency, amplitude, lacunarity, persistence);
    std::vector<Blob*> blobs;

    // Latitude-profile mask noise fields. Each is an independent SimplexNoise
    // instance (its constructor shuffles its own permutation table), so none
    // correlates with the elevation noise. They use a low frequency and few
    // octaves so the warps are broad wanders, not fine fuzz, and are constructed
    // only when their parameter is active, so switching a feature off does not
    // shift the world's RNG stream.
    SimplexNoise* latJitterNoise = nullptr;
    if (maskStrength > 0.0 && maskLatJitter > 0.0) {
        latJitterNoise = new SimplexNoise(1.0, amplitude, lacunarity, persistence);
    }

    // 0. elevation
    const int ELEVATION = 16000;    // elevation range is 16km
    int minElevation = ELEVATION;
    int maxElevation = 0;
    std::map<int, int> hist;
    const double halfHeight = map.height / 2.0;
    // Regions sample only the EVEN cell rows (each region reads the top-left
    // cell of its 2x2 block, and the loop skips (x+y)%2), so the span must be
    // measured across that sampled subset, not across the full grid: with -1.0
    // the sampled y=0 gives +90 and the last sampled row gives -90, keeping the
    // two poles symmetric. The unsampled odd row below is clamped at the two
    // latitude sites below.
    const double halfSpan = halfHeight - 1.0;

    // Noise aspect. The noise is sampled on a unit square (a cylinder of
    // circumference 1 by a height of 1), but the hex map it lands on is not
    // square: regions exist only where x + y is even, so a column holds H/2
    // hexes, and neighbouring columns sit sqrt(3)/2 of a hex apart. The map is
    // therefore W * sqrt(3)/2 wide by H/2 tall - 1.73 : 1 for a square region
    // grid - and a round noise blob came out 1.73 times wider than tall, which
    // is what stretched landmasses into east-west strips. Dividing the noise's
    // y by that ratio makes one noise unit the same distance in both directions.
    // map.width / map.height equals the region grid's W / H (both are doubled).
    // noiseAspectCorrection scales how much of that ratio is taken out: 1 leaves
    // blobs round, 0 keeps the old east-west stretch, and values between give a
    // milder stretch of aspect^(1 - correction).
    const double noiseAspect = pow(std::sqrt(3.0) * map.width / map.height,
                                   std::max(0.0, std::min(1.0, noiseAspectCorrection)));

    for (int i = 0; i < len; i++) {
        auto cell = map.items[i];

        double nx = (double) cell->x / map.width;
        double ny = (double) cell->y / map.height / noiseAspect;  // noise space, see above

        // Base elevation from main noise (creates continents)
        double e = pow((noise->cylinderFractal(octaves, nx, ny) + 1.0) / 2.0, redistribution);

        double lat = std::abs(((halfSpan - cell->y) / halfSpan) * 90.0);  // 0-90°
        lat = std::min(lat, 90.0);  // clamp: the odd row below the sampled subset overshoots the pole

        // Latitude-profile mask: bias the elevation field toward the designer's
        // macro-geography (open polar seas, a mid-latitude land band, an
        // equatorial sea). This runs BEFORE the polar block so polar
        // fragmentation operates on top of the mask rather than fighting it.
        // maskStrength == 0 reproduces the unmasked field bit-for-bit.
        if (maskStrength > 0.0) {
            // Latitude jitter (domain warp): shift the band edge north/south
            // with a broad low-frequency field so coasts wander with longitude
            // into bays, peninsulas and straits instead of ruler-straight rows.
            double warpedLat = lat;
            if (latJitterNoise != nullptr) {
                warpedLat += maskLatJitter * latJitterNoise->cylinderFractal(2, nx, ny);
                warpedLat = std::max(0.0, std::min(90.0, warpedLat));
            }

            double kEffective = maskStrength;

            // Band release: on the band plateau the profile is a flat 1.0, so a
            // uniform blend weight would push every cell toward the same target
            // height and flatten the noise relief. Weight the blend by how far
            // the profile pushes DOWN instead: full strength only at the deepest
            // push-down (the equator trough, where the sea must carve), and
            // released by maskBandRelease on the plateau, where the noise decides.
            // The equator is the deepest point of the profile by construction -
            // the poles are not pushed down here, that is polarElevationRedux.
            double profile = latitude_profile(warpedLat);
            double depth = 1.0 - profile;  // 0 on the plateau, larger toward the equator
            double maxDepth = equatorSeaDepth;
            double shape = (maxDepth > 0.0) ? std::min(1.0, depth / maxDepth) : 0.0;  // 0..1
            double localK = kEffective * (1.0 - maskBandRelease * (1.0 - shape));

            e = e * (1.0 - localK) + profile * localK;
        }

        // Polar block: sink the land toward the poles so an open sea lane runs
        // around each of them.
        if (lat > polarLatitudeStart && polarLatitudeStart < 89.0) {
            // Calculate polar blending factor (0.0 at polarLatitudeStart, 1.0 at pole)
            double polarAmount = (lat - polarLatitudeStart) / (90.0 - polarLatitudeStart);
            double elevationReduction = polarAmount * polarElevationRedux;
            e *= (1.0 - elevationReduction);
        }

        cell->elevation = round(e * ELEVATION);
        minElevation = std::min(minElevation, cell->elevation);
        maxElevation = std::max(maxElevation, cell->elevation);

        ++hist[cell->elevation];
    }

    delete latJitterNoise;

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
        double lat = ((halfSpan - item->y) / halfSpan) * 90.0;
        lat = std::max(-90.0, std::min(90.0, lat));  // clamp the odd row to the pole, symmetrically
        int tempElevation = isWater ? 0 : item->elevation;

        double tempValue = temperature(minTemp, maxTemp, degToRad(/* 23.5 */ 0), degToRad(lat), tempElevation);
        item->temperature = round(tempValue);
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

    // Diagnostic: report the elevation / temperature / rainfall spread that drove
    // the lowland biome assignment, so the terrain mix can be tuned against the
    // BIOMES table instead of guessed. Runs only during world creation (`new`).
    {
        std::map<int, int> entryCount;   // BIOMES index -> matched lowland cell count
        int lowland = 0;
        long elevSum = 0, tempSum = 0, rainSum = 0;
        int elevMin = 1000000, elevMax = -1000000;
        int tempMin = 1000000, tempMax = -1000000;
        int rainMin = 1000000, rainMax = -1;

        for (auto item : map.items) {
            int b = item->biome;
            if (b == B_WATER || b == B_MOUNTAINS || b == B_HILLS || b == B_UNKNOWN)
                continue;

            lowland++;
            elevSum += item->elevation;
            tempSum += item->temperature;
            rainSum += item->rainfall;
            elevMin = std::min(elevMin, item->elevation);
            elevMax = std::max(elevMax, item->elevation);
            tempMin = std::min(tempMin, item->temperature);
            tempMax = std::max(tempMax, item->temperature);
            rainMin = std::min(rainMin, item->rainfall);
            rainMax = std::max(rainMax, item->rainfall);

            // First match, mirroring the assignment loop above.
            for (size_t i = 0; i < BIOMES.size(); i++) {
                if (BIOMES[i].match(item)) {
                    entryCount[(int)i]++;
                    break;
                }
            }
        }

        logger::write("=== TUNING: biome distribution (lowland) ===");
        logger::write("lowland cells: " + std::to_string(lowland));
        if (lowland) {
            logger::write("elevation m: min " + std::to_string(elevMin) + ", mean " +
                std::to_string(elevSum / lowland) + ", max " + std::to_string(elevMax));
            logger::write("temperature C: min " + std::to_string(tempMin) + ", mean " +
                std::to_string(tempSum / lowland) + ", max " + std::to_string(tempMax));
            logger::write("rainfall: min " + std::to_string(rainMin) + ", mean " +
                std::to_string(rainSum / lowland) + ", max " + std::to_string(rainMax));
        }
        for (size_t i = 0; i < BIOMES.size(); i++) {
            auto it = entryCount.find((int)i);
            if (it == entryCount.end()) continue;
            const Biome& b = BIOMES[i];
            logger::write("  " + std::string(biomeName(b.name)) +
                "  temp[" + std::to_string(b.temp.min) + ".." + std::to_string(b.temp.max) + "]" +
                "  rain[" + std::to_string(b.rainfall.min) + ".." + std::to_string(b.rainfall.max) + "]" +
                "  : " + std::to_string(it->second));
        }

        // Rainfall histogram over lowland cells, so the BIOMES thresholds can be
        // tuned against the actual moisture distribution instead of guessed.
        {
            std::map<int, int> rainHist;
            for (auto item : map.items) {
                int b = item->biome;
                if (b == B_WATER || b == B_MOUNTAINS || b == B_HILLS || b == B_UNKNOWN)
                    continue;
                int bucket = (item->rainfall / 100) * 100;
                rainHist[bucket]++;
            }
            logger::write("rainfall histogram (lowland, 100-unit buckets):");
            for (const auto& [bucket, count] : rainHist) {
                logger::write("  " + std::to_string(bucket) + "-" +
                    std::to_string(bucket + 99) + ": " + std::to_string(count));
            }
        }
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
