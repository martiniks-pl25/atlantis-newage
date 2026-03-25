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

#include "object.h"
#include "items.h"

#include <string>

std::string getAbstractName();
std::string getEthnicName(const Ethnicity etnos);
std::string getInnName();
std::string getFortressName(const ObjectType& type);
std::string getObjectName(const int typeIndex, const ObjectType& type);
std::string getRegionName(const Ethnicity etnos, const int type, const int size, const bool island);
std::string getRiverName(const int size, const int min, const int max);

// Production building names with race-specific and resource-specific variants
std::string getProductionBuildingName(int buildingType, int resourceType, int race);

// Road names with direction and builder-race cultural flavour
std::string getRoadName(int objectType, int race);

// Caravanserai names — culturally themed trade waystation names
std::string getCaravanseraiName(int race);

// Lair names with monster-specific and ethnicity-specific variants
std::string getLairName(int lairType, int monsterType, int race);

Ethnicity raceToEthnicity(int race);

// Personal name for a single-person player unit, keyed by race item ID.
// Unknown/disabled races fall back to getAbstractName() so unit is always renamed.
// See docs/UNIT_NAMING_SYSTEM.md for Birthday Problem analysis and lore sources.
std::string getPersonName(int raceItem);
