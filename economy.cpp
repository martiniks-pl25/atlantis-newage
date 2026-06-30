#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include "game.h"
#include "gamedata.h"

/* TownType */
// Depends on population and town development
int TownInfo::TownType()
{
    int prestige = pop * (dev + 220) / 270;
    if (prestige < Globals->CITY_POP / 4) return TOWN_VILLAGE;
    if (prestige < Globals->CITY_POP * 4 / 5) return TOWN_TOWN;
    return TOWN_CITY;
}

int ARegion::Population()
{
    if (town) {
        return population + town->pop;
    } else {
        return population;
    }
}

// IMPORTANT: wages now represent fractional wages, that
// means the value of the wages variable is 10 times the silver value
int ARegion::Wages()
{
    // Calculate new wages
    wages = 0;
    if (Population() == 0) return 0;
    int level = 1;
    int last = 0;
    int dv = development + RoadDevelopment() + (earthlore + clearskies) * 12;
    // Note: earthlore and clearskies represent LEVEL of the spell
    // Adjust for TownType
    if (town) {
        int tsize = town->TownType();
        dv += (tsize * tsize + 1);
    }
    while (dv >= level) {
        wages++;
        last = level;
        level += wages+1;
    }
    wages *= 10;
    if (dv > last)
        wages += 10 * (dv - last) / (level - last);
    return wages;
}

std::string ARegion::wages_for_report()
{
    Production *p = get_production_for_skill(I_SILVER, -1);
    if (p) {
        return "$" + std::to_string(p->productivity / 10) +
            "." + std::to_string(p->productivity % 10) + " (Max: $" + std::to_string(p->amount) + ")";
    } else
        return "$" + std::to_string(0);
}

/**
 * @brief Creates recruitment market for common units
 *
 * Creates a M_BUY market for the region's race with quantity based on population.
 * Formula: amount = Population() / MEN_PER_MARKET_UNIT
 *
 * @note Market amount will be recalculated by Market::post_turn() each turn
 * @see AddLeadersMarket(), Market::post_turn()
 */
void ARegion::AddMenMarket() {
    float ratio = ItemDefs[race].baseprice / ((float)Globals->BASE_MAN_COST * 10);
    // hack: include wage factor of 10 in float calculation above
    Market* m = new Market(
        Market::MarketType::M_BUY, race,
        (int)(Wages() * 4 * ratio),
        Population() / MEN_PER_MARKET_UNIT, 0, 10000, 0, 2000
    );
    markets.push_back(m);
}

/**
 * @brief Creates recruitment market for leaders
 *
 * Creates a M_BUY market for leaders with quantity based on population.
 * Formula: amount = Population() / LEADERS_PER_MARKET_UNIT  (1 per 900 pop)
 *
 * In regions WITHOUT a settlement the market is created with the calculated
 * amount, but PostTurn() (and UpdateEditRegion()) will apply a 35% wilderness
 * chance roll afterward — on a failed roll amount is set to 0 for that turn.
 * Regions WITH a settlement are not subject to the roll (leaders always available).
 *
 * @note Skips if LEADERS_EXIST is disabled or terrain has NO_LEADERS flag
 * @note Market amount will be recalculated by Market::post_turn() each turn,
 *       then wilderness roll is applied (see PostTurn(), UpdateEditRegion())
 * @see AddMenMarket(), Market::post_turn(), ARegion::PostTurn()
 */
void ARegion::AddLeadersMarket() {
    if (!Globals->LEADERS_EXIST) return;

    // Check if terrain allows leader recruitment
    TerrainType* terrain = &TerrainDefs[type];
    if (terrain->flags & TerrainType::NO_LEADERS) return;

    float ratio = ItemDefs[I_LEADERS].baseprice / ((float)Globals->BASE_MAN_COST * 10);
    // hack: include wage factor of 10 in float calculation above
    Market* m = new Market(
        Market::MarketType::M_BUY, I_LEADERS,
        (int)(Wages() * 4 * ratio),
        Population() / LEADERS_PER_MARKET_UNIT, 0, 10000, 0, 400
    );
    markets.push_back(m);
}

void ARegion::SetupHabitat(TerrainType* terrain) {
    if (habitat > 1) habitat *= 5;
    if ((habitat < 100) && (terrain->similar_type != R_OCEAN)) habitat = 100;

    int pop = terrain->pop;
    int mw = terrain->wages;

    // fix economy when MAINTENANCE_COST has been adjusted
    mw += Globals->MAINTENANCE_COST - 10;
    if (mw < 0) mw = 0;

    if (pop == 0) {
        population = 0;
        basepopulation = 0;
        wages = 0;
        maxwages = 0;
        wealth = 0;
        return;
    }

    // Only select race here if it hasn't been set during Race Growth
    // in the World Creation process.
    if ((race == -1) || (!Globals->GROW_RACES)) {
        int noncoastalraces = sizeof(terrain->races) / sizeof(terrain->races[0]);
        int allraces =
            noncoastalraces + sizeof(terrain->coastal_races) / sizeof(terrain->coastal_races[0]);

        race = -1;
        while (race == -1 || (ItemDefs[race].flags & ItemType::DISABLED)) {
            int n = rng::get_random(IsCoastal() ? allraces : noncoastalraces);
            if (n > noncoastalraces-1) {
                race = terrain->coastal_races[n-noncoastalraces-1];
            } else
                race = terrain->races[n];
        }
    }

    habitat = habitat * 2 / 3 + rng::get_random(habitat / 3);

    // Bounds check
    if (race < 0 || race >= (int)ItemDefs.size()) {
        logger::write("      ERROR: race " + std::to_string(race) + " is out of bounds! ItemDefs.size()=" + std::to_string(ItemDefs.size()));
        race = I_PLAINSMAN; // Fallback to a safe default race
    }

    // Only call find_race if ManDefs is not empty
    if (!ManDefs.empty()) {
        auto race_opt = find_race(ItemDefs[race].abr);
        if (race_opt.has_value()) {
            auto& mt = race_opt->get();
            if (mt.terrain == terrain->similar_type) {
                habitat = (habitat * 9)/8;
            }
        }
    }

    if (!IsNativeRace(race)) {
        habitat = (habitat * 4)/5;
    }

    basepopulation = habitat / 3;
    // hmm... somewhere not too far off equilibrium pop
    population = habitat * (60 + rng::get_random(6) + rng::get_random(6)) / 100;

    // Setup development
    int level = 1;
    development = 1;
    int prev = 0;
    while (level < mw) {
        development++;
        prev++;
        if (prev > level) {
            level++;
            prev = 0;
        }
    }
    development += rng::get_random(25);
    maxdevelopment = development;
}

/**
 * @brief Creates economic infrastructure during world generation
 *
 * Final step of region initialization. Creates work/entertainment Production
 * objects and recruitment markets. Called after terrain, population, and towns
 * are set up.
 *
 * Flow: SetupProds() → SetupHabitat() → SetupPop() → SetupEconomy()
 *
 * @note Only called during world generation, not during turn processing
 * @see SetupPop(), AddMenMarket(), AddLeadersMarket()
 */
void ARegion::SetupEconomy() {
    /* Setup basic economy */
    maxwages = Wages();

    /* taxable region wealth */
    wealth = (int) ((float) (Population()
        * (Wages() - 10 * Globals->MAINTENANCE_COST) / 50));
    if (wealth < 0) wealth = 0;

    // wage-relevant population (/10 wages /5 popfactor)
    int pp = Population();
    // adjustment for rural areas
    if (pp < 3000) {
        float wpfactor = (float) (120 / (61 - pp / 50));
        pp += (int) ((float) ((wpfactor * pp + 3000)/(wpfactor + 1)));
    }
    int wagelimit = (int) ((float) (pp * (Wages() - 10 * Globals->MAINTENANCE_COST) /50));
    if (wagelimit < 0) wagelimit = 0;
    Production * w = new Production;
    w->itemtype = I_SILVER;
    w->amount = wagelimit / Globals->WORK_FRACTION;
    w->baseamount = wagelimit / Globals->WORK_FRACTION;
    w->skill = -1;
    w->productivity = wages;

    /* Entertainment - setup or adjust */
    int ep = Population();
    // adjustment for rural areas
    if (ep < 3000) {
        int epf = (ep / 10 + 300) / 6;
        ep = ep * epf / 100;
    }
    int maxent = (int) ((float) (ep * ((Wages() - 10 * Globals->MAINTENANCE_COST) + 1) /50));
    if (maxent < 0) maxent = 0;

    Production * e = new Production;
    e->itemtype = I_SILVER;
    e->skill = S_ENTERTAINMENT;
    // Reduce entertainment by 20% to compensate for increased building bonuses
    // (buildings now give +100% instead of +25% on first building)
    e->amount = (int)((maxent / Globals->ENTERTAIN_FRACTION) * 0.8f);

    e->baseamount = (int)((maxent / Globals->ENTERTAIN_FRACTION) * 0.8f);
    // raise entertainment income by productivity factor 10
    e->productivity = Globals->ENTERTAIN_INCOME * 10;

    // note: wage factor 10, population factor 5 - included as "/ 50"
    /* More wealth in safe Starting Cities */
    if ((Globals->SAFE_START_CITIES) && (IsStartingCity())) {
        int wbonus = (Population() / 5) * Globals->MAINTENANCE_COST;
        wealth += wbonus;
        w->amount += wbonus / Globals->WORK_FRACTION;
        w->baseamount += wbonus / Globals->WORK_FRACTION;
        e->amount += (int)((wbonus / Globals->ENTERTAIN_FRACTION) * 0.8f);
        e->baseamount += (int)((wbonus / Globals->ENTERTAIN_FRACTION) * 0.8f);
    }
    products.push_back(w);
    products.push_back(e);

    AddMenMarket();
    AddLeadersMarket();

}

