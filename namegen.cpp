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
// Personal name tables (for AutoNameSoloUnits — see docs/UNIT_NAMING_SYSTEM.md)
// Sources: D&D 5e PHB, Tolkien LotR/Silmarillion/Appendix F, Norse Prose Edda,
//          R.A. Salvatore Dark Elf Trilogy, FR Menzoberranzan (TSR 1992),
//          Warcraft III, Pathfinder Inner Sea Races, D&D Theros/Ravnica
//---------------------------------------------------------------------------

// Human (I_MAN): compound English surnames (adj+noun). First name from aPrefMale+aSufMale.
// Source: Tolkien Appendix F Anglo-Saxon structure; D&D 5e PHB Human Names
// Space: aPrefMale(102)×aSufMale(100) × humanSurAdj(25)×humanSurNoun(25) = 6,375,000
std::vector<std::string> aHumanSurAdj = {
    "Black", "Bright", "Brown", "Dark", "Deep", "Fair", "Fast", "Fire",
    "Gold", "Grey", "Hard", "High", "Iron", "Long", "Mud", "Old", "Red",
    "Sharp", "Silver", "Stone", "Strong", "True", "White", "Wild", "Wood"
};
std::vector<std::string> aHumanSurNoun = {
    "axe", "blade", "bow", "brook", "dale", "fell", "ford",
    "forge", "grove", "hammer", "hand", "hill", "lock", "moor",
    "oak", "peak", "ridge", "shield", "smith", "stone",
    "sword", "thorn", "tower", "wood", "yard"
};

// Orc (I_ORC): hard-consonant epithet (70% chance). First name from aPrefOrchish+aSufOrchish.
// Source: Warcraft III (Thrall, Grom, Durotan); D&D 5e PHB Orc Names; Tolkien Orcish
// Space: aPrefOrchish(40)×aSufOrchish(12)=480 × (0.3 + 15×15×0.7) = ~75,744
std::vector<std::string> aOrcEpiAdj = {
    "Bone", "Blood", "Iron", "Black", "Stone", "War", "Skull", "Death",
    "Dark", "Grim", "Hate", "Red", "Fang", "Scar", "Steel"
};
std::vector<std::string> aOrcEpiNoun = {
    "Crusher", "Breaker", "Smasher", "Splitter", "Carver", "Render",
    "Ripper", "Basher", "Gnasher", "Gouger", "Stomper", "Hewer",
    "Brawler", "Cleaver", "Warchief"
};

// Hill Dwarf (I_HILLDWARF): pref-suf always has clan. aPrefDwarven+aSufDwarven existing.
// Source: Tolkien The Hobbit (Thorin Oakenshield, Dwalin, Balin); D&D 5e PHB Dwarf Names
// Space: aPrefDwarven(34)×aSufDwarven(43)=1,462 × clanAdj(20)×clanNoun(20)=400 → 584,800
std::vector<std::string> aClanAdj = {
    "Iron", "Stone", "Deep", "Bronze", "Golden", "Silver", "Dark",
    "Black", "Old", "High", "Fire", "Bold", "Cold", "Hard", "Strong",
    "Bright", "Ash", "Storm", "Copper", "Red"
};
std::vector<std::string> aClanNoun = {
    "Hold", "Forge", "Peak", "Hammer", "Axe", "Shield", "Anvil",
    "Deep", "Vault", "Gate", "Hall", "Mine", "Crag", "Throne", "Ridge",
    "Keep", "Wall", "Hearth", "Keg", "Helm"
};

// High Elf (I_HIGHELF): melodic pref+suf always has house. aPrefElven2+aSufElven2 existing.
// Source: Tolkien The Silmarillion (Quenya phonology); D&D 5e PHB High Elf
// Space: aPrefElven2(39)×aSufElven2(52)=2,028 × elfClanAdj(25)×elfClanNoun(25)=625 → 1,267,500
std::vector<std::string> aElfClanAdj = {
    "Silver", "Golden", "Starlit", "Moon", "Sun", "Ancient", "Radiant",
    "Timeless", "Crystal", "Emerald", "Sapphire", "Azure", "Ivory",
    "Serene", "Twilight", "Dawn", "Eternal", "Gossamer", "Pristine", "Opaline",
    "Shadowed", "Misted", "Gleaming", "Hallowed", "Undying"
};
std::vector<std::string> aElfClanNoun = {
    "Dawn", "Moon", "Star", "Tree", "Path", "Song", "Leaf", "Brook",
    "Glade", "Vale", "Wind", "Rain", "Bloom", "Flame", "Arrow",
    "Tower", "Gate", "Crown", "Throne", "Ring",
    "Shore", "Tide", "Mist", "Dream", "Light"
};

