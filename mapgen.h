// START A3HEADER
//
// This source file is part of the Atlantis PBM game program.
// Copyright (C) 2022 Valdis Zobēla
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program, in the file license.txt. If not, write
// to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
//
// See the Atlantis Project web page for details:
// http://www.prankster.com/project
//
// END A3HEADER

#pragma once

#include "graphs.h"

#include <vector>
#include <functional>

#define B_UNKNOWN        0
#define B_TUNDRA         1
#define B_MOUNTAINS      2
#define B_SWAMP          3
#define B_FOREST         4
#define B_PLAINS         5
#define B_JUNGLE         6
#define B_DESERT         7
#define B_WATER          8
#define B_HILLS          9
#define B_COUNT          10

struct Vapor {
    int echelon;
    double amount;
};

struct Edge {
    int dx;
    int dy;
    double weight;
};

struct Range {
    int min;
    int max;

    bool in(const int value) const;
};

struct Cell {
    int index;
    int x;
    int y;
    int biome;
    int elevation;
    int temperature;
    int saturation;
    int evoparation;
    int rainfall;

    // int moistureIn;
    // int moistureOut;
    // double saturation;
    // std::vector<Vapor> moistureIn;
    // std::vector<Vapor> moistureOut;
};

struct Biome {
    int name;
    double feritality;
    Range temp;
    Range rainfall;
    
    bool match(const Cell* cell) const;
};

class CellMap {
public:
    CellMap(int width, int height);
    ~CellMap();

    int width;
    int height;
    std::vector<Cell*> items;

    Cell* get(int x, int y);
    int index(int x, int y);
    void coords(int index, int& x, int& y);
    bool normalize(int& x, int& y);
};

struct Blob {
    std::vector<Cell*> items;
    int biome;

    bool includes(Cell* cell);
    bool add(Cell* cell);
    void add(Blob* blob);
};

using CellInclusionFunction = std::function<bool(Cell*, Cell*)>;

class CellGraph : public graphs::Graph<int, Cell*> {
public:
    CellGraph(CellMap* map);
    ~CellGraph();

    Cell* get(int id);
    std::vector<int> neighbors(int id);
    double cost(int current, int next);

    void setInclusion(CellInclusionFunction includeFn);

private:
    CellInclusionFunction includeFn;
    CellMap* map;

    void addCell(Cell* current, int dx, int dy, std::vector<int>& list);
};


class Map {
public:
    Map(int width, int height);

    int minTemp;
    int maxTemp;
    double evoparation;
    double redistribution;
    double frequency;
    double amplitude;

    // Fractal sum: each next octave is lacunarity times finer and carries
    // persistence times the weight of the previous one. Lower persistence
    // leaves the largest shape dominant; higher persistence adds roughness
    // at intermediate scales.
    int    octaves;        // fractal octaves for the continental noise
    double lacunarity;     // frequency multiplier between octaves
    double persistence;    // amplitude retained by each next octave
    double waterPercent;
    double mountainPercent;
    double hillPercent;
    double lakePercent;

    // Historical buildings generation (world creation)
    bool generateHistoricalRoads;              // Generate ancient roads between cities
    bool generateHistoricalProductionBuildings; // Generate ruined production buildings for basic resources

    // Polar archipelago parameters (configurable in world.cpp)
    double polarLatitudeStart;    // Latitude where island effect begins (degrees, 60-75)
    double polarIslandBlend;      // Strength of island fragmentation (0.0-1.0)
    double polarElevationRedux;   // Elevation reduction at poles (0.0-1.0, higher = more ocean)

    // Latitude-profile mask parameters (configurable in world.cpp)
    double landEdgeLatitude;   // Latitude where the land ends toward the equator (degrees, absolute)
    double equatorSeaDepth;    // How far the equator is pushed down (0.0-1.0)
    double maskStrength;       // Blend weight of the profile against the noise (0.0-1.0, 0 = off)
    double maskLatJitter;      // Latitude jitter of the band edge (degrees, 0 = off)
    double maskLongVariance;   // Longitudinal variance of mask strength (0.0-1.0, 0 = off)
    double maskBandRelease;    // Band release of the mask inside the land band (0.0-1.0, 0 = off)

    CellMap map;

    double latitude_profile(double lat) const;
    void Generate();
};