void ARegion::SetupPop()
{
    if (type < 0 || type >= (int)TerrainDefs.size()) {
        return;
    }

    TerrainType *typer = &(TerrainDefs[type]);
    habitat = typer->pop+1;

    SetupHabitat(typer);
    if (population == 0) {
        return;
    }

    // Lakes and BARREN-flagged terrains (oceans, deadwater, barrens, dungeon) must
    // never host settlements. Surface parametric generator already filters these in
    // economy() (aregion.cpp), but underworld/underdeep go through FinalSetup → Setup →
    // SetupPop, so the check must live here too.
    bool town_eligible_terrain = (type != R_LAKE) &&
        !(TerrainDefs[type].flags & TerrainType::BARREN);

    if (Globals->TOWNS_EXIST && town_eligible_terrain) {
        int adjacent = 0;
        int prob = Globals->TOWN_PROBABILITY;
        if (prob < 1) prob = 100;
        int townch = (int) 80000 / prob;
        if (Globals->TOWNS_NOT_ADJACENT) {
                for (int d = 0; d < NDIRS; d++) {
                    ARegion *newregion = neighbors[d];
                    if ((newregion) && (newregion->town)) adjacent++;
                }
            }
        if (Globals->LESS_ARCTIC_TOWNS) {
            int dnorth = GetPoleDistance(D_NORTH);
            int dsouth = GetPoleDistance(D_SOUTH);
            if (dnorth < 9)
                townch = townch + 25 * (9 - dnorth) *
                    (9 - dnorth) * Globals->LESS_ARCTIC_TOWNS;
            if (dsouth < 9)
                townch = townch + 25 * (9 - dsouth) *
                    (9 - dsouth) * Globals->LESS_ARCTIC_TOWNS;
        }
        int spread = Globals->TOWN_SPREAD;
        if (spread > 100) spread = 100;
        int townprob = (TerrainDefs[type].economy * 4 * (100 - spread) +
            100 * spread) / 100;
        if (adjacent > 0) townprob = townprob * (100 - Globals->TOWNS_NOT_ADJACENT) / 100;
        if (rng::get_random(townch) < townprob) add_town();
    }

    SetupEconomy();
}

/* ONE function where the wage and entertainment income
 * is set (previously this has been all over the place!) */
void ARegion::SetIncome()
{
    /* do nothing in unpopulated regions */
    if (basepopulation == 0) return;

    maxwages = Wages();

    /* taxable region wealth */
    wealth = (int) ((float) (Population()
        * (Wages() - 10 * Globals->MAINTENANCE_COST) / 50));
    if (wealth < 0) wealth = 0;

    /* Wages */
    // wage-relevant population (/10 wages /5 popfactor)
    int pp = Population();
    // adjustment for rural areas
    if (pp < 3000) {
        float wpfactor = (float) (120 / (61 - pp / 50));
        pp += (int) ((float) ((wpfactor * pp + 3000)/(wpfactor + 1)));
    }
    int maxwages = (int) ((float) (pp * (Wages() - 10 * Globals->MAINTENANCE_COST) /50));
    if (maxwages < 0) maxwages = 0;
    Production * w = get_production_for_skill(I_SILVER,-1);
    // In some cases (ie. after products.DeleteAll() in EditGameRegionTerrain)
    // I_SILVER is not in ProductionList
    if( !w ) {
      w = new Production;
      products.push_back(w);
    }
    w->itemtype = I_SILVER;
    w->amount = maxwages / Globals->WORK_FRACTION;
    w->baseamount = maxwages / Globals->WORK_FRACTION;
    w->skill = -1;
    w->productivity = wages;

    /* Entertainment - setup or adjust */
    int ep = Population();
    // adjustment for rural areas
    if (ep < 3000) {
        int epf = (ep / 10 + 300) / 6;
        ep = ep * epf / 100;
    }
    int maxent = (int) ((float) (ep * ((Wages() - 10 * Globals->MAINTENANCE_COST) + 10) /50));
    if (maxent < 0) maxent = 0;
    Production * e = get_production_for_skill(I_SILVER,S_ENTERTAINMENT);
    // In some cases (ie. after products.DeleteAll() in EditGameRegionTerrain)
    // I_SILVER is not in ProductionList
    if( !e ) {
      e = new Production;
      products.push_back(e);
    }
    e->itemtype = I_SILVER;
    e->amount = maxent / Globals->ENTERTAIN_FRACTION;
    e->baseamount = maxent / Globals->ENTERTAIN_FRACTION;
    e->skill = S_ENTERTAINMENT;
    // raise entertainment income by productivity factor 10
    e->productivity = Globals->ENTERTAIN_INCOME * 10;

    // note: wage factor 10, population factor 5 - included as "/ 50"
    /* More wealth in safe Starting Cities */
    if ((Globals->SAFE_START_CITIES) && (IsStartingCity())) {
        int wbonus = (Population() / 5) * Globals->MAINTENANCE_COST;
        wealth += wbonus;
        w->amount += wbonus / Globals->WORK_FRACTION;
        w->baseamount += wbonus / Globals->WORK_FRACTION;
        e->amount += (int)((wbonus / Globals->ENTERTAIN_FRACTION) * 0.8f);
        e->baseamount += (int)((wbonus / Globals->ENTERTAIN_FRACTION) * 0.8f);
    }
}

void ARegion::DisbandInRegion(int item, int amt)
{
    if (!Globals->DYNAMIC_POPULATION) return;
    if (amt > 0) {
        if (amt > Population()) {
            // exchange region race!
            race = item;
            population = 0;
            if (town) town->pop = 0;
            AdjustPop(amt);
        } else {
            if (race != item) amt = amt * 2 / 3;
            AdjustPop(amt);
        }
    }
}

void ARegion::Recruit(int amt)
{
    if (!Globals->DYNAMIC_POPULATION) return;
    int loss = amt * Globals->RECRUIT_POP_LOSS_PERCENT / 100;
    AdjustPop(-loss);
}

/**
 * @brief Distribute population growth between town and region
 *
 * Proportional allocation based on available space:
 * - town_growth = adjustment × (town_space / total_space)
 * - region_growth = adjustment × (region_space / total_space)
 *
 * Formula:
 * - town gets: adjustment × tspace / (tspace + rspace)
 * - region gets: adjustment × rspace / (tspace + rspace)
 *
 * @param adjustment Total population change (can be negative)
 */
void ARegion::AdjustPop(int adjustment)
{
    // Regions without towns: all growth goes to regional population
    if (!town) {
        population += adjustment;
        return;
    }

    int tspace = town->hab - town->pop;
    int rspace = habitat - population;

    if (adjustment > 0) {
        // Growth: distribute proportionally to available capacity.
        // Clamp to non-negative so overcrowded side does not absorb growth.
        int eff_t = std::max(0, tspace);
        int eff_r = std::max(0, rspace);
        int total = eff_t + eff_r;
        if (total == 0) return;  // both full
        town->pop += adjustment * eff_t / total;
        if (town->pop < 0) town->pop = 0;
        population += adjustment * eff_r / total;
        if (population < 0) population = 0;
    } else if (adjustment < 0) {
        // Shrinkage: absorbed by whichever side is relatively more occupied.
        // Compare population/habitat vs town->pop/town->hab via cross-multiply.
        // This ensures: overcrowded region always absorbs its own excess;
        // a town that is less full than the region is protected.
        bool regionMoreFull = (habitat > 0 && town->hab > 0)
            ? ((long long)population * town->hab >= (long long)town->pop * habitat)
            : true;
        if (regionMoreFull) {
            population += adjustment;
            if (population < 0) population = 0;
        } else {
            town->pop += adjustment;
            if (town->pop < 0) town->pop = 0;
        }
    }
}

