#pragma once
#ifndef EVENTS_H
#define EVENTS_H

class Events;
class FactBase;
class BattleFact;
class AssassinationFact;
class GuardAttitudeFact;

#include "unit.h"
#include <string>
#include <list>
#include <vector>
#include <utility>

std::string townType(const int type);

enum EventCategory {
    EVENT_BATTLE,
    EVENT_CITY_CAPTURE,
    EVENT_MONSTER_HUNT,
    EVENT_MONSTER_AGGRESSION,
    EVENT_ASSASSINATION,
    EVENT_ANNIHILATION,
    EVENT_ANOMALY,
    EVENT_GUARD_REPUTATION,
    EVENT_SETTLEMENT_STATS,
    EVENT_PIRATE_SIGHTING,
};

struct Event {
    EventCategory category;
    int score;
    std::string text;
};


class FactBase {
public:
    virtual ~FactBase() = 0;
    virtual void GetEvents(std::list<Event> &events) = 0;
};

class Events {
public:
    Events();
    ~Events();

    std::string Write(std::string worldName, std::string month, int year);
    std::string WriteJSON(std::string worldName, std::string month, int year,
                          std::vector<std::pair<int,std::string>> wanted,
                          std::vector<std::string> pirate_context = {});

    void AddFact(FactBase *fact);

private:
    std::list<FactBase *> facts;
};

struct BattleSide {
    int factionNum;
    std::string factionName;

    int unitNum;
    std::string unitName;

    int total;

    int mages;
    int monsters;
    int undead;
    int fmi;

    int lost;

    int magesLost;
    int fmiLost;
    int undeadLost;
    int monstersLost;

    void AssignUnit(Unit* unit);
    void AssignArmy(Army* army);
};

namespace events {
    enum LandmarkType {
        UNKNOWN,
        SETTLEMENT,
        FORTIFICATION,
        MOUNTAIN,
        FOREST,
        VOLCANO,
        RIVER,
        FORD,
        OCEAN
    };
}

struct Landmark {
    events::LandmarkType type;
    std::string name;
    std::string title;
    int distance;
    int weight;
    int x;
    int y;
    int z;
};

bool compareLandmarks(const Landmark &a, const Landmark &b);

struct EventLocation {
    int x;
    int y;
    int z;
    int terrainType;
    std::string province;
    std::string settlement;
    int settlementType;

    std::vector<Landmark> landmarks;

    events::LandmarkType GetLandmarkType();
    const std::string GetTerrainName(const bool plural = false);
    static const EventLocation Create(ARegion* region);
    const Landmark *GetSignificantLandmark();
};

class BattleFact : public FactBase {
public:
    BattleFact();
    ~BattleFact();

    void GetEvents(std::list<Event> &events);

    EventLocation location;
    BattleSide attacker;
    BattleSide defender;

    std::string fortification;
    int fortificationType;

    int outcome;    // BATTLE_LOST, BATTLE_WON, BATTLE_DRAW
};

class AssassinationFact : public FactBase {
    public:
        AssassinationFact();
        ~AssassinationFact();

        void GetEvents(std::list<Event> &events);

        EventLocation location;
        // BattleSide victim;

        int outcome;    // BATTLE_LOST, BATTLE_WON, BATTLE_DRAW
};

class AnnihilationFact : public FactBase {
    public:
        AnnihilationFact();
        ~AnnihilationFact();

        void GetEvents(std::list<Event> &events);

        std::string message;
};

class AnomalyFact : public FactBase {
    public:
        AnomalyFact();
        ~AnomalyFact();

        void GetEvents(std::list<Event> &events);

        ARegion *location;
};

class GuardAttitudeFact : public FactBase {
    public:
        GuardAttitudeFact();
        ~GuardAttitudeFact();

        void GetEvents(std::list<Event> &events) override;

        std::string faction_name;
        int faction_num;
        AttitudeType new_attitude;  // UNFRIENDLY or HOSTILE
};

struct SettlementOwner {
    int faction_num;
    std::string faction_name;  // already stripped of number
    int villages = 0;
    int towns = 0;
    int cities = 0;
    int total = 0;
};

class PirateSightingFact : public FactBase {
public:
    PirateSightingFact() = default;
    ~PirateSightingFact() override = default;
    void GetEvents(std::list<Event> &events) override;

    std::string ship_name;
    std::string captain_name;
    std::string terrain_name;
    std::string region_name;
};

class SettlementStatsFact : public FactBase {
    public:
        SettlementStatsFact();
        ~SettlementStatsFact() override;

        void GetEvents(std::list<Event> &events) override;

        int total_settlements = 0;
        int surface_settlements = 0;
        int contested_settlements = 0;
        std::vector<SettlementOwner> top_owners;  // up to top 5, sorted by total desc
};

#endif // EVENTS_H
