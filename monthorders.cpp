#include <random>
#include <stdlib.h>

#include "events.h"
#include "game.h"
#include "gamedata.h"
#include "logger.hpp"
#include "namegen.h"
#include "quests.h"
#include "rng.hpp"

using namespace std;

void Game::RunMovementOrders()
{
    int phase, error;
    Unit *u;
    std::list<Location *> locs;
    Location *l;
    AString order;

    for (phase = 0; phase < Globals->MAX_SPEED; phase++) {
        for (const auto r : regions) {
            for (const auto o : r->objects) {
                for (const auto u : o->units) { DoMoveEnter(u, r); }
            }
        }
        for (const auto r : regions) {
            for (const auto o : r->objects) {
                error = 1;
                if (o->IsFleet()) {
                    u = o->GetOwner();
                    if (!u) continue;
                    if (u->phase >= phase) continue;
                    if (u->nomove) {
                        error = 4;
                    } else if (u->monthorders && u->monthorders->type == O_SAIL) {
                        u->phase = phase;
                        if (o->incomplete < 50) {
                            l = Do1SailOrder(r, o, u);
                            if (l) locs.push_back(l);
                            error = 0;
                        } else {
                            error = 3;
                        }
                    } else {
                        error = 2;
                    }
                }
                if (error > 0) {
                    for (const auto u : o->units) {
                        if (u && u->monthorders && u->monthorders->type == O_SAIL) {
                            switch (error) {
                            case 1: u->error("SAIL: Must be on a ship."); break;
                            case 2: u->error("SAIL: Owner must issue fleet directions."); break;
                            case 3: u->error("SAIL: Fleet is too damaged to sail."); break;
                            case 4: u->error("SAIL: Unable to sail due to combat losses."); break;
                            }
                            delete u->monthorders;
                            u->monthorders = nullptr;
                        }
                    }
                }
            }
        }
        for (const auto r : regions) {
            for (auto o : r->objects) {
                for (const auto u : o->units) {
                    if (u->phase >= phase) continue;
                    u->phase = phase;
                    if (u && !u->nomove && u->monthorders &&
                        (u->monthorders->type == O_MOVE || u->monthorders->type == O_ADVANCE)) {
                        l = DoAMoveOrder(u, r, o);
                        if (l) locs.push_back(l);
                    }
                }
            }
        }
        DoMovementAttacks(locs);
        std::for_each(locs.begin(), locs.end(), [](Location *loc) { delete loc; });
        locs.clear();
    }

    // Do a final round of Enters after the phased movement is done,
    // in case such a thing is at the end of a move chain
    for (const auto r : regions) {
        for (const auto o : r->objects) {
            for (const auto u : o->units) { DoMoveEnter(u, r); }
        }
    }

    // Queue remaining moves
    for (const auto r : regions) {
        for (const auto o : r->objects) {
            for (const auto u : o->units) {
                MoveOrder *mo = dynamic_cast<MoveOrder *>(u->monthorders);
                if (!mo || (mo->type != O_MOVE && mo->type != O_ADVANCE)) {
                    u->savedmovement = 0;
                    u->savedmovedir = -1;
                    continue;
                }

                if (mo->dirs.size() > 0) {
                    if (u->nomove) {
                        u->savedmovedir = -1;
                        u->savedmovement = 0;
                    } else {
                        auto d = mo->dirs.front();
                        if (u->savedmovedir != d->dir) { u->savedmovement = 0; }
                        u->savedmovement += u->movepoints / Globals->MAX_SPEED;
                        u->savedmovedir = d->dir;
                    }

                    string tOrder = (mo->advancing ? "ADVANCE" : "MOVE");
                    string temp = tOrder + ": Unit has insufficient movement points; remaining moves queued.";
                    u->event(temp, "movement");

                    for (const auto d : mo->dirs) {
                        tOrder += " ";

                        // Add the appropriate direction token
                        if (d->dir < NDIRS)
                            tOrder += DirectionAbrs[d->dir];
                        else if (d->dir == MOVE_IN)
                            tOrder += "IN";
                        else if (d->dir == MOVE_OUT)
                            tOrder += "OUT";
                        else if (d->dir == MOVE_PAUSE)
                            tOrder += "P";
                        else
                            tOrder += to_string(d->dir - MOVE_ENTER);
                    }
                    u->oldorders.push_front(tOrder);
                }
            }
            u = o->GetOwner();
            if (o->IsFleet() && u && !u->nomove && u->monthorders && u->monthorders->type == O_SAIL) {
                SailOrder *so = dynamic_cast<SailOrder *>(u->monthorders);
                if (so->dirs.size() > 0) {
                    u->event("SAIL: Can't sail that far; remaining moves queued.", "movement");
                    string tOrder = "SAIL";
                    for (const auto d : so->dirs) {
                        tOrder += " ";
                        if (d->dir == MOVE_PAUSE)
                            tOrder += "P";
                        else
                            tOrder += DirectionAbrs[d->dir];
                    }
                    u->oldorders.push_front(tOrder);
                }
            }
        }
    }
}

Location *Game::Do1SailOrder(ARegion *reg, Object *fleet, Unit *cap)
{
    SailOrder *o = dynamic_cast<SailOrder *>(cap->monthorders);
    int stop, wgt, slr, nomove, cost;
    std::set<Faction *> facs;
    ARegion *newreg;
    Location *loc;

    // NPC fleets bypass sailor skill check and use raw ship speed
    bool is_npc = cap->faction->is_npc;
    if (is_npc) {
        // Use the slowest ship's raw speed, ignoring crew skill
        int npc_speed = Globals->MAX_SPEED;
        for (int item = 0; item < NITEMS; item++) {
            if (fleet->GetNumShips(item) > 0 && ItemDefs[item].speed < npc_speed)
                npc_speed = ItemDefs[item].speed;
        }
        fleet->movepoints += npc_speed;
    } else {
        fleet->movepoints += fleet->GetFleetSpeed(0);
    }
    stop = 0;
    wgt = 0;
    slr = 0;
    nomove = 0;
    for (const auto unit : fleet->units) {
        facs.insert(unit->faction);
        wgt += unit->Weight();
        if (unit->nomove) {
            // If any unit on-board was in a fight (and
            // suffered > 5% casualties), then halt movement
            nomove = 1;
        }
        if (unit->monthorders && unit->monthorders->type == O_SAIL) {
            slr += unit->GetSkill(S_SAILING) * unit->GetMen();
        }
    }

    if (nomove) {
        stop = 1;
    } else if (!o->dirs.size()) {
        stop = 1;
    } else if (!is_npc && wgt > fleet->FleetCapacity()) {
        // NPC fleets skip overload check
        cap->error("SAIL: Fleet is overloaded.");
        stop = 1;
    } else if (!is_npc && slr < fleet->GetFleetSize()) {
        // NPC fleets skip sailor count check
        cap->error("SAIL: Not enough sailors.");
        stop = 1;
    } else {
        auto x = o->dirs.front();
        if (x->dir == MOVE_PAUSE) {
            newreg = reg;
        } else {
            newreg = reg->neighbors[x->dir];
        }
        cost = 1;

        // Blizzard effect
        if (newreg && newreg->weather == W_BLIZZARD && !newreg->clearskies) { cost = 4; }

        if (Globals->WEATHER_EXISTS) {
            if (newreg && newreg->weather != W_NORMAL && !newreg->clearskies) cost = 2;
        }
        if (x->dir == MOVE_PAUSE) { cost = 1; }

        // Canal through-pass cost: charged ONLY when the canal is what enables the
        // move (i.e. the exit would be blocked without it). An ordinary allowed exit
        // from a canal region — sailing back the way it came, or turning to an
        // adjacent ocean hex — keeps its normal cost; the canal must not slow down a
        // move that was already legal. The extra cost applies only to the straight
        // through-pass the canal makes possible. Stone canal = 2 (half speed),
        // Mystic (rootstone, IT_ADVANCED) canal = 1. cost = max(weather, canal).
        if (x->dir != MOVE_PAUSE && newreg &&
            TerrainDefs[reg->type].similar_type != R_OCEAN &&
            TerrainDefs[newreg->type].similar_type == R_OCEAN) {

            // Would this exit be allowed WITHOUT a canal? (mirrors SailThroughCheck)
            int d1 = (fleet->prevdir + 1) % NDIRS;
            int d2 = (fleet->prevdir - 1 + NDIRS) % NDIRS;
            bool allowed_without_canal =
                (fleet->prevdir == -1) ||
                (fleet->prevdir == x->dir) ||
                (x->dir == d1 && reg->neighbors[d1] &&
                    TerrainDefs[reg->neighbors[d1]->type].similar_type == R_OCEAN) ||
                (x->dir == d2 && reg->neighbors[d2] &&
                    TerrainDefs[reg->neighbors[d2]->type].similar_type == R_OCEAN);

            if (!allowed_without_canal) {
                int canal_cost = 0;
                for (const auto o : reg->objects) {
                    if (!(ObjectDefs[o->type].flags & ObjectType::CANAL)) continue;
                    if (o->incomplete > 0) continue;
                    int mat = ObjectDefs[o->type].item;
                    int c = (mat >= 0 && (ItemDefs[mat].type & IT_ADVANCED)) ? 1 : 2;
                    if (canal_cost == 0 || c < canal_cost) canal_cost = c;
                }
                if (canal_cost > cost) cost = canal_cost;
            }
        }
        // We probably shouldn't see terrain-based errors until
        // we accumulate enough movement points to get there
        if (fleet->movepoints < cost * Globals->MAX_SPEED) return 0;
        if (!newreg) {
            cap->error("SAIL: Can't sail that way.");
            stop = 1;
        } else if (x->dir == MOVE_PAUSE) {
            // Can always do maneuvers
        } else if (fleet->flying < 1 && !newreg->IsCoastalOrLakeside()) {
            cap->error("SAIL: Can't sail inland.");
            stop = 1;
        } else if ((fleet->flying < 1) && (TerrainDefs[reg->type].similar_type != R_OCEAN) &&
                   (TerrainDefs[newreg->type].similar_type != R_OCEAN)) {
            cap->error("SAIL: Can't sail inland.");
            stop = 1;
        } else if (fleet->SailThroughCheck(x->dir) < 1) {
            cap->error(
                "SAIL: Could not sail " + DirectionStrs[x->dir] + " from " + reg->short_print() +
                ". Cannot sail through land."
            );
            stop = 1;
        }

        if (!stop) {
            // Check the new region for barriers and the fleet units for keys to the barriers
            int needed_key = -1;
            for (auto o : newreg->objects) {
                if (ObjectDefs[o->type].flags & ObjectType::KEYBARRIER) { needed_key = ObjectDefs[o->type].key_item; }
            }
            if (needed_key != -1) { // we found a barrier
                bool has_key = false;
                for (const auto u : fleet->units) {
                    if (u->items.GetNum(needed_key) > 0) {
                        has_key = true;
                        break;
                    }
                }
                if (!has_key) {
                    cap->error(
                        "SAIL: Can't sail " + DirectionStrs[x->dir] + " from " + reg->short_print() +
                        " due to mystical barrier."
                    );
                    stop = 1;
                }
            }
        }

        // We could have been stopped by not having the key above.
        if (!stop) {
            fleet->movepoints -= cost * Globals->MAX_SPEED;
            if (x->dir != MOVE_PAUSE) {
                // this can invalidate the object iterator if we are in an iteration
                fleet->MoveObject(newreg);
                fleet->SetPrevDir(reg->GetRealDirComp(x->dir));
            }
            for (const auto unit : fleet->units) {
                unit->moved += cost;
                if (unit->guard == GUARD_GUARD) unit->guard = GUARD_NONE;
                unit->alias = 0;
                unit->PracticeAttribute("wind");
                if (unit->monthorders) {
                    if (unit->monthorders->type == O_SAIL) unit->Practice(S_SAILING);
                    if (unit->monthorders->type == O_MOVE) {
                        delete unit->monthorders;
                        unit->monthorders = nullptr;
                    }
                }
                unit->DiscardUnfinishedShips();
                facs.insert(unit->faction);
            }

            for (const auto f : facs) {
                string temp = fleet->name;
                temp += (x->dir == MOVE_PAUSE ? " performs maneuvers in " : " sails from ") + reg->short_print();
                if (x->dir != MOVE_PAUSE) temp += " to " + newreg->short_print();
                f->event(temp, "sail");
            }
            if (Globals->TRANSIT_REPORT != GameDefs::REPORT_NOTHING && x->dir != MOVE_PAUSE) {
                if (!(cap->faction->is_npc)) newreg->visited = 1;
                for (const auto unit : fleet->units) {
                    // Everyone onboard gets to see the sights
                    // Note the hex being left
                    for (const auto f : reg->passers) {
                        if (f->unit == unit) {
                            // We moved into here this turn
                            f->exits_used[x->dir] = 1;
                        }
                    }
                    // And mark the hex being entered
                    Farsight *f = new Farsight;
                    f->faction = unit->faction;
                    f->level = 0;
                    f->unit = unit;
                    f->exits_used[reg->GetRealDirComp(x->dir)] = 1;
                    newreg->passers.push_back(f);
                }
            }
            reg = newreg;
            if (newreg->ForbiddenShip(fleet)) {
                string temp = fleet->name + " is stopped by guards in " + newreg->short_print() + ".";
                cap->faction->event(temp, "sail");
                stop = 1;
            }
            std::erase(o->dirs, x);
            delete x;
        }
    }

    if (stop) {
        // Clear out everyone's orders
        for (const auto unit : fleet->units) {
            if (unit->monthorders && unit->monthorders->type == O_SAIL) {
                delete unit->monthorders;
                unit->monthorders = nullptr;
            }
        }
    }

    loc = new Location;
    loc->unit = cap;
    loc->region = reg;
    loc->obj = fleet;
    return loc;
}