// Wood Elf (I_WOODELF): nature epithet always present. aPrefElven1+aSufElven1 existing.
// Source: Tolkien LotR Mirkwood (Legolas, Thranduil) Sindarin; D&D 5e PHB Wood Elf
// Space: aPrefElven1(30)×aSufElven1(29)=870 × epithetAdj(25)×epithetNoun(25)=625 → 543,750
std::vector<std::string> aEpithetAdj = {
    "Swift", "Silent", "Wild", "Ancient", "Bright", "Dark", "Gentle",
    "Green", "Hidden", "Keen", "Lone", "Quick", "Tall", "Wandering",
    "Wise", "Fleet", "Deft", "Wary", "Lithe", "Deep",
    "Far", "High", "Lost", "Mist", "Still"
};
std::vector<std::string> aEpithetNoun = {
    "Arrow", "Bow", "Branch", "Deer", "Fern", "Flame", "Fox", "Hart",
    "Hawk", "Leaf", "Moon", "Oak", "Path", "Rain", "River",
    "Root", "Shadow", "Star", "Stone", "Stream",
    "Thorn", "Tree", "Wind", "Wood", "Wolf"
};

// Hobbit (I_HOBBIT): Shire first name + compound plant/place surname.
// Source: Tolkien LotR + The Hobbit Appendix C Shire family trees
// Space: hobbitsFirst(50) × hobbitsAdj(25) × hobbitsNoun(20) = 25,000
std::vector<std::string> aHobbitsFirst = {
    "Bilbo", "Frodo", "Samwise", "Peregrin", "Meriadoc", "Lotho",
    "Hamfast", "Lobelia", "Drogo", "Primula", "Bungo", "Belladonna",
    "Rufus", "Pansy", "Berilo", "Camellia", "Falco", "Hanna", "Isumbras",
    "Mirabella", "Nob", "Olo", "Polo", "Rosa", "Tobold", "Uffo", "Viola",
    "Wilcome", "Angelica", "Berylla", "Celandine", "Daisy", "Estella",
    "Fatty", "Griffo", "Hob", "Odo", "Pimpernel", "Rosamunde", "Seredic",
    "Bingo", "Milo", "Largo", "Balbo", "Posco", "Sigismond", "Teobald",
    "Merry", "Pippin", "Rosie"
};
std::vector<std::string> aHobbitsAdj = {
    "Briar", "Bracken", "Elder", "Golden", "Green", "Hollow",
    "Meadow", "Moss", "Oak", "Sandy", "Thistle", "Thorn",
    "Tumble", "Under", "White", "Wood", "Yarrow", "Buck",
    "Copper", "Curly", "Dusty", "Fern", "Amber", "Clover", "Heather"
};
std::vector<std::string> aHobbitsNoun = {
    "bank", "borough", "bottom", "brook", "bush", "fields",
    "ford", "gap", "grove", "hill", "hollow", "lock",
    "meadow", "moor", "nook", "side", "toe", "wick", "wood", "yard"
};

// Leader (I_LEADERS): Latin formal title + male name + 60% "of the [order]".
// Source: Latin/Roman naming tradition; D&D 5e Human Noble variant; Classical sources
// Space: leaderTitle(13) × aPrefMale(102)×aSufMale(100) × (0.4 + 25×0.6) = ~20.9M
std::vector<std::string> aLeaderTitle = {
    "Magister", "Dominus", "Prefect", "Legate", "Consul",
    "Praetor", "Proconsul", "Tribune", "Centurion", "Pontifex",
    "Rector", "Arbiter", "Curator"
};
std::vector<std::string> aLeaderOrder = {
    "Silver Hand", "Iron Tower", "Golden Dawn", "White Flame",
    "Crimson Blade", "Blue Shield", "Black Star", "Green Veil",
    "Stone Circle", "Eternal Flame", "Sacred Seal", "Ivory Spire",
    "Amber Crown", "Crystal Archive", "Jade Throne", "Opal Ring",
    "Obsidian Court", "Sapphire Order", "Ruby Chalice", "Emerald Path",
    "Gilded Scepter", "Azure Banner", "Onyx Compact", "Pearl Covenant", "Sunlit Accord"
};

