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

#include "namegen.h"

#include "game.h"
#include "gamedata.h"

#include <cctype>
#include "rng.hpp"
#include "string_filters.hpp"
std::vector<std::string> aPrefAbstract = {
    "A", "Ab", "Ach", "Ad", "Ae", "Ag", "Ai", "Ak", "Al", "Am", "An", "Ap", "Ar", "As", "Ash", "At", "Ath", "Au", "Ay",
    "Ban", "Bar", "Brel", "Bren",
    "Cam", "Cla", "Cler", "Col", "Con", "Cor", "Cul", "Cuth", "Cy", "Chal", "Chan", "Chi", "Chon", "Chul", "Chur",
    "Del", "Dur", "Dwar", "Dwur",
    "Ek", "El", "En", "Eth", "Fal", "Far", "Fel", "Fell", "Fen", "Flan", "Flar", "Fly", "Fur", "Fy",
    "Gal", "Gan", "Gar", "Gel", "Glan", "Glar", "Glen", "Glir", "Glyn", "Glyr", "Glyth", "Gogh", "Gor", "Goth", "Gwal", "Gwen", "Gwur", "Gy", "Gyl", "Gyn", "Ghal", "Ghash", "Ghor", "Ghoz", "Ghul",
    "Hach", "Haj", "Hal", "Ham", "Hel", "Hen", "Hil", "Ho", "Hol", "Hul",
    "Ice", "Id", "Ie", "Il", "Im", "In", "Ir", "Is", "Iz",
    "Ja", "Jak", "Jar", "Jaz", "Jeth", "Jez", "Ji", "Jul", "Jur", "Juz",
    "Kag", "Kai", "Kaj", "Kal", "Kam", "Ken", "Kor", "Kul", "Kwal", "Kwar", "Kwel", "Kwen", "Kha", "Khel", "Khor", "Khul", "Khuz",
    "Lagh", "Lar", "Lin", "Lir", "Loch", "Lor", "Lyn", "Lyth",
    "Mal", "Man", "Mar", "Me", "Mer", "Meth", "Mil", "Min", "Mir",
    "Nam", "Nar", "Nel", "Nem", "Nen", "Nor", "Noth", "Nyr",
    "Ob", "Oe", "Ok", "Ol", "On", "Or", "Ow",
    "Par", "Pel", "Por", "Py", "Pyr", "Pyl",
    "Ral", "Ra", "Ram", "Rath", "Re", "Rel", "Ren", "Ri", "Ril", "Ro", "Ror", "Ruk", "Ry",
    "Sen", "Seth", "Sul", "Shae", "Shal", "Shar", "Shen", "Shir",
    "Tal", "Tam", "Tar", "Tel", "Ten", "Tir", "Tol", "Tul", "Tur", "Thor", "Thul",
    "U", "Uk", "Un", "Ul",
    "Va", "Val", "Van", "Vel", "Ven", "Veren", "Vul",
    "Wal", "War", "We", "Wel", "Wil", "Win",
    "Y", "Ya", "Ych", "Ye", "Yg", "Yi", "Yl", "Yn", "Yo", "Yp", "Yr", "Yth", "Yu", "Yul", "Za", "Zar",
    "Zel", "Zi", "Zim", "Zir", "Zol", "Zor", "Zhok", "Zhu", "Zhuk", "Zhul"
};

std::vector<std::string> aSufAbstract = {
    "a", "ach", "aech", "ael", "aem", "aen", "aer", "aeth", "ail", "ain", "air", "aith", "all", "an", "and", "ar", "ash", "auch", "aul", "aun", "aur",
    "baen", "bain", "bar", "bath", "ben", "byr",
    "cael", "caer", "can", "cen", "cor", "cynd",
    "dach", "dail", "dain", "dan", "dar", "dik", "dir", "dy",
    "e", "eal", "el", "eld", "eth",
    "gar", "gath", "grim",
    "i", "ian", "ield", "ien", "ieth", "il", "ior", "ioth", "ish",
    "maer", "mail", "main", "mar", "maren", "miel", "mieth",
    "nain", "nair", "naith", "nal", "nar", "nath", "nen", "ner", "niel", "nien", "nieth", "nor", "noth", "nul", "nur", "nyr",
    "o", "och", "or", "oth", "oum", "owen",
    "rach", "raid", "rail", "rain", "raish", "raith", "ran", "rar", "ras", "raven", "ren", "riel", "rien", "rier", "rik", "ril", "rish", "ron", "ror", "ros", "roth", "rych", "ryl", "ryr", "rych", "thach", "thain", "thak", "thal", "than", "thar", "thiel", "thien", "thor", "thul", "thur",
    "ug", "uild", "uin", "uith", "uk", "ul", "un", "ur", "uth",
    "wain", "waith", "wald", "war", "ward", "well", "wen", "win",
    "y", "yll", "ynd", "yr", "yth",
    "zak", "zel", "zen", "zokh", "zor", "zul", "zuth"
};


std::vector<std::string> aPrefEscimo = {
    "ach", "achana", "achka", "achuk", "ak", "akip", "akun", "ani", "aninnik", "ap", "apat", "api", "apucha", "chat", "chit", "chua", "chut", "ichis", "ichitt", "ikissik", "ikiutta", "iniki", "iniss", "inissik", "ippik", "issi", "it", "ita", "itani", "kass", "kikk", "kiun", "kyap", "kyaun", "kyu", "kyup", "kuk", "kup", "kupp", "kut", "pi", "pyaun", "pyi", "pyu", "pyuin", "pyup", "siss", "syaun", "syiin", "syu", "syuss", "syuun", "saun", "suss", "suun", "tap", "tikk", "tya", "tyaan", "tyiun", "tuan", "uch", "uchach", "uchk", "ucht", "upani", "upik", "utin", "unuchut"
};

std::vector<std::string> aSufEscimo = {
    "ach", "akan", "akya", "ani", "anu", "chach", "chat", "chin", "chit", "chun", "iakap", "ichak", "ikin", "ikip", "ikta", "in", "innik", "ipa", "ippik", "ipuch", "issik", "it", "itut", "kan", "kip", "kuk", "kukik", "kya", "pich", "pin", "pip", "pput", "sup", "tait", "tin", "tip", "tit", "titut", "uchi", "uk", "ukta", "utta"
};


std::vector<std::string> aPrefGreek = { "aer", "agamen", "agor", "aion", "air", "aker", "akrogon", "akro", "amen", "ametan", "amiant", "ampel", "anan", "anankamom", "andro", "aner", "antano", "aorat", "apolytro", "athem", "autark", "biast", "byblo", "chrono", "dogmo", "dokim", "ekbalo", "ekpipto", "ektrom", "entell", "epikatar", "ereun", "exang", "exod", "gorgo", "hekono", "heter", "hikan", "hilar", "hymno", "hypno", "ianno", "ierem", "kalym", "katar", "klepto", "kreman", "makar", "malak", "maran", "metan", "nest", "oikonom", "optan", "orgil", "otar", "ouran", "papyr", "parait", "paramen", "parthen", "perik", "peril", "perim", "philag", "polyl", "poro", "prax", "sin", "sken", "smyrno", "strato", "thron", "trit", "troglo", "zelot"};
std::vector<std::string> aSufGreek = { "akos", "alizo", "alotos", "arenos", "aros", "arotes", "arx", "askalos", "atos", "atres", "einos", "elos", "eros", "eryx", "etes", "imos", "irmos", "itos", "okles", "opos", "otus" "polis" "us"};

std::vector<std::string> aPrefAztec = { "Acayu", "Alar", "Apatzin", "Ayoquez", "Ayu", "Cham", "Chetu", "Chi", "Cho", "Chun", "Colo", "Comalcal", "Comi", "Cuet", "Hala", "Huicha", "Huimax", "Hunuc", "Ix", "Ixmiquil", "Iza", "Jal", "Jamil", "Juchi", "Kaminal", "Kantunil", "Maya", "Mapas", "Maxcan", "Maz", "Miahu", "Minatit", "Mul", "Noch", "Oax", "Oco", "Ome", "Ozibilchal", "Panab", "Pet", "Pochu", "Popoca", "Say", "Sayax", "Tehuan", "Tenoxtit", "Tep", "Tik", "Tiz", "Tizi", "Tlaco", "Tom", "Ton", "Tul", "Tun", "Tux", "Uaxac", "Urua", "Yaxchi", "Zacat", "Zana", "Zima"};
std::vector<std::string> aSufAztec = { "atlan", "ixtlan", "huas", "juyu", "poton", "talpan", "tepec", "tepetl", "titlan", "zalan"};

std::vector<std::string> aPrefDrow = { "Alean", "Ale", "Arab", "Arken", "Auvry", "Baen", "Barri", "Cladd", "Desp", "De", "Do'", "Eils", "Everh", "Fre", "Gode", "Helvi", "Hla", "Hun", "Ken", "Kil", "Mae", "Mel", "My", "Noqu", "Orly", "Ouss", "Rilyn", "Teken'", "Tor", "Zau"};
std::vector<std::string> aSufDrow = { "afin", "ana", "ani", "ar", "arn", "ate", "ath", "duis", "ervs", "ep", "ett", "ghym", "iryn", "lyl", "mtor", "ndar", "neld", "rae", "rahel", "rret", "sek", "th", "tlar", "t'tar", "tyl", "und", "urden", "val", "virr", "zynge"};

std::vector<std::string> aPrefScotish = { "Aber", "Ar", "As", "At", "Avie", "Bal", "Ben", "Bran", "Brech", "Bro", "Cairn", "Can", "Carl", "Colon", "Clyde", "Craig", "Cum", "Dearg", "Don", "Dor", "Dun", "Dur", "El", "Fal", "For", "Fyne", "Glas", "Hal", "Inver", "Ju", "Kil", "Kilbran", "Kirrie", "Lairg", "Lin", "Lo", "Loch", "Lorn", "Lyb", "Ma", "Mal", "Mel", "Monadh", "Nairn", "Nith", "Ob", "Oron", "Ran", "Scar", "Scour", "Spey", "Stom", "Strom", "Tar", "Tay", "Ti", "Tober", "Uig", "Ulla", "Wick"};
std::vector<std::string> aSufScotish = { "aline", "an", "aray", "avon", "ba", "bert", "bis", "blane", "bran", "da", "dee", "deen", "far", "feldy", "gin", "gorm", "ie", "in", "kaig", "kirk", "laig", "liath", "maol", "mond", "moral", "more", "mory", "muir", "na", "nan", "ner", "ness", "nhe", "nock", "noth", "nure", "ock", "pool", "ra", "ran", "ree", "res", "say", "ster", "tow"};

std::vector<std::string> aPrefAfrica = { "Ag", "Ahr", "Ba", "Bor", "Dar", "Don", "Dor", "Dung", "Ga", "Gal", "Gam", "Gul", "Gur", "Gwa", "Gwah", "Gwar", "Gwul", "Ig", "Ja", "Jih", "Jug", "Kas", "Kesh", "Kides", "Kili", "Kor", "Kul", "Kush", "Lar", "Lu", "Ma", "Mat", "Mbeg", "Mbeng", "Min", "Ngor", "Ngul", "N'Gul", "Nyag", "N'Yag", "N'Zin", "Ong", "Rod", "Sha", "Sum", "Swa", "Ti", "Tot", "Ug", "Ung", "Wad", "Waz", "Wur", "Ya", "Za", "Zang", "Zar", "Zem", "Zik", "Zim", "Zu", "Zul"};
std::vector<std::string> aSufAfrica = { "a", "ad", "aga", "ara", "ai", "al", "alo", "ang", "anga", "ani", "bab", "bal", "balla", "biba", "bu", "buk", "buru", "daja", "dar", "donga", "dor", "du", "dul", "duru", "daza", "'guba", "'gung", "hili", "i", "id", "iji", "ili", "jari", "jaro", "juri", "'ka", "lah", "lur", "mala", "mim", "mu", "munga", "mur", "nur", "nuzi", "o", "od", "ofo", "oja", "onga", "ozi", "ra", "sala", "sula", "sunga", "tulo", "u", "ula", "ulga", "unga", "wa", "wath", "we", "wuzi", "zaja", "zaza", "zin", "zum", "zung", "zur"};

std::vector<std::string> aPrefElven1 = { "Ama", "Ari", "Aza", "Cla", "Cy", "Dae", "Dho", "Dre", "Fi", "Ia", "Ky", "Lue", "Ly", "Mai", "My", "Na", "Nai", "Nu", "Ny", "Py", "Ry", "Rua", "Sae", "Sha", "She", "Si", "Tia", "Ty", "Ya", "Zy"};
std::vector<std::string> aSufElven1 = { "nae", "lae", "dar", "drimme", "lath", "lith", "lyth", "lan", "lanna", "lirr", "lis", "lys", "lyn", "llinn", "lihn", "nal", "nin", "nine", "nyn", "nis", "sal", "sel", "tas", "thi", "thil", "vain", "vin", "wyn", "zair"};

