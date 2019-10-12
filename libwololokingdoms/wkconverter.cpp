#include "wkconverter.h"
#include "caseless.h"
#include "drs.h"
#include "drs_creator.h"
#include "fixes/ai900unitidfix.h"
#include "fixes/berbersutfix.h"
#include "fixes/burmesefix.h"
#include "fixes/cuttingfix.h"
#include "fixes/demoshipfix.h"
#include "fixes/disablenonworkingunits.h"
#include "fixes/ethiopiansfreepikeupgradefix.h"
#include "fixes/hotkeysfix.h"
#include "fixes/khmerfix.h"
#include "fixes/malayfix.h"
#include "fixes/maliansfreeminingupgradefix.h"
#include "fixes/portuguesefix.h"
#include "fixes/siegetowerfix.h"
#include "fixes/smallfixes.h"
#include "fixes/tricklebuildingfix.h"
#include "fixes/vietfix.h"
#include "md5.h"
#include "missing_strings.h"
#include "platform.h"
#include "string_helpers.h"
#include "wk_xml.h"
#include "wololo/datPatch.h"
#include "zr_map_creator.h"
#include <cctype>
#include <ctll.hpp>
#include <ctre.hpp>
#include <fs.h>
#include <fstream>
#include <genie/dat/DatFile.h>
#include <genie/lang/LangFile.h>
#include <map>
#include <optional>
#include <string>

enum TerrainType {
  None,
  WaterTerrain,
  FixedTerrain,
  LandTerrain,
  ForestTerrain,
  UnbuildableTerrain
};

struct MapConvertData {
  std::string slp_name;
  std::vector<std::string> const_names;
  std::string replaced_name;
  int old_terrain_id;
  int new_terrain_id;
  TerrainType terrain_type;
};

// this copy is unfortunate but cfs::resolve returns a temporary :/
fs::path resolve_path(const fs::path& input) {
#ifdef _WIN32
  return input;
#else
  return cfs::resolve(input);
#endif
}

void WKConverter::loadGameStrings(std::map<int, std::string>& langReplacement,
                                  fs::path file) {
  std::string line;
  std::ifstream translationFile(file);
  while (std::getline(translationFile, line)) {
    unsigned int index = line.find('=');
    std::string key = line.substr(0, index);
    try {
      int keyNo = std::stoi(key);
      langReplacement[keyNo] = line.substr(index + 1, std::string::npos);
    } catch (std::invalid_argument& e) {
      continue;
    }
  }
  translationFile.close();
}

/**
 * Index files to be written into the drs into a map, with the ID the file
 * will have later as  the key, and the path it should be copied from as the
 * value
 *
 * @param src: The directory to iterate through. All .slp and .wav files in this
 * directory will be indexed
 * @param expansionFiles: If false, files are written to a seperate map.
 *        They are not written to the drs later, but we need them for comparison
 * purposes in the independent architecture patching.
 * @param terrainFiles: If true, these are terrain files, written to a seperate
 * map, as we need them for expansion map creation.
 */
void WKConverter::indexDrsFiles(fs::path const& src, bool expansionFiles,
                                bool terrainFiles) {
  if (cfs::is_directory(src)) {
    for (const auto& current : fs::directory_iterator(src)) {
      indexDrsFiles(current.path(), expansionFiles, terrainFiles);
    }
  } else {
    std::string extension = src.extension().string();
    if (terrainFiles) {
      if (extension == ".slp") {
        newTerrainFiles[src.filename().string()] = src;
      }
    } else {
      if (extension == ".slp") {
        int id = atoi(src.stem().string().c_str());
        if (!expansionFiles)
          aocSlpFiles.insert(id);
        else
          slpFiles[id] = src;
      } else if (extension == ".wav") {
        int id = atoi(src.stem().string().c_str());
        wavFiles[id] = src;
      }
    }
  }
}

void WKConverter::copyHistoryFiles(fs::path inputDir, fs::path outputDir) {
  std::vector<std::string> civs = {
      "italians-utf8.txt",   "indians-utf8.txt", "incas-utf8.txt",
      "magyars-utf8.txt",    "slavs-utf8.txt",   "portuguese-utf8.txt",
      "ethiopians-utf8.txt", "malians-utf8.txt", "berbers-utf8.txt",
      "burmese-utf8.txt",    "malay-utf8.txt",   "vietnamese-utf8.txt",
      "khmer-utf8.txt"};
  for (auto& hd_name : civs) {
    auto aoc_name = hd_name.substr(0, hd_name.length() - 9) + ".txt";
    std::ifstream langIn(inputDir / hd_name);
    std::string contents = concat_stream(langIn);
    std::string outputContent = iconvert(contents, "UTF8", "WINDOWS-1252");

    std::ofstream langOut(outputDir / aoc_name);
    langOut << outputContent;
    langOut.close();
  }
}

using KeyValueString = std::pair<int, std::string>;
std::optional<KeyValueString> parseHDTextLine(std::string line) noexcept {
  int spaceIdx = line.find_first_of(" \t");
  auto stringKey = line.substr(0, spaceIdx);

  if (stringKey.empty() ||
      !std::all_of(stringKey.begin(), stringKey.end(),
                   [](char ch) { return std::isdigit(ch); })) {
    return std::nullopt;
  }

  int nb = std::atoi(stringKey.c_str());
  if (nb == 0) {
    // Conversion failed.
    return std::nullopt;
  }
  if (nb == 0xFFFF) {
    /*
     * this one seems to be used by AOC for dynamically-generated strings
     * (like market tributes), maybe it's the maximum the game can read ?
     */
    return std::nullopt;
  }
  if (nb <= 1000) {
    // skip changes to fonts
    return std::nullopt;
  }
  if (nb >= 20150 && nb <= 20167) {
    // skip the old civ descriptions
    return std::nullopt;
  }
  if (nb >= 9871 && nb <= 9946) {
    // skip the old civ descriptions
    return std::nullopt;
  }

  // load the string from the HD edition file
  int startIdx = line.find('"', spaceIdx);
  int endIdx = line.find('"', startIdx + 1);
  auto value = line.substr(startIdx + 1, endIdx - startIdx - 1);

  // Remap string key IDs to AoC

  if (nb >= 20312 && nb <= 20341) {
    switch (nb) {
    case 20312:
      nb = 20334;
      break;
    case 20313:
      nb = 20312;
      break;
    case 20314:
      nb = 20338;
      break;
    case 20315:
      nb = 20313;
      break;
    case 20316:
      nb = 20314;
      break;
    case 20317:
      nb = 20315;
      break;
    case 20318:
      nb = 20335;
      break;
    case 20319:
      nb = 20316;
      break;
    case 20320:
      nb = 20317;
      break;
    case 20321:
      nb = 20318;
      break;
    case 20322:
      nb = 20329;
      break;
    case 20323:
      nb = 20330;
      break;
    case 20324:
      nb = 20331;
      break;
    case 20325:
      nb = 20319;
      break;
    case 20326:
      nb = 20339;
      break;
    case 20327:
      nb = 20320;
      break;
    case 20328:
      nb = 20332;
      break;
    case 20329:
      nb = 20340;
      break;
    case 20330:
      nb = 20336;
      break;
    case 20331:
      nb = 20321;
      break;
    case 20332:
      nb = 20322;
      break;
    case 20333:
      nb = 20323;
      break;
    case 20334:
      nb = 20337;
      break;
    case 20335:
      nb = 20324;
      break;
    case 20336:
      nb = 20333;
      break;
    case 20337:
      nb = 20325;
      break;
    case 20338:
      nb = 20326;
      break;
    case 20339:
      nb = 20327;
      break;
    case 20340:
      nb = 20341;
      break;
    case 20341:
      nb = 20328;
      break;
    }
  }
  /*
   * Conquerors AI names start at 5800 (5800 = 4660+1140, so offset 1140 in the
   * xml file) However, there's only space for 10 civ AI names. We'll shift AI
   * names to 11500+ instead (offset 6840 or 1140+5700)
   */

  if (nb >= 5800 && nb < 6000) {
    nb += 5700;
  }
  if (nb >= 106000 && nb < 106160) { // AK&AoR AI names have 10xxxx id, get rid
                                     // of the 10, then shift
    nb -= 100000;
    nb += 5700;
  }

  // These civ descriptions can be too long for the tech tree, we'll take out
  // some newlines
  if (nb >= 120150 && nb <= 120180) {
    if (nb == 120156 || nb == 120155) {
      replace_all(value, "civilization \\n\\n", "civilization \\n");
    }
    if (nb == 120167) {
      replace_all(value, "civilization \\n\\n", "civilization \\n");
      replace_all(value, "\\n\\n<b>Unique Tech", "\\n<b>Unique Tech");
    }
    // replace the old descriptions of the civs in the base game
    nb -= 100000;
  }

  return std::make_pair(nb, value);
}

void WKConverter::createLanguageFile(fs::path languageIniPath,
                                     fs::path patchFolder) {
  std::map<int, std::string> langReplacement;
  fs::path keyValuesStringsPath =
      settings.language == "zht"
          ? resourceDir / "locales" / "traditional chinese game strings" /
                "key-value-strings-utf8.txt"
          : settings.hdPath / "resources" / settings.language / "strings" /
                "key-value" / "key-value-strings-utf8.txt";
  fs::path improvedTooltipStrings =
      resourceDir / "improved-tooltips" / (settings.language + ".ini");
  /*
   * Create the language files (.ini for Voobly, .dll for offline)
   */

  fs::path extraGameStringFile =
      fs::path(resourceDir / "locales" / (settings.language + "_game.txt"));
  if (fs::exists(extraGameStringFile)) {
    loadGameStrings(langReplacement, extraGameStringFile);
  } else {
    loadGameStrings(langReplacement, resourceDir / "locales" / ("en_game.txt"));
  }

  listener->log("Replace tooltips");
  if (settings.replaceTooltips) {
    loadModdedStrings(improvedTooltipStrings, langReplacement);
  }
  listener->increaseProgress(1); // 2

  if (settings.patch >= 0 &&
      ((std::get<3>(settings.dataModList[settings.patch]) & 2) != 0)) {
    /*
     * A data mod might need slightly changed strings.
     */
    std::ifstream modLang(patchFolder / (settings.language + ".txt"));
    std::string line;
    while (std::getline(modLang, line)) {
      auto result = parseHDTextLine(line);
      if (result) {
        auto& [id, value] = result.value();
        langReplacement[id] = value;
      }
    }
    modLang.close();
    if (settings.replaceTooltips) {
      loadModdedStrings(patchFolder / (settings.language + ".ini"),
                        langReplacement);
    }
  }

  std::ifstream langIn(keyValuesStringsPath);
  std::ofstream langOut(languageIniPath);

  listener->increaseProgress(1); // 3

  listener->log("convert language file");
  listener->increaseProgress(1); // 4
  convertLanguageFile(langIn, langOut, langReplacement);
  listener->increaseProgress(1); // 5
}

void WKConverter::loadModdedStrings(
    fs::path moddedStringsFile, std::map<int, std::string>& langReplacement) {
  std::ifstream modLang(moddedStringsFile);
  std::string line;
  while (std::getline(modLang, line)) {
    unsigned int spaceIdx = line.find('=');
    std::string number = line.substr(0, spaceIdx);
    int nb;
    try {
      nb = stoi(number);
    } catch (std::invalid_argument const& e) {
      continue;
    }
    line = line.substr(spaceIdx + 1, std::string::npos);

    line = iconvert(line, "WINDOWS-1252", "UTF8");
    langReplacement[nb] = line;
  }
  modLang.close();
}

void WKConverter::convertLanguageFile(
    std::ifstream& in, std::ofstream& iniOut,
    std::map<int, std::string>& langReplacement) {
  std::string line;
  int nb;
  while (std::getline(in, line)) {
    auto result = parseHDTextLine(line);
    if (result) {
      std::tie(nb, line) = result.value();
    } else {
      continue;
    }
    if (langReplacement.count(nb) != 0u) {
      // this string has been changed by one of our patches (modified attributes
      // etc.)
      line = langReplacement[nb];
      langReplacement.erase(nb);
    }
    // convert UTF-8 into ANSI
    std::string outputLine = iconvert(line, "UTF8", "WINDOWS-1252");
    iniOut << std::to_string(nb) << '=' << outputLine << std::endl;
  }

  /*
   * Stuff that's in lang replacement but not in the HD files (in this case
   * extended language height box)
   */
  for (auto& [id, value] : langReplacement) {
    // convert UTF-8 into ANSI
    std::string outputLine = iconvert(value, "UTF8", "WINDOWS-1252");
    iniOut << std::to_string(id) << '=' << outputLine << std::endl;
  }
  for (auto& [id, value] : missing_strings) {
    iniOut << std::to_string(id) << '=' << value << std::endl;
  }
  in.close();
  iniOut.close();
}

void WKConverter::makeRandomMapScriptsDrs(std::ofstream& out,
                                          const fs::path& drsDir) {
  DRSCreator drs(out);
  for (const auto& p : fs::directory_iterator(resolve_path(drsDir))) {
    auto id = std::atoi(p.path().stem().c_str());
    drs.addFile(DRSTableType::Bina, id, p.path());
  }
  drs.commit();
}

