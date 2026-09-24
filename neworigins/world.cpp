#include "game.h"
#include "gamedata.h"
#include <cmath>
#include <algorithm>
#include <string.h>
#include "../string_filters.hpp"
#include <iostream>
#include <fstream>

using namespace std;

// Helper functions for interactive parameter input
int ask_parameter(const std::string& prompt, int default_val, int min_val, int max_val) {
    while (true) {
        logger::write(prompt + " [" + std::to_string(default_val) + "]: ");
        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) return default_val;

        try {
            int val = std::stoi(input);
            if (val >= min_val && val <= max_val) return val;
            logger::write("Value must be between " + std::to_string(min_val) +
                         " and " + std::to_string(max_val));
        } catch (...) {
            logger::write("Invalid number");
        }
    }
}

float ask_parameter_float(const std::string& prompt, float default_val, float min_val, float max_val) {
    while (true) {
        logger::write(prompt + " [" + std::to_string(default_val) + "]: ");
        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) return default_val;

        try {
            float val = std::stof(input);
            if (val >= min_val && val <= max_val) return val;
            logger::write("Value must be between " + std::to_string(min_val) +
                         " and " + std::to_string(max_val));
        } catch (...) {
            logger::write("Invalid number");
        }
    }
}

typedef struct
{
    const std::string word;
    int prob;
} WordList;

// Initial Consonant, Vowel and Final Consonant sequences and
// probabilities were derived by analysis of a convenient unix
// (English) dictionary

WordList ic[] =
{
{ "c", 1804 }, { "m", 1418 }, { "p", 1282 }, { "b", 1272 }, { "s", 1254 },
{ "d", 1251 }, { "h", 1115 }, { "r", 918 }, { "l", 901 }, { "t", 773 },
{ "f", 699 }, { "g", 596 }, { "w", 543 }, { "n", 518 }, { "v", 415 },
{ "pr", 400 }, { "st", 333 }, { "tr", 326 }, { "ch", 309 }, { "j", 300 },
{ "br", 255 }, { "k", 240 }, { "sh", 228 }, { "gr", 216 }, { "cr", 214 },
{ "sp", 183 }, { "th", 171 }, { "cl", 165 }, { "fr", 164 }, { "fl", 160 },
{ "pl", 151 }, { "bl", 147 }, { "qu", 136 }, { "wh", 117 }, { "sc", 111 },
{ "dr", 105 }, { "sl", 104 }, { "y", 100 }, { "gl", 97 }, { "sw", 95 },
{ "ph", 95 }, { "str", 89 }, { "sn", 62 }, { "z", 60 },
{ "sk", 54 }, { "scr", 51 }, { "sch", 46 }, { "kn", 45 }, { "sm", 43 },
{ "wr", 41 }, { "thr", 39 }, { "chr", 38 }, { "squ", 36 }, { "rh", 34 },
{ "tw", 33 }, { "ps", 33 }, { "shr", 26 }, { "spr", 25 }, { "kr", 19 },
{ "spl", 17 }, { "my", 11 }, { "x", 10 }, { "gn", 10 }
};

WordList v[] =
{
{ "e", 1681 }, { "a", 1356 }, { "i", 1045 }, { "o", 931 }, { "u", 410 },
{ "y", 189 }, { "ia", 93 }, { "ea", 86 }, { "io", 82 }, { "ou", 78 },
{ "oo", 51 }, { "ee", 49 }, { "ai", 47 }, { "ie", 46 }, { "au", 34 },
{ "oa", 25 }, { "oi", 22 }, { "ei", 22 }, { "eo", 17 }, { "ue", 15 },
{ "eu", 15 }, { "iu", 15 }, { "iou", 15 }, { "ua", 14 }, { "ui", 12 },
{ "oe", 12 }, { "ae", 6 }
};

WordList fc[] =
{
{ "n", 2642 }, { "r", 1496 }, { "l", 1254 }, { "s", 1064 }, { "t", 763 },
{ "c", 760 }, { "nt", 685 }, { "d", 583 }, { "m", 538 }, { "ng", 389 },
{ "y", 370 }, { "nd", 300 }, { "st", 278 }, { "p", 259 }, { "sh", 252 },
{ "rd", 249 }, { "ck", 244 }, { "ll", 229 }, { "w", 193 }, { "ss", 170 },
{ "rt", 157 }, { "ld", 134 }, { "x", 128 }, { "g", 127 }, { "th", 125 },
{ "ct", 125 }, { "k", 114 }, { "rn", 97 }, { "ght", 93 }, { "sm", 90 },
{ "b", 86 }, { "rk", 84 }, { "ch", 81 }, { "nk", 76 }, { "ff", 73 },
{ "rm", 66 }, { "wn", 65 }, { "lt", 64 }, { "tt", 58 }, { "tch", 54 },
{ "f", 49 }, { "h", 47 }, { "mp", 46 }, { "rg", 44 }, { "ft", 44 },
{ "pt", 41 }, { "gh", 40 }, { "nch", 39 }, { "ns", 37 }, { "ph", 29 },
{ "lk", 29 }, { "z", 28 }, { "rth", 26 }, { "sk", 23 }, { "wl", 22 },
{ "rs", 22 }, { "nn", 22 }, { "mb", 22 }, { "rch", 21 }, { "lm", 20 },
{ "tz", 19 }, { "rl", 19 }, { "nth", 19 }, { "lf", 19 }, { "v", 18 },
{ "rb", 18 }, { "gn", 16 }, { "rst", 14 }, { "nct", 13 }, { "rp", 12 },
{ "sp", 11 }, { "rr", 11 }
};

typedef struct
{
    int terrain;
    const std::string word;
    int prob;
    int town;
    int port;
} SuffixList;

