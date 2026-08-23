#include "game.h"
#include "gamedata.h"
#include "namegen.h"
#include "quests.h"
#include "rng.hpp"
#include <numeric>
#include <limits>
#include <map>

void Game::CreateCityMons()
{
    if (!Globals->CITY_MONSTERS_EXIST) return;

    for(const auto r : regions) {
        if ((r->type == R_NEXUS) || r->IsStartingCity() || r->town) {
            CreateCityMon(r, 100, 1);
        }
    }
}

void Game::CreateWMons()
{
    if (!Globals->WANDERING_MONSTERS_EXIST) return;

    GrowWMons(50);
}

void Game::CreateLMons()
{
    if (!Globals->LAIR_MONSTERS_EXIST) return;

    GrowLMons(50);
}

void Game::GrowWMons(int rate)
{
    //
    // Now, go through each 8x8 block of the map, and make monsters if
    // needed.
    //
    int level;
    for (level = 0; level < regions.numLevels; level++) {
        ARegionArray *pArr = regions.pRegionArrays[level];

        for (int xsec=0; xsec < pArr->x; xsec+=8) {
            for (int ysec=0; ysec < pArr->y; ysec+=16) {
                int mons=0;
                int wanted=0;

                for (int x=0; x < 8; x++) {
                    if (x+xsec > pArr->x) break;

                    for (int y=0; y < 16; y+=2) {
                        if (y+ysec > pArr->y) break;

                        ARegion *reg = pArr->GetRegion(x+xsec, y+ysec+x%2);
                        if (reg && reg->zloc == level && (!reg->IsGuarded() || reg->HasLair())) {
                            mons += reg->CountWMons();
                            /*
                             * Make sure there is at least one monster type
                             * enabled for this region
                             */
                            int avail = 0;
                            int mon = TerrainDefs[reg->type].smallmon;
                            if (!((mon == -1) ||
                                 (ItemDefs[mon].flags & ItemType::DISABLED)))
                                avail = 1;
                            mon = TerrainDefs[reg->type].bigmon;
                            if (!((mon == -1) ||
                                 (ItemDefs[mon].flags & ItemType::DISABLED)))
                                avail = 1;
                            mon = TerrainDefs[reg->type].humanoid;
                            if (!((mon == -1) ||
                                 (ItemDefs[mon].flags & ItemType::DISABLED)))
                                avail = 1;

                            if (avail) {
                                wanted += TerrainDefs[reg->type].wmonfreq;
                            }
                        }
                    }
                }

                wanted /= 10;
                wanted -= mons;
                wanted = (wanted*rate + rng::get_random(100))/100;

                // printf("\n\n WANTED WMON at (xsec: %d, ysec: %d) : %d \n\n", xsec, ysec, wanted);

                if (wanted > 0) {
                    // TODO: instead of loop guard need to check how many available regions
                    // are there and random them
                    int loop_guard = 1000;
                    for (int i=0; i < wanted;) {
                        int x = rng::get_random(8);
                        int y = rng::get_random(16);
                        if (y%2 == 1) {
                            if (y > 0) {
                                y -= 1;
                            } else {
                                y = 0;
                            }
                        }

                        ARegion *reg = pArr->GetRegion(x + xsec, y + ysec + x%2);

                        // Can place wandering monster if:
                        // - Region is not guarded, OR
                        // - Region has a lair (lairs attract monsters regardless of guards)
                        if (reg && reg->zloc == level && (!reg->IsGuarded() || reg->HasLair()) && MakeWMon(reg)) {
                            i++;
                        }

                        // In worst case scenario it will randomly pick same not matching regions
                        // with potential of infinitie loop (ie dodgy RNG)
                        loop_guard--;
                        if (loop_guard == 0) break;
                    }
                }
            }
        }
    }
}

void Game::GrowLMons(int rate)
{
    for(const auto r : regions) {
        //
        // Don't make lmons in guarded regions
        //
        if (r->IsGuarded()) continue;

        for(const auto obj : r->objects) {
            if (obj->units.size()) continue;
            int montype = ObjectDefs[obj->type].monster;
            int grow=!(ObjectDefs[obj->type].flags&ObjectType::NOMONSTERGROWTH);
            if ((montype != -1) && grow) {
                if (rng::get_random(100) < rate) {
                    MakeLMon(obj);
                }
            }
        }
    }
}

int Game::MakeWMon(ARegion *pReg)
{
    if (!Globals->WANDERING_MONSTERS_EXIST) return 0;

    if (TerrainDefs[pReg->type].wmonfreq == 0) return 0;

    int montype = TerrainDefs[pReg->type].smallmon;
    if (rng::get_random(2) && (TerrainDefs[pReg->type].humanoid != -1))
        montype = TerrainDefs[pReg->type].humanoid;
    if (TerrainDefs[pReg->type].bigmon != -1 && !rng::get_random(8)) {
        montype = TerrainDefs[pReg->type].bigmon;
    }
    if ((montype == -1) || (ItemDefs[montype].flags & ItemType::DISABLED))
        return 0;

    // Pirates on ocean: spawn as a fleet with optional captain+bosun
    if (montype == I_PIRATES)
        return MakePirateFleet(pReg);

    auto monster = find_monster(ItemDefs[montype].abr, (ItemDefs[montype].type & IT_ILLUSION))->get();
    Faction *monfac = GetFaction(factions, monfaction);
    Unit *u = GetNewUnit(monfac, 0);
    u->MakeWMon(monster.name.c_str(), montype, (monster.number+rng::get_random(monster.number)+1)/2);
    u->MoveUnit(pReg->GetDummy());
    u->free = Globals->MONSTER_SPOILS_RECOVERY;
    u->UpdateMonsterDescription();
    return(1);
}