std::vector<std::string> aPrefElven2 = { "Aer", "Al", "Am", "Ang", "Ansr", "Ar", "Arn", "Bael", "Cael", "Cal", "Cas", "Cor", "Eil", "Eir", "El", "Er", "Ev", "Fir", "Fis", "Gael", "Gil", "Il", "Kan", "Ker", "Keth", "Koeh", "Kor", "Laf", "Lam", "Mal", "Nim", "Rid", "Rum", "Seh", "Sel", "Sim", "Syl", "Tahl", "Vil"};
std::vector<std::string> aSufElven2 = { "ael", "aer", "aera", "aias", "aia", "aith", "aira", "ala", "ali", "ani", "uanna", "ari", "aro", "ibrar", "adar", "odar", "udrim", "emar", "esti", "evar", "afel", "efel", "ihal", "ihar", "ahel", "ihel", "ian", "ianna", "iat", "iel", "ila", "inar", "ine", "ith", "elis", "ellon", "inal", "anis", "aruil", "eruil", "isal", "sali", "sar", "asar", "isar", "asel", "isel", "itas", "ethil", "avain", "avin", "azair"};

std::vector<std::string> aPrefDwarven = { "agar", "agaz", "barak", "baruk", "baraz", "bizar", "bizul", "bul", "buzar", "garak", "gor", "gog", "gorog", "gothol", "guzib", "ibin", "ibiz", "izil", "izuk", "kelek", "kezan", "kibil", "kinil", "kun", "kheled", "khelek", "khimil", "khuz", "laruk", "luz", "moran", "moril", "nibin", "nukul"};
std::vector<std::string> aSufDwarven = { "akar", "agul", "amen", "gib", "gol", "gog", "gul", "guluth", "gundil", "gundag", "guzun", "lib", "lizil", "loth", "mab", "mor", "mud", "mur", "nazar", "nigin", "niz", "nizil", "nuz", "nuzum", "thibil", "thizar", "ulin", "uzar", "uzun", "zad", "zakar", "zal", "zalak", "zam", "zan", "zaral", "zarak", "zeg", "zerek", "zibith", "zikil", "zokh", "zukum"};

std::vector<std::string> aPrefOrchish = { "Arg", "Az", "Azog", "Bad", "Balkh", "Bol", "Bolg", "Dreg", "Dur", "Durba", "Ghash", "Gork", "Grish", "Gruk", "Gul", "Gurz", "Lurg", "Luz", "Mor", "Muzg", "Naz", "Nazg", "Og", "Olog", "Shag", "Skag", "Skarn", "Slag", "Snag", "Tarkh", "Thrak", "Urg", "Ug", "Uzg", "Vol", "Vrak", "Yazh", "Zag", "Zorn", "Zug"};
std::vector<std::string> aSufOrchish = { "agal", "buz", "dor", "dur", "gar", "mog", "narb", "nazg", "rod", "shak", "waz", "ubal"};

std::vector<std::string> aPrefArabic = { "Aaza", "Abha", "Ad", "Aga", "Ah", "Ain", "Ait", "Ajda", "Ali", "Al", "Arrer", "As", "Ash", "Ay", "Az", "Bab", "Bani", "Bari", "Bat", "Birak", "Bitam", "Bou", "Dakh", "Dha", "Dham", "Djaz", "Djeb", "Fash", "Ghad", "Ghar", "Ghat", "Gra", "Had", "Ham", "Har", "Jawf", "Jer", "Jid", "Jir", "Kabir", "Kebir", "Ket", "Khat", "Khem", "Kher", "Khum", "Ksar", "Mak", "Mara", "Men", "Mu", "Qat", "Qay", "Sa", "Sab", "Sah", "Sal", "Sidi", "Sma", "Sulay", "Tabel", "Tar", "Tay", "Taza", "Ubay", "Wah", "Yab", "Yaf", "Yous", "Zil", "Zou"};
std::vector<std::string> aSufArabic = { "ada", "ah", "air", "ama", "amis", "aq", "ar", "ash", "at", "bala", "biya", "dah", "dir", "el", "faya", "fi", "fir", "ha", "hab", "ia", "idj", "ir", "is", "ja", "jel", "ka", "kah", "kha", "khari", "la", "lah", "ma", "na", "nen", "ra", "ran", "rar", "rata", "rin", "rem", "run", "sef", "sumah", "tar", "ya", "yan", "yil"};

std::vector<std::string> aPrefViking = { "al", "ber", "drammen", "grong", "hag", "hauge", "hed", "kinsar", "kol", "koper", "lin", "nas", "norr", "olof", "os", "Ost", "Oster", "skellef", "soder", "stal", "stavan", "stock", "tons", "trond", "vin"};
std::vector<std::string> aSufViking = { "fors", "gard", "heim", "holm", "lag", "mar", "marden", "mark", "stad", "strom"};

std::vector<std::string> aPrefHumans = { "basing", "birming", "black", "bland", "bletch", "brack", "brent", "bridge", "broms", "bur", "cam", "canter", "chelten", "chester", "col", "dor", "dun", "glaston", "grim", "grin", "harro", "hastle", "hels", "hemp", "herne", "horn", "hors", "hum", "ketter", "lei", "maiden", "marble", "mar", "mel", "new", "nor", "notting", "oak", "ox", "ports", "sher", "stam", "stan", "stock", "stroud", "tuan", "warring", "wind"};
std::vector<std::string> aSufHumans = { "dare", "don", "field", "ford", "grove", "ham", "hill", "lock", "mere", "moor", "ton", "vil", "wood"};

std::vector<std::string> aPrefInn = { "Bent", "Black", "Blind", "Blue", "Bob's", "Joe's", "Broken", "Buxom", "Cat's", "Crow's", "Dirty", "Dragon", "Dragon's", "Drunken", "Diamond", "Eagle's", "Eastern", "Falcon's", "Fawning", "Fiend's", "Flaming", "Frosty", "Frozen", "Gilded", "Genie's", "Golden", "Golden", "Gray", "Green", "King's", "Licked", "Lion's", "Iron", "Mended", "Octopus", "Old", "Old", "Orc's", "Pink", "Pot", "Puking", "Queen's", "Red", "Ruby", "Delicate", "Sea", "Sexy", "Shining", "Silver", "Singing", "Steel", "Strange", "Thirsty", "Violet", "White", "Wild", "Yawing"};
std::vector<std::string> aSufInn = { " Axe", " Anchor", " Barrel", " Basilisk", " Belly", " Blade", " Boar", " Breath", " Brew", " Claw", " Coin", " Delight", " Den", " Dragon", " Drum", " Dwarf", " Fist", " Flower", " Gem", " Gryphon", " Hand", " Head", " Hole", " Inn", " Lady", " Maiden", " Lantern", " Monk", " Mug", " Nest", " Orc", " Paradise", " Pearl", " Pig", " Pit", " Place", " Tavern", " Portal", " Ranger", " Rest", " Sailor", " Sleep", " Song", " Swan", " Swords", " Tree", " Unicorn", " Whale", " Wish", " Wizard", " Rain"};

std::vector<std::string> aPrefFort = { "Mind ", "Iron ", "Dimension ", "Demonic ", "Blood ", "Mystery ", "Ancient ", "Doom ", "Black ", "Crimson ", "Blue ", "Eternal ", "Cursed ", "Ruined ", "Stone ", "Ethereal ", "Phantom ", "Forgotten ", "King's ", "Queen's ", "Royal ", "Fallen ", "Lost ", "Warrior's ", "Sorcerer's ", "Steel ", "Blademaster's ", "Screaming ", "Ice ", "Frozen ", "Dragon ", "Glorious ", "Infernal "};
std::vector<std::string> aSufFort = { "Storm", "Fist", "Keep", "Rage", "Rose", "Residence", "Mansion", "Haven", "Gates", "" };

std::vector<std::string> aPrefShip = { "Absolute", "Adventure", "Alisa", "Altered", "Amber", "Ancient", "Angel's", "Animal", "Another", "Azure", "Bad", "Bad Moon", "Betty", "Big", "Black", "Blue", "Breaking", "Crime", "Crimson", "Dancing", "Dark", "Dawn", "Dirty", "Distant", "Double", "Dragon", "Dream", "Emerald", "Empty", "Enchanted", "Exotic", "Extra", "Extreme", "Fallen", "Fast", "Fatal", "Fifth", "Final", "Fine", "Fire", "First", "Flying", "Foreign", "Fortune", "Funny", "Gentle", "Golden", "Grand", "Great", "Green", "Grey", "Gypsy", "Half", "Happy", "High", "Impossible", "Jade", "Little", "Lone", "Lucky", "Mad", "Mermaid", "Midnight", "Moon", "Morning", "Naked", "Naughty", "Naval", "New", "Night", "Ocean", "Old", "Pacific", "Perfect", "Pretty", "Quick", "Quiet", "Red", "Saint", "Sea", "Sapphire", "Second", "Silver", "Southern", "Stella", "Sun", "Sunset", "Sweet", "Third", "Thunder", "Treasure", "Ultimate", "Wave", "Zephyr", "Zodiac" };
std::vector<std::string> aSufShip = { " Adventure", " Amore", " Angel", " Answer", " Attraction", " Bird", " Boat", " Body", " Bound", " Boy", " Breaker", " Breeze", " Cat", " Catcher", " Chaser", " Courier", " Crusher", " Devil", " Diamond", " Dog", " Dolphin", " Dream", " Dreamer", " Eagle", " Elf", " Fish", " Flash", " Flight", " Fox", " Girl", " Ghost", " Goose", " Gull", " Hawk", " Huntress", " Hunter", " Jack", " Jane", " Jewel", " Jumper", " Karma", " King", " Kiss", " Knight", " Lady", " Lion", " Love", " Lover", " Madness", " Magic", " Marie", " Minstrel", " Mist", " Mistake", " Money", " Monkey", " Monster", " Nest", " Nightmare", " Owl", " Queen", " Quest", " Pig", " Pirate", " Plainsman", " Phantom", " Power", " Presence", " Prince", " Princess", " Rainbow", " Rising", " Rose", " Runner", " Scare", " Seeker", " Sight", " Sirena", " Sixteen", " Shadow", " Shift", " Shine", " Stalker", " Stripe", " Song", " Spirit", " Spice", " Star", " Storm", " Swan", " Tide", " Tiger", " Toy", " Trouble", " Turtle", " Viking", " Unicorn", " Walker", " Wind", " Wine", " Wish", " Witch", " Wizard", " White", " Wolf", " Woman", " Zebra" };

std::vector<std::string> aPrefFemale = { "Ail", "Ara", "Ay", "Bren", "Astar", "Dae", "Dren", "Dwen", "El", "Erin", "Eth", "Fae", "Fay", "Gae", "Gay", "Glae", "Gwen", "Il", "Jey", "Lae", "Lan", "Lin", "Mae", "Mara", "More", "Mi", "Min", "Ne", "Nel", "Pae", "Pwen", "Rae", "Ray", "Re", "Ri", "Si", "Sal", "Say", "Tae", "Te", "Ti", "Tin", "Tir", "Vi", "Vul" };
std::vector<std::string> aSufFemale = { "ta", "alle", "ann", "arra", "aye", "da", "dolen", "ell", "enn", "eth", "eya", "fa", "fey", "ga", "gwenn", "hild", "ill", "ith", "la", "lana", "lar", "len", "lwen", "ma", "may", "na", "narra", "navia", "nwen", "ola", "pera", "pinn", "ra", "rann", "rell", "ress", "reth", "riss", "sa", "shann", "shara", "shea", "shell", "tarra", "tey", "ty", "unn", "ura", "valia", "vara", "vinn", "wen", "weth", "wynn", "wyrr", "ya", "ye", "yll", "ynd", "yrr", "yth" };

std::vector<std::string> aPrefMale = { "ache", "aim", "bald", "bear", "cron", "boar", "boast", "boil", "boni", "boy", "bower", "churl", "corn", "cuff", "dark", "dire", "dour", "dross", "dupe", "dusk", "dwar", "dwarf", "ebb", "el", "elf", "fag", "fate", "fay", "fell", "fly", "fowl", "gard", "gay", "gilt", "girth", "glut", "goad", "gold", "gorge", "grey", "groan", "haft", "hale", "hawk", "haught", "hiss", "hock", "hoof", "hook", "horn", "kin", "kith", "lank", "leaf", "lewd", "louse", "lure", "man", "mars", "meed", "moat", "mould", "muff", "muse", "not", "numb", "odd", "ooze", "ox", "pale", "port", "quid", "rau", "red", "rich", "rob", "rod", "rud", "ruff", "run", "rush", "scoff", "skew", "sky", "sly", "sow", "stave", "steed", "swar", "thor", "tort", "twig", "twit", "vain", "vent", "vile", "wail", "war", "whip", "wise", "worm", "yip" };
std::vector<std::string> aSufMale = { "os", "ard", "bald", "ban", "baugh", "bert", "brand", "cas", "celot", "cent", "cester", "cott", "dane", "dard", "doch", "dolph", "don", "doric", "dower", "dred", "fird", "ford", "fram", "fred", "frid", "fried", "gal", "gard", "gernon", "gill", "gurd", "gus", "ham", "hard", "hart", "helm", "horne", "ister", "kild", "lan", "lard", "ley", "lisle", "loch", "man", "mar", "mas", "mon", "mond", "mour", "mund", "nald", "nard", "nath", "ney", "olas", "pold", "rad", "ram", "rard", "red", "rence", "reth", "rick", "ridge", "riel", "ron", "rone", "roth", "sander", "sard", "shall", "shaw", "son", "steen", "stone", "ter", "than", "ther", "thon", "thur", "ton", "tor", "tran", "tus", "ulf", "vald", "van", "vard", "ven", "vid", "vred", "wald", "wallader", "ward", "werth", "wig", "win", "wood", "yard" };