void WKConverter::makeDrs(std::ofstream& out) {
  DRSCreator drs(out);
  listener->setInfo("working$\n$workingDrs");

  // Exclude Forgotten Empires leftovers
  // slpFiles.erase(50163); // Forgotten Empires loading screen
  slpFiles.erase(50189); // Forgotten Empires main menu
  // slpFiles.erase(53207); // Forgotten Empires in-game logo
  slpFiles.erase(53208); // Forgotten Empires objective window
  slpFiles.erase(53209); // ???

  /*
   * Some of the interface files are duplicates because of the shifting,
   * get rid of those
   */
  auto start = slpFiles.find(51132);
  slpFiles.erase(start, std::next(start, 8));
  start = slpFiles.find(51172);
  slpFiles.erase(start, std::next(start, 8));

  for (auto& [id, data] : slpFiles) {
    drs.addFile(DRSTableType::Slp, id, data);
  }
  listener->increaseProgress(2);
  for (auto& [id, data] : wavFiles) {
    drs.addFile(DRSTableType::Wav, id, data);
  }
  listener->increaseProgress(2);

  listener->setInfo("working$\n$workingDrs2");
  drs.commit();
  listener->increaseProgress(4);
  out.close();
}

#include "drs_reader.h"
/** A method to add extra (slp) files to an already existing drs file.
 *  Reads the old files, changes the offsets where necessary and writes back to
 * a new file with changed offsets and the new files in: The stream of the old
 * drs file out: The stream of the new drs file
 */
void WKConverter::editDrs(fs::path input_path, std::ostream& out) {
  DRSReader reader(input_path);
  DRSCreator writer(out);

  // These are the new files to be added to the drs
  listener->log("number of new slps: " + std::to_string(slpFiles.size()));
  listener->setInfo("working$\n$workingDrs2");

  listener->log("read existing files");
  // Copy all existing files into the new file
  // TODO do not allocate all of these at once? Maybe use iterator pairs for the mmapped file?
  auto tables = reader.read_tables();
  for (const auto& table : tables) {
    auto files = reader.read_files(table);
    for (const auto& file : files) {
      int fileId = file.id();
      // Remap some IDs
      if (fileId >= 60000 &&
          (fileId <= 60138 || (fileId >= 70000 && fileId <= 70138) ||
           (fileId >= 80000 && fileId <= 80017))) {
        // First if is just a cheaper hardcoded precheck, can be removed if the
        // function needs to be more general
        if (slpFiles.count(fileId) > 0) {
          fileId += 900000; // Doesn't really matter, just a large number that the
                            // game will never read
        }
      }

      writer.addFile(table.file_type(),
          fileId,
          reader.read_file(file));
    }
  }

  listener->increaseProgress(3); // 24
  listener->log("add new files");

  for (auto& [id, path] : slpFiles) {
    writer.addFile(DRSTableType::Slp, id, path);
  }

  for (auto& [id, path] : wavFiles) {
    writer.addFile(DRSTableType::Wav, id, path);
  }
  listener->increaseProgress(1); // 25

  listener->log("write the whole bunch");
  listener->setInfo("working$\n$workingDrs3");
  writer.commit();
  listener->increaseProgress(8); // 33
}

void WKConverter::copyCivIntroSounds(const fs::path& inputDir,
                                     const fs::path& outputDir) {
  std::vector<std::string> civs = {
      "italians",   "indians",    "incas",   "magyars", "slavs",
      "portuguese", "ethiopians", "malians", "berbers", "burmese",
      "malay",      "vietnamese", "khmer"};
  for (auto& civ_name : civs) {
    std::error_code ec;
    cfs::copy_file(inputDir / (civ_name + ".mp3"),
                   outputDir / (civ_name + ".mp3"), ec);
  }
}

/*
 * Shifts the offset between interface files by 10 so there's space for the new
 * civs
 */
static void shiftHudIndices(std::map<int, fs::path>& slpFiles,
                            const fs::path& assetsPath) {
  const std::array hudFiles = {51130, 51160};
  int base_index = 0;
  for (auto hud_file_id : hudFiles) {
    for (auto i = 1; i <= 23; i++) {
      slpFiles[hud_file_id + i + (base_index + 1) * 10] =
          assetsPath / (std::to_string(hud_file_id + i) + ".slp");
    }
    base_index++;
  }
}

void WKConverter::copyWallFiles(const fs::path& inputDir) {
  /*
   * Base IDS 2098 Stone 2110 Fortified 4169 Damaged 4173 Damaged Fortified
   * Central European +0, Asian +1, Middle Eastern +2, West European +3
   * +8 per damage increase
   *
   * New Types:
   * 5000 American, 7000 Mediterranean, 15000 Eastern European, 16000 Indian,
   * 17000 African, 18000 South East Asian Base IDs 124 Stone 126 Fortified 145
   * Damaged 147 Damaged Fortified +4 per damage increase
   */
  indexDrsFiles(inputDir);
  std::array conversionTable = {3, -15, 2, 0, 3, -18, -5, 1,  0,
                                1, 2,   3, 0, 1, 2,   2,  -5, -7};
  int newBaseSLP = 24000;
  for (size_t i = 0; i < conversionTable.size(); i++) {
    int archID = conversionTable[i];
    if (archID < 0) {
      archID *= -1;
      int digits = archID == 5 || archID == 7 ? 124 : 324;
      slpFiles[newBaseSLP + i * 1000 + 324] =
          inputDir / (std::to_string(archID * 1000 + digits) + ".slp");
      slpFiles[newBaseSLP + i * 1000 + 326] =
          inputDir / (std::to_string(archID * 1000 + digits + 2) + ".slp");
      for (int j = 0; j <= 10; j += 2)
        slpFiles[newBaseSLP + i * 1000 + 345 + j] =
            inputDir /
            (std::to_string(archID * 1000 + digits + 21 + j) + ".slp");
    } else {
      slpFiles[newBaseSLP + i * 1000 + 324] =
          inputDir / (std::to_string(2098 + archID) + ".slp");
      slpFiles[newBaseSLP + i * 1000 + 326] =
          inputDir / (std::to_string(2110 + archID) + ".slp");
      for (int j = 0; j <= 10; j += 2)
        slpFiles[newBaseSLP + i * 1000 + 345 + j] =
            inputDir / (std::to_string(4169 + archID + j * 2) + ".slp");
    }
  }
}

void WKConverter::createMusicPlaylist(const fs::path& inputDir,
                                      const fs::path& outputDir) {
  std::ofstream outputFile(outputDir);
  for (int i = 1; i <= 23; i++) {
    outputFile << inputDir.string() << "\\xmusic" << std::to_string(i) << ".mp3"
               << std::endl;
  }
  outputFile.close();
}

static const std::array<std::map<int, std::string_view>, 6> terrainsPerType = {
    {// The Order is important, see the TerrainTypes Enum!
     {},
     {{23, "MED_WATER"}, {22, "DEEP_WATER"}},
     {},
     {{0, "GRASS"},
      {3, "DIRT3"},
      {6, "DIRT1"},
      {9, "GRASS3"},
      {12, "GRASS2"},
      {14, "DESERT"},
      {24, "ROAD"},
      {25, "ROAD2"},
      {39, "ROAD3"}},
     {{10, "FOREST"}, {13, "PALM_DESERT"}, {21, "SNOW_FOREST"}},
     {{40, "DLC_ROCK"}, {35, "ICE"}}}};

static const std::vector<MapConvertData> rmsTerrainReplacements = {
    // slp_name, const_name_pattern, replaced_name_pattern, old_terrain_id,
    // new_terrain_id, terrain_type
    {"DRAGONFOREST.slp",
     {"DRAGONFORES", "DRAGONFOREST"},
     "DRAGONFOREST",
     48,
     21,
     TerrainType::ForestTerrain},
    {"ACACIA_FOREST.slp",
     {"ACCACIA_FOREST", "ACACIA_FOREST", "ACACIAFORES"},
     "ACACIA_FOREST",
     50,
     41,
     TerrainType::None},
    {"DLC_RAINFOREST.slp",
     {"DLC_RAINFOREST"},
     "DLC_RAINFOREST",
     56,
     10,
     TerrainType::ForestTerrain},
    {"BAOBAB.slp",
     {"BAOBABS", "BAOBAB_FOREST"},
     "BAOBAB_FOREST",
     49,
     16,
     TerrainType::None},
    {"DLC_MANGROVESHALLOW.slp",
     {"DLC_MANGROVESHALLOW"},
     "DLC_MANGROVESHALLOW",
     54,
     11,
     TerrainType::None},
    {"DLC_MANGROVEFOREST.slp",
     {"DLC_MANGROVEFOREST"},
     "DLC_MANGROVEFOREST",
     55,
     20,
     TerrainType::None},
    {"DLC_NEWSHALLOW.slp",
     {"DLC_NEWSHALLOW"},
     "DLC_NEWSHALLOW",
     59,
     4,
     TerrainType::FixedTerrain},
    {"SAVANNAH.slp",
     {"SAVANNAH", "DLC_SAVANNAH"},
     "SAVANNAH",
     41,
     14,
     TerrainType::LandTerrain},
    {"DIRT4.slp",
     {"DIRT4", "DLC_DIRT4"},
     "DIRT4",
     42,
     3,
     TerrainType::LandTerrain},
    {"MOORLAND.slp",
     {"DLC_MOORLAND", "MOORLAND"},
     "DLC_MOORLAND",
     44,
     9,
     TerrainType::LandTerrain},
    {"CRACKEDIT.slp", {"CRACKEDIT"}, "CRACKEDIT", 45, 38, TerrainType::None},
    {"QUICKSAND.slp",
     {"QUICKSAND", "DLC_QUICKSAND"},
     "QUICKSAND",
     46,
     40,
     TerrainType::FixedTerrain},
    {"BLACK.slp",
     {"BLACK", "DLC_BLACK"},
     "DLC_BLACK",
     47,
     40,
     TerrainType::FixedTerrain},
    {"DLC_BEACH2.slp",
     {"DLC_BEACH2"},
     "DLC_BEACH2",
     51,
     2,
     TerrainType::FixedTerrain},
    {"DLC_BEACH3.slp",
     {"DLC_BEACH3"},
     "DLC_BEACH3",
     52,
     2,
     TerrainType::FixedTerrain},
    {"DLC_BEACH4.slp",
     {"DLC_BEACH4"},
     "DLC_BEACH4",
     53,
     2,
     TerrainType::FixedTerrain},
    {"DLC_DRYROAD.slp",
     {"DLC_DRYROAD"},
     "DLC_DRYROAD",
     43,
     25,
     TerrainType::LandTerrain},
    {"DLC_WATER4.slp",
     {"DLC_WATER4"},
     "DLC_WATER4",
     57,
     22,
     TerrainType::WaterTerrain},
    {"DLC_WATER5.slp",
     {"DLC_WATER5"},
     "DLC_WATER5",
     58,
     1,
     TerrainType::WaterTerrain},
    {"DLC_JUNGLELEAVES.slp",
     {"DLC_JUNGLELEAVES"},
     "DLC_JUNGLELEAVES",
     62,
     5,
     TerrainType::LandTerrain},
    {"DLC_JUNGLEROAD.slp",
     {"DLC_JUNGLEROAD"},
     "DLC_JUNGLEROAD",
     61,
     39,
     TerrainType::LandTerrain},
    {"DLC_JUNGLEGRASS.slp",
     {"DLC_JUNGLEGRASS"},
     "DLC_JUNGLEGRASS",
     60,
     12,
     TerrainType::LandTerrain}};

static const std::map<int, std::string> terrainSLPNames = {
    {0, "15001.slp"},  {1, "15002.slp"},  {2, "15017.slp"},  {3, "15007.slp"},
    {4, "15014.slp"},  {5, "15011.slp"},  {6, "15014.slp"},  {9, "15009.slp"},
    {10, "15011.slp"}, {12, "15008.slp"}, {13, "15010.slp"}, {14, "15010.slp"},
    {21, "15029.slp"}, {22, "15015.slp"}, {23, "15016.slp"}, {24, "15018.slp"},
    {25, "15019.slp"}, {35, "15024.slp"}, {39, "15031.slp"}, {40, "15033.slp"}};