void Game::MakeLMon(Object *pObj)
{
    if (!Globals->LAIR_MONSTERS_EXIST) return;
    if (ObjectDefs[pObj->type].flags & ObjectType::NOMONSTERGROWTH) return;

    int montype = ObjectDefs[pObj->type].monster;

    if (montype == I_TRENT)
        montype = TerrainDefs[pObj->region->type].bigmon;

    if (montype == I_CENTAUR)
        montype = TerrainDefs[pObj->region->type].humanoid;

    if ((montype == -1) || (ItemDefs[montype].flags & ItemType::DISABLED))
        return;

    // Pirates in a lair: spawn pirate unit(s) without a fleet
    if (montype == I_PIRATES) { MakePirateLair(pObj); return; }

    auto monster = find_monster(ItemDefs[montype].abr, (ItemDefs[montype].type & IT_ILLUSION))->get();
    Faction *monfac = GetFaction(factions, monfaction);
    Unit *u = GetNewUnit(monfac, 0);
    switch(montype) {
        case I_IMP:
            u->MakeWMon("Demons", I_IMP, rng::get_random(monster.number + 1));

            monster = find_monster(ItemDefs[I_DEMON].abr, (ItemDefs[I_DEMON].type & IT_ILLUSION))->get();
            u->items.SetNum(I_DEMON, rng::get_random(monster.number + 1));

            monster = find_monster(ItemDefs[I_DEVIL].abr, (ItemDefs[I_DEVIL].type & IT_ILLUSION))->get();
            u->items.SetNum(I_DEVIL, rng::get_random(monster.number + 1));
            break;
        case I_SKELETON:
            u->MakeWMon("Undead", I_SKELETON, rng::get_random(monster.number + 1));

            monster = find_monster(ItemDefs[I_UNDEAD].abr, (ItemDefs[I_UNDEAD].type & IT_ILLUSION))->get();
            u->items.SetNum(I_UNDEAD, rng::get_random(monster.number + 1));

            monster = find_monster(ItemDefs[I_LICH].abr, (ItemDefs[I_LICH].type & IT_ILLUSION))->get();
            u->items.SetNum(I_LICH, rng::get_random(monster.number + 1));
            break;
        case I_MAGICIANS:
            u->MakeWMon("Evil Mages", I_MAGICIANS, (monster.number + rng::get_random(monster.number) + 1) / 2);

            monster = find_monster(ItemDefs[I_SORCERERS].abr, (ItemDefs[I_SORCERERS].type & IT_ILLUSION))->get();
            u->items.SetNum(I_SORCERERS, rng::get_random(monster.number + 1));
            u->SetFlag(FLAG_BEHIND, 1);
            u->guard = GUARD_NONE;
            u->MoveUnit(pObj);
            u->free = Globals->MONSTER_SPOILS_RECOVERY;
            u->UpdateMonsterDescription();

            u = GetNewUnit(monfac, 0);

            monster = find_monster(ItemDefs[I_WARRIORS].abr, (ItemDefs[I_WARRIORS].type & IT_ILLUSION))->get();
            u->MakeWMon(monster.name.c_str(), I_WARRIORS, (monster.number + rng::get_random(monster.number) + 1) / 2);
            u->guard = GUARD_NONE;

            break;
        case I_DARKMAGE:
            u->MakeWMon("Dark Mages", I_DARKMAGE, (rng::get_random(monster.number) + 1));

            monster = find_monster(ItemDefs[I_MAGICIANS].abr, (ItemDefs[I_MAGICIANS].type & IT_ILLUSION))->get();
            u->items.SetNum(I_MAGICIANS, (monster.number + rng::get_random(monster.number) + 1) / 2);

            monster = find_monster(ItemDefs[I_SORCERERS].abr, (ItemDefs[I_SORCERERS].type & IT_ILLUSION))->get();
            u->items.SetNum(I_SORCERERS, rng::get_random(monster.number + 1));

            monster = find_monster(ItemDefs[I_DARKMAGE].abr, (ItemDefs[I_DARKMAGE].type & IT_ILLUSION))->get();
            u->items.SetNum(I_DARKMAGE, rng::get_random(monster.number + 1));
            u->SetFlag(FLAG_BEHIND, 1);
            u->guard = GUARD_NONE;
            u->MoveUnit(pObj);
            u->free = Globals->MONSTER_SPOILS_RECOVERY;
            u->UpdateMonsterDescription();

            u = GetNewUnit(monfac, 0);

            monster = find_monster(ItemDefs[I_DROW].abr, (ItemDefs[I_DROW].type & IT_ILLUSION))->get();
            u->MakeWMon(monster.name.c_str(), I_DROW, (monster.number + rng::get_random(monster.number) + 1) / 2);
            u->guard = GUARD_NONE;

            break;
        case I_ILLYRTHID:
            u->MakeWMon(monster.name.c_str(), I_ILLYRTHID, (monster.number + rng::get_random(monster.number) + 1) / 2);
            u->SetFlag(FLAG_BEHIND, 1);
            u->guard = GUARD_NONE;
            u->MoveUnit(pObj);
            u->free = Globals->MONSTER_SPOILS_RECOVERY;
            u->UpdateMonsterDescription();

            u = GetNewUnit(monfac, 0);

            monster = find_monster(ItemDefs[I_SKELETON].abr, (ItemDefs[I_SKELETON].type & IT_ILLUSION))->get();
            u->MakeWMon("Undead", I_SKELETON, rng::get_random(monster.number + 1));

            monster = find_monster(ItemDefs[I_UNDEAD].abr, (ItemDefs[I_UNDEAD].type & IT_ILLUSION))->get();
            u->items.SetNum(I_UNDEAD, rng::get_random(monster.number + 1));
            u->guard = GUARD_NONE;
            break;
        case I_STORMGIANT:
            if (rng::get_random(3) < 1) {
                montype = I_CLOUDGIANT;
                monster = find_monster(ItemDefs[montype].abr, (ItemDefs[montype].type & IT_ILLUSION))->get();
            }
            u->MakeWMon(monster.name.c_str(), montype, (monster.number + rng::get_random(monster.number) + 1) / 2);
            break;
        default:
            u->MakeWMon(monster.name.c_str(), montype, (monster.number + rng::get_random(monster.number) + 1) / 2);
            break;
    }
    u->MoveUnit(pObj);
    u->free = Globals->MONSTER_SPOILS_RECOVERY;
    u->UpdateMonsterDescription();
}