void ARegion::SetupCityMarket()
{
    int cap;
    int offset = 0;
    int citymax = Globals->CITY_POP;
    auto localrace = find_race(ItemDefs[race].abr);
    if (!localrace) localrace = find_race("SELF");
    auto locals = localrace->get();
    /* compose array of possible supply & demand items */
    int supply[NITEMS];
    int demand[NITEMS];
    /* possible advanced and magic items */
    int rare[NITEMS];
    int antiques[NITEMS];
    int i;
    for (i=0; i<NITEMS; i++) {
        supply[i] = 0;
        demand[i] = 0;
        rare[i] = 0;
        antiques[i] = 0;
        if (ItemDefs[i].flags & ItemType::DISABLED) continue;
        if (ItemDefs[i].flags & ItemType::NOMARKET) continue;
        if (ItemDefs[i].type & IT_SHIP) continue;
        if (i==I_SILVER) continue;
        if ((ItemDefs[i].type & IT_MAN)
            || (ItemDefs[i].type & IT_LEADER)) continue;

        int canProduceHere = 0;
        // Check if the product can be produced in the region
        // Raw goods
        if (ItemDefs[i].pInput[0].item == -1) {
            for (unsigned int c = 0;
                c<(sizeof(TerrainDefs[type].prods)/sizeof(TerrainDefs[type].prods[0]));
                c++) {
                int resource = TerrainDefs[type].prods[c].product;
                if (i == resource) {
                    canProduceHere = 1;
                    break;
                }
            }
        }
        // Non-raw goods
        else {
            canProduceHere = 1;
            for (unsigned int c = 0;
                c<(sizeof(ItemDefs[i].pInput)/sizeof(ItemDefs[i].pInput[0]));
                c++) {
                int match = 0;
                int need = ItemDefs[i].pInput[c].item;
                for (unsigned int r=0;
                    r<(sizeof(TerrainDefs[type].prods)/sizeof(TerrainDefs[type].prods[0]));
                    r++) {
                    if (TerrainDefs[type].prods[r].product == need)
                        match = 1;
                }
                if (!match) {
                    canProduceHere = 0;
                    break;
                }
            }
        }
        bool canProduce = false;
        // Check if the locals can produce this item
        if (canProduceHere) canProduce = locals.CanProduce(i);
        bool isUseful = locals.CanUse(i);
        //Normal Items
        if (ItemDefs[ i ].type & IT_NORMAL) {

            if (i==I_GRAIN || i==I_LIVESTOCK || i==I_FISH) {
                // Add foodstuffs directly to market.
                // minpop=0 / maxpop=CITY_POP spans the full town->pop range.
                // food_effective_amount() overrides amount in PostTurn using
                // three-tier scaling; stored maxamt is the village-tier base.
                int amt = Globals->CITY_MARKET_NORMAL_AMT;
                int price;

                if (Globals->RANDOM_ECONOMY) {
                    amt += rng::get_random(amt);
                    price = (ItemDefs[i].baseprice * (100 + rng::get_random(50))) / 100;
                } else {
                    price = ItemDefs[ i ].baseprice;
                }

                Market * m = new Market(
                    Market::MarketType::M_SELL, i, price, amt, 0, citymax, amt, amt * 2
                );
                markets.push_back(m);
            } else if (i == I_FOOD) {
                // Add foodstuffs directly to market
                int amt = Globals->CITY_MARKET_NORMAL_AMT;
                int price;
                if (Globals->RANDOM_ECONOMY) {
                    amt += rng::get_random(amt);
                    price = (ItemDefs[i].baseprice * (120 + rng::get_random(80))) /
                        100;
                } else {
                    price = ItemDefs[ i ].baseprice;
                }

                cap = (citymax * 3/4) - 5000;
                if (cap < 0) cap = citymax/2;
                Market * m = new Market(
                    Market::MarketType::M_BUY, i, price, amt, population, population + 2 * cap, amt, amt * 5
                );
                markets.push_back(m);
            } else if (ItemDefs[i].pInput[0].item == -1) {
                // Basic resource
                // Add to supply?
                if (canProduce) supply[i] = 4;
                // Add to demand?
                if (!canProduceHere) {
                    // Is it a mount?
                    if (ItemDefs[i].type & IT_MOUNT) {
                        if (locals.CanProduce(i)) demand[i] = 4;
                    } else if (isUseful) demand[i] = 4;
                }
            } else {

                // Tool, weapon or armor
                if (isUseful) {
                    // Add to supply?
                    if (canProduce) supply[i] = 2;
                    // Add to demand?
                    if (!canProduceHere) demand[i] = 2;
                }
            }
        } // end Normal Items
        // Advanced Items
        else if ((Globals->CITY_MARKET_ADVANCED_AMT)
            && (ItemDefs[i].type & IT_ADVANCED)) {
            if (isUseful) rare[i] = 4;
            if (ItemDefs[i].hitchItem > 0) rare[i] = 2;
        }
        // Magic Items
        else if ((Globals->CITY_MARKET_MAGIC_AMT)
            && (ItemDefs[i].type & IT_MAGIC)) {
            if (isUseful) antiques[i] = 4;
                else antiques[i] = 1;
            if (ItemDefs[i].hitchItem > 0) antiques[i] = 2;
        }
    }
    /* Check for advanced item */
    if ((Globals->CITY_MARKET_ADVANCED_AMT) && (rng::get_random(4) == 1)) {
        int ad = 0;
        for (int i=0; i<NITEMS; i++) ad += rare[i];
        ad = rng::get_random(ad);
        int i;
        int sum = 0;
        for (i=0; i<NITEMS; i++) {
            sum += rare[i];
            if (ad < sum) break;
        }
        if (ad < sum) {
            int amt = Globals->CITY_MARKET_ADVANCED_AMT;
            int price;
            if (Globals->RANDOM_ECONOMY) {
                amt += rng::get_random(amt);
                price = (ItemDefs[i].baseprice * (100 + rng::get_random(50))) / 100;
            } else {
                price = ItemDefs[ i ].baseprice;
            }

            cap = (citymax *3/4) - 5000;
            if (cap < citymax/2) cap = citymax / 2;
            offset = citymax / 8;
            if (cap+offset < citymax) {
                Market * m = new Market(
                    Market::MarketType::M_SELL, i, price, amt / 6, population + cap + offset,
                    population + citymax, 0, amt
                );
                markets.push_back(m);
            }
        }
    }
    /* Check for magic item */
    if ((Globals->CITY_MARKET_MAGIC_AMT) && (rng::get_random(8) == 1)) {
        int mg = 0;
        for (int i=0; i<NITEMS; i++) mg += antiques[i];
        mg = rng::get_random(mg);
        int i;
        int sum = 0;
        for (i=0; i<NITEMS; i++) {
            sum += antiques[i];
            if (mg < sum) break;
        }
        if (mg < sum) {
            int amt = Globals->CITY_MARKET_MAGIC_AMT;
            int price;

            if (Globals->RANDOM_ECONOMY) {
                amt += rng::get_random(amt);
                price = (ItemDefs[i].baseprice * (100 + rng::get_random(50))) / 100;
            } else {
                price = ItemDefs[ i ].baseprice;
            }

            cap = (citymax *3/4) - 5000;
            if (cap < citymax/2) cap = citymax / 2;
            offset = (citymax/20) + ((citymax/5) * 2);
            Market * m = new Market(
                Market::MarketType::M_SELL, i, price, amt / 6, population + cap, population + citymax, 0, amt
            );
            markets.push_back(m);
        }
    }

    /* Add demand (normal) items */
    int num = 4;
    int sum = 1;
    while((num > 0) && (sum > 0)) {
        int dm = 0;
        for (int i=0; i<NITEMS; i++) dm += demand[i];
        dm = rng::get_random(dm);
        int i;
        sum = 0;
        for (i=0; i<NITEMS; i++) {
            sum += demand[i];
            if (dm < sum) break;
        }
        if (dm >= sum) continue;

        int amt = Globals->CITY_MARKET_NORMAL_AMT;
        amt = demand[i] * amt / 4;
        int price;

        if (Globals->RANDOM_ECONOMY) {
            amt += rng::get_random(amt);
            price = (ItemDefs[i].baseprice *
                (100 + rng::get_random(50))) / 100;
        } else {
            price = ItemDefs[i].baseprice;
        }

        cap = (citymax/4);
        offset = - (citymax/20) + ((5-num) * citymax * 3/40);
        Market * m = new Market(
            Market::MarketType::M_SELL, i, price, amt / 6, population + cap + offset, population + citymax, 0, amt
        );
        markets.push_back(m);
        demand[i] = 0;
        num--;
    }

    /* Add supply (normal) items */
    num = 2;
    sum = 1;
    while((num > 0) && (sum > 0)) {
        int su = 0;
        for (int i=0; i<NITEMS; i++) su += supply[i];
        su = rng::get_random(su);
        int i;
        sum = 0;
        for (i=0; i<NITEMS; i++) {
            sum += supply[i];
            if (su < sum) break;
        }
        if (su >= sum) continue;

        int amt = Globals->CITY_MARKET_NORMAL_AMT;
        amt = supply[i] * amt / 4;
        int price;

        if (Globals->RANDOM_ECONOMY) {
            amt += rng::get_random(amt);
            price = (ItemDefs[i].baseprice *
                (150 + rng::get_random(50))) / 100;
        } else {
            price = ItemDefs[ i ].baseprice;
        }

        cap = (citymax/4);
        offset = ((3-num) * citymax * 3 / 40);
        if (supply[i] < 4) offset += citymax / 20;
        Market * m = new Market(
            Market::MarketType::M_BUY, i, price, 0, population + cap + offset, population + citymax, 0, amt
        );
        markets.push_back(m);
        supply[i] = 0;
        num--;
    }

}