void WKConverter::copyHDMaps(const fs::path& inputDir,
                             const fs::path& outputDir, bool replace) {
  // Find all maps to be converted
  std::vector<fs::path> mapNames;
  for (const auto& it : fs::directory_iterator(resolve_path(inputDir))) {
    auto extension = it.path().extension();
    if (extension == ".rms") {
      mapNames.push_back(it.path());
    }
  }
  listener->increaseProgress(2); // 15+19
  std::map<std::string, fs::path> terrainOverrides;

  // Main Loop, Iterate through the maps to be converted
  for (auto& it : mapNames) {
    /*
     * If the Map is already a ZR@ map, just copy it
     * If not, check if a ZR@ map with that name exists in the target directory
     * and remove it to be replaced by the newly converted map, if the replace
     * option is set to true. Else skip that map.
     */
    std::string mapName = it.stem().string() + ".rms";
    std::string mapPrefix = mapName.substr(0, 3);
    if (mapPrefix == "ZR@") {
      cfs::copy_file(it, outputDir / mapName,
                     fs::copy_options::update_existing);
      continue;
    }
    if (mapPrefix == "es_") {
      continue; // These maps are already added to Voobly with the es@ prefix,
                // no need to have this twice
    }
    if (cfs::exists(outputDir / it.filename()) ||
        cfs::exists(outputDir / ("ZR@" + it.filename().string()))) {
      if (replace)
        cfs::remove(outputDir / it.filename());
      else
        continue;
    }
    cfs::remove(outputDir / ("ZR@" + it.filename().string()));
    std::ifstream input(resolve_path(inputDir / it.filename()));
    std::string map = concat_stream(input);
    input.close();

    std::map<int, bool> terrainsUsed = {
        {0, false},  {1, false},  {2, false},  {3, false},  {4, false},
        {5, false},  {6, false},  {9, false},  {10, false}, {11, false},
        {12, false}, {13, false}, {14, false}, {16, false}, {20, false},
        {21, false}, {22, false}, {23, false}, {24, false}, {25, false},
        {35, false}, {38, false}, {39, false}, {40, false},

        {41, false}, {42, false}, {43, false}, {44, false}, {45, false},
        {46, false}, {47, false}, {48, false}, {49, false}, {50, false},
        {51, false}, {52, false}, {53, false}, {54, false}, {55, false},
        {56, false}, {57, false}, {58, false}, {59, false}, {60, false},
        {61, false}, {62, false}};

    /*
     * Search for specific constant names in the map.
     * Some constants have multiple spellings, which is why const_names is a
     * vector. Finding one is enough, so in that case we can break out of the
     * inner loop
     */
    for (auto& replacement : rmsTerrainReplacements) {
      for (auto& const_name : replacement.const_names) {
        if (map.find(const_name) != std::string::npos) {
          if (replacement.new_terrain_id < 41) {
            // 41 is also an expansion terrain, but that's okay, it's a fixed
            // replacement
            terrainsUsed.at(replacement.new_terrain_id) = true;
          }
          terrainsUsed.at(replacement.old_terrain_id) = true;
          break;
        }
      }
    }

    for (auto& replacement : rmsTerrainReplacements) {
      if (!terrainsUsed.at(replacement.old_terrain_id))
        continue;
      // Check if replacement candidate is already used
      int usedTerrain = replacement.new_terrain_id;
      // If it's one of the terrains with a shared slp, we need to search the
      // map for these other terrains too, else just the usedTerrain
      if (replacement.terrain_type > TerrainType::FixedTerrain &&
          isTerrainUsed(usedTerrain, terrainsUsed, map,
                        terrainsPerType[replacement.terrain_type])) {
        bool success = false;
        for (auto& [id, rx] : terrainsPerType[replacement.terrain_type]) {
          if (terrainsUsed.at(id))
            continue;
          if (isTerrainUsed(id, terrainsUsed, map,
                            terrainsPerType[replacement.terrain_type])) {
            continue;
          }
          success = true;
          usedTerrain = id;
          terrainsUsed.at(id) = true;
          break;
        }
        if (!success && replacement.terrain_type == TerrainType::LandTerrain &&
            !isTerrainUsed(5, terrainsUsed, map,
                           terrainsPerType[replacement.terrain_type])) {
          usedTerrain =
              5; // Leaves is a last effort, usually likely to be used already
          terrainsUsed[5] = true;
        }
      }

      std::regex terrainConstDef;
      std::string terrainName;
      if (replacement.const_names.size() == 1)
        terrainName = replacement.const_names[0];
      else {
        terrainName = "(";
        for (auto& const_name : replacement.const_names) {
          terrainName += const_name + "|";
        }
        terrainName[terrainName.size() - 1] = ')';
      }
      if (usedTerrain != replacement.new_terrain_id) {
        map = std::regex_replace(map, std::regex(terrainName),
                                 "MY" + replacement.replaced_name);
        std::regex terrainConstDef =
            std::regex("#const\\sMY+" + terrainName + "\\s+" +
                       std::to_string(replacement.old_terrain_id));
        std::string temp =
            std::regex_replace(map, terrainConstDef,
                               "#const MY" + replacement.replaced_name + " " +
                                   std::to_string(usedTerrain));
        if (temp != map)
          map = temp;
        else {
          map = "#const MY" + replacement.replaced_name + " " +
                std::to_string(usedTerrain) + "\n" + map;
        }
      } else {
        terrainConstDef =
            std::regex("#const\\s+" + terrainName + "\\s+" +
                       std::to_string(replacement.old_terrain_id));
        map = std::regex_replace(map, terrainConstDef,
                                 "#const " + replacement.replaced_name + " " +
                                     std::to_string(usedTerrain));
      }

      if (replacement.terrain_type == TerrainType::None ||
          (replacement.terrain_type == TerrainType::WaterTerrain &&
           usesMultipleWaterTerrains(map, terrainsUsed)))
        continue;

      terrainOverrides[terrainSLPNames.at(usedTerrain)] =
          newTerrainFiles.at(replacement.slp_name);

      if (replacement.terrain_type == TerrainType::ForestTerrain) {
        upgradeTrees(usedTerrain, replacement.old_terrain_id, map);
      }
    }
    if (terrainsUsed.at(11)) {
      terrainOverrides["15004.slp"] = newTerrainFiles.at("15004.slp");
      terrainOverrides["15005.slp"] = newTerrainFiles.at("15005.slp");
      terrainOverrides["15021.slp"] = newTerrainFiles.at("15021.slp");
      terrainOverrides["15022.slp"] = newTerrainFiles.at("15022.slp");
      terrainOverrides["15023.slp"] = newTerrainFiles.at("15023.slp");
    }
    std::ofstream out(outputDir.string() + "/" + mapName);
    out << map;
    out.close();
    if (mapName.substr(0, 3) == "rw_" || mapName.substr(0, 3) == "sm_") {
      std::string scenarioFile = it.stem().string() + ".scx";
      terrainOverrides[scenarioFile] = resolve_path(inputDir / scenarioFile);
    }
    if (!terrainOverrides.empty()) {
      createZRmap(terrainOverrides, outputDir, mapName);
    }
    terrainOverrides.clear();
  }
  listener->increaseProgress(1); // 16+20 22?
}

static constexpr auto rxAnyWatterConst =
    ctll::fixed_string(R"(\W(MED_|DEEP_)?WATER|DLC_WATER[45]\W)");
bool WKConverter::usesMultipleWaterTerrains(const std::string& map,
                                            std::map<int, bool>& terrainsUsed) {
  if (!terrainsUsed[23]) {
    terrainsUsed[23] = ctre::search<rxAnyWatterConst>(map);
  }
  return terrainsUsed[23];
}

static const std::regex rxPlayerSetup(R"(<PLAYER_SETUP>\s*(\r*)\n)");
static const std::regex
    rxIncludeDrs(R"(#include_drs\s+random_map\.def\s*(\r*)\n)");
void WKConverter::upgradeTrees(int usedTerrain, int oldTerrain,
                               std::string& map) {
  std::string newTree;
  std::string oldTree;
  switch (usedTerrain) {
  case 10:
    oldTree = "FOREST_TREE";
    break;
  case 13:
    oldTree = "PALMTREE";
    break;
  case 21:
    oldTree = "SNOWPINETREE";
    break;
  }
  if (oldTerrain == 48) {
    newTree = "DRAGONTREE";
  } else {
    newTree = "DLC_RAINTREE";
  }
  if (map.find("<PLAYER_SETUP>") != std::string::npos)
    map = std::regex_replace(
        map, rxPlayerSetup,
        "<PLAYER_SETUP>$1\n  effect_amount GAIA_UPGRADE_UNIT " + oldTree + " " +
            newTree + " 0$1\n");
  else
    map =
        std::regex_replace(map, rxIncludeDrs,
                           "#include_drs random_map.def$1\n<PLAYER_SETUP>$1\n  "
                           "effect_amount GAIA_UPGRADE_UNIT " +
                               oldTree + " " + newTree + " 0$1\n");
}

bool contains_rms_word(std::string_view haystack, std::string_view needle) {
  auto index = haystack.find(needle);
  if (index == std::string_view::npos) {
    return false;
  } else {
    // We can only check for whitespace, because the RMS parser in the game is
    // extremely whitespace sensitive. For example, it parser `WATER)` as a
    // single word, not `WATER` followed by `)`.
    auto endOfName = index + needle.size();
    if ((index > 0 && !std::isspace(haystack[index - 1])) ||
        (endOfName < haystack.size() - 1 && !std::isspace(haystack[endOfName]))) {
      return false;
    }
  }
  return true;
}

static constexpr auto rxForest =
    ctll::fixed_string("\\W(PINE_FOREST|LEAVES|JUNGLE|BAMBOO|FOREST)\\W");
static constexpr auto rxDesert =
    ctll::fixed_string("\\W(PALM_DESERT|DESERT)\\W");
bool WKConverter::isTerrainUsed(
    int terrain, std::map<int, bool>& terrainsUsed, const std::string& map,
    const std::map<int, std::string_view>& constNames) {
  if (terrain == 5 || terrain == 10) {
    if (!terrainsUsed[63]) {
      terrainsUsed[63] = true;
      terrainsUsed[64] = ctre::search<rxForest>(map);
    }
    return terrainsUsed[64];
  }
  if (terrain == 13 || terrain == 14) {
    if (!terrainsUsed[65]) {
      terrainsUsed[65] = true;
      terrainsUsed[66] = ctre::search<rxDesert>(map);
    }
    return terrainsUsed[66];
  }
  terrainsUsed[terrain] = contains_rms_word(map, constNames.at(terrain));
  return terrainsUsed[terrain];
}

void WKConverter::createZRmap(std::map<std::string, fs::path>& terrainOverrides,
                              fs::path outputDir, std::string mapName) {
  fs::path outname = outputDir / ("ZR@" + mapName);
  std::ofstream outstream(outname, std::ios_base::out | std::ios_base::binary);
  outstream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  ZRMapCreator map(outstream);
  terrainOverrides[mapName] = outputDir / mapName;
  for (auto& [name, path] : terrainOverrides) {
    std::ifstream file_stream(path, std::ios_base::in | std::ios_base::binary);
    map.addFile(name, file_stream);
  }
  map.end();
  outstream.close();
  cfs::remove(outputDir / mapName);
}

void WKConverter::transferHdDatElements(genie::DatFile* hdDat,
                                        genie::DatFile* aocDat) {

  aocDat->Sounds = std::move(hdDat->Sounds);
  aocDat->GraphicPointers = std::move(hdDat->GraphicPointers);
  aocDat->Graphics = std::move(hdDat->Graphics);
  aocDat->Effects = std::move(hdDat->Effects);
  aocDat->UnitHeaders = std::move(hdDat->UnitHeaders);
  aocDat->Civs = std::move(hdDat->Civs);
  aocDat->Techs = std::move(hdDat->Techs);
  aocDat->UnitLines = std::move(hdDat->UnitLines);
  aocDat->TechTree = std::move(hdDat->TechTree);
  aocDat->TerrainRestrictions.push_back(aocDat->TerrainRestrictions[14]);
  aocDat->FloatPtrTerrainTables.push_back(1);
  aocDat->TerrainPassGraphicPointers.push_back(1);
  for (auto& it : aocDat->TerrainRestrictions[22].TerrainPassGraphics) {
    it.EnterTileSpriteID = 11164;
  }

  // Copy Terrains
  aocDat->TerrainBlock.TerrainsUsed2 = 42;
  aocDat->TerrainsUsed1 = 42;

  terrainSwap(hdDat, aocDat, 33, 38, 15030); // Snow road to snow dirt
  terrainSwap(hdDat, aocDat, 38, 45, 15032); // Cracked Earth to Snow road

  terrainSwap(hdDat, aocDat, 11, 54, 15012); // mangrove terrain
  terrainSwap(hdDat, aocDat, 20, 55, 15012); // mangrove forest
  terrainSwap(hdDat, aocDat, 41, 50, 15013); // acacia forest
  terrainSwap(hdDat, aocDat, 16, 49, 15025); // baobab forest

  const std::array<std::tuple<int, std::string>, 7> newTerrainSlps = {
      {{15012, "DLC_MANGROVEFOREST.slp"},
       {15013, "ACACIA_FOREST.slp"},
       {15025, "BAOBAB.slp"},
       {15003, "15003.slp"},
       {15032, "CRACKEDIT.slp"},
       {15034, "ICE_SOLID.slp"},
       {15020, "ICE_BEACH.slp"}}};

  for (auto& [id, name] : newTerrainSlps) {
    if (slpFiles[id].empty())
      slpFiles[id] = newTerrainFiles[name];
  }

  aocDat->TerrainBlock.Terrains[35].TerrainToDraw = -1;
  aocDat->TerrainBlock.Terrains[35].SLP = 15020;
  aocDat->TerrainBlock.Terrains[35].Name2 = "g_ice";

  aocDat->TerrainBlock.Terrains[37].TerrainToDraw = -1;
  aocDat->TerrainBlock.Terrains[37].SLP = 15034;

  aocDat->TerrainBlock.Terrains[36].TerrainToDraw = -1;
  aocDat->TerrainBlock.Terrains[36].SLP = 15027;

  aocDat->TerrainBlock.Terrains[15].SLP = 15003;
  aocDat->TerrainBlock.Terrains[40].SLP = 15033;
  aocDat->TerrainBlock.Terrains[40].TerrainToDraw = -1;
  aocDat->TerrainBlock.Terrains[15].TerrainToDraw = -1;
}