void Game::RunTeachOrders()
{
    for (const auto r : regions) {
        for (const auto obj : r->objects) {
            for (const auto u : obj->units) {
                if (u->monthorders) {
                    if (u->monthorders->type == O_TEACH) {
                        Do1TeachOrder(r, u);
                        delete u->monthorders;
                        u->monthorders = nullptr;
                    }
                }
            }
        }
    }
}

void Game::Do1TeachOrder(ARegion *reg, Unit *unit)
{
    /* First pass, find how many to teach */
    if (Globals->LEADERS_EXIST && !unit->IsLeader()) {
        /* small change to handle Ceran's mercs */
        if (!unit->GetMen(I_MERC)) {
            // Mercs can teach even though they are not leaders.
            // They cannot however improve their own skills
            unit->error("TEACH: Only leaders can teach.");
            return;
        }
    }

    int students = 0;
    TeachOrder *order = dynamic_cast<TeachOrder *>(unit->monthorders);
    reg->deduplicate_unit_list(order->targets, unit->faction->num);
    for (auto it = order->targets.begin(); it != order->targets.end();) {
        UnitId *id = *it;
        Unit *target = reg->GetUnitId(id, unit->faction->num);
        if (!target) {
            unit->error("TEACH: No such unit.");
            it = order->targets.erase(it);
            delete id;
            continue;
        }
        if (target->faction->get_attitude(unit->faction->num) < AttitudeType::FRIENDLY) {
            unit->error("TEACH: " + target->name + " is not a member of a friendly faction.");
            it = order->targets.erase(it);
            delete id;
            continue;
        }
        if (!target->monthorders || target->monthorders->type != O_STUDY) {
            unit->error("TEACH: " + target->name + " is not studying.");
            it = order->targets.erase(it);
            delete id;
            continue;
        }

        StudyOrder *so = dynamic_cast<StudyOrder *>(target->monthorders);
        int sk = so->skill;
        if (unit->GetRealSkill(sk) <= target->GetRealSkill(sk)) {
            unit->error("TEACH: " + target->name + " is not studying a skill you can teach.");
            it = order->targets.erase(it);
            delete id;
            continue;
        }
        // Check whether it's a valid skill to teach
        if (SkillDefs[sk].flags & SkillType::NOTEACH) {
            unit->error("TEACH: " + SkillDefs[sk].name + " cannot be taught.");
            it = order->targets.erase(it);
            delete id;
            continue;
        }
        students += target->GetMen();
        ++it;
    }

    if (!students) return;

    int days = (30 * unit->GetMen() * Globals->STUDENTS_PER_TEACHER);

    /* We now have a list of valid targets */
    for (const auto id : order->targets) {
        Unit *u = reg->GetUnitId(id, unit->faction->num);

        int umen = u->GetMen();
        int tempdays = (umen * days) / students;
        if (tempdays > 30 * umen) tempdays = 30 * umen;
        days -= tempdays;
        students -= umen;

        StudyOrder *o = dynamic_cast<StudyOrder *>(u->monthorders);
        o->days += tempdays;
        if (o->days > 30 * umen) {
            days += o->days - 30 * umen;
            o->days = 30 * umen;
        }
        unit->event("Teaches " + SkillDefs[o->skill].name + " to " + u->name + ".", "teach");
        // The TEACHER may learn something in this process!
        unit->Practice(o->skill);
    }
    std::for_each(order->targets.begin(), order->targets.end(), [](UnitId *id) { delete id; });
    order->targets.clear();
}

