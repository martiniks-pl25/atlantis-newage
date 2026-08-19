#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "quest_data.h"
#include <cmath>
#include <string>
#include <iterator>
#include <memory>

void NewOriginsSetupQuests(); // defined in neworigins/quest_setup.cpp

using namespace std;

// ---------------------------------------------------------------------------
// Village founding: forbidden settler races
// Add item IDs of races that cannot found a settlement via CREATE VILLAGE.
// Example: { I_FAIRY, I_SOMEMONSTER, -1 }
// ---------------------------------------------------------------------------
extern const std::vector<int> CANNOT_FOUND_SETTLEMENT = {
    // No races fully forbidden by default — add item IDs here as needed
};

// Races that can found a village but will not change the region's dominant race.
// Use this for magical or wandering races that adapt to their surroundings.
extern const std::vector<int> RACE_NEUTRAL_FOUNDERS = {
};


int Game::SetupFaction( Faction *pFac )
{
    // Check if a faction can be started due to end game conditions
    if(rulesetSpecificData.value("victory_type", "") == "annihilation") {
        ARegionArray *surface = regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
        ARegion *surface_center = surface->GetRegion(surface->x / 2, surface->y / 2);

        int count = 0;
        for (int i = 0; i < 6; i++) {
            ARegion *r = surface_center->neighbors[i];
            // search that region for an altar
            for(const auto o : r->objects) {
                if (o->type == O_EMPOWERED_ALTAR) count++;
            }
        }
        // if all altars to the center are empowered, then factions cannot be started
        if (count == 6) return 0;
    }

    pFac->unclaimed = Globals->START_MONEY + TurnNumber() * 300;

    if (pFac->noStartLeader) {
        return 1;
    }

    //
    // Set up first unit.
    //
    Unit *temp2 = GetNewUnit( pFac );
    temp2->SetMen(I_LEADERS, 1);
    pFac->DiscoverItem(I_LEADERS, 0, 1);
    temp2->reveal = REVEAL_FACTION;

    //
    // Set up magic
    //
    temp2->type = U_MAGE;
    temp2->Study(S_OBSERVATION, 30);
    temp2->Study(S_FORCE, 30);
    temp2->Study(S_PATTERN, 30);
    temp2->Study(S_SPIRIT, 30);
    temp2->Study(S_GATE_LORE, 30);
    temp2->Study(S_FIRE, 30);

    // Set up health
    temp2->Study(S_COMBAT, 180);

    // Set up flags
    temp2->SetFlag(FLAG_BEHIND, 1);
    temp2->SetFlag(FLAG_NOCROSS_WATER, 0);
    temp2->SetFlag(FLAG_HOLDING, 0);
    temp2->SetFlag(FLAG_NOAID, 0);

    if (Globals->UPKEEP_MINIMUM_FOOD > 0)
    {
        if (!(ItemDefs[I_FOOD].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_FOOD, 6);
            pFac->DiscoverItem(I_FOOD, 0, 1);
        } else if (!(ItemDefs[I_FISH].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_FISH, 6);
            pFac->DiscoverItem(I_FISH, 0, 1);
        } else if (!(ItemDefs[I_LIVESTOCK].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_LIVESTOCK, 6);
            pFac->DiscoverItem(I_LIVESTOCK, 0, 1);
        } else if (!(ItemDefs[I_GRAIN].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_GRAIN, 2);
            pFac->DiscoverItem(I_GRAIN, 0, 1);
        }
        temp2->items.SetNum(I_SILVER, 10);
    }

    ARegion *reg = NULL;
    if (pFac->pStartLoc) {
        reg = pFac->pStartLoc;
    } else if (!Globals->MULTI_HEX_NEXUS) {
        reg = regions.front();
    } else {
        ARegionArray *pArr = regions.GetRegionArray(ARegionArray::LEVEL_NEXUS);
        while(!reg) {
            reg = pArr->GetRegion(rng::get_random(pArr->x), rng::get_random(pArr->y));
        }
    }
    temp2->MoveUnit(reg->GetDummy());

    if (Globals->LAIR_MONSTERS_EXIST || Globals->WANDERING_MONSTERS_EXIST) {
        // Set neutral attitude towards Creatures: avoids unintended guard blocks
        // and noisy "Forbids entry" messages for new players.
        pFac->set_attitude(monfaction, AttitudeType::NEUTRAL);
    }

    return( 1 );
}

// Just a quick function to count the number of empowered altars.
int report_and_count_empowered_altars(ARegionList& regions, std::list<Faction *>& factions) {
    ARegionArray *surface = regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
    ARegion *surface_center = surface->GetRegion(surface->x / 2, surface->y / 2);

    int count = 0;
    for (int i = 0; i < 6; i++) {
        ARegion *r = surface_center->neighbors[i];
        // search that region for an altar
        for(const auto o : r->objects) {
            if (o->type == O_EMPOWERED_ALTAR) {
                count++;
                for(const auto f : factions) {
                    if (f->is_npc) continue;
                    f->event("The altar in " + r->short_print() + " is fully empowered.", "anomaly", r);
                }
            }
        }
    }
    return count;
}

void empower_random_altar(ARegionList& regions, std::list<Faction *>& factions) {
    ARegionArray *surface = regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
    ARegion *surface_center = surface->GetRegion(surface->x / 2, surface->y / 2);

    std::vector<Object *> unempowered_altars;
    for (int i = 0; i < 6; i++) {
        ARegion *r = surface_center->neighbors[i];
        // search that region for an altar
        for(const auto o : r->objects) {
            if (o->type == O_RITUAL_ALTAR) unempowered_altars.push_back(o);
        }
    }
    // pick a random altar to empower
    int num = rng::get_random(unempowered_altars.size());
    Object *o = unempowered_altars[num];
    o->type = O_EMPOWERED_ALTAR;
    o->set_name(ObjectDefs[O_EMPOWERED_ALTAR].name);
    // notify all factions.
    for(const auto f : factions) {
        if (f->is_npc) continue;
        f->event("The altar in " + o->region->short_print() + " is fully empowered.", "anomaly", o->region);
    }

    // find all current anomalies and entities
    std::vector<Object *> anomalies;
    std::vector<Unit *> entities;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            if (o->type == O_ENTITY_CAGE) anomalies.push_back(o);

            for(const auto u : o->units) {
                int i = u->items.GetNum(I_IMPRISONED_ENTITY);
                for(int j = 0; j < i; j++) {
                    // put a unit in multiple times if it has multiple entities
                    entities.push_back(u);
                }
            }
        }
    }
    if (anomalies.size() + entities.size() < unempowered_altars.size()) {
        // We have unspawend entities and anomalies, so just return without removing one.
        return;
    }
    // If we have any anomalies, remove the one farthest from the center.
    if (anomalies.size() > 0) {
        int max_dist = 0;
        Object *far_anomaly = nullptr;
        for (auto& anomaly : anomalies) {
            int dist = regions.find_distance_between_regions(anomaly->region, surface_center);
            if (dist > max_dist) {
                max_dist = dist;
                far_anomaly = anomaly;
            }
        }
        if (far_anomaly) {
            // remove the farthest anomaly
            // Just in case, move any units in the anomaly.
            for(const auto u : far_anomaly->units) u->MoveUnit(far_anomaly->region->GetDummy());

            std::erase(far_anomaly->region->objects, far_anomaly);
            delete far_anomaly;

            // Notify any factions in that region that the anomaly has been removed.
            auto reg_faction = far_anomaly->region->PresentFactions();
            for(const auto f : reg_faction) {
                if (f->is_npc) continue;
                f->event("The anomaly in " + far_anomaly->region->short_print() + " vanishes.", "anomaly", far_anomaly->region);
            }
            return;
        }
    }
    // Ok, we couldn't remove any anomalies, so remove the farthest entity instead.
    if (entities.size() > 0) {
        int max_dist = 0;
        Unit *far_entity = nullptr;
        for (auto& entity : entities) {
            int dist = regions.find_distance_between_regions(entity->object->region, surface_center);
            if (dist > max_dist) {
                max_dist = dist;
                far_entity = entity;
            }
        }
        if (far_entity) {
            far_entity->items.SetNum(I_IMPRISONED_ENTITY, far_entity->items.GetNum(I_IMPRISONED_ENTITY) - 1);
            far_entity->event(item_string(I_IMPRISONED_ENTITY, 1) + " vanishes suddenly.", "anomaly");
            return;
        }
    }
    // We somehow got here without removing anything, so just return.
    return;
}


int report_and_count_anomalies(ARegionList& regions, std::list<Faction *>& factions) {
    int count = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            if (o->type == O_ENTITY_CAGE) {
                count++;
                for(const auto f : factions) {
                    if (f->is_npc) continue;
                    f->event("A strange anomaly has been seen in " + r->short_print() + ".", "anomaly", r);
                }
            }
        }
    }
    return count;
}

int report_and_count_entities(ARegionList& regions, std::list<Faction *>& factions) {
    int count = 0;
    for(const auto r : regions) {
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                if (u->items.GetNum(I_IMPRISONED_ENTITY) > 0) {
                    count += u->items.GetNum(I_IMPRISONED_ENTITY);
                    for(const auto f : factions) {
                        if (f->is_npc) continue;
                        f->event("An imprisoned entity has been spotted in " + r->short_print() +
                            " in the possession of " + u->get_name(0) + ".", "anomaly", r);
                    }
                }
            }
        }
    }
    return count;
}