std::vector<std::string> aEpithetMageM = { "Black", "White", "Blue", "Green", "Brown", "Ruthless", "Heartless", "Ugly", "Mad", "Careless", "Restless", "Impartial", "Immortal", "Colourless", "Callous", "Cruel", "Powerful", "Mage", "Evil", "Weak", "Wize", "Handless", "Furious", "Flamer", "Malicius" };
std::vector<std::string> aEpithetMageF = { "Black", "White", "Blue", "Green", "Brown", "Ruthless", "Heartless", "Ugly", "Mad", "Careless", "Restless", "Impartial", "Immortal", "Colourless", "Callous", "Cruel", "Powerful", "Witch", "Evil", "Weak", "Wize", "Handless", "Furious", "Malicius" };
std::vector<std::string> aEpithetClericM = { "Good", "Merciful", "Compassionate", "Healer", "Magnanimous", "Saint", "High Priest", "Hermit", "Monk", "Brother", "Father", "Cleric", "Wizard", "Elder", "Deathkiller", "Painkiller", "Reviver", "Accurate", "Silver", "Snake", "Silent", "Quiet", "Kindest", "Childless", "Herbologist", "Alchemist", "Modest" };
std::vector<std::string> aEpithetClericF = { "Good", "Merciful", "Compassionate", "Healer", "Magnanimous", "Saint", "High Priestess", "Cleric", "Sorceress", "Sister", "Mother", "Elder", "Accurate", "Silver", "Beautiful", "Silent", "Quiet", "Kindest", "Childless", "Herbologist", "Alchemist" };
std::vector<std::string> aEpithetShamanM = { "Summoner", "Shaman", "Chanter", "Scull", "Claw", "Warlock", "Dragon", "Elder", "Incomer", "Dominator", "Beastlord", "Master", "Outsider", "Exiled", "Halfhuman", "Ancient", "Returned", "Banisher", "Lonely", "Seeker", "Wereman" };
std::vector<std::string> aEpithetShamanF = { "Dominatrix", "Mistress", "Black Widow", "Ancient", "Lonely", "Werewoman" };
std::vector<std::string> aEpithetGeneralM = { "Ruthless", "Bloody", "Mighty", "Wild", /*"Demigod",*/ "Lord", "Headcutter", "Executor", "Loud", "Prince", "King", "Don", "Commander", "Great", "Scullsmasher", "Big Axe", "Heavy Axe", "Heavy Fist", "Quick Sword", "Golden Sword", "Silver Sword", "Iron Sword", "Iron Fist", "Wooden Sword", "Sharp Sword", "Deadly Blade", "Shining Sword", "Black Sword", "First Sword", "Champion", "Big Mug", "Demon Hunter", "Big Helm", "Horny", "Dragonslayer", "Peacemaker", "Liberator", "Barbarian", "Dark Knight", "White Knight", "Glorious", "One-eye", "Butcher", "Murderer", "Capitan", "General", "Warlord", "Chief" };
std::vector<std::string> aEpithetGeneralF = { "Ruthless", "Bloody", "Mighty", "Wild", /*"Demigoddess",*/ "Executor", "Princess", "Queen", "Great", "Lady", "Quick Sword", "Golden Sword", "Silver Sword", "Iron Sword", "Wooden Sword", "Sharp Sword", "Deadly Blade", "Shining Sword", "Black Sword", "Demon Hunter", "Dragonslayer", "Glorious", "Capitan", "General", "Warlord", "Chief" };

//---------------------------------------------------------------------------
// Production building name tables (by race/ethnicity)
//---------------------------------------------------------------------------

// Dwarven resource words
std::vector<std::string> aDwarfIron = { "uzun", "zikil", "zakar", "gol", "felak", "kibil", "mazar", "rukhs", "baraz" };
std::vector<std::string> aDwarfStone = { "barak", "gundil", "zaral", "zeg", "tumun", "brak", "khund", "ragal", "sark" };
std::vector<std::string> aDwarfWood = { "khelek", "zirik", "khim", "thrak", "binar" };
std::vector<std::string> aDwarfFood = { "dur", "mab", "kun", "zun", "khur", "breg" };
std::vector<std::string> aDwarfFur = { "guzib", "khuz", "snar", "durak" };
std::vector<std::string> aDwarfHerbs = { "lib", "niz", "ithil", "zarn" };
std::vector<std::string> aDwarfHorse = { "moril", "thizar", "brund", "garn" };
std::vector<std::string> aDwarfMine = { "gundag", "gundabad", "zad", "khazad", "khaz", "tum", "dumal" };
std::vector<std::string> aDwarfQuarry = { "barak", "gol", "nazar", "bragal", "sark" };
std::vector<std::string> aDwarfWorkshop = { "zal", "thibil", "agul", "mazgal", "thrakul" };
std::vector<std::string> aDwarfFarm = { "dal", "kuluth", "narg", "kudal" };
std::vector<std::string> aDwarfStable = { "uzar", "mur", "baruz", "ruk" };
std::vector<std::string> aDwarfOasis = { "zaram", "ithar", "zulun", "barith", "khurim" };
std::vector<std::string> aDwarfCamel = { "baruk", "thram", "zugal", "packdur" };

// Elven resource words
std::vector<std::string> aElfIron = { "thil", "ril", "tin", "mith", "celebrim" };
std::vector<std::string> aElfStone = { "gond", "rond", "sar", "edhel", "norn" };
std::vector<std::string> aElfWood = { "las", "taur", "orn", "galen", "echor", "tawar", "silva" };
std::vector<std::string> aElfFood = { "yav", "loth", "nan", "mir", "aes", "salph" };
std::vector<std::string> aElfFur = { "raw", "lhaw", "fael", "raew" };
std::vector<std::string> aElfHerbs = { "galen", "glas", "laeg", "saer", "lind", "niph" };
std::vector<std::string> aElfHorse = { "roch", "arod", "glin", "noroth" };
std::vector<std::string> aElfMine = { "fost", "grond", "mithrend", "dagnir" };
std::vector<std::string> aElfQuarry = { "rond", "sarn", "sirth", "celu" };
std::vector<std::string> aElfWorkshop = { "sammath", "angos", "curun", "gwaith" };
std::vector<std::string> aElfFarm = { "parth", "talath", "nan", "ethir", "parad" };
std::vector<std::string> aElfStable = { "ost", "bar", "rochben", "lodh" };
std::vector<std::string> aElfShrine = { "iant", "iaur", "fane", "aerlinn", "elbereth" };
std::vector<std::string> aElfOasis = { "lothwen", "nenar", "ethil", "galewen", "sirion" };
std::vector<std::string> aElfCamel = { "harad", "rochim", "lavan", "silroch" };

// Orcish resource words
std::vector<std::string> aOrcIron = { "dur", "nazg", "gul", "krug", "skorn", "blakst", "cruhor", "ironak" };
std::vector<std::string> aOrcStone = { "buz", "rod", "gar", "drak", "mog", "grit", "rubble", "hardbit" };
std::vector<std::string> aOrcWood = { "shak", "waz", "rag", "chop", "splint", "hackwz", "stumpk" };
std::vector<std::string> aOrcFood = { "narb", "ubal", "mog", "flesh", "grub", "slop", "gruel", "narbl" };
std::vector<std::string> aOrcFur = { "ghash", "shak", "hidek", "skal", "grakh", "skrag", "peltz" };
std::vector<std::string> aOrcHerbs = { "glob", "snaga", "rot", "slime", "muzg", "skagz", "blort" };
std::vector<std::string> aOrcHorse = { "lug", "tark", "snort", "beast", "lughak", "drakul", "snarg" };
std::vector<std::string> aOrcMine = { "gar", "gul", "durbul", "grukul", "skarg", "durzad", "mogar", "nazk" };
std::vector<std::string> aOrcQuarry = { "buz", "agal", "gorak", "crag", "buzgal", "krazg", "skorn" };
std::vector<std::string> aOrcWorkshop = { "zog", "uruk", "bang", "smash", "urgol", "grind", "drazh" };
std::vector<std::string> aOrcFarm = { "waz", "ubal", "wazok", "mud", "ubnak", "grunak", "mudar" };
std::vector<std::string> aOrcStable = { "lugburz", "lugdush", "lurkan", "maw", "gruksh", "mugoth", "kragul" };
std::vector<std::string> aOrcOasis = { "mudhole", "drinkpit", "ghashnar", "rotwater", "snar" };
std::vector<std::string> aOrcCamel = { "lug", "snorg", "gruk", "packul" };

// Human resource words
std::vector<std::string> aHumanIron = { "iron", "metal", "steel", "forge", "anvil", "ingot", "smithy" };
std::vector<std::string> aHumanStone = { "stone", "rock", "marble", "granite", "slab", "block", "mason" };
std::vector<std::string> aHumanWood = { "wood", "timber", "oak", "pine", "beam", "log", "plank" };
std::vector<std::string> aHumanFood = { "grain", "wheat", "barley", "corn", "rye", "oats", "meal" };
std::vector<std::string> aHumanFur = { "fur", "hide", "pelt", "leather", "skin", "tannery" };
std::vector<std::string> aHumanHerbs = { "herb", "sage", "wort", "bloom", "root", "leaf", "seed" };
std::vector<std::string> aHumanHorse = { "horse", "steed", "mare", "colt", "stud", "charger" };
std::vector<std::string> aHumanMine = { "mine", "pit", "delve", "shaft", "diggings", "deep" };
std::vector<std::string> aHumanQuarry = { "quarry", "stoneworks", "yard", "cut", "stonepit" };
std::vector<std::string> aHumanWorkshop = { "mill", "yard", "works", "shop", "foundry", "hall" };
std::vector<std::string> aHumanFarm = { "farm", "field", "grange", "croft", "stead", "hold" };
std::vector<std::string> aHumanStable = { "stable", "mews", "paddock", "barn", "stockyard" };
std::vector<std::string> aHumanShrine = { "temple", "shrine", "chapel", "fane", "sanctum", "altar" };
std::vector<std::string> aHumanOasis = { "oasis", "spring", "well", "greenrest", "waterhold" };
std::vector<std::string> aHumanCamel = { "camel", "dromedary", "packbeast", "caravan", "sandsteed" };

//---------------------------------------------------------------------------
// Advanced material resource words (by race)
//---------------------------------------------------------------------------

// Mithril (I_MITHRIL) - for Arcane Mine
std::vector<std::string> aDwarfMithril = { "kibil", "mithrim", "zirak", "silrak", "laug" };
std::vector<std::string> aElfMithril   = { "celebrin", "mithril", "thilorn", "silivren", "thinras" };
std::vector<std::string> aOrcMithril   = { "gilrok", "glenak", "lunzad", "shilok", "silvok" };
std::vector<std::string> aHumanMithril = { "mithril", "moonsteel", "starore", "brightmetal", "silvervein" };

// Rootstone (I_ROOTSTONE) - for Mystic Quarry
std::vector<std::string> aDwarfRootstone = { "rakhal", "ibrul", "zarim", "nuzum", "thibrak" };
std::vector<std::string> aElfRootstone   = { "celebrond", "sildur", "arnond", "lithron", "gonnsar" };
std::vector<std::string> aOrcRootstone   = { "rakbol", "gorith", "duzrak", "nardul", "oldrok" };
std::vector<std::string> aHumanRootstone = { "rootstone", "deeprock", "veinstone", "soulstone", "arkstone" };

// Ironwood (I_IRONWOOD) - for Forest Preserve
std::vector<std::string> aDwarfIronwood = { "zirik", "khelehorn", "gundtree", "mazorn", "bintree" };
std::vector<std::string> aElfIronwood   = { "galorn", "ninglor", "echor", "ornmeth", "gladil" };
std::vector<std::string> aOrcIronwood   = { "hardtree", "irontree", "metalwood", "stifftwig", "stonebark" };
std::vector<std::string> aHumanIronwood = { "ironwood", "hardwood", "armorwood", "steelwood", "bladewood" };