/**
 * @brief True if `r` holds a settlement of at least `min_tier` guarded by a player.
 *
 * NPC guards are ignored on purpose: they never attack pirates (Faction's default
 * attitude is NEUTRAL and nothing makes guardfaction hostile to monfaction), so they
 * pose no threat and deter nothing.
 *
 * @param r        region to test; a null region is never guarded
 * @param min_tier TOWN_TOWN or TOWN_CITY (tiers are ordered, see aregion.h)
 * @return true if a non-guardfaction unit stands on GUARD_GUARD there
 * @note The guard faction is identified by number 1. See the known-wart note in
 *       docs/PIRATE_FLEET_SYSTEM.md - is_npc would be the correct test.
 */
static bool player_guarded_at_least(const ARegion *r, int min_tier)
{
    if (!r || !r->town || r->town->TownType() < min_tier) return false;
    for (const auto *o : r->objects)
        for (const auto *u : o->units)
            if (u->guard == GUARD_GUARD && u->faction->num != 1) return true;
    return false;
}

/**
 * @brief Hex rule: a player-guarded town or city is refused outright.
 *
 * Villages are never refused - they hold too little player force to matter.
 *
 * @param r candidate destination region
 * @return true if a pirate fleet must not enter
 * @see pirate_avoids_city_ring
 */
bool pirate_avoids_settlement(const ARegion *r)
{
    return player_guarded_at_least(r, TOWN_TOWN);
}

/**
 * @brief Ring rule: a land region next to a player-guarded city is refused too.
 *
 * Only a city projects force onto the land around it; a town holds its own hex only.
 * Water is never refused at any tier, which is also what makes trapping impossible:
 * land->land moves are forbidden for fleets, so water is the only way off land.
 *
 * @param r candidate destination region
 * @return true if a pirate fleet must not stop here
 * @see pirate_avoids_settlement
 */
bool pirate_avoids_city_ring(const ARegion *r)
{
    if (!r) return false;
    // R_LAKE has similar_type == R_OCEAN, so this covers lakes too.
    if (TerrainDefs[r->type].similar_type == R_OCEAN) return false;
    for (int d = 0; d < NDIRS; d++)
        if (player_guarded_at_least(r->neighbors[d], TOWN_CITY)) return true;
    return false;
}

/**
 * @brief Spawns a pirate fleet in an ocean region.
 *
 * Regular: Cog + crew of pirates.
 * Elite:   Galley  + crew (×3 pirates) + 1 captain + 1 bosun (both FLAG_BEHIND).
 *
 * The fleet object is named via getPirateShipName(); boss units get personal names
 * via getPirateName(). Rank prefixes are added in GetMonsterDisplayName().
 *
 * @param pReg Ocean region where the fleet is created
 * @return 1 on success
 */
int Game::MakePirateFleet(ARegion *pReg)
{
    auto pmon = find_monster(ItemDefs[I_PIRATES].abr, false)->get();
    Faction *monfac = GetFaction(factions, monfaction);
    bool elite = (rng::get_random(10) == 0);  // 10% chance of elite pirate captain

    int pira_count = (pmon.number + rng::get_random(pmon.number) + 1) / 2;
    if (elite) pira_count *= 3;

    // Build the fleet object
    Object *fleet = new Object(pReg);
    fleet->type = O_FLEET;
    fleet->num = shipseq++;
    fleet->set_name(getPirateShipName());
    fleet->AddShip(elite ? I_GALLEY : I_COG);
    pReg->objects.push_back(fleet);
    fleet->FleetCapacity();

    // Main pirate crew
    Unit *u = GetNewUnit(monfac, 0);
    u->MakeWMon("Pirates", I_PIRATES, pira_count);
    u->free = Globals->MONSTER_SPOILS_RECOVERY;
    u->MoveUnit(fleet);
    u->UpdateMonsterDescription();

    if (elite) {
        // Pirate captain (BEHIND_CAPABLE, sets FLAG_BEHIND)
        Unit *cap = GetNewUnit(monfac, 0);
        cap->MakeWMon(getPirateName().c_str(), I_PIRATE_CAPTAIN, 1);
        cap->SetFlag(FLAG_BEHIND, 1);
        cap->free = Globals->MONSTER_SPOILS_RECOVERY;
        cap->MoveUnit(fleet);
        cap->UpdateMonsterDescription();

        // Pirate bosun (BEHIND_CAPABLE, sets FLAG_BEHIND)
        Unit *bos = GetNewUnit(monfac, 0);
        bos->MakeWMon(getPirateName().c_str(), I_PIRATE_BOSUN, 1);
        bos->SetFlag(FLAG_BEHIND, 1);
        bos->free = Globals->MONSTER_SPOILS_RECOVERY;
        bos->MoveUnit(fleet);
        bos->UpdateMonsterDescription();

        logger::write("MakePirateFleet: ELITE fleet '" + fleet->name + "' at (" +
            std::to_string(pReg->xloc) + "," + std::to_string(pReg->yloc) + ") — " +
            std::to_string(pira_count) + " pirates + captain '" + cap->name +
            "' + bosun '" + bos->name + "'");

        // Create a HUNT_PIRATE quest for this captain (unconditional).
        TryCreatePirateHuntQuest(cap);
    } else {
        logger::write("MakePirateFleet: fleet '" + fleet->name + "' at (" +
            std::to_string(pReg->xloc) + "," + std::to_string(pReg->yloc) + ") — " +
            std::to_string(pira_count) + " pirates");
    }

    return 1;
}