/**
 * @brief Creates trade markets for a city from pre-assigned item lists.
 *
 * Items are pre-assigned globally by ARegionList::economy() using round-robin
 * distribution to guarantee all 18 trade goods appear in both M_BUY and M_SELL
 * across the world. This function just creates the Market objects with correct
 * prices and population thresholds.
 *
 * Population thresholds are absolute, tied to town type transitions:
 *   idx=0: village→town boundary = CITY_POP*270/(4*(dev+220))
 *   idx=1: midpoint of town range
 *   idx=2: town→city boundary = CITY_POP*4*270/(5*(dev+220))
 *
 * @param buy_items  Items city sells to player (M_BUY, always visible)
 * @param sell_items Items city buys from player (M_SELL, hidden until player has item)
 */
void ARegion::SetupTradeMarkets(const std::vector<int>& buy_items,
                                const std::vector<int>& sell_items)
{
    int citymax = Globals->CITY_POP;

    int dev_factor       = town ? (town->dev + 220) : 220;
    int pop_village_town = citymax * 270 / (4 * dev_factor);
    int pop_town_city    = citymax * 4 * 270 / (5 * dev_factor);
    int pop_mid          = (pop_village_town + pop_town_city) / 2;
    int thresholds[3]    = { pop_village_town, pop_mid, pop_town_city };

    // M_SELL: town buys from player — hidden until player has the item (aregion.cpp report)
    for (int idx = 0; idx < (int)sell_items.size() && idx < 3; idx++) {
        int i = sell_items[idx];
        int amt = Globals->CITY_MARKET_TRADE_AMT;
        int price;
        if (Globals->RANDOM_ECONOMY) {
            amt += rng::get_random(amt);
            price = Globals->MORE_PROFITABLE_TRADE_GOODS
                ? (ItemDefs[i].baseprice * (300 + rng::get_random(100))) / 100
                : (ItemDefs[i].baseprice * (150 + rng::get_random(50))) / 100;
        } else {
            price = ItemDefs[i].baseprice;
        }
        markets.push_back(new Market(
            Market::MarketType::M_SELL, i, price, amt / 5,
            thresholds[idx], citymax, 0, amt
        ));
    }

    // M_BUY: town sells to player — always visible in report
    for (int idx = 0; idx < (int)buy_items.size() && idx < 3; idx++) {
        int i = buy_items[idx];
        int amt = Globals->CITY_MARKET_TRADE_AMT;
        int price;
        if (Globals->RANDOM_ECONOMY) {
            amt += rng::get_random(amt);
            price = Globals->MORE_PROFITABLE_TRADE_GOODS
                ? (ItemDefs[i].baseprice * (100 + rng::get_random(90))) / 100
                : (ItemDefs[i].baseprice * (100 + rng::get_random(50))) / 100;
        } else {
            price = ItemDefs[i].baseprice;
        }
        markets.push_back(new Market(
            Market::MarketType::M_BUY, i, price, amt / 8,
            thresholds[idx], citymax, 0, amt
        ));
    }
}

/**
 * @brief Assigns random trade goods for a single city (used by edit/reset, no global context).
 */
void ARegion::SetupRandomTradeMarkets()
{
    const int TRADE_COUNT = 3;
    std::vector<int> pool;
    for (int i = 0; i < NITEMS; i++) {
        if (ItemDefs[i].flags & ItemType::DISABLED) continue;
        if (ItemDefs[i].flags & ItemType::NOMARKET) continue;
        if (!(ItemDefs[i].type & IT_TRADE)) continue;
        pool.push_back(i);
    }
    if ((int)pool.size() < TRADE_COUNT * 2) return;

    // Shuffle and split: first half → buy, second half → sell (no overlap guaranteed)
    for (int i = (int)pool.size() - 1; i > 0; i--) {
        int j = rng::get_random(i + 1);
        std::swap(pool[i], pool[j]);
    }
    std::vector<int> buy_items(pool.begin(), pool.begin() + TRADE_COUNT);
    std::vector<int> sell_items(pool.begin() + TRADE_COUNT, pool.begin() + TRADE_COUNT * 2);
    SetupTradeMarkets(buy_items, sell_items);
}

void ARegion::SetupProds(double weight)
{
    Production *p = NULL;
    TerrainType *typer = &(TerrainDefs[type]);

    if (Globals->FOOD_ITEMS_EXIST) {
        if (typer->economy) {
            // Foodchoice = 0 or 1 if inland, 0, 1, or 2 if coastal
            int foodchoice = rng::get_random(2 +
                    (Globals->COASTAL_FISH && IsCoastal()));
            switch (foodchoice) {
                case 0:
                    if (!(ItemDefs[I_GRAIN].flags & ItemType::DISABLED))
                        p = new Production(I_GRAIN, typer->economy);
                    break;
                case 1:
                    if (!(ItemDefs[I_LIVESTOCK].flags & ItemType::DISABLED))
                        p = new Production(I_LIVESTOCK, typer->economy);
                    break;
                case 2:
                    if (!(ItemDefs[I_FISH].flags & ItemType::DISABLED))
                        p = new Production(I_FISH, typer->economy);
                    break;
            }
            products.push_back(p);
        }
    }

    for (unsigned int c= 0; c < (sizeof(typer->prods)/sizeof(typer->prods[0])); c++) {
        int item = typer->prods[c].product;
        int chance = typer->prods[c].chance * weight;
        int amt = typer->prods[c].amount;
        if (item != -1) {
            if (!(ItemDefs[item].flags & ItemType::DISABLED) &&
                    (rng::get_random(100) < chance)) {
                p = new Production(item, amt);
                products.push_back(p);
            }
        }
    }
}

/* Create a town randomly */
void ARegion::add_town()
{
    std::string tname = AGetNameString(AGetName(1, this));
    int size = DetermineTownSize();
    add_town(size, tname);
}

/* Create a town of any type with given name */
void ARegion::add_town(const std::string& tname)
{
    int size = DetermineTownSize();
    add_town(size, tname);
}

/* Create a town of given Town Type */
void ARegion::add_town(int size)
{
    std::string tname = AGetNameString(AGetName(1, this));
    add_town(size, tname);
}

/* Create a town of specific type with name
 * All other town creation functions call this one
 * in the last instance. */
void ARegion::add_town(int size, const std::string& name)
{
    town = new TownInfo;
    town->name = name;
    SetTownType(size);
    SetupCityMarket();
    /* remove all lairs */
    for(const auto obj : objects) {
        if (obj->type == O_DUMMY) continue;
        if ((ObjectDefs[obj->type].monster != -1) && (!(ObjectDefs[obj->type].flags & ObjectType::CANENTER))) {
            std::for_each(obj->units.begin(), obj->units.end(), [](Unit *u) { delete u; });
            obj->units.clear();
            std::erase(objects, obj);
            delete obj;
        }
    }
}

// Used at start to set initial town's size
int ARegion::DetermineTownSize()
{
    // If VILLAGES_ONLY is enabled, all settlements start as villages
    if (Globals->VILLAGES_ONLY) {
        return TOWN_VILLAGE;
    }

    // is it a city?
    if (rng::get_random(300) < Globals->TOWN_DEVELOPMENT) {
        return TOWN_CITY;
    }
    // is it a town?
    if (rng::get_random(220) < Globals->TOWN_DEVELOPMENT + 10) {
        return TOWN_TOWN;
    }
    // ... then it's a village!
    return TOWN_VILLAGE;
}

// Set an existing town to a specific town type
void ARegion::SetTownType(int level)
{
    if (!town) return;
    // set some basics
    town->hab = TownHabitat();
    town->pop = town->hab * 2 / 3;
    town->dev = TownDevelopment();

    // Sanity check
    if ((level < TOWN_VILLAGE) || (level > TOWN_CITY)) return;

    // increment values
    int poptown = rng::get_random((level -1) * (level -1) * Globals->CITY_POP/12) + level * level * Globals->CITY_POP/12;
    town->hab += poptown;
    town->pop = town->hab * 2 / 3;
    development += level * 6 + 2;
    town->dev = TownDevelopment();

    // now increment until we reach the right size
    while(town->TownType() != level) {
        // Increase?
        if (level > town->TownType()) {
            development += rng::get_random(Globals->TOWN_DEVELOPMENT / 10 + 5);
            int poplus = rng::get_random(Globals->CITY_POP/3) + rng::get_random(Globals->CITY_POP/3);
            // don't overgrow!
            while (town->pop + poplus > Globals->CITY_POP) {
                poplus = poplus / 2;
            }
            town->hab += poplus;
            town->pop = town->hab * 2 / 3;
            town->dev = TownDevelopment();
        }
            // or decrease...
        else {
            development -= rng::get_random(20 - Globals->TOWN_DEVELOPMENT / 10);
            int popdecr = rng::get_random(Globals->CITY_POP/3) + rng::get_random(Globals->CITY_POP/3);
            // don't depopulate
            while ((town->pop < popdecr) || (town->hab < popdecr)) {
                popdecr = popdecr / 2;
            }
            town->hab -= popdecr;
            town->pop = town->hab * 2 / 3;
            town->dev = TownDevelopment();
        }
    }

    maxdevelopment = development;
}