// Drow (I_DROWMAN): pref+suf always appended with FR canon house name.
// Source: R.A. Salvatore Dark Elf Trilogy; FR Menzoberranzan (TSR 1992)
// Space: aPrefDrow(30)×aSufDrow(30)=900 × drowHouses(25) = 22,500
std::vector<std::string> aDrowHouses = {
    "Baenre", "Barrison Del'Armgo", "Fey-Branche", "Oblodra",
    "Horlbar", "Xorlarrin", "Hunzrin", "Mizzrym", "Faen Tlabbar",
    "Vandree", "Tuin'Tarl", "Agrach Dyrr", "Kenafin", "Druu'giir",
    "Zauvirr", "Duskryn", "Srune'lett", "Hlaund", "Symryvvin",
    "Everhate", "Tlar'vel", "Rhynnoth", "Ulvithis", "Vrinn", "Kilsek"
};

// Gnome (I_GNOME): compound nickname + clan name.
// Source: D&D 5e PHB Gnome Names (true name private, nickname + clan)
// Space: gnomePart1(40) × gnomePart2(40) × gnomeClan(40) = 64,000
std::vector<std::string> aGnomePart1 = {
    "Alst", "Bib", "Blip", "Boff", "Bobbery", "Clink", "Dap", "Fink",
    "Flick", "Fob", "Gadge", "Gibb", "Glib", "Kix", "Nim",
    "Nip", "Nod", "Nog", "Orb", "Pip", "Plink", "Pock", "Quib",
    "Reck", "Riff", "Rizz", "Scribb", "Seebo", "Sniff", "Spiff",
    "Squib", "Tick", "Tink", "Tip", "Titch", "Tiz", "Twib", "Wobb",
    "Whizz", "Zip"
};
std::vector<std::string> aGnomePart2 = {
    "ace", "ald", "berry", "blast", "bounce", "brow", "clank",
    "cog", "crank", "dazzle", "ears", "fast", "finger", "fizz",
    "flash", "foot", "fumble", "gadget", "gear", "gem",
    "giggles", "glint", "hat", "hop", "jangle", "jig",
    "jump", "kit", "loop", "mop", "noodle", "pod",
    "scratch", "skip", "snatch", "spark", "spring", "squeak", "twist", "whirl"
};
std::vector<std::string> aGnomeClan = {
    "Timbers", "Nackle", "Daergel", "Folkor", "Garrick", "Scheppen",
    "Turen", "Murnig", "Ningel", "Waywocket", "Sparkwidget", "Thistletop",
    "Copperkettle", "Fizzwhistle", "Wobblewick", "Rumblebottom", "Tumblegear",
    "Glimmerwick", "Ticktock", "Whistlegap", "Thornberry", "Quickfingers",
    "Brightgem", "Clatterbox", "Jinglebell", "Tinkertop", "Whirligig",
    "Snapperjack", "Mirthbell", "Pebbletoss", "Swiftpocket", "Crankshaft",
    "Bubblecork", "Bumblebrock", "Silverstring", "Pallabar", "Nipsqueak",
    "Sparkplug", "Tumblecork", "Zook"
};

// Ice Dwarf (I_ICEDWARF): Norse-style pref+suf always has arctic hold name.
// Source: Norse Prose Edda (Snorri Sturluson) dwarf names; Pathfinder frost dwarves
// Space: iceDwarfPref(24) × iceDwarfSuf(20) × holdAdj(20) × holdNoun(20) = 192,000
std::vector<std::string> aIceDwarfPref = {
    "Nyi", "Nithi", "Nordri", "Sudri", "Austri", "Vestri", "Althjof",
    "Dvalin", "Nar", "Nain", "Niping", "Dain", "Bifur", "Bafur",
    "Bombor", "Nori", "Ori", "Onar", "Oin", "Modvit",
    "Aud", "Imir", "Dur", "Frosti"
};
std::vector<std::string> aIceDwarfSuf = {
    "inn", "ald", "ur", "ar", "rim", "nar", "den",
    "tir", "var", "gir", "kin", "lid", "mir", "nyr",
    "orm", "rod", "sel", "vin", "dag", "rald"
};
std::vector<std::string> aHoldAdj = {
    "Frost", "Ice", "Cold", "Frozen", "Snow", "Winter", "Glacier",
    "Blizzard", "Arctic", "Polar", "Crystal", "Bitter", "Black",
    "Iron", "Stone", "Dark", "Deep", "Silent", "Sharp", "Storm"
};
std::vector<std::string> aHoldNoun = {
    "Hold", "Keep", "Vault", "Peak", "Ridge", "Crag", "Spire",
    "Bastion", "Forge", "Deep", "Hall", "Gate", "Helm", "Throne",
    "Fortress", "Rampart", "Citadel", "Pinnacle", "Tor", "Cairn"
};