/**
 * @brief Spawns pirates in a lair object (no fleet).
 *
 * 80%: crew of pirates only.
 * 20%: crew (×1.5 pirates) + 1 bosun (FLAG_BEHIND).
 *
 * @param pObj Lair object (e.g. O_ISLE) where pirates are placed
 */
void Game::MakePirateLair(Object *pObj)
{
    auto pmon = find_monster(ItemDefs[I_PIRATES].abr, false)->get();
    Faction *monfac = GetFaction(factions, monfaction);
    // Lakes spawn only plain pirates — no bosun
    bool has_bosun = (pObj->region->type != R_LAKE) && (rng::get_random(5) == 0);

    int pira_count = (pmon.number + rng::get_random(pmon.number) + 1) / 2;
    if (has_bosun) pira_count = (pira_count * 3) / 2;

    // Main pirate crew
    Unit *u = GetNewUnit(monfac, 0);
    u->MakeWMon("Pirates", I_PIRATES, pira_count);
    u->free = Globals->MONSTER_SPOILS_RECOVERY;
    u->MoveUnit(pObj);
    u->UpdateMonsterDescription();

    if (has_bosun) {
        // Pirate bosun (BEHIND_CAPABLE, sets FLAG_BEHIND)
        Unit *bos = GetNewUnit(monfac, 0);
        bos->MakeWMon(getPirateName().c_str(), I_PIRATE_BOSUN, 1);
        bos->SetFlag(FLAG_BEHIND, 1);
        bos->free = Globals->MONSTER_SPOILS_RECOVERY;
        bos->MoveUnit(pObj);
        bos->UpdateMonsterDescription();

        logger::write("MakePirateLair: lair '" + pObj->name + "' at (" +
            std::to_string(pObj->region->xloc) + "," + std::to_string(pObj->region->yloc) + ") — " +
            std::to_string(pira_count) + " pirates + bosun '" + bos->name + "'");
    } else {
        logger::write("MakePirateLair: lair '" + pObj->name + "' at (" +
            std::to_string(pObj->region->xloc) + "," + std::to_string(pObj->region->yloc) + ") — " +
            std::to_string(pira_count) + " pirates");
    }
}

// Helper struct for weapon selection
struct SuitableWeapon {
    int index; // Index into WeaponDefs
    unsigned int weight;
};

Unit *Game::MakeManUnit(Faction *fac, int mantype, int num, int level, int weaponlevel, int armor, int behind)
{
    Unit *u = GetNewUnit(fac);
    auto men = find_race(ItemDefs[mantype].abr)->get();

    int scomb = men.defaultlevel;
    int sxbow = men.defaultlevel;
    int slbow = men.defaultlevel;
    for (unsigned int i = 0; i < (sizeof(men.skills) / sizeof(men.skills[0])); i++) {
        if (!men.skills[i]) continue;
        auto pS = FindSkill(men.skills[i]->c_str())->get();
        if (pS == FindSkill("COMB")->get()) scomb = men.speciallevel;
        if (pS == FindSkill("XBOW")->get()) sxbow = men.speciallevel;
        if (pS == FindSkill("LBOW")->get()) slbow = men.speciallevel;
    }

    int combat_level = scomb;
    int sk = lookup_skill("COMB");
    if (behind) {
        if (slbow >= sxbow) {
            sk = lookup_skill("LBOW");
            combat_level = slbow;
        } else {
            sk = lookup_skill("XBOW");
            combat_level = sxbow;
        }
    }

    if (combat_level < level) {
        weaponlevel += level - combat_level;
    }

    int weapon_index = -1;
    int witem = -1;

    while (weapon_index == -1) {
        std::vector<SuitableWeapon> suitable_weapons;

        for (size_t i = 0; i < WeaponDefs.size(); ++i) {
            int current_witem = lookup_item(WeaponDefs[i].abbr);

            if (ItemDefs[current_witem].flags & ItemType::DISABLED) continue;
            if (current_witem == lookup_item("PICK")) continue;
            if (ItemDefs[current_witem].pSkill != FindSkill("WEAP")->get().abbr) continue;

            bool is_ranged = (WeaponDefs[i].flags & WeaponType::RANGED);
            if (is_ranged && !behind) continue;

            int weapon_base_skill_idx = lookup_skill(WeaponDefs[i].baseSkill);
            int weapon_or_skill_idx = lookup_skill(WeaponDefs[i].orSkill);
            bool skill_match = (weapon_base_skill_idx == sk || weapon_or_skill_idx == sk);

            bool javelin_case = false;
            if (behind && !skill_match && scomb > combat_level) {
                if (is_ranged && (weapon_base_skill_idx == lookup_skill("COMB") || weapon_or_skill_idx == lookup_skill("COMB"))) {
                    skill_match = true;
                    javelin_case = true;
                }
            }

            if (!skill_match) continue;

            int attack = WeaponDefs[i].attackBonus;
            int producelevel = ItemDefs[current_witem].pLevel;
            if (attack < (producelevel - 1)) attack = producelevel - 1;

            bool level_match = false;
            unsigned int weight = 1;
            if (behind) {
                if (attack + (javelin_case ? scomb : combat_level) <= weaponlevel) {
                    level_match = true;
                    if (WeaponDefs[i].attackBonus == weaponlevel) {
                        weight = 5;
                    }
                }
            } else {
                if (attack == weaponlevel) {
                    level_match = true;
                }
            }

            if (!level_match) continue;

            if (!men.CanUse(current_witem)) continue;

            suitable_weapons.push_back({static_cast<int>(i), weight});
        }

        if (suitable_weapons.empty()) {
            weaponlevel++;
            continue;
        }

        std::vector<unsigned int> weights;
        weights.reserve(suitable_weapons.size());
        for (const auto& sw : suitable_weapons) {
            weights.push_back(sw.weight);
        }

        std::optional<size_t> selected_suitable_index_opt = rng::get_weighted_index(weights);

        // If the weighted selection failed, we know that suitable_weapons is not empty, so we can safely pick the first one.
        if (selected_suitable_index_opt) weapon_index = suitable_weapons[*selected_suitable_index_opt].index;
        else weapon_index = suitable_weapons[0].index;
        witem = lookup_item(WeaponDefs[weapon_index].abbr);
    }

    int final_skill_idx = lookup_skill(WeaponDefs[weapon_index].baseSkill);
    if (final_skill_idx != sk && lookup_skill(WeaponDefs[weapon_index].orSkill) != sk) sk = final_skill_idx;

    int maxskill = men.defaultlevel;
    for (unsigned int i = 0; i < (sizeof(men.skills) / sizeof(men.skills[0])); i++) {
        if (men.skills[i] && FindSkill(men.skills[i]->c_str())->get() == FindSkill(SkillDefs[sk].abbr.c_str())->get()) {
            maxskill = men.speciallevel;
            break;
        }
    }

    if (level > maxskill) level = maxskill;

    u->SetMen(mantype, num);
    u->items.SetNum(witem, num);
    u->SetSkill(sk, level);
    if (behind) u->SetFlag(FLAG_BEHIND, 1);

    if (armor) {
        int ar = I_PLATEARMOR;
        if (!men.CanUse(ar)) ar = I_CHAINARMOR;
        if (!men.CanUse(ar)) ar = I_LEATHERARMOR;
        if (men.CanUse(ar)) u->items.SetNum(ar, num);
    }

    return u;
}