void WKConverter::adjustArchitectureFlags(genie::DatFile* aocDat,
                                          fs::path flagFilename) {
  std::string line;
  std::ifstream flagFile(flagFilename);
  while (std::getline(flagFile, line)) {
    int index = line.find(',');
    int civID = std::atoi(line.substr(0, index).c_str());
    line = line.substr(index + 1, std::string::npos);
    index = line.find(',');
    int unitID = std::atoi(line.substr(0, index).c_str());
    line = line.substr(index + 1, std::string::npos);
    index = line.find(',');
    int delta = std::atoi(line.substr(0, index).c_str());
    line = line.substr(index + 1, std::string::npos);
    index = line.find(',');
    int x = std::atoi(line.substr(0, index).c_str());
    int y = std::atoi(line.substr(index + 1, std::string::npos).c_str());
    if (unitID == 18 || unitID == 103) {
      aocDat->Graphics[aocDat->Civs[civID].Units[unitID].StandingGraphic.first]
          .Deltas[delta]
          .OffsetX = x;
      aocDat->Graphics[aocDat->Civs[civID].Units[unitID].StandingGraphic.first]
          .Deltas[delta]
          .OffsetY = y;
    } else {
      aocDat
          ->Graphics
              [aocDat->Civs[civID].Units[unitID].Creatable.GarrisonGraphic]
          .Deltas[delta]
          .OffsetX = x;
      aocDat
          ->Graphics
              [aocDat->Civs[civID].Units[unitID].Creatable.GarrisonGraphic]
          .Deltas[delta]
          .OffsetY = y;
    }
  }
  flagFile.close();
}

void WKConverter::patchArchitectures(genie::DatFile* aocDat) {
  char const* civLetterList = "XEWMFI";
  civLetters.insert(civLetterList, civLetterList + strlen(civLetterList));

  /*
   * Manual Fixes before the IA seperation
   */

  aocDat->Graphics[3229].Deltas[0].GraphicID = 427;
  aocDat->Graphics[3229].Deltas[1].GraphicID = 428;

  aocDat->Graphics[9196].Deltas.erase(aocDat->Graphics[9196].Deltas.begin());

  /*
   * IA seperation
   */

  const std::array buildingIDs = {
      10,  14,  18,  19,  20,  30,  31,  32,  47,   49,  51,  63,  64,
      67,  71,  78,  79,  80,  81,  82,  84,  85,   86,  87,  88,  90,
      91,  92,  95,  101, 103, 104, 105, 110, 116,  117, 129, 130, 131,
      132, 133, 137, 141, 142, 150, 153, 155, 179,  190, 209, 210, 234,
      235, 236, 276, 463, 464, 465, 481, 482, 483,  484, 487, 488, 490,
      491, 498, 562, 563, 564, 565, 584, 585, 586,  587, 597, 611, 612,
      613, 614, 615, 616, 617, 659, 660, 661, 662,  663, 664, 665, 666,
      667, 668, 669, 670, 671, 672, 673, 674, 1102, 1189};
  const std::array unitIDs = {17,  21,  420, 442, 527,  528, 529,
                              532, 539, 545, 691, 1103, 1104};
  const std::array civIDs = {13, 23, 7, 17, 14, 31, 21, 6,  11,
                             12, 27, 1, 4,  18, 9,  8,  16, 24};
  const auto& burmese = aocDat->Civs[30]; // These are used for ID reference
  for (size_t c = 0; c < civIDs.size(); c++) {
    const auto civId = civIDs[c];
    auto& civ = aocDat->Civs[civId];

    std::map<short, short> replacedGraphics;
    std::map<short, short> replacedFlags;
    // buildings
    for (const auto buildingId : buildingIDs) {
      replaceGraphic(aocDat, &civ.Units[buildingId].StandingGraphic.first,
                     burmese.Units[buildingId].StandingGraphic.first, c,
                     replacedGraphics);
      short oldGraphicID = civ.Units[buildingId].Building.ConstructionGraphicID;
      if (oldGraphicID > 130 &&
          oldGraphicID !=
              4248) { // exclude standard construction graphics for all civs
        replaceGraphic(aocDat,
                       &civ.Units[buildingId].Building.ConstructionGraphicID,
                       burmese.Units[buildingId].Building.ConstructionGraphicID,
                       c, replacedGraphics);
      }
      auto burmeseGraphic = burmese.Units[buildingId].DamageGraphics.begin();
      for (auto& graphic : civ.Units[buildingId].DamageGraphics) {
        replaceGraphic(aocDat, &graphic.GraphicID, burmeseGraphic->GraphicID, c,
                       replacedGraphics);
        burmeseGraphic++;
      }
      oldGraphicID = civ.Units[buildingId].Creatable.GarrisonGraphic;
      if (oldGraphicID != -1) {
        if (replacedFlags[oldGraphicID] > 0)
          civ.Units[buildingId].Creatable.GarrisonGraphic =
              replacedFlags[oldGraphicID];
        else {
          genie::Graphic newFlag = aocDat->Graphics[oldGraphicID];
          newFlag.ID = aocDat->Graphics.size();
          aocDat->Graphics.push_back(newFlag);
          aocDat->GraphicPointers.push_back(1);
          replacedFlags[oldGraphicID] = newFlag.ID;
          civ.Units[buildingId].Creatable.GarrisonGraphic = newFlag.ID;
        }
      }
    }
    // units like ships
    for (const auto unitId : unitIDs) {
      replaceGraphic(aocDat, &civ.Units[unitId].StandingGraphic.first,
                     burmese.Units[unitId].StandingGraphic.first, c,
                     replacedGraphics);
      replaceGraphic(aocDat, &civ.Units[unitId].Moving.WalkingGraphic,
                     burmese.Units[unitId].Moving.WalkingGraphic, c,
                     replacedGraphics);
      replaceGraphic(aocDat, &civ.Units[unitId].Combat.AttackGraphic,
                     burmese.Units[unitId].Combat.AttackGraphic, c,
                     replacedGraphics);
    }

    listener->increaseProgress(1); // 34-51
  }

  // Separate Units into 4 major regions (Europe, Asian, Southern, American)
  const std::vector<std::vector<int32_t>> civGroups = {
      {3, 4, 11},
      {7, 23},
      {14, 19, 24}, // Central Eu, Orthodox, Mediterranean
      {5},
      {6, 18},
      {28, 29, 30, 31}, // Japanese, East Asian, SE Asian
      {8, 9, 10, 27},
      {20},
      {25, 26},     // Middle Eastern, Indian, African
      {15, 16, 21}, // American
      {17, 12},
      {22} // Steppe, Magyars
  };
  // std::map<int,int> slpIdConversion =
  // {{2683,0},{376,2},{4518,1},{2223,3},{3482,4},{3483,5},{4172,6},{4330,7},{889,10},{4612,16},{891,17},{4611,15},{3596,12},
  //						 {4610,14},{3594,11},{3595,13},{774,131},{779,134},{433,10},{768,130},{433,10},{771,132},{775,133},{3831,138},{3827,137}};
  // const std::array cgBuildingIDs = {12, 68, 70, 109, 598, 618, 619, 620}; //
  // There's no IA dark age building mod, but regular ones that get broken by
  // enabling this, so we won't do it.
  const std::array cgUnitIDs = {
      125, 134, 286, 4,   3,   5,   98,  6,   100, 7,   238, 24,  26,  37,
      113, 38,  111, 39,  34,  74,  152, 75,  154, 77,  180, 93,  140, 283,
      139, 329, 330, 495, 358, 501, 359, 502, 440, 441, 480, 448, 449, 473,
      500, 474, 631, 492, 496, 546, 547, 567, 568, 569, 570};
  for (size_t cg = 0; cg < civGroups.size(); cg++) {
    if (cg == 3) {
      /* We'll temporarily give the monk 10 frames so this value is the one used
       * for the new Asian and African/Middle Eastern civs.
       */
      aocDat->Graphics[998].FrameCount = 10;
    } else if (cg == 11) {
      aocDat->Graphics[998].FrameCount = 6; // Old Value again
    }
    const auto civGroup = civGroups[cg];
    short monkHealingGraphic;
    if (cg != 9) {
      int newSLP = 60000 + 10000 * cg + 776;
      genie::Graphic newGraphic = aocDat->Graphics[1597];
      monkHealingGraphic = aocDat->Graphics.size();
      newGraphic.ID = monkHealingGraphic;
      newGraphic.SLP = newSLP;
      aocDat->Graphics.push_back(newGraphic);
      aocDat->GraphicPointers.push_back(1);
      slpFiles[newSLP] = settings.hdPath / "resources" / "_common" / "drs" /
                         "graphics" / "776.slp";
    } else {
      monkHealingGraphic = 7340; // meso healing graphic
    }
    std::map<short, short> replacedGraphics;
    for (unsigned int civId = 0; civId < civGroup.size(); civId++) {
      auto& civ = aocDat->Civs[civGroup[civId]];

      /*
                  for(unsigned int b = 0; b <
      sizeof(cgBuildingIDs)/sizeof(short); b++) { replaceGraphic(aocDat,
      &civ.Units[cgBuildingIDs[b]].StandingGraphic.first, -1, cg,
      replacedGraphics, slpIdConversion);
          for(std::vector<genie::unit::DamageGraphic>::iterator it =
      civ.Units[cgBuildingIDs[b]].DamageGraphics.begin(); it !=
      civ.Units[cgBuildingIDs[b]].DamageGraphics.end(); it++) {
              replaceGraphic(aocDat, &(it->GraphicID), -1, cg, replacedGraphics,
      slpIdConversion);
                          }
      }*/
      // Units
      for (const auto unitId : cgUnitIDs) {
        replaceGraphic(aocDat, &civ.Units[unitId].StandingGraphic.first,
                       aocDat->Civs[0].Units[unitId].StandingGraphic.first, cg,
                       replacedGraphics, true);
        if (civ.Units[unitId].Moving.WalkingGraphic != -1) { // Not a Dead Unit
          replaceGraphic(aocDat, &civ.Units[unitId].Moving.WalkingGraphic,
                         aocDat->Civs[0].Units[unitId].Moving.WalkingGraphic,
                         cg, replacedGraphics, true);
          replaceGraphic(aocDat, &civ.Units[unitId].Combat.AttackGraphic,
                         aocDat->Civs[0].Units[unitId].Combat.AttackGraphic, cg,
                         replacedGraphics, true);
          replaceGraphic(aocDat, &civ.Units[unitId].DyingGraphic,
                         aocDat->Civs[0].Units[unitId].DyingGraphic, cg,
                         replacedGraphics, true);
        }
      }
      // special UP healing slp workaround
      for (unsigned int civId = 0; civId < civGroup.size(); civId++) {
        size_t code = 0x811E0000 + monkHealingGraphic;
        int ccode = (int)code;
        civ.Units[125].LanguageDLLHelp = ccode;

        if ((cg >= 3 && cg <= 5) || cg == 10) { // Shaman icons, "Eastern" civs
          civ.Units[125].IconID = 218;
          civ.Units[286].IconID = 218;
        } else if (cg >= 6 &&
                   cg <=
                       8) { // Imam Icons, Middle Eastern/southern civId groups
          civ.Units[125].IconID = 169;
          civ.Units[286].IconID = 169;
        }
      }
    }
    listener->increaseProgress(1); // 52-63
  }

  /*
   * Manual fixes after IA seperation
   */

  // Let the Mongol Mill have 40 frames instead of 8/10, which is close to the
  // dark age mill with 43 frames
  aocDat->Graphics[aocDat->Civs[12].Units[129].StandingGraphic.first]
      .FrameCount = 40;
  aocDat->Graphics[aocDat->Civs[12].Units[130].StandingGraphic.first]
      .FrameCount = 40;
  // aocDat->Graphics[aocDat->Graphics[aocDat->Civs[12].Units[129].StandingGraphic.first].Deltas[0].GraphicID].FrameCount
  // = 40;
  // aocDat->Graphics[aocDat->Graphics[aocDat->Civs[12].Units[130].StandingGraphic.first].Deltas[1].GraphicID].FrameCount
  // = 40;

  // Let the Berber Mill have 40 frames instead of 8/10, which is close to the
  // african mill with 38 frames
  aocDat->Graphics[aocDat->Civs[27].Units[129].StandingGraphic.first]
      .FrameCount = 40;
  aocDat->Graphics[aocDat->Civs[27].Units[130].StandingGraphic.first]
      .FrameCount = 40;
  aocDat
      ->Graphics
          [aocDat->Graphics[aocDat->Civs[27].Units[129].StandingGraphic.first]
               .Deltas[0]
               .GraphicID]
      .FrameCount = 40;
  aocDat
      ->Graphics
          [aocDat->Graphics[aocDat->Civs[27].Units[130].StandingGraphic.first]
               .Deltas[1]
               .GraphicID]
      .FrameCount = 40;

  // Fix the missionary converting frames while we're at it
  aocDat->Graphics[6616].FrameCount = 14;
  // Manual fix for missing portugese flags
  slpFiles[41178] = settings.hdPath / "resources" / "_common" / "drs" /
                    "graphics" / "4522.slp";
  slpFiles[41181] = settings.hdPath / "resources" / "_common" / "drs" /
                    "graphics" / "4523.slp";
}