/**
 * @brief Recalculates region markets after population changes (GM editing)
 *
 * Updates income and market quantities based on new population. Recreates
 * recruitment markets (IT_MAN/IT_LEADER) to reset prices and reflect race changes.
 *
 * @note Called from edit.cpp when GM regenerates a region
 * @see SetupEditRegion(), AddMenMarket(), AddLeadersMarket()
 */
void ARegion::UpdateEditRegion()
{
    // redo markets and entertainment/tax income for extra people.
    SetIncome();
    for (auto& m : markets) {
        // Trade goods and food use town->pop; other markets use total Population()
        int pop = (town && (ItemDefs[m->item].type & (IT_TRADE | IT_FOOD))) ? town->pop : Population();
        m->post_turn(pop, Wages());
        // Food markets scale supply through village/town/city tiers based on town->pop
        if (town && (ItemDefs[m->item].type & IT_FOOD) && m->type == Market::MarketType::M_SELL)
            m->amount = food_effective_amount(m);
    }

    // Recreate recruitment markets to reset market state (prices, activity)
    // and update race if it changed
    markets.erase(
        remove_if(markets.begin(), markets.end(), [](const Market * m) { return ItemDefs[m->item].type & IT_MAN; }),
        markets.end()
    );

    AddMenMarket();
    AddLeadersMarket();

    // Wilderness leader availability: same 35% roll as in PostTurn()
    if (!town && Globals->LEADERS_EXIST) {
        constexpr int WILDERNESS_LEADER_CHANCE = 35; // % chance leaders are available this turn
        if (rng::get_random(100) >= WILDERNESS_LEADER_CHANCE) {
            for (auto& m : markets)
                if (ItemDefs[m->item].type & IT_LEADER)
                    m->amount = 0;
        }
    }
}

/**
 * @brief Fully initializes region economy for GM editing
 *
 * Regenerates entire region: race, population, development, towns, and markets.
 * Used by GM "g" command to recreate a region from scratch while preserving terrain.
 *
 * @note Called from edit.cpp after markets are cleared externally
 * @note Followed by UpdateEditRegion() to finalize market state
 * @see UpdateEditRegion(), SetupEconomy()
 */
void ARegion::SetupEditRegion()
{
    // Direct copy of SetupPop() except that it calls AddTown(AString*)
    TerrainType *typer = &(TerrainDefs[type]);
    habitat = typer->pop+1;
    // Population factor: 5 times
    if (habitat > 1) habitat *= 5;
    if ((habitat < 100) && (typer->similar_type != R_OCEAN)) habitat = 100;

    int pop = typer->pop;
    int mw = typer->wages;

    // fix economy when MAINTENANCE_COST has been adjusted
    mw += Globals->MAINTENANCE_COST - 10;
    if (mw < 0) mw = 0;

    if (pop == 0) {
        population = 0;
        basepopulation = 0;
        wages = 0;
        maxwages = 0;
        wealth = 0;
        return;
    }

    // Only select race here if it hasn't been set during Race Growth
    // in the World Creation process.
    if ((race == -1) || (!Globals->GROW_RACES)) {
        int noncoastalraces = sizeof(typer->races)/sizeof(typer->races[0]);
        int allraces =
            noncoastalraces + sizeof(typer->coastal_races)/sizeof(typer->coastal_races[0]);

        race = -1;
        while (race == -1 || (ItemDefs[race].flags & ItemType::DISABLED)) {
            int n = rng::get_random(IsCoastal() ? allraces : noncoastalraces);
            if (n > noncoastalraces-1) {
                race = typer->coastal_races[n-noncoastalraces-1];
            } else
                race = typer->races[n];
        }
    }

    habitat = habitat * 2/3 + rng::get_random(habitat/3);
    auto mt = find_race(ItemDefs[race].abr)->get();
    if (mt.terrain == typer->similar_type) {
        habitat = (habitat * 9)/8;
    }
    if (!IsNativeRace(race)) {
        habitat = (habitat * 4)/5;
    }
    basepopulation = habitat / 3;
    // hmm... somewhere not too far off equilibrium pop
    population = habitat * (60 + rng::get_random(6) + rng::get_random(6)) / 100;

    // Setup development
    int level = 1;
    development = 1;
    int prev = 0;
    while (level < mw) {
        development++;
        prev++;
        if (prev > level) {
            level++;
            prev = 0;
        }
    }
    development += rng::get_random(25);
    maxdevelopment = development;

    // Same filter as SetupPop — lakes/BARREN terrains are never valid settlement sites.
    bool town_eligible_terrain = (type != R_LAKE) &&
        !(TerrainDefs[type].flags & TerrainType::BARREN);

    if (Globals->TOWNS_EXIST && town_eligible_terrain) {
        int adjacent = 0;
        int prob = Globals->TOWN_PROBABILITY;
        if (prob < 1) prob = 100;
        int townch = (int) 80000 / prob;
        if (Globals->TOWNS_NOT_ADJACENT) {
            for (int d = 0; d < NDIRS; d++) {
                ARegion *newregion = neighbors[d];
                if ((newregion) && (newregion->town)) adjacent++;
            }
        }
        if (Globals->LESS_ARCTIC_TOWNS) {
            int dnorth = GetPoleDistance(D_NORTH);
            int dsouth = GetPoleDistance(D_SOUTH);
            if (dnorth < 9)
                townch = townch + 25 * (9 - dnorth) *
                    (9 - dnorth) * Globals->LESS_ARCTIC_TOWNS;
            if (dsouth < 9)
                townch = townch + 25 * (9 - dsouth) *
                    (9 - dsouth) * Globals->LESS_ARCTIC_TOWNS;
        }
        int spread = Globals->TOWN_SPREAD;
        if (spread > 100) spread = 100;
        int townprob = (TerrainDefs[type].economy * 4 * (100 - spread) +
            100 * spread) / 100;
        if (adjacent > 0) townprob = townprob * (100 - Globals->TOWNS_NOT_ADJACENT) / 100;
        std::string tname = AGetNameString(AGetName(1, this));
        if (rng::get_random(townch) < townprob) add_town(tname);
    }

    // set up work and entertainment income
    SetIncome();

    AddMenMarket();
    AddLeadersMarket();
}

void ARegion::UpdateProducts()
{
    for (auto& prod : products) {
        int lastbonus = prod->baseamount * 2;
        int bonus = 0;

        if (prod->itemtype == I_SILVER && prod->skill == -1) continue;

        for(const auto o : objects) {
            if (o->incomplete < 1 && ObjectDefs[o->type].productionAided == prod->itemtype) {
                lastbonus /= 2;
                bonus += lastbonus;
            }
        }
        prod->amount = prod->baseamount + bonus;

        if (prod->itemtype == I_GRAIN || prod->itemtype == I_LIVESTOCK) {
            prod->amount += ((earthlore + clearskies) * 40) / prod->baseamount;
        }
    }
}

// Permanently add `amount` to a product's baseamount in this region.
// If the product already exists, just increases its baseamount.
// If not, creates a new Production entry sized to `amount` only.
void ARegion::add_or_increase_product(int item, int amount)
{
    for (auto& prod : products) {
        if (prod->itemtype == item) {
            prod->baseamount += amount;
            prod->amount = prod->baseamount;  // UpdateProducts re-applies building bonuses next turn
            return;
        }
    }
    // New product: the chart reveals a fresh deposit of exactly `amount` size.
    // A discovery adds only the small bonus (1-2) — NOT a full natural-sized
    // deposit — so do not seed it with the terrain-default amount.
    Production *p = new Production(item, 0);  // ctor still sets itemtype + production skill
    p->baseamount = amount;
    p->amount = amount;
    products.push_back(p);
}

/* BaseDev is the development floor at which poor
 * regions stabilise without player activity */
int ARegion::BaseDev()
{
    int level = 1;
    int basedev = 1;
    int prev = 0;
    while (level <= TerrainDefs[type].wages) {
        prev++;
        basedev++;
        if (prev > level) {
            level++;
            prev = 0;
        }
    }

    basedev = (Globals->MAINTENANCE_COST + basedev) / 2;
    return basedev;
}

/* ProdDev is the development floor for regions
 * factoring in player production in the region */
int ARegion::ProdDev()
{
    int basedev = BaseDev();
    for (const auto& p : products) {
        if (ItemDefs[p->itemtype].type & IT_NORMAL && p->itemtype != I_SILVER) {
            basedev += p->activity;
        }
    }
    return basedev;
}