SuffixList ts[] =
{
{ -1,       "acre",      3, 0, 0 },
{ -1,       "bach",      3, 0, 0 },
{ -2,       "bank",     10, 0, 1 },
{ -2,       "bay",      10, 0, 1 },
{ R_MOUNTAIN,   "berg",     10, 0, 0 },
{ -2,       "borough",   4, 1, 0 },
{ R_PLAIN,  "bost",     10, 0, 0 },
{ -1,       "brook",     3, 0, 0 },
{ -2,       "bruk",      4, 1, 0 },
{ -2,       "burg",      4, 1, 0 },
{ -1,       "burn",      3, 0, 0 },
{ -2,       "bury",      4, 1, 0 },
{ -2,       "by",        4, 1, 0 },
{ R_FOREST, "cot",      10, 0, 0 },
{ -2,       "dale",      4, 1, 0 },
{ R_JUNGLE, "dee",      10, 0, 0 },
{ R_MOUNTAIN,   "del",      10, 0, 0 },
{ R_DESERT, "dhan",     10, 0, 0 },
{ R_MOUNTAIN,   "don",      10, 0, 0 },
{ -2,       "dorf",      4, 1, 0 },
{ R_SWAMP,  "fel",      10, 0, 0 },
{ R_PLAIN,  "field",    10, 0, 0 },
{ R_FOREST, "firth",    10, 0, 0 },
{ -1,       "folk",      3, 0, 0 },
{ -1,       "ford",      3, 0, 0 },
{ -1,       "gate",      3, 0, 0 },
{ R_MOUNTAIN,   "gill",     10, 0, 0 },
{ -1,       "glen",      3, 0, 0 },
{ R_DESERT, "gobi",     10, 0, 0 },
{ R_JUNGLE, "gol",       3, 0, 0 },
{ -2,       "gost",      4, 1, 0 },
{ -2,       "grad",      4, 1, 0 },
{ -2,       "grave",     4, 1, 0 },
{ R_FOREST, "grove",    10, 0, 0 },
{ R_SWAMP,  "gwern",    10, 0, 0 },
{ -2,       "ham",       4, 1, 0 },
{ -2,       "haven",    10, 0, 1 },
{ R_MOUNTAIN,   "head",     10, 0, 0 },
{ R_TUNDRA, "heath",    10, 0, 0 },
{ -1,       "heim",      3, 0, 0 },
{ -2,       "hold",      4, 1, 0 },
{ -2,       "holm",      4, 1, 0 },
{ R_FOREST, "hurst",    10, 0, 0 },
{ -2,       "kirk",      4, 1, 0 },
{ R_DESERT, "kum",      10, 0, 0 },
{ -1,       "land",      3, 0, 0 },
{ R_FOREST, "lea",       2, 0, 0 },
{ R_FOREST, "lee",       2, 0, 0 },
{ R_FOREST, "leigh",     2, 0, 0 },
{ R_FOREST, "ley",       2, 0, 0 },
{ R_TUNDRA, "ling",     10, 0, 0 },
{ R_OCEAN,  "loch",     15, 0, 0 },
{ R_FOREST, "ly",        2, 0, 0 },
{ R_OCEAN,  "mar",      10, 0, 0 },
{ -1,       "mark",      3, 0, 0 },
{ R_OCEAN,  "mere",     10, 0, 0 },
{ -2,       "minster",   4, 1, 0 },
{ R_MOUNTAIN,   "mont",     10, 0, 0 },
{ R_SWAMP,  "moor",     10, 0, 0 },
{ R_SWAMP,  "more",     10, 0, 0 },
{ R_SWAMP,  "moss",     10, 0, 0 },
{ -2,       "mouth",    10, 0, 1 },
{ -2,       "pest",      4, 1, 0 },
{ -2,       "pol",       4, 1, 0 },
{ -2,       "pool",     10, 0, 1 },
{ -2,       "port",     10, 0, 1 },
{ R_FOREST, "rath",     10, 0, 0 },
{ R_PLAIN,  "run",      10, 0, 0 },
{ -2,       "sale",      4, 1, 0 },
{ R_DESERT, "sand",     10, 0, 0 },
{ R_FOREST, "shaw",     10, 0, 0 },
{ R_PLAIN,  "shire",    10, 0, 0 },
{ -2,       "side",     10, 0, 1 },
{ -2,       "stad",      4, 1, 0 },
{ -1,       "stan",      3, 0, 0 },
{ -2,       "stead",     4, 1, 0 },
{ -2,       "stoke",     4, 1, 0 },
{ -2,       "stowe",     4, 1, 0 },
{ R_OCEAN,  "tarn",     15, 0, 0 },
{ R_MOUNTAIN,   "tell",     10, 0, 0 },
{ -2,       "ton",       4, 1, 0 },
{ R_MOUNTAIN,   "tor",      10, 0, 0 },
{ -2,       "town",      4, 1, 0 },
{ -1,       "vale",      3, 0, 0 },
{ -2,       "ville",     4, 1, 0 },
{ R_JUNGLE, "wald",     10, 0, 0 },
{ R_OCEAN,  "water",    15, 0, 0 },
{ -1,       "way",       3, 0, 0 },
{ -2,       "wick",      4, 1, 0 },
{ R_FOREST, "wood",     10, 0, 0 },
};

int syllprob[] = { 0, 60, 40, 0 };

//
// The following stuff is just for this file, to setup the names during
// world generation
//

static std::vector<std::string> regionnames;
static int nnames;
static int ntowns;
static int nregions;
static int tSyll, tIC, tV, tFC;

void SetupNames()
{
    unsigned int i;

    nnames = 0;
    ntowns = 0;
    nregions = 0;

    for (i = 0, tIC = 0; i < sizeof(ic) / sizeof(ic[0]); i++)
        tIC += ic[i].prob;
    for (i = 0, tV = 0; i < sizeof(v) / sizeof(v[0]); i++)
        tV += v[i].prob;
    for (i = 0, tFC = 0; i < sizeof(fc) / sizeof(fc[0]); i++)
        tFC += fc[i].prob;
    for (i = 0, tSyll = 0; i < sizeof(syllprob) / sizeof(syllprob[0]); i++)
        tSyll += syllprob[i];
}

void CountNames()
{
    logger::write("Regions " + std::to_string(nregions));

    // Dump all the names we created to a file so the GM can scan
    // them easily (to check for randomly generated rude words,
    // for example)
    ofstream names("names.out", ios::out|ios::ate);
    for(const auto& name: regionnames) {
        names << name << '\n';
    }
}

