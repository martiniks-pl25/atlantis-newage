#include "logger.hpp"
#include "events.h"
#include "gamedata.h"
#include "graphs.h"
#include "object.h"
#include "rng.hpp"

#include "external/nlohmann/json.hpp"
using json = nlohmann::json;

#include <map>
#include <queue>
#include <algorithm>

std::string townType(const int type) {
    switch (type) {
        case TOWN_VILLAGE: return "village";
        case TOWN_TOWN:    return "town";
        case TOWN_CITY:    return "city";
        default:           return "unknown";
    }
}

FactBase::~FactBase() {

}

void BattleSide::AssignUnit(Unit* unit) {
    this->factionName = unit->faction->name;
    this->factionNum = unit->faction->num;
    this->unitName = unit->name;
    this->unitNum = unit->num;
}

void BattleSide::AssignArmy(Army* army) {
    this->total = army->count;

    for (int i = 0; i < army->count; i++) {
        auto soldier = army->soldiers[i];

        bool lost = soldier->hits == 0;
        if (lost) this->lost++;

        ItemType& item = ItemDefs[soldier->race];

        if (item.flags & ItemType::MANPRODUCE) {
            this->fmi++;
            if (lost) this->fmiLost++;
            continue;
        }

        if (item.type & IT_UNDEAD) {
            this->undead++;
            if (lost) this->undeadLost++;
            continue;
        }

        if (item.type & IT_MONSTER && !(item.type & IT_UNDEAD)) {
            this->monsters++;
            if (lost) this->monstersLost++;
            continue;
        }

        auto unit = soldier->unit;
        if (unit == NULL) {
            continue;
        }

        auto type = unit->type;
        if (type == U_MAGE || type == U_GUARDMAGE || type == U_APPRENTICE) {
            this->mages++;
            if (lost) this->magesLost++;
        }
    }
}

const std::string EventLocation::GetTerrainName(const bool plural) {
    TerrainType &terrain = TerrainDefs[this->terrainType];
    auto terrainName = plural ? terrain.plural : terrain.name;
    return terrainName;
}

void populateSettlementLandmark(std::vector<Landmark> &landmarks, ARegion *reg, const int distance) {
    if (!reg->town) {
        return;
    }

    std::string name = reg->town->name;
    std::string title = townType(reg->town->TownType()) + " of " + name;

    landmarks.push_back({
        .type = events::LandmarkType::SETTLEMENT,
        .name = name,
        .title = title,
        .distance = distance,
        .weight = 10,
        .x = reg->xloc,
        .y = reg->yloc,
        .z = reg->zloc
    });
}

void populateRegionLandmark(std::vector<Landmark> &landmarks, ARegion *source, ARegion *reg, const int distance) {
    TerrainType& terrain = TerrainDefs[reg->type];
    int alias = terrain.similar_type;

    std::string name = reg->name;
    std::string sourceName = source->name;
    if (name == sourceName) {
        return;
    }

    std::string title = std::string(terrain.plural) + " of " + name;
    int weight = 1;

    events::LandmarkType type = events::LandmarkType::UNKNOWN;
    if (alias == R_MOUNTAIN) {
        type = events::LandmarkType::MOUNTAIN;
    }
    else if (alias == R_FOREST || alias == R_JUNGLE) {
        type = events::LandmarkType::FOREST;
    }
    else if (alias == R_VOLCANO) {
        type = events::LandmarkType::VOLCANO;
        weight = 2;
    }
    else if (alias == R_OCEAN) {
        type = name.ends_with("River") ? events::LandmarkType::RIVER : events::LandmarkType::OCEAN;
    }
    else if (name.ends_with("River")) {
        type = events::LandmarkType::FORD;
        weight = 2;
    }

    if (type == events::LandmarkType::UNKNOWN) {
        return;
    }

    landmarks.push_back({
        .type = type,
        .name = name,
        .title = title,
        .distance = distance,
        .weight = weight,
        .x = reg->xloc,
        .y = reg->yloc,
        .z = reg->zloc
    });
}