Faction *Game::CheckVictory()
{
    int visited, unvisited;
    int d, count;
    int dir;
    unsigned ucount;
    ARegion *r, *start;
    Object *o;
    Location *l;
    std::string message, times, temp;
    map <string, int> vRegions, uvRegions;
    map <string, int>::iterator it;
    string stlstr;
    set<string> intersection, un;
    set<string>::iterator it2;
    Faction *winner = nullptr;

    for(const auto& q: quests) {
        if (q->type != Quest::VISIT) continue;
        for (auto dest: q->destinations) {
            un.insert(dest);
        }
    }

    visited = 0;
    unvisited = 0;
    for(const auto r : regions) {
        if (r->Population() > 0) {
            stlstr = r->name;
            if (r->visited) {
                visited++;
                vRegions[stlstr]++;
            } else {
                unvisited++;
                uvRegions[stlstr]++;
            }
        }
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                intersection.clear();
                set_intersection(
                    u->visited.begin(), u->visited.end(),
                    un.begin(), un.end(),
                    inserter(intersection, intersection.begin()),
                    less<string>()
                );
                u->visited = intersection;
            }
        }
    }

    printf("Players have visited %d regions; %d unvisited.\n", visited, unvisited);

    if (unvisited) {
        // Tell the players to get exploring :-)
        if (visited > 9 * unvisited) {
            // 90% explored; specific hints
            d = rng::get_random(12);
        } else if (visited > 3 * unvisited) {
            // 75% explored; some general hints
            d = rng::get_random(8);
        } else {
            // lots of unexplored area; just tell them to explore
            d = rng::get_random(6);
        }
        if (d == 2) {
            message = "Be productive and strong; explore new land and find a way to survive.";
            write_times_article(message);
        } else if (d == 3) {
            message = "Go into all the world, and tell all people that new world is great.";
            write_times_article(message);
        } else if (d == 4 || d == 5) {
            message = "Players have visited " + std::to_string(visited * 100 / (visited + unvisited)) +
                "% of all inhabited regions.";
            write_times_article(message);
        } else if (d == 6) {
            // report an incompletely explored region
            count = 0;
            // see how many incompletely explored regions we have
            for (it = vRegions.begin(); it != vRegions.end(); it++) {
                if (uvRegions[it->first] > 0)
                    count++;
            }
            if (count > 0) {
                // choose one, and find it
                count = rng::get_random(count);
                for (it = vRegions.begin(); it != vRegions.end(); it++) {
                    if (uvRegions[it->first] > 0)
                        if (!count--)
                            break;
                }
                // pick a hex within that region, and find it
                count = rng::get_random(it->second);
                for(const auto r : regions) {
                    if (it->first == r->name) {
                        if (!count--) {
                            // report this hex
                            message = "The " + TerrainDefs[TerrainDefs[r->type].similar_type].name + " of " + r->name +
                                (TerrainDefs[r->type].similar_type == R_TUNNELS ? " are" : " is") +
                                " only partly explored.";
                            write_times_article(message);
                        }
                    }
                }
            }
        } else if (d == 7) {
            // report a completely unknown region
            count = 0;
            // see how many completely unexplored regions we have
            for (it = uvRegions.begin(); it != uvRegions.end(); it++) {
                if (vRegions[it->first] == 0)
                    count++;
            }
            if (count > 0) {
                // choose one, and find it
                count = rng::get_random(count);
                for (it = uvRegions.begin(); it != uvRegions.end(); it++) {
                    if (vRegions[it->first] == 0) {
                        if (!count--)
                            break;
                    }
                }
                // pick a hex within that region, and find it
                count = rng::get_random(it->second);
                for(const auto r : regions) {
                    if (it->first == r->name) {
                        if (!count--) {
                            // report this hex
                            dir = -1;
                            start = regions.FindNearestStartingCity(r, &dir);
                            message = "The " + TerrainDefs[TerrainDefs[r->type].similar_type].name + " of " + r->name;
                            if (start == r) {
                                message += ", containing " + start->town->name + ",";
                            } else if (start && dir != -1) {
                                message += ", ";
                                if (r->zloc != start->zloc && dir != MOVE_IN)
                                    message += "through a shaft ";
                                switch (dir) {
                                    case D_NORTH:
                                    case D_NORTHWEST:
                                        message += "north of";
                                        break;
                                    case D_NORTHEAST:
                                        message += "east of";
                                        break;
                                    case D_SOUTH:
                                    case D_SOUTHEAST:
                                        message += "south of";
                                        break;
                                    case D_SOUTHWEST:
                                        message += "west of";
                                        break;
                                    case MOVE_IN:
                                        message += "through a shaft in";
                                        break;
                                }
                                message += " " + start->town->name + ",";
                            }
                            message += (TerrainDefs[r->type].similar_type == R_TUNNELS ? " have" : " has");
                            message += " yet to be visited by exiles from destroyed worlds.";
                            write_times_article(message);
                        }
                    }
                }
            }
        } else if (d > 7) {
            // report exact coords of an unexplored hex
            count = rng::get_random(unvisited);
            for(const auto r : regions) {
                if (r->Population() > 0 && !r->visited) {
                    if (!count--) {
                        message = "The people of the " + r->short_print();
                        switch (rng::get_random(4)) {
                            case 0:
                                message += " have not been visited by exiles.";
                                break;
                            case 1:
                                message += " are still in need of your guidance.";
                                break;
                            case 2:
                                message += " have not yet been graced by your presence.";
                                break;
                            case 3:
                                message += " are still in need of your guidance.";
                                break;
                        }
                        write_times_article(message);
                    }
                }
            }
        }
    }

    std::vector<std::shared_ptr<Quest>> questsWithProblems;
    for(const auto& q: quests) {
        // New-format quests have type=-1 (Quest() default). They are fully managed
        // by the new quest system (scope/subtype) and must not go through the legacy
        // type switch — the default: branch would delete them every turn.
        if (q->type == -1) continue;
        switch(q->type) {
            case Quest::SLAY:
                l = regions.FindUnit(q->target);
                if (!l || l->unit->faction->num != monfaction) {
                    // Something has gone wrong with this quest!
                    // shouldn't ever happen, but...
                    questsWithProblems.push_back(q);
                    if (l) delete l;
                } else {
                    message = "Quest: In the ";
                    message += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
                    message += " of ";
                    message += l->region->name;
                    if (l->obj->type == O_DUMMY)
                        message += " roams";
                    else
                        message += " lurks";
                    message += " the ";
                    message += l->unit->name;
                    message += ".  Free the world from this menace and be rewarded!";
                    write_times_article(message);
                    delete l;
                }

                break;
            case Quest::HARVEST:
                r = regions.GetRegion(q->regionnum);
                message = "Quest: Seek a token of the Ancient Ones legacy amongst the ";
                message += ItemDefs[q->objective.type].names;
                message += " of ";
                message += r->name;
                message += ".";
                write_times_article(message);
                break;
            case Quest::BUILD:
                message = "Quest: Build a ";
                message += ObjectDefs[q->building].name;
                message += " in ";
                message += q->regionname;
                message += " for the glory of the Gods.";
                write_times_article(message);
                break;
            case Quest::VISIT:
                message = "Quest: Show your devotion by visiting ";
                message += ObjectDefs[q->building].name;
                message += "s in ";
                ucount = 0;
                for (it2 = q->destinations.begin();
                    it2 != q->destinations.end();
                    it2++) {
                    ucount++;
                    if (ucount == q->destinations.size()) {
                        message += " and ";
                    } else if (ucount > 1) {
                        message += ", ";
                    }
                    message += it2->c_str();
                }
                message += ".";
                write_times_article(message);
                break;
            case Quest::DEMOLISH:
                r = regions.GetRegion(q->regionnum);
                if (r)
                    o = r->GetObject(q->target);
                else
                    o = 0;
                if (!r || !o) {
                    // Something has gone wrong with this quest!
                    // shouldn't ever happen, but...
                    questsWithProblems.push_back(q);
                } else {
                    message = "Quest: Tear down the blasphemous ";
                    message += o->name;
                    message += " : ";
                    message += ObjectDefs[o->type].name;
                    message += " in ";
                    message += r->name;
                    message += "!";
                    write_times_article(message);
                }
                break;
            case Quest::HUNT_PIRATE:
                // Mirror Quest::SLAY: surface the active hunt as a times article,
                // and only mark as orphan if the target captain unit is gone or
                // no longer belongs to the monster faction. Without this branch
                // the quest fell through to default: and was erased every turn,
                // forcing EnsureElitePirateQuests() to recreate it with a fresh
                // random reward — observable as quest rewards mutating between
                // turns. See docs/CURRENT_QUEST_SYSTEM_ANALYSIS.md §10.2.
                l = regions.FindUnit(q->target);
                if (!l || l->unit->faction->num != monfaction) {
                    questsWithProblems.push_back(q);
                    if (l) delete l;
                } else {
                    message = "Quest: ";
                    message += l->unit->name;
                    message += " commands the pirate galley '";
                    message += l->obj->name;
                    message += "', last sighted in the ";
                    message += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
                    message += " of ";
                    message += l->region->name;
                    message += ". Bring proof of their destruction and claim the bounty!";
                    write_times_article(message);
                    delete l;
                }
                break;
            default:
                questsWithProblems.push_back(q);
                break;
        }
    }
    for(const auto& q: questsWithProblems) quests.erase(q);

    // Publish GLOBAL_BOSS_HUNT quests (pirate captains) to the gazette each turn.
    for (const auto& q : quests) {
        if (q->subtype != Quest::GLOBAL_BOSS_HUNT) continue;
        Location *l = regions.FindUnit(q->target);
        if (!l) continue;
        std::string message = "Quest: ";
        message += l->unit->name;
        message += " commands the pirate galley '";
        message += (l->obj ? l->obj->name : "unknown galley");
        message += "', last sighted in the ";
        message += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
        message += " of ";
        message += l->region->name;
        message += ". Bring proof of their destruction and claim the bounty!";
        write_times_article(message);
        delete l;
    }

    if(rulesetSpecificData.value("victory_type", "") == "city_vote") {
        std::map <int, int> votes; // track votes per faction id
        int total_cities = 0; // total cities possible for vote count

        for(const auto r : regions) {
            // Ignore anything but the surface
            if (r->level->levelType != ARegionArray::LEVEL_SURFACE) continue;
            if (!r->town || (r->town->TownType() != TOWN_CITY)) continue;

            total_cities++;

            string name = r->town->name;
            string possible_faction = name.substr(0, name.find_first_of(" \t\n"));
            // The first word of the name was not all numeric, don't count for anyone
            if (!all_of(
                possible_faction.begin(),
                possible_faction.end(),
                [](unsigned char ch){ return std::isdigit(ch); }
            )) continue;
            // Now that we know it's all numeric, convert it to an int
            int faction_id = stoi(possible_faction);

            // Make sure it's a valid faction
            Faction *f = GetFaction(factions, faction_id);
            if (!f || f->is_npc) continue;

            auto vote = votes.find(faction_id);
            if (vote == votes.end()) {
                votes[faction_id] = 1;
            } else {
                vote->second++;
            }
        }

        // Set up the voting result to be reported if we are far enough in
        string message = "Voting results: \n";

        int max_vote = -1;
        bool tie = false;
        Faction *maxFaction = nullptr;
        for (const auto& vote : votes) {
            Faction *f = GetFaction(factions, vote.first);
            if (vote.second > max_vote) {
                max_vote = vote.second;
                maxFaction = f;
                tie = false;
            } else if (vote.second == max_vote) {
                tie = true;
                maxFaction = nullptr;
            }
            message += "Faction " + f->name + " has " + to_string(vote.second) + " votes.\n";
        }

        // See if we have enough votes to even report the info.  Since a win requires 50% + 1, we can start reporting
        // once someone has more than 25% of the cities.
        if (max_vote > (total_cities / 4)) {
            // Now see if we have a winner at all
            if (max_vote > ((total_cities / 2) + 1)) {
                winner = maxFaction;
                message += "\n" + winner->name + " has enough votes and has won the game!";
            } else {
                int percent = floor((max_vote * 100) / total_cities);
                if (tie) {
                    message += "\nThere is a tie for the most votes with multiple factions having ";
                } else {
                    message += "\nThe current leader is " + maxFaction->name + " with ";
                }
                message += to_string(max_vote) + "/" + to_string(total_cities) + " votes (" + to_string(percent) + "%).";
            }
            write_times_article(message);
        }
    }

    // Check for victory conditions for the annihilation win
    if(rulesetSpecificData.value("victory_type", "") == "annihilation") {
        // before turn 50 none of the end game code will be active.
        if (TurnNumber() < 50) return nullptr;

        int empowered_altars = report_and_count_empowered_altars(regions, factions);
        // If we have hit turn 70, rather than spawning a new anomaly, we will activate a random altar and if
        // needed, destroy a randomly chosen entity.
        if (TurnNumber() >= 70 && empowered_altars < 6) {
            empower_random_altar(regions, factions);
            empowered_altars++;
        }

        int entities = report_and_count_entities(regions, factions);
        int anomalies = report_and_count_anomalies(regions, factions);

        int completed_entities = empowered_altars + entities;

        // This function will count the number of entities in the game + number of activated altars.
        // If this number is < 6, then we have a chance of spawning a new anomaly.  This chance starts at 10%
        // for the first anomaly after turn 50 and then increases by 12% for each entity or active altar already
        // existing.
        if (completed_entities < 6) {
            int chance = 10 + (completed_entities * 12);
            logger::write("Endgame: entities: " + std::to_string(completed_entities) + ", anomalies: " +
                std::to_string(anomalies) + ", chance: " + std::to_string(chance) + "%");
            if (rng::get_random(100) < chance) {
                // Okay, let's see if we can spawn a new entity
                // If we can, see if we already have those anomalies and report them to all factions if so.
                if (anomalies + completed_entities >= 6) {
                    // We have all the anomalies that we can have still, so cannot spawn any more.
                    return nullptr;
                }

                // Ok, we can spawn a new anomaly.  Let's do it.
                // We want a random land hex that is not a city and that does not have an anomaly and is not guarded.
                ARegion *r = nullptr;
                ARegionArray *surface = regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
                while (r == nullptr) {
                    r = (ARegion *)surface->GetRegion(rng::get_random(surface->x), rng::get_random(surface->y));
                    if (r == nullptr) continue;

                    // An anomaly won't spawn in the ocean or in a barren region or in a city or a guarded region.
                    TerrainType type = TerrainDefs[r->type];
                    if (type.similar_type == R_OCEAN || type.similar_type == R_BARREN || r->town || r->IsGuarded()) {
                        r = nullptr;
                        continue;
                    }
                    // Make sure it doesn't already have an anomaly
                    for(const auto o : r->objects) {
                        if (o->type == O_ENTITY_CAGE) {
                            r = nullptr;
                            break;
                        }
                    }
                    if(!r) continue;
                }

                // Okay, we have a new region to spawn an anomaly in.  Let's do it.
                Object *o = new Object(r);
                ObjectType ob = ObjectDefs[O_ENTITY_CAGE];
                o->type = O_ENTITY_CAGE;
                o->num = r->buildingseq++;
                o->set_name(ob.name);
                if (ob.flags & ObjectType::SACRIFICE) {
                    o->incomplete = -(ob.sacrifice_amount);
                }
                r->objects.push_back(o);
                // Now tell all the factions about it.
                for(const auto f : factions) {
                    if (f->is_npc) continue;
                    f->event("A strange anomaly has appeared in " + r->short_print() + ".", "anomaly", r);
                }
                logger::write("Spawned new anomaly at " + r->short_print() + ".");
            }

            // If we haven't completed all the entities, then noone can win yet.
            return nullptr;
        }

        // We have all entities completed, but.. have all altars been empowered?  if not, nooone can win yet.
        if (empowered_altars < 6) {
            logger::write("Only " + std::to_string(empowered_altars) + " altars have been empowered, no winner yet.");
            return nullptr;
        }

        // Ok we have completed all entities, so we can check for if a faction has won.
        // the winner will be the owner of the world breaker monolith if and only if either
        // 1) all living factions are allied with the owner of the monolith
        // or
        // 2) the entire surface has been annihilated.

        // FInd the owner of the monolith.
        ARegionArray *underworld = regions.GetRegionArray(ARegionArray::LEVEL_UNDERWORLD);
        ARegion *center = underworld->GetRegion(underworld->x / 2, underworld->y / 2);
        for(const auto o : center->objects) {
            if (o->type == O_ACTIVE_MONOLITH) {
                Unit *owner = o->GetOwner();
                // If noone owns the monolith, then we have no possible winner yet.
                if (owner) winner = owner->faction;
                break;
            }
        }

        // No one owns the monolith, so no one can win yet.
        if (!winner) {
            if (TurnNumber() < 100) {
                // If it's before turn 100, we can't declare a winner yet
                logger::write("No monolith owner found, no winner yet.");
                return nullptr;
            }
            // If the monolith is unowned on turn 100 or later, the monsters win.
            return GetFaction(factions, monfaction); // monsters win
        }

        logger::write("Checking for winner: " + winner->name);
        // Ok, we have a possible winner, check for sufficient alive factions mutually allied to the monolith owner.
        int allied_count = 0;
        int total_factions = 0;
        for(const auto f : factions) {
            if (f->is_npc) continue;
            if (f == winner) continue;
            total_factions++;
            // This faction is not allied to the monolith owner, so they don't count
            if (f->get_attitude(winner->num) != AttitudeType::ALLY) continue;
            // The winner is not allied to this faction, so they don't count;
            if (winner->get_attitude(f->num) != AttitudeType::ALLY) continue;
            allied_count++;
        }

        int needed_percent = rulesetSpecificData.value("allied_percent", 100);
        int current_percent = (allied_count * 100) / total_factions;
        logger::write("Factions allied: " + std::to_string(allied_count) + "/" + std::to_string(total_factions) + " (" +
            std::to_string(current_percent) + "% out of " + std::to_string(needed_percent) + "%)");
        if (current_percent >= needed_percent) {
            logger::write("Enough factions allied to the monolith owner, so they win.");
            // We have enough factions allied to the monolith owner, so they win.
            return winner;
        }

        // No winner yet, so check if the surface has been completely destroyed.
        int total_surface = 0;
        int total_annihilated = 0;
        for(const auto r : regions) {
            if (r->level->levelType != ARegionArray::LEVEL_SURFACE) continue;
            total_surface++;
            if (TerrainDefs[r->type].flags & TerrainType::ANNIHILATED) total_annihilated++;
        }

        int needed_surface = rulesetSpecificData.value("annihilate_percent", 100);
        int current_surface = (total_annihilated * 100) / total_surface;
        logger::write("Surface annihilated: " + std::to_string(total_annihilated) + "/" + std::to_string(total_surface) +
            " (" + std::to_string(current_surface) + "% out of " + std::to_string(needed_surface) + "%)");
        if (current_surface >= needed_surface) {
            logger::write("Sufficient surface annihilated, so the monolith owner wins.");
            // The surface has been sufficiently destroyed, so the monolith owner wins.
            return winner;
        }
        // No winner yet, so clear the potential winner and return null.
        logger::write("No winner yet.");
        winner = nullptr;
    }

    // Trident coronation victory (Phase B). Dormant unless rulesetSpecificData
    // victory_type == "coronation"; engine-side so it stays unit-testable.
    if (Faction *crowned = check_coronation()) return crowned;

    return winner;
}