int ARegion::TownHabitat()
{
    // Effect of existing buildings
    int farm = 0;
    int inn = 0;
    int temple = 0;
    int caravan = 0;
    int fort = 0;
    for(const auto obj : objects) {
        if (ObjectDefs[obj->type].protect > fort) fort = ObjectDefs[obj->type].protect;
        if (ItemDefs[ObjectDefs[obj->type].productionAided].type & IT_FOOD) farm++;
        if (ObjectDefs[obj->type].productionAided == I_SILVER) inn++;
        if (ObjectDefs[obj->type].productionAided == I_HERBS) temple++;
        if (
            (ObjectDefs[obj->type].flags & ObjectType::TRANSPORT) &&
            (ItemDefs[ObjectDefs[obj->type].productionAided].type & IT_MOUNT)
        ) {
            caravan++;
        }
    }
    int hab = 2;
    int step = 0;
    for (int i=0; i<5; i++) {
        if (fort > step) hab++;
        step *= 5;
        if (step == 0) step = 10;
    }
    int build = 0;
    if (farm) build++;
    if (inn) build++;
    if (temple) build++;
    if (caravan) build++;
    if (build > 2) build = 2;

    build++;
    hab = (build * build + 1) * hab * hab + habitat / 4 + 50;

    // Effect of town development on habitat:
    int totalhab = hab + (TownDevelopment() * (habitat + 800 + hab
        + Globals->CITY_POP / 2)) / 100;

    return totalhab;
}

// Use this to determine development advantages
// due to connecting roads
int ARegion::RoadDevelopment()
{
    // Road bonus
    int roads = 0;
    for (int i=0; i<NDIRS; i++) {
        if (HasExitRoad(i)) roads++;
    }

    int dbonus = 0;
    if (roads > 0) {
        dbonus = RoadDevelopmentBonus(16, development);
        if (!town) dbonus = dbonus / 2;
    }
    // Maximum bonus of 45 to development for roads
    int bonus = 5;
    int leveloff = 1;
    int plateau = 4;
    int totalb = 0;
    while((dbonus > 0) && (totalb < 46)) {
        dbonus--;
        // reduce development adjustment gradually for large road bonuses
        if (leveloff >= plateau) if (bonus > 1) {
            bonus--;
            leveloff = 1;
            plateau--;
        }
        leveloff++;
        totalb += bonus;
    }
    return totalb;
}

// Measure of the economic development of a town
// on a scale of 0-100. Used for town growth/hab/limits
// and also for raising development through markets.
// Do NOT take roads into account as they are a bonus and
// considered outside of these limits.
int ARegion::TownDevelopment()
{
    int basedev = BaseDev();
    int df = development - basedev;
    if (df < 0) df = 0;
    if (df > 100) df = 100;

    return df;
}

/**
 * @brief Computes the effective food market supply for the current turn.
 *
 * Scales food market supply (GRAIN, LIVESTOCK, FISH) through three tiers
 * as town->pop grows. Uses town->pop as the growth metric (same as IT_TRADE).
 * The stored m->maxamt serves as the village-tier maximum (base).
 *
 * Tiers (based on CITY_POP = 10000):
 *   Village (0 → 2500):   minamt → maxamt        (base supply)
 *   Town    (2500 → 8000): maxamt → maxamt×2      (growing city)
 *   City    (8000 → 10000): maxamt×2 → maxamt×3   (major city)
 *
 * @param m  IT_FOOD M_SELL market. m->maxamt is the stored creation base.
 * @return   Effective amount available this turn.
 *
 * @see TownGrowth() — uses this for the tot denominator
 * @see PostTurn(), UpdateEditRegion() — apply this to m->amount each turn
 */
int ARegion::food_effective_amount(const Market* m) const {
    if (!town) return m->maxamt;
    const int p     = town->pop;
    const int p_vt  = Globals->CITY_POP / 4;       // 2500: village→town
    const int p_tc  = Globals->CITY_POP * 4 / 5;   // 8000: town→city
    const int p_max = Globals->CITY_POP;             // 10000: maximum
    const int base  = m->maxamt;

    if (p <= 0)    return m->minamt;
    if (p < p_vt)  return m->minamt + (base - m->minamt) * p / p_vt;
    if (p < p_tc)  return base + base * (p - p_vt) / (p_tc - p_vt);
    if (p < p_max) return base * 2 + base * (p - p_tc) / (p_max - p_tc);
    return base * 3;
}

/**
 * @brief Development-weight divisor for entertainment income, by town tier.
 *
 * Entertainment income contributes to town development as
 * (entertainment_silver / ENTERTAIN_FRACTION) / divisor. Entertainment costs no
 * faction points (unlike production), so it is given full weight for small
 * villages (a meaningful boost where it is most needed) and a heavily reduced
 * weight for large cities (so it is not a "free" path to high development).
 *
 * @param towntype TOWN_VILLAGE / TOWN_TOWN / TOWN_CITY
 * @return divisor: village 1, town 4, city 8 (defaults to 4 for unknown)
 * @see ARegion::TownGrowth()
 */
int entertainment_dev_divisor(int towntype)
{
    switch (towntype) {
        case TOWN_VILLAGE: return 1;
        case TOWN_TOWN:    return 4;
        case TOWN_CITY:    return 8;
        default:           return 4;
    }
}

/**
 * @brief Calculate target town population based on market trading activity
 *
 * Market terminology:
 * - M_BUY market: Player BUYS from town (report shows "For Sale")
 * - M_SELL market: Player SELLS to town (report shows "Wanted")
 *
 * Formula: tarpop = town->pop + (CITY_POP × amt / tot)
 * Where:
 *   amt = weighted sum of market activity
 *   tot = weighted sum of effective food capacity + trade capacity
 *
 * Market activity weights:
 *   M_BUY + IT_TRADE: 4×
 *   M_SELL + IT_FOOD: 2×
 *   M_SELL + other: 1×
 *
 * Tot (mandatory) calculation:
 *   M_BUY + IT_TRADE: 4× maxamt
 *   M_SELL + IT_FOOD (m->item != I_FISH): 2× food_effective_amount()
 *
 * Constraints:
 *   if (amt > tot) amt = tot
 *   if (tarpop > CITY_POP) tarpop = CITY_POP
 *
 * @return Target population for town growth calculation
 */
int ARegion::TownGrowth()
{
    int tarpop = town->pop;

    // Starting Cities don't grow (Nexus cities)
    if (!IsStartingCity()) {
        // amt = weighted sum of market activity (numerator)
        // tot = weighted sum of mandatory item capacity (denominator)
        int amt = 0;
        int tot = 0;
        for (const auto& m : markets) {
            // Only count markets if population exceeds minpop threshold
            if (Population() > m->minpop) {

                // M_BUY: Player BUYS from town (town SELLS)
                if (m->type == Market::MarketType::M_BUY) {
                    // Only IT_TRADE items count
                    if (ItemDefs[m->item].type & IT_TRADE) {
                        amt += 4 * m->activity;      // 4× weight
                        tot += 4 * m->maxamt;        // Count in denominator
                        improvement += 3 * amt;      // Generate development
                    }
                    // Non-trade M_BUY items don't affect growth

                // M_SELL: Player SELLS to town (town BUYS)
                } else { // m->type == M_SELL
                    // Add to amt based on item type
                    if (ItemDefs[m->item].type & IT_FOOD) {
                        // Food growth weight reduced 2x -> 1.5x. The tot
                        // denominator below stays at 2x, so food pulls ~75% of
                        // its former population growth per unit sold to town.
                        amt += (3 * m->activity) / 2;  // 1.5× weight for food
                    } else {
                        amt += m->activity;          // 1× weight for non-food
                    }

                    // Add to tot ONLY for mandatory food (NOT FISH!)
                    // GRAIN and LIVESTOCK are mandatory, FISH is not
                    // Use food_effective_amount() so tot scales with town tier,
                    // keeping the supply/demand ratio consistent at all town sizes.
                    if ((ItemDefs[m->item].type & IT_FOOD) && (m->item != I_FISH)) {
                        tot += 2 * food_effective_amount(m);
                    }

                    // Trade goods also generate development
                    if (ItemDefs[m->item].type & IT_TRADE) {
                        improvement += 3 * amt;  // Trade goods: 3× (highest)
                    }
                    // Food goods generate moderate development
                    else if (ItemDefs[m->item].type & IT_FOOD) {
                        improvement += amt / 3;  // Food goods: ~0.33×
                    }
                    // Other normal goods generate development
                    else if (ItemDefs[m->item].type & IT_NORMAL) {
                        improvement += amt;  // Normal goods: 1×
                    }
                }
            }
        }

        // Entertainment production contribution to improvement (size-dependent
        // weight, see entertainment_dev_divisor): entertainment helps small
        // settlements develop but is a weak lever for large cities.
        int ent_div = entertainment_dev_divisor(town->TownType());
        for (const auto& p : products) {
            if (p->itemtype == I_SILVER && p->skill == S_ENTERTAINMENT) {
                improvement += (p->activity / Globals->ENTERTAIN_FRACTION) / ent_div;
            }
        }

        // Cap: selling more than tot doesn't increase amt
        if (amt > tot) amt = tot;

        // Calculate target population: tarpop += (CITY_POP × amt / tot)
        if (tot) {
            tarpop += (Globals->CITY_POP * amt) / tot;
        }

        // Optional boost (commented out in code)
        // tarpop = (tarpop * 5) / 4;

        // DYNAMIC CAP: Based on habitat and max multiplier
        // Allows cities to grow beyond fixed 10000 limit
        // Cap grows with town development (via hab which reflects dev bonus)
        // Consistent with growth multiplier formula (2.0 + dev/100*2 = max 4.0)
        int max_tarpop = town->hab * 4;
        if (tarpop > max_tarpop) tarpop = max_tarpop;
    }
    return tarpop;
}