int AGetName(int town, ARegion *reg)
{
    int unique, rnd, syllables, i, trail, port, similar;
    unsigned int u;
    std::string temp;

    port = 0;
    if (town) {
        for (i = 0; i < NDIRS; i++)
            if (reg->neighbors[i] &&
                    TerrainDefs[reg->neighbors[i]->type].similar_type == R_OCEAN)
                port = 1;
    }

    unique = 0;
    while (!unique) {
        rnd = rng::get_random(tSyll);
        for (syllables = 0; rnd >= syllprob[syllables]; syllables++) rnd -= syllprob[syllables];
        syllables++;
        temp = "";
        trail = 0;
        while (syllables-- > 0) {
            if (!syllables) {
                // Might replace the last syllable with a
                // terrain specific suffix
                rnd = rng::get_random(400);
                similar = TerrainDefs[reg->type].similar_type;
                // Use forest names for underforest
                if (similar == R_UFOREST) similar = R_FOREST;
                // ocean (water) names for lakes
                if (similar == R_LAKE) similar = R_OCEAN;
                // and plains names for cavern
                if (similar == R_CAVERN) similar = R_PLAIN;
                for (u = 0; u < sizeof(ts) / sizeof(ts[0]); u++) {
                    if (ts[u].terrain == similar || ts[u].terrain == -1 || (ts[u].town && town) || (ts[u].port && port)) {
                        if (rnd >= ts[u].prob) rnd -= ts[u].prob;
                        else {
                            if (trail) {
                                switch(ts[u].word[0]) {
                                    case 'a':
                                    case 'e':
                                    case 'i':
                                    case 'o':
                                    case 'u':
                                        temp += "'";
                                        break;
                                    default:
                                        break;
                                }
                            }
                            temp += ts[u].word;
                            break;
                        }
                    }
                }
                if (u < sizeof(ts) / sizeof(ts[0]))
                    break;
            }
            if (rng::get_random(5) > 0) {
                // 4 out of 5 syllables start with a consonant sequence
                rnd = rng::get_random(tIC);
                for (i = 0; rnd >= ic[i].prob; i++) rnd -= ic[i].prob;
                temp += ic[i].word;
            } else if (trail) {
                // separate adjacent vowels
                temp += "'";
            }
            // All syllables have a vowel sequence
            rnd = rng::get_random(tV);
            for (i = 0; rnd >= v[i].prob; i++) rnd -= v[i].prob;
            temp += v[i].word;
            if (rng::get_random(5) > 1) {
                // 3 out of 5 syllables end with a consonant sequence
                rnd = rng::get_random(tFC);
                for (i = 0; rnd >= fc[i].prob; i++) rnd -= fc[i].prob;
                temp += fc[i].word;
                trail = 0;
            } else {
                trail = 1;
            }
        }
        temp = temp | filter::capitalize;
        unique = 1;
        for(const auto& name: regionnames) {
            if (name == temp) {
                unique = 0;
                break;
            }
        }
        if (temp.length() > 12) unique = 0;
    }

    nnames++;
    if (town) ntowns++;
    else nregions++;

    regionnames.push_back(temp);

    return regionnames.size();
}

const std::string& AGetNameString(int name)
{
    static const std::string errorName = "Error";
    if (name <= 0 || name > (int) regionnames.size()) return errorName;
    return regionnames[name-1];
}

/**
 * @brief Size of an underground level derived from a surface size and a scale.
 *
 * Rounds to an even number of regions: regions exist only where x + y is even,
 * so an odd size would leave a half-empty edge column or row.
 *
 * @param surface Surface size in regions (width or height)
 * @param scale Shrink factor from GetLevelXScale()/GetLevelYScale()
 * @return Level size in regions, at least 8
 * @see ARegionList::GetLevelXScale()
 */
static int level_size(int surface, double scale)
{
    int size = (int) lround(surface / scale);
    if (size % 2) size--;
    return std::max(8, size);
}