void Game::ModifyTablesPerRuleset(void)
{
    if (Globals->APPRENTICES_EXIST)
        EnableSkill(S_MANIPULATE);

    if (!Globals->GATES_EXIST)
        DisableSkill(S_GATE_LORE);

    if (Globals->FULL_TRUESEEING_BONUS) {
        ModifyAttribMod("observation", 1, AttribModItem::SKILL, "TRUE", AttribModItem::UNIT_LEVEL, 1);
    }
    if (Globals->IMPROVED_AMTS) {
        ModifyAttribMod("observation", 2, AttribModItem::ITEM, "AMTS", AttribModItem::CONSTANT, 3);
    }
    if (Globals->FULL_INVIS_ON_SELF) {
        ModifyAttribMod("stealth", 3, AttribModItem::SKILL, "INVI", AttribModItem::UNIT_LEVEL, 1);
    }

    if (Globals->NEXUS_IS_CITY && Globals->TOWNS_EXIST) {
        ClearTerrainRaces(R_NEXUS);
        ModifyTerrainRace(R_NEXUS, 0, I_HIGHELF);
        ModifyTerrainRace(R_NEXUS, 1, I_MAN);
        ModifyTerrainRace(R_NEXUS, 2, I_HILLDWARF);
        ClearTerrainItems(R_NEXUS);
        ModifyTerrainItems(R_NEXUS, 0, I_IRON, 100, 10);
        ModifyTerrainItems(R_NEXUS, 1, I_WOOD, 100, 10);
        ModifyTerrainItems(R_NEXUS, 2, I_STONE, 100, 10);
        ModifyTerrainEconomy(R_NEXUS, 1000, 15, 50, 2);
    }

    // set up game specific tracked data
    rulesetSpecificData.clear();

    // this set is for the NO7 annihilation win condition, and is active for NO7
//    rulesetSpecificData["victory_type"] = "annihilation";
//    rulesetSpecificData["allowed_annihilates"] = 3;
//    rulesetSpecificData["allied_percent"] = 50;
//    rulesetSpecificData["annihilate_percent"] = 10;
//    rulesetSpecificData["random_annihilates"] = true;

    // this set is for the city vote win condition, and was not active for NO7
    // rulesetSpecificData["victory_type"] = "city_vote";

    // Trident coronation win — ENABLED on this Trident release branch. On dev
    // (Arcanum, neworigins-v8-newage) these lines stay commented so the engine ships
    // dormant. See Game::check_coronation() and docs/TRIDENT_VICTORY_MECHANIC_DESIGN.md.
    rulesetSpecificData["victory_type"]     = "coronation";
    rulesetSpecificData["crowns_to_win"]    = 3;
    rulesetSpecificData["coronation_turns"] = 5;

    // Pirate map-chance turn ramp + hideout-supply throttle — active on both Arcanum
    // (dev) and Trident (prod), unlike the coronation flag above. See
    // docs/PIRATE_MAP_CHANCE_RAMP_PLAN.md.
    rulesetSpecificData["map_chance_ramp_turns"]     = 60;
    rulesetSpecificData["map_chance_ramp_bonus"]     = 0.5;
    rulesetSpecificData["tmap_share_early"]          = 10;
    rulesetSpecificData["tmap_share_late"]           = 20;
    rulesetSpecificData["hideout_soft_cap_percent"]  = 7;

    EnableItem(I_CAMEL);
    EnableItem(I_MCROSSBOW);
    EnableItem(I_MWAGON);
    EnableItem(I_GLIDER);
    EnableItem(I_LEATHERARMOR);
    EnableItem(I_MCHAIN);  // Mithril chain armor (MCAR) - light chain tier 2
    EnableItem(I_SPEAR);
    EnableItem(I_JAVELIN);
    EnableItem(I_MSHIELD);
    EnableItem(I_ISHIELD);
    EnableItem(I_WSHIELD);
    EnableItem(I_ASHIELD);  // Adamantium shield (ASHD) - highest defense,
    EnableItem(I_AEGIS);
    EnableItem(I_WINDCHIME);
    EnableItem(I_GATE_CRYSTAL);
    EnableItem(I_STAFFOFH);
    EnableItem(I_SCRYINGORB);
    EnableItem(I_CORNUCOPIA);
    EnableItem(I_BOOKOFEXORCISM);
    EnableItem(I_HOLYSYMBOL);
    EnableItem(I_CENSER);
    EnableItem(I_FSWORD);
    EnableItem(I_MUSHROOM);
    EnableItem(I_HEALPOTION);
    EnableItem(I_GEMS);
    EnableItem(I_PIKE);
    EnableItem(I_LANCE);
    EnableItem(I_BAXE);
    EnableItem(I_MBAXE);
    EnableItem(I_ADBAXE);
    EnableItem(I_BHAMMER);
    EnableItem(I_MBHAM);
    EnableItem(I_ABHAM);
    // Tools
    EnableItem(I_PICK);
    EnableItem(I_AXE);
    EnableItem(I_HAMMER);
    EnableItem(I_NET);
    EnableItem(I_LASSO);
    EnableItem(I_BAG);
    EnableItem(I_SPINNING);

    // FMI
    EnableItem(I_CATAPULT);
    EnableItem(I_STEEL_DEFENDER);

    //
    // Change craft: adamantium
    //
    EnableItem(I_ADMANTIUM);
    EnableItem(I_ADSWORD);
    EnableItem(I_ADRING);
    EnableItem(I_ADPLATE);
    ModifyItemProductionSkill(I_ADMANTIUM, "MINI", 5);

    // Artifacts of power
    DisableItem(I_RELICOFGRACE);

    // Trade goods - enable 7 additional items (total pool: 18)
    // Must use ModifyItemFlags(0) to clear both DISABLED and NOMARKET
    ModifyItemFlags(I_FIGURINES, 0);
    ModifyItemFlags(I_CAVIAR, 0);
    ModifyItemFlags(I_ROSES, 0);
    ModifyItemFlags(I_VELVET, 0);
    ModifyItemFlags(I_MINK, 0);
    ModifyItemFlags(I_DYES, 0);
    ModifyItemFlags(I_WOOL, 0);
    ModifyItemBasePrice(I_VELVET, 90);
    ModifyItemBasePrice(I_ROSES, 80);
    ModifyItemBasePrice(I_MINK, 80);

    // Disable items
    DisableItem(I_SUPERBOW);
    DisableItem(I_BOOTS);
    DisableItem(I_CLOTHARMOR);
    DisableItem(I_ROUGHGEM);

    // No staff of lightning
    DisableSkill(S_CREATE_STAFF_OF_LIGHTNING);
    DisableItem(I_STAFFOFL);

    // Trident: mages cannot enchant mithril weapons/armor/shields (balance).
    // Swords and armor are enabled by default in gamedata; shields would otherwise
    // be enabled below — so it is left un-enabled and all three are disabled here.
    DisableSkill(S_ENCHANT_SWORDS);   // no I_MSWORD production
    DisableSkill(S_ENCHANT_ARMOR);    // no I_MPLATE production
    DisableSkill(S_ENCHANT_SHIELDS);  // no I_MSHIELD production

    EnableSkill(S_CREATE_AEGIS);
    EnableSkill(S_CREATE_WINDCHIME);
    EnableSkill(S_CREATE_GATE_CRYSTAL);
    EnableSkill(S_CREATE_STAFF_OF_HEALING);
    EnableSkill(S_CREATE_SCRYING_ORB);
    EnableSkill(S_CREATE_CORNUCOPIA);
    EnableSkill(S_CREATE_BOOK_OF_EXORCISM);
    EnableSkill(S_CREATE_HOLY_SYMBOL);
    EnableSkill(S_CREATE_CENSER);
    EnableSkill(S_CREATE_FLAMING_SWORD);
    EnableSkill(S_TRANSMUTATION);
    EnableSkill(S_CALL_PIRATES);
    DisableSkill(S_CAMELTRAINING);
    DisableSkill(S_RANCHING);

    // No endurance
    DisableSkill(S_ENDURANCE);

    DisableSkill(S_GEMCUTTING);

    // Food
    EnableSkill(S_COOKING);
    EnableItem(I_FOOD);

    // Magic

    ModifySkillDependancy(S_RAISE_UNDEAD, 0, "SUSK", 3);
    ModifySkillDependancy(S_SUMMON_LICH, 0, "RAIS", 3);
    ModifySkillDependancy(S_DRAGON_LORE, 1, "WOLF", 3);

    ModifyItemMagicOutput(I_SKELETON, 200);
    ModifyItemMagicOutput(I_WOLF, 200);
    ModifyItemMagicOutput(I_IMP, 200);
    ModifyItemMagicOutput(I_UNDEAD, 100);
    ModifyItemMagicOutput(I_EAGLE, 100);
    ModifyItemMagicOutput(I_DEMON, 100);
    ModifyItemMagicOutput(I_DRAGON, 20);
    ModifyItemMagicOutput(I_LICH, 30);
    ModifyItemEscape(I_IMP, ItemType::ESC_LEV_LINEAR | ItemType::LOSE_LINKED, "SUIM", 20);
    ModifyItemEscape(I_DEMON, ItemType::ESC_LEV_LINEAR | ItemType::LOSE_LINKED, "SUDE", 20);
    ModifyItemEscape(I_BALROG, ItemType::ESC_LEV_LINEAR | ItemType::LOSE_LINKED, "SUBA", 20);

    //
    // Roads
    //
    EnableObject(O_ROADN);
    EnableObject(O_ROADNE);
    EnableObject(O_ROADNW);
    EnableObject(O_ROADS);
    EnableObject(O_ROADSE);
    EnableObject(O_ROADSW);
    ModifyObjectConstruction(O_ROADN, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADNE, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADNW, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADS, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADSE, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADSW, I_STONE, 30, "BUIL", 2);

    EnableObject(O_TEMPLE);
    EnableObject(O_MQUARRY);
    EnableObject(O_AMINE);
    EnableObject(O_PRESERVE);
    EnableObject(O_SACGROVE);
    EnableObject(O_MTOWER);
    EnableObject(O_MFORTRESS);
    EnableObject(O_MCITADEL);
    EnableObject(O_STABLE);
    EnableObject(O_MSTABLE);
    EnableObject(O_HUT);
    EnableObject(O_TRAPPINGLODGE);
    EnableObject(O_FAERIERING);
    EnableObject(O_ALCHEMISTLAB);
    EnableObject(O_OASIS);
    EnableObject(O_TRAPPINGHUT);
    EnableObject(O_TOWN_HALL);
    EnableObject(O_CANAL);
    EnableObject(O_MCANAL);

    DisableObject(O_GEMAPPRAISER);
    // O_PALACE is enabled in NewAge (capital seat for the Trident victory mechanic).

    ModifyObjectName(O_MFORTRESS, "Magical Fortress");
    ModifyObjectName(O_MCASTLE, "Magical Castle");

    EnableObject(O_ISLE);
    EnableObject(O_DERELICT);
    EnableObject(O_OCAVE);
    EnableObject(O_WHIRL);

    // Dungeon system — one entrance object used on both sides (surface + inside).
    // See docs/DUNGEON_SYSTEM_DESIGN.md
    EnableObject(O_DUNGEON_ENTRANCE);

    //
    // Monsters
    //
    EnableItem(I_PIRATES);
    EnableItem(I_KRAKEN);
    EnableItem(I_MERFOLK);
    EnableItem(I_ELEMENTAL);
    EnableItem(I_HYDRA);
    EnableItem(O_BOG);
    EnableItem(I_ICEDRAGON);
    EnableItem(O_ICECAVE);
    EnableItem(I_ILLYRTHID);
    EnableItem(O_ILAIR);
    EnableItem(I_DEVIL);

    EnableItem(I_STORMGIANT);
    EnableItem(I_CLOUDGIANT);
    EnableItem(O_GIANTCASTLE);

    EnableItem(I_WARRIORS);

    EnableItem(I_DARKMAGE);
    EnableItem(O_DARKTOWER);

    EnableItem(I_MAGICIANS);
    EnableItem(O_MAGETOWER);

    //
    // Change races
    //
    DisableItem(I_ESKIMO);
    DisableItem(I_TRIBESMAN);
    DisableItem(I_NOMAD);
    DisableItem(I_TRIBALELF);
    DisableItem(I_VIKING);
    DisableItem(I_BARBARIAN);
    DisableItem(I_DARKMAN);
    DisableItem(I_DESERTDWARF);
    DisableItem(I_PLAINSMAN);
    DisableItem(I_SEAELF);
    DisableItem(I_GREYELF);
    DisableItem(I_MINOTAUR);
    DisableItem(I_OGREMAN);
    DisableItem(I_GNOLL);

    ModifyItemBasePrice(I_LEADERS, 800);

    EnableItem(I_MAN);
    ModifyItemBasePrice(I_MAN, 40);
    modify_race_skill_levels("HUMN", 2, 4);
    modify_race_skills("HUMN", 0, "OBSE");
    modify_race_skills("HUMN", 1, "STEA");
    modify_race_skills("HUMN", 2, "TACT");

    EnableItem(I_HILLDWARF);
    ModifyItemBasePrice(I_HILLDWARF, 40);
    modify_race_skill_levels("HDWA", 5, 2);
    modify_race_skills("HDWA", 0, "ARMO");
    modify_race_skills("HDWA", 1, "WEAP");
    modify_race_skills("HDWA", 2, "QUAR");
    modify_race_skills("HDWA", 3, "MINI");
    modify_race_skills("HDWA", 4, "BUIL");

    EnableItem(I_ICEDWARF);
    ModifyItemBasePrice(I_ICEDWARF, 40);
    modify_race_skill_levels("IDWA", 5, 2);
    modify_race_skills("IDWA", 0, "COMB");
    modify_race_skills("IDWA", 1, "WEAP");
    modify_race_skills("IDWA", 2, "MINI");
    modify_race_skills("IDWA", 3, "FISH");
    modify_race_skills("IDWA", 4, "ARMO");

    EnableItem(I_HIGHELF);
    ModifyItemBasePrice(I_HIGHELF, 40);
    modify_race_skill_levels("HELF", 5, 2);
    modify_race_skills("HELF", 0, "HORS");
    modify_race_skills("HELF", 1, "FISH");
    modify_race_skills("HELF", 2, "LBOW");
    modify_race_skills("HELF", 3, "SHIP");
    modify_race_skills("HELF", 4, "SAIL");

    EnableItem(I_WOODELF);
    ModifyItemBasePrice(I_WOODELF, 40);
    modify_race_skill_levels("WELF", 5, 2);
    modify_race_skills("WELF", 0, "LUMB");
    modify_race_skills("WELF", 1, "LBOW");
    modify_race_skills("WELF", 2, "HUNT");
    modify_race_skills("WELF", 3, "CARP");
    modify_race_skills("WELF", 4, "WEAP");
    modify_race_skills("WELF", 5, "COOK");

    EnableItem(I_GNOME);
    ModifyItemBasePrice(I_GNOME, 30);
    modify_race_skill_levels("GNOM", 5, 2);
    modify_race_skills("GNOM", 0, "HERB");
    modify_race_skills("GNOM", 1, "QUAR");
    modify_race_skills("GNOM", 2, "ENTE");
    modify_race_skills("GNOM", 3, "XBOW");
    modify_race_skills("GNOM", 4, "HEAL");
    modify_race_skills("GNOM", 5, "CARP");
    ModifyItemCapacities(I_GNOME,7,0,0,0);
    ModifyItemWeight(I_GNOME, 5);

    EnableItem(I_CENTAURMAN);
    ModifyItemBasePrice(I_CENTAURMAN, 70);
    modify_race_skill_levels("CTAU", 5, 2);
    modify_race_skills("CTAU", 0, "LUMB");
    modify_race_skills("CTAU", 1, "HORS");
    modify_race_skills("CTAU", 2, "RIDI");
    modify_race_skills("CTAU", 3, "HEAL");
    modify_race_skills("CTAU", 4, "FARM");

    EnableItem(I_LIZARDMAN);
    ModifyItemBasePrice(I_LIZARDMAN, 40);
    modify_race_skill_levels("LIZA", 5, 2);
    modify_race_skills("LIZA", 0, "HUNT");
    modify_race_skills("LIZA", 1, "HERB");
    modify_race_skills("LIZA", 2, "CARP");
    modify_race_skills("LIZA", 3, "SAIL");
    modify_race_skills("LIZA", 4, "HEAL");

    EnableItem(I_GOBLINMAN);
    ModifyItemBasePrice(I_GOBLINMAN, 30);
    modify_race_skill_levels("GBLN", 5, 2);
    modify_race_skills("GBLN", 0, "QUAR");
    modify_race_skills("GBLN", 1, "XBOW");
    modify_race_skills("GBLN", 2, "HERB");
    modify_race_skills("GBLN", 3, "WEAP");
    modify_race_skills("GBLN", 4, "ENTE");
    modify_race_skills("GBLN", 5, "HEAL");
    ModifyItemCapacities(I_GOBLINMAN,7,0,0,0);
    ModifyItemWeight(I_GOBLINMAN, 5);

    EnableItem(I_HOBBIT);
    ModifyItemBasePrice(I_HOBBIT, 30);
    ModifyItemCapacities(I_HOBBIT,7,0,0,0);
    ModifyItemWeight(I_HOBBIT, 5);
    modify_race_skill_levels("HOBB", 5, 2);
    modify_race_skills("HOBB", 0, "HEAL");
    modify_race_skills("HOBB", 1, "HERB");
    modify_race_skills("HOBB", 2, "XBOW");
    modify_race_skills("HOBB", 3, "FARM");
    modify_race_skills("HOBB", 4, "ENTE");
    modify_race_skills("HOBB", 5, "COOK");

    EnableItem(I_ORC);
    ModifyItemBasePrice(I_ORC, 40);
    modify_race_skill_levels("ORC", 5, 2);
    modify_race_skills("ORC", 0, "MINI");
    modify_race_skills("ORC", 1, "LUMB");
    modify_race_skills("ORC", 2, "COMB");
    modify_race_skills("ORC", 3, "BUIL");
    modify_race_skills("ORC", 4, "SHIP");
    modify_race_skills("ORC", 5, "QUAR");

    // Underworld races
    EnableItem(I_DROWMAN);
    ModifyItemBasePrice(I_DROWMAN, 40);
    modify_race_skill_levels("DRLF", 5, 2);
    modify_race_skills("DRLF", 0, "SAIL");
    modify_race_skills("DRLF", 1, "HUNT");
    modify_race_skills("DRLF", 2, "LBOW");
    modify_race_skills("DRLF", 3, "LUMB");
    modify_race_skills("DRLF", 4, "COOK");

    EnableItem(I_UNDERDWARF);
    ModifyItemBasePrice(I_UNDERDWARF, 40);
    modify_race_skill_levels("UDWA", 5, 2);
    modify_race_skills("UDWA", 0, "WEAP");
    modify_race_skills("UDWA", 1, "ARMO");
    modify_race_skills("UDWA", 2, "QUAR");
    modify_race_skills("UDWA", 3, "MINI");
    modify_race_skills("UDWA", 4, "BUIL");
    modify_race_skills("UDWA", 5, "COMB");

    EnableItem(I_FAIRY);
    ModifyItemBasePrice(I_FAIRY, 200);
    modify_race_skill_levels("FAIR", 4, 2);
    modify_race_skills("FAIR", 0, "OBSE");
    modify_race_skills("FAIR", 1, "HEAL");
    modify_race_skills("FAIR", 2, "HERB");
    modify_race_skills("FAIR", 3, "ENTE");
    ModifyItemCapacities(I_FAIRY,7,0,7,0);
    ModifyItemWeight(I_FAIRY, 5);

    EnableItem(I_TIEFLING);
    ModifyItemBasePrice(I_TIEFLING, 200);
    modify_race_skill_levels("TIEF", 5, 2);
    modify_race_skills("TIEF", 0, "STEA");
    modify_race_skills("TIEF", 1, "COMB");
    ModifyItemCapacities(I_TIEFLING,15,0,0,0);
    ModifyItemWeight(I_TIEFLING, 10);

    //
    // Change races per terrain
    //

    // Upper world
    // TODO: add ocean

    ClearTerrainRaces(R_PLAIN);
    ModifyTerrainRace(R_PLAIN, 0, I_HIGHELF);
    ModifyTerrainRace(R_PLAIN, 1, I_CENTAURMAN);
    ModifyTerrainRace(R_PLAIN, 2, I_HOBBIT);
    ModifyTerrainRace(R_PLAIN, 3, I_MAN);
    ModifyTerrainCoastRace(R_PLAIN, 0, I_HIGHELF);
    ModifyTerrainCoastRace(R_PLAIN, 1, I_MAN);
    ModifyTerrainCoastRace(R_PLAIN, 2, I_HIGHELF);
    ModifyTerrainEconomy(R_PLAIN, 600, 12, 30, 1);

    ClearTerrainRaces(R_FOREST);
    ModifyTerrainRace(R_FOREST, 0, I_WOODELF);
    ModifyTerrainRace(R_FOREST, 1, I_GOBLINMAN);
    ModifyTerrainRace(R_FOREST, 2, I_MAN);
    ModifyTerrainRace(R_FOREST, 3, I_WOODELF);
    ModifyTerrainCoastRace(R_FOREST, 0, I_WOODELF);
    ModifyTerrainCoastRace(R_FOREST, 1, I_ORC);
    ModifyTerrainCoastRace(R_FOREST, 2, I_HIGHELF);
    ModifyTerrainEconomy(R_FOREST, 450, 12, 18, 2);

    ClearTerrainRaces(R_MOUNTAIN);
    ModifyTerrainRace(R_MOUNTAIN, 0, I_HILLDWARF);
    ModifyTerrainRace(R_MOUNTAIN, 1, I_ORC);
    ModifyTerrainRace(R_MOUNTAIN, 2, I_HILLDWARF);
    ModifyTerrainCoastRace(R_MOUNTAIN, 0, I_HILLDWARF);
    ModifyTerrainCoastRace(R_MOUNTAIN, 1, I_ORC);
    ModifyTerrainCoastRace(R_MOUNTAIN, 2, I_GNOME);
    ModifyTerrainEconomy(R_MOUNTAIN, 300, 11, 10, 2);

    ClearTerrainRaces(R_HILL);
    ModifyTerrainRace(R_HILL, 0, I_HILLDWARF);
    ModifyTerrainRace(R_HILL, 1, I_ORC);
    ModifyTerrainRace(R_HILL, 2, I_MAN);
    ModifyTerrainRace(R_HILL, 3, I_HOBBIT);
    ModifyTerrainCoastRace(R_HILL, 0, I_ORC);
    ModifyTerrainCoastRace(R_HILL, 1, I_MAN);
    ModifyTerrainCoastRace(R_HILL, 2, I_HILLDWARF);
    ModifyTerrainEconomy(R_HILL, 450, 12, 18, 2);

    ClearTerrainRaces(R_SWAMP);
    ModifyTerrainRace(R_SWAMP, 0, I_LIZARDMAN);
    ModifyTerrainRace(R_SWAMP, 1, I_GOBLINMAN);
    ModifyTerrainRace(R_SWAMP, 2, I_GNOME);
    ModifyTerrainRace(R_SWAMP, 3, I_ORC);
    ModifyTerrainCoastRace(R_SWAMP, 0, I_LIZARDMAN);
    ModifyTerrainCoastRace(R_SWAMP, 1, I_MAN);
    ModifyTerrainCoastRace(R_SWAMP, 2, I_ORC);
    ModifyTerrainEconomy(R_SWAMP, 400, 11, 10, 2);

    ClearTerrainRaces(R_JUNGLE);
    ModifyTerrainRace(R_JUNGLE, 0, I_ORC);
    ModifyTerrainRace(R_JUNGLE, 1, I_MAN);
    ModifyTerrainRace(R_JUNGLE, 2, I_GOBLINMAN);
    ModifyTerrainRace(R_JUNGLE, 3, I_GNOME);
    ModifyTerrainCoastRace(R_JUNGLE, 0, I_ORC);
    ModifyTerrainCoastRace(R_JUNGLE, 1, I_MAN);
    ModifyTerrainCoastRace(R_JUNGLE, 2, I_LIZARDMAN);
    ModifyTerrainEconomy(R_JUNGLE, 400, 11, 18, 2);

    ClearTerrainRaces(R_DESERT);
    ModifyTerrainRace(R_DESERT, 0, I_CENTAURMAN);
    ModifyTerrainRace(R_DESERT, 1, I_GOBLINMAN);
    ModifyTerrainRace(R_DESERT, 2, I_MAN);
    ModifyTerrainCoastRace(R_DESERT, 0, I_ORC);
    ModifyTerrainCoastRace(R_DESERT, 1, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_DESERT, 2, I_MAN);
    ModifyTerrainEconomy(R_DESERT, 350, 11, 10, 1);

    ClearTerrainRaces(R_TUNDRA);
    ModifyTerrainRace(R_TUNDRA, 0, I_ICEDWARF);
    ModifyTerrainRace(R_TUNDRA, 1, I_GNOME);
    ModifyTerrainRace(R_TUNDRA, 2, I_MAN);
    ModifyTerrainRace(R_TUNDRA, 3, I_ICEDWARF);
    ModifyTerrainCoastRace(R_TUNDRA, 0, I_ICEDWARF);
    ModifyTerrainCoastRace(R_TUNDRA, 1, I_GNOME);
    ModifyTerrainCoastRace(R_TUNDRA, 2, I_MAN);
    ModifyTerrainEconomy(R_TUNDRA, 350, 11, 10, 2);

    ModifyTerrainEconomy(R_VOLCANO, 50, 6, 0, 4);

    ModifyTerrainEconomy(R_LAKE, 50, 6, 0, 1);

    // Underworld terrain

    ClearTerrainRaces(R_CAVERN);
    ModifyTerrainRace(R_CAVERN, 0, I_DROWMAN);
    ModifyTerrainRace(R_CAVERN, 1, I_UNDERDWARF);
    ModifyTerrainRace(R_CAVERN, 2, I_ORC);
    ModifyTerrainCoastRace(R_CAVERN, 0, I_UNDERDWARF);
    ModifyTerrainCoastRace(R_CAVERN, 1, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_CAVERN, 2, I_ORC);
    ModifyTerrainEconomy(R_CAVERN, 300, 11, 10, 2);

    ClearTerrainRaces(R_UFOREST);
    ModifyTerrainRace(R_UFOREST, 0, I_DROWMAN);
    ModifyTerrainRace(R_UFOREST, 1, I_GNOME);
    ModifyTerrainRace(R_UFOREST, 2, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_UFOREST, 0, I_DROWMAN);
    ModifyTerrainCoastRace(R_UFOREST, 1, I_GNOME);
    ModifyTerrainCoastRace(R_UFOREST, 2, I_DROWMAN);
    ModifyTerrainEconomy(R_UFOREST, 300, 11, 10, 2);

    ClearTerrainRaces(R_TUNNELS);
    ModifyTerrainEconomy(R_TUNNELS, 0, 0, 0, 2);

    // Underdeep terrain

    ClearTerrainRaces(R_CHASM);
    ModifyTerrainRace(R_CHASM, 0, I_DROWMAN);
    ModifyTerrainRace(R_CHASM, 1, I_GNOME);
    ModifyTerrainRace(R_CHASM, 2, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_CHASM, 0, I_UNDERDWARF);
    ModifyTerrainCoastRace(R_CHASM, 1, I_DROWMAN);
    ModifyTerrainCoastRace(R_CHASM, 2, I_ORC);
    ModifyTerrainEconomy(R_CHASM, 250, 11, 10, 2);

    // Chasm is inhabited in this ruleset - population 250, wages 11, the same band
    // as grotto and deepforest above - but gamedata.cpp still hands it
    // TerrainType::BARREN, which across the engine means "no settlements here":
    // CREATE VILLAGE refuses it (monthorders.cpp), SetupPop skips it (economy.cpp)
    // and both generators drop it from their candidate lists. The two statements
    // contradicted each other, and the flag was winning.
    //
    // That cost the underdeep about a third of its usable ground: on 64x48 only
    // grotto and deepforest qualified, 72-109 hexes of 192, which is why the level
    // came out with 3-4 cities no matter how large the world was. Live Arcanum,
    // generated before economy_underground() existed, has 8 of its 18 underdeep
    // settlements on chasm and two of them have grown into towns - so a settled
    // chasm is proven, not a guess.
    //
    // Clearing just the one bit rather than assigning a flag word keeps whatever
    // else the terrain carries (SHOW_RULES) intact.
    ModifyTerrainFlags(R_CHASM, TerrainDefs[R_CHASM].flags & ~TerrainType::BARREN);

    ClearTerrainRaces(R_GROTTO);
    ModifyTerrainRace(R_GROTTO, 0, I_UNDERDWARF);
    ModifyTerrainRace(R_GROTTO, 1, I_ORC);
    ModifyTerrainRace(R_GROTTO, 2, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_GROTTO, 0, I_DROWMAN);
    ModifyTerrainEconomy(R_GROTTO, 250, 11, 12, 2);

    ClearTerrainRaces(R_DFOREST);
    ModifyTerrainRace(R_DFOREST, 0, I_DROWMAN);
    ModifyTerrainRace(R_DFOREST, 1, I_UNDERDWARF);
    ModifyTerrainRace(R_DFOREST, 2, I_ORC);
    ModifyTerrainRace(R_DFOREST, 3, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_DFOREST, 0, I_DROWMAN);
    ModifyTerrainCoastRace(R_DFOREST, 1, I_UNDERDWARF);
    ModifyTerrainCoastRace(R_DFOREST, 2, I_ORC);
    ModifyTerrainEconomy(R_DFOREST, 250, 11, 12, 2);

    // wandering monsters
    ModifyTerrainWMons(R_OCEAN,8,I_PIRATES,I_KRAKEN,I_MERFOLK);

    ModifyTerrainWMons(R_PLAIN,2,I_LION,I_BEHEMOTH,I_CENTAUR);
    ModifyTerrainWMons(R_FOREST,3,I_WOLF,I_TRENT,I_KOBOLD);
    ModifyTerrainWMons(R_MOUNTAIN,8,I_GBEAR,I_WYVERN,I_OGRE);
    ModifyTerrainWMons(R_HILL,3,I_GBEAR,I_ROC,I_OGRE);
    ModifyTerrainWMons(R_SWAMP,8,I_CROCODILE,I_BTHING,I_TROLL);
    ModifyTerrainWMons(R_JUNGLE,3,I_ANACONDA,I_KONG,I_WMEN);
    ModifyTerrainWMons(R_DESERT,8,I_SCORPION,I_SPHINX,I_SANDLING);
    ModifyTerrainWMons(R_TUNDRA,8,I_PBEAR,I_IWURM,I_YETI);

    ModifyTerrainWMons(R_VOLCANO,12,I_IMP,I_IFRIT,I_DEMON);
    ModifyTerrainWMons(R_LAKE,4,I_MERFOLK,I_ELEMENTAL,I_MERFOLK);

    ModifyTerrainWMons(R_CAVERN,12,I_RAT,I_DRAGON,I_GOBLIN);
    ModifyTerrainWMons(R_UFOREST,12,I_SPIDER,I_DRAGON,I_TROLL);
    ModifyTerrainWMons(R_TUNNELS,12,I_LIZARD,I_WYVERN,I_ETTIN);

    ModifyTerrainWMons(R_GROTTO,24,I_DEMON,I_DRAGON,I_IFRIT);
    ModifyTerrainWMons(R_DFOREST,24,I_LMEN,I_DRAGON,I_TROLL);
    ModifyTerrainWMons(R_CHASM,24,I_DEMON,I_DEVIL,I_ETTIN);

    // monster lairs
    ModifyTerrainLairChance(R_OCEAN, 10);
    ModifyTerrainLair(R_OCEAN, 0, O_ISLE);
    ModifyTerrainLair(R_OCEAN, 1, O_DERELICT);
    ModifyTerrainLair(R_OCEAN, 2, O_OCAVE);
    ModifyTerrainLair(R_OCEAN, 3, O_WHIRL);
    ModifyTerrainLair(R_OCEAN, 4, O_ISLE);
    ModifyTerrainLair(R_OCEAN, 5, O_OCAVE);

    ModifyTerrainLairChance(R_PLAIN, 10);
    ModifyTerrainLair(R_PLAIN, 0, O_RUIN);
    ModifyTerrainLair(R_PLAIN, 1, O_RUIN);
    ModifyTerrainLair(R_PLAIN, 2, O_CRYPT);
    ModifyTerrainLair(R_PLAIN, 3, O_CRYPT);
    ModifyTerrainLair(R_PLAIN, 4, O_MAGETOWER);
    ModifyTerrainLair(R_PLAIN, 5, -1);

    ModifyTerrainLairChance(R_FOREST, 10);
    ModifyTerrainLair(R_FOREST, 0, O_RUIN);
    ModifyTerrainLair(R_FOREST, 1, O_RUIN);
    ModifyTerrainLair(R_FOREST, 2, O_LAIR);
    ModifyTerrainLair(R_FOREST, 3, O_LAIR);
    ModifyTerrainLair(R_FOREST, 4, O_CRYPT);
    ModifyTerrainLair(R_FOREST, 5, -1);

    ModifyTerrainLairChance(R_MOUNTAIN, 25);
    ModifyTerrainLair(R_MOUNTAIN, 0, O_LAIR);
    ModifyTerrainLair(R_MOUNTAIN, 1, O_RUIN);
    ModifyTerrainLair(R_MOUNTAIN, 2, O_CAVE);
    ModifyTerrainLair(R_MOUNTAIN, 3, O_IFRITLAIR);
    ModifyTerrainLair(R_MOUNTAIN, 4, O_MAGETOWER);
    ModifyTerrainLair(R_MOUNTAIN, 5, O_GIANTCASTLE);

    ModifyTerrainLairChance(R_HILL, 15);
    ModifyTerrainLair(R_HILL, 0, O_LAIR);
    ModifyTerrainLair(R_HILL, 1, O_RUIN);
    ModifyTerrainLair(R_HILL, 2, O_LAIR);
    ModifyTerrainLair(R_HILL, 3, O_RUIN);
    ModifyTerrainLair(R_HILL, 4, O_MAGETOWER);
    ModifyTerrainLair(R_HILL, 5, O_CRYPT);

    ModifyTerrainLairChance(R_SWAMP, 20);
    ModifyTerrainLair(R_SWAMP, 0, O_LAIR);
    ModifyTerrainLair(R_SWAMP, 1, O_RUIN);
    ModifyTerrainLair(R_SWAMP, 2, O_LAIR);
    ModifyTerrainLair(R_SWAMP, 3, O_RUIN);
    ModifyTerrainLair(R_SWAMP, 4, O_BOG);
    ModifyTerrainLair(R_SWAMP, 5, O_CRYPT);

    ModifyTerrainLairChance(R_JUNGLE, 15);
    ModifyTerrainLair(R_JUNGLE, 0, O_LAIR);
    ModifyTerrainLair(R_JUNGLE, 1, O_RUIN);
    ModifyTerrainLair(R_JUNGLE, 2, O_LAIR);
    ModifyTerrainLair(R_JUNGLE, 3, O_RUIN);
    ModifyTerrainLair(R_JUNGLE, 4, O_BOG);
    ModifyTerrainLair(R_JUNGLE, 5, O_CRYPT);

    ModifyTerrainLairChance(R_DESERT, 20);
    ModifyTerrainLair(R_DESERT, 0, O_LAIR);
    ModifyTerrainLair(R_DESERT, 1, O_RUIN);
    ModifyTerrainLair(R_DESERT, 2, O_LAIR);
    ModifyTerrainLair(R_DESERT, 3, O_RUIN);
    ModifyTerrainLair(R_DESERT, 4, O_BOG);
    ModifyTerrainLair(R_DESERT, 5, O_CRYPT);

    ModifyTerrainLairChance(R_TUNDRA, 20);
    ModifyTerrainLair(R_TUNDRA, 0, O_BOG);
    ModifyTerrainLair(R_TUNDRA, 1, O_ICECAVE);
    ModifyTerrainLair(R_TUNDRA, 2, O_GIANTCASTLE);
    ModifyTerrainLair(R_TUNDRA, 3, O_RUIN);
    ModifyTerrainLair(R_TUNDRA, 4, O_CRYPT);
    ModifyTerrainLair(R_TUNDRA, 5, O_CRYPT);

    ModifyTerrainLairChance(R_VOLCANO, 25);
    ModifyTerrainLair(R_VOLCANO, 0, O_DEMONPIT);
    ModifyTerrainLair(R_VOLCANO, 1, O_IFRITLAIR);
    ModifyTerrainLair(R_VOLCANO, 2, O_GIANTCASTLE);
    ModifyTerrainLair(R_VOLCANO, 3, O_IFRITLAIR);
    ModifyTerrainLair(R_VOLCANO, 4, -1);
    ModifyTerrainLair(R_VOLCANO, 5, -1);

    ModifyTerrainLairChance(R_LAKE, 10);
    ModifyTerrainLair(R_LAKE, 0, O_ISLE);
    ModifyTerrainLair(R_LAKE, 1, O_OCAVE);
    ModifyTerrainLair(R_LAKE, 2, O_DERELICT);
    ModifyTerrainLair(R_LAKE, 3, O_ISLE);
    ModifyTerrainLair(R_LAKE, 4, O_OCAVE);
    ModifyTerrainLair(R_LAKE, 5, -1);

    ModifyTerrainLairChance(R_CAVERN, 20);
    ModifyTerrainLair(R_CAVERN, 0, O_LAIR);
    ModifyTerrainLair(R_CAVERN, 1, O_RUIN);
    ModifyTerrainLair(R_CAVERN, 2, O_IFRITLAIR);
    ModifyTerrainLair(R_CAVERN, 3, O_ILAIR);
    ModifyTerrainLair(R_CAVERN, 4, O_CAVE);
    ModifyTerrainLair(R_CAVERN, 5, O_DARKTOWER);

    ModifyTerrainLairChance(R_UFOREST, 20);
    ModifyTerrainLair(R_UFOREST, 0, O_LAIR);
    ModifyTerrainLair(R_UFOREST, 1, O_RUIN);
    ModifyTerrainLair(R_UFOREST, 2, O_GIANTCASTLE);
    ModifyTerrainLair(R_UFOREST, 3, O_ILAIR);
    ModifyTerrainLair(R_UFOREST, 4, O_CAVE);
    ModifyTerrainLair(R_UFOREST, 5, O_DARKTOWER);

    ModifyTerrainLairChance(R_TUNNELS, 20);
    ModifyTerrainLair(R_TUNNELS, 0, O_LAIR);
    ModifyTerrainLair(R_TUNNELS, 1, O_RUIN);
    ModifyTerrainLair(R_TUNNELS, 2, O_GIANTCASTLE);
    ModifyTerrainLair(R_TUNNELS, 3, O_DEMONPIT);
    ModifyTerrainLair(R_TUNNELS, 4, O_CAVE);
    ModifyTerrainLair(R_TUNNELS, 5, O_DARKTOWER);

    ModifyTerrainLairChance(R_GROTTO, 25);
    ModifyTerrainLair(R_GROTTO, 0, O_LAIR);
    ModifyTerrainLair(R_GROTTO, 1, O_IFRITLAIR);
    ModifyTerrainLair(R_GROTTO, 2, O_GIANTCASTLE);
    ModifyTerrainLair(R_GROTTO, 3, O_ILAIR);
    ModifyTerrainLair(R_GROTTO, 4, O_CAVE);
    ModifyTerrainLair(R_GROTTO, 5, O_DARKTOWER);

    ModifyTerrainLairChance(R_DFOREST, 25);
    ModifyTerrainLair(R_DFOREST, 0, O_RUIN);
    ModifyTerrainLair(R_DFOREST, 1, O_CAVE);
    ModifyTerrainLair(R_DFOREST, 2, O_DEMONPIT);
    ModifyTerrainLair(R_DFOREST, 3, O_MAGETOWER);
    ModifyTerrainLair(R_DFOREST, 4, O_ILAIR);
    ModifyTerrainLair(R_DFOREST, 5, O_DARKTOWER);

    ModifyTerrainLairChance(R_CHASM, 25);
    ModifyTerrainLair(R_CHASM, 0, O_LAIR);
    ModifyTerrainLair(R_CHASM, 1, O_RUIN);
    ModifyTerrainLair(R_CHASM, 2, O_CAVE);
    ModifyTerrainLair(R_CHASM, 3, O_DEMONPIT);
    ModifyTerrainLair(R_CHASM, 4, O_MAGETOWER);
    ModifyTerrainLair(R_CHASM, 5, O_GIANTCASTLE);

    // --- Monster aggression and group size (modify_monster_threat) ---
    // Natural wandering monsters
    modify_monster_threat("LION",  4,   20);  // Pride of Lions
    modify_monster_threat("WOLF",  10,  20);  // Wolf Pack
    modify_monster_threat("GRIZ",  3,   20);  // Grizzly Bears
    modify_monster_threat("CROC",  6,   20);  // Crocodiles
    modify_monster_threat("ANAC",  6,   20);  // Anacondas
    modify_monster_threat("SCOR",  8,   20);  // Giant Scorpions
    modify_monster_threat("POLA",  3,   20);  // Polar Bears
    modify_monster_threat("GRAT",  30,  20);  // Pack of Rats
    modify_monster_threat("GSPI",  4,   20);  // Giant Spiders
    modify_monster_threat("GLIZ",  3,   25);  // Giant Lizards
    modify_monster_threat("TREN",  7,   30);  // Living Trees
    modify_monster_threat("ROC",   2,   50);  // Giant Birds
    modify_monster_threat("BOGT",  2,   30);  // Swamp Creatures
    modify_monster_threat("KONG",  2,   80);  // Great Apes
    modify_monster_threat("SPHI",  1,   50);  // Sphinx
    modify_monster_threat("ICEW",  8,   30);  // Ice Wurms
    modify_monster_threat("DRAG",  1,   80);  // Dragon
    modify_monster_threat("WYVR",  1,   50);  // Wyvern
    modify_monster_threat("CENT",  8,   20);  // Tribe of Centaurs
    modify_monster_threat("KOBO",  20,  20);  // Kobold Pack
    modify_monster_threat("OGRE",  2,   25);  // Family of Ogres
    modify_monster_threat("IFRI",  2,   25);  // Fire Ifrits
    modify_monster_threat("LMAN",  10,  20);  // Lizard Men
    modify_monster_threat("WMAN",  10,  20);  // Clan of Wild Men
    modify_monster_threat("SAND",  10,  20);  // Sandlings
    modify_monster_threat("YETI",  5,   25);  // Yeti
    modify_monster_threat("GOBL",  40,  20);  // Goblin Horde
    modify_monster_threat("TROL",  8,   25);  // Troll Pack
    modify_monster_threat("ETTI",  2,   25);  // Ettins

    // Summoned / undead monsters
    modify_monster_threat("SKEL",  100, 20);  // Skeleton
    modify_monster_threat("UNDE",  10,  50); // Undead
    modify_monster_threat("LICH",  1,   50);  // Lich
    modify_monster_threat("IMP",   50,  20);  // Imp
    modify_monster_threat("DEMO",  10,  50);  // Demon
    modify_monster_threat("BALR",  1,   80); // Balrog
    modify_monster_threat("EAGL",  1,   20);  // Eagle

    // Sea creatures
    modify_monster_threat("PIRA",  25,  30);  // Pirates        (default: num=20, hostile=50%)
    modify_monster_threat("PCAP",  1,   30);  // Pirate Captain (default: num=1,  hostile=50%)
    modify_monster_threat("PBOS",  1,   30);  // Pirate Bosun   (default: num=1,  hostile=50%)
    modify_monster_threat("PKIN",  1,  100);  // Pirate King    — always attacks
    modify_monster_threat("KRAK",  1,   50);  // Kraken
    modify_monster_threat("MERF",  100, 30);  // Merfolk
    modify_monster_threat("ELEM",  7,   35);  // Living Water

    // Special monsters (enabled via EnableItem)
    modify_monster_threat("HYDR",  1,   50);  // Hydra
    modify_monster_threat("IDRA",  1,   50);  // Ice Dragon
    modify_monster_threat("ILLY",  1,   50);  // Illyrthid
    modify_monster_threat("STGI",  1,   50);  // Storm Giant
    modify_monster_threat("CLGI",  1,   50);  // Cloud Giant
    modify_monster_threat("DEVL",  1,   80); // Devil
    modify_monster_threat("WARR",  30,  100); // Evil Warriors
    modify_monster_threat("DMAG",  1,   100); // Dark Mage
    modify_monster_threat("MAGI",  2,   100); // Evil Magicians

    // --- Monster loot drops (modify_monster_spoils) ---
    // Format: modify_monster_spoils(abbr, silver, spoiltype)
    // spoiltype: -1=no items, IT_NORMAL=basic goods, IT_ADVANCED=advanced items, IT_MAGIC=magic items
    // Comment shows default from gamedata.cpp for easy comparison when tuning.
    // Silver is max pool: actual drop = random(0, silver-1) per dead monster.
    // Pool system: max 4 distinct item types per unit; 5th+ monsters add qty to existing types.

    // Natural wandering monsters — mostly animal, no item drops
    modify_monster_spoils("LION",  200,  IT_NORMAL);           // Pride of Lions      (default: 200, -1)
    modify_monster_spoils("WOLF",  120,  IT_NORMAL);           // Wolf Pack           (default: 120, -1)
    modify_monster_spoils("GRIZ",  450,  IT_NORMAL);           // Grizzly Bears       (default: 450, -1)
    modify_monster_spoils("CROC",  120,  IT_NORMAL);           // Crocodiles          (default: 120, -1)
    modify_monster_spoils("ANAC",  120,  IT_NORMAL);           // Anacondas           (default: 120, -1)
    modify_monster_spoils("SCOR",  160,  IT_NORMAL);           // Giant Scorpions     (default: 160, -1)
    modify_monster_spoils("POLA",  450,  IT_NORMAL);           // Polar Bears         (default: 450, -1)
    modify_monster_spoils("GRAT",  70,   IT_NORMAL);           // Pack of Rats        (default: 30,  -1)
    modify_monster_spoils("GSPI",  300,  IT_NORMAL);           // Giant Spiders       (default: 300, -1)
    modify_monster_spoils("GLIZ",  400,  IT_NORMAL);           // Giant Lizards       (default: 400, -1)
    modify_monster_spoils("TREN",  800,  IT_ADVANCED);  // Living Trees        (default: 600, IT_ADVANCED)
    modify_monster_spoils("ROC",   1200, IT_ADVANCED);  // Giant Birds         (default: 1500, IT_ADVANCED)
    modify_monster_spoils("BOGT",  1200, IT_ADVANCED);  // Swamp Creatures     (default: 2000, IT_ADVANCED)
    modify_monster_spoils("KONG",  2000, IT_ADVANCED);  // Great Apes          (default: 2500, IT_ADVANCED)
    modify_monster_spoils("SPHI",  4000, IT_ADVANCED);  // Sphinx              (default: 5000, IT_ADVANCED)
    modify_monster_spoils("ICEW",  800,  IT_ADVANCED);  // Ice Wurms           (default: 500,  IT_ADVANCED)
    modify_monster_spoils("DRAG",  8000, IT_MAGIC);     // Dragon              (default: 8000, IT_MAGIC)
    modify_monster_spoils("WYVR",  4000, IT_ADVANCED);  // Wyvern              (default: 3000, IT_ADVANCED)

    // Humanoid wandering monsters — drop basic goods/items
    modify_monster_spoils("CENT",  250,  IT_NORMAL);    // Tribe of Centaurs   (default: 250,  IT_NORMAL)
    modify_monster_spoils("KOBO",  70,   IT_NORMAL);    // Kobold Pack         (default: 60,   IT_NORMAL)
    modify_monster_spoils("OGRE",  800,  IT_NORMAL);    // Family of Ogres     (default: 800,  IT_NORMAL)
    modify_monster_spoils("IFRI",  1500, IT_ADVANCED);  // Fire Ifrits         (default: 1500, IT_ADVANCED)
    modify_monster_spoils("LMAN",  120,  IT_NORMAL);    // Lizard Men          (default: 120,  IT_NORMAL)
    modify_monster_spoils("WMAN",  120,  IT_NORMAL);    // Clan of Wild Men    (default: 120,  IT_NORMAL)
    modify_monster_spoils("SAND",  70,   IT_NORMAL);    // Sandlings           (default: 60,   IT_NORMAL)
    modify_monster_spoils("YETI",  250,  IT_NORMAL);    // Yeti                (default: 250,  IT_NORMAL)
    modify_monster_spoils("GOBL",  70,   IT_NORMAL);    // Goblin Horde        (default: 50,   IT_NORMAL)
    modify_monster_spoils("TROL",  400,  IT_ADVANCED);  // Troll Pack          (default: 500,  IT_ADVANCED)
    modify_monster_spoils("ETTI",  1000, IT_ADVANCED);  // Ettins              (default: 1200, IT_ADVANCED)

    // Summoned / undead monsters
    modify_monster_spoils("SKEL",  70,   IT_NORMAL);    // Skeleton            (default: 60,   IT_NORMAL)
    modify_monster_spoils("UNDE",  300,  IT_ADVANCED);  // Undead              (default: 400,  IT_ADVANCED)
    modify_monster_spoils("LICH",  4000, IT_MAGIC);     // Lich                (default: 5000, IT_MAGIC)
    modify_monster_spoils("IMP",   70,   IT_NORMAL);    // Imp                 (default: 60,   IT_NORMAL)
    modify_monster_spoils("DEMO",  800,  IT_ADVANCED);  // Demon               (default: 800,  IT_ADVANCED)
    modify_monster_spoils("BALR",  20000,IT_MAGIC);     // Balrog              (default: 25000, IT_MAGIC)
    modify_monster_spoils("EAGL",  20,   -1);           // Eagle               (default: 20,  -1)

    // Sea creatures
    modify_monster_spoils("PIRA",  250,  IT_ADVANCED);  // Pirates             (default: 300,  IT_ADVANCED)
    modify_monster_spoils("KRAK",  20000,IT_MAGIC);     // Kraken              (default: 20000, IT_MAGIC)
    modify_monster_spoils("MERF",  70,   IT_NORMAL);    // Merfolk             (default: 70,   IT_NORMAL)
    modify_monster_spoils("ELEM",  1200, IT_ADVANCED);  // Living Water        (default: 1300, IT_ADVANCED)

    // Special monsters (enabled via EnableItem)
    modify_monster_spoils("HYDR",  8000,IT_MAGIC);     // Hydra               (default: 10000, IT_MAGIC)
    modify_monster_spoils("IDRA",  8000,IT_MAGIC);     // Ice Dragon          (default: 15000, IT_MAGIC)
    modify_monster_spoils("ILLY",  4000, IT_MAGIC);     // Illyrthid           (default: 4000, IT_MAGIC)
    modify_monster_spoils("STGI",  12000,IT_MAGIC);     // Storm Giant         (default: 13000, IT_MAGIC)
    modify_monster_spoils("CLGI",  16000,IT_MAGIC);     // Cloud Giant         (default: 20000, IT_MAGIC)
    modify_monster_spoils("DEVL",  30000,IT_MAGIC);     // Devil               (default: 40000, IT_MAGIC)
    modify_monster_spoils("WARR",  120,  IT_NORMAL);    // Evil Warriors       (default: 120,  IT_NORMAL)
    modify_monster_spoils("DMAG",  5000, IT_MAGIC);     // Dark Mage           (default: 5000, IT_MAGIC)
    modify_monster_spoils("MAGI",  4000, IT_MAGIC);     // Evil Magicians      (default: 4000, IT_MAGIC)
    modify_monster_spoils("SORC",  2000, IT_ADVANCED);  // Evil Sorcerers      (default: 1000, IT_ADVANCED)

        // --- Base prices: weapons, armor, tools ---
    // Resources: IRON=30  WOOD=30  FUR=30  HERBS=30  MITH=100  IRWD=100  ADMT=300
    // Training:  L1=10s(1mo)  L2=30s(3mo)  L3=60s(6mo)  L4=100s(10mo)  L5=150s(15mo)
    // Upkeep: 30 silver/production month
    // Cost = materials + training(level) + 30*production_months

    // --- Weapons IT_NORMAL L1 ---
    ModifyItemBasePrice(I_SPEAR,        70); // was:60  | WEAP1 1m 1xWOOD(30)
    ModifyItemBasePrice(I_JAVELIN,      70); // was:60  | WEAP1 1m 1xWOOD(30)
    ModifyItemBasePrice(I_PICK,         70); // was:60  | WEAP1 1m 1xIRON(30)
    ModifyItemBasePrice(I_AXE,          70); // was:60  | WEAP1 1m 1xWOOD(30)
    ModifyItemBasePrice(I_HAMMER,       70); // was:60  | WEAP1 1m 1xIRON(30)
    ModifyItemBasePrice(I_NET,          70); // was:80  | FISH1 1m 1xHERBS(30)
    ModifyItemBasePrice(I_LASSO,        70); // was:60  | HERB1 1m 1xHERBS(30)
    ModifyItemBasePrice(I_BAG,          70); // was:60  | HERB1 1m 1xHERBS(30)
    ModifyItemBasePrice(I_SPINNING,     70); // was:60  | CARP1 1m 1xWOOD(30)

    // --- Weapons IT_NORMAL L2 ---
    ModifyItemBasePrice(I_SWORD,       120); // was:80  | WEAP2 2m 2xIRON(60)
    ModifyItemBasePrice(I_CROSSBOW,    120); // was:80  | WEAP2 2m 1xWOOD+1xIRON(60)
    ModifyItemBasePrice(I_LONGBOW,     120); // was:80  | WEAP2 2m 1xWOOD+1xHERBS(60)
    ModifyItemBasePrice(I_PIKE,        120); // was:100 | WEAP2 2m 2xWOOD(60)
    ModifyItemBasePrice(I_BHAMMER,     120); // was:100 | WEAP2 2m 1xIRON+1xWOOD(60)
    ModifyItemBasePrice(I_BAXE,        120); // was:200 | WEAP2 2m 1xWOOD+1xIRON(60)

    // --- Weapons IT_ADVANCED ---
    ModifyItemBasePrice(I_MCROSSBOW,   350); // was:200 | WEAP4 2m 1xIRWD+1xXBOW(220)
    ModifyItemBasePrice(I_LANCE,       350); // was:300 | WEAP4 2m 1xPIKE(100)+1xIRWD(220)
    ModifyItemBasePrice(I_MBAXE,       300); // was:300 | WEAP3 2m 1xMITH+1xBAXE(220)
    ModifyItemBasePrice(I_MBHAM,       300); // was:300 | WEAP3 2m 1xMITH+1xBHAM(220)
    ModifyItemBasePrice(I_MSWORD,      300); // was:300 | WEAP3 2m 1xMITH+1xSWOR(220)

    // --- Weapons IT_ADVANCED + NOMARKET ---
    ModifyItemBasePrice(I_ADBAXE,      800); // was:1200 | WEAP5 3m 1xADMT+1xMBAX(600)
    ModifyItemBasePrice(I_ABHAM,       800); // was:1000 | WEAP5 3m 1xADMT+1xMBAH(600)
    ModifyItemBasePrice(I_ADSWORD,     800); // was:800  | WEAP5 3m 1xADMT+1xMSWO(600)
    ModifyItemBasePrice(I_DOUBLEBOW,   800); // was:400  | WEAP5 3m 1xYEW+1xLBOW+1xHERBS(450)

    // --- Armor IT_NORMAL L1 ---
    ModifyItemBasePrice(I_LEATHERARMOR, 70); // was:45  | ARMO1 1m 1xFUR(30)
    ModifyItemBasePrice(I_CHAINARMOR,   70); // was:60  | ARMO1 1m 1xIRON(30)

    // --- Armor IT_NORMAL L2 ---
    ModifyItemBasePrice(I_PLATEARMOR,  150); // was:250 | ARMO2 2m 2xIRON(60)

    // --- Armor IT_ADVANCED ---
    ModifyItemBasePrice(I_MCHAIN,      450); // was:400 | ARMO3 2m 1xCARM(70)+1xMITH(100)
    ModifyItemBasePrice(I_MPLATE,      900); // was:500 | ARMO4 2m 1xPARM(120)+2xMITH(200)

    // --- Armor IT_ADVANCED + NOMARKET ---
    ModifyItemBasePrice(I_ADRING,     1200); // was:1500 | ARMO5 4m 1xMCAR(400)+1xADMT(300)
    ModifyItemBasePrice(I_ADPLATE,    1800); // was:1800 | ARMO5 4m 1xMARM(900)+2xADMT(600)

    // --- Shields IT_NORMAL ---
    ModifyItemBasePrice(I_WSHIELD,     120); // was:40  | ARMO1 1m 1xWOOD(30)
    ModifyItemBasePrice(I_ISHIELD,     220); // was:80  | ARMO3 1m 1xWSHD(120)+1xIRON(30)

    // --- Shields IT_ADVANCED ---
    ModifyItemBasePrice(I_MSHIELD,     450); // was:300 | ARMO4 1m 1xISHD(220)+1xMITH(100)

    // --- Shields IT_ADVANCED + NOMARKET ---
    ModifyItemBasePrice(I_ASHIELD,    1200); // was:600 | ARMO5 1m 1xMSHD(450)+1xADMT(300)

    // --- Raw materials IT_ADVANCED ---
    ModifyItemBasePrice(I_MITHRIL,     100); // was:100 | MINI3 1m (resource)
    ModifyItemBasePrice(I_IRONWOOD,    100); // was:100 | LUMB3 1m (resource)
    ModifyItemBasePrice(I_ROOTSTONE,   100); // was:100 | QUAR3 1m (resource)
    ModifyItemBasePrice(I_YEW,         200); // was:200 | LUMB5 1m (resource)
    ModifyItemBasePrice(I_FLOATER,     100); // was:100 | HUNT3 1m (resource)
    ModifyItemBasePrice(I_ADMANTIUM,   300); // was:300 | MINI5 1m (resource, NOMARKET)
    ModifyItemBasePrice(I_MUSHROOM,    100); // was:100 | HERB3 1m (resource)

    // --- Mounts IT_NORMAL ---
    ModifyItemBasePrice(I_CAMEL,        30); // was:30  | HORS1 1m (capture)

    // --- Mounts IT_ADVANCED ---
    ModifyItemBasePrice(I_WHORSE,      300); // was:300 | HORS5 1m (capture)
    ModifyItemBasePrice(I_TURT,        100); // was:100 | FISH3 1m (capture)

    // --- Transport IT_ADVANCED ---
    ModifyItemBasePrice(I_MWAGON,      200); // was:200 | CARP3 1m 1xIRWD(100)
    ModifyItemBasePrice(I_GLIDER,      250); // was:250 | CARP5 1m 2xFLOA(100)

    // --- Misc IT_ADVANCED ---
    ModifyItemBasePrice(I_HEALPOTION,  200); // was:200 | HEAL3 1m 1xHERBS+1xMUSH(100)

    // Modify the various spells which are allowed to cross levels
    if (Globals->EASIER_UNDERWORLD) {
        modify_range_flags("rng_teleport", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_portal", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_farsight", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_clearsky", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_weather", RangeType::RNG_CROSS_LEVELS);
    }

    if (Globals->TRANSPORT & GameDefs::ALLOW_TRANSPORT) {
        EnableSkill(S_QUARTERMASTER);
        EnableObject(O_CARAVANSERAI);
        if (Globals->EASIER_UNDERWORLD) modify_range_level_penalty("rng_transport", 4);
    }

    // NO7 - Enable the various parts of the victory conditions
    if (rulesetSpecificData.value("victory_type", "") == "annihilation") {
        EnableObject(O_RITUAL_ALTAR);
        EnableObject(O_EMPOWERED_ALTAR);
        EnableObject(O_ENTITY_CAGE);
        EnableObject(O_DORMANT_MONOLITH);
        EnableObject(O_ACTIVE_MONOLITH);
        EnableItem(I_IMPRISONED_ENTITY);
        EnableSkill(S_ANNIHILATION);
        modify_range_flags("rng_annihilate", RangeType::RNG_SURFACE_ONLY | RangeType::RNG_CROSS_LEVELS);
    }

    // Weapon BM example

    // Make SWOR to have malus of -1 on attack and -2 on defense vs. SPEA
    // modify_weapon_bonus_malus("SWOR", 0, "SPEA", -1, -2);

    // At the same time give SPEA bonus of 2 on attacka and 2 on defense vs. SWOR
    // modify_weapon_bonus_malus("SPEA", 0, "SWOR", 2, 2);

    // Dungeon entrance/exit portal (same object type for both directions — see dungeon.h)
    EnableObject(O_DUNGEON_ENTRANCE);

    NewOriginsSetupQuests();
}