void WKConverter::replaceGraphic(genie::DatFile* aocDat, short* graphicID,
                                 short compareID, short c,
                                 std::map<short, short>& replacedGraphics,
                                 bool civGroups) {
  if (replacedGraphics.count(*graphicID) != 0)
    *graphicID = replacedGraphics[*graphicID];
  else {
    short newGraphicID;
    std::vector<short> duplicatedGraphics;
    newGraphicID =
        duplicateGraphic(aocDat, replacedGraphics, duplicatedGraphics,
                         *graphicID, compareID, c, civGroups);
    *graphicID = newGraphicID;
  }
}

bool WKConverter::checkGraphics(genie::DatFile* aocDat, short graphicID,
                                std::vector<int> checkedGraphics) {
  /*
   * Tests if any SLP of a graphic, or a graphic Delta is in the right range
   * (18000-19000), which means they are a civ-dependant graphic (in this case
   * for SEA civs) instead of a shared graphic (Like flags, flames or stuff like
   * that)
   *
   * Returns: true if at least one SLP is in this range, false otherwise
   * Parameters:
   * aocDat: The dat file to be checked
   * graphicID: The ID of the graphic to be checked
   * checkedGraphics: The graphics checked so far, to avoid an endless recursion
   * in case of circular references
   */
  checkedGraphics.push_back(graphicID);
  genie::Graphic newGraphic = aocDat->Graphics[graphicID];
  if (aocDat->Graphics[graphicID].SLP < 18000 ||
      aocDat->Graphics[graphicID].SLP >= 19000) {
    for (auto& it : newGraphic.Deltas) {
      if (it.GraphicID != -1 &&
          std::find(checkedGraphics.begin(), checkedGraphics.end(),
                    it.GraphicID) == checkedGraphics.end() &&
          checkGraphics(aocDat, it.GraphicID, checkedGraphics))
        return true;
    }
    return false;
  }
  return true;
}

/**
 * @param aocDatFile The Dat File of WK to be patched.
 * @param replacedGraphics These graphics have already been replaced, we can
 * just return the id.
 * @param duplicatedGraphics These Graphics have already been duplicated with a
 * previous duplicateGraphic call. Passed on to avoid circular loops with
 * recursive calls for deltas.
 * @param graphicID The ID of the graphic to be duplicated.
 * @param compareID The ID of the same building of the burmese, to serve as a
 * comparison. If -1, it's one of the monk/dark age graphics to be duped, see
 * slpIdConversion parameter.
 * @param offset The offset of the civ/civ group for the new SLPs
 * (24000+offset*1000 and so on).
 * @param civGroup This is mostly unit graphics, which are only seperated into
 * civ groups, not per civs.
 */
short WKConverter::duplicateGraphic(genie::DatFile* aocDat,
                                    std::map<short, short>& replacedGraphics,
                                    std::vector<short> duplicatedGraphics,
                                    short graphicID, short compareID,
                                    short offset, bool civGroups) {

  if (replacedGraphics.count(graphicID) !=
      0u) // We've already replaced this, return the new graphics ID
    return replacedGraphics[graphicID];

  /*
   * Check if at least one SLP in this graphic or graphic deltas is in the right
   * range, else we don't need to do any duplication in which case we can just
   * return the current graphic ID as a result
   */
  if (civGroups && aocDat->Graphics[compareID].SLP >= 10000)
    throw std::runtime_error("Unit slp over 10k");
  if (!civGroups && (aocDat->Graphics[compareID].SLP < 18000 ||
                     aocDat->Graphics[compareID].SLP >= 19000)) {
    std::vector<int> checkedGraphics;
    if (!checkGraphics(aocDat, compareID, checkedGraphics))
      return graphicID;
  }

  genie::Graphic newGraphic = aocDat->Graphics[graphicID];
  int newSLP = civGroups ? 60000 : 24000;

  short newGraphicID = aocDat->Graphics.size();
  if (civGroups) { // Unit Graphics for the 4 civ groups
    newSLP += 10000 * offset + aocDat->Graphics[graphicID].SLP;
  } else if (aocDat->Graphics[compareID].SLP < 18000 ||
             aocDat->Graphics[compareID].SLP >= 19000) {
    if (slpFiles.count(aocDat->Graphics[graphicID].SLP) +
            aocSlpFiles.count(aocDat->Graphics[graphicID].SLP) !=
        0) {
      newSLP = aocDat->Graphics[graphicID].SLP;
    } else
      newSLP = -1; // seems to happen only for 15516 and 15536 but not cause
                   // harm in these cases
  } else
    newSLP += aocDat->Graphics[compareID].SLP - 18000 + 1000 * offset;

  replacedGraphics[newGraphic.ID] = newGraphicID;
  duplicatedGraphics.push_back(newGraphic.ID);
  newGraphic.ID = newGraphicID;
  if (newSLP > 0 && newSLP != aocDat->Graphics[graphicID].SLP &&
      newSLP != aocDat->Graphics[compareID].SLP) {
    // This is a graphic where we want a new SLP file (as opposed to one where
    // the a new SLP mayb just be needed for some deltas
    fs::path src = settings.hdPath / "resources" / "_common" / "drs" /
                   "gamedata_x2" / (std::to_string(newGraphic.SLP) + ".slp");
    if (cfs::exists(src))
      slpFiles[newSLP] = src;
    else {
      src = settings.hdPath / "resources" / "_common" / "drs" / "graphics" /
            (std::to_string(newGraphic.SLP) + ".slp");
      if (cfs::exists(src))
        slpFiles[newSLP] = src;
    }
    newGraphic.SLP = newSLP;
  }
  std::string civCode;
  if (civGroups) {
    switch (offset) {
    case 0:
      civCode = "AS";
      break;
    case 1:
      civCode = "SO";
      break;
    case 2:
      civCode = "AM";
      break;
    }
  } else {
    switch (offset) {
    case 0:
      civCode = "CE";
      break;
    case 1:
      civCode = "SL";
      break;
    case 2:
      civCode = "BY";
      break;
    case 3:
      civCode = "HU";
      break;
    case 4:
      civCode = "SP";
      break;
    case 5:
      civCode = "VI";
      break;
    case 6:
      civCode = "IC";
      break;
    case 7:
      civCode = "CH";
      break;
    case 8:
      civCode = "VK";
      break;
    case 9:
      civCode = "MO";
      break;
    case 10:
      civCode = "BE";
      break;
    case 11:
      civCode = "BR";
      break;
    case 12:
      civCode = "TE";
      break;
    case 13:
      civCode = "KO";
      break;
    case 14:
      civCode = "SA";
      break;
    case 15:
      civCode = "PE";
      break;
    case 16:
      civCode = "MY";
      break;
    case 17:
      civCode = "PO";
      break;
    }
  }

  char civLetter = newGraphic.Name.at(newGraphic.Name.length() - 1);
  if (civLetters.count(civLetter) != 0u) {
    if (newGraphic.FileName == newGraphic.Name) {
      newGraphic.FileName.replace(newGraphic.FileName.length() - 1, 1, civCode);
      newGraphic.Name = newGraphic.FileName;
    } else
      newGraphic.Name.replace(newGraphic.Name.length() - 1, 1, civCode);
  }
  aocDat->Graphics.push_back(newGraphic);
  aocDat->GraphicPointers.push_back(1);

  if (!civGroups &&
      aocDat->Graphics[compareID].Deltas.size() == newGraphic.Deltas.size()) {
    /* don't copy graphics files if the amount of deltas is different to the
     * comparison, this is usually with damage graphics and different amount of
     * Flames.
     */
    auto compIt = aocDat->Graphics[compareID].Deltas.begin();
    for (auto& it : newGraphic.Deltas) {
      if (it.GraphicID != -1 &&
          std::find(duplicatedGraphics.begin(), duplicatedGraphics.end(),
                    it.GraphicID) == duplicatedGraphics.end())
        it.GraphicID =
            duplicateGraphic(aocDat, replacedGraphics, duplicatedGraphics,
                             it.GraphicID, compIt->GraphicID, offset);
      compIt++;
    }
    aocDat->Graphics.at(newGraphicID) = newGraphic;
  }
  return newGraphicID;
}

void WKConverter::terrainSwap(genie::DatFile* hdDat, genie::DatFile* aocDat,
                              int tNew, int tOld, int slpID) {
  aocDat->TerrainBlock.Terrains[tNew] = hdDat->TerrainBlock.Terrains[tOld];
  aocDat->TerrainBlock.Terrains[tNew].SLP = slpID;
  if (tNew == 41) {
    for (size_t j = 0; j < aocDat->TerrainRestrictions.size(); j++) {
      aocDat->TerrainRestrictions[j].PassableBuildableDmgMultiplier.push_back(
          hdDat->TerrainRestrictions[j].PassableBuildableDmgMultiplier[tOld]);
      aocDat->TerrainRestrictions[j].TerrainPassGraphics.push_back(
          hdDat->TerrainRestrictions[j].TerrainPassGraphics[tOld]);
    }
  } else {
    for (size_t j = 0; j < aocDat->TerrainRestrictions.size(); j++) {
      aocDat->TerrainRestrictions[j].PassableBuildableDmgMultiplier[tNew] =
          hdDat->TerrainRestrictions[j].PassableBuildableDmgMultiplier[tOld];
      aocDat->TerrainRestrictions[j].TerrainPassGraphics[tNew] =
          hdDat->TerrainRestrictions[j].TerrainPassGraphics[tOld];
    }
  }
}

bool WKConverter::identifyHotkeyFile(const fs::path& directory,
                                     fs::path& maxHki,
                                     fs::path& lastEditedHki) {
  /*
   * Checks all .hki file in directory. The hotkey file with the highest number
   * is saved in maxHki, the hotkey file that was last edited in lastEditedHki.
   * These are the two most likely candidates for the hotkey file that's
   * actually in use.
   *
   * Returns true if a hotkey file was found
   */
  int maxHkiNumber = -1;
  fs::file_time_type lastHkiEdit;
  for (const auto& f : fs::directory_iterator(directory)) {
    if (f.path().extension() == ".hki") {
      std::string numberString = f.path().stem().string().substr(6);
      if (numberString.find_first_not_of("0123456789") != std::string::npos)
        continue;
      int hkiNumber = std::atoi(numberString.c_str());
      if (hkiNumber > maxHkiNumber) {
        maxHkiNumber = hkiNumber;
        maxHki = f.path();
      }
      auto lastModified = cfs::last_write_time(f.path());
      if (lastModified > lastHkiEdit) {
        lastHkiEdit = lastModified;
        lastEditedHki = f.path();
      }
    }
  }
  return maxHkiNumber != -1;
}

void WKConverter::copyHotkeyFile(const fs::path& maxHki,
                                 const fs::path& lastEditedHki, fs::path dst) {
  /*
   * See identifyHotkeyFile for extra info on maxHki, lastEditedHki
   * copies the .hki file maxHki into the directory dst. If maxHki already
   * exists, the last 2 versions are kept as backup If maxHki is not the same as
   * lastEditedHki, it also copies lastEditedHki into dst, with the suffix
   * "_alt". Players can remove the suffix and thus replace the .hki file in
   * case maxHki did not contain the correct set
   */
  fs::path bak1 = dst.parent_path() / (dst.stem().string() + "_bak1.hki");
  fs::path bak2 = dst.parent_path() / (dst.stem().string() + "_bak2.hki");
  if (cfs::exists(bak1))
    cfs::copy_file(bak1, bak2, fs::copy_options::update_existing);
  if (cfs::exists(dst))
    cfs::copy_file(dst, bak1, fs::copy_options::update_existing);
  cfs::copy_file(maxHki, dst, fs::copy_options::update_existing);
  if (!cfs::equivalent(lastEditedHki, maxHki)) {
    cfs::copy_file(lastEditedHki,
                   dst.parent_path() / (dst.stem().string() + "_alt.hki"),
                   fs::copy_options::update_existing);
  }
}

void WKConverter::removeWkHotkeys() {
  /*
   * This function removes hotkeys in the WK-specific folders, if a user wants
   * to use the same set of hotkeys for WK and AoC. With the copyHotkeyFile
   * function, 2 backups are kept
   */

  fs::path maxDstHki;
  fs::path lastDstHki;
  fs::path aocHkiPath = resourceDir / "player1.hki";

  if (settings.useExe) {
    if (identifyHotkeyFile(settings.upDir, maxDstHki, lastDstHki)) {
      copyHotkeyFile(aocHkiPath, aocHkiPath, maxDstHki);
      cfs::remove(maxDstHki);
      if (!cfs::equivalent(maxDstHki, lastDstHki)) {
        copyHotkeyFile(aocHkiPath, aocHkiPath, lastDstHki);
        cfs::remove(lastDstHki);
      }
    }
  } else if (identifyHotkeyFile(settings.vooblyDir, maxDstHki, lastDstHki)) {
    copyHotkeyFile(aocHkiPath, aocHkiPath, maxDstHki);
    cfs::remove(maxDstHki);
    if (!cfs::exists(lastDstHki)) {
      copyHotkeyFile(aocHkiPath, aocHkiPath, lastDstHki);
      cfs::remove(lastDstHki);
    }
  }
}