// Yew (I_YEW) - for Sacred Grove
std::vector<std::string> aDwarfYew = { "ulinoth", "zahalorn", "ithtree", "spiritwood", "elderbark" };
std::vector<std::string> aElfYew   = { "galadh", "lothornl", "yavanna", "celeborn", "neldor" };
std::vector<std::string> aOrcYew   = { "mordak", "gorak", "durwig", "ghostk", "nazbol" };
std::vector<std::string> aHumanYew = { "yew", "spiritwood", "blessedwood", "elderwood", "sacredoak" };

// Winged Horse (I_WHORSE) - for Mythic Stables
std::vector<std::string> aDwarfWhorse = { "winguzar", "skyruk", "cloudmur", "windsteed", "airbaruk" };
std::vector<std::string> aElfWhorse   = { "roccoair", "skyarod", "winglin", "celebroch", "aearoch" };
std::vector<std::string> aOrcWhorse   = { "skywng", "skyrak", "guzwng", "flyrak", "airmak" };
std::vector<std::string> aHumanWhorse = { "winged", "skymare", "cloudsteed", "windmount", "skyhorse" };

// Floater (I_FLOATER) - for Trapping Lodge
std::vector<std::string> aDwarfFloater = { "airbaraz", "floattum", "skyuzar", "windgol", "cloudzad" };
std::vector<std::string> aElfFloater   = { "aerraw", "cloudlhaw", "skyfael", "windraew", "airsnare" };
std::vector<std::string> aOrcFloater   = { "luftak", "airgit", "guzbol", "winsnag", "skygit" };
std::vector<std::string> aHumanFloater = { "floater", "cloudtrap", "skysnare", "windtrap", "airhunt" };

// Mushroom (I_MUSHROOM) - for Faerie Ring
std::vector<std::string> aDwarfMushroom = { "zarithniz", "spiritlib", "faeithil", "moonspore", "mysticzarn" };
std::vector<std::string> aElfMushroom   = { "faeriegalen", "spiritglas", "lindlaeg", "moonspore", "aerniph" };
std::vector<std::string> aOrcMushroom   = { "shrumk", "rotnak", "darkspor", "slimak", "fungak" };
std::vector<std::string> aHumanMushroom = { "mushroom", "moonspore", "elderbloom", "nightcap", "fairybloom" };

// Adamantium (I_ADMANTIUM) - for Alchemist Lab
std::vector<std::string> aDwarfAdamant = { "admant-zad", "voiduzun", "shadowzakar", "blackgol", "darkzikil" };
std::vector<std::string> aElfAdamant   = { "voidmith", "shadowthil", "darkril", "eluchil", "voidtin" };
std::vector<std::string> aOrcAdamant   = { "grukpit", "shadok", "voidgut", "guldak", "adamok" };
std::vector<std::string> aHumanAdamant = { "adamantine", "darksteel", "voidmetal", "shadowore", "starstone" };

//---------------------------------------------------------------------------
// Advanced building type words (by race)
//---------------------------------------------------------------------------

// Forest Preserve (O_PRESERVE)
std::vector<std::string> aDwarfPreserve = { "ithilorn", "bintaur", "malbethgal", "urnorn", "zirikgal" };
std::vector<std::string> aElfPreserve   = { "galadhbar", "taurcaer", "silvaost", "woodbarad", "foresttham" };
std::vector<std::string> aOrcPreserve   = { "warhut", "trohol", "barkut", "gruvak", "holtok" };
std::vector<std::string> aHumanPreserve = { "preserve", "grove", "woodland", "greenhold", "forestkeep" };

// Sacred Grove (O_SACGROVE)
std::vector<std::string> aDwarfGrove = { "ulinbarak", "zahalgal", "spiritgal", "holybund", "sacredgundag" };
std::vector<std::string> aElfGrove   = { "galadhiaur", "yavannaost", "blessedbar", "spirittham", "holyminas" };
std::vector<std::string> aOrcGrove   = { "dargal", "nazpit", "gholhol", "gorgrv", "shadgal" };
std::vector<std::string> aHumanGrove = { "sacred grove", "holy grove", "spirit grove", "elder grove", "blessed grove" };

// Faerie Ring (O_FAERIERING)
std::vector<std::string> aDwarfRing = { "spiritbund", "faezaral", "mysticgol", "faenazar", "holybarak" };
std::vector<std::string> aElfRing   = { "faerierond", "spiritost", "blessedrond", "faecaer", "spirittham" };
std::vector<std::string> aOrcRing   = { "gholzad", "darkrng", "curnak", "ghstok", "bonzad" };
std::vector<std::string> aHumanRing = { "faerie ring", "spirit circle", "mystic ring", "fae circle", "enchanted ring" };

// Alchemist Lab (O_ALCHEMISTLAB)
std::vector<std::string> aDwarfLab = { "greatzal", "masterthibil", "arcaneagul", "spiritmazgal", "voidthrakul" };
std::vector<std::string> aElfLab   = { "arcanesammath", "masterangos", "spiritcurun", "voidgwaith", "greatangos" };
std::vector<std::string> aOrcLab   = { "arkzog", "ghulurk", "voidzg", "mastzog", "drkfrg" };
std::vector<std::string> aHumanLab = { "laboratory", "alchemy works", "arcane hall", "spirit forge", "void lab" };

// Trapping Lodge (O_TRAPPINGLODGE)
std::vector<std::string> aDwarfLodge = { "grandguzib", "lodgekhuz", "mastersnar", "huntdurak", "greatthizar" };
std::vector<std::string> aElfLodge   = { "grandfael", "masterlhaw", "huntraew", "greatraw", "lordfael" };
std::vector<std::string> aOrcLodge   = { "grugal", "warbunk", "maskhid", "huntskal", "lordnk" };
std::vector<std::string> aHumanLodge = { "lodge", "hunting hall", "trappers hall", "manor", "great hall" };

//---------------------------------------------------------------------------
// ROAD NAMING SYSTEM - Cultural road type words per ethnicity
//---------------------------------------------------------------------------
// Road type words meaning "road/path/track/way" in each culture's language
std::vector<std::string> aDwarfRoad  = { "gard", "tum", "raz", "naz", "zul", "keled" };
std::vector<std::string> aElfRoad    = { "iant", "men", "peth", "rath", "sir", "lond" };
std::vector<std::string> aOrcRoad    = { "gar", "naz", "gul", "raz", "gruk", "bol" };
std::vector<std::string> aNomadRoad  = { "tariq", "sikka", "darb", "maslak" };
std::vector<std::string> aHumanRoad  = { "Road", "Path", "Trail", "Track", "Way", "Tract" };

//---------------------------------------------------------------------------
// CARAVANSERAI NAMING SYSTEM - Trade waystation words per ethnicity
//---------------------------------------------------------------------------
// "What moves through / is valued" — Dwarven (precious metals / packed goods)
std::vector<std::string> aDwarfCaravanTrade = { "kibil", "baruk", "khad", "mazar", "durak" };
// "The structure itself" — Dwarven (hall / workshop / chamber)
std::vector<std::string> aDwarfCaravanHall  = { "zal", "dum", "thibil", "bar", "tum" };
// "What moves through / is valued" — Elvish (silver / path / guardian)
std::vector<std::string> aElfCaravanTrade   = { "celeb", "mith", "pant", "tirn", "narw" };
// "The structure itself" — Elvish (dwelling / place / chambers)
std::vector<std::string> aElfCaravanHall    = { "bar", "ost", "sammath", "gwaith", "lodh" };
// "What moves through / is valued" — Orcish (iron / haul / loot)
std::vector<std::string> aOrcCaravanTrade   = { "dur", "lug", "skag", "gash", "drag" };
// "The structure itself" — Orcish (den / post / grind)
std::vector<std::string> aOrcCaravanHall    = { "zog", "bang", "drazh", "urgol", "grind" };
// Nomad type words — historically authentic Arabic/Persian terms for caravan inn
std::vector<std::string> aNomadCaravan      = { "Khan", "Serai", "Funduq", "Ribat", "Wikala" };
// Human trade and building words (English)
std::vector<std::string> aHumanCaravanTrade = { "road", "merchant", "trade", "caravan", "way" };
std::vector<std::string> aHumanCaravanHall  = { "inn", "post", "house", "hall", "lodge" };

//---------------------------------------------------------------------------
// LAIR NAMING SYSTEM - Monster and Lair Type Words
//---------------------------------------------------------------------------

// Dwarven monster words
std::vector<std::string> aDwarfDragon = { "draug", "lhug", "urkul", "azag", "ghazan", "smaug" };
std::vector<std::string> aDwarfTrent = { "durad", "ornul", "thrandul", "malbeth", "galorn", "entuk" };
std::vector<std::string> aDwarfDemon = { "goroth", "balrog", "gorthaur", "morgoth", "udun", "nazak" };
std::vector<std::string> aDwarfTroll = { "throg", "olog", "urak", "grond", "throk", "bolog" };
std::vector<std::string> aDwarfUndead = { "gul", "morgul", "dushgoi", "nazgul", "barrow", "tumun" };
std::vector<std::string> aDwarfCentaur = { "taur", "rath", "rundak", "kevar", "horath", "durath" };
std::vector<std::string> aDwarfBeast = { "karak", "uruk", "ghul", "baraz", "tharg", "durak" };

// Dwarven lair type words
std::vector<std::string> aDwarfCave = { "dum", "grond", "zad", "aghan", "khazad", "nurn" };
std::vector<std::string> aDwarfLair = { "ost", "gard", "durbul", "kheled", "bund", "gundag" };
std::vector<std::string> aDwarfRuin = { "nurn", "bragar", "zahar", "keled", "dumal", "gundabad" };
std::vector<std::string> aDwarfPit = { "gar", "ghash", "buzar", "dushgoi", "gorog", "garaz" };
std::vector<std::string> aDwarfCrypt = { "tumun", "guldur", "morgul", "nazar", "baruk", "zahal" };
std::vector<std::string> aDwarfShaft = { "gundag", "khazad", "zirak", "tharaz", "kibil", "mazal" };

// Elven monster words
std::vector<std::string> aElfDragon = { "lhug", "rauko", "angulok", "orme", "loke", "uruloke" };
std::vector<std::string> aElfTrent = { "ent", "onod", "tawar", "ornul", "galadh", "mallorn" };
std::vector<std::string> aElfDemon = { "rauko", "valaraukar", "gothmog", "morgoth", "moringotto", "bauglir" };
std::vector<std::string> aElfTroll = { "torog", "olog", "draugluin", "carcharoth", "ungol", "athelas" };
std::vector<std::string> aElfUndead = { "gul", "morgul", "uvanimor", "fea", "houseless", "barrow" };
std::vector<std::string> aElfCentaur = { "rocco", "taur", "rath", "rond", "celeb", "galad" };
std::vector<std::string> aElfBeast = { "rauko", "uruk", "narmos", "lhang", "angulok", "carak" };

// Elven lair type words
std::vector<std::string> aElfCave = { "rond", "groth", "roth", "fela", "gondolin", "nargothrond" };
std::vector<std::string> aElfLair = { "ost", "barad", "minas", "iaur", "caer", "tham" };
std::vector<std::string> aElfRuin = { "iaur", "fallen", "loss", "annon", "daedhelos", "gondolin" };
std::vector<std::string> aElfPit = { "dum", "moria", "udun", "thangorodrim", "angband", "utumno" };
std::vector<std::string> aElfCrypt = { "haudh", "tumun", "gurth", "barrow", "sarn", "gwathuirim" };
std::vector<std::string> aElfShaft = { "tham", "groth", "fela", "orod", "eithel", "annon" };

// Orcish monster words
std::vector<std::string> aOrcDragon = { "lhug", "ghazan", "firetooth", "smokewing", "smaug", "scatha" };
std::vector<std::string> aOrcTrent = { "treegit", "woodrot", "bark", "rootmaw", "thrangul", "splinter" };
std::vector<std::string> aOrcDemon = { "goroth", "flame", "burnlord", "hellspawn", "firegit", "ashbringer" };
std::vector<std::string> aOrcTroll = { "throg", "stonegit", "rockhead", "mountainmaw", "bonecrush", "olog" };
std::vector<std::string> aOrcUndead = { "gul", "deadgit", "bonewraith", "rotlord", "corpse", "deathspawn" };
std::vector<std::string> aOrcCentaur = { "halfgit", "horsemeat", "prancegit", "hoofhead", "taur", "galop" };
std::vector<std::string> aOrcBeast = { "claw", "fang", "bloodmaw", "killer", "slasher", "ripper" };

// Orcish lair type words
std::vector<std::string> aOrcCave = { "hai", "snaga", "hole", "burz", "ghash", "durbul" };
std::vector<std::string> aOrcLair = { "gar", "burz", "durb", "ukh", "grishnakh", "lugburz" };
std::vector<std::string> aOrcRuin = { "gijak", "broken", "oldgit", "rubble", "smashed", "wrecked" };
std::vector<std::string> aOrcPit = { "gul", "hellhole", "bloodpit", "darkhole", "ghashnar", "skullpit" };
std::vector<std::string> aOrcCrypt = { "guldur", "bonepile", "deathhole", "morgul", "corpsepit", "gravegit" };
std::vector<std::string> aOrcShaft = { "deephole", "darkshaft", "durbul", "ghashgul", "blackpit", "dighole" };