/**
 * @brief True when any unit aboard the fleet object holds the captain item.
 *
 * An elite pirate fleet keeps its captain (I_PIRATE_CAPTAIN) in a unit of his
 * own aboard the fleet object, not in the crew unit, so the crew unit can never
 * answer this question - the whole fleet object must be asked.
 *
 * @param fleet the fleet object whose units are checked
 * @return true if some unit aboard holds I_PIRATE_CAPTAIN
 */
bool fleet_has_captain(Object *fleet)
{
    for (const auto u : fleet->units)
        if (u->items.GetNum(I_PIRATE_CAPTAIN) > 0) return true;
    return false;
}

void Game::PirateRecruitLandCrew()
{
    // Ruleset tuning, read once for the whole pass. The defaults keep an
    // unconfigured ruleset sane: the intake ceiling then sits exactly on the
    // ship's sailor requirement, and a kept hand costs the region one person,
    // the same as a player's recruiter does.
    int intake_up = std::max(1, rulesetSpecificData.value("pirate_recruit_intake_up", 1));
    int intake_down = std::max(1, rulesetSpecificData.value("pirate_recruit_intake_down", 1));
    int pop_cost = std::max(1, rulesetSpecificData.value("pirate_recruit_pop_cost", 1));
    int offshore_pct = std::max(0, rulesetSpecificData.value("pirate_recruit_offshore_pct", 50));

    // Stage 1: what each land hex gives up in a month is what a recruiter could
    // hire out of it: one market unit per MEN_PER_MARKET_UNIT people. Built up
    // front, keyed by region, so the docked and offshore passes drain the same
    // per-hex pool - a stack of ships cannot multiply what a village loses.
    std::map<ARegion *, int> allowance;
    for (const auto r : regions) {
        if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
        if (TerrainDefs[r->type].similar_type == R_LAKE) continue;
        allowance[r] = r->Population() / MEN_PER_MARKET_UNIT;
    }

    // Signs one hull's crew on out of `target`'s pool. `offshore` halves the
    // intake and drops a single hand, so offshore work is a mature fleet's
    // bonus. The crew still comes off the land, hence the function's name.
    auto press = [&](Object *obj, Unit *u, ARegion *target, int &pool, bool offshore) {
        int current = u->items.GetNum(I_PIRATES);
        if (current <= 0) return;

        int pirate_w = ItemDefs[I_PIRATES].weight;
        int cap = (pirate_w > 0) ? obj->capacity / pirate_w : 0;
        if (cap <= 0) return;
        if (current >= cap) {
            logger::write("PirateRecruitLandCrew: fleet \"" + obj->name + "\""
                + " at " + obj->region->short_print()
                + " - at cap (" + std::to_string(current) + "/" + std::to_string(cap)
                + "), no recruitment");
            return;
        }

        // What the crew itself goes looking for.
        int pct = 15 + rng::get_random(11);  // 15-25%
        int demand = std::max(1, current * pct / 100);

        // What the deck can absorb: a ship signs on about as many hands a
        // month as it needs to sail her, give or take the ruleset's spread.
        int ceiling = std::max(1, obj->GetFleetSize()
            + rng::get_random(intake_up) - rng::get_random(intake_down));

        // The hex pool is counted in people; a kept hand costs pop_cost of them.
        int supply = pool / pop_cost;

        int gained = std::min(std::min(demand, cap - current),
                              std::min(ceiling, supply));
        if (gained < 1) return;

        if (offshore) {
            gained = gained * offshore_pct / 100;
            if (gained < 1) return;
        }

        int taken = gained * pop_cost;
        pool -= taken;
        target->Recruit(taken);   // the call a player's men purchase goes through
        u->items.SetNum(I_PIRATES, current + gained);

        logger::write("PirateRecruitLandCrew: fleet \"" + obj->name + "\""
            + " at " + obj->region->short_print()
            + " - pressed " + std::to_string(gained) + " pirates"
            + (offshore ? std::string(" off the coast of ") + target->name : std::string())
            + " (" + std::to_string(pct) + "%, demand " + std::to_string(demand)
            + ", ceiling " + std::to_string(ceiling)
            + ", cost " + std::to_string(taken) + " people"
            + ", hex left " + std::to_string(pool)
            + ", was " + std::to_string(current)
            + ", now " + std::to_string(current + gained) + ")");

        // Notify factions present in the target hex. The press gang carries off
        // more people than it keeps; the rest never come home either.
        std::string msg = "Pirates from " + obj->name + " recruited " + std::to_string(gained)
            + " new crew members out of " + std::to_string(taken)
            + " willing hands "
            + (offshore ? std::string("off the coast of ") + target->name + "."
                        : std::string("while docked in ") + target->short_print() + ".");
        std::set<Faction *> presentFactions = target->PresentFactions();
        for (const auto f : presentFactions) {
            f->event(msg, "monster", target, u);
        }

        // Collect for AI gazette context (pirate_context in times.json).
        bool is_elite = fleet_has_captain(obj);
        std::string ctx = (is_elite ? obj->name : "A pirate fleet")
            + " recruited crew "
            + (offshore ? std::string("off the coast of ") + target->name
                        : std::string("in ") + target->name)
            + ".";
        if (is_elite)
            pirate_context_elite.push_back(ctx);
        else
            pirate_context_regular.push_back(ctx);
    };

    // Stage 2: docked fleets, exactly as before. A fleet that took the risk of
    // landing is served before one that stands offshore.
    for (const auto r : regions) {
        if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
        if (TerrainDefs[r->type].similar_type == R_LAKE) continue;

        for (const auto obj : r->objects) {
            if (!obj->IsFleet()) continue;

            for (const auto u : obj->units) {
                if (!u->faction->is_npc) continue;
                if (u->items.GetNum(I_PIRATES) <= 0) continue;
                press(obj, u, r, allowance[r], false);
                if (allowance[r] < pop_cost) break;
            }
            if (allowance[r] < pop_cost) break;
        }
    }

    // Stage 3: offshore fleets. Each water fleet works the land neighbour with
    // the greatest remaining allowance (ties: lowest direction index); a fleet
    // with no land neighbour does nothing. offshore_pct == 0 skips this stage.
    if (offshore_pct > 0) {
        for (const auto r : regions) {
            if (TerrainDefs[r->type].similar_type != R_OCEAN
                && TerrainDefs[r->type].similar_type != R_LAKE) continue;

            for (const auto obj : r->objects) {
                if (!obj->IsFleet()) continue;

                for (const auto u : obj->units) {
                    if (!u->faction->is_npc) continue;
                    if (u->items.GetNum(I_PIRATES) <= 0) continue;

                    ARegion *best = nullptr;
                    int best_allowance = -1;
                    for (int d = 0; d < NDIRS; d++) {
                        ARegion *nb = r->neighbors[d];
                        if (!nb) continue;
                        if (TerrainDefs[nb->type].similar_type == R_OCEAN) continue;
                        if (TerrainDefs[nb->type].similar_type == R_LAKE) continue;
                        int a = allowance[nb];
                        if (a > best_allowance) {
                            best_allowance = a;
                            best = nb;
                        }
                    }
                    if (!best) continue;

                    press(obj, u, best, allowance[best], true);
                }
            }
        }
    }
}