void WKConverter::hotkeySetup() {

  /*
   * A method to hopefully copy the correct hotkey files so there's no extra
   * setup needed. The User has 3 options: 1) Choose "Voobly/Aoc hotkeys". If a
   * voobly .hki file is in the correct place, it will just copy the .nfz
   * profile file so the same hki file is used for WK as well. If there's no
   * .hki file in place, it will copy a set of default aoc hotkeys 2) Choose "Hd
   * hotkeys for this mod only". Copies the HD hotkeys but puts them in the WK
   * folder so it will only be used for that. If it can't find a HD hotkey file,
   * uses default HD hotkeys instead 3) Same as 2) except it replaces the hki
   * file for regular aoc instead (so that it's used for both aoc and wk)
   *
   */

  fs::path maxDstHki;
  fs::path lastDstHki;
  fs::path maxSrcHki;
  fs::path lastSrcHki;

  fs::path nfz1Path = resourceDir / "player1.nfz";
  fs::path nfzPath = settings.outPath / "player.nfz";
  fs::path aocHkiPath = resourceDir / "player1.hki";
  fs::path nfzOutPath =
      settings.useExe ? settings.nfzUpOutPath : settings.nfzVooblyOutPath;

  std::error_code ec;

  if (cfs::exists(nfzPath)) // Copy the Aoc Profile
    cfs::copy_file(nfzPath, nfzOutPath, ec);
  else { // otherwise copy the default profile included
    cfs::copy_file(nfz1Path, nfzOutPath, ec);
    cfs::copy_file(nfz1Path, nfzPath, ec);
  }
  if (settings.useBoth) { // Profiles for UP
    cfs::copy_file(nfzPath, settings.nfzUpOutPath, ec);
  }
  // Copy hotkey files
  if (settings.hotkeyChoice == 1) { // Use AoC/Voobly Hotkeys
    removeWkHotkeys();
    if (!identifyHotkeyFile(settings.outPath, maxDstHki,
                            lastDstHki)) // In case there are no voobly hotkeys,
                                         // copy standard aoc hotkeys
      cfs::copy_file(aocHkiPath, settings.outPath / "player1.hki");
  } else {
    if (!identifyHotkeyFile(settings.hdPath / "Profiles", maxSrcHki,
                            lastSrcHki)) {
      maxSrcHki = resourceDir / "player1_age2hd.hki";
      lastSrcHki = maxSrcHki;
    }
  }
  if (settings.hotkeyChoice == 2) { // Use HD hotkeys only for WK
    if (!identifyHotkeyFile(installDir, maxDstHki, lastDstHki)) {
      if (!identifyHotkeyFile(settings.outPath, maxDstHki, lastDstHki)) {
        maxDstHki = installDir / "player1.hki";
        lastDstHki = maxDstHki;
      } else {
        maxDstHki = installDir / maxDstHki.filename();
        lastDstHki = installDir / lastDstHki.filename();
      }
    }
    copyHotkeyFile(maxSrcHki, lastSrcHki, maxDstHki);
    if (!cfs::equivalent(maxDstHki, lastDstHki))
      copyHotkeyFile(maxSrcHki, lastSrcHki, lastDstHki);
    if (settings.useBoth) {
      fs::path maxUpDstHki;
      fs::path lastUpDstHki;
      if (!identifyHotkeyFile(settings.upDir, maxUpDstHki, lastUpDstHki)) {
        maxUpDstHki = settings.upDir / maxDstHki.filename();
        lastUpDstHki = settings.upDir / lastDstHki.filename();
      }
      copyHotkeyFile(maxSrcHki, lastSrcHki, maxUpDstHki);
      if (!cfs::equivalent(maxUpDstHki, lastUpDstHki))
        copyHotkeyFile(maxSrcHki, lastSrcHki, lastUpDstHki);
    }
  }
  if (settings.hotkeyChoice == 3) {
    removeWkHotkeys();

    if (!identifyHotkeyFile(settings.outPath, maxDstHki, lastDstHki)) {
      maxDstHki = settings.outPath / "player1.hki";
      lastDstHki = maxDstHki;
    }
    copyHotkeyFile(maxSrcHki, lastSrcHki, maxDstHki);
    if (!cfs::equivalent(maxDstHki, lastDstHki))
      copyHotkeyFile(maxSrcHki, lastSrcHki, lastDstHki);
  }
}

/**
 * Adds monk graphics for one civ/civ group with a given prefix to the drs
 *
 * @param slpFiles map to add the monk SLP files to.
 * @param newMonkGraphicsDir directory path on disk to the monk graphic SLPs.
 * @param Number of the civ/civ group prefix
 */
static void addMonkGraphicsToCiv(std::map<int, fs::path>& slpFiles,
                                 const fs::path& newMonkGraphicsDir,
                                 int prefix) {
  for (const auto& current : fs::directory_iterator(newMonkGraphicsDir)) {
    auto src = current.path();
    std::string extension = src.extension().string();
    int id = prefix * 10000 + atoi(src.stem().string().c_str());
    slpFiles[id] = src;
  }
}

/**
 * Add graphics for the new monk units.
 *
 * @param slpFiles map to add the monk SLP files to.
 * @param newMonkGraphicsDir directory path on disk to the monk graphic SLPs.
 */
static void addNewMonkGraphics(std::map<int, fs::path>& slpFiles,
                               const fs::path& newMonkGraphicsDir) {
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "christian",
                       6); // Goths, Teutons, Vikings
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "christian",
                       7); // Byzantines, Slavs
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "shaman", 9); // Japanese
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "shaman",
                       10); // Chinese, Koreans
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "shaman", 11); // SEA
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "shaman",
                       16); // Huns, Mongols
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "imam",
                       12); // Turks, Persians, Saracens, Berbers
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "african", 13); // Indians
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "african",
                       14); // Ethiopians, Malians
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "legacy",
                       6); // Legacy SLPs for data mods of older patches
  addMonkGraphicsToCiv(slpFiles, newMonkGraphicsDir / "legacy",
                       7); // Legacy SLPs for data mods of older patches
}

/**
 * Add graphics for the new monk units.
 *
 * @param slpFiles map to add the monk SLP files to.
 * @param newMonkGraphicsDir directory path on disk to the monk graphic SLPs.
 */
static void addOldMonkGraphics(std::map<int, fs::path>& slpFiles,
                               const fs::path& newMonkGraphicsDir) {
  slpFiles[50730] = newMonkGraphicsDir / "icons.slp";
  slpFiles[60131] = newMonkGraphicsDir / "european_monk.slp";
  slpFiles[70131] = newMonkGraphicsDir / "european_monk.slp";
  for (auto i = 90774; i <= 160774; i += 10000) {
    slpFiles[i] = newMonkGraphicsDir / "european_monk.slp";
  }
}

/**
 * Checks if a symlink exists already. If not, it removes a possibly existing
 * directory/file and creates a symlink.
 *
 * @param oldPath The directory/file the symlink references.
 * @param newPath The directory/file the symlink should be created in.
 * @param type Soft for files, Dir for directories
 * @param copyOldContents possible contents of the newPath folder are copied to
 * oldPath before the symlink is created
 */
void WKConverter::refreshSymlink(const fs::path& oldPath,
                                 const fs::path& newPath, const LinkType type,
                                 bool copyOldContents) {
  if (cfs::is_symlink(newPath))
    return;
  if (copyOldContents && cfs::exists(newPath))
    cfs::copy(newPath, oldPath,
              cfs::copy_options::skip_existing | cfs::copy_options::recursive);
  cfs::remove_all(newPath);
  mklink(type, resolve_path(newPath), resolve_path(oldPath));
}

/**
 * Sets up symlinks between the different mod versions (offline/AK/FE), so as
 * much as possible is shared and as little space is needed as possible.
 *
 * @param oldDir The directory the symlink references.
 * @param newDir The directory the symlink should be created in.
 * @param dataMod If true, the symlink is for wk-based datamod.
 */

void WKConverter::symlinkSetup(const fs::path& oldDir, const fs::path& newDir,
                               bool dataMod) {
  bool vooblySrc =
      tolower(oldDir).find("\\voobly mods\\aoc") != std::string::npos;
  bool vooblyDst =
      tolower(newDir).find("\\voobly mods\\aoc") != std::string::npos;

  cfs::create_directory(newDir);
  bool datalink = vooblySrc == vooblyDst && !dataMod;

  if (datalink) {
    refreshSymlink(oldDir / "Data", newDir / "Data", LinkType::Dir);
  } else {
    cfs::create_directory(newDir / "Data");
    refreshSymlink(oldDir / "Data" / "gamedata_x1.drs",
                   newDir / "Data" / "gamedata_x1.drs", LinkType::Soft);
    refreshSymlink(oldDir / "Data" / "gamedata_x1_p1.drs",
                   newDir / "Data" / "gamedata_x1_p1.drs", LinkType::Soft);
  }
  for (const auto& current : fs::directory_iterator(oldDir)) {
    fs::path currentPath = current.path();
    std::string extension = currentPath.extension().string();
    if (extension == ".hki") {
      refreshSymlink(resolve_path(currentPath), newDir / currentPath.filename(),
                     LinkType::Soft);
    }
  }

  if (!dataMod) {
    refreshSymlink(oldDir / "language.ini", newDir / "language.ini",
                   LinkType::Soft);
    if (!vooblyDst) {
      refreshSymlink(oldDir / "SaveGame", newDir / "SaveGame", LinkType::Dir,
                     true);
    }
    if (!vooblySrc) {
      refreshSymlink(oldDir / "Data" / "language_x1_p1.dll",
                     newDir / "Data" / "language_x1_p1.dll", LinkType::Soft);
    }
  }

  refreshSymlink(oldDir / "Taunt", newDir / "Taunt", LinkType::Dir);
  refreshSymlink(oldDir / "Sound", newDir / "Sound", LinkType::Dir);
  refreshSymlink(oldDir / "History", newDir / "History", LinkType::Dir);
  refreshSymlink(oldDir / "Script.Rm", newDir / "Script.Rm", LinkType::Dir);
  refreshSymlink(oldDir / "Script.Ai", newDir / "Script.Ai", LinkType::Dir);
  refreshSymlink(oldDir / "Screenshots", newDir / "Screenshots", LinkType::Dir);
  refreshSymlink(oldDir / "Scenario", newDir / "Scenario", LinkType::Dir);
  refreshSymlink(oldDir / "player.nfz", newDir / "player.nfz", LinkType::Soft);

  if (!cfs::exists(
          newDir /
          "Taunt")) { // Symlink didn't work, we'll do a regular copy instead
    for (const auto& current : fs::directory_iterator(oldDir)) {
      const auto& currentPath = current.path();
      if (!cfs::is_directory(currentPath) ||
          tolower(currentPath.filename().string()) != "savegame") {
        cfs::copy(currentPath, newDir / currentPath.filename(),
                  fs::copy_options::recursive |
                      fs::copy_options::skip_existing);
      }
    }
  }
  cfs::create_directories(newDir / "Savegame" / "Multi");
  if (vooblyDst && !dataMod)
    cfs::copy_file(oldDir / "version.ini", newDir / "version.ini",
                   fs::copy_options::update_existing);
}

int WKConverter::retryInstall() {
  listener->log("Retry installation with removing folders first");
  fs::path tempFolder = "retryTemp";
  try {
    cfs::create_directories(tempFolder / "Scenario");
    cfs::create_directories(tempFolder / "SaveGame");
    cfs::create_directories(tempFolder / "Script.RM");
    cfs::copy(installDir / "SaveGame", tempFolder / "SaveGame",
              fs::copy_options::recursive | fs::copy_options::update_existing);
    cfs::copy(installDir / "Script.RM", tempFolder / "Script.RM",
              fs::copy_options::recursive | fs::copy_options::update_existing);
    cfs::copy(installDir / "Scenario", tempFolder / "Scenario",
              fs::copy_options::recursive | fs::copy_options::update_existing);
    cfs::copy_file(installDir / "player.nfz", tempFolder / "player.nfz",
                   fs::copy_options::update_existing);
    if (cfs::exists(installDir / "player1.hki"))
      cfs::copy_file(installDir / "player1.hki", tempFolder / "player1.hki",
                     fs::copy_options::update_existing);
  } catch (std::exception const& e) {
    listener->error(e);
    listener->log(e.what());
    return -2;
  }

  cfs::remove_all(installDir);
  cfs::remove_all(installDir.parent_path() / "WololoKingdoms AK");
  cfs::remove_all(installDir.parent_path() / "WololoKingdoms FE");

  auto result = run();
  cfs::copy(tempFolder / "SaveGame", installDir / "SaveGame",
            fs::copy_options::recursive | fs::copy_options::skip_existing);
  cfs::copy(tempFolder / "Script.RM", installDir / "Script.RM",
            fs::copy_options::recursive | fs::copy_options::skip_existing);
  cfs::copy(tempFolder / "Scenario", installDir / "Scenario",
            fs::copy_options::recursive | fs::copy_options::skip_existing);
  std::error_code ec;
  cfs::copy_file(tempFolder / "player.nfz", installDir / "player.nfz", ec);
  if (cfs::exists(tempFolder / "player1.hki"))
    cfs::copy_file(tempFolder / "player1.hki", installDir / "player1.hki", ec);
  cfs::remove_all(tempFolder);

  return result;
}