// Human monster words
std::vector<std::string> aHumanDragon = { "wyrm", "drake", "serpent", "firedrake", "worm", "winged" };
std::vector<std::string> aHumanTrent = { "treant", "ent", "guardian", "ancient", "elder", "walking" };
std::vector<std::string> aHumanDemon = { "demon", "devil", "fiend", "hellspawn", "infernal", "abyssal" };
std::vector<std::string> aHumanTroll = { "troll", "giant", "ogre", "brute", "behemoth", "colossus" };
std::vector<std::string> aHumanUndead = { "wraith", "ghost", "specter", "phantom", "revenant", "shade" };
std::vector<std::string> aHumanCentaur = { "centaur", "halfling", "horseman", "wildrunner", "plainsfolk", "horsefolk" };
std::vector<std::string> aHumanBeast = { "beast", "creature", "monster", "predator", "stalker", "hunter" };

// Human lair type words
std::vector<std::string> aHumanCave = { "cave", "cavern", "hollow", "grotto", "den", "warren" };
std::vector<std::string> aHumanLair = { "lair", "den", "nest", "dwelling", "haunt", "hideout" };
std::vector<std::string> aHumanRuin = { "ruins", "wreck", "remains", "rubble", "fallen", "forsaken" };
std::vector<std::string> aHumanPit = { "pit", "abyss", "chasm", "hellgate", "maw", "depths" };
std::vector<std::string> aHumanCrypt = { "crypt", "tomb", "grave", "barrow", "cairn", "sepulcher" };
std::vector<std::string> aHumanShaft = { "shaft", "mine", "delve", "tunnel", "passage", "dungeon" };

//---------------------------------------------------------------------------

std::string getPrefix(std::vector<std::string>& prefixTable) {
    return rng::one_of(prefixTable);
}

std::string getSuffix(std::vector<std::string>& suffixTable) {
    return rng::one_of(suffixTable);
}

std::string getEthnicPrefix(Ethnicity etnos) {
    switch(etnos) {
        case Ethnicity::VIKING:    return getPrefix(aPrefViking);
        case Ethnicity::BARBARIAN: return getPrefix(aPrefScotish);
        case Ethnicity::MAN:       return getPrefix(aPrefHumans);
        case Ethnicity::ESKIMO:    return getPrefix(aPrefEscimo);
        case Ethnicity::NOMAD:     return getPrefix(aPrefArabic);
        case Ethnicity::TRIBESMAN: return getPrefix(aPrefAfrica);
        case Ethnicity::HIGHELF:   return getPrefix(aPrefElven1);
        case Ethnicity::ELF:       return getPrefix(aPrefElven2);
        case Ethnicity::DWARF:     return getPrefix(aPrefDwarven);
        case Ethnicity::ORC:       return getPrefix(aPrefOrchish);
        case Ethnicity::LIZARDMAN: return getPrefix(aPrefGreek);
        case Ethnicity::DROW:      return getPrefix(aPrefDrow);
        case Ethnicity::TITAN:     return getPrefix(aPrefAztec);
        default:                   return getPrefix(aPrefMale);
    }
}

std::string getEthnicSuffix(Ethnicity etnos) {
    switch(etnos) {
        case Ethnicity::VIKING:    return getSuffix(aSufViking);
        case Ethnicity::BARBARIAN: return getSuffix(aSufScotish);
        case Ethnicity::MAN:       return getSuffix(aSufHumans);
        case Ethnicity::ESKIMO:    return getSuffix(aSufEscimo);
        case Ethnicity::NOMAD:     return getSuffix(aSufArabic);
        case Ethnicity::TRIBESMAN: return getSuffix(aSufAfrica);
        case Ethnicity::HIGHELF:   return getSuffix(aSufElven1);
        case Ethnicity::ELF:       return getSuffix(aSufElven2);
        case Ethnicity::DWARF:     return getSuffix(aSufDwarven);
        case Ethnicity::ORC:       return getSuffix(aSufOrchish);
        case Ethnicity::LIZARDMAN: return getSuffix(aSufGreek);
        case Ethnicity::DROW:      return getSuffix(aSufDrow);
        case Ethnicity::TITAN:     return getSuffix(aSufAztec);
        default:                   return getSuffix(aSufMale);
    }
}

std::string getAbstractName() {
    std::string first = getPrefix(aPrefAbstract);
    std::string second = getSuffix(aSufAbstract);

    return (first + second) | filter::capitalize;
}

std::string getEthnicName(Ethnicity etnos) {
    std::string first = getEthnicPrefix(etnos);
    std::string second = getEthnicSuffix(etnos);

    return (first + second) | filter::capitalize;
}

std::string getShipName() {
    std::string first = getPrefix(aPrefShip);
    std::string second = getSuffix(aSufShip);

    return (first + second) | filter::capitalize;
}

std::string getFortressName(const ObjectType& type) {
    std::string first = getPrefix(aPrefFort);
    std::string second = getSuffix(aSufFort);

    if (second.empty()) {
        second = type.name;
    }

    return (first + second) | filter::capitalize;
}

std::string getInnName() {
    std::string first = getPrefix(aPrefInn);
    std::string second = getSuffix(aSufInn);

    return (first + second) | filter::capitalize;
}

/**
 * @brief Generate a culturally-flavoured road name based on direction and builder race.
 *
 * Format by ethnicity:
 *   Dwarf/Elf/Orc: prefix + "-" + dir + "-" + roadWord  (e.g. "Khuz-north-gard")
 *   Nomad:         prefix + " " + dir + " " + roadWord  (e.g. "Al northeast tariq")
 *   Human/default: dir + " " + roadWord                 (e.g. "North Road")
 *
 * @param objectType  One of O_ROADN, O_ROADNE, O_ROADNW, O_ROADS, O_ROADSE, O_ROADSW
 * @param race        Builder's primary race item ID (falls back to MAN if unknown)
 */
std::string getRoadName(int objectType, int race) {
    // Map object type to English direction word
    std::string dir;
    switch(objectType) {
        case O_ROADN:  dir = "north";     break;
        case O_ROADNE: dir = "northeast"; break;
        case O_ROADNW: dir = "northwest"; break;
        case O_ROADS:  dir = "south";     break;
        case O_ROADSE: dir = "southeast"; break;
        case O_ROADSW: dir = "southwest"; break;
        default:       dir = "north";     break;
    }

    Ethnicity ethnicity = raceToEthnicity(race);
    std::string prefix;
    std::string roadWord;

    switch(ethnicity) {
        case Ethnicity::DWARF:
            prefix = getPrefix(aPrefDwarven);
            roadWord = rng::one_of(aDwarfRoad);
            // Example: "Khuz-north-gard"
            return (prefix + "-" + dir + "-" + roadWord) | filter::capitalize;

        case Ethnicity::ELF:
        case Ethnicity::HIGHELF:
            prefix = getPrefix(aPrefElven2);
            roadWord = rng::one_of(aElfRoad);
            // Example: "Nim-north-men"
            return (prefix + "-" + dir + "-" + roadWord) | filter::capitalize;

        case Ethnicity::ORC:
            prefix = getPrefix(aPrefOrchish);
            roadWord = rng::one_of(aOrcRoad);
            // Example: "Lurg-north-gar"
            return (prefix + "-" + dir + "-" + roadWord) | filter::capitalize;

        case Ethnicity::NOMAD:
            prefix = getPrefix(aPrefArabic);
            roadWord = rng::one_of(aNomadRoad);
            // Example: "Al northeast tariq"
            return (prefix + " " + dir + " " + roadWord) | filter::title_case;

        case Ethnicity::MAN:
        case Ethnicity::VIKING:
        case Ethnicity::BARBARIAN:
        default:
            roadWord = rng::one_of(aHumanRoad);
            // Example: "North Road", "Southwest Trail"
            return (dir + " " + roadWord) | filter::title_case;
    }
}

/**
 * @brief Generates a culturally-themed name for a Caravanserai (trade waystation).
 *
 * Each culture names their caravanserai differently:
 * - Nomad:   historically authentic Arabic/Persian terms (Khan, Serai, Funduq, ...)
 *            Format: prefix + typeWord  → "Ali Khan", "Bab Serai"
 * - Dwarf:   prefix + trade-word + hall-word → "Khuz Kibil Zal"
 * - Elf:     prefix + path-word + haven-word → "Nim Celeb Bar"
 * - Orc:     prefix + haul-word + den-word   → "Ghash Dur Zog"
 * - Human/default: prefix + trade-word + building-word → "Brent Trade Hall"
 *
 * @param race  Builder's race item ID (0 or unknown → human format)
 * @return      Title-cased name string
 */
std::string getCaravanseraiName(int race) {
    Ethnicity ethnicity = raceToEthnicity(race);
    std::string prefix;

    switch(ethnicity) {
        case Ethnicity::DWARF: {
            prefix = getPrefix(aPrefDwarven);
            std::string tradeWord = rng::one_of(aDwarfCaravanTrade);
            std::string hallWord  = rng::one_of(aDwarfCaravanHall);
            return (prefix + " " + tradeWord + " " + hallWord) | filter::title_case;
        }
        case Ethnicity::ELF:
        case Ethnicity::HIGHELF: {
            prefix = getPrefix(aPrefElven2);
            std::string tradeWord = rng::one_of(aElfCaravanTrade);
            std::string hallWord  = rng::one_of(aElfCaravanHall);
            return (prefix + " " + tradeWord + " " + hallWord) | filter::title_case;
        }
        case Ethnicity::ORC: {
            prefix = getPrefix(aPrefOrchish);
            std::string tradeWord = rng::one_of(aOrcCaravanTrade);
            std::string hallWord  = rng::one_of(aOrcCaravanHall);
            return (prefix + " " + tradeWord + " " + hallWord) | filter::title_case;
        }
        case Ethnicity::NOMAD: {
            prefix = getPrefix(aPrefArabic);
            std::string typeWord = rng::one_of(aNomadCaravan);
            return (prefix + " " + typeWord) | filter::title_case;
        }
        case Ethnicity::VIKING:
        case Ethnicity::BARBARIAN:
        case Ethnicity::MAN:
        default: {
            prefix = (ethnicity == Ethnicity::VIKING)    ? getPrefix(aPrefViking) :
                     (ethnicity == Ethnicity::BARBARIAN) ? getPrefix(aPrefScotish) :
                                                           getPrefix(aPrefHumans);
            std::string tradeWord = rng::one_of(aHumanCaravanTrade);
            std::string hallWord  = rng::one_of(aHumanCaravanHall);
            return (prefix + " " + tradeWord + " " + hallWord) | filter::title_case;
        }
    }
}

// Helper function: convert race ID to ethnicity for name generation
Ethnicity raceToEthnicity(int race) {
    switch(race) {
        // Dwarves
        case I_ICEDWARF:
        case I_HILLDWARF:
        case I_UNDERDWARF:
        case I_DESERTDWARF:
        case I_GNOME:
            return Ethnicity::DWARF;

        // Elves (including fey-like races)
        case I_WOODELF:
        case I_SEAELF:
        case I_HIGHELF:
        case I_TRIBALELF:
        case I_DROWMAN:
        case I_TIEFLING:
        case I_FAIRY:
            return Ethnicity::ELF;

        // Orcs and goblinoids
        case I_ORC:
        case I_LIZARDMAN:
        case I_GOBLINMAN:
            return Ethnicity::ORC;

        // Nomads and desert folk
        case I_NOMAD:
        case I_ESKIMO:
        case I_CENTAURMAN:
            return Ethnicity::NOMAD;

        // Vikings
        case I_VIKING:
            return Ethnicity::VIKING;

        // Tribal/barbarian
        case I_BARBARIAN:
        case I_TRIBESMAN:
            return Ethnicity::BARBARIAN;

        // Humans and hobbits (default)
        case I_MAN:
        case I_PLAINSMAN:
        case I_DARKMAN:
        case I_HOBBIT:
        default:
            return Ethnicity::MAN;
    }
}