void populateForitifcationLandmark(std::vector<Landmark> &landmarks, ARegion *reg, const int distance) {
    int protect = 0;
    Object *building = NULL;

    for(const auto obj : reg->objects) {
        ObjectType& type = ObjectDefs[obj->type];

        if (type.flags & ObjectType::GROUP) {
            continue;
        }

        if (obj->IsFleet()) {
            continue;
        }

        if (protect >= type.protect) {
            continue;
        }

        protect = type.protect;
        building = obj;
    }

    if (!building) {
        return;
    }

    std::string name = building->name;
    std::string title = std::string(ObjectDefs[building->type].name) + " " + name;

    landmarks.push_back({
        .type = events::LandmarkType::FORTIFICATION,
        .name = name,
        .title = title,
        .distance = distance,
        .weight = 5,
        .x = reg->xloc,
        .y = reg->yloc,
        .z = reg->zloc
    });
}

bool compareLandmarks(const Landmark &first, const Landmark &second) {
    // Making this a bit more explicit since if you end up with 2 equidistant, equal weight landmarks, then
    // it will be arbitary which landmark is chosen based on the initial ordering in the vector being sorted,
    // which is based solely on an *unordered* map, which means the ordering is not guaranteed to be the same
    // across runs even with identical input since it's based on memory layout on the executing machine.

    // prefer shorter distances.
    if (first.distance != second.distance) {
        return first.distance < second.distance;
    }
    // prefer more weighty locales (citys > fortifications > volcano/river > other terrains)
    if (first.weight != second.weight) {
        return first.weight > second.weight;
    }
    // if everything else is equal, prefer one with the lower x coordinate
    if (first.x != second.x) {
        return first.x < second.x;
    }
    // If they are *still* equal, prefer the smaller y coordinate
    return first.y < second.y;
}

const EventLocation EventLocation::Create(ARegion* region) {
    EventLocation loc;

    loc.x = region->xloc;
    loc.y = region->yloc;
    loc.z = region->zloc;
    loc.terrainType = region->type;
    loc.province = region->name;

    if (region->town) {
        loc.settlement = region->town->name;
        loc.settlementType = region->town->TownType();
    }

    loc.landmarks = { };

    auto items = breadthFirstSearch(region, 4);
    for (auto &kv : items) {
        auto reg = kv.second.key;
        auto distance = kv.second.distance;

        populateSettlementLandmark(loc.landmarks, reg, distance);
        populateRegionLandmark(loc.landmarks, region, reg, distance);
        populateForitifcationLandmark(loc.landmarks, reg, distance);
    }

    std::sort(std::begin(loc.landmarks), std::end(loc.landmarks), compareLandmarks);

    return loc;
}

const Landmark* EventLocation::GetSignificantLandmark() {
    if (this->landmarks.empty()) {
        return NULL;
    }

    return &(this->landmarks.at(0));
}


/////-----


Events::Events() {
}

Events::~Events() {
    for (auto &fact : this->facts) {
        delete fact;
    }

    this->facts.clear();
}

void Events::AddFact(FactBase *fact) {
    this->facts.push_back(fact);
}

bool compareEvents(const Event &first, const Event &second) {
    // return true if first should go before second
    return first.score > second.score;
}

std::list<std::string> wrapText(std::string input, std::size_t width) {
    std::size_t curpos = 0;
    std::size_t nextpos = 0;

    std::list<std::string> lines;
    std::string substr = input.substr(curpos, width + 1);

    while (substr.length() == width + 1 && (nextpos = substr.rfind(' ')) != input.npos) {
        lines.push_back(input.substr(curpos, nextpos));

        curpos += nextpos + 1;
        substr = input.substr(curpos, width + 1);
    }

    if (curpos != input.length()) {
        lines.push_back(input.substr(curpos, input.npos));
    }

    return lines;
}

std::string makeLine(std::size_t width, bool odd, std::string text)  {
    std::string line = (odd ? "( " : " )");

    std::size_t left = (width - text.size()) / 2;

    while (line.size() < left) line += ' ';
    line += text;
    while (line.size() < (width - 2)) line += ' ';

    line += (odd ? " )" : "(");
    line += "\n";

    return line;
}