void WKConverter::setupFolders(fs::path xmlOutPathUP) {

  fs::path languageIniPath = settings.vooblyDir / "language.ini";
  fs::path versionIniPath = settings.vooblyDir / "version.ini";

  listener->log("Check for symlink");
  if (cfs::is_symlink(installDir / "Taunt")) {
    listener->log("Removing all but SaveGame and profile");
    fs::path tempFolder = "temp";
    cfs::create_directories(tempFolder / "Scenario");
    cfs::create_directories(tempFolder / "SaveGame");
    cfs::create_directories(tempFolder / "Script.RM");
    cfs::copy(installDir / "SaveGame", tempFolder / "SaveGame",
              fs::copy_options::recursive | fs::copy_options::update_existing);
    cfs::copy(installDir / "Script.RM", tempFolder / "Script.RM",
              fs::copy_options::recursive | fs::copy_options::update_existing);
    cfs::copy(installDir / "Scenario", tempFolder / "Scenario",
              fs::copy_options::recursive | fs::copy_options::update_existing);
    cfs::copy_file(installDir / "player.nfz", tempFolder / "player.nfz",
                   fs::copy_options::update_existing);
    if (cfs::exists(installDir / "player1.hki"))
      cfs::copy_file(installDir / "player1.hki", tempFolder / "player1.hki",
                     fs::copy_options::update_existing);
    cfs::remove_all(installDir);
    cfs::create_directories(installDir / "SaveGame");
    cfs::create_directories(installDir / "SaveGame");
    cfs::create_directories(installDir / "Script.RM");
    cfs::copy(tempFolder / "SaveGame", installDir / "SaveGame",
              fs::copy_options::recursive | fs::copy_options::skip_existing);
    cfs::copy(tempFolder / "Script.RM", installDir / "Script.RM",
              fs::copy_options::recursive | fs::copy_options::skip_existing);
    cfs::copy(tempFolder / "Scenario", installDir / "Scenario",
              fs::copy_options::recursive | fs::copy_options::skip_existing);
    std::error_code ec;
    cfs::copy_file(tempFolder / "player.nfz", installDir / "player.nfz", ec);
    if (cfs::exists(tempFolder / "player1.hki"))
      cfs::copy_file(tempFolder / "player1.hki", installDir / "player1.hki",
                     ec);
    cfs::remove_all(tempFolder);
  }

  listener->log("Removing base folders");
  cfs::remove_all(installDir / "Data");
  /*
  cfs::remove_all(installDir/"Script.Ai"/"Brutal2");
  cfs::remove(installDir/"Script.Ai"/"BruteForce3.1.ai");
  cfs::remove(installDir/"Script.Ai"/"BruteForce3.1.per");
  */

  listener->log("Creating base folders");
  cfs::create_directories(installDir / "SaveGame" / "Multi");
  cfs::create_directories(installDir / "Sound" / "stream");
  cfs::create_directory(installDir / "Data");
  cfs::create_directory(installDir / "Taunt");
  cfs::create_directory(installDir / "History");
  cfs::create_directory(installDir / "Screenshots");
  cfs::create_directory(installDir / "Scenario");

  if (!settings.useExe) {
    cfs::remove(settings.vooblyDir / "age2_x1.xml");
    cfs::remove(versionIniPath);
    listener->log("Removing language.ini");
    cfs::remove(languageIniPath);
  } else {
    listener->log("Removing UP base folders");
    cfs::remove(xmlOutPathUP);
    cfs::remove(settings.upDir / "Data" / "empires2_x1_p1.dat");
    cfs::remove(settings.upDir / "Data" / "gamedata_x1.drs");
    cfs::remove(settings.upDir / "Data" / "gamedata_x1_p1.drs");
    /*
    cfs::remove_all(settings.upDir/"Script.Ai"/"Brutal2");
    cfs::remove(settings.upDir/"Script.Ai"/"BruteForce3.1.ai");
    cfs::remove(settings.upDir/"Script.Ai"/"BruteForce3.1.per");
    */
    cfs::create_directories(settings.upDir / "Data");
    listener->log("Removing language.ini");
    cfs::remove(settings.upDir / "language.ini");
  }
}