/* Damage region because of pillaging */

void ARegion::Pillage()
{
    wealth = 0;
    int damage = development / 3;
    development -= damage;
    if (Globals->DYNAMIC_POPULATION) {
        // Don't do population damage if population can't recover
        int popdensity = Globals->CITY_POP / 2000;
        AdjustPop(- damage * rng::get_random(popdensity) - rng::get_random(5 * popdensity));
    }
    /* Stabilise at minimal development levels */
    while (Wages() < Globals->MAINTENANCE_COST / 20) development += rng::get_random(5);
}


/**
 * @brief Calculate and apply population growth for region and town
 *
 * Two-phase growth:
 * 1. Regional growth (from production activity)
 * 2. Town growth (from TownGrowth result)
 *
 * Growth rate divisor (line 1436-1443):
 * - if (!VILLAGES_ONLY && DYNAMIC_POPULATION): tgrowth /= 4
 * - else: tgrowth /= 2
 *
 * Formulas:
 * Regional: dgrow = (adiff × habitat) / (5 × (habitat + 3×adiff))
 * Town: increase = tgrowth × (2×hab - pop), then divide by (10×hab)
 * Total: growpop = regional_dgrow + town_damped
 *
 * Distribution: AdjustPop(growpop)
 */
void ARegion::Grow()
{
    // Skip regions with no base population
    if (basepopulation == 0) return;

    // Total population growth accumulator (regional + town)
    int growpop = 0;

    // ===== MIGRATION PARAMETERS SETUP =====
    // immigrants = potential population that could enter
    // emigrants = excess population that could leave
    immigrants = habitat - basepopulation;
    emigrants = population - basepopulation;

    // ===== PHASE 1: REGIONAL POPULATION GROWTH (from production) =====

    // Sum up all production activity (PRODUCE orders executed)
    // IT_NORMAL items and entertainment production count
    int activity = 0;  // How much was actually produced this turn
    int amount = 0;    // Base production capacity (from products)

    for (const auto& p : products) {
        if (ItemDefs[p->itemtype].type & IT_NORMAL && p->itemtype != I_SILVER) {
            activity += p->activity;  // Accumulated in RunAProduction()
            amount += p->baseamount;  // Static base amount
        }
        // Entertainment production attracts population
        else if (p->itemtype == I_SILVER && p->skill == S_ENTERTAINMENT) {
            activity += p->activity / Globals->ENTERTAIN_FRACTION;  // Normalize to person-levels
            amount += p->baseamount;  // Already divided by ENTERTAIN_FRACTION in SetIncome()
        }
    }
    // Example: activity=33 (18 LIVESTOCK + 15 WOOD), amount=44 (baseamounts)

    // Calculate target regional population
    // Base formula: restore toward equilibrium (habitat vs population)
    int tarpop = habitat - population + basepopulation;

    // Add bonus from production activity
    // Formula: bonus = (habitat - basepopulation) × 2×activity / 3×amount
    // Higher activity/amount ratio → higher target population
    if (amount) {
        tarpop += ((habitat - basepopulation) * 2 * activity) / (3 * amount);
    }
    // Example: tarpop = 2408 - 1671 + 802 + ((2408-802)×2×33)/(3×44) = 2342

    // Growth/shrinkage amount
    int diff = tarpop - population;
    int adiff = abs(diff);

    // --- Basepopulation adjustment (long-term equilibrium point) ---

    // Increase basepop if sustained high production
    if (diff > (basepopulation / 20)) {
        int gpop = rng::get_random(Globals->CITY_POP / 600);  // Random 0-16
        if (gpop > diff / 20) gpop = diff / 20;
        int relativeg = basepopulation * gpop / 1000;
        if (diff > habitat / 20) {
            basepopulation += gpop + relativeg;
        }
    }

    // Decrease basepop if population is very low
    if (population < basepopulation) {
        int depop = (basepopulation - population) / 4;
        basepopulation -= depop + rng::get_random(depop);
    }

    // --- Calculate actual regional growth with damping ---

    // Damping formula prevents explosive growth
    // grow2 = denominator for damping calculation
    long int grow2 = 5 * ((long int) habitat + (3 * (long int) adiff));

    // dgrow = actual growth amount (heavily damped)
    // Formula: dgrow = (adiff × habitat) / grow2
    // Example: dgrow = (671 × 2408) / 22105 = 73
    long int dgrow = ((long int) adiff * (long int) habitat) / grow2;

    // Apply growth or shrinkage to regional population
    if (diff < 0) growpop -= (int) dgrow;  // Shrinking
    if (diff > 0) growpop += (int) dgrow;  // Growing

    // Update emigration potential (for migration system)
    if (emigrants > 0) emigrants += diff;

    // ===== PHASE 2: TOWN POPULATION GROWTH (from trading) =====

    if (town) {
        // Always recalculate hab/dev from current buildings + development.
        // town->hab is set at world-gen via SetTownType() and never updated
        // otherwise, causing AdjustPop to route all growth to the region.
        town->hab = TownHabitat();
        town->dev = TownDevelopment();

        // Local overflow migration: when the region is overcrowded but the
        // town has capacity, move 1/10 of the excess into the town each turn.
        // This rebalances the abnormal state caused by a stale town->hab and
        // ensures the town reaches its growth threshold within 1-2 turns.
        {
            int tspace = town->hab - town->pop;
            int rspace = habitat - population;
            if (rspace < 0 && tspace > 0) {
                int migrate = std::min(-rspace, tspace) / 10;
                if (migrate > 0) {
                    town->pop += migrate;
                    population -= migrate;
                }
            }
        }

        // Get target population from market activity (see TownGrowth())
        int maxpop = TownGrowth();

        // Growth potential = difference between target and current
        int tgrowth = maxpop - town->pop;

        // Track immigration potential
        immigrants += tgrowth;

        // --- Apply growth rate divisor ---
        // CRITICAL: Rate depends on VILLAGES_ONLY setting

        if (!Globals->VILLAGES_ONLY && Globals->DYNAMIC_POPULATION) {
            // Condition: VILLAGES_ONLY=0 AND DYNAMIC_POPULATION=1
            // More aggressive slowdown (÷4) for dynamic worlds
            tgrowth = tgrowth / 4;
        } else {
            // Condition: VILLAGES_ONLY=1 OR DYNAMIC_POPULATION=0
            // Moderate slowdown (÷2)
            // NewOrigins uses this (VILLAGES_ONLY=1, DYNAMIC_POPULATION=1)
            tgrowth = tgrowth / 2;
        }
        // Example NewOrigins: tgrowth = 1148 / 2 = 574

        // --- Apply damping based on town habitat capacity ---
        // Damping increases as town->pop approaches town->hab

        // DYNAMIC MULTIPLIER: Based on town development
        // Formula: multiplier = 2.0 + (dev / 100.0) × 2.0
        // Range: 2.0 (dev=0) to 4.0 (dev=100+)
        // Effect: Higher development → higher population capacity
        float multiplier = 2.0 + (town->dev / 100.0) * 2.0;

        // Numerator: tgrowth × (multiplier×hab - pop)
        // When pop << hab: factor ≈ multiplier×hab
        // When pop = hab: factor = (multiplier-1)×hab
        // When pop → multiplier×hab: factor → 0 (growth stops)
        float increase = tgrowth * (multiplier * town->hab - town->pop);

        // Denominator: 10 × hab
        float limitingfactor = (10 * town->hab);

        // Final damped growth added to total
        // Example: (574 × 4219) / 31640 = 76
        growpop += (int) (increase / limitingfactor);
    }
    // At this point: growpop = regional_growth + town_growth_damped

    // ===== APPLY TOTAL GROWTH =====
    // AdjustPop() distributes growpop between town->pop and region->population
    // based on available space in each (proportional allocation)
    AdjustPop(growpop);

    // ===== GENTLE TOWN DECLINE (FLOOR AT HABITAT) =====
    // A town shrinks slightly only when there was NO real development effort in the
    // region this turn. "Real effort" = producing goods (PRODUCE) OR selling real
    // goods (food / trade / other normal goods) to the town (M_SELL). Entertainment
    // is deliberately EXCLUDED: it costs no faction points, so otherwise nearly every
    // town would always count as active. Work/tax/recruitment also do not count.
    bool dev_effort = false;
    if (town) {
        for (const auto& p : products) {
            if ((ItemDefs[p->itemtype].type & IT_NORMAL) && p->itemtype != I_SILVER
                    && p->activity > 0) { dev_effort = true; break; }
        }
        if (!dev_effort) {
            for (const auto& m : markets) {
                if (m->type == Market::MarketType::M_SELL && m->activity > 0
                        && (ItemDefs[m->item].type & (IT_FOOD | IT_TRADE | IT_NORMAL))) {
                    dev_effort = true; break;
                }
            }
        }
    }
    // Decline erodes only the player-inflated EXCESS above town->hab (the size the
    // town's development/buildings naturally support; fresh towns are generated at
    // 2/3 hab, so they are below the floor and never decay). It stops at hab, so a
    // city stays a city — only PILLAGE (which cuts development/hab) can demote a tier.
    // Decay-only: we never raise population (so it can't "heal" pillage damage).
    if (town && !dev_effort && town->pop > town->hab) {
        const int TOWN_DECAY_DIVISOR = 200;   // ~0.5% of population per idle turn
        int decayed = town->pop - town->pop / TOWN_DECAY_DIVISOR;
        if (decayed < town->hab) decayed = town->hab;
        if (decayed < town->pop) town->pop = decayed;
    }

    // Reset migration tracking variables
    migdev = 0;
    migfrom.clear();
}