// Under Dwarf (I_UNDERDWARF): deep/dark pref+suf always has underground clan.
// Source: D&D Underdark sourcebook; Tolkien Moria dwarves; distinct from Hill/Ice Dwarf
// Space: underPref(20) × underSuf(20) × darkClanAdj(15) × darkClanNoun(15) = 90,000
std::vector<std::string> aUnderDwarfPref = {
    "Azag", "Barag", "Darg", "Durakh", "Garak", "Grag", "Grul",
    "Kazag", "Kharg", "Krag", "Muzag", "Narak", "Rag", "Skar",
    "Ugrak", "Umbrak", "Urduk", "Uzgar", "Varg", "Zorak"
};
std::vector<std::string> aUnderDwarfSuf = {
    "akh", "arak", "bur", "dak", "dur", "gak", "goth",
    "gruk", "kak", "kur", "mak", "nak", "nok", "rag",
    "rok", "ruk", "sak", "thak", "tur", "uk"
};
std::vector<std::string> aDarkClanAdj = {
    "Shadow", "Dark", "Black", "Deep", "Stone",
    "Iron", "Ancient", "Silent", "Grim", "Dread",
    "Sunken", "Forsaken", "Blind", "Bitter", "Coal"
};
std::vector<std::string> aDarkClanNoun = {
    "Vault", "Deep", "Crypt", "Forge", "Mine",
    "Pit", "Shaft", "Delve", "Burrow", "Cavern",
    "Hall", "Keep", "Throne", "Gate", "Hearth"
};