void Game::CreateWorld()
{
    int nx = 0;
    int ny = 1;
    parser::string_parser parser;
    if (Globals->MULTI_HEX_NEXUS) {
        ny = 2;
        while(nx <= 0) {
            logger::write("How many hexes should the nexus region be?");
            std::cin >> parser;
            auto token = parser.get_token();
            nx = token.get_number().value_or(-1);
            if (nx <= 0) continue;
            if (nx == 1) ny = 1;
            else if (nx % 2) {
                nx = 0;
                logger::write("The width must be a multiple of 2.");
            }
        }
    } else {
        nx = 1;
    }

    int generator = -1;
    while (generator < 1 || generator > 3) {
        logger::write("Selected surface land generator? [Original - 1, Parametrical - 2, Island Ring - 3]");
        std::cin >> parser;
        auto token = parser.get_token();
        generator = token.get_number().value_or(-1);
    }

    int xx = 0;
    while (xx <= 0) {
        logger::write("How wide should the map be? ");
        std::cin >> parser;
        auto token = parser.get_token();
        xx = token.get_number().value_or(-1);
        if (xx <= 0) continue;
        if (xx % 8) {
            xx = 0;
            logger::write("The width must be a multiple of 8.");
        }
    }
    int yy = 0;
    while (yy <= 0) {
        logger::write("How tall should the map be? ");
        std::cin >> parser;
        auto token = parser.get_token();
        yy = token.get_number().value_or(-1);
        if (yy <= 0) continue;
        if (yy % 8) {
            yy = 0;
            logger::write("The height must be a multiple of 8.");
        }
    }

    regions.create_levels(2 + Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS +
                          Globals->ABYSS_LEVEL + Globals->DUNGEON_LEVEL);

    SetupNames();

    regions.create_nexus_level(0, nx, ny, "nexus");
    if (generator == 1) {
        regions.create_surface_level(1, xx, yy, "");
    } else if (generator == 2) {
        Map* map = new Map(xx * 2, yy * 2);

        // Default parameters (can be changed before compilation or at runtime)
        int default_minTemp = -45;
        int default_maxTemp = 45;
        // Noise shape (frequency, octaves, persistence). Frequency sets how many
        // separate landmasses and mountain groups there are, the octaves add finer
        // detail on top, and persistence is how loud that detail is.
        //
        // Retuned 2026-09-23 for 64x64 with the noise aspect correction in place:
        // that correction slows the noise down north-south by 1.73, so the old 3.6
        // produced blobs so large that only 1 world in 60 could seat five villages
        // per terrain. The share of usable worlds peaks at 5.2 and falls off on both
        // sides; lower values merge the land into fewer, larger masses. Octaves 5 with persistence 0.6 break the mountain
        // ridges apart (18-23 separate massifs instead of 12-14, biggest 12-17 hexes
        // instead of 24-27), which is what lifts mountain villages from 4.5 to 5.0
        // against the floor of 5. Persistence 0.7 goes too far: the coast gets
        // ragged but the map fills with one-hex islets (24 per world against 12).
        float default_frequency = 5.2;
        int   default_octaves = 5;
        float default_lacunarity = 2.0;
        float default_persistence = 0.6;
        float default_evoparation = 0.91;    // Rainfall; swamps sit right at the settlement floor below this
        // Share of the surface under water. Above 0.60 fewer worlds seat five villages
        // per terrain: the extra sea eats the cold high-latitude land tundra needs.
        float default_waterPercent = 0.60;
        float default_mountainPercent = 0.08;  // mountain + hill
        float default_hillPercent = 0.57;      // hill share within the mountain block
        float default_lakePercent = 0.15;  // 15% chance for lake placement

        // Polar block: applied after the mask, above polarLatitudeStart only.
        // Gate + ramp steepness: amount = (lat-this)/(90-this). >=89 disables the whole
        // block. It is the only thing that ends the land on the polar side (the mask's
        // plateau runs from landEdgeLatitude to the pole), so it must stay above
        // landEdgeLatitude or it drowns the plateau; measured refusals 0% at 80, 40% at
        // 72.
        //
        // 82 makes the block cover three region rows per pole on a 64-row map
        // (rows 0-2 sit at 90, 87.1 and 84.3 degrees; row 3 is 81.4). Measured
        // 2026-09-23 over 40 worlds against 80 (four rows): the outermost row is
        // open water at both poles in every accepted world, the second row in
        // about half of them, and one row less of drowned land leaves enough
        // ground that 17 of 40 worlds pass the settlement verdict instead of 12.
        float default_polarLatitudeStart = 82.0;
        // Polar submersion, elevation *= (1 - amount*this): sets the sea-lane width.
        // On 64x48: 0.40 clears the outermost row, 0.50 clears two rows, above 0.60 only
        // the third row thins. Row counts scale with map height.
        float default_polarElevationRedux = 0.50;

        // Latitude-profile mask (macro-geography bias blended into elevation)
        float default_landEdgeLatitude = 33.0; // Latitude where the land ends toward the equator (degrees)
        // How far the equator is pushed down (0.0-1.0). A deeper equatorial sea moves
        // land toward the poles, which is where tundra and its villages come from;
        // 0.40 does that, going further gains nothing.
        float default_equatorSeaDepth = 0.40;
        // Blend weight of the profile against the noise (0 = mask off entirely, and with
        // it the jitter and band release below). Measured on 64x48: below 0.45 the noise
        // wins and the equatorial sea barely forms (1-2 rows, absent in some worlds);
        // 0.45 is the threshold where it appears in every world (3-8 rows) and the two
        // hemispheres even out. Above that only the sea widens. Useful range 0.30-0.45.
        float default_maskStrength = 0.45;
        // Latitude jitter: each cell reads the profile at lat + this * noise(x,y), so the
        // coastline wanders instead of following a parallel. Measured on 64x48: coast
        // raggedness rises 3.70 -> 4.39 from 0 to 10 degrees and then stops, so 5-10 is
        // the useful range; 20-30 only thins the third landmass and costs tundra.
        // TODO it only reaches the EQUATOR-side coast: above landEdgeLatitude the profile
        // is a flat 1.0, so warping the latitude changes nothing there. The polar coast is
        // cut by the polar block alone, which is strictly zonal - it looks knife-straight.
        // Feeding the same warped latitude to the polar block would fix that.
        float default_maskLatJitter = 0.0;
        float default_maskBandRelease = 0.30;  // Band release of the mask inside the land band (0.0-1.0, 0 = off)
        // How much of the hex-map aspect (1.73:1 on a square region grid) is taken out
        // of the noise: 1 = round landmasses, 0 = the old east-west stretch.
        // 0.80 leaves a slight east-west stretch (aspect^0.2 = 1.13). Full correction
        // (1.0) makes landmasses round but fewer worlds pass and there are fewer
        // liveable islands; 0 is the old unstretched noise, which produced strips.
        float default_noiseAspectCorrection = 0.80;

        // Show current defaults
        logger::write("");
        logger::write("Fractal map generation parameters:");
        logger::write("  Temperature: " + std::to_string(default_minTemp) + " (poles) to " +
                      std::to_string(default_maxTemp) + " (equator)");
        logger::write("  Terrain generation:");
        logger::write("    Continent frequency: " + std::to_string(default_frequency) +
                      " (higher = MORE, smaller continents)");
        logger::write("  Land/Water distribution:");
        logger::write("    Water: " + std::to_string((int)(default_waterPercent * 100)) + "%");
        logger::write("    Mountains: " + std::to_string((int)(default_mountainPercent * 100)) + "%");
        logger::write("    Hills: " + std::to_string((int)(default_hillPercent * 100)) + "%");
        logger::write("    Lake chance: " + std::to_string((int)(default_lakePercent * 100)) + "%");
        logger::write("  Climate:");
        logger::write("    Rainfall balance: " + std::to_string(default_evoparation) +
                      " (higher = WETTER)");
        logger::write("  Macro-geography mask (maskStrength 0 = mask off):");
        logger::write("    Land edge latitude: " + std::to_string(default_landEdgeLatitude) + " deg");
        logger::write("    Equator push-down: " + std::to_string(default_equatorSeaDepth));
        logger::write("    Mask blend strength: " + std::to_string(default_maskStrength) +
                      " (0 = off)");
        logger::write("    Band-edge latitude jitter: " + std::to_string(default_maskLatJitter) +
                      " deg (0 = off)");
        logger::write("    Band release: " + std::to_string(default_maskBandRelease) +
                      " (0 = off)");
        logger::write("    Noise aspect correction: " + std::to_string(default_noiseAspectCorrection) +
                      " (1 = round landmasses, 0 = east-west stretch)");
        logger::write("    Underground water: " + std::to_string(undergroundWaterShare) +
                      " (sea only under surface sea)");
        logger::write("  Polar shaping (polarLatitudeStart >= 89 disables it):");
        logger::write("    Latitude start: " + std::to_string(default_polarLatitudeStart) + " deg");
        logger::write("    Elevation redux: " + std::to_string(default_polarElevationRedux));
        logger::write("");
        logger::write("Use these settings? (y/n) [y]: ");

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "n" || choice == "N") {
            logger::write("");
            logger::write("Enter new values (or press Enter to keep default):");
            logger::write("");

            // Temperature
            map->minTemp = ask_parameter("Min temperature (poles, -60 to 20)",
                                         default_minTemp, -60, 20);
            map->maxTemp = ask_parameter("Max temperature (equator, 20 to 80)",
                                         default_maxTemp, 20, 80);

            // Terrain generation
            map->frequency = ask_parameter_float("Continent frequency (1.0-10.0, higher = more and smaller)",
                                                default_frequency, 1.0, 10.0);

            // Land/Water
            map->waterPercent = ask_parameter_float("Water percentage (0.05-0.90)",
                                                   default_waterPercent, 0.05, 0.90);
            map->mountainPercent = ask_parameter_float("Mountain percentage (0.0-0.50)",
                                                       default_mountainPercent, 0.0, 0.50);
            map->hillPercent = ask_parameter_float("Hill share of the mountain block (0.0-0.90)",
                                                   default_hillPercent, 0.0, 0.90);
            map->lakePercent = ask_parameter_float("Lake placement chance (0.0-1.00)",
                                                   default_lakePercent, 0.0, 1.0);

            // Historical buildings (use defaults)
            // To customize, modify mapgen.cpp constructor defaults
            map->generateHistoricalRoads = true;
            map->generateHistoricalProductionBuildings = true;

            // Climate (IMPORTANT: higher evoparation = MORE evaporation = WETTER world)
            map->evoparation = ask_parameter_float("Rainfall balance (0.0-1.0, higher=WETTER)",
                                                  default_evoparation, 0.0, 1.0);

            // Fractal noise summation
            map->octaves     = ask_parameter("Noise octaves (1-6)", default_octaves, 1, 6);
            map->lacunarity  = ask_parameter_float("Octave frequency step (1.2-3.0)", default_lacunarity, 1.2, 3.0);
            map->persistence = ask_parameter_float("Octave weight decay (0.1-0.9)", default_persistence, 0.1, 0.9);

            // Latitude-profile mask (macro-geography bias; 0 = off)
            map->landEdgeLatitude = ask_parameter_float("Land edge latitude (0-90, degrees)",
                                                        default_landEdgeLatitude, 0.0, 90.0);
            map->equatorSeaDepth = ask_parameter_float("Equator push-down (0.0-1.0)",
                                                       default_equatorSeaDepth, 0.0, 1.0);
            map->maskStrength = ask_parameter_float("Mask blend strength (0.0-1.0, 0 = off)",
                                                    default_maskStrength, 0.0, 1.0);
            map->maskLatJitter = ask_parameter_float("Band-edge latitude jitter (0.0-30.0 deg, 0 = off)",
                                                     default_maskLatJitter, 0.0, 30.0);
            map->maskBandRelease = ask_parameter_float("Band release (0.0-1.0, 0 = off)",
                                                       default_maskBandRelease, 0.0, 1.0);
            map->noiseAspectCorrection = ask_parameter_float("Noise aspect correction (0.0-1.0, 1 = round)",
                                                             default_noiseAspectCorrection, 0.0, 1.0);
            // Ruleset global rather than a Map field: the underground levels are
            // not built by the parametric generator. Asked here so a sweep can
            // find the value; see neworigins/rules.cpp.
            undergroundWaterShare = ask_parameter_float("Underground water share (0.0-0.80)",
                                                        undergroundWaterShare, 0.0, 0.80);

            // Polar archipelago parameters (polarLatitudeStart >= 89 disables the block)
            map->polarLatitudeStart = ask_parameter_float("Polar latitude start (0.0-90.0 deg, 89+ disables the polar block)",
                                                          default_polarLatitudeStart, 0.0, 90.0);
            map->polarElevationRedux = ask_parameter_float("Polar elevation redux (0.0-1.0, higher = more ocean at poles)",
                                                           default_polarElevationRedux, 0.0, 1.0);
        } else {
            // Use defaults
            map->minTemp = default_minTemp;
            map->maxTemp = default_maxTemp;
            map->frequency = default_frequency;
            map->octaves = default_octaves;
            map->lacunarity = default_lacunarity;
            map->persistence = default_persistence;
            map->evoparation = default_evoparation;
            map->waterPercent = default_waterPercent;
            map->mountainPercent = default_mountainPercent;
            map->hillPercent = default_hillPercent;
            map->lakePercent = default_lakePercent;
            map->generateHistoricalRoads = true;
            map->generateHistoricalProductionBuildings = true;
            map->polarLatitudeStart = default_polarLatitudeStart;
            map->polarElevationRedux = default_polarElevationRedux;
            map->landEdgeLatitude = default_landEdgeLatitude;
            map->equatorSeaDepth = default_equatorSeaDepth;
            map->maskStrength = default_maskStrength;
            map->maskLatJitter = default_maskLatJitter;
            map->maskBandRelease = default_maskBandRelease;
            map->noiseAspectCorrection = default_noiseAspectCorrection;
        }

        regions.create_natural_surface_level(map);
    } else if (generator == 3) {
        regions.create_island_ring_level(1, xx, yy, "");
    }

    // Create underworld levels
    int i;
    for (i = 2; i < Globals->UNDERWORLD_LEVELS+2; i++) {
        int lx = level_size(xx, regions.GetLevelXScale(i));
        int ly = level_size(yy, regions.GetLevelYScale(i));
        if (generator == 3 && i == 2) {
            regions.create_underworld_ring_level(i, lx, ly, "underworld");
        } else {
            regions.create_underworld_level(i, lx, ly, "underworld");
        }
    }
    // Underdeep levels
    for (i = Globals->UNDERWORLD_LEVELS + 2; i < Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS + 2; i++) {
        int lx = level_size(xx, regions.GetLevelXScale(i));
        int ly = level_size(yy, regions.GetLevelYScale(i));
        regions.create_underdeep_level(i, lx, ly, "underdeep");
    }

    if (Globals->ABYSS_LEVEL) {
        regions.create_abyss_level(Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS + 2, "abyss");
    }

    if (Globals->DUNGEON_LEVEL) {
        int dungeon_idx = 2 + Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS + Globals->ABYSS_LEVEL;
        regions.create_dungeon_level(dungeon_idx, xx, yy, "dungeon");
    }

    CountNames();

    // --- START OF SMART SHAFTS GENERATION ---
    // This logic handles 1, 2, 3 or more levels dynamically based on Globals.

    // 1. Connection: Surface (L1) -> Top Underworld (L2)
    if (Globals->UNDERWORLD_LEVELS > 0) {
        // Entrance on Surface: dynamic spacing 2d2+2 (mean 5), 4 seeds, no stairwell check.
        // Deliberately the same roll the villages use, so the surface carries about
        // one shaft per settlement. The count is set by that roll, not by maxShafts,
        // which never binds here - so do NOT cap this call by the destination level.
        regions.CreateSmartShafts(1, 2, 5, 0, 4);
    }

    // 2. Connections between multiple Underworld levels (L2 -> L3, etc.)
    if (Globals->UNDERWORLD_LEVELS > 1) {
        for (int i = 2; i < Globals->UNDERWORLD_LEVELS + 1; i++) {
            // Dynamic 2d2+2 spacing, 2 seeds, stairwell prevention 4, capped by
            // the level below - see the surface call above for why only these are.
            regions.CreateSmartShafts(i, i + 1, 5, 4, 2, true);
        }
    }

    // 3. Connection: Bottom of Underworld -> Top of Underdeep
    if (Globals->UNDERWORLD_LEVELS > 0 && Globals->UNDERDEEP_LEVELS > 0) {
        int bottomUW = Globals->UNDERWORLD_LEVELS + 1;
        int topUD = bottomUW + 1;
        // Dynamic 2d2+2 spacing, 2 seeds, stairwell prevention 2, capped by the
        // underdeep's own size, which is a fraction of the underworld's - counting
        // over the underworld would riddle the smaller level with shafts.
        regions.CreateSmartShafts(bottomUW, topUD, 4, 2, 2, true);
    }

    // 4. Connections between multiple Underdeep levels
    if (Globals->UNDERDEEP_LEVELS > 1) {
        int firstUD = Globals->UNDERWORLD_LEVELS + 2;
        int lastUD = Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS + 1;
        for (int i = firstUD; i < lastUD; i++) {
            // Dynamic 2d2+2 spacing, 2 seeds, stairwell prevention 2, capped below.
            regions.CreateSmartShafts(i, i + 1, 4, 2, 2, true);
        }
    }

    // 5. Lairs at both ends of every shaft, on every level. Run once all shafts
    // exist: a shaft puts an O_SHAFT object at its entrance and at its exit, so a
    // per-level pass covers both, and the deepest level - which holds only exits -
    // is included. Settlement hexes are skipped inside CreateLairsAtShafts.
    {
        int lastLevel = Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS + 1;
        for (int i = 1; i <= lastLevel; i++)
            regions.CreateLairsAtShafts(i);
    }
    // --- END OF SMART SHAFTS GENERATION ---