void Game::Run1BuildOrder(ARegion *r, Object *obj, Unit *u)
{
    Object *buildobj;
    string quest_rewards;

    if (!Globals->BUILD_NO_TRADE && !ActivityCheck(r, u->faction, FactionActivity::TRADE)) {
        u->error("BUILD: Faction can't produce in that many regions.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    buildobj = r->GetObject(u->build);
    // plain "BUILD" order needs to check that the unit is in something
    // that can be built AFTER enter/leave orders have executed
    if (!buildobj || buildobj->type == O_DUMMY) buildobj = obj;

    if (!buildobj || buildobj->type == O_DUMMY) {
        u->error("BUILD: Nothing to build.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }
    int type = buildobj->type;
    int sk = lookup_skill(ObjectDefs[type].skill);
    if (sk == -1) {
        u->error("BUILD: Can't build " + ObjectDefs[type].name + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    int usk = u->GetSkill(sk);
    if (usk < ObjectDefs[type].level) {
        u->error("BUILD: Can't build " + ObjectDefs[type].name + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    int needed = buildobj->incomplete;
    // AS
    if (((ObjectDefs[type].flags & ObjectType::NEVERDECAY) || !Globals->DECAY) && needed < 1) {
        u->error("BUILD: " + ObjectDefs[type].name + " is finished.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    // AS
    if (needed <= -(ObjectDefs[type].maxMaintenance)) {
        u->error("BUILD: " + ObjectDefs[type].name + " does not yet require maintenance.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    int it = ObjectDefs[type].item;
    BuildOrder *border = dynamic_cast<BuildOrder *>(u->monthorders);
    int preferred = (border ? border->preferred_material : -1);

    // Resolve preferred material for I_WOOD_OR_STONE buildings
    if (it == I_WOOD_OR_STONE && preferred != -1) {
        if (preferred != I_WOOD && preferred != I_STONE) preferred = -1; // safety
    }

    int itn;
    if (it == I_WOOD_OR_STONE) {
        if (preferred != -1) {
            itn = u->GetSharedNum(preferred);
        } else {
            itn = u->GetSharedNum(I_WOOD) + u->GetSharedNum(I_STONE);
        }
    } else {
        itn = u->GetSharedNum(it);
    }

    if (itn == 0) {
        if (it == I_WOOD_OR_STONE && preferred != -1) {
            u->error("BUILD: Don't have " + item_string(preferred, 1) + " to build " + ObjectDefs[type].name + ".");
        } else {
            u->error("BUILD: Don't have the required materials to build " + ObjectDefs[type].name + ".");
        }
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    int num = u->GetMen() * usk;

    // AS
    string job;
    if (needed < 1) {
        // This looks wrong, but isn't.
        // If a building has a maxMaintenance of 75 and the road is at
        // -70 (ie, 5 from max) then we want the value of maintMax to be
        // 5 here.  Then we divide by maintFactor (some things are easier
        // to refix than others) to get how many items we need to fix it.
        // Then we fix it by that many items * maintFactor
        int maintMax = ObjectDefs[type].maxMaintenance + needed;
        maintMax /= ObjectDefs[type].maintFactor;
        if (num > maintMax) num = maintMax;
        if (itn < num) num = itn;
        job = "Performs maintenance on ";
        buildobj->incomplete -= (num * ObjectDefs[type].maintFactor);
        if (buildobj->incomplete < -(ObjectDefs[type].maxMaintenance))
            buildobj->incomplete = -(ObjectDefs[type].maxMaintenance);
    } else if (needed > 0) {
        if (num > needed) num = needed;
        if (itn < num) num = itn;
        job = "Performs construction on ";
        buildobj->incomplete -= num;
        if (buildobj->incomplete == 0) {
            job = "Completes construction of ";
            buildobj->incomplete = -(ObjectDefs[type].maxMaintenance);
            quests.check_build_target(r, type, u, &quest_rewards);
            if (buildobj->IsRoad()) quests.check_road_quest(r, type, u, &quest_rewards, regions, this->events);
            if (type == O_TOWER || type == O_MTOWER)
                quests.check_tower_quest(r, u, &quest_rewards, regions, this->events);
            if (type == O_INN)
                quests.check_inn_quest(r, u, &quest_rewards, regions, this->events);
        }
    }

    /* Perform the build */

    if (obj != buildobj) u->MoveUnit(buildobj);

    if (it == I_WOOD_OR_STONE) {
        if (preferred != -1) {
            u->ConsumeShared(preferred, num);
        } else {
            // Default: consume stone first, then wood
            if (num > u->GetSharedNum(I_STONE)) {
                num -= u->GetSharedNum(I_STONE);
                u->ConsumeShared(I_STONE, u->GetSharedNum(I_STONE));
                u->ConsumeShared(I_WOOD, num);
            } else {
                u->ConsumeShared(I_STONE, num);
            }
        }
    } else {
        u->ConsumeShared(it, num);
    }

    /* Regional economic improvement */
    r->improvement += num;

    // AS
    u->event(job + buildobj->name, "build");
    if (!quest_rewards.empty()) { u->event(quest_rewards, "quest"); }
    u->Practice(sk);
}

/* Alternate processing for building item-type ship
 * objects and instantiating fleets.
 */
void Game::RunBuildShipOrder(ARegion *r, Object *obj, Unit *u)
{
    int ship, skill, level, maxbuild, unfinished, output, percent;
    AString skname;

    ship = std::abs(u->build);
    skill = lookup_skill(ItemDefs[ship].pSkill);
    level = u->GetSkill(skill);

    if (skill == -1) {
        u->error("BUILD: Can't build " + ItemDefs[ship].name + ".");
        return;
    }

    // get needed to complete
    maxbuild = 0;
    if ((u->monthorders) && (u->monthorders->type == O_BUILD)) {
        BuildOrder *b = dynamic_cast<BuildOrder *>(u->monthorders);
        maxbuild = b->needtocomplete;
    }
    if (maxbuild < 1) {
        // Our helpers have already finished the hard work, so
        // just put the finishing touches on the new vessel
        unfinished = 0;
        // Also clear our month orders since it's done.
        delete u->monthorders;
        u->monthorders = nullptr;
    } else {
        output = ShipConstruction(r, u, u, level, maxbuild, ship);

        if (output < 1) return;

        // are there unfinished ship items of the given type?
        unfinished = u->items.GetNum(ship);

        if (unfinished == 0) {
            // Start a new ship's construction from scratch
            unfinished = ItemDefs[ship].pMonths;
            u->items.SetNum(ship, unfinished);
        }

        // Now reduce unfinished by produced amount
        unfinished -= output;
        if (unfinished < 0) unfinished = 0;
    }
    u->items.SetNum(ship, unfinished);

    // practice
    u->Practice(skill);

    if (unfinished == 0) {
        u->event("Finishes building a " + ItemDefs[ship].name + " in " + r->short_print() + ".", "build");
        CreateShip(r, u, ship);
    } else {
        percent = 100 * output / ItemDefs[ship].pMonths;
        u->event(
            "Performs construction work on a " + ItemDefs[ship].name + " (" + to_string(percent) + "%) in " +
                r->short_print() + ".",
            "build", r
        );
    }
}

void Game::AddNewBuildings(ARegion *r)
{
    int i;
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if (u->monthorders && u->monthorders->type == O_BUILD) {
                BuildOrder *o = dynamic_cast<BuildOrder *>(u->monthorders);

                // If BUILD order was marked for creating new building
                // in parse phase, it is time to create one now.
                if (o->new_building != -1) {
                    if (o->new_building == u->object->type && u->object->incomplete > 0) {
                        // we have a complete for the type of building we are in and it's not finished, so no new
                        // building.
                        u->build = u->object->num; // keep the current building as the build target
                        break;
                    }

                    // Re-check SETTLEMENT_ONLY at the destination. The parse-time check
                    // uses the unit's position when orders are written (source region),
                    // which is wrong for units that sail — they should be checked where
                    // they actually build, not where they boarded the ship.
                    if (ObjectDefs[o->new_building].flags & ObjectType::SETTLEMENT_ONLY) {
                        if (!r->town) {
                            u->error("BUILD: " + ObjectDefs[o->new_building].name +
                                     " can only be built in settlements.");
                            o->new_building = -1;
                            continue;
                        }
                    }

                    // Defence-in-depth: re-check ONE_PER_REGION here. The parse-time
                    // check in ProcessBuildStructure cannot see BUILD orders from other
                    // units, so two builders in the same region can both pass parse when
                    // no such object exists yet. Without this guard they would both
                    // create one in the month phase.
                    if (ObjectDefs[o->new_building].flags & ObjectType::ONE_PER_REGION) {
                        bool collision = false;
                        for (const auto existing : r->objects) {
                            if (existing->type == o->new_building) { collision = true; break; }
                        }
                        if (collision) {
                            u->error("BUILD: " + ObjectDefs[o->new_building].name +
                                     " can only exist once in a region.");
                            o->new_building = -1;
                            continue;
                        }
                    }

                    for (i = 1; i < FLEET_NUM_START; i++) {
                        if (!r->GetObject(i)) break;
                    }
                    if (i < FLEET_NUM_START) {
                        Object *obj = new Object(r);
                        obj->type = o->new_building;
                        obj->incomplete = ObjectDefs[obj->type].cost;
                        obj->num = i;
                        {
                            // Use region race for cultural name generation.
                            // Buildings reflect the culture of the land, not the builder.
                            const ObjectType& ot = ObjectDefs[obj->type];
                            std::string autoName;
                            if (obj->type == O_INN)
                                autoName = getInnName();
                            else if (obj->IsRoad())
                                autoName = getRoadName(obj->type, r->race);
                            else if (obj->type == O_CARAVANSERAI)
                                autoName = getCaravanseraiName(r->race);
                            else if (obj->type == O_TOWN_HALL)
                                autoName = getTownHallName(r->race);
                            else if (obj->type == O_CANAL || obj->type == O_MCANAL)
                                autoName = getCanalName();
                            else if (ot.productionAided != -1)
                                autoName = getProductionBuildingName(obj->type, ot.productionAided, r->race);
                            else
                                autoName = getObjectName(obj->type, ot);
                            obj->set_name(autoName.empty() ? "Building" : autoName);
                            u->event("Construction started: " + obj->name + " (" + ot.name + ")", "building");
                        }
                        u->build = obj->num;
                        r->objects.push_back(obj);

                        // This moves unit to a new building.
                        // This unit might be processed again but from new object.
                        u->MoveUnit(obj);
                        // This why we need to unset new_building so it will not
                        // try to create new object again.
                        o->new_building = -1;
                    } else {
                        u->error("BUILD: The region is full.");
                    }
                }
            }
        }
    }
}

void Game::RunBuildHelpers(ARegion *r)
{
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if (u->monthorders && u->monthorders->type == O_BUILD) {
                BuildOrder *o = dynamic_cast<BuildOrder *>(u->monthorders);
                Object *tarobj = NULL;
                if (o->target) {
                    Unit *target = r->GetUnitId(o->target, u->faction->num);
                    if (!target) {
                        u->error("BUILD: No such unit to help.");
                        delete u->monthorders;
                        u->monthorders = nullptr;
                        continue;
                    }
                    // Make sure that unit is building
                    if (!target->monthorders || target->monthorders->type != O_BUILD) {
                        u->error("BUILD: Unit isn't building.");
                        delete u->monthorders;
                        u->monthorders = nullptr;
                        continue;
                    }
                    // Make sure that unit considers you friendly!
                    if (target->faction->get_attitude(u->faction->num) < AttitudeType::FRIENDLY) {
                        u->error("BUILD: Unit you are helping rejects your help.");
                        delete u->monthorders;
                        u->monthorders = nullptr;
                        continue;
                    }
                    if (target->build == 0) {
                        // Help with whatever building the target is in
                        tarobj = target->object;
                        u->build = tarobj->num;
                    } else if (target->build > 0) {
                        u->build = target->build;
                        tarobj = r->GetObject(target->build);
                    } else {
                        // help build ships
                        int ship = std::abs(target->build);
                        int skill = lookup_skill(ItemDefs[ship].pSkill);
                        int level = u->GetSkill(skill);
                        int needed = 0;
                        if (target->monthorders && (target->monthorders->type == O_BUILD)) {
                            BuildOrder *border = dynamic_cast<BuildOrder *>(target->monthorders);
                            needed = border->needtocomplete;
                        }
                        if (needed < 1) {
                            u->error("BUILD: Construction is already complete.");
                            delete u->monthorders;
                            u->monthorders = nullptr;
                            continue;
                        }
                        int output = ShipConstruction(r, u, target, level, needed, ship);
                        if (output < 1) continue;

                        int unfinished = target->items.GetNum(ship);
                        if (unfinished == 0) {
                            // Start construction on a new ship
                            unfinished = ItemDefs[ship].pMonths;
                            target->items.SetNum(ship, unfinished);
                        }
                        unfinished -= output;

                        // practice
                        u->Practice(skill);

                        if (unfinished > 0) {
                            target->items.SetNum(ship, unfinished);
                            if (target->monthorders && (target->monthorders->type == O_BUILD)) {
                                BuildOrder *border = dynamic_cast<BuildOrder *>(target->monthorders);
                                border->needtocomplete = unfinished;
                            }
                        } else {
                            // CreateShip(r, target, ship);
                            // don't create the ship yet; leave that for the unit we're helping
                            target->items.SetNum(ship, 1);
                            if (target->monthorders && (target->monthorders->type == O_BUILD)) {
                                BuildOrder *border = dynamic_cast<BuildOrder *>(target->monthorders);
                                border->needtocomplete = 0;
                            }
                        }
                        int percent = 100 * output / ItemDefs[ship].pMonths;
                        u->event(
                            "Helps " + target->name + " with construction of a " + ItemDefs[ship].name + " (" +
                                std::to_string(percent) + "%) in " + r->short_print() + ".",
                            "build", r
                        );
                    }
                    // no need to move unit if item-type ships
                    // are being built. (leave this commented out)
                    // if (tarobj == NULL) tarobj = target->object;
                    if ((tarobj != NULL) && (u->object != tarobj)) u->MoveUnit(tarobj);
                } else {
                    Object *buildobj;
                    if (u->build > 0) {
                        buildobj = r->GetObject(u->build);
                        if (buildobj && buildobj != r->GetDummy() && buildobj != u->object) { u->MoveUnit(buildobj); }
                    }
                }
            }
        }
    }
}

/* Creates a new ship - either by adding it to a
 * compatible fleet object or creating a new fleet
 * object with Unit u as owner consisting of exactly
 * ONE ship of the given type.
 */
void Game::CreateShip(ARegion *r, Unit *u, int ship)
{
    Object *obj = u->object;
    // Do we need to create a new fleet?
    bool newfleet = true;
    if (u->object->IsFleet()) {
        newfleet = false;
        bool fleet_flying = obj->flying;
        bool ship_flies = ItemDefs[ship].fly > 0;

        switch (Globals->NEW_SHIP_JOINS_FLEET_BEHAVIOR) {
            case GameDefs::NewShipJoinsFleetBehavior::ALL_CROSS_JOIN:
                break;
            case GameDefs::NewShipJoinsFleetBehavior::ONLY_FLYING_CROSS_JOIN:
                if (ship_flies) break;
                newfleet = fleet_flying; // we aren't a flying ship, so only need a new fleet if fleet is flying.
                break;
            case GameDefs::NewShipJoinsFleetBehavior::NO_CROSS_JOIN:
                newfleet = (fleet_flying != ship_flies); // true if they are different, else false
                break;
            default:
                // this is an impossible case as we exhaustively check the enum
                break;
        }
    }
    if (newfleet) {
        // create a new fleet
        Object *fleet = new Object(r);
        fleet->type = O_FLEET;
        fleet->num = shipseq++;
        fleet->set_name("Ship");
        fleet->AddShip(ship);
        u->object->region->objects.push_back(fleet);
        u->MoveUnit(fleet);
        fleet->FleetCapacity();
    } else {
        obj->AddShip(ship);
        obj->FleetCapacity();
    }
}

// This is a utility function used by both ship building and unit production to correctly consume
// input items for production.  In the case of ORINPUT items it will make sure to consume items from the
// unit itself before consuming from the shared pool.
// Returns the number of items created.
int Game::consume_production_inputs(Unit *u, int item, int maxproduced)
{
    unsigned int maxInputs = sizeof(ItemDefs[0].pInput) / sizeof(ItemDefs[0].pInput[0]);

    if (ItemDefs[item].flags & ItemType::ORINPUTS) {
        // Figure out the max we can produce based on the inputs
        int count = 0;
        unsigned int c;
        for (c = 0; c < sizeof(ItemDefs[0].pInput) / sizeof(ItemDefs[0].pInput[0]); c++) {
            int i = ItemDefs[item].pInput[c].item;
            if (i != -1) count += u->GetSharedNum(i) / ItemDefs[item].pInput[c].amt;
        }
        if (maxproduced > count) maxproduced = count;
        count = maxproduced;

        if (count < 1) return 0;

        // Now consume the items from the unit itself first if possible.
        for (c = 0; c < maxInputs; c++) {
            int i = ItemDefs[item].pInput[c].item;
            int a = ItemDefs[item].pInput[c].amt;
            if (i != -1) {
                // Consume from the unit's own items first
                int amt = u->items.GetNum(i);
                if (count > amt / a) {
                    count -= amt / a;
                    u->items.SetNum(i, amt - ((amt / a) * a));
                } else {
                    u->items.SetNum(i, amt - (count * a));
                    count = 0;
                }
            }
        }

        // If we paid for everything, return now.
        if (count == 0) return maxproduced;

        // Deduct the items spent
        for (c = 0; c < maxInputs; c++) {
            int i = ItemDefs[item].pInput[c].item;
            int a = ItemDefs[item].pInput[c].amt;
            if (i != -1) {
                int amt = u->GetSharedNum(i);
                if (count > amt / a) {
                    count -= amt / a;
                    u->ConsumeShared(i, (amt / a) * a);
                } else {
                    u->ConsumeShared(i, count * a);
                    count = 0;
                }
            }
        }
    } else {
        // Figure out the max we can produce based on the inputs
        unsigned int c;
        for (c = 0; c < maxInputs; c++) {
            int i = ItemDefs[item].pInput[c].item;
            if (i != -1) {
                int amt = u->GetSharedNum(i);
                if ((amt / ItemDefs[item].pInput[c].amt) < maxproduced) {
                    maxproduced = amt / ItemDefs[item].pInput[c].amt;
                }
            }
        }

        // If we can't produce anything, return 0
        if (maxproduced < 1) return 0;

        // Deduct the items spent
        for (c = 0; c < maxInputs; c++) {
            int i = ItemDefs[item].pInput[c].item;
            int a = ItemDefs[item].pInput[c].amt;
            if (i != -1) { u->ConsumeShared(i, maxproduced * a); }
        }
    }
    return maxproduced;
}

/* Checks and returns the amount of ship construction,
 * handles material use and practice for both the main
 * shipbuilders and the helpers.
 */
int Game::ShipConstruction(ARegion *r, Unit *u, Unit *target, int level, int needed, int ship)
{
    if (!Globals->BUILD_NO_TRADE && !ActivityCheck(r, u->faction, FactionActivity::TRADE)) {
        u->error("BUILD: Faction can't produce in that many regions.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return 0;
    }

    if (level < ItemDefs[ship].pLevel) {
        u->error("BUILD: Can't build " + ItemDefs[ship].name + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return 0;
    }

    // are there unfinished ship items of the given type?
    int unfinished = target->items.GetNum(ship);

    int number = u->GetMen() * level + u->GetProductionBonus(ship);

    // find the max we can possibly produce based on man-months of labor
    int maxproduced = (ItemDefs[ship].flags & ItemType::SKILLOUT) ? u->GetMen() : number;

    // adjust maxproduced for items needed until completion
    if (needed < maxproduced) maxproduced = needed;

    // adjust maxproduced for unfinished ships
    if ((unfinished > 0) && (maxproduced > unfinished)) maxproduced = unfinished;

    maxproduced = consume_production_inputs(u, ship, maxproduced);
    if (maxproduced < 1) {
        // We don't have enough input items to produce anything
        u->error("BUILD: Don't have the required materials to build " + ItemDefs[ship].name + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return 0;
    }
    r->improvement += maxproduced; // regional economic improvement

    int output = maxproduced * ItemDefs[ship].pOut;
    if (ItemDefs[ship].flags & ItemType::SKILLOUT) output *= level;

    delete u->monthorders;
    u->monthorders = nullptr;

    return output;
}

void Game::RunMonthOrders()
{
    for (const auto r : regions) {
        RunIdleOrders(r);
        RunStudyOrders(r);
        AddNewBuildings(r);
        RunBuildHelpers(r);
        RunProduceOrders(r);
        RunExploreOrders(r);
    }
}

void Game::RunUnitProduce(ARegion *r, Unit *u)
{
    ProduceOrder *o = (ProduceOrder *)u->monthorders;

    for (const auto& p : r->products) {
        // PRODUCE orders for producing goods from the land
        // are shared among factions, and therefore handled
        // specially by the RunAProduction() function
        if (o->skill == p->skill && o->item == p->itemtype) return;
    }

    if (o->item == I_SILVER) {
        if (!o->quiet) u->error("Can't do that in this region.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    if (o->item == -1 || ItemDefs[o->item].flags & ItemType::DISABLED) {
        std::string name = (o->item == -1) ? "that" : item_string(o->item, 1, ALWAYSPLURAL);
        u->error("PRODUCE: Can't produce " + name + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    int input = ItemDefs[o->item].pInput[0].item;
    if (input == -1) {
        u->error("PRODUCE: Can't produce " + item_string(o->item, 1, ALWAYSPLURAL) + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    int level = u->GetSkill(o->skill);
    if (level < ItemDefs[o->item].pLevel) {
        u->error("PRODUCE: Can't produce " + item_string(o->item, 1, ALWAYSPLURAL) + ".");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    // LLS
    int number = u->GetMen() * level + u->GetProductionBonus(o->item);

    if (!ActivityCheck(r, u->faction, FactionActivity::TRADE)) {
        u->error("PRODUCE: Faction can't produce in that many regions.");
        delete u->monthorders;
        u->monthorders = nullptr;
        return;
    }

    // find the max we can possibly produce based on man-months of labor
    int maxproduced;
    if (ItemDefs[o->item].flags & ItemType::SKILLOUT)
        maxproduced = u->GetMen();
    else if (ItemDefs[o->item].flags & ItemType::SKILLOUT_HALF)
        maxproduced = u->GetMen();
    else
        maxproduced = number / ItemDefs[o->item].pMonths;

    if (o->target > 0 && maxproduced > o->target) maxproduced = o->target;

    maxproduced = consume_production_inputs(u, o->item, maxproduced);
    r->improvement += maxproduced; // regional economic improvement

    // Now give the items produced
    int output = maxproduced * ItemDefs[o->item].pOut;
    if (ItemDefs[o->item].flags & ItemType::SKILLOUT) output *= level;
    if (ItemDefs[o->item].flags & ItemType::SKILLOUT_HALF) { output *= (level + 1) / 2; }

    u->items.SetNum(o->item, u->items.GetNum(o->item) + output);
    u->event("Produces " + item_string(o->item, output) + " in " + r->short_print() + ".", "produce", r);
    u->Practice(o->skill);
    o->target -= output;
    if (o->target > 0) {
        TurnOrder *tOrder = new TurnOrder;
        tOrder->repeating = 0;
        std::string order = "PRODUCE " + to_string(o->target) + " " + ItemDefs[o->item].abr;
        tOrder->turnOrders.push_back(order);
        u->turnorders.push_front(tOrder);
    }
    delete u->monthorders;
    u->monthorders = nullptr;
}

void Game::RunProduceOrders(ARegion *r)
{
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if (u->monthorders) {
                if (u->monthorders->type == O_PRODUCE) {
                    RunUnitProduce(r, u);
                } else if (u->monthorders->type == O_BUILD) {
                    if (u->build >= 0) {
                        Run1BuildOrder(r, obj, u);
                    } else {
                        RunBuildShipOrder(r, obj, u);
                    }
                } else if (u->monthorders->type == O_CREATE) {
                    Run1CreateOrder(r, u);
                    delete u->monthorders;
                    u->monthorders = nullptr;
                }
            }
        }
        // Cleanup any build 'complete' orders where the object has been built or save them off for next month if not
        for (const auto u : obj->units) {
            if (u->monthorders && u->monthorders->type == O_BUILD) {
                BuildOrder *o = dynamic_cast<BuildOrder *>(u->monthorders);
                if (o->until_complete) {
                    if ((u->build > 0 || o->target) && u->object->incomplete > 0) {
                        string order = "BUILD ";
                        if (o->target) {
                            Unit *t = r->GetUnitId(o->target, u->faction->num);
                            order += "HELP " + to_string(t->num);
                        } else {
                            string name = ObjectDefs[u->object->type].name;
                            if (name.find(" ") != std::string::npos) {
                                // If the name has spaces, we need to quote it
                                order += "\"" + name + "\"";
                            } else {
                                order += name;
                            }
                        }
                        order += " COMPLETE";
                        u->oldorders.push_front(order);
                    } else if (u->build < 0 || o->target) {
                        string order = "BUILD ";
                        BuildOrder *border = nullptr;
                        Unit *t;
                        if (o->target) {
                            t = r->GetUnitId(o->target, u->faction->num);
                            if (t->monthorders && (t->monthorders->type == O_BUILD)) {
                                border = dynamic_cast<BuildOrder *>(t->monthorders);
                            }
                        } else {
                            border = dynamic_cast<BuildOrder *>(u->monthorders);
                        }
                        if (border && border->needtocomplete > 0) {
                            if (o->target) {
                                order += "HELP " + to_string(t->num);
                            } else {
                                order += ItemDefs[-(u->build)].abr;
                            }
                            order += " COMPLETE";
                            u->oldorders.push_front(order);
                        }
                    }
                }
                delete u->monthorders;
                u->monthorders = nullptr; // clear the monthorders to avoid reprocessing
            }
        }
    }
    for (const auto& p : r->products) RunAProduction(r, p);
}

int Game::ValidProd(Unit *u, ARegion *r, Production *p)
{
    if (u->monthorders->type != O_PRODUCE) return 0;

    ProduceOrder *po = dynamic_cast<ProduceOrder *>(u->monthorders);
    if (p->itemtype == po->item && p->skill == po->skill) {
        if (p->skill == -1) {
            /* Factor for fractional productivity: 10 */
            po->productivity = (int)((float)(u->GetMen() * p->productivity / 10));
            return po->productivity;
        }
        int level = u->GetSkill(p->skill);
        if (level < ItemDefs[p->itemtype].pLevel) {
            u->error("PRODUCE: Unit isn't skilled enough to produce " + ItemDefs[p->itemtype].name + ".");
            delete u->monthorders;
            u->monthorders = nullptr;
            return 0;
        }

        //
        // Check faction limits on production. If the item is silver, then the
        // unit is entertaining or working, and the limit does not apply
        //
        if (p->itemtype != I_SILVER && !ActivityCheck(r, u->faction, FactionActivity::TRADE)) {
            u->error("PRODUCE: Faction can't produce in that many regions.");
            delete u->monthorders;
            u->monthorders = nullptr;
            return 0;
        }

        /* check for bonus production */
        // LLS
        int bonus = u->GetProductionBonus(p->itemtype);
        /* Factor for fractional productivity: 10 */
        po->productivity = (int)((float)(u->GetMen() * level * p->productivity / 10)) + bonus;
        if (po->target > 0 && po->productivity > po->target) po->productivity = po->target;
        return po->productivity;
    }
    return 0;
}

int Game::FindAttemptedProd(ARegion *r, Production *p)
{
    int attempted = 0;
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if ((u->monthorders) && (u->monthorders->type == O_PRODUCE)) { attempted += ValidProd(u, r, p); }
        }
    }
    return attempted;
}

void Game::RunAProduction(ARegion *r, Production *p)
{
    int questcomplete;
    string quest_rewards;

    p->activity = 0;
    if (p->amount == 0) return;

    /* First, see how many units are trying to work */
    int attempted = FindAttemptedProd(r, p);
    int amt = p->amount;
    if (attempted < amt) attempted = amt;
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            questcomplete = 0;
            if (!u->monthorders || u->monthorders->type != O_PRODUCE) continue;

            ProduceOrder *po = dynamic_cast<ProduceOrder *>(u->monthorders);
            if (po->skill != p->skill || po->item != p->itemtype) continue;

            /* We need to implement a hack to avoid overflowing */
            int uatt, ubucks;

            uatt = po->productivity;
            if (uatt && amt && attempted) {
                double dUbucks = ((double)amt) * ((double)uatt) / ((double)attempted);
                ubucks = (int)dUbucks;
                questcomplete = quests.check_harvest_target(r, po->item, ubucks, amt, u, &quest_rewards);
            } else {
                ubucks = 0;
            }

            amt -= ubucks;
            attempted -= uatt;
            u->items.SetNum(po->item, u->items.GetNum(po->item) + ubucks);
            u->faction->DiscoverItem(po->item, 0, 1);
            p->activity += ubucks;
            po->target -= ubucks;
            if (po->target > 0) {
                TurnOrder *tOrder = new TurnOrder;
                tOrder->repeating = 0;
                std::string order = "PRODUCE " + to_string(po->target) + " " + ItemDefs[po->item].abr;
                tOrder->turnOrders.push_back(order);
                u->turnorders.push_front(tOrder);
            }

            /* Show in unit's events section */
            if (po->item == I_SILVER) {
                //
                // WORK
                //
                if (po->skill == -1) {
                    u->event(
                        "Earns " + to_string(ubucks) + " silver working in " + r->short_print() + ".",
                        "work", r
                    );
                } else {
                    //
                    // ENTERTAIN
                    //
                    u->event(
                        "Earns " + std::to_string(ubucks) + " silver entertaining in " + r->short_print() + ".",
                        "entertain", r
                    );
                    // If they don't have PHEN, then this will fail safely
                    u->Practice(S_PHANTASMAL_ENTERTAINMENT);
                    u->Practice(S_ENTERTAINMENT);
                }
            } else {
                /* Everything else */
                u->event("Produces " + item_string(po->item, ubucks) + " in " + r->short_print() + ".", "produce", r);
                u->Practice(po->skill);
            }
            delete u->monthorders;
            u->monthorders = nullptr;
            if (questcomplete) { u->event("You have completed a quest! " + quest_rewards, "quest"); }
        }
    }
}

void Game::RunStudyOrders(ARegion *r)
{
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if (u->monthorders) {
                if (u->monthorders->type == O_STUDY) {
                    Do1StudyOrder(u, obj);
                    delete u->monthorders;
                    u->monthorders = nullptr;
                }
            }
        }
    }
}

void Game::RunIdleOrders(ARegion *r)
{
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if (u->monthorders && u->monthorders->type == O_IDLE) {
                u->event("Sits idle.", "idle");
                delete u->monthorders;
                u->monthorders = nullptr;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// CREATE VILLAGE order
// ---------------------------------------------------------------------------

#include "village_founding.h"

/// Races that cannot found a settlement; defined in neworigins/extra.cpp
extern const std::vector<int> CANNOT_FOUND_SETTLEMENT;
extern const std::vector<int> RACE_NEUTRAL_FOUNDERS;

/// Item costs required to found a village (consumed on success).
/// Terminated by { -1, 0, nullptr }. Add entries here to tune costs.
const SettlementCost VILLAGE_ITEM_COSTS[] = {
    { I_WAGON, 100, "CREATE: You need at least 100 wagons to found a village." },
    // Future additional costs:
    // { I_STONE, 50, "CREATE: You need at least 50 stone to found a village." },
    { -1, 0, nullptr }   // terminator
};

/**
 * @brief Execute the CREATE VILLAGE <name> month-long order.
 *
 * The founding unit must have:
 *  - ≥ VILLAGE_FOUND_MEN people total (IT_MAN + IT_LEADER)
 *  - ≥ required amounts of VILLAGE_ITEM_COSTS items
 *
 * On success:
 *  - 1000 men consumed (primary IT_MAN type first; overflow from other IT_MAN races, largest first)
 *  - Item costs consumed
 *  - Region race set to primary IT_MAN type (unchanged if only leaders)
 *  - Village created with full markets (food + trade goods + recruitment)
 *
 * @see VILLAGE_FOUND_MEN, VILLAGE_ITEM_COSTS, CANNOT_FOUND_SETTLEMENT
 */
void Game::Run1CreateOrder(ARegion *r, Unit *u)
{
    CreateOrder *order = dynamic_cast<CreateOrder *>(u->monthorders);

    // --- 1. Only VILLAGE supported for now ---
    if (order->settlementType != TOWN_VILLAGE) {
        u->error("CREATE: Only villages can be founded this way.");
        return;
    }

    // --- 2. Region must not already have a settlement ---
    if (r->town) {
        u->error("CREATE: There is already a settlement in this region.");
        return;
    }

    // --- 3. Terrain must allow settlement ---
    if (TerrainDefs[r->type].similar_type == R_OCEAN) {
        u->error("CREATE: Cannot found a settlement in ocean or lake terrain.");
        return;
    }
    if (r->type == R_VOLCANO) {
        u->error("CREATE: Cannot found a settlement on a volcano.");
        return;
    }
    if (TerrainDefs[r->type].flags & TerrainType::BARREN) {
        u->error("CREATE: Cannot found a settlement in this terrain.");
        return;
    }

    // --- 4. No existing settlement within 2 hexes ---
    for (int d = 0; d < NDIRS; d++) {
        ARegion *n1 = r->neighbors[d];
        if (!n1) continue;
        if (n1->town) {
            u->error("CREATE: There is a settlement too close to found a new one here.");
            return;
        }
        for (int d2 = 0; d2 < NDIRS; d2++) {
            ARegion *n2 = n1->neighbors[d2];
            if (!n2) continue;
            if (n2->town) {
                u->error("CREATE: There is a settlement too close to found a new one here.");
                return;
            }
        }
    }

    // --- 5. Count settlers: all IT_MAN items + I_LEADERS ---
    int total_people = 0;
    int primary_man_type = -1;
    int primary_man_count = 0;
    int leaders_count = 0;

    for (const auto& it : u->items) {
        int itype = it->type;
        if (itype < 0 || itype >= NITEMS) continue;
        int count = it->num;
        if (count <= 0) continue;
        // Check I_LEADERS first — leaders are race-neutral and must not
        // become the primary_man_type even if IT_MAN flag is set on them.
        if (itype == I_LEADERS) {
            leaders_count += count;
            total_people += count;
        } else if (ItemDefs[itype].type & IT_MAN) {
            total_people += count;
            if (count > primary_man_count) {
                primary_man_count = count;
                primary_man_type = itype;
            }
        }
    }

    if (total_people < VILLAGE_FOUND_MEN) {
        u->error("CREATE: You need at least " + to_string(VILLAGE_FOUND_MEN) +
                 " people (men or leaders) to found a village.");
        return;
    }

    // --- 6. Check forbidden races ---
    if (primary_man_type != -1) {
        for (int forbidden : CANNOT_FOUND_SETTLEMENT) {
            if (primary_man_type == forbidden) {
                u->error("CREATE: This race cannot found a settlement.");
                return;
            }
        }
    }

    // --- 7. Check item costs ---
    for (int i = 0; VILLAGE_ITEM_COSTS[i].item != -1; i++) {
        int have = u->items.GetNum(VILLAGE_ITEM_COSTS[i].item);
        if (have < VILLAGE_ITEM_COSTS[i].amount) {
            u->error(VILLAGE_ITEM_COSTS[i].error_msg);
            return;
        }
    }

    // ===== All checks passed — execute =====

    // Preserve the founding race for consumption (before race-neutral check clears it)
    int consume_from_type = primary_man_type;

    // --- 8. Check race-neutral founders ---
    // Some races can found a village but are magically/culturally neutral —
    // they do not impose their race on the region.
    if (primary_man_type != -1) {
        for (int neutral : RACE_NEUTRAL_FOUNDERS) {
            if (primary_man_type == neutral) {
                primary_man_type = -1; // suppress race change; still consume normally
                break;
            }
        }
    }

    // --- 9. Determine region race ---
    // Race changes only when the primary IT_MAN group strictly outnumbers leaders
    // AND the race is not in RACE_NEUTRAL_FOUNDERS (primary_man_type cleared above).
    // Examples: 1200 men + 0 leaders → change; 600 men + 400 leaders → change;
    //           200 men + 800 leaders → no change; 1000 leaders → no change;
    //           1000 fairies → no change (race-neutral).
    if (primary_man_type != -1 && primary_man_count > leaders_count) {
        r->race = primary_man_type;
    }

    // --- 10. Consume settlers (1000 from primary IT_MAN type; overflow from other IT_MAN races, largest first) ---
    int to_consume = VILLAGE_FOUND_MEN;
    if (consume_from_type != -1 && primary_man_count > 0) {
        int from_men = min(to_consume, primary_man_count);
        u->items.SetNum(consume_from_type, primary_man_count - from_men);
        to_consume -= from_men;
    }
    if (to_consume > 0) {
        std::vector<std::pair<int,int>> others;
        for (const auto& it : u->items) {
            if (it->type == consume_from_type) continue;
            if (!(ItemDefs[it->type].type & IT_MAN)) continue;
            if (it->num > 0) others.push_back({it->type, it->num});
        }
        std::sort(others.begin(), others.end(),
                  [](const auto& a, const auto& b){ return a.second > b.second; });
        for (auto& [itype, count] : others) {
            if (to_consume <= 0) break;
            int take = min(to_consume, count);
            u->items.SetNum(itype, count - take);
            to_consume -= take;
        }
    }

    // --- 10. Consume item costs (wagons, etc.) ---
    for (int i = 0; VILLAGE_ITEM_COSTS[i].item != -1; i++) {
        int have = u->items.GetNum(VILLAGE_ITEM_COSTS[i].item);
        u->items.SetNum(VILLAGE_ITEM_COSTS[i].item, have - VILLAGE_ITEM_COSTS[i].amount);
    }

    // --- 11. Create the village (auto-generate name if not provided) ---
    std::string village_name = order->name;
    if (village_name.empty()) {
        int name_race = (primary_man_type != -1) ? primary_man_type : r->race;
        village_name = getEthnicName(raceToEthnicity(name_race));
    }
    r->add_town(TOWN_VILLAGE, village_name);

    logger::write("Village '" + village_name + "' founded by faction " +
                  to_string(u->faction->num) + " at (" +
                  to_string(r->xloc) + "," + to_string(r->yloc) + ").");

    if (this->events) {
        auto *vf = new VillageFoundedFact();
        vf->village_name  = village_name;
        vf->faction_name  = u->faction->name;
        vf->terrain_name  = TerrainDefs[TerrainDefs[r->type].similar_type].name;
        vf->region_name   = r->name;
        this->events->AddFact(vf);
    }

    // --- 12. Set up markets (trade goods + recruitment) ---
    r->SetupRandomTradeMarkets();
    r->markets.erase(
        remove_if(r->markets.begin(), r->markets.end(),
                  [](const Market *m) { return ItemDefs[m->item].type & IT_MAN; }),
        r->markets.end()
    );
    r->AddMenMarket();
    r->AddLeadersMarket();

    // --- 13. Report ---
    u->event("founds the village of " + village_name + ".", "create");
}

void Game::Do1StudyOrder(Unit *u, Object *obj)
{
    StudyOrder *o = dynamic_cast<StudyOrder *>(u->monthorders);
    int sk, cost, reset_man, skmax, taughtdays, days;

    reset_man = -1;
    sk = o->skill;
    if (sk == -1 || SkillDefs[sk].flags & SkillType::DISABLED ||
        (SkillDefs[sk].flags & SkillType::APPRENTICE && !Globals->APPRENTICES_EXIST)) {
        std::string name = (sk == -1) ? "that" : SkillDefs[sk].name;
        u->error("STUDY: Can't study " + name + ".");
        return;
    }

    // Check that the skill can be studied
    if (SkillDefs[sk].flags & SkillType::NOSTUDY) {
        u->error("STUDY: " + SkillDefs[sk].name + " cannot be studied.");
        return;
    }

    // Small patch for Ceran Mercs
    if (u->GetMen(I_MERC)) {
        u->error("STUDY: Mercenaries are not allowed to study.");
        return;
    }

    if (o->level != -1) {
        skmax = u->GetSkillMax(sk);
        if (skmax < o->level) {
            o->level = skmax;
            if (u->GetRealSkill(sk) >= o->level) {
                u->error("STUDY: Cannot study " + SkillDefs[sk].name + " beyond level " + to_string(o->level) + ".");
                return;
            } else {
                u->error(
                    "STUDY: set study goal for " + SkillDefs[sk].name + " to the maximum achievable level (" +
                    to_string(o->level) + ")."
                );
            }
        }
        if (u->GetRealSkill(sk) >= o->level) {
            u->error("STUDY: already reached specified level; nothing to study.");
            return;
        }
    }

    cost = SkillCost(sk) * u->GetMen();
    if (cost > u->GetSharedMoney()) {
        u->error("STUDY: Not enough funds to study " + SkillDefs[sk].name + ".");
        return;
    }

    if ((SkillDefs[sk].flags & SkillType::MAGIC) && u->type != U_MAGE) {
        if (u->type == U_APPRENTICE) {
            u->error(string("STUDY: An ") + Globals->APPRENTICE_NAME + " cannot be made into a mage.");
            return;
        }
        if (Globals->FACTION_LIMIT_TYPE != GameDefs::FACLIM_UNLIMITED) {
            if (CountMages(u->faction) >= AllowedMages(u->faction)) {
                u->error("STUDY: Can't have another magician.");
                return;
            }
        }
        if (u->GetMen() != 1) {
            u->error("STUDY: Only 1-man units can be magicians.");
            return;
        }
        if (!(Globals->MAGE_NONLEADERS)) {
            if (u->GetLeaders() != 1) {
                u->error("STUDY: Only leaders may study magic.");
                return;
            }
        }
        reset_man = u->type;
        u->type = U_MAGE;
    }

    if ((SkillDefs[sk].flags & SkillType::APPRENTICE) && u->type != U_APPRENTICE) {
        if (u->type == U_MAGE) {
            u->error(string("STUDY: A mage cannot be made into an ") + Globals->APPRENTICE_NAME + ".");
            return;
        }

        if (Globals->FACTION_LIMIT_TYPE != GameDefs::FACLIM_UNLIMITED) {
            if (CountApprentices(u->faction) >= AllowedApprentices(u->faction)) {
                u->error(string("STUDY: Can't have another ") + Globals->APPRENTICE_NAME + ".");
                return;
            }
        }
        if (u->GetMen() != 1) {
            u->error(string("STUDY: Only 1-man units can be ") + Globals->APPRENTICE_NAME + "s.");
            return;
        }
        if (!(Globals->MAGE_NONLEADERS)) {
            if (u->GetLeaders() != 1) {
                u->error(string("STUDY: Only leaders may be ") + Globals->APPRENTICE_NAME + "s.");
                return;
            }
        }
        reset_man = u->type;
        u->type = U_APPRENTICE;
    }

    if ((Globals->TRANSPORT & GameDefs::ALLOW_TRANSPORT) && (sk == S_QUARTERMASTER) &&
        (u->GetSkill(S_QUARTERMASTER) == 0) && (Globals->FACTION_LIMIT_TYPE == GameDefs::FACLIM_FACTION_TYPES)) {
        if (CountQuarterMasters(u->faction) >= AllowedQuarterMasters(u->faction)) {
            u->error("STUDY: Can't have another quartermaster.");
            return;
        }
        if (u->GetMen() != 1) {
            u->error("STUDY: Only 1-man units can be quartermasters.");
            return;
        }
    }

    // If TACTICS_NEEDS_WAR is enabled, and the unit is trying to study to tact-5,
    // check that there's still space...
    if (Globals->TACTICS_NEEDS_WAR && sk == S_TACTICS && u->GetSkill(sk) == 4 &&
        u->skills.GetDays(sk) / u->GetMen() >= 300) {
        if (CountTacticians(u->faction) >= AllowedTacticians(u->faction)) {
            u->error("STUDY: Can't start another level 5 tactics leader.");
            return;
        }
        if (u->GetMen() != 1) {
            u->error("STUDY: Only 1-man units can study to level 5 in tactics.");
            return;
        }

    } // end tactics check

    // adjust teaching for study rate
    taughtdays = ((long int)o->days * u->skills.GetStudyRate(sk, u->GetMen()) / 30);

    days = u->skills.GetStudyRate(sk, u->GetMen()) * u->GetMen() + taughtdays;

    if ((SkillDefs[sk].flags & SkillType::MAGIC) && u->GetRealSkill(sk) >= 2) {
        if (obj->incomplete > 0 || obj->type == O_DUMMY) {
            u->error("Warning: Magic study rate outside of a building cut in half above level 2.");
            days /= 2;
        } else if (obj->mages < 1) {
            if (!Globals->LIMITED_MAGES_PER_BUILDING || (!obj->IsFleet() && !ObjectDefs[obj->type].maxMages)) {
                u->error("Warning: Magic study rate cut in half above level 2 due to unsuitable building.");
            } else {
                u->error(
                    "Warning: Magic study rate cut in half above level 2 due to number of mages studying in structure."
                );
            }
            days /= 2;
        } else if (Globals->LIMITED_MAGES_PER_BUILDING) {
            obj->mages--;
        }
    }

    if (SkillDefs[sk].flags & SkillType::SLOWSTUDY) { days /= 2; }

    if (u->Study(sk, days)) {
        u->ConsumeSharedMoney(cost);
        string str = "Studies " + SkillDefs[sk].name;
        taughtdays = taughtdays / u->GetMen();
        if (taughtdays) { str += " and was taught for " + to_string(taughtdays) + " days"; }
        str += " at a cost of " + item_string(I_SILVER, cost) + ".";
        u->event(str, "study");
        // study to level order
        if (o->level != -1) {
            if (u->GetRealSkill(sk) < o->level) {
                TurnOrder *tOrder = new TurnOrder;
                tOrder->repeating = 0;
                std::string order = "STUDY " + string(SkillDefs[sk].abbr) + " " + to_string(o->level);
                tOrder->turnOrders.push_back(order);
                u->turnorders.push_front(tOrder);
            } else {
                string msg = "Completes study to level " + to_string(o->level) + " in " + SkillDefs[sk].name + ".";
                u->event(msg, "study");
            }
        }
    } else {
        // if we just tried to become a mage or apprentice, but
        // were unable to study, reset unit to whatever it was before.
        if (reset_man != -1) u->type = reset_man;
    }
}

void Game::DoMoveEnter(Unit *unit, ARegion *region)
{
    if (!unit->monthorders || ((unit->monthorders->type != O_MOVE) && (unit->monthorders->type != O_ADVANCE))) return;

    MoveOrder *o = dynamic_cast<MoveOrder *>(unit->monthorders);
    while (o->dirs.size()) {
        auto x = o->dirs.front();
        int i = x->dir;
        if (i != MOVE_OUT && i < MOVE_ENTER) return;
        std::erase(o->dirs, x);
        delete x;

        if (i >= MOVE_ENTER) {
            Object *to = region->GetObject(i - MOVE_ENTER);
            if (!to) {
                unit->error("MOVE: Can't find object.");
                continue;
            }

            if (!to->CanEnter(region, unit)) {
                unit->error("ENTER: Can't enter that.");
                continue;
            }

            Unit *forbid = to->ForbiddenBy(region, unit);
            if (forbid && !o->advancing) {
                unit->error("ENTER: Is refused entry.");
                continue;
            }

            if (forbid && region->IsSafeRegion()) {
                unit->error("ENTER: No battles allowed in safe regions.");
                continue;
            }

            if (forbid && !(unit->canattack && unit->IsAlive())) {
                unit->error("ENTER: Unable to attack " + forbid->name);
                continue;
            }

            bool done = false;
            while (forbid) {
                int result = RunBattle(region, unit, forbid, 0, 0);
                if (result == BATTLE_IMPOSSIBLE) {
                    unit->error("ENTER: Unable to attack " + forbid->name);
                    done = 1;
                    break;
                }
                if (!unit->canattack || !unit->IsAlive()) {
                    done = true;
                    break;
                }
                forbid = to->ForbiddenBy(region, unit);
            }
            if (done) continue;

            // Verify the target object still exists — NPC fleets are destroyed
            // immediately when all pirates die, so 'to' may be a dangling pointer
            if (std::find(region->objects.begin(), region->objects.end(), to) == region->objects.end()) {
                unit->event("The pirate vessel has sunk.", "movement");
                continue;
            }

            unit->MoveUnit(to);
            unit->event("Enters " + to->name + ".", "movement");
        } else {
            if (i == MOVE_OUT) {
                bool isOcean = (TerrainDefs[region->type].similar_type == R_OCEAN);
                if (isOcean && (!unit->CanSwim() || unit->GetFlag(FLAG_NOCROSS_WATER))) {
                    unit->error("MOVE: Can't leave ship.");
                    continue;
                }
                Object *to = region->GetDummy();
                unit->MoveUnit(to);
            }
        }
    }
}

Location *Game::DoAMoveOrder(Unit *unit, ARegion *region, Object *obj)
{
    MoveOrder *o = dynamic_cast<MoveOrder *>(unit->monthorders);
    ARegion *newreg;
    string road, temp;

    int movetype, cost, startmove, weight;
    Unit *ally, *forbid;
    Location *loc;
    std::optional<std::string> prevented = std::nullopt;

    if (!o->dirs.size()) {
        delete o;
        unit->monthorders = nullptr;
        return 0;
    }

    auto x = o->dirs.front();

    if (x->dir == MOVE_IN) {
        if (obj->inner == -1) {
            unit->error("MOVE: Can't move IN there.");
            goto done_moving;
        }

        // Make sure that items which cannot go through a shaft don't
        for (auto i : unit->items) {
            if (ItemDefs[i->type].flags & ItemType::NO_SHAFT) {
                unit->error("MOVE: Unable to fit through the shaft.");
                goto done_moving;
            }
        }

        newreg = regions.GetRegion(obj->inner);
        if (obj->type == O_GATEWAY) {
            // Gateways exist in the nexus and move the unit to a region of the target terrain type.
            // See docs/GATEWAY_ENTRY_SYSTEM.md for full algorithm description.
            //
            // Block A — own terrain (chosen gateway), phases 1 and 2, with filter then without:
            //   1  : VILLAGE, no player units,              resource filter ON
            //   2  : VILLAGE, guard + 1-2 small units,     resource filter ON
            //   1a : VILLAGE, no player units,              resource filter OFF
            //   2a : VILLAGE, guard + 1-2 small units,     resource filter OFF
            // Block B — other 7 terrain types (cycling), same phases 1, 2, 1a, 2a
            // Phases 3/3a — all terrains (own first), any village players, filter ON then OFF
            // Phase 4 — TOWN or CITY, ≤3 players, all gateways, no filter
            // Phase 5 — empty non-town hex, own terrain, no filter
            // Phase 6 — any region of own terrain, ultimate fallback
            // Monsters (monfaction) disqualify a region in phases 1-5.

            ARegion *anchor = newreg;
            ARegionArray *level = regions.GetRegionArray(anchor->zloc);
            int own_terrain = anchor->type;

            // Collect all gateways in the nexus and find index of the entered one
            std::vector<Object *> gateways;
            for (const auto g : region->objects)
                if (g->type == O_GATEWAY) gateways.push_back(g);
            int startIdx = 0;
            for (int i = 0; i < (int)gateways.size(); i++)
                if (gateways[i] == obj) { startIdx = i; break; }

            // Count non-guard, non-monster player units in a region
            auto count_players = [&](ARegion *r) -> int {
                int cnt = 0;
                for (const auto ro : r->objects)
                    for (const auto u : ro->units)
                        if (u->faction->num != guardfaction && u->faction->num != monfaction)
                            cnt++;
                return cnt;
            };

            // True if any monster units are present in the region
            auto has_monsters = [&](ARegion *r) -> bool {
                for (const auto ro : r->objects)
                    for (const auto u : ro->units)
                        if (u->faction->num == monfaction) return true;
                return false;
            };

            // Phase 2 eligibility: 1-2 non-guard non-monster units, each with ≤2 men
            auto is_phase2 = [&](ARegion *r) -> bool {
                int cnt = 0;
                for (const auto ro : r->objects) {
                    for (const auto u : ro->units) {
                        if (u->faction->num == guardfaction || u->faction->num == monfaction) continue;
                        if (u->GetMen() > 2) return false;
                        if (++cnt >= 3) return false;
                    }
                }
                return cnt > 0;
            };

            // Find matching villages of a given terrain type
            // vphase: 1=empty, 2=small players, 3=any players
            auto find_villages = [&](int terrain, int vphase, bool use_filter) -> std::vector<ARegion *> {
                auto cands = level->get_starting_region_candidates(terrain, use_filter);
                std::vector<ARegion *> matching;
                for (const auto r : cands) {
                    if (!r->town || r->town->TownType() != TOWN_VILLAGE) continue;
                    if (has_monsters(r)) continue;
                    int np = count_players(r);
                    if (vphase == 1 && np == 0) matching.push_back(r);
                    else if (vphase == 2 && is_phase2(r)) matching.push_back(r);
                    else if (vphase == 3 && np > 0) matching.push_back(r);
                }
                return matching;
            };

            ARegion *found = nullptr;
            int found_phase = 0; // 1=Block A, 2=Block B, 3=Phase3, 4=Town, 5=Empty hex, 6=Fallback

            // Block A: own terrain, phases 1 and 2 (with filter, then without)
            for (int vphase = 1; vphase <= 2 && !found; vphase++) {
                for (int use_filter = 1; use_filter >= 0 && !found; use_filter--) {
                    auto matching = find_villages(own_terrain, vphase, use_filter == 1);
                    if (!matching.empty()) {
                        found = matching[rng::get_random(matching.size())];
                        found_phase = 1;
                    }
                }
            }

            // Block B: other terrain types, phases 1 and 2 (with filter, then without)
            for (int vphase = 1; vphase <= 2 && !found; vphase++) {
                for (int use_filter = 1; use_filter >= 0 && !found; use_filter--) {
                    for (int gi = 1; gi < (int)gateways.size() && !found; gi++) {
                        int idx = (startIdx + gi) % (int)gateways.size();
                        ARegion *gw_anchor = regions.GetRegion(gateways[idx]->inner);
                        auto matching = find_villages(gw_anchor->type, vphase, use_filter == 1);
                        if (!matching.empty()) {
                            found = matching[rng::get_random(matching.size())];
                            found_phase = 2;
                        }
                    }
                }
            }

            // Phases 3/3a: all terrain types, any village with players (own first, then others)
            for (int use_filter = 1; use_filter >= 0 && !found; use_filter--) {
                auto matching = find_villages(own_terrain, 3, use_filter == 1);
                if (!matching.empty()) {
                    found = matching[rng::get_random(matching.size())];
                    found_phase = 3;
                    break;
                }
                for (int gi = 1; gi < (int)gateways.size() && !found; gi++) {
                    int idx = (startIdx + gi) % (int)gateways.size();
                    ARegion *gw_anchor = regions.GetRegion(gateways[idx]->inner);
                    matching = find_villages(gw_anchor->type, 3, use_filter == 1);
                    if (!matching.empty()) {
                        found = matching[rng::get_random(matching.size())];
                        found_phase = 3;
                    }
                }
            }

            // Phase 4: TOWN or CITY, guard + ≤3 players, no monsters, all gateways, no filter
            if (!found) {
                for (int gi = 0; gi < (int)gateways.size() && !found; gi++) {
                    int idx = (startIdx + gi) % (int)gateways.size();
                    ARegion *gw_anchor = regions.GetRegion(gateways[idx]->inner);
                    auto cands = level->get_starting_region_candidates(gw_anchor->type, false);
                    std::vector<ARegion *> matching;
                    for (const auto r : cands) {
                        if (!r->town || r->town->TownType() == TOWN_VILLAGE) continue;
                        if (has_monsters(r)) continue;
                        if (count_players(r) <= 3) matching.push_back(r);
                    }
                    if (!matching.empty()) {
                        found = matching[rng::get_random(matching.size())];
                        found_phase = 4;
                    }
                }
            }

            // Phase 5: empty non-town hex, no monsters, own terrain only, no filter
            if (!found) {
                auto cands = level->get_starting_region_candidates(own_terrain, false);
                std::vector<ARegion *> matching;
                for (const auto r : cands) {
                    if (r->town) continue;
                    if (has_monsters(r)) continue;
                    if (count_players(r) == 0) matching.push_back(r);
                }
                if (!matching.empty()) {
                    found = matching[rng::get_random(matching.size())];
                    found_phase = 5;
                }
            }

            // Phase 6: any region of own terrain, ultimate fallback, no filter
            if (!found) {
                auto cands = level->get_starting_region_candidates(own_terrain, false);
                if (!cands.empty()) {
                    found = cands[rng::get_random(cands.size())];
                    found_phase = 6;
                }
            }

            if (found) {
                newreg = found;
                string terrain_name = TerrainDefs[own_terrain].name;
                if (found_phase == 2) {
                    string dest_terrain = TerrainDefs[found->type].name;
                    unit->event("Warning: all " + terrain_name + " villages are occupied. "
                        "You were redirected to a " + dest_terrain + " village instead.", "move");
                } else if (found_phase == 3) {
                    unit->event("Warning: all empty " + terrain_name + " villages are occupied. "
                        "You share this village with existing players.", "move");
                } else if (found_phase >= 4) {
                    unit->event("Warning: all " + terrain_name + " villages are occupied. "
                        "You were placed in a non-village region.", "move");
                }
            }
        }
    } else if (x->dir == MOVE_PAUSE) {
        newreg = region;
    } else {
        newreg = region->neighbors[x->dir];
    }

    if (!newreg) {
        unit->error("MOVE: Can't move that direction.");
        goto done_moving;
    }

    // Check for any keybarrier objects in the target region
    for (const auto o : newreg->objects) {
        if (ObjectDefs[o->type].flags & ObjectType::KEYBARRIER) {
            if (unit->items.GetNum(ObjectDefs[o->type].key_item) < 1) {
                unit->error("MOVE: A mystical barrier prevents movement in that direction.");
                goto done_moving;
            }
        }
    }

    prevented = newreg->movement_forbidden_by_ruleset(unit, region, regions);
    if (prevented) {
        unit->error("MOVE: " + *prevented + " prevents movement in that direction.");
        goto done_moving;
    }

    unit->movepoints += unit->CalcMovePoints(region);

    road = "";
    startmove = 0;
    movetype = unit->MoveType(region);
    cost = newreg->MoveCost(movetype, region, x->dir, &road);
    if (x->dir == MOVE_PAUSE) cost = 1;
    if (region->type == R_NEXUS) {
        cost = 1;
        startmove = 1;
    }
    if ((TerrainDefs[region->type].similar_type == R_OCEAN) &&
        (!unit->CanSwim() || (unit->type != U_WMON && unit->GetFlag(FLAG_NOCROSS_WATER)))) {
        unit->error("MOVE: Can't move while in the ocean.");
        goto done_moving;
    }
    weight = unit->items.Weight();
    if ((TerrainDefs[region->type].similar_type == R_OCEAN) && (TerrainDefs[newreg->type].similar_type != R_OCEAN) &&
        !unit->CanWalk(weight) && !unit->CanRide(weight) && !unit->CanFly(weight)) {
        unit->error("Must be able to walk to climb out of the ocean.");
        goto done_moving;
    }
    if (movetype == M_NONE) {
        unit->error("MOVE: Unit is overloaded and cannot move.");
        goto done_moving;
    }

    // If we're moving in the same direction as last month and
    // have stored movement points, then add in those stored
    // movement points, but make sure that these are only used
    // towards entering the hex we were trying to enter
    if (!unit->moved && unit->movepoints >= Globals->MAX_SPEED && unit->movepoints < cost * Globals->MAX_SPEED &&
        x->dir == unit->savedmovedir) {
        while (unit->savedmovement > 0 && unit->movepoints < cost * Globals->MAX_SPEED) {
            unit->movepoints += Globals->MAX_SPEED;
            unit->savedmovement--;
        }
        unit->savedmovement = 0;
        unit->savedmovedir = -1;
    }

    if (unit->movepoints < cost * Globals->MAX_SPEED) return 0;

    if (x->dir == MOVE_PAUSE) {
        unit->event("Pauses to admire the scenery in " + region->short_print() + ".", "movement");
        unit->movepoints -= cost * Globals->MAX_SPEED;
        unit->moved += cost;
        std::erase(o->dirs, x);
        delete x;
        return 0;
    }

    if ((TerrainDefs[newreg->type].similar_type == R_OCEAN) &&
        (!unit->CanSwim() || (unit->type != U_WMON && unit->GetFlag(FLAG_NOCROSS_WATER)))) {
        unit->event("Discovers that " + newreg->short_print() + " is " + TerrainDefs[newreg->type].name + ".", "movement");
        goto done_moving;
    }

    // Check deep ocean restriction for SWIMMERS_COASTAL_ONLY
    // Only apply to units that are actually swimming (not in ships, not flying, not walking)
    if (Globals->SWIMMERS_COASTAL_ONLY &&
        unit->MoveType(newreg) == M_SWIM &&
        !unit->CanSwimTo(newreg)) {
        unit->error("MOVE: Can only swim in coastal waters and lakes.");
        goto done_moving;
    }

    if (unit->type == U_WMON && newreg->town && newreg->IsGuarded()) {
        unit->event("Monsters don't move into guarded towns.", "movement");
        goto done_moving;
    }

    // Pirates landing from ocean to land: non-advancing move is blocked by guards (peaceful landing requires no guards)
    if (unit->type == U_WMON &&
        TerrainDefs[region->type].similar_type == R_OCEAN &&
        TerrainDefs[newreg->type].similar_type != R_OCEAN &&
        !o->advancing) {
        Unit *forbidUnit = newreg->Forbidden(unit);
        if (forbidUnit && !startmove) {
            unit->event("Attempts to land ashore but is repelled by guards.", "movement");
            goto done_moving;
        }
    }

    if (unit->guard == GUARD_ADVANCE) {
        ally = newreg->ForbiddenByAlly(unit);
        if (ally && !startmove) {
            unit->event("Can't ADVANCE: " + newreg->name + " is guarded by " + ally->name + ", an ally.", "movement");
            goto done_moving;
        }
    }

    if (o->advancing) unit->guard = GUARD_ADVANCE;

    forbid = newreg->Forbidden(unit);
    if (forbid && !startmove && unit->guard != GUARD_ADVANCE) {
        int obs = unit->GetAttribute("observation");
        unit->event("Is forbidden entry to " + newreg->short_print() + " by " + forbid->get_name(obs) + ".", "movement");
        obs = forbid->GetAttribute("observation");
        forbid->event(std::string("Forbids entry to ") + unit->get_name(obs) + ".", "guarding");
        goto done_moving;
    }

    if (unit->guard == GUARD_GUARD) unit->guard = GUARD_NONE;

    unit->alias = 0;
    unit->movepoints -= cost * Globals->MAX_SPEED;
    unit->moved += cost;
    unit->MoveUnit(newreg->GetDummy());
    unit->DiscardUnfinishedShips();

    // Track the initial region the unit started from for part of NO7 victory handling
    if (unit->initial_region == nullptr) { unit->initial_region = region; }

    switch (movetype) {
    case M_WALK:
    default: temp = "Walks " + road; break;
    case M_RIDE:
        temp = "Rides " + road;
        unit->Practice(S_RIDING);
        break;
    case M_FLY:
        temp = "Flies ";
        unit->Practice(S_SUMMON_WIND);
        unit->Practice(S_RIDING);
        break;
    case M_SWIM: temp = "Swims "; break;
    }
    unit->event(temp + "from " + region->short_print() + " to " + newreg->short_print() + ".", "movement");

    if (forbid) { unit->advancefrom = region; }

    // TODO: Should we get a transit report on the starting region?
    if (Globals->TRANSIT_REPORT != GameDefs::REPORT_NOTHING) {
        if (!(unit->faction->is_npc)) newreg->visited = 1;
        // Update our visit record in the region we are leaving.
        for (const auto f : region->passers) {
            if (f->unit == unit) {
                // We moved into here this turn
                if (x->dir < MOVE_IN) { f->exits_used[x->dir] = 1; }
            }
        }
        // And mark the hex being entered
        Farsight *f = new Farsight;
        f->faction = unit->faction;
        f->level = 0;
        f->unit = unit;
        if (x->dir < MOVE_IN) { f->exits_used[region->GetRealDirComp(x->dir)] = 1; }
        newreg->passers.push_back(f);
    }

    region = newreg;

    std::erase(o->dirs, x);
    delete x;

    loc = new Location;
    loc->unit = unit;
    loc->region = region;
    loc->obj = nullptr;
    return loc;

done_moving:
    delete o;
    unit->monthorders = nullptr;
    return 0;
}

// Build weighted candidate pool for RMAP: terrain products + food.
// Returns a vector of {item, weight} pairs; empty if region has no products.
static std::vector<std::pair<int,int>> rmap_candidate_pool(ARegion *r)
{
    std::vector<std::pair<int,int>> pool;

    // Nexus has no terrain products — RMAP unusable there.
    if (r->type == R_NEXUS) return pool;

    TerrainType *typer = &TerrainDefs[r->type];

    // Terrain products from TerrainDefs.
    for (unsigned int c = 0; c < sizeof(typer->prods)/sizeof(typer->prods[0]); c++) {
        int item   = typer->prods[c].product;
        int chance = typer->prods[c].chance;
        if (item != -1 && chance > 0 &&
            !(ItemDefs[item].flags & ItemType::DISABLED)) {
            pool.push_back({item, chance});
        }
    }

    // Food: if region has food production, add that food item to the pool.
    // If the terrain can produce food but the region got none at world gen,
    // offer grain (or fish for coastal) so RMAP can introduce it.
    if (typer->economy > 0) {
        int existing_food = -1;
        for (const auto& prod : r->products) {
            if (prod->itemtype == I_GRAIN    ||
                prod->itemtype == I_LIVESTOCK ||
                prod->itemtype == I_FISH) {
                existing_food = prod->itemtype;
                break;
            }
        }
        if (existing_food != -1) {
            pool.push_back({existing_food, 50});
        } else {
            // No food yet — offer grain/livestock each at weight 25 so RMAP
            // can unlock the region's first food source.
            if (!(ItemDefs[I_GRAIN].flags & ItemType::DISABLED))
                pool.push_back({I_GRAIN, 25});
            if (!(ItemDefs[I_LIVESTOCK].flags & ItemType::DISABLED))
                pool.push_back({I_LIVESTOCK, 25});
        }
    }

    return pool;
}

void Game::RunExploreOrders(ARegion *r)
{
    for (const auto obj : r->objects) {
        for (const auto u : obj->units) {
            if (!u->monthorders || u->monthorders->type != O_EXPLORE) continue;

            ExploreOrder *o = static_cast<ExploreOrder *>(u->monthorders);

            if (o->mapitem == I_RESOURCE_MAP) {
                // Consume the RMAP.
                if (u->items.GetNum(I_RESOURCE_MAP) < 1) {
                    u->error("EXPLORE: No resource map to use.");
                    delete u->monthorders;
                    u->monthorders = nullptr;
                    continue;
                }
                u->items.SetNum(I_RESOURCE_MAP,
                    u->items.GetNum(I_RESOURCE_MAP) - 1);

                auto pool = rmap_candidate_pool(r);
                if (pool.empty()) {
                    u->error("EXPLORE: This region has no terrain resources to chart.");
                    delete u->monthorders;
                    u->monthorders = nullptr;
                    continue;
                }

                // Weighted random selection.
                int total_weight = 0;
                for (const auto& [item, w] : pool) total_weight += w;
                int roll = rng::get_random(total_weight);
                int chosen = pool[0].first;
                int acc = 0;
                for (const auto& [item, w] : pool) {
                    acc += w;
                    if (roll < acc) { chosen = item; break; }
                }

                int bonus = 1 + rng::get_random(2);  // 1d2
                r->add_or_increase_product(chosen, bonus);

                u->event("Studying the ancient charts, " +
                    u->name + " discovers new " +
                    item_string(chosen, 2, ALWAYSPLURAL) +
                    " deposits in " + r->short_print() +
                    ", adding to the region's production.", "explore");

            } else if (o->mapitem == I_TREASURE_MAP) {
                if (u->items.GetNum(I_TREASURE_MAP) < 1) {
                    u->error("EXPLORE: No treasure map to use.");
                    delete u->monthorders;
                    u->monthorders = nullptr;
                    continue;
                }

                // Treasure maps lead to coastal pirate hideouts, which spawn only on the
                // surface (find_pirate_hideout_spot searches LEVEL_SURFACE). Block use from
                // the underworld/underdeep with a clear error and do NOT consume the map.
                if (!r->level || r->level->levelType != ARegionArray::LEVEL_SURFACE) {
                    u->error("EXPLORE: Treasure maps can only be used on the surface.");
                    delete u->monthorders;
                    u->monthorders = nullptr;
                    continue;
                }

                // Compass doubles the base 25% success chance to 50%.
                bool has_compass = (u->items.GetNum(I_COMPASS) > 0);
                int chance = has_compass ? 50 : 25;
                bool success = (rng::get_random(100) < chance);

                if (success) {
                    // Decipher succeeded — try to place the hideout BEFORE spending the
                    // map. The treasure map is consumed ONLY if a hideout is actually
                    // created, so a successful roll that fails to place (no coastal spot
                    // in range, all dungeon cells taken, no building slot) never wastes
                    // the map — the player can simply try again next month.
                    bool found = spawn_pirate_hideout(r, u);
                    if (found) {
                        u->items.SetNum(I_TREASURE_MAP,
                            u->items.GetNum(I_TREASURE_MAP) - 1);
                    } else if (r->IsCoastal()) {
                        u->event(u->name + " follows the treasure map's bearings but finds"
                            " no hidden cove this month — the charts still hold. Try again,"
                            " perhaps from a different stretch of coast.",
                            "explore");
                    } else {
                        u->event(u->name + " studies the treasure map but the charts"
                            " describe a coastal hideout that lies beyond reach from here."
                            " Move closer to the sea before attempting to use this map.",
                            "explore");
                    }
                } else {
                    // Failed to decipher. 50% chance map is destroyed.
                    if (rng::get_random(2) == 0) {
                        u->items.SetNum(I_TREASURE_MAP,
                            u->items.GetNum(I_TREASURE_MAP) - 1);
                        u->event(u->name + " failed to decipher the treasure map "
                            "and the salt-stained charts fell apart in the attempt.",
                            "explore");
                    } else {
                        u->event(u->name + " failed to decipher the treasure map "
                            "this month" +
                            std::string(has_compass ? "" : " (a compass might help)") +
                            ".", "explore");
                    }
                }
            }

            delete u->monthorders;
            u->monthorders = nullptr;

        }
    }
}