// Goblinman (I_GOBLINMAN): short punchy name + (60%: "the tag", 40%: compound surname).
// Style: mischievous trickster — thief, dreamer, show-off, chaos agent. Not bloodthirsty.
// Source: D&D 5e MM; Pathfinder Inner Sea Races; Warcraft goblins
// Space: goblinPref(30)×goblinSuf(35)=1050 × (0.6×55 + 0.4×30×45) = ~600,600
std::vector<std::string> aGoblinPref = {
    "Brix", "Bug", "Clag", "Crud", "Dob", "Dreg", "Driz", "Dug",
    "Fang", "Fizz", "Gag", "Gib", "Glix", "Glub", "Gnash", "Gob",
    "Gog", "Grax", "Grix", "Grub", "Gug", "Gunk", "Krix", "Lug",
    "Mog", "Mug", "Nab", "Nik", "Pug", "Rix"
};
std::vector<std::string> aGoblinSuf = {
    // original kept (removed: gash, gut, pus, retch, rot)
    "bit", "bix", "brak", "crash", "dirt", "dreg", "fang",
    "gib", "gnash", "grab", "grub", "guck", "jabber",
    "krak", "lurk", "mire", "muck", "nab", "nix", "poke",
    // new: quick/trickster sounds
    "flick", "nick", "slick", "pox", "twitch", "squeak", "drib", "blip",
    "grip", "flip", "quick", "itch", "reek", "snit", "burp"
};
std::vector<std::string> aGoblinEpiAdj = {
    // body parts (kept) — work great as funny compound: Earsniffer, Nosegazer
    "Bone", "Skull", "Ear", "Nose", "Brain", "Fist",
    // trickster/thief
    "Quick", "Long", "Short", "Sharp", "Gold", "Loud", "Sly", "Crooked",
    "Sticky", "Empty", "Shifty", "Nimble", "Old", "Bold", "Tiny", "Bright",
    "Silver", "Flat", "Wide", "Rusty", "Loose", "Big", "Vain", "Dream"
};
std::vector<std::string> aGoblinEpiNoun = {
    // kept combat-ish (removed: muncher, licker, sucker, gouger)
    "kicker", "crusher", "stomper", "sniffer", "biter", "gnawer",
    "cruncher", "smasher", "basher", "ripper", "snapper", "chewer",
    "clawer", "dragger", "hacker", "slasher", "chomper",
    "poker", "puller", "picker", "grabber",
    // trickster/thief
    "snatcher", "hoarder", "stealer", "finger", "purse", "tooth",
    "pocket", "tongue", "deal", "coin", "runner", "dodger", "trader", "liar",
    // personality quirks — combine with adj: "Boldboaster", "Dreamgazer", "Loudcackler"
    "dreamer", "smiler", "boaster", "gazer", "peeker", "schemer",
    "talker", "prancer", "napper", "worrier", "thinker", "gawker",
    "squawker", "faker", "counter", "giggler", "mumbler", "whistler",
    "wobbler", "bouncer", "spinner", "cackler", "winker",
    // compound quirky nouns — "Longflowersniffer", "Brightshinyhunter"
    "stargazer", "flowersniffer", "shinyhunter", "shadowchaser",
    "mirrorgazer", "coinhoarder", "sockstealer", "cheesehunter"
};
std::vector<std::string> aGoblinTag = {
    // original kept
    "Sneaky", "Stabby", "Grabby", "Jumpy", "Greedy", "Scruffy", "Cowardly",
    // trickster/thief
    "Sly", "Crooked", "Bold", "Unlucky", "Confused", "Obvious", "Invisible",
    "Clumsy", "Crafty", "Lucky", "Persistent", "Broke", "Shifty", "Reckless",
    "Noisy", "Loud",
    // personality
    "Dreamy", "Smiley", "Boastful", "Vain", "Nosy", "Peeky", "Chatty",
    "Forgetful", "Wandering", "Fancy", "Dramatic", "Suspicious", "Romantic",
    "Philosophical", "Artistic", "Giggly", "Gloomy", "Fidgety", "Mumbling",
    "Wobbly", "Scheming",
    // strange habits
    "Sneezy", "Sleepy", "Hiccupy", "Bouncy", "Twirly", "Blinky", "Stompy",
    "Whistling", "Starry", "Flowery", "Shiny", "Daydreaming", "Overconfident"
};

// Lizardman (I_LIZARDMAN): sibilant hissing sounds + 50% tribal suffix.
// Source: D&D 5e MM (lizardfolk); D&D 3.5e Savage Species; Pathfinder lizardfolk
// NOT using Aztec arrays — those generate place names, not personal names
// Space: lizardPref(40) × lizardSuf(40) × (0.5 + 0.5×25) = 32,000 → P(n=15) ≈ 0.35%
std::vector<std::string> aLizardPref = {
    "Hiss", "Kass", "Kraak", "Krass", "Rass", "Rish", "Sask",
    "Siss", "Skrath", "Slash", "Sliss", "Slith", "Srak", "Ssar",
    "Ssath", "Sshiss", "Ssik", "Ssilt", "Tark", "Task",
    "Thrak", "Thresh", "Thriss", "Tikk", "Trask",
    "Tsak", "Tsar", "Tsith", "Vass", "Vrak",
    "Wash", "Wriss", "Xarr", "Xass", "Xish",
    "Yiss", "Zark", "Zass", "Zish", "Zulk"
};
std::vector<std::string> aLizardSuf = {
    "akh", "arash", "aris", "arth", "ash", "athiss", "ek", "ess",
    "eth", "ik", "ish", "iss", "ith", "kaas", "kash", "keth",
    "liss", "lath", "ok", "rak", "ras", "rash", "reth", "rik",
    "riss", "rith", "roth", "sek", "sith", "slith",
    "tharr", "thiss", "thresh", "tik", "uk",
    "ulk", "urr", "uss", "xarr", "zulk"
};
std::vector<std::string> aLizardTribe = {
    "Bloodscale", "Coldwater", "Darkmarsh", "Deadpool",
    "Fenmire", "Greathunter", "Longfang", "Mudhunter",
    "Nightstalker", "Oldwater", "Quicktongue", "Redclaw",
    "Saltmarsh", "Shadowscale", "Skyraider", "Stoneback",
    "Sunbasker", "Swamplurk", "Thornfang", "Warclaw",
    "Wetforest", "Whisperblade", "Wildrunner", "Yellowstrip", "Zilok"
};