//    if (Globals->UNDERWORLD_LEVELS+Globals->UNDERDEEP_LEVELS == 1) {
//        regions.MakeShaftLinks( 2, 1, 8 );
//        // regions.MakeShaftLinks( 2, 1, 6 );
//    } else if (Globals->UNDERWORLD_LEVELS+Globals->UNDERDEEP_LEVELS) {
//        int i, ii;
//        // shafts from surface to underworld
//        regions.MakeShaftLinks(2, 1, 10);
//        // regions.MakeShaftLinks(2, 1, 8);
//        for (i=3; i<Globals->UNDERWORLD_LEVELS+2; i++) {
//            regions.MakeShaftLinks(i, 1, 10*i-10);
//        }
//        // Shafts from underworld to underworld
//        if (Globals->UNDERWORLD_LEVELS > 1) {
//            for (i = 3; i < Globals->UNDERWORLD_LEVELS+2; i++) {
//                for (ii = 2; ii < i; ii++) {
//                    if (i == ii+1) {
//                        regions.MakeShaftLinks(i, ii, 12);
//                    } else {
//                        regions.MakeShaftLinks(i, ii, 24);
//                    }
//                }
//            }
//        }
//        // underdeeps to underworld
//        if (Globals->UNDERDEEP_LEVELS && Globals->UNDERWORLD_LEVELS) {
//            // Connect the topmost of the underdeep to the bottommost
//            // underworld
//            regions.MakeShaftLinks(Globals->UNDERWORLD_LEVELS+2,
//                    Globals->UNDERWORLD_LEVELS+1, 12);
//        }
//        // Now, connect the underdeep levels together
//        if (Globals->UNDERDEEP_LEVELS > 1) {
//            for (i = Globals->UNDERWORLD_LEVELS+3;
//                    i < Globals->UNDERWORLD_LEVELS+Globals->UNDERDEEP_LEVELS+2;
//                    i++) {
//                for (ii = Globals->UNDERWORLD_LEVELS+2; ii < i; ii++) {
//                    if (i == ii+1) {
//                        regions.MakeShaftLinks(i, ii, 12);
//                    } else {
//                        regions.MakeShaftLinks(i, ii, 25);
//                    }
//                }
//            }
//        }
//    }

    regions.SetACNeighbors( 0, 1, xx, yy );

    regions.InitSetupGates( 1 );
    // Gates disabled in Underworld - only Surface level has gates
    // Underworld/Underdeep accessible only via shafts
    // for (int i=2; i < Globals->UNDERWORLD_LEVELS+2; i++) {
    //     regions.InitSetupGates( i );
    // }

    regions.FixUnconnectedRegions();

    regions.FinalSetupGates();

    // do final modifications to add in the victory objects (the ritual altars and the deactivated monolith)

    regions.CalcDensities();

    regions.TownStatistics();

    regions.ResourcesStatistics();

    regions.NameStatistics();

    regions.MapStatistics();
}