std::string Events::Write(std::string worldName, std::string month, int year) {
    std::list<Event> events;

    for (auto &fact : this->facts) {
        fact->GetEvents(events);
    }

    std::map<EventCategory, std::vector<Event>> categories;
    for (auto &event : events) {
        if (categories.find(event.category) == categories.end()) {
            std::vector<Event> list = {};
            categories.insert(std::pair<EventCategory, std::vector<Event>>(event.category, list));
        }

        categories[event.category].push_back(event);
    }

    std::string text =  "   _.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._.-=-._\n";
                text += ".----      - ---     --     ---   -----   - --       ----  ----   -     ----.\n";
                text += " )                                                                         (\n";

    std::list<std::string> lines;
    for (auto &cat : categories) {
        auto list = cat.second;

        std::sort(list.begin(), list.end(), compareEvents);
        list.resize(std::min((int) list.size(), 10));

        int n = std::min((int) list.size(), rng::get_random(3) + 3);
        while (n-- > 0) {
            int i = rng::get_random(list.size());

            if (lines.size() > 0) {
                lines.push_back("");
                lines.push_back(".:*~*:._.:*~*:._.:*~*:.");
                lines.push_back("");
            }

            auto tmp = wrapText(list[i].text, 65);
            for (auto &line : tmp) {
                lines.push_back(line);
            }

            list.erase(list.begin() + i);
        }
    }

    bool noNews = lines.size() == 0;

    lines.push_front("");
    lines.push_front(month + ", Year " + std::to_string(year));
    lines.push_front(worldName + " Events");

    if (noNews) {
        lines.push_back("--== Nothing mentionable happened in the world this month ==--");
    }

    if (lines.size() % 2) {
        lines.push_back("");
    }

    int n = 3;
    for (auto &line : lines) {
        text += makeLine(77, (n++) % 2, line);
    }

    text += "(__       _       _       _       _       _       _       _       _       __)\n";
    text += "    '-._.-' (___ _) '-._.-' '-._.-' )     ( '-._.-' '-._.-' (__ _ ) '-._.-'\n";
    text += "            ( _ __)                (_     _)                (_ ___)\n";
    text += "            (__  _)                 '-._.-'                 (___ _)\n";
    text += "            '-._.-'                                         '-._.-'\n";

    return text;
}

AnnihilationFact::AnnihilationFact() {
    this->message = "";
}

AnnihilationFact::~AnnihilationFact() {
}

void AnnihilationFact::GetEvents(std::list<Event> &events) {
    events.push_back({
        .category = EventCategory::EVENT_ANNIHILATION,
        .score = 1000,
        .text = this->message
    });
}

AnomalyFact::AnomalyFact() {
    this->location = nullptr;
}

AnomalyFact::~AnomalyFact() {
}

void AnomalyFact::GetEvents(std::list<Event> &events) {
    events.push_back({
        .category = EventCategory::EVENT_ANOMALY,
        .score = 100,
        .text = "A strange anomaly was detected in region " + location->print() + "."
    });
}

// --- GuardAttitudeFact ---

static const std::vector<std::string> unfriendly_templates = {
    "By official decree of the City Guard, the faction known as {NAME} is henceforth "
    "declared persona non grata. Their protection within guarded settlements is revoked.",

    "Notice is hereby given: {NAME} has been placed on the City Guard's watch-list. "
    "Guard patrols will no longer answer their calls for aid.",

    "The gates of our cities grow cold. The City Guard has withdrawn its protection "
    "from {NAME}, who must now seek shelter at their own risk.",

    "Hear ye, hear ye! The City Guard announces that {NAME} is no longer under its "
    "protection. They walk the streets of our towns as strangers, not as guests.",

    "From this day forth, {NAME} shall receive no aid from the City Guard. "
    "Their conduct has earned them the status of unwelcome wanderers in our realm.",

    "The City Guard's ledger marks {NAME} as suspect. Citizens are advised: "
    "their protection at the city gates has been suspended."
};