// Centaur (I_CENTAURMAN): single Greek heroic first name + "of the [herd adj noun]".
// Source: Greek mythology (Chiron, Nessus, Eurytion); D&D Theros/Ravnica centaurs
// NOT Arabic (raceToEthnicity maps centaur → NOMAD, which is wrong for personal names)
// Space: centaurFirst(30) × herdAdj(20) × herdNoun(20) = 12,000
std::vector<std::string> aCentaurFirst = {
    "Achios", "Aetos", "Agathon", "Alexion", "Aristos",
    "Cheiron", "Dexios", "Elatos", "Eurytion", "Hippion",
    "Ixion", "Kallisto", "Kanthos", "Kratos", "Kydon",
    "Lykos", "Melanthon", "Nesos", "Nikion", "Orion",
    "Paion", "Pelion", "Phokos", "Rhekos", "Skiros",
    "Stratos", "Theron", "Xanthos", "Zephyros", "Astrion"
};
std::vector<std::string> aHerdAdj = {
    "Bold", "Far", "Fast", "Fleet", "Free", "Great", "High",
    "Iron", "Lone", "Lost", "Old", "Open", "Quick",
    "Proud", "Raging", "Rolling", "Roaming", "Swift", "Wild", "Wise"
};
std::vector<std::string> aHerdNoun = {
    "Field", "Gallop", "Gale", "Glade", "Grass", "Herd",
    "Hill", "Horizon", "Meadow", "Path", "Plain", "Prairie",
    "Ridge", "Run", "Steppe", "Storm", "Stream", "Thunder", "Trail", "Wind"
};

// Fairy (I_FAIRY): fey/pixie D&D-style — syllabic name + seasonal Court
// Source: D&D 5e Feywild lore, Pathfinder First World
// Space: aFairyPref(25) × aFairySuf(20) × aFairyCourt(20) = 10,000
std::vector<std::string> aFairyPref = {
    "Bell", "Crys", "Dew", "Dawn", "Flit", "Glim", "Gossam",
    "Lace", "Lumi", "Mist", "Moon", "Nim", "Petal", "Pix",
    "Sil", "Silk", "Star", "Sun", "Tink", "Twirl",
    "Vel", "Wisp", "Wish", "Zeph", "Lyra"
};
std::vector<std::string> aFairySuf = {
    "ael", "ara", "bella", "bryn", "drop",
    "ella", "iel", "ine", "ira", "iss",
    "mere", "ring", "wyn", "ze", "la",
    "lith", "shine", "bell", "vale", "flame"
};
std::vector<std::string> aFairyCourt = {
    "Amber", "Azure", "Blossom", "Crystal", "Dawn",
    "Dusk", "Evening", "Frost", "Gloaming", "Golden",
    "Jade", "Mist", "Moon", "Petal", "Rose",
    "Silver", "Starlight", "Thorn", "Twilight", "Verdant"
};

// Tiefling (I_TIEFLING): 60% infernal syllabic name, 40% virtue name
// Source: D&D 5e PHB Tiefling Names, Forgotten Realms infernal lore
// Space: aTiefPref(25) × aTiefSuf(18) + aTiefVirtue(20) ≈ 470 unique names
std::vector<std::string> aTiefPref = {
    "Ak", "Am", "Bar", "Bry", "Cri", "Da", "De", "Ek",
    "Ia", "Ib", "Ka", "Kal", "Le", "Mal", "Me",
    "Mor", "Ne", "Or", "Pe", "Pha", "Ri", "Sk", "Th", "Zar", "Zra"
};
std::vector<std::string> aTiefSuf = {
    "anos", "akos", "aron", "eis", "ella", "emon",
    "ia", "ios", "ira", "issa", "on", "os",
    "oth", "ra", "ris", "ren", "ros", "eth"
};
std::vector<std::string> aTiefVirtue = {
    "Anguish", "Carrion", "Chant", "Creed", "Despair",
    "Doom", "Exile", "Fear", "Gloom", "Grief",
    "Hope", "Ideal", "Malice", "Nowhere", "Penance",
    "Reverie", "Sorrow", "Torment", "Unrest", "Wrath"
};

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
            // Example: "Nim North Men"
            return (prefix + " " + dir + " " + roadWord) | filter::title_case;

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