/* Performs a search for each round of Migration for
 * the most attractive valid target region within
 * 2 hexes distance. */
void ARegion::FindMigrationDestination(int round)
{
    // is emigration possible?
    if (emigrants < 0) return;

    int maxattract = 0;
    ARegion *target = this;
    // Check all hexes within 2 hexes
    // range one neighbours
    for (int d=0; d < NDIRS; d++) {
        ARegion *nb = neighbors[d];
        if (!nb) continue;
        if (TerrainDefs[nb->type].similar_type == R_OCEAN) continue;
        int ma = nb->MigrationAttractiveness(development, 1, round);
        // check that we didn't migrate there in previous round
        if ((ma > maxattract) &&
            (!((nb->xloc == target->xloc) && (nb->yloc == target->yloc)))) {
            // set migration target
            target = nb;
            maxattract = ma;
        }
        // range two neighbours
        for (int d2=0; d2 < NDIRS; d2++) {
            ARegion *nb2 = nb->neighbors[d2];
            if (!nb2) continue;
            if (TerrainDefs[nb2->type].similar_type == R_OCEAN) continue;
            ma = nb2->MigrationAttractiveness(development, 2, round);
            // check that we didn't migrate there the previous round
            if ((ma > maxattract) &&
                (!((nb2->xloc == target->xloc) && (nb2->yloc == target->yloc)))) {
                // set migration target
                target = nb2;
                maxattract = ma;
            }
        }
    }
    // do we have a target?
    if (target == this) return;

    // then add this region to the target's migfrom list
    ARegion *self = this;
    target->migfrom.push_back(self);
}

/* Attractiveness of the region as a destination for migrants */
int ARegion::MigrationAttractiveness(int homedev, int range, int round)
{
    int attractiveness = 0;
    int mdev = development;
    /* Is there enough immigration capacity? */
    if (immigrants < 100) return 0;
    /* on the second round, consider as a mid-way target */
    if (round > 1) mdev = migdev;
    /* minimum development difference 8 x range */
    mdev -= 8 * range;
    if (mdev <= homedev) return 0;
    /* available entertainment */
    Production *p = get_production_for_skill(I_SILVER, S_ENTERTAINMENT);
    int entertain = p->activity / 20;
    /* available space */
    float space = 1 / 2;
    int offset = Globals->CITY_POP / 100;
    if (town) {
        space += ((habitat - population) + (town->hab - town->pop) + offset)
            / (habitat + town->hab + offset);
    } else {
        space += (habitat - population + offset) / (habitat + offset);
    }
    /* attractiveness due to development */
    attractiveness += (int) (space * ((float) 100 * (mdev - homedev) / homedev + entertain));

    return attractiveness;
}

/* Performs migration for each region with a migration
 * route pointing to the region (i.e. element of migfrom list),
 * adjusting population for hex of origin and itself */
void ARegion::Migrate()
{
    // calculate total potential migrants
    int totalmig = 0;
    for(const auto r : migfrom) {
        if (!r) continue;
        totalmig += r->emigrants;
    }

    // is there any migration to perform?
    if (totalmig == 0) return;

    // do each migration
    int totalimm = 0;
    for (auto r : migfrom) {
        if (!r) continue;

        // figure range
        int xdist = r->xloc - xloc;
        if (xdist < 0) xdist = - xdist;
        int ydist = r->yloc - yloc;
        if (ydist < 0) ydist = - ydist;
        ydist = (ydist - xdist) / 2;
        int range = xdist + ydist;

        // sanity check - huh?
        if (range < 1) continue;
        int migrants = (int) (immigrants * ((float) (r->emigrants / totalmig)));
        int mdiff = development - 7 - r->development;
        mdiff -= 8 * (range - 1);
        if (mdiff < 0) continue;
        int mmult = 1;
        for (int x=1; x*x < mdiff; x++) mmult = x;
        // adjust migrants according to development difference
        migrants = (int) (migrants * (float) (((mdiff + 100) * mmult) / 500));
        AdjustPop(migrants);
        r->AdjustPop(-migrants);
        r->emigrants -= migrants;
        totalimm += migrants;
        std::string wout = "Migrating from " + std::to_string(r->xloc) + "," + std::to_string(r->yloc) + " to " +
            std::to_string(xloc) + "," + std::to_string(yloc) + ": " + std::to_string(migrants) + " migrants.";
        logger::write(wout);
        // set the region's mid-way migration development
        r->migdev = (development - 8 * (range-1) + r->development) / 2;
        if (r->development > migdev) r->migdev = r->development;
    }
    // reduce possible immigrants
    immigrants -= totalimm;
    // clear migfrom
    migfrom.clear();
}

/**
 * @brief End-of-turn region maintenance and economy updates
 *
 * Called after all orders are processed. Updates:
 * - Building decay
 * - Development (player activity + recovery for poor regions)
 * - Starting city markets (if city was captured)
 * - Wages and entertainment income
 * - All market quantities/prices via Market::post_turn()
 * - Wilderness leader chance (35% roll for regions without a settlement)
 * - Production resources
 * - Unit PostTurn processing
 *
 * @note This is where Market::post_turn() recalculates recruitment markets
 * @note After market update, regions without a settlement apply a 35% roll
 *       for leader availability (WILDERNESS_LEADER_CHANCE local constant)
 * @see Market::post_turn(), UpdateProducts(), SetIncome(), AddLeadersMarket()
 */
void ARegion::PostTurn()
{

    /* Check decay */
    if (Globals->DECAY) DoDecayCheck();

    /* Development increase due to player activity */
    // scale improvement
    float imp1 = improvement / 25;
    int imp2 = (improvement * 2 + 15) / 3;
    improvement = (int) (imp1 * imp2);
    // development increase possible?
    if (improvement > development) {
        int diff = improvement - development;
        /* Let road development increase chance of improvement */
        int progress = development - RoadDevelopment();
        /* Three chances to improve */
        for (int a=0; a<3; a++) if (rng::get_random(progress) < diff) development++;
        if (development > maxdevelopment) maxdevelopment = development;
    }

    /* Development increase for very poor regions */
    int recoveryRounds = 1 + earthlore + clearskies;

    if (wealth > 0) recoveryRounds++;

    while (recoveryRounds-- > 0) {
        if (maxdevelopment > development) {
            if (rng::get_random(maxdevelopment) > development) development++;
        }
        if (maxdevelopment > development) {
            if (rng::get_random(maxdevelopment) > development) development++;
        }
        if (maxdevelopment > development) {
            if (rng::get_random(3) == 1) development++;
        }
    }

    /* Check if we were a starting city and got taken over */
    if (IsStartingCity() && !HasCityGuard() && !Globals->SAFE_START_CITIES) {
        // Make sure we haven't already been modified.
        int done = 1;
        for (const auto& m : markets) {
            if (m->minamt == -1) {
                done = 0;
                break;
            }
        }

        if (!done) {
            for (auto& m : markets) delete m; // Free the allocated object
            markets.clear(); // empty the vector.
            SetupCityMarket();
            SetupRandomTradeMarkets();
            AddMenMarket();
            AddLeadersMarket();
        }
    }

    /* Set wage income and entertainment */
    if (type != R_NEXUS) {
        SetIncome();
    }

    /* update markets */
    for (auto& m : markets) {
        // Trade goods and food use town->pop; other markets use total Population()
        int pop = (town && (ItemDefs[m->item].type & (IT_TRADE | IT_FOOD))) ? town->pop : Population();
        m->post_turn(pop, Wages());
        // Food markets scale supply through village/town/city tiers based on town->pop
        if (town && (ItemDefs[m->item].type & IT_FOOD) && m->type == Market::MarketType::M_SELL)
            m->amount = food_effective_amount(m);
    }

    // Wilderness leader availability: in regions without a settlement, leaders are
    // not permanently present — they appear with 35% probability per turn
    // (wandering leaders "passing through"). In settlements supply is stable.
    if (!town && Globals->LEADERS_EXIST) {
        constexpr int WILDERNESS_LEADER_CHANCE = 35; // % chance leaders are available this turn
        if (rng::get_random(100) >= WILDERNESS_LEADER_CHANCE) {
            for (auto& m : markets)
                if (ItemDefs[m->item].type & IT_LEADER)
                    m->amount = 0;
        }
    }

    /* update resources */
    UpdateProducts();

    // Set these guys to 0.
    earthlore = 0;
    clearskies = 0;

    for(const auto o : objects) {
        for(const auto u : o->units) {
            u->PostTurn(this);
        }
    }
}