/**
 * @brief Is the surface hex above this underground region water?
 *
 * The projection is the same proportional one the shafts use, so "above" here
 * means the same hex a shaft dug at this spot would come out of.
 *
 * @param regions Region list (the surface level must already exist)
 * @param pReg Underground region
 * @return true when the surface hex above is ocean or lake
 * @see ARegionList::GetLevelXScale(), ARegionList::CreateSmartShafts()
 */
static bool surface_above_is_water(ARegionList& regions, ARegion* pReg)
{
    ARegionArray* surface = regions.pRegionArrays[ARegionArray::LEVEL_SURFACE];
    if (!surface) return false;

    int x = (int) lround(pReg->xloc * regions.GetLevelXScale(pReg->zloc));
    int y = (int) lround(pReg->yloc * regions.GetLevelYScale(pReg->zloc));
    // Regions exist only where x + y is even; step back inside the map rather
    // than wrapping to the opposite pole.
    if ((x + y) % 2) y = (y + 1 < surface->y) ? y + 1 : y - 1;
    if (x >= surface->x) x = surface->x - 1;
    if (y >= surface->y) y = surface->y - 1;

    ARegion* above = surface->GetRegion(x, y);
    return above && TerrainDefs[above->type].similar_type == R_OCEAN;
}

static double surface_water_share(ARegionList& regions);

/**
 * @brief Should this underground anchor be sea?
 *
 * True only where the surface above is water, and then with the probability
 * that makes the level come out at undergroundWaterShare overall: the offer is
 * divided by the surface's own water share, because it is only ever made over
 * water.
 *
 * @param regions Region list
 * @param pReg Underground region being seeded
 * @return true when this anchor should be ocean
 * @see surface_above_is_water(), surface_water_share()
 */
static bool ocean_anchor_here(ARegionList& regions, ARegion* pReg)
{
    if (!surface_above_is_water(regions, pReg)) return false;

    double surfaceWater = surface_water_share(regions);
    double chance = (surfaceWater > 0.0) ? undergroundWaterShare / surfaceWater : 0.0;
    if (chance > 1.0) chance = 1.0;
    return rng::get_random(1000) < (int) lround(chance * 1000.0);
}

/**
 * @brief Water share of the finished surface, cached after the first call.
 *
 * Underground ocean anchors are only kept where the surface above is water, so
 * the roll that offers them has to be divided by this to hit
 * undergroundWaterShare. One world is generated per process, so a single cache
 * is enough.
 *
 * @param regions Region list (the surface level must already exist)
 * @return Share of surface hexes that are ocean or lake, 0..1
 */
static double surface_water_share(ARegionList& regions)
{
    static double cached = -1.0;
    if (cached >= 0.0) return cached;

    ARegionArray* surface = regions.pRegionArrays[ARegionArray::LEVEL_SURFACE];
    if (!surface) return 0.0;

    int water = 0, total = 0;
    for (int x = 0; x < surface->x; x++) {
        for (int y = 0; y < surface->y; y++) {
            ARegion* reg = surface->GetRegion(x, y);
            if (!reg) continue;
            total++;
            if (TerrainDefs[reg->type].similar_type == R_OCEAN) water++;
        }
    }
    cached = total ? (double) water / total : 0.0;
    return cached;
}