const std::optional<std::string> ARegion::movement_forbidden_by_ruleset(Unit *u, ARegion *origin, ARegionList& regions) {

    // If Empowered Altars are active, we should check for them.  This is only used for the NO7 victory condition.
    if (!(ObjectDefs[O_EMPOWERED_ALTAR].flags & ObjectType::DISABLED)) {
        ARegionArray *surface = regions.get_first_region_array_of_type(ARegionArray::LEVEL_SURFACE);
        ARegion *surface_center = surface->GetRegion(surface->x / 2, surface->y / 2);

        // If we don't have a barrens in the center, then this is not an N07 ring map and thus we won't have altars.
        if (surface_center->type != R_BARREN) {
            return std::nullopt;
        }

        ARegionArray *this_level = this->level;
        ARegion *this_center = this_level->GetRegion(this_level->x / 2, this_level->y / 2);

        if (this == this_center || this == surface_center) {
            // This is a center region.  You can only enter here if all the altars have been empowered.

            int count = 0;
            for (int i = 0; i < 6; i++) {
                ARegion *r = surface_center->neighbors[i];
                // search that region for an altar
                for(const auto o : r->objects) {
                    if (o->type == O_EMPOWERED_ALTAR) {
                        count++;
                    }
                }
            }
            if (count < 6) {
                return "A mystical barrier";
            }
        }
    }
    return std::nullopt;
}