/**
 * @brief Pirates merge abandoned ships into their own fleets.
 *
 * For each pirate fleet docked in a non-ocean, non-lake region, looks for
 * empty fleets (no units) in the same region and merges one eligible ship
 * per turn into the pirate's own fleet object - no crew split, no prize
 * crew, no rename. A ship is eligible when it is a sea ship (IT_SHIP,
 * fly == 0, swim > 0) and, unless allow_slower, not slower than the fleet
 * (the same min-speed rule the NPC branch of Do1SailOrder uses). An
 * armoured hull bypasses the fill gate for a fleet that has none; otherwise
 * a crowded fleet takes the biggest hull. A source emptied by the merge is
 * deleted.
 *
 * Called once per turn after PirateRecruitLandCrew(), before movement.
 */
void Game::PirateSeizeEmptyShips()
{
    // Ruleset tuning, read once for the whole pass. min_crew, fill_pct and
    // max_per_turn floor at 0, so a ruleset that sets max_per_turn to 0
    // disables seizure entirely - the per-turn loop never runs. allow_slower
    // and offshore are read raw: any nonzero value enables them.
    int min_crew = std::max(0, rulesetSpecificData.value("pirate_seize_min_crew", 20));
    int fill_pct = std::max(0, rulesetSpecificData.value("pirate_seize_fill_pct", 50));
    int max_per_turn = std::max(0, rulesetSpecificData.value("pirate_seize_max_per_turn", 1));
    int allow_slower = rulesetSpecificData.value("pirate_seize_allow_slower", 0);
    int offshore_enabled = rulesetSpecificData.value("pirate_seize_offshore", 1);

    // A ship type's hull protection (ObjectDefs protect), guarded against a
    // negative lookup: an unknown item simply shelters nobody.
    auto ship_protect = [](int t) -> int {
        int obid = lookup_object(ItemDefs[t].name);
        return (obid >= 0) ? ObjectDefs[obid].protect : 0;
    };

    // One pirate fleet absorbs up to max_per_turn eligible ships out of the
    // given empty fleets, ranking them by the armour exception first and the
    // fill gate second. `offshore` changes only the message: the event goes to
    // the factions in the land hex the ship was taken from, off the coast.
    auto seize_into = [&](Object *pobj, std::vector<Object *> &empty_fleets, bool offshore) {
        // The pirate fleet's crew: the first NPC unit aboard with enough hands.
        Unit *pirate_unit = nullptr;
        for (const auto u : pobj->units) {
            if (!u->faction->is_npc) continue;
            if (u->items.GetNum(I_PIRATES) >= min_crew) { pirate_unit = u; break; }
        }
        if (!pirate_unit) return;

        for (int taken = 0; taken < max_per_turn; taken++) {
            // Fleet state, recomputed each iteration: a merge changes the hull
            // count and therefore both the speed floor and the crew capacity.
            int fleet_min_speed = Globals->MAX_SPEED;
            int fleet_max_protect = 0;
            for (int item = 0; item < NITEMS; item++) {
                if (pobj->GetNumShips(item) <= 0) continue;
                if (ItemDefs[item].speed < fleet_min_speed)
                    fleet_min_speed = ItemDefs[item].speed;
                int p = ship_protect(item);
                if (p > fleet_max_protect) fleet_max_protect = p;
            }

            int crew = pirate_unit->items.GetNum(I_PIRATES);
            int pirate_w = ItemDefs[I_PIRATES].weight;
            int crew_cap = (pirate_w > 0) ? pobj->capacity / pirate_w : 0;
            bool crowded = crew * 100 >= fill_pct * crew_cap;

            // One pass over the candidates, keeping the best armoured hull and
            // the best hull by size; the mode below picks which one is taken.
            Object *armor_src = nullptr, *size_src = nullptr;
            int armor_type = -1, size_type = -1;
            for (const auto src : empty_fleets) {
                if (!src) continue;  // emptied and deleted earlier this turn
                for (const auto ship : src->ships) {
                    int t = ship->type;
                    if (ship->num <= 0) continue;
                    if (!(ItemDefs[t].type & IT_SHIP)) continue;
                    if (ItemDefs[t].fly != 0 || ItemDefs[t].swim <= 0) continue;
                    if (!allow_slower && ItemDefs[t].speed < fleet_min_speed) continue;

                    int p = ship_protect(t);
                    if (p > 0) {
                        if (armor_type < 0
                            || p > ship_protect(armor_type)
                            || (p == ship_protect(armor_type)
                                && ItemDefs[t].swim > ItemDefs[armor_type].swim)) {
                            armor_type = t;
                            armor_src = src;
                        }
                    }
                    if (size_type < 0
                        || ItemDefs[t].swim > ItemDefs[size_type].swim
                        || (ItemDefs[t].swim == ItemDefs[size_type].swim
                            && (ItemDefs[t].speed > ItemDefs[size_type].speed
                                || (ItemDefs[t].speed == ItemDefs[size_type].speed
                                    && ItemDefs[t].weight < ItemDefs[size_type].weight)))) {
                        size_type = t;
                        size_src = src;
                    }
                }
            }

            Object *best_src = nullptr;
            int best_type = -1;
            if (fleet_max_protect == 0 && armor_type >= 0) {
                best_src = armor_src;  // armour exception bypasses the fill gate
                best_type = armor_type;
            } else if (crowded && size_type >= 0) {
                best_src = size_src;   // crowded: take the biggest hull
                best_type = size_type;
            } else {
                break;  // nothing eligible this turn
            }

            // Merge the ship into the pirate's own fleet, exactly the ship
            // transfer pattern: decrement the source, increment the fleet, and
            // delete the source once it is emptied. The fleet object stays where
            // it is - docked on land, or standing in the water.
            std::string src_name = best_src->name;
            ARegion *src_region = best_src->region;
            best_src->SetNumShips(best_type, best_src->GetNumShips(best_type) - 1);
            pobj->SetNumShips(best_type, pobj->GetNumShips(best_type) + 1);

            logger::write("PirateSeizeEmptyShips: \"" + pobj->name + "\""
                + " at " + pobj->region->short_print()
                + " merged an abandoned " + ItemDefs[best_type].name
                + " from \"" + src_name + "\" into its fleet"
                + " - crew " + std::to_string(crew)
                + ", capacity now " + std::to_string(pobj->capacity));

            if (best_src->GetFleetSize() == 0) {
                src_region->objects.remove(best_src);
                for (auto &e : empty_fleets) if (e == best_src) e = nullptr;
                delete best_src;
            }

            // Notify factions present in the hex the ship was taken from, then
            // collect the same line for the AI gazette context.
            bool is_elite = fleet_has_captain(pobj);
            std::string msg = (is_elite ? pobj->name : "A pirate fleet")
                + " seized an abandoned " + ItemDefs[best_type].name
                + (offshore ? " off the coast of " + src_region->name + "."
                            : " in " + src_region->name + ".");
            for (const auto f : src_region->PresentFactions()) {
                f->event(msg, "monster", src_region, pirate_unit);
            }
            if (is_elite)
                pirate_context_elite.push_back(msg);
            else
                pirate_context_regular.push_back(msg);
        }
    };

    // Land pass: a docked fleet merges out of empty fleets in its own region.
    for (const auto r : regions) {
        if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
        if (TerrainDefs[r->type].similar_type == R_LAKE) continue;

        for (const auto pobj : r->objects) {
            if (!pobj->IsFleet()) continue;

            // Candidate sources are collected up front: a source deleted when it
            // empties mid-loop must never be dereferenced again (design edge 13).
            std::vector<Object *> empty_fleets;
            for (const auto obj : r->objects) {
                if (!obj->IsFleet()) continue;
                if (!obj->units.empty()) continue;
                empty_fleets.push_back(obj);
            }
            if (empty_fleets.empty()) continue;

            seize_into(pobj, empty_fleets, false);
        }
    }

    // Offshore pass: a fleet in the water scans every land neighbour for empty
    // fleets and takes the single best ship across all of them. Avoidance is
    // deliberately not consulted - an offshore raid on a guarded coast is a
    // choice, and the counterplay is active, not passive.
    if (offshore_enabled) {
        for (const auto r : regions) {
            if (TerrainDefs[r->type].similar_type != R_OCEAN
                && TerrainDefs[r->type].similar_type != R_LAKE) continue;

            for (const auto pobj : r->objects) {
                if (!pobj->IsFleet()) continue;

                std::vector<Object *> empty_fleets;
                for (int d = 0; d < NDIRS; d++) {
                    ARegion *nb = r->neighbors[d];
                    if (!nb) continue;
                    if (TerrainDefs[nb->type].similar_type == R_OCEAN) continue;
                    if (TerrainDefs[nb->type].similar_type == R_LAKE) continue;
                    for (const auto obj : nb->objects) {
                        if (!obj->IsFleet()) continue;
                        if (!obj->units.empty()) continue;
                        empty_fleets.push_back(obj);
                    }
                }
                if (empty_fleets.empty()) continue;

                seize_into(pobj, empty_fleets, true);
            }
        }
    }
}