int ARegionList::GetRegType( ARegion *pReg )
{
    //
    // Figure out the distance from the equator, from 0 to 3.
    //
    // Note that the -3 applied to y here is because I'm assuming we're using
    // icosahedral levels, which don't quite fill their y-space
    int lat = ( pReg->yloc * 8 ) / ( pRegionArrays[ pReg->zloc ]->y);
    if (lat > 3)
    {
        lat = (7 - lat);
    }
    if (lat < 0) lat = 0;

    // Underworld region. These are the anchor seeds GrowTerrain spreads, so the
    // ocean share of the finished level follows the ocean share here.
    //
    // An ocean anchor is only kept where the surface above is water, so the
    // underground seas lie under the surface seas and the land lies under the
    // land. A shaft's exit sits directly below its entrance, so without this
    // most exits under a continent fell into underground ocean and were thrown
    // away (measured: 35 of 57 candidates on one world).
    if ((pReg->zloc > 1) && (pReg->zloc < Globals->UNDERWORLD_LEVELS+2)) {
        if (ocean_anchor_here(*this, pReg)) return R_OCEAN;
        int r = rng::get_random(11);
        if (r < 5) return R_CAVERN;
        if (r < 8) return R_UFOREST;
        return R_TUNNELS;
    }

    // Underdeep region
    if ((pReg->zloc > Globals->UNDERWORLD_LEVELS+1) &&
            (pReg->zloc < Globals->UNDERWORLD_LEVELS+
                          Globals->UNDERDEEP_LEVELS+2)) {
        // Same rule as the underworld: sea only under sea (see above).
        if (ocean_anchor_here(*this, pReg)) return R_OCEAN;
        int r = rng::get_random(3);
        if (r == 0) return R_CHASM;
        if (r == 1) return R_DFOREST;
        return R_GROTTO;
    }

    // surface region
    if ( pReg->zloc == 1 ) {
        int r = rng::get_random(64);
        switch (lat)
        {
        case 0: /* Arctic regions */
            if (r < 32) return R_TUNDRA;
            if (r < 40) return R_MOUNTAIN;
            if (r < 48) return R_FOREST;
            return R_PLAIN;
        case 1: /* Colder regions */
            if (r < 8) return R_TUNDRA;
            if (r < 24) return R_PLAIN;
            if (r < 40) return R_FOREST;
            if (r < 48) return R_MOUNTAIN;
            return R_SWAMP;
        case 2: /* Warmer regions */
            if (r < 16) return R_PLAIN;
            if (r < 28) return R_FOREST;
            if (r < 36) return R_MOUNTAIN;
            if (r < 44) return R_SWAMP;
            if (r < 52) return R_JUNGLE;
            return R_DESERT;
        case 3: /* tropical */
            if (r < 16) return R_PLAIN;
            if (r < 24) return R_MOUNTAIN;
            if (r < 36) return R_SWAMP;
            if (r < 48) return R_JUNGLE;
            return R_DESERT;
        }
        return R_OCEAN;
    }

    if ( pReg->zloc == 0 )
    {
        //
        // This really shouldn't ever get called.
        //
        return( R_NEXUS );
    }

    //
    // This really shouldn't get called either
    //
    return( R_OCEAN );
}

/**
 * @brief Horizontal shrink factor of a level relative to the surface.
 *
 * Level width = surface width / this factor. Used when the levels are created
 * and by GetPlanarDistance() to project a region onto surface coordinates, so it
 * must stay in step with the arrays a world was generated with.
 *
 * The factors are rational rather than whole numbers (Trident, 2026-09-22): a
 * single underworld is 3/4 of the surface in each direction and a single
 * underdeep 3/8, so a 64x64 surface carries a 48x48 underworld and a 24x24
 * underdeep. Halving both had left the underworld too cramped for the shafts
 * the surface sends down - one shaft, each with its own lair, per 13 hexes
 * against 5-7 settlements on the whole level.
 *
 * @param level Level index (0 nexus, 1 surface, then underworld, then underdeep)
 * @return 1 for nexus/surface, 4/3 for underworld levels, 8/3 for underdeep levels.
 * @see GetLevelYScale(), level_size()
 */
double ARegionList::GetLevelXScale(int level)
{
    // Surface and nexus are unscaled
    if (level < 2) return 1.0;

    if (level < Globals->UNDERWORLD_LEVELS + 2) {
        // Underworld: 3/4 of the surface
        return 4.0 / 3.0;
    }
    if (level < Globals->UNDERWORLD_LEVELS + Globals->UNDERDEEP_LEVELS + 2) {
        // Underdeep: 3/8 of the surface
        return 8.0 / 3.0;
    }
    // We couldn't figure it out, assume not scaled.
    return 1.0;
}

/**
 * @brief Vertical shrink factor of a level relative to the surface.
 *
 * The same ratios as GetLevelXScale(), so the underground levels keep the
 * surface's proportions instead of being stretched east-west.
 *
 * @param level Level index (0 nexus, 1 surface, then underworld, then underdeep)
 * @return 1 for nexus/surface, 4/3 for underworld levels, 8/3 for underdeep levels.
 * @see GetLevelXScale()
 */
double ARegionList::GetLevelYScale(int level)
{
    return GetLevelXScale(level);
}

int ARegionList::CheckRegionExit(ARegion *pFrom, ARegion *pTo )
{
    if ((pFrom->zloc==1) ||
        (pFrom->zloc>Globals->UNDERWORLD_LEVELS+Globals->UNDERDEEP_LEVELS+1)) {
        return( 1 );
    }

    int chance = 0;
    if ( pFrom->type == R_CAVERN || pFrom->type == R_UFOREST ||
        pTo->type == R_CAVERN || pTo->type == R_UFOREST )
    {
        chance = 25;
    }
    if ( pFrom->type == R_TUNNELS || pTo->type == R_TUNNELS)
    {
        chance = 50;
    }
    if (pFrom->type == R_GROTTO || pFrom->type == R_DFOREST ||
       pTo->type == R_GROTTO || pTo->type == R_DFOREST) {
        // better connected underdeeps
        chance = 40;
    }
    if (pFrom->type == R_CHASM || pTo->type == R_CHASM) {
        chance = 40;
    }
    if (rng::get_random(100) < chance) {
        return( 0 );
    }
    return( 1 );
}

int ARegionList::GetWeather( ARegion *pReg, int month )
{
    if (pReg->zloc == 0)
    {
        return W_NORMAL;
    }

    if ( pReg->zloc > 1 )
    {
        return( W_NORMAL );
    }

    int ysize = pRegionArrays[ 1 ]->y;

    if ((3*( pReg->yloc+1))/ysize == 0)
    {
        /* Northern third of the world */
        if (month > 9 || month < 2)
        {
            return W_WINTER;
        }
        else
        {
            return W_NORMAL;
        }
    }

    if ((3*( pReg->yloc+1))/ysize == 1)
    {
        /* Middle third of the world */
        if (month == 11 || month == 0 || month == 5 || month == 6)
        {
            return W_MONSOON;
        }
        else
        {
            return W_NORMAL;
        }
    }

    if (month > 3 && month < 8)
    {
        /* Southern third of the world */
        return W_WINTER;
    }
    else
    {
        return W_NORMAL;
    }
}