static const std::vector<std::string> hostile_templates = {
    "WANTED: By order of the City Guard, {NAME} is declared an enemy of the realm! "
    "Their units may be attacked without consequence in any settlement under our "
    "protection. No mercy shall be shown.",

    "OUTLAWED! The City Guard has issued a warrant of aggression against {NAME}. "
    "All guard units are ordered to attack them on sight. Citizens in protected "
    "settlements are granted the right to defend themselves against their forces.",

    "A dark proclamation echoes through the city: {NAME} stands condemned by the "
    "City Guard. Their presence in guarded settlements is forbidden — by sword if "
    "necessary. Any who strike them within our walls commit no crime.",

    "The drums of justice beat for {NAME}! Declared enemies of all guarded cities, "
    "they have forfeited their right to safety within our walls. The City Guard "
    "commands: drive them out, or cut them down.",

    "By solemn decree: {NAME} is branded outlaw. City guards are authorized — nay, "
    "commanded — to engage their forces within any settlement they patrol. "
    "Let no gate offer them shelter.",

    "The realm speaks with one voice: {NAME} is exiled and condemned. They shall "
    "find no sanctuary in our cities. Every sword arm in every guarded town "
    "is raised against them."
};

static std::string applyTemplate(const std::string &tmpl, const std::string &faction_name) {
    std::string text = tmpl;
    size_t pos;
    while ((pos = text.find("{NAME}")) != std::string::npos)
        text.replace(pos, 6, faction_name);
    return text;
}

GuardAttitudeFact::GuardAttitudeFact() : faction_num(0), new_attitude(AttitudeType::UNFRIENDLY) {}
GuardAttitudeFact::~GuardAttitudeFact() {}

void GuardAttitudeFact::GetEvents(std::list<Event> &events) {
    const auto &templates = (new_attitude == AttitudeType::HOSTILE)
        ? hostile_templates : unfriendly_templates;

    int idx = rng::get_random(templates.size());
    std::string text = applyTemplate(templates[idx], faction_name);

    events.push_back({
        .category = EVENT_GUARD_REPUTATION,
        .score = (new_attitude == AttitudeType::HOSTILE) ? 90 : 70,
        .text = text
    });
}

// --- JSON output ---

static std::string categoryToString(EventCategory cat) {
    switch (cat) {
        case EVENT_BATTLE:           return "battle";
        case EVENT_CITY_CAPTURE:     return "city_capture";
        case EVENT_MONSTER_HUNT:     return "monster_hunt";
        case EVENT_MONSTER_AGGRESSION: return "monster_aggression";
        case EVENT_ASSASSINATION:    return "assassination";
        case EVENT_ANNIHILATION:     return "annihilation";
        case EVENT_ANOMALY:          return "anomaly";
        case EVENT_GUARD_REPUTATION: return "guard_reputation";
        default:                     return "unknown";
    }
}

std::string Events::WriteJSON(std::string worldName, std::string month, int year,
                              std::vector<std::pair<int,std::string>> wanted) {
    std::list<Event> events;
    for (auto &fact : this->facts) {
        fact->GetEvents(events);
    }

    // Group by category, sort by score descending, cap at 10 per category
    std::map<EventCategory, std::vector<Event>> categories;
    for (auto &event : events) {
        categories[event.category].push_back(event);
    }

    json j;
    j["world"] = worldName;
    j["month"] = month;
    j["year"]  = year;

    json eventArray = json::array();
    for (auto &cat : categories) {
        auto list = cat.second;
        std::sort(list.begin(), list.end(), compareEvents);
        if ((int)list.size() > 10) list.resize(10);

        for (auto &e : list) {
            json item;
            item["category"] = categoryToString(e.category);
            item["text"]     = e.text;
            eventArray.push_back(item);
        }
    }
    j["events"] = eventArray;

    json wantedArray = json::array();
    for (auto &w : wanted) {
        json item;
        item["faction_num"]  = w.first;
        item["faction_name"] = w.second;
        wantedArray.push_back(item);
    }
    j["wanted"] = wantedArray;

    return j.dump(2);
}