// Generate production building name based on race and resource
std::string getProductionBuildingName(int buildingType, int resourceType, int race) {
    Ethnicity ethnicity = raceToEthnicity(race);

    std::string resourceWord;
    std::string buildingWord;
    std::string prefix;

    // Select resource-specific word based on ethnicity
    switch(ethnicity) {
        case Ethnicity::DWARF:
            prefix = getPrefix(aPrefDwarven);

            // Resource words
            switch(resourceType) {
                case I_IRON:      resourceWord = rng::one_of(aDwarfIron); break;
                case I_STONE:     resourceWord = rng::one_of(aDwarfStone); break;
                case I_WOOD:      resourceWord = rng::one_of(aDwarfWood); break;
                case I_GRAIN:
                case I_LIVESTOCK: resourceWord = rng::one_of(aDwarfFood); break;
                case I_FUR:       resourceWord = rng::one_of(aDwarfFur); break;
                case I_HERBS:     resourceWord = rng::one_of(aDwarfHerbs); break;
                case I_HORSE:     resourceWord = rng::one_of(aDwarfHorse); break;
                case I_CAMEL:     resourceWord = rng::one_of(aDwarfCamel); break;
                case I_MITHRIL:   resourceWord = rng::one_of(aDwarfMithril); break;
                case I_ROOTSTONE: resourceWord = rng::one_of(aDwarfRootstone); break;
                case I_IRONWOOD:  resourceWord = rng::one_of(aDwarfIronwood); break;
                case I_YEW:       resourceWord = rng::one_of(aDwarfYew); break;
                case I_WHORSE:    resourceWord = rng::one_of(aDwarfWhorse); break;
                case I_FLOATER:   resourceWord = rng::one_of(aDwarfFloater); break;
                case I_MUSHROOM:  resourceWord = rng::one_of(aDwarfMushroom); break;
                case I_ADMANTIUM: resourceWord = rng::one_of(aDwarfAdamant); break;
                default:          resourceWord = "barak";
            }

            // Building type words
            switch(buildingType) {
                case O_MINE:          buildingWord = rng::one_of(aDwarfMine); break;
                case O_QUARRY:        buildingWord = rng::one_of(aDwarfQuarry); break;
                case O_TIMBERYARD:    buildingWord = rng::one_of(aDwarfWorkshop); break;
                case O_FARM:
                case O_RANCH:         buildingWord = rng::one_of(aDwarfFarm); break;
                case O_STABLE:        buildingWord = rng::one_of(aDwarfStable); break;
                case O_OASIS:         buildingWord = rng::one_of(aDwarfOasis); break;
                case O_TRAPPINGHUT:   buildingWord = rng::one_of(aDwarfWorkshop); break;
                case O_TEMPLE:        buildingWord = rng::one_of(aDwarfQuarry); break; // stone hall
                case O_AMINE:         buildingWord = rng::one_of(aDwarfMine); break;
                case O_MQUARRY:       buildingWord = rng::one_of(aDwarfQuarry); break;
                case O_MSTABLE:       buildingWord = rng::one_of(aDwarfStable); break;
                case O_PRESERVE:      buildingWord = rng::one_of(aDwarfPreserve); break;
                case O_SACGROVE:      buildingWord = rng::one_of(aDwarfGrove); break;
                case O_FAERIERING:    buildingWord = rng::one_of(aDwarfRing); break;
                case O_ALCHEMISTLAB:  buildingWord = rng::one_of(aDwarfLab); break;
                case O_TRAPPINGLODGE: buildingWord = rng::one_of(aDwarfLodge); break;
                default:              buildingWord = "zal";
            }

            // Dwarven format: prefix + resource + building
            // Example: "Khuz-uzun-gundag" = "Deep Iron Mine"
            return (prefix + "-" + resourceWord + "-" + buildingWord) | filter::capitalize;

        case Ethnicity::ELF:
        case Ethnicity::HIGHELF:
            prefix = getPrefix(aPrefElven2);

            // Resource words
            switch(resourceType) {
                case I_IRON:      resourceWord = rng::one_of(aElfIron); break;
                case I_STONE:     resourceWord = rng::one_of(aElfStone); break;
                case I_WOOD:      resourceWord = rng::one_of(aElfWood); break;
                case I_GRAIN:
                case I_LIVESTOCK: resourceWord = rng::one_of(aElfFood); break;
                case I_FUR:       resourceWord = rng::one_of(aElfFur); break;
                case I_HERBS:     resourceWord = rng::one_of(aElfHerbs); break;
                case I_HORSE:     resourceWord = rng::one_of(aElfHorse); break;
                case I_CAMEL:     resourceWord = rng::one_of(aElfCamel); break;
                case I_MITHRIL:   resourceWord = rng::one_of(aElfMithril); break;
                case I_ROOTSTONE: resourceWord = rng::one_of(aElfRootstone); break;
                case I_IRONWOOD:  resourceWord = rng::one_of(aElfIronwood); break;
                case I_YEW:       resourceWord = rng::one_of(aElfYew); break;
                case I_WHORSE:    resourceWord = rng::one_of(aElfWhorse); break;
                case I_FLOATER:   resourceWord = rng::one_of(aElfFloater); break;
                case I_MUSHROOM:  resourceWord = rng::one_of(aElfMushroom); break;
                case I_ADMANTIUM: resourceWord = rng::one_of(aElfAdamant); break;
                default:          resourceWord = "las";
            }

            // Building type words
            switch(buildingType) {
                case O_MINE:          buildingWord = rng::one_of(aElfMine); break;
                case O_QUARRY:        buildingWord = rng::one_of(aElfQuarry); break;
                case O_TIMBERYARD:    buildingWord = rng::one_of(aElfWorkshop); break;
                case O_FARM:
                case O_RANCH:         buildingWord = rng::one_of(aElfFarm); break;
                case O_STABLE:        buildingWord = rng::one_of(aElfStable); break;
                case O_OASIS:         buildingWord = rng::one_of(aElfOasis); break;
                case O_TRAPPINGHUT:   buildingWord = rng::one_of(aElfWorkshop); break;
                case O_TEMPLE:        buildingWord = rng::one_of(aElfShrine); break;
                case O_AMINE:         buildingWord = rng::one_of(aElfMine); break;
                case O_MQUARRY:       buildingWord = rng::one_of(aElfQuarry); break;
                case O_MSTABLE:       buildingWord = rng::one_of(aElfStable); break;
                case O_PRESERVE:      buildingWord = rng::one_of(aElfPreserve); break;
                case O_SACGROVE:      buildingWord = rng::one_of(aElfGrove); break;
                case O_FAERIERING:    buildingWord = rng::one_of(aElfRing); break;
                case O_ALCHEMISTLAB:  buildingWord = rng::one_of(aElfLab); break;
                case O_TRAPPINGLODGE: buildingWord = rng::one_of(aElfLodge); break;
                default:              buildingWord = "ost";
            }

            // Elven format: prefix + resource + building
            // Example: "Nim-las-sammath" = "White Forest Workshop"
            return (prefix + "-" + resourceWord + "-" + buildingWord) | filter::capitalize;

        case Ethnicity::ORC:
            prefix = getPrefix(aPrefOrchish);

            // Resource words
            switch(resourceType) {
                case I_IRON:      resourceWord = rng::one_of(aOrcIron); break;
                case I_STONE:     resourceWord = rng::one_of(aOrcStone); break;
                case I_WOOD:      resourceWord = rng::one_of(aOrcWood); break;
                case I_GRAIN:
                case I_LIVESTOCK: resourceWord = rng::one_of(aOrcFood); break;
                case I_FUR:       resourceWord = rng::one_of(aOrcFur); break;
                case I_HERBS:     resourceWord = rng::one_of(aOrcHerbs); break;
                case I_HORSE:     resourceWord = rng::one_of(aOrcHorse); break;
                case I_CAMEL:     resourceWord = rng::one_of(aOrcCamel); break;
                case I_MITHRIL:   resourceWord = rng::one_of(aOrcMithril); break;
                case I_ROOTSTONE: resourceWord = rng::one_of(aOrcRootstone); break;
                case I_IRONWOOD:  resourceWord = rng::one_of(aOrcIronwood); break;
                case I_YEW:       resourceWord = rng::one_of(aOrcYew); break;
                case I_WHORSE:    resourceWord = rng::one_of(aOrcWhorse); break;
                case I_FLOATER:   resourceWord = rng::one_of(aOrcFloater); break;
                case I_MUSHROOM:  resourceWord = rng::one_of(aOrcMushroom); break;
                case I_ADMANTIUM: resourceWord = rng::one_of(aOrcAdamant); break;
                default:          resourceWord = "dur";
            }

            // Building type words
            switch(buildingType) {
                case O_MINE:          buildingWord = rng::one_of(aOrcMine); break;
                case O_QUARRY:        buildingWord = rng::one_of(aOrcQuarry); break;
                case O_TIMBERYARD:    buildingWord = rng::one_of(aOrcWorkshop); break;
                case O_FARM:
                case O_RANCH:         buildingWord = rng::one_of(aOrcFarm); break;
                case O_STABLE:        buildingWord = rng::one_of(aOrcStable); break;
                case O_OASIS:         buildingWord = rng::one_of(aOrcOasis); break;
                case O_TRAPPINGHUT:   buildingWord = rng::one_of(aOrcWorkshop); break;
                case O_TEMPLE:        buildingWord = rng::one_of(aOrcWorkshop); break; // dark shrine
                case O_AMINE:         buildingWord = rng::one_of(aOrcMine); break;
                case O_MQUARRY:       buildingWord = rng::one_of(aOrcQuarry); break;
                case O_MSTABLE:       buildingWord = rng::one_of(aOrcStable); break;
                case O_PRESERVE:      buildingWord = rng::one_of(aOrcPreserve); break;
                case O_SACGROVE:      buildingWord = rng::one_of(aOrcGrove); break;
                case O_FAERIERING:    buildingWord = rng::one_of(aOrcRing); break;
                case O_ALCHEMISTLAB:  buildingWord = rng::one_of(aOrcLab); break;
                case O_TRAPPINGLODGE: buildingWord = rng::one_of(aOrcLodge); break;
                default:              buildingWord = "gar";
            }

            // Orcish format: prefix + resource + building
            // Example: "Ghash-dur-gar" = "Fire Black Pit"
            return (prefix + "-" + resourceWord + "-" + buildingWord) | filter::capitalize;

        case Ethnicity::MAN:
        case Ethnicity::VIKING:
        case Ethnicity::BARBARIAN:
        case Ethnicity::NOMAD:
        default:
            // Humans use more readable format
            prefix = (ethnicity == Ethnicity::VIKING) ? getPrefix(aPrefViking) :
                     (ethnicity == Ethnicity::BARBARIAN) ? getPrefix(aPrefScotish) :
                     (ethnicity == Ethnicity::NOMAD) ? getPrefix(aPrefArabic) :
                     getPrefix(aPrefHumans);

            // Resource words
            switch(resourceType) {
                case I_IRON:      resourceWord = rng::one_of(aHumanIron); break;
                case I_STONE:     resourceWord = rng::one_of(aHumanStone); break;
                case I_WOOD:      resourceWord = rng::one_of(aHumanWood); break;
                case I_GRAIN:
                case I_LIVESTOCK: resourceWord = rng::one_of(aHumanFood); break;
                case I_FUR:       resourceWord = rng::one_of(aHumanFur); break;
                case I_HERBS:     resourceWord = rng::one_of(aHumanHerbs); break;
                case I_HORSE:     resourceWord = rng::one_of(aHumanHorse); break;
                case I_CAMEL:     resourceWord = rng::one_of(aHumanCamel); break;
                case I_MITHRIL:   resourceWord = rng::one_of(aHumanMithril); break;
                case I_ROOTSTONE: resourceWord = rng::one_of(aHumanRootstone); break;
                case I_IRONWOOD:  resourceWord = rng::one_of(aHumanIronwood); break;
                case I_YEW:       resourceWord = rng::one_of(aHumanYew); break;
                case I_WHORSE:    resourceWord = rng::one_of(aHumanWhorse); break;
                case I_FLOATER:   resourceWord = rng::one_of(aHumanFloater); break;
                case I_MUSHROOM:  resourceWord = rng::one_of(aHumanMushroom); break;
                case I_ADMANTIUM: resourceWord = rng::one_of(aHumanAdamant); break;
                default:          resourceWord = "stone";
            }

            // Building type words
            switch(buildingType) {
                case O_MINE:          buildingWord = rng::one_of(aHumanMine); break;
                case O_QUARRY:        buildingWord = rng::one_of(aHumanQuarry); break;
                case O_TIMBERYARD:    buildingWord = rng::one_of(aHumanWorkshop); break;
                case O_FARM:
                case O_RANCH:         buildingWord = rng::one_of(aHumanFarm); break;
                case O_STABLE:        buildingWord = rng::one_of(aHumanStable); break;
                case O_OASIS:         buildingWord = rng::one_of(aHumanOasis); break;
                case O_TRAPPINGHUT:   buildingWord = rng::one_of(aHumanWorkshop); break;
                case O_TEMPLE:        buildingWord = rng::one_of(aHumanShrine); break;
                case O_AMINE:         buildingWord = rng::one_of(aHumanMine); break;
                case O_MQUARRY:       buildingWord = rng::one_of(aHumanQuarry); break;
                case O_MSTABLE:       buildingWord = rng::one_of(aHumanStable); break;
                case O_PRESERVE:      buildingWord = rng::one_of(aHumanPreserve); break;
                case O_SACGROVE:      buildingWord = rng::one_of(aHumanGrove); break;
                case O_FAERIERING:    buildingWord = rng::one_of(aHumanRing); break;
                case O_ALCHEMISTLAB:  buildingWord = rng::one_of(aHumanLab); break;
                case O_TRAPPINGLODGE: buildingWord = rng::one_of(aHumanLodge); break;
                default:              buildingWord = "works";
            }

            // Human format: prefix + " " + resource + " " + building
            // Example: "Brent Iron Mine" or "Oak Wood Mill"
            return (prefix + " " + resourceWord + " " + buildingWord) | filter::title_case;
    }
}