/**
 * @brief Generation-time tuning report on what the monster passes produced
 *
 * Runs after CreateWMons/CreateLMons/CreateVMons, so every monster exists.
 *
 * Pirates are reported per level on purpose: the underworld and underdeep carry
 * ocean hexes, and ocean is where pirate fleets spawn, so a share of the pirate
 * content sits where players do not go early. On a world built around fighting
 * pirates that split has to be visible.
 *
 * @note Runs only from Game::NewGame(), guarded by GENERATION_TUNING_STATS.
 * @see Game::MakePirateFleet(), Game::MakePirateLair()
 */
void Game::MonsterStatistics()
{
    if constexpr (!GENERATION_TUNING_STATS) return;

    struct LevelTally {
        int fleets = 0, elite_fleets = 0, afloat = 0;
        int lairs = 0, lair_pirates = 0, lair_bosuns = 0;
        std::map<int, int> monsters_by_item;
    };
    std::map<int, LevelTally> by_level;

    for (const auto reg : regions) {
        LevelTally &t = by_level[reg->zloc];

        for (const auto obj : reg->objects) {
            bool is_fleet = obj->IsFleet();
            bool is_pirate_lair = (obj->type >= 0 && obj->type < (int)ObjectDefs.size() &&
                                   ObjectDefs[obj->type].monster == I_PIRATES);

            int pirates = 0, captains = 0, bosuns = 0;
            for (const auto u : obj->units) {
                if (u->faction->num != monfaction) continue;
                for (auto i : u->items)
                    if (ItemDefs[i->type].type & IT_MONSTER)
                        t.monsters_by_item[i->type] += i->num;
                pirates  += u->items.GetNum(I_PIRATES);
                captains += u->items.GetNum(I_PIRATE_CAPTAIN);
                bosuns   += u->items.GetNum(I_PIRATE_BOSUN);
            }

            if (is_fleet && (pirates || captains)) {
                t.fleets++;
                t.afloat += pirates;
                if (captains > 0) t.elite_fleets++;
            } else if (is_pirate_lair && pirates) {
                t.lairs++;
                t.lair_pirates += pirates;
                if (bosuns > 0) t.lair_bosuns++;
            }
        }
    }

    int boss_hunts = 0;
    for (auto q = quests.begin(); q != quests.end(); ++q)
        if ((*q)->subtype == Quest::GLOBAL_BOSS_HUNT) boss_hunts++;

    // A world with no monsters has nothing to report, and an all-zero block is
    // just noise - worlds built by the test harness are exactly that case.
    bool anything = (boss_hunts > 0);
    for (const auto &[level, t] : by_level)
        if (t.fleets || t.lairs || !t.monsters_by_item.empty()) { anything = true; break; }
    if (!anything) return;

    logger::write("");
    logger::write("=== TUNING: monsters at world creation ===");

    int all_fleets = 0, all_elite = 0, all_afloat = 0;
    for (const auto &[level, t] : by_level) {
        if (t.fleets == 0 && t.lairs == 0 && t.monsters_by_item.empty()) continue;

        all_fleets += t.fleets;
        all_elite  += t.elite_fleets;
        all_afloat += t.afloat;

        logger::write("-- level " + std::to_string(level) + " --");
        logger::write(
            "  pirate fleets: " + std::to_string(t.fleets) +
            ", elite: " + std::to_string(t.elite_fleets) +
            ", pirates afloat: " + std::to_string(t.afloat) +
            ", average crew: " + std::to_string(rounded_div(t.afloat, t.fleets))
        );
        logger::write(
            "  pirate lairs: " + std::to_string(t.lairs) +
            ", with bosun: " + std::to_string(t.lair_bosuns) +
            ", pirates in lairs: " + std::to_string(t.lair_pirates)
        );
        for (const auto &[item, count] : t.monsters_by_item)
            logger::write("    " + std::string(ItemDefs[item].name) + ": " + std::to_string(count));
    }

    logger::write("-- all levels --");
    logger::write(
        "  pirate fleets: " + std::to_string(all_fleets) +
        ", elite: " + std::to_string(all_elite) +
        " (" + std::to_string(percent_rounded(all_elite, all_fleets)) + "%)" +
        ", pirates afloat: " + std::to_string(all_afloat)
    );
    logger::write("  global boss-hunt quests: " + std::to_string(boss_hunts));
    logger::write("");
}