int WKConverter::run() {
  listener->setInfo("working");

  if (settings.dlcLevel == 0) { // This should never happen
    listener->setInfo("noSteam");
    listener->error("You shouldn't be here! $noSteam");
    return -1;
  }

  int ret = 0;
  slpFiles.clear();
  wavFiles.clear();
  newTerrainFiles.clear();

  // Installer Resources
  fs::path newMonkGraphicsDir = resourceDir / "graphics" / "monks";
  fs::path newTerrainGraphicsDir = resourceDir / "graphics" / "terrains";
  fs::path newArchitectureGraphicsDir =
      resourceDir / "graphics" / "architecture";
  fs::path scenarioInputDir = resourceDir / "Scenario";
  fs::path slpCompatDir = resourceDir / "old dat slp compatibility";
  fs::path wallsInputDir = resourceDir / "visual-mods" / "short-walls";
  fs::path aiInputPath = resourceDir / "Script.Ai";
  fs::path upSetupAoCSource = resourceDir / "SetupAoc.exe";
  fs::path aocLanguageIniModDll = resourceDir / "language_x1_p1.dll";
  fs::path patchFolder;

  // HD Resources
  fs::path historyInputPath =
      settings.language == "zht"
          ? (resourceDir / "locales" / "traditional chinese game strings" /
             "history")
          : (settings.hdPath / "resources" / settings.language / "strings" /
             "history");
  fs::path soundsInputPath =
      settings.hdPath / "resources" / "_common" / "sound";
  fs::path tauntInputPath =
      settings.hdPath / "resources" / "en" / "sound" / "taunt";
  fs::path scenarioSoundsInputPath =
      settings.hdPath / "resources" / "en" / "sound" / "scenario";
  fs::path assetsPath =
      settings.hdPath / "resources" / "_common" / "drs" / "gamedata_x2";
  fs::path aocAssetsPath =
      settings.hdPath / "resources" / "_common" / "drs" / "graphics";
  fs::path aocDatPath =
      settings.hdPath / "resources" / "_common" / "dat" / "empires2_x1_p1.dat";
  fs::path hdDatPath =
      settings.hdPath / "resources" / "_common" / "dat" / "empires2_x2_p1.dat";

  installDir = settings.useExe ? settings.upDir : settings.vooblyDir;

  // Voobly Target
  fs::path versionIniPath = settings.vooblyDir / "version.ini";
  fs::path xmlOutPath = settings.vooblyDir / "age2_x1.xml";
  settings.nfzVooblyOutPath = settings.vooblyDir / "Player.nfz";

  // Offline Target
  fs::path xmlOutPathUP;
  std::string upModdedExeName;
  fs::path upSetupAoCPath = settings.outPath / "SetupAoc.exe";
  settings.nfzUpOutPath = settings.upDir / "Player.nfz";

  // Any Target
  fs::path languageIniPath = installDir / "language.ini";
  fs::path soundsOutputPath = installDir / "Sound";
  fs::path scenarioSoundsOutputPath = installDir / "Sound" / "Scenario";
  fs::path historyOutputPath = installDir / "History";
  fs::path tauntOutputPath = installDir / "Taunt";
  fs::path drsOutPath = installDir / "Data" / "gamedata_x1_p1.drs";
  fs::path outputDatPath = installDir / "Data" / "empires2_x1_p1.dat";

  switch (settings.dlcLevel) {
  case 1:
    xmlOutPathUP = settings.outPath / "Games" / "WKFE.xml";
    upModdedExeName = "WKFE";
    break;
  case 2:
    xmlOutPathUP = settings.outPath / "Games" / "WKAK.xml";
    upModdedExeName = "WKAK";
    break;
  case 3:
    xmlOutPathUP = settings.outPath / "Games" / "WK.xml";
    upModdedExeName = "WK";
    break;
  }

  listener->log(secondAttempt
                    ? "Second Attempt\n"
                    : "NewRun\n"
                      "HD Path:" +
                          settings.hdPath.string() +
                          "\nAoC Path:" + installDir.string() +
                          "\nPatch mode: " + std::to_string(settings.patch) +
                          "\nDLC level: " + std::to_string(settings.dlcLevel));

  std::string line;

  listener->setProgress(1); // 1

  if (settings.patch < 0) {
    setupFolders(xmlOutPathUP);
  } else {
    cfs::create_directories(outputDatPath.parent_path());
    patchFolder = resourceDir / "patches" /
                  std::get<0>(settings.dataModList[settings.patch]);
    hdDatPath = patchFolder / "empires2_x1_p1.dat";
    upModdedExeName = std::get<1>(settings.dataModList[settings.patch]);
  }

  createLanguageFile(languageIniPath, patchFolder);
  if (settings.useExe || settings.useBoth) {
    cfs::create_directories(settings.upDir / "Data");
    cfs::copy_file(aocLanguageIniModDll,
                   settings.upDir / "Data" / "language_x1_p1.dll",
                   fs::copy_options::update_existing);
  }

  listener->increaseProgress(1); // 6

  if (settings.patch < 0) {
    listener->log("index DRS files");
    indexDrsFiles(
        assetsPath); // Slp/wav files to be written into gamedata_x1_p1.drs
    indexDrsFiles(aocAssetsPath,
                  false); // Aoc slp files, just needed for comparison purposes

    listener->log("Visual Mod Stuff");
    listener->setInfo("working$\n$workingMods");
    listener->increaseProgress(1); // 7
    if (settings.useGrid) {
      listener->increaseProgress(1); // 8
      listener->increaseProgress(2); // 10
    } else {
      indexDrsFiles(newTerrainGraphicsDir, true, true);
      listener->increaseProgress(3); // 10
    }
    listener->increaseProgress(1); // 11

    for (const auto& [directory, index_type] : settings.drsModDirectories) {
      indexDrsFiles(directory, index_type);
    }

    listener->setInfo("working$\n$workingFiles");

    listener->log("History Files");
    copyHistoryFiles(historyInputPath, historyOutputPath);

    listener->log("Civ Intro Sounds");
    copyCivIntroSounds(soundsInputPath / "civ", soundsOutputPath / "stream");

    listener->increaseProgress(1); // 12
    listener->log("Create Music Playlist");
    createMusicPlaylist(soundsInputPath / "music",
                        soundsOutputPath / "music.m3u");

    listener->increaseProgress(1); // 13
    listener->log("Copy Taunts");
    cfs::copy(tauntInputPath, tauntOutputPath,
              fs::copy_options::recursive | fs::copy_options::skip_existing);

    listener->log("Copy Scenario Sounds");
    cfs::copy(scenarioSoundsInputPath, scenarioSoundsOutputPath,
              fs::copy_options::recursive | fs::copy_options::skip_existing);

    listener->increaseProgress(1); // 14
    listener->log("Write expansion XML");
    if (settings.useExe || settings.useBoth) {
      std::ofstream xml_output(xmlOutPathUP);
      write_wk_xml(xml_output, settings.dlcLevel);
    }
    if (settings.useVoobly || settings.useBoth) {
      std::ofstream xml_output(xmlOutPath);
      write_wk_xml(xml_output, settings.dlcLevel);
    }

    fs::path installMapDir = installDir / "Script.Rm";
    listener->log("Copy Voobly Map folder");
    if (cfs::exists(settings.outPath / "Random")) {
      cfs::copy(settings.outPath / "Random", installMapDir,
                fs::copy_options::recursive | fs::copy_options::skip_existing);
    } else {
      cfs::create_directory(installMapDir);
    }
    listener->increaseProgress(1); // 15

    if (settings.copyCustomMaps) {
      listener->log("Copy HD Maps");
      copyHDMaps(settings.hdPath / "resources" / "_common" /
                     "random-map-scripts",
                 installMapDir);
    } else {
      listener->increaseProgress(3); // 18
    }
    listener->increaseProgress(1); // 19

    listener->log("Copy Special Maps");
    if (settings.copyMaps) {
      copyHDMaps(resourceDir / "Script.Rm", installMapDir, true);
    } else {
      listener->increaseProgress(3);
    }
    listener->increaseProgress(1); // 23
    cfs::copy(scenarioInputDir, installDir / "Scenario",
              fs::copy_options::recursive | fs::copy_options::update_existing);

    listener->log("Copying AI");
    cfs::copy(aiInputPath, installDir / "Script.Ai",
              fs::copy_options::recursive | fs::copy_options::update_existing);

    listener->increaseProgress(1); // 24
    listener->log("Hotkey Setup");
    if (settings.hotkeyChoice != 0)
      hotkeySetup();

    listener->increaseProgress(1); // 25

    listener->log("Opening dats");
    listener->setInfo("working$\n$workingAoc");

    genie::DatFile aocDat;
    aocDat.setGameVersion(genie::GameVersion::GV_TC);
    aocDat.load(aocDatPath.string());
    listener->increaseProgress(3); // 28

    listener->setInfo("working$\n$workingInterface");

    listener->log("HUD Hack");
    shiftHudIndices(slpFiles, assetsPath);
    indexDrsFiles(resourceDir / "graphics" / "huds");
    listener->increaseProgress(1); // 29

    // `hdDat` is destroyed by `transferHdDatElements` so make this scope small
    // to prevent accidental use afterward
    {
      listener->setInfo("working$\n$workingHD");
      genie::DatFile hdDat;
      hdDat.setGameVersion(genie::GameVersion::GV_Cysion);
      hdDat.load(hdDatPath.string());
      listener->increaseProgress(3); // 32

      listener->setInfo("working$\n$workingDat");
      listener->log("Transfer HD Dat elements");
      transferHdDatElements(&hdDat, &aocDat);
      listener->increaseProgress(1); // 33
    }

    listener->log("Patch Architectures");
    /*
     * As usual, we have to fix some mediterranean stuff first where builidings
     * that shouldn't share the same garrison flag graphics.
     */

    const std::array buildingIDs = {47, 51, 116, 137, 234, 235, 236};
    for (const auto buildingId : buildingIDs) {
      short oldGraphicID =
          aocDat.Civs[19].Units[buildingId].Creatable.GarrisonGraphic;
      genie::Graphic newFlag = aocDat.Graphics[oldGraphicID];
      newFlag.ID = aocDat.Graphics.size();
      aocDat.Graphics.push_back(newFlag);
      aocDat.GraphicPointers.push_back(1);
      aocDat.Civs[19].Units[buildingId].Creatable.GarrisonGraphic = newFlag.ID;
      aocDat.Civs[24].Units[buildingId].Creatable.GarrisonGraphic = newFlag.ID;
    }

    adjustArchitectureFlags(&aocDat, resourceDir / "Flags.txt");

    patchArchitectures(&aocDat);

    if (settings.fixFlags)
      adjustArchitectureFlags(&aocDat, resourceDir / "WKFlags.txt");

    if (settings.useShortWalls) // This needs to be AFTER patchArchitectures
      copyWallFiles(wallsInputDir);

    listener->increaseProgress(1); // 64
    // Add monk graphics
    if (settings.useMonks) {
      addNewMonkGraphics(slpFiles, newMonkGraphicsDir);
    } else {
      addOldMonkGraphics(slpFiles, newMonkGraphicsDir);
    }
    listener->increaseProgress(1); // 65

    indexDrsFiles(newArchitectureGraphicsDir);
    listener->increaseProgress(1); // 66

    listener->log("Opening DRS");
    std::ofstream drsOut(drsOutPath, std::ios::binary);
    listener->log("Make DRS " + drsOutPath.string());
    makeDrs(drsOut);
    listener->increaseProgress(1); // 75

    listener->log("Make random map scripts DRS");
    std::ofstream gameDataX1(installDir / "Data" / "gamedata_x1.drs",
                             std::ios::binary);
    makeRandomMapScriptsDrs(gameDataX1, resourceDir / "gamedata_x1");

    listener->increaseProgress(1); // 76

    /*
     * Read what the current patch number and what the expected hashes
     * (with/without flag adjustment) are
     */

    std::ifstream versionFile(resourceDir / "version.txt");
    std::string patchNumber;
    std::getline(versionFile, patchNumber);
    std::string dataVersion;
    std::getline(versionFile, dataVersion);

    std::string hash1;
    std::string hash2;
    std::getline(versionFile, hash1);
    std::getline(versionFile, hash2);
    versionFile.close();

    const std::vector<wololo::DatPatch> patchTab = {
        wololo::berbersUTFix,
        wololo::demoShipFix,
        wololo::vietFix,
        wololo::malayFix,
        wololo::ethiopiansFreePikeUpgradeFix,
        wololo::hotkeysFix,
        wololo::maliansFreeMiningUpgradeFix,
        wololo::portugueseFix,
        wololo::disableNonWorkingUnits,
        wololo::burmeseFix,
        wololo::siegeTowerFix,
        wololo::khmerFix,
        wololo::trickleBuildingFix,
        wololo::smallFixes,
        wololo::cuttingFix,
        wololo::ai900UnitIdFix};

    listener->setInfo("working$\n$workingPatches");

    listener->log("DAT Patches");
    for (auto& patch : patchTab) {
      patch.patch(&aocDat);
      listener->setInfo(std::string("working$\n$") + patch.name);
      listener->increaseProgress(1); // 77-93
    }

    for (auto& civ : aocDat.Civs) {
      civ.Resources[198] = std::stoi(
          dataVersion); // Mod version: WK=1, last 3 digits are patch number
    }

    listener->log("Save DAT");
    aocDat.saveAs(outputDatPath.string().c_str());

    if (!settings.useExe) {
      /*
       * Generate version.ini based on the installer and the hash of the dat.
       */
      listener->log("Create Hash");
      auto fileStream = std::fstream(outputDatPath.string(),
                                     std::ios_base::in | std::ios_base::binary);
      std::string fileData = concat_stream(fileStream);

      std::string hash = MD5(fileData).b64digest().substr(0, 10);
      std::ofstream versionOut(versionIniPath);
      if (hash != hash1 && hash != hash2) {
        listener->createDialog("dialogBeta");

        versionOut << hash;
      } else {
        versionOut << patchNumber;
      }
      versionOut.close();
    }

    if (settings.useBoth) {
      listener->log("Offline installation symlink");
      symlinkSetup(settings.vooblyDir, settings.upDir);
    }

  } else { // If we use a balance mod or old patch, just copy the supplied dat
           // fil
    listener->log("Copy DAT file");
    cfs::remove(outputDatPath);
    genie::DatFile dat;
    dat.setGameVersion(genie::GameVersion::GV_TC);
    dat.load(hdDatPath.string());
    if (settings.fixFlags)
      adjustArchitectureFlags(&dat, resourceDir / "WKFlags.txt");
    dat.saveAs(outputDatPath.string().c_str());
    listener->setProgress(20);
    std::string patchNumber = std::get<2>(settings.dataModList[settings.patch]);
    std::ofstream versionOut(versionIniPath);
    versionOut << patchNumber;
    versionOut.close();
  }

  listener->increaseProgress(1); // 94

  /*
   * If a user has access to more than just FE, also generate those versions
   * For an old patch, we'll just use the highest one
   * For a data mod, not sure if we should generate all versions or the highest
   * one
   */

  if (settings.patch >= 0) {
    listener->log("Patch setup");
    std::string dlcLevelName =
        baseModName +
        (settings.dlcLevel == 3 ? "" : settings.dlcLevel == 2 ? " AK" : " FE");
    std::stringstream sstream;
    write_wk_xml(sstream, settings.dlcLevel);
    auto str = sstream.str();
    replace_all(str, dlcLevelName, settings.modName);
    if (settings.useBoth || settings.useVoobly) {
      std::ofstream outstream(settings.vooblyDir / "age2_x1.xml");
      outstream << str;
      outstream.close();
      symlinkSetup(settings.vooblyDir.parent_path() / dlcLevelName,
                   settings.vooblyDir, true);
      if ((std::get<3>(settings.dataModList[settings.patch]) & 4) != 0) {
        /*
         * Older patches need some SLP files that aren't in the drs used for the
         * main WK mod So we'll remove the symlink, copy the drs instead and
         * insert the missing SLP files
         */
        indexDrsFiles(slpCompatDir);
        fs::remove(settings.vooblyDir / "data" / "gamedata_x1_p1.drs");
        std::ofstream newDrs(settings.vooblyDir / "data" / "gamedata_x1_p1.drs",
                             std::ios::binary);
        editDrs(settings.vooblyDir.parent_path() / dlcLevelName / "data" / "gamedata_x1_p1.drs", newDrs);
      }
    }
    if (settings.useBoth || settings.useExe) {
      std::ofstream outstream(settings.upDir.parent_path() /
                              (upModdedExeName + ".xml"));
      outstream << str;
      outstream.close();
      fs::copy_file(settings.vooblyDir / "language.ini",
                    settings.upDir / "language.ini",
                    fs::copy_options::update_existing);
      symlinkSetup(settings.upDir.parent_path() / dlcLevelName, settings.upDir,
                   true);
      if ((std::get<3>(settings.dataModList[settings.patch]) & 4) != 0) {
        /*
         * Older patches need some SLP files that aren't in the drs used for the
         * main WK mod So we'll remove the symlink, copy the drs instead and
         * insert the missing SLP files
         *
         * Possible improvement: If useBoth is selected, the drs file is
         * symlinked from the voobly folder to save space The rest of the files
         * are symlinked from the base UP folder. Two different sources for the
         * symlinks is kind of ugly, but for now it works Linking everything
         * from the Voobly folder instead would be nicer
         */
        if (settings.useBoth) {
          refreshSymlink(settings.vooblyDir / "data" / "gamedata_x1_p1.drs",
                         settings.upDir / "data" / "gamedata_x1_p1.drs",
                         LinkType::Soft);
        } else {
          indexDrsFiles(slpCompatDir);
          fs::remove(settings.upDir / "data" / "gamedata_x1_p1.drs");
          std::ofstream newDrs(settings.upDir / "data" / "gamedata_x1_p1.drs",
                               std::ios::binary);
          editDrs(settings.upDir.parent_path() / dlcLevelName / "data" / "gamedata_x1_p1.drs", newDrs);
        }
      }
    }
  } else if (settings.restrictedCivMods) {
    if (settings.dlcLevel > 1) {
      listener->log("FE Setup");
      fs::path vooblyModDir =
          settings.vooblyDir.parent_path() / (baseModName + " FE");
      if (settings.useBoth || settings.useVoobly) {
        std::ofstream outstream(vooblyModDir / "age2_x1.xml");
        write_wk_xml(outstream, 1);
        symlinkSetup(settings.vooblyDir, vooblyModDir);
      }
    }
    if (settings.dlcLevel > 2) {
      listener->log("AK Setup");
      fs::path vooblyModDir =
          settings.vooblyDir.parent_path() / (baseModName + " AK");
      if (settings.useBoth || settings.useVoobly) {
        std::ofstream outstream(vooblyModDir / "age2_x1.xml");
        write_wk_xml(outstream, 2);
        symlinkSetup(settings.vooblyDir, vooblyModDir);
      }
    }
  }

  /*
   * Copy the data folder from the Voobly folder and
   * create the offline exe
   */
  if (settings.useBoth) {
    cfs::copy_file(settings.vooblyDir / "Data" / "empires2_x1_p1.dat",
                   settings.upDir / "Data" / "empires2_x1_p1.dat",
                   fs::copy_options::update_existing);
  }
  if (settings.useVoobly) {
    listener->createDialog("dialogDone");
  } else {
    listener->log("Create Offline Exe");
    listener->setInfo("working$\n$workingUP");
    listener->increaseProgress(1); // 95
    cfs::copy_file(upSetupAoCSource, upSetupAoCPath,
                   fs::copy_options::update_existing);

    listener->increaseProgress(1); // 96

    std::vector<std::string> flags = {"-g:" + upModdedExeName};
    listener->installUserPatch(upSetupAoCPath, flags);

    std::string newExeName;
    if (settings.patch >= 0 &&
        !(newExeName = std::get<4>(settings.dataModList[settings.patch]))
             .empty()) {
      if (cfs::exists(settings.outPath / "age2_x1" / (newExeName + ".exe"))) {
        cfs::rename(settings.outPath / "age2_x1" / (newExeName + ".exe"),
                    settings.outPath / "age2_x1" / (newExeName + ".exe.bak"));
      }
      cfs::rename(settings.outPath / "age2_x1" / (upModdedExeName + ".exe"),
                  settings.outPath / "age2_x1" / (newExeName + ".exe"));
      upModdedExeName = newExeName;
    }

    listener->increaseProgress(1); // 97
    std::string info = settings.useBoth ? "dialogBoth" : "dialogExe";
    listener->createDialog(info, "<exe>", upModdedExeName);
  }
  listener->setInfo("workingDone");

  if (settings.patch < 0 &&
      cfs::equivalent(settings.outPath, settings.hdPath)) {

    listener->log("Fix Compat Patch");
    /*
     * Several small fixes for the compatibility patch. This only needs to be
     * run once An update to the compatibility patch would make this unnecessary
     * most likely.
     */

    cfs::remove_all(settings.outPath / "compatslp");

    cfs::create_directory(settings.outPath / "data" / "Load");
    if (settings.useExe) { // this causes a crash with UP 1.5 otherwise
      listener->setInfo("workingDone");

      if (cfs::file_size(settings.outPath / "data" / "blendomatic.dat") <
          400000) {
        cfs::rename(settings.outPath / "data" / "blendomatic.dat",
                    settings.outPath / "data" / "blendomatic.dat.bak");
        cfs::rename(settings.outPath / "data" / "blendomatic_x1.dat",
                    settings.outPath / "data" / "blendomatic.dat");
      }
      listener->increaseProgress(1); // 98
    }
  }

  listener->setProgress(100);

  listener->finished();
  return ret;
}