// Generate lair name based on monster type, lair type, and region's ethnicity
std::string getLairName(int lairType, int monsterType, int race) {
    Ethnicity ethnicity = raceToEthnicity(race);

    std::string monsterWord;
    std::string lairWord;
    std::string prefix;

    // Select monster and lair words based on ethnicity
    switch(ethnicity) {
        case Ethnicity::DWARF:
            prefix = getPrefix(aPrefDwarven);

            // Monster words
            switch(monsterType) {
                case I_DRAGON:
                case I_ICEDRAGON:   monsterWord = rng::one_of(aDwarfDragon); break;
                case I_TRENT:       monsterWord = rng::one_of(aDwarfTrent); break;
                case I_IMP:
                case I_DEMON:
                case I_BALROG:      monsterWord = rng::one_of(aDwarfDemon); break;
                case I_TROLL:
                case I_ETTIN:
                case I_OGRE:        monsterWord = rng::one_of(aDwarfTroll); break;
                case I_SKELETON:
                case I_UNDEAD:
                case I_LICH:        monsterWord = rng::one_of(aDwarfUndead); break;
                case I_CENTAUR:     monsterWord = rng::one_of(aDwarfCentaur); break;
                default:            monsterWord = rng::one_of(aDwarfBeast); break;
            }

            // Lair type words
            switch(lairType) {
                case O_CAVE:
                case O_ICECAVE:
                case O_OCAVE:       lairWord = rng::one_of(aDwarfCave); break;
                case O_LAIR:
                case O_ILAIR:       lairWord = rng::one_of(aDwarfLair); break;
                case O_RUIN:        lairWord = rng::one_of(aDwarfRuin); break;
                case O_DEMONPIT:    lairWord = rng::one_of(aDwarfPit); break;
                case O_CRYPT:       lairWord = rng::one_of(aDwarfCrypt); break;
                case O_SHAFT:       lairWord = rng::one_of(aDwarfShaft); break;
                case O_BOG:         lairWord = rng::one_of(aDwarfLair); break;
                default:            lairWord = "zad";
            }

            // Dwarven format: prefix + monster + lair
            // Example: "Khuz-draug-dum" = "Deep Dragon Cave"
            return (prefix + monsterWord + lairWord) | filter::capitalize;

        case Ethnicity::ELF:
        case Ethnicity::HIGHELF:
            prefix = getPrefix(aPrefElven2);

            // Monster words
            switch(monsterType) {
                case I_DRAGON:
                case I_ICEDRAGON:   monsterWord = rng::one_of(aElfDragon); break;
                case I_TRENT:       monsterWord = rng::one_of(aElfTrent); break;
                case I_IMP:
                case I_DEMON:
                case I_BALROG:      monsterWord = rng::one_of(aElfDemon); break;
                case I_TROLL:
                case I_ETTIN:
                case I_OGRE:        monsterWord = rng::one_of(aElfTroll); break;
                case I_SKELETON:
                case I_UNDEAD:
                case I_LICH:        monsterWord = rng::one_of(aElfUndead); break;
                case I_CENTAUR:     monsterWord = rng::one_of(aElfCentaur); break;
                default:            monsterWord = rng::one_of(aElfBeast); break;
            }

            // Lair type words
            switch(lairType) {
                case O_CAVE:
                case O_ICECAVE:
                case O_OCAVE:       lairWord = rng::one_of(aElfCave); break;
                case O_LAIR:
                case O_ILAIR:       lairWord = rng::one_of(aElfLair); break;
                case O_RUIN:        lairWord = rng::one_of(aElfRuin); break;
                case O_DEMONPIT:    lairWord = rng::one_of(aElfPit); break;
                case O_CRYPT:       lairWord = rng::one_of(aElfCrypt); break;
                case O_SHAFT:       lairWord = rng::one_of(aElfShaft); break;
                case O_BOG:         lairWord = rng::one_of(aElfLair); break;
                default:            lairWord = "ost";
            }

            // Elven format: prefix + monster + lair
            // Example: "Nim-lhug-rond" = "White Dragon Cavern"
            return (prefix + monsterWord + lairWord) | filter::capitalize;

        case Ethnicity::ORC:
            prefix = getPrefix(aPrefOrchish);

            // Monster words
            switch(monsterType) {
                case I_DRAGON:
                case I_ICEDRAGON:   monsterWord = rng::one_of(aOrcDragon); break;
                case I_TRENT:       monsterWord = rng::one_of(aOrcTrent); break;
                case I_IMP:
                case I_DEMON:
                case I_BALROG:      monsterWord = rng::one_of(aOrcDemon); break;
                case I_TROLL:
                case I_ETTIN:
                case I_OGRE:        monsterWord = rng::one_of(aOrcTroll); break;
                case I_SKELETON:
                case I_UNDEAD:
                case I_LICH:        monsterWord = rng::one_of(aOrcUndead); break;
                case I_CENTAUR:     monsterWord = rng::one_of(aOrcCentaur); break;
                default:            monsterWord = rng::one_of(aOrcBeast); break;
            }

            // Lair type words
            switch(lairType) {
                case O_CAVE:
                case O_ICECAVE:
                case O_OCAVE:       lairWord = rng::one_of(aOrcCave); break;
                case O_LAIR:
                case O_ILAIR:       lairWord = rng::one_of(aOrcLair); break;
                case O_RUIN:        lairWord = rng::one_of(aOrcRuin); break;
                case O_DEMONPIT:    lairWord = rng::one_of(aOrcPit); break;
                case O_CRYPT:       lairWord = rng::one_of(aOrcCrypt); break;
                case O_SHAFT:       lairWord = rng::one_of(aOrcShaft); break;
                case O_BOG:         lairWord = rng::one_of(aOrcLair); break;
                default:            lairWord = "gar";
            }

            // Orcish format: prefix + monster + lair
            // Example: "Ghash-lhug-hai" = "Fire Dragon Hole"
            return (prefix + monsterWord + lairWord) | filter::capitalize;

        case Ethnicity::MAN:
        case Ethnicity::VIKING:
        case Ethnicity::BARBARIAN:
        case Ethnicity::NOMAD:
        default:
            // Humans use more readable format
            prefix = (ethnicity == Ethnicity::VIKING) ? getPrefix(aPrefViking) :
                     (ethnicity == Ethnicity::BARBARIAN) ? getPrefix(aPrefScotish) :
                     (ethnicity == Ethnicity::NOMAD) ? getPrefix(aPrefArabic) :
                     getPrefix(aPrefHumans);

            // Monster words
            switch(monsterType) {
                case I_DRAGON:
                case I_ICEDRAGON:   monsterWord = rng::one_of(aHumanDragon); break;
                case I_TRENT:       monsterWord = rng::one_of(aHumanTrent); break;
                case I_IMP:
                case I_DEMON:
                case I_BALROG:      monsterWord = rng::one_of(aHumanDemon); break;
                case I_TROLL:
                case I_ETTIN:
                case I_OGRE:        monsterWord = rng::one_of(aHumanTroll); break;
                case I_SKELETON:
                case I_UNDEAD:
                case I_LICH:        monsterWord = rng::one_of(aHumanUndead); break;
                case I_CENTAUR:     monsterWord = rng::one_of(aHumanCentaur); break;
                default:            monsterWord = rng::one_of(aHumanBeast); break;
            }

            // Lair type words
            switch(lairType) {
                case O_CAVE:
                case O_ICECAVE:
                case O_OCAVE:       lairWord = rng::one_of(aHumanCave); break;
                case O_LAIR:
                case O_ILAIR:       lairWord = rng::one_of(aHumanLair); break;
                case O_RUIN:        lairWord = rng::one_of(aHumanRuin); break;
                case O_DEMONPIT:    lairWord = rng::one_of(aHumanPit); break;
                case O_CRYPT:       lairWord = rng::one_of(aHumanCrypt); break;
                case O_SHAFT:       lairWord = rng::one_of(aHumanShaft); break;
                case O_BOG:         lairWord = rng::one_of(aHumanLair); break;
                default:            lairWord = "dungeon";
            }

            // Human format: prefix + " " + monster + " " + lair
            // Example: "Blackwood Dragon Cave" or "Grimstone Troll Den"
            return (prefix + " " + monsterWord + " " + lairWord) | filter::title_case;
    }
}

std::string getObjectName(const int typeIndex, const ObjectType& type) {
    switch(typeIndex)
    {
        case O_DUMMY:
            break;

        case O_LONGBOAT:
        case O_CLIPPER:
        case O_GALLEON:
        case O_BALLOON:
        case O_AGALLEON:
        case O_DERELICT:
            return getShipName();

        case O_TOWER:
        case O_FORT:
        case O_CASTLE:
        case O_CITADEL:
        case O_MCASTLE:
        case O_MCITADEL:
        case O_MTOWER:
        case O_PALACE:
        case O_STOCKADE:
        case O_CPALACE:
        case O_HTOWER:
        case O_MAGETOWER:
        case O_DARKTOWER:
        case O_GIANTCASTLE:
        case O_HPTOWER:
            return getFortressName(type);

        case O_SHAFT:
        case O_LAIR:
        case O_RUIN:
        case O_CAVE:
        case O_DEMONPIT:
        case O_CRYPT:
        case O_MINE:
        case O_FARM:
        case O_RANCH:
        case O_TIMBERYARD:
        case O_QUARRY:
        case O_MQUARRY:
        case O_AMINE:
        case O_PRESERVE:
        case O_SACGROVE:
        case O_TRAPPINGHUT:
        case O_STABLE:
        case O_MSTABLE:
        case O_TRAPPINGLODGE:
        case O_FAERIERING:
        case O_ALCHEMISTLAB:
        case O_OASIS:
        case O_GEMAPPRAISER:
                break;

        case O_INN:
            return getInnName();

        case O_ISLE:
        case O_OCAVE:
        case O_WHIRL:
            break;

        case O_ROADN:
        case O_ROADNW:
        case O_ROADNE:
        case O_ROADSW:
        case O_ROADSE:
        case O_ROADS:
            // Fallback: no builder race known — use human format
            return getRoadName(typeIndex, 0);

        case O_TEMPLE:
        case O_BKEEP:
        case O_DCLIFFS:
        case O_HUT:
        case O_NGUILD:
        case O_AGUILD:
        case O_ATEMPLE:
        case O_ILAIR:
        case O_ICECAVE:
        case O_BOG:
            break;
    }

    return type.name;
}

std::string getForestName(std::string s, int area) {

    if (area == 1) {
        return s + rng::one_of({
            " Grove", " Copse", " Thicket", " Glade", " Wood",
            " Stand"," Taur", " Galadh", " Nim",
            " Loth", " Nan", " Dor", " Tin", " Lin",
        });
    }

    if (area < 15) {
        return s + rng::one_of({
            " Forest", " Woods", " Woodland", " Timberland", " Thicket",
            " Wildwood", " Greenwood", " Deepwood", " Darkwood", " Heartwood",
            " Oldwood", " Newwood", " Highwood", " Lowwood", " Riverwood",
            " Taur", " Galadhrim", " Nimloth", " Lothlorien", " Fangorn",
            " Mirkwood", " Greenwood", " Sunwood", " Moonwood", " Starwood",
            " Silverwood", " Goldwood", " Ironwood", " Glasswood", " Crystalwood",
            " Elderwood", " Ancient Forest", " Sacred Grove", " Blessed Woods", " Cursed Forest",
            " Whispering Woods", " Singing Forest", " Dreaming Woodland", " Twilight Thicket", " Moonlit Grove",
            " Shadowwood", " Ghostwood", " Spirit Forest"
        });
    }

    return rng::one_of({
        "Great " + s + " Forest",
        "Vast " + s + " Woods",
        "Endless " + s + " Woodland",
        "Immense " + s + " Timberland",
    });
}

std::string getJungleName(std::string s, int area) {

    if (area == 1) {
        return s + rng::one_of({
            " Grove", " Copse", " Thicket", " Glade",
            " Canopy", " Underbrush", " Vines", " Ferns", " Moss",
            " Shade", " Whisper", " Breath", " Heart",
        });
    }

    if (area < 15) {
        return s + rng::one_of({
            // Общие
            " Jungle", " Rainforest", " Wildwood",
            " Thicket", " Canopy", " Undergrowth", " Vines", " Ferns",
            " Temple Jungle", " Altar Woods", " Shrine Jungle", " Sacred Vines", " Ritual Grove",
        });
    }

    return rng::one_of({
        "Great " + s + " Jungle",
        "Vast " + s + " Rainforest",
        "Endless " + s + " Wildwood",
        "Immense " + s + " Canopy",
        "The " + s + " of a Thousand Vines",
    });
}