// --- Global Boss Hunt helpers ---

/**
 * @brief Creates a GLOBAL_BOSS_HUNT quest for the given pirate captain unit.
 *
 * Called at spawn time from MakePirateFleet() and each turn from
 * EnsureElitePirateQuests(). Creates at most one quest per captain.
 * Token payout from boss_targets[I_PIRATE_CAPTAIN]; falls back to 4 if
 * quest_setup has not yet populated boss_targets (e.g. in unit tests).
 */
void Game::TryCreatePirateHuntQuest(Unit *cap) {
    for (const auto& q : quests) {
        if (q->subtype == Quest::GLOBAL_BOSS_HUNT && q->target == cap->num) return;
    }

    auto q         = std::make_shared<Quest>();
    q->num         = questseq++;
    q->scope       = Quest::SCOPE_GLOBAL;
    q->subtype     = Quest::GLOBAL_BOSS_HUNT;
    q->target      = cap->num;
    q->target_monster = I_PIRATE_CAPTAIN;
    q->tokens      = LookupBossTokens(I_PIRATE_CAPTAIN, 4);
    q->created_turn = TurnNumber();
    quests.push_back(q);

    printf("Pirate hunt quest %d created for captain '%s' (unit %d), %d tokens.\n",
           q->num, cap->name.c_str(), cap->num, q->tokens);
}

/**
 * @brief Each turn: ensures every elite pirate captain has a GLOBAL_BOSS_HUNT quest.
 *
 * Runs after GrowWMons() in PostProcessTurn(). Walks all I_PIRATE_CAPTAIN units
 * in the monster faction and creates a quest for any uncovered captain (1:1 mapping,
 * no cap, no probabilistic gating). Mirrors pirate_sighting at game.cpp:1197.
 */
void Game::EnsureElitePirateQuests() {
    for (const auto r : regions) {
        for (const auto o : r->objects) {
            for (const auto u : o->units) {
                if (u->faction->num != monfaction) continue;
                if (u->items.GetNum(I_PIRATE_CAPTAIN) == 0) continue;
                TryCreatePirateHuntQuest(u);
            }
        }
    }
}
