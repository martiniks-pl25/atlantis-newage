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
 * 80% regular: Cog + crew of pirates.
 * 20% elite:   Galley  + crew (×3 pirates) + 1 captain + 1 bosun (both FLAG_BEHIND).
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

        // 35% chance to create a HUNT_PIRATE quest for this captain at spawn
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


void Game::PirateRecruitLandCrew()
{
    for (const auto r : regions) {
        for (const auto obj : r->objects) {
            if (!obj->IsFleet()) continue;
            if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
            if (TerrainDefs[r->type].similar_type == R_LAKE) continue;

            for (const auto u : obj->units) {
                if (!u->faction->is_npc) continue;
                int current = u->items.GetNum(I_PIRATES);
                if (current <= 0) continue;

                int pirate_w = ItemDefs[I_PIRATES].weight;
                int cap = (pirate_w > 0) ? obj->capacity / pirate_w : 0;
                if (cap <= 0) continue;
                if (current >= cap) {
                    logger::write("PirateRecruitLandCrew: fleet \"" + obj->name + "\""
                        + " at " + r->short_print()
                        + " — at cap (" + std::to_string(current) + "/" + std::to_string(cap)
                        + "), no recruitment");
                    continue;
                }

                int pct = 10 + rng::get_random(11);  // 10–20%
                int gained = std::max(1, current * pct / 100);
                // Do not exceed cap
                gained = std::min(gained, cap - current);
                u->items.SetNum(I_PIRATES, current + gained);

                logger::write("PirateRecruitLandCrew: fleet \"" + obj->name + "\""
                    + " at " + r->short_print()
                    + " — recruited " + std::to_string(gained) + " pirates"
                    + " (" + std::to_string(pct) + "%, was " + std::to_string(current)
                    + ", now " + std::to_string(current + gained) + ")");

                // Notify factions present in the region
                std::string msg = "Pirates from " + obj->name + " recruited " + std::to_string(gained)
                    + " new crew members while docked in " + r->short_print() + ".";
                std::set<Faction *> presentFactions = r->PresentFactions();
                for (const auto f : presentFactions) {
                    f->event(msg, "monster", r, u);
                }

                // Collect for AI gazette context (pirate_context in times.json).
                bool is_elite = u->items.GetNum(I_PIRATE_CAPTAIN) > 0;
                std::string ctx = (is_elite ? obj->name : "A pirate fleet")
                    + " recruited crew in " + r->name + ".";
                if (is_elite)
                    pirate_context_elite.push_back(ctx);
                else
                    pirate_context_regular.push_back(ctx);
            }
        }
    }
}

/**
 * @brief Pirates seize empty ships docked in the same non-ocean region.
 *
 * For each empty fleet (no units) in a land/lake-adjacent region,
 * finds a pirate fleet with >= 20 crew and splits 10–15% (min 1) of
 * pirates into the captured vessel. If the seized ship bears a generic
 * name (matching an IT_SHIP item name, "Ship", or "Fleet"), it is
 * renamed via getPirateShipName().
 *
 * Called once per turn after PirateRecruitLandCrew(), before movement.
 */
void Game::PirateSeizeEmptyShips()
{
    const int MIN_PIRATES = 20;

    for (const auto r : regions) {
        if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
        if (TerrainDefs[r->type].similar_type == R_LAKE) continue;

        // Collect empty fleets in this region
        std::vector<Object *> empty_fleets;
        for (const auto obj : r->objects) {
            if (!obj->IsFleet()) continue;
            if (!obj->units.empty()) continue;
            empty_fleets.push_back(obj);
        }
        if (empty_fleets.empty()) continue;

        // For each empty fleet, find one pirate fleet with enough crew
        for (const auto target : empty_fleets) {
            for (const auto pobj : r->objects) {
                if (!pobj->IsFleet()) continue;

                Unit *pirate_unit = nullptr;
                for (const auto u : pobj->units) {
                    if (!u->faction->is_npc) continue;
                    int n = u->items.GetNum(I_PIRATES);
                    if (n >= MIN_PIRATES) { pirate_unit = u; break; }
                }
                if (!pirate_unit) continue;

                int current = pirate_unit->items.GetNum(I_PIRATES);
                int pct = 10 + rng::get_random(6);  // 10–15%
                // At least 2 taken from original fleet
                int split = std::max(2, current * pct / 100);
                // Recruits bring the boarding crew up to 4 minimum
                int bonus = std::max(0, 4 - split);
                int total = split + bonus;

                pirate_unit->items.SetNum(I_PIRATES, current - split);

                // Spawn new crew as owner of the seized fleet
                Faction *monfac = GetFaction(factions, monfaction);
                Unit *crew = GetNewUnit(monfac, 0);
                crew->MakeWMon("Pirates", I_PIRATES, total);
                crew->free = Globals->MONSTER_SPOILS_RECOVERY;
                crew->MoveUnit(target);
                crew->UpdateMonsterDescription();

                // Rename if generic ship/fleet name (default names players rarely keep)
                static const std::initializer_list<std::string_view> kGenericPrefixes = {
                    "Ship", "Fleet", "Raft", "Longship", "Cog",
                    "Galleon", "Galley", "Clipper", "Skyship", "Balloon"
                };
                bool generic = false;
                for (auto p : kGenericPrefixes) {
                    if (target->name.starts_with(p)) { generic = true; break; }
                }

                std::string old_name = target->name;
                if (generic) target->set_name(getPirateShipName());

                logger::write("PirateSeizeEmptyShips: \"" + pobj->name + "\""
                    + " at " + r->short_print()
                    + " seized \"" + old_name + "\""
                    + (generic ? " → \"" + target->name + "\"" : "")
                    + " — boarding crew " + std::to_string(total)
                    + " (" + std::to_string(split) + " split"
                    + (bonus > 0 ? "+" + std::to_string(bonus) + " recruits" : "")
                    + ", " + std::to_string(pct) + "%"
                    + ", was " + std::to_string(current)
                    + ", remaining " + std::to_string(current - split) + ")");

                break;  // one pirate fleet per empty ship
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