std::string getDesertName(std::string s, int area) {
    if (area < 15) {
        return s + rng::one_of({
            " Ash Flats", " Stonewaste", " Cinder Sands", " Broken Anvil", " Dust Of Khaz",
            " Sunstep Sands", " Windrunner Flats", " Golden Steppe", " Hoofwind Plains", " Dustmane Reach",
            " Liraeth Sands", " Sunveil", " Ashenbloom", " Silvaran Dunes", " Ithil Dust",
            " Dry Meadows", " Sandy Downs", " Sunbaked Fields"," Old Sandpatch"
        });
    }

    return rng::one_of({
        "Great " + s + " Flats",
        "Ashen " + s + " Dunes",
        "Sunlit " + s + " Sands",
        "Golden " + s + " Reach",
        "Windy " + s + " Steppe",
        "Burning " + s + " Wastes",
        "Stormy " + s + " Flats",
        "Fiery " + s + " Dunes"
    });
}

std::string getVolcanoName(std::string s) {
    return s + rng::one_of({
               " Mount Pyraxis",
               " Ashen Crown",
               " Cinderfell",
               " Fireheart Peak",
               " Molten Throne",
               " Emberspire",
               " Scorchreach",
               " Infernum Rise",
               " Obsidian Peak",
               " Caldera of Flames",
               " Dragon's Maw",
               " Burning Spire",
               " Crown of Cinders",
               " Hellfire Peak",
               " Magma Sanctum",
               " Khazdûr Peak",
               " Bronzefire Mountain",
               " Forgeheart",
               " Anvilspire",
               " Stoneflame Hold",
               " Ironcinder",
               " Deepforge Volcano",
               " Emberhall",
               " Molten Anvil",
               " Ashen Forge",
               " Blackhammer Peak",
               " Firebeard's Crown"
           });
}

std::string getMountainName(std::string s, int area) {
    if (area == 1) {
        return s + rng::one_of({
            " Mountain", " Peak", " Summit", " Pinnacle", " Crest",
            " Tor", " Crag", " Bluff", " Butte", " Mesa",
            " Spire", " Horn", " Tooth", " Fang", " Crown",
            " Anvil", " Forge", " Pick", " Axepeak",
            " Stonepeak", " Orepeak", " Gemspire", " Veinspire", " Deepstone",
            " Warrenpeak", " Burrowspire", " Excavation",
        });
    }

    if (area < 15) {
        return s + rng::one_of({
            // Общие
            " Mountains", " Heights", " Rocks", " Peaks", " Summits",
            " Pinnacles", " Crags", " Bluffs", " Spires", " Horns",
            " Range", " Massif", " Ridge", " Escarpment", " Wall",
            " Spine", " Teeth", " Fangs", " Crowns", " Bastion",
            " Warrens", " Burrows", " Excavations", " Pits", " Quarries",
            " Gemfields", " Motherlodes", " Veins", " Seams", " Lodes",
            " Camps", " Forts", " Holds", " Lairs", " Dens",
            " Bloodpeaks", " Warcamps", " Skullheights", " Gorecrags", " Boneteeth",
            " Sentinel Peaks", " Watcher Spires", " Guard Mountains", " Warrior Crags"
        });
    }

    return rng::one_of({
        "Great " + s + " Mountains",
        "Vast " + s + " Range",
        "Endless " + s + " Peaks",
        "Immense " + s + " Massif",
        "The " + s + " of a Thousand Summits",
        s + ", Roof of the World",
        "The Eternal " + s + " Spines",
        "The " + s + " That Pierce Heaven",
        s + ", Forge of Creation",
        "The " + s + " Adamantine Peaks",
        "Ancestral " + s,
        "Throne of " + s,
        "Great Warren " + s,
        s + " of Endless Delves",
        "The " + s + " Gemheart",
        "Motherlode " + s + " Range",
        "The " + s + " Prospector's Paradise",
        "Burrow-Empire of " + s
    });
}

std::string getHillsName(std::string s, int area) {

    if (area == 1) {
        return s + rng::one_of({
            // Общие
            " Hill", " Barrow", " Knoll", " Mound", " Rise",
            " Slope", " Ridge", " Spur", " Tor", " Cairn",
            " Anvil", " Forge", " Ore", " Gem", " Vein",
            " Burrow", " Warren", " Delve", " Excavation"
        });
    }

    if (area < 15) {
        return s + rng::one_of({
            " Hills", " Barrows", " Heights", " Downs", " Fells",
            " Highlands", " Uplands", " Moorlands", " Braes", " Crags",
            " Ridges", " Slopes", " Escarpment", " Bluffs", " Cliffs",
            " Halls"," Forges", " Anvils",
            " Delve", " Hold", " Karak", " Dwarrow",
            " Warrens"," Burrows", " Diggings",
            " Delvings"," Redoubts"," Gorehills", " Bonecrags"
        });
    }

    return rng::one_of({
        "Great " + s + " Hills",
        "Vast " + s + " Highlands",
        "Endless " + s + " Uplands",
        "The Eternal " + s + " Heights",
        "The " + s + " Iron Hills",
        "Deep " + s + " Delve",
        "Great Warren " + s,
        "The " + s + " Gemfields",
        "Motherlode " + s,
        s + ", Vein of the World",
        "Burrow-Kingdom of " + s
    });
}

std::string getSwampName(std::string s, int area) {

    if(area == 1) {
        return s + rng::one_of({
            " Bog", " Fen", " Mire", " Quagmire",
            " Morass", " Slough", " Pool", " Puddle",
            " Slime", " Muck", " Grime", " Gloop",
            " Ooze", " Sludge", " Drip", " Slog", " Squelch",
            " Hissk", " Slith", " Scale", " Fang",
            " Clutch", " Brood",
            " Dirt", " Mud", " Filth", " Rot", " Stench",
            " Gore", " Bone", " Wart"
        });
    }


    if (area < 15) {
        return s + rng::one_of({
            " Bog", " Fen", " Mire",
            " Quagmire", " Morass", " Wetland", " Slough", " Bayou",
            " Everglade", " Carr", " Vlei", " Muskeg", " Moor",
            " Gloommire", " Dreadmarsh", " Foulfen", " Witchfen", " Elder Bog",
            " Blackwater Bog", " Sinking Mire", " Whispering Bog", " Deathfen",
            " Slimepit", " Muckhole", " Oozepool", " Glooptrench",
            " Sludgebog", " Shamanfen", " Skullbog", " Warpmire",
            " Nesting", " Hatchery", " Basking", " Ssanss", " Hatching",
            " Clutching", " Scalelands", " Fangmarsh", " Tailfen",
            " Bloodmarsh", " Bonefen", " Gorebog", " Rotmire", " Wartland"
        });
    }

    return rng::one_of({
        "Great " + s + " Swamp",
        "Vast " + s + " Marshes",
        "Endless " + s + " Bog",
        "Immense " + s + " Fen",
        "The " + s + " of Lost Souls",
        "The " + s + " That Never Dries",
        "Great " + s + " Goblinmire",
        "King " + s + "'s Slime Empire",
        "The " + s + " of a Thousand Stinks",
        s + ", Shaman's Dominion",
        "Warlord " + s + "'s Muck Kingdom",
        "Great " + s + " Ssaruth",
        s + ", Egg of the World",
        "The " + s + " of the Old Blood",
        "Ssun God's " + s,
        s + ", Throne of the Scale King",
        "Waaagh! " + s,
    });
}

std::string getPlainName(std::string s, int area) {
    if(area == 1) {
        return s + rng::one_of({
            " Dale", " Meadow", " Field", " Lea", " Glade",
            " Green", " Meads", " Garden", " Plot", " Croft",
            " Patch", " Dell", " Hollow", " Nook", " Copse"
        });
    }

    if (area < 15) {
            return s + rng::one_of({
                " Marsh", " Downs", " Slope", " Meadowlands", " Pastures",
                " Farmlands", " Hayfields", " Cornlands", " Wheatlands", " Talath", " Laer", " Loth", " Nan", " Dor",
                " Ard", " Ethir", " Imrath", " Pel", " Reg",
                " Aelin", " Lin", " Tir", " Calen", " Laire", " Plains", " Valley", " Fields", " Lowlands", " Flatlands",
                " Grasslands", " Prairie", " Steppe", " Expanse", " Plateau"
            });
    }

    return rng::one_of({
        "Great " + s + " Plains",
        "Vast " + s + " Expanse",
        "Immense " + s + " Grasslands",
        s + ", Sea of Grass",
        "Horizonless " + s
    });
}

std::string getTundraName(std::string s, int area) {

    if (area == 1) {
        return s + rng::one_of({
            " Frost", " Rime", " Hoar", " Gelid", " Cryo", " Glace", " Nive", " Pruina"
        });
    }

    if (area < 5) {
        return s + rng::one_of({
            " Fell", " Wold", " Heath", " Moor", " Downs", " Fells", " Brae", " Tor"
        });
    }

    if (area < 10) {
        return s + rng::one_of({
            " Waste", " Barrens", " Expanse", " Steppe", " Veldt", " Pampas", " Llano", " Tundra"
        });
    }

    if (area < 15) {
        return s + rng::one_of({
            " Snowfield",
            " Ice Flats",
            " Frost Vale",
            " Cold Barrens",
            " Glacial Hollow",
            " Frozen Vale",
            " Chill Expanse"
        });
    }

    // Огромные территории (15+)
    return rng::one_of({
        "Frostmaiden's " + s,
        "Ice Dragon's " + s + " Dominion",
        "Yeti King's Frozen " + s,
        "White Wyrm's " + s,
        "Frost Giant Jarl's " + s,
        "The " + s + " Where Stars Freeze",
        s + " of the Sleeping Titans",
        "The " + s + " That Time Forgot",
        s + ", Graveyard of Suns",
        "The " + s + " Beyond the North Wind",
        s + " the Unmelting",
        "Everice " + s,
        "Endwinter " + s,
        s + " of Perpetual Gloom",
        "The Glacier-throne " + s
    });
}

std::string getOceanName(std::string s, int area) {
    if(area == 1) {
        return s + rng::one_of({" Lagoon", " Pond", " Tarn", " Lough"});
    }

    if (area < 15) {
        return s + " Sea";
    }

    return s + " Ocean";
}

std::string getRegionName(const Ethnicity etnos, const int type, const int size, const bool island) {
    std::string name = getEthnicName(etnos);

    if (island) {
        return name + " Island";
    }

    switch(type)
    {
        case R_FOREST:
        case R_UFOREST:
        case R_CERAN_FOREST1:
        case R_CERAN_FOREST2:
        case R_CERAN_FOREST3:
        case R_CERAN_MYSTFOREST:
        case R_CERAN_MYSTFOREST1:
        case R_CERAN_MYSTFOREST2:
        case R_CERAN_UFOREST1:
        case R_CERAN_UFOREST2:
        case R_CERAN_UFOREST3:
        case R_DFOREST:
        case R_CERAN_DFOREST1:
            return getForestName(name, size);

        case R_JUNGLE:
        case R_CERAN_JUNGLE1:
        case R_CERAN_JUNGLE2:
        case R_CERAN_JUNGLE3:
            return getJungleName(name, size);

        case R_DESERT:
        case R_CERAN_DESERT1:
        case R_CERAN_DESERT2:
        case R_CERAN_DESERT3:
            return getDesertName(name, size);

        case R_VOLCANO:
            return getVolcanoName(name);

        case R_MOUNTAIN:
        case R_ISLAND_MOUNTAIN:
        case R_CERAN_MOUNTAIN1:
        case R_CERAN_MOUNTAIN2:
        case R_CERAN_MOUNTAIN3:
            return getMountainName(name, size);

        case R_HILL:
        case R_CERAN_HILL:
        case R_CERAN_HILL1:
        case R_CERAN_HILL2:
            return getHillsName(name, size);

        case R_SWAMP:
        case R_ISLAND_SWAMP:
        case R_CERAN_SWAMP1:
        case R_CERAN_SWAMP2:
        case R_CERAN_SWAMP3:
            return getSwampName(name, size);

        case R_PLAIN:
        case R_ISLAND_PLAIN:
        case R_CERAN_PLAIN1:
        case R_CERAN_PLAIN2:
        case R_CERAN_PLAIN3:
            return getPlainName(name, size);

        case R_TUNDRA:
        case R_CERAN_TUNDRA1:
        case R_CERAN_TUNDRA2:
        case R_CERAN_TUNDRA3:
            return getTundraName(name, size);

        case R_OCEAN:
            return getOceanName(name, size);

        default:
            return getAbstractName();
    }
}

std::string getRiverName(const int size, const int min, const int max) {
    std::string s = getAbstractName();

    int d = max - min;
	int greatRiver = max - d / 3;
    if (size >= greatRiver) {
        s = "Great " + s;
    }

    s += " River";

    return s;
}