/**
 * @brief Generates a culturally appropriate personal name for a solo player unit.
 *
 * Each race uses its own naming convention derived from authoritative sources.
 * Does NOT route through raceToEthnicity() — that function merges Hill/Ice/Under Dwarf
 * into Ethnicity::DWARF and Centaur into Ethnicity::NOMAD (Arabic), both wrong for names.
 *
 * Uniqueness: caller maintains a per-pass set; this function just generates. Large
 * combinatorial spaces (12k–6M) keep Birthday Problem collision probability < 2%.
 * See docs/UNIT_NAMING_SYSTEM.md for full Birthday Problem analysis and research sources.
 *
 * @param raceItem  IT_MAN or IT_LEADER item type (e.g. I_HILLDWARF, I_ORC, I_LEADERS)
 * @return Generated name string; unknown/disabled races fall back to getAbstractName()
 */
std::string getPersonName(int raceItem) {
    switch(raceItem) {

        case I_MAN: {
            // Anglo-Saxon first name + compound English surname
            // "Aldric Ironwood", "Godwin Stonepeak"
            std::string first = (getPrefix(aPrefMale) + getSuffix(aSufMale)) | filter::capitalize;
            std::string sur = (rng::one_of(aHumanSurAdj) + rng::one_of(aHumanSurNoun)) | filter::capitalize;
            return first + " " + sur;
        }

        case I_ORC: {
            // Hard-consonant core + 70% "the Adj Noun" epithet
            // "Krusk", "Azogdor the Bone Crusher"
            std::string name = (getPrefix(aPrefOrchish) + getSuffix(aSufOrchish)) | filter::capitalize;
            if (rng::get_random(10) < 7) {
                std::string epithet = (rng::one_of(aOrcEpiAdj) + " " + rng::one_of(aOrcEpiNoun)) | filter::title_case;
                name += " the " + epithet;
            }
            return name;
        }

        case I_HILLDWARF: {
            // Tolkien/Norse pref-suf + "of IronpeakClan" suffix
            // "Thorin-gol of Ironpeak Clan", "Khuz-zad of Stoneforge Clan"
            std::string name = (getPrefix(aPrefDwarven) + "-" + getSuffix(aSufDwarven)) | filter::capitalize;
            std::string clan = (rng::one_of(aClanAdj) + rng::one_of(aClanNoun)) | filter::capitalize;
            return name + " of " + clan + " Clan";
        }

        case I_HIGHELF: {
            // Quenya melodic pref+suf + "of Silver Dawn House" suffix
            // "Nimanis of Silver Dawn House", "Galadir of Moon Leaf House"
            std::string name = (getPrefix(aPrefElven2) + getSuffix(aSufElven2)) | filter::capitalize;
            std::string house = (rng::one_of(aElfClanAdj) + " " + rng::one_of(aElfClanNoun)) | filter::title_case;
            return name + " of " + house + " House";
        }

        case I_WOODELF: {
            // Sindarin shorter pref+suf + "of the Swift Wind" nature epithet
            // "Cylae of the Swift Wind", "Ryalin of the Silent Oak"
            std::string name = (getPrefix(aPrefElven1) + getSuffix(aSufElven1)) | filter::capitalize;
            std::string epithet = (rng::one_of(aEpithetAdj) + " " + rng::one_of(aEpithetNoun)) | filter::title_case;
            return name + " of the " + epithet;
        }

        case I_HOBBIT: {
            // Shire first name + compound plant/place surname (adj+noun)
            // "Peregrin Greenhill", "Lobelia Thornbrook"
            std::string first = rng::one_of(aHobbitsFirst);
            std::string sur = (rng::one_of(aHobbitsAdj) + rng::one_of(aHobbitsNoun)) | filter::capitalize;
            return first + " " + sur;
        }

        case I_LEADERS: {
            // Latin formal title + Anglo personal name
            // "Magister Aldric", "Prefect Waltheron"
            std::string title = rng::one_of(aLeaderTitle);
            std::string name = (getPrefix(aPrefMale) + getSuffix(aSufMale)) | filter::capitalize;
            return title + " " + name;
        }

        case I_DROWMAN: {
            // FR canon pref+suf + "of Baenre House" Menzoberranzan suffix
            // "Zar'ress of Baenre House", "Auvryrae of Xorlarrin House"
            std::string name = (getPrefix(aPrefDrow) + getSuffix(aSufDrow)) | filter::capitalize;
            return name + " of " + rng::one_of(aDrowHouses) + " House";
        }

        case I_GNOME: {
            // D&D-style compound nickname + family clan
            // "Nipsberry Timbers", "Wobblefizz Garrick"
            std::string nick = (rng::one_of(aGnomePart1) + rng::one_of(aGnomePart2)) | filter::capitalize;
            return nick + " " + rng::one_of(aGnomeClan);
        }

        case I_ICEDWARF: {
            // Norse pref+suf + "of Frostpeak Hold" suffix
            // "Dvalinur of Frostpeak Hold", "Orivar of Icehall Hold"
            std::string name = (rng::one_of(aIceDwarfPref) + rng::one_of(aIceDwarfSuf)) | filter::capitalize;
            std::string hold = (rng::one_of(aHoldAdj) + rng::one_of(aHoldNoun)) | filter::capitalize;
            return name + " of " + hold + " Hold";
        }

        case I_UNDERDWARF: {
            // Deep/dark pref+suf + "of Shadowvault Clan" suffix
            // "Kazagrak of Shadowvault Clan", "Narak of Deepforge Clan"
            std::string name = (rng::one_of(aUnderDwarfPref) + rng::one_of(aUnderDwarfSuf)) | filter::capitalize;
            std::string clan = (rng::one_of(aDarkClanAdj) + rng::one_of(aDarkClanNoun)) | filter::capitalize;
            return name + " of " + clan + " Clan";
        }

        case I_GOBLINMAN: {
            // Short punchy name + (60% "the tag", 40% compound surname)
            // "Nixflick the Dreamy", "Gobpox the Unlucky", "Graxgrib Longfinger"
            std::string name = (rng::one_of(aGoblinPref) + rng::one_of(aGoblinSuf)) | filter::capitalize;
            if (rng::get_random(10) < 6) {
                return name + " the " + rng::one_of(aGoblinTag);
            } else {
                std::string sur = (rng::one_of(aGoblinEpiAdj) + rng::one_of(aGoblinEpiNoun)) | filter::capitalize;
                return name + " " + sur;
            }
        }

        case I_LIZARDMAN: {
            // Sibilant hissing name + 50% "of the Bloodscale Tribe" suffix
            // "Ssarash", "Vraketh of the Bloodscale Tribe"
            std::string name = (rng::one_of(aLizardPref) + rng::one_of(aLizardSuf)) | filter::capitalize;
            if (rng::get_random(2) == 0) {
                return name + " of the " + rng::one_of(aLizardTribe) + " Tribe";
            }
            return name;
        }

        case I_CENTAURMAN: {
            // Greek heroic first name + "of the Swift Herd" suffix
            // "Theron of the Swift Herd", "Dexios of the Rolling Plain"
            std::string first = rng::one_of(aCentaurFirst);
            std::string herd = (rng::one_of(aHerdAdj) + " " + rng::one_of(aHerdNoun)) | filter::title_case;
            return first + " of the " + herd;
        }

        case I_FAIRY: {
            // Fey two-word name + "of X Court" affiliation
            // "Dew Drop of Silver Court", "Moon Bell of Twilight Court"
            std::string name = (rng::one_of(aFairyPref) + " " + rng::one_of(aFairySuf)) | filter::title_case;
            return name + " of " + rng::one_of(aFairyCourt) + " Court";
        }

        case I_TIEFLING: {
            // 60% infernal syllabic name, 40% dark virtue name
            // "Kairon", "Akmenos", "Despair", "Torment"
            if (rng::get_random(10) < 6) {
                return (rng::one_of(aTiefPref) + rng::one_of(aTiefSuf)) | filter::capitalize;
            } else {
                return rng::one_of(aTiefVirtue);
            }
        }

        default:
            // Unknown/disabled race — use abstract name so unit gets renamed regardless
            return getAbstractName();
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
            // Example: "Nim Las Sammath" = "White Forest Workshop"
            return (prefix + " " + resourceWord + " " + buildingWord) | filter::title_case;

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
               " Khazdur Peak",
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