int ARegion::CanBeStartingCity( ARegionArray *pRA )
{
    if (type == R_OCEAN) return 0;
    if (!IsCoastal()) return 0;
    if (town && town->pop == 5000) return 0;

    int regs = 0;
    std::list<ARegion *> toconsider;
    std::unordered_set<ARegion *> seen;

    toconsider.push_back(this);
    seen.insert(this);

    while (!toconsider.empty()) {
        ARegion *reg = toconsider.front();
        toconsider.pop_front();
        for (int i=0; i<NDIRS; i++) {
            ARegion * r2 = reg->neighbors[i];
            if (!r2) continue;
            if (r2->type == R_OCEAN) continue;
            if (seen.find(r2) != seen.end()) continue;
            regs++;
            if (regs > 20) return 1;
            toconsider.push_back(r2);
            seen.insert(r2);
        }
    }
    return 0;
}

void ARegion::MakeStartingCity()
{
    if (!Globals->TOWNS_EXIST) return;

    if (Globals->GATES_EXIST) gate = -1;

    if (town) delete town;

    add_town(TOWN_CITY);

    if (!Globals->START_CITIES_EXIST) return;

    town->hab = 125 * Globals->CITY_POP / 100;
    while (town->pop < town->hab) town->pop += rng::get_random(200)+200;
    town->dev = TownDevelopment();

    float ratio;

    for (auto& m : markets) delete m; // Free the allocated object
    markets.clear(); // empty the vector.

    Market *m;
    if (Globals->START_CITIES_START_UNLIMITED) {
        for (int i=0; i<NITEMS; i++) {
            if ( ItemDefs[i].flags & ItemType::DISABLED ) continue;
            if ( ItemDefs[ i ].type & IT_NORMAL ) {
                if (i==I_SILVER || i==I_LIVESTOCK || i==I_FISH || i==I_GRAIN)
                    continue;
                m = new Market(Market::MarketType::M_BUY, i, (ItemDefs[i].baseprice * 5 / 2), -1, 5000, 5000, -1, -1);
                markets.push_back(m);
            }
        }
        ratio = ItemDefs[race].baseprice / ((float)Globals->BASE_MAN_COST * 10);
        // hack: include wage factor of 10 in float calculation above
        m = new Market(Market::MarketType::M_BUY, race, (int)(Wages() * 4 * ratio), -1, 5000, 5000, -1, -1);
        markets.push_back(m);
        if (Globals->LEADERS_EXIST) {
            // Check if terrain allows leader recruitment
            TerrainType* terrain = &TerrainDefs[type];
            if (!(terrain->flags & TerrainType::NO_LEADERS)) {
                ratio = ItemDefs[I_LEADERS].baseprice/((float)Globals->BASE_MAN_COST * 10);
                // hack: include wage factor of 10 in float calculation above
                m = new Market(Market::MarketType::M_BUY, I_LEADERS, (int)(Wages() * 4 * ratio), -1, 5000, 5000, -1, -1);
                markets.push_back(m);
            }
        }
    } else {
        SetupCityMarket();
        ratio = ItemDefs[race].baseprice / ((float)Globals->BASE_MAN_COST * 10);
        // hack: include wage factor of 10 in float calculation above
        /* Setup Recruiting */
        m = new Market(
            Market::MarketType::M_BUY, race, (int)(Wages() * 4 * ratio), Population() / 5, 0, 10000, 0, 2000
        );
        markets.push_back(m);
        if ( Globals->LEADERS_EXIST ) {
            // Check if terrain allows leader recruitment
            TerrainType* terrain = &TerrainDefs[type];
            if (!(terrain->flags & TerrainType::NO_LEADERS)) {
                ratio=ItemDefs[I_LEADERS].baseprice/((float)Globals->BASE_MAN_COST * 10);
                // hack: include wage factor of 10 in float calculation above
                m = new Market(
                    Market::MarketType::M_BUY, I_LEADERS, (int)(Wages() * 4 * ratio), Population() / 25, 0, 10000, 0, 400
                );
                markets.push_back(m);
            }
        }
    }
}

int ARegion::IsStartingCity() {
    if (town && town->pop >= (Globals->CITY_POP * 120 / 100)) return 1;
    return 0;
}

int ARegion::IsSafeRegion()
{
    if (type == R_NEXUS) return 1;
    return( Globals->SAFE_START_CITIES && IsStartingCity() );
}

ARegion *ARegionList::GetStartingCity( ARegion *AC,
                    int i,
                    int level,
                    int maxX,
                    int maxY )
{
    ARegionArray *pArr = pRegionArrays[ level ];
    ARegion * reg = 0;

    if ( pArr->x < maxX ) maxX = pArr->x;
    if ( pArr->y < maxY ) maxY = pArr->y;

    int tries = 0;
    while (!reg && tries < 10000) {
        //
        // We'll just let AC exits be all over the map.
        //
        int x = rng::get_random( maxX );
        int y = 2 * rng::get_random( maxY / 2 ) + x % 2;

        reg = pArr->GetRegion( x, y);

        if (!reg || !reg->CanBeStartingCity( pArr )) {
            reg = 0;
            tries++;
            continue;
        }

        for (int j=0; j<i; j++) {
            if (!AC->neighbors[j]) continue;
            if (GetPlanarDistance(reg,AC->neighbors[j], 0, maxY / 10 + 2) < maxY / 10 + 2 ) {
                reg = 0;
                tries++;
                break;
            }
        }
    }

    // Okay, we failed to find something that normally would work
    // we'll just take anything that's of the right distance
    tries = 0;
    while (!reg && tries < 10000) {
        //
        // We couldn't find a normal starting city, let's just go for ANY
        // city
        //
        int x = rng::get_random( maxX );
        int y = 2 * rng::get_random( maxY / 2 ) + x % 2;
        reg = pArr->GetRegion( x, y);
        if (!reg || reg->type == R_OCEAN) {
            tries++;
            reg = 0;
            continue;
        }

        for (int j=0; j<i; j++) {
            if (!AC->neighbors[j]) continue;
            if (GetPlanarDistance(reg,AC->neighbors[j], 0, maxY / 10 + 2) < maxY / 10 + 2 ) {
                reg = 0;
                tries++;
                break;
            }
        }
    }

    // Okay, if we still don't have anything, we're done.
    return reg;
}

