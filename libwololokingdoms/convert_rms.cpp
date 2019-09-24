#include "caseless.h"
#include "zr_map_creator.h"
#include "string_helpers.h"
#include <array>
#include <fs.h>
#include <map>
#include <regex>
#include <string>
#include <vector>

static fs::path resolve_path(const fs::path& input) {
#ifdef _WIN32
  return input;
#else
  return cfs::resolve(input);
#endif
}

enum TerrainGroup {
  None,
  WaterTerrain,
  FixedTerrain,
  LandTerrain,
  ForestTerrain,
  UnbuildableTerrain
};

enum TerrainType {
  Grass = 0,
  WaterShallow = 1,
  Beach = 2,
  Desert3 = 3,
  WalkableShallows = 4,
  Leaves = 5,
  Desert = 6,
  Farm1 = 7,
  Farm2 = 8,
  Grass3 = 9,
  Forest = 10,
  Desert2 = 11,
  Grass2 = 12,
  PalmDesert = 13,
  Sand = 14,
  OldWater = 15,
  ImpassableCliffGrass = 16,
  Jungle = 17,
  Bamboo = 18,
  Pine = 19,
  OakForest = 20,
  SnowForest = 21,
  WaterDeep = 22,
  WaterNormal = 23,
  Road = 24,
  Road2 = 25,
  Ice = 26,
  Foundation = 27,
  WaterBridge = 28,
  FarmCnst1 = 29,
  FarmCnst2 = 30,
  FarmCnst3 = 31,
  Snow = 32,
  SnowDirt = 33,
  SnowGrass = 34,
  Ice2 = 35,
  SnowFoundation = 36,
  IceBeach = 37,
  SnowRoad = 38,
  FungusRoad = 39,
  UnbuildableRock = 40,
  DLC2Savannah = 41,
  DLC2Dirt4 = 42,
  DLC2DesertRoad = 43,
  DLC2Moorland = 44,
  DLC2CrackedDesert = 45,
  DLC2QuickSand = 46,
  DLC2ForgottenBlack = 47,
  DLC2DragonTreeForest = 48,
  DLC2BaobabForest = 49,
  DLC2AcaciaForest = 50,
  DLC3Beach2 = 51,
  DLC3Beach3 = 52,
  DLC3Beach4 = 53,
  DLC3WalkableShallowsMangrove = 54,
  DLC3MangroveForest = 55,
  DLC3Rainforest = 56,
  DLC3WaterDeepOcean = 57,
  DLC3WaterAzure = 58,
  DLC3WalkableShallowsAzure = 59,
  DLC3GrassJungle = 60,
  DLC3RoadJungle = 61,
  DLC3LeavesJungle = 62,
  DLC3RiceFarm1 = 63,
  DLC3RiceFarm2 = 64,
  DLC3RiceFarmCnst1 = 65,
  DLC3RiceFarmCnst2 = 66,
  DLC3RiceFarmCnst3 = 67,
  Unused68 = 68,
  Unused69 = 69,
  ModdableLand0 = 70,
  ModdableLand1 = 71,
  ModdableLand2 = 72,
  ModdableLand3 = 73,
  ModdableLand4 = 74,
  ModdableLand5 = 75,
  ModdableLand6 = 76,
  ModdableLand7 = 77,
  ModdableLand8 = 78,
  ModdableLand9 = 79,
  ModdableLand10 = 0x50,
  ModdableLand11 = 0x51,
  ModdableLand12 = 0x52,
  ModdableLand13 = 0x53,
  ModdableLand14 = 0x54,
  ModdableLand15 = 0x55,
  ModdableLand16 = 0x56,
  ModdableLand17 = 0x57,
  ModdableLand18 = 0x58,
  ModdableLand19 = 0x59,
  ModdableShore0 = 0x5A,
  ModdableShore1 = 0x5B,
  ModdableShore2 = 0x5C,
  ModdableShore3 = 0x5D,
  ModdableShore4 = 0x5E,
  ModdableWater0 = 0x5F,
  ModdableWater1 = 0x60,
  ModdableWater2 = 0x61,
  ModdableWater3 = 0x62,
  ModdableWater4 = 0x63,
};

struct MapConvertData {
  std::string slp_name;
  std::vector<std::string> const_names;
  std::string replaced_name;
  TerrainType old_terrain_id;
  TerrainType new_terrain_id;
  TerrainGroup terrain_group;
};

std::vector<char> read_file(const fs::path& filename) {
  std::vector<char> vec;
  std::ifstream stream(resolve_path(filename), std::ios_base::binary);
  stream.seekg(0, std::ios_base::end);
  auto size = stream.tellg();
  vec.resize(size);
  stream.seekg(0, std::ios_base::beg);
  stream.read(vec.data(), size);
  return vec;
}

template <typename CharT, typename TraitsT = std::char_traits<CharT>>
class vecbuf : public std::basic_streambuf<CharT, TraitsT> {
public:
  vecbuf(std::vector<CharT>& vec) {
    this->setg(vec.data(), vec.data(), vec.data() + vec.size());
  }
};

/**
 * List of used terrains. Check `usedTerrains[terrainTypeId] == true` to see if
 * a terrain type is in use in the map.
 */
using UsedTerrainsMap = std::array<bool, 100>;

/**
 * In-memory representation of a ZR@ map, mapping file names to binary data.
 */
using ZipRMSFiles = std::map<std::string, std::vector<char>>;

static const auto rxForest =
    std::regex(R"(\W(PINE_FOREST|LEAVES|JUNGLE|BAMBOO|FOREST)\W)");
static const auto rxDesert = std::regex(R"(\W(PALM_DESERT|DESERT)\W)");

/**
 * Check if a certain terrain is used by a map script and mark it as used if so.
 */
static bool is_terrain_used(TerrainType terrain, UsedTerrainsMap& used_terrains,
                            const std::string& source_code,
                            const std::map<TerrainType, std::regex>& patterns) {
  if (terrain == TerrainType::Leaves || terrain == TerrainType::Forest) {
    if (!used_terrains[TerrainType::DLC3RiceFarm1]) {
      used_terrains[TerrainType::DLC3RiceFarm1] = true;
      used_terrains[TerrainType::DLC3RiceFarm2] =
          std::regex_search(source_code, rxForest);
    }
    return used_terrains[TerrainType::DLC3RiceFarm2];
  }
  if (terrain == TerrainType::PalmDesert || terrain == TerrainType::Sand) {
    if (!used_terrains[TerrainType::DLC3RiceFarmCnst1]) {
      used_terrains[TerrainType::DLC3RiceFarmCnst1] = true;
      used_terrains[TerrainType::DLC3RiceFarmCnst2] =
          std::regex_search(source_code, rxDesert);
    }
    return used_terrains[TerrainType::DLC3RiceFarmCnst2];
  } else {
    return used_terrains[terrain] =
               std::regex_search(source_code, patterns.at(terrain));
  }
}

static const auto rxDlcWater4 = std::regex(R"(\WDLC_WATER4\W)");
static const auto rxDlcWater5 = std::regex(R"(\WDLC_WATER5\W)");
static const auto rxWater = std::regex(R"(\WWATER\W)");
static const auto rxMedWater = std::regex(R"(\WMED_WATER\W)");
static const auto rxDeepWater = std::regex(R"(\WDEEP_WATER\W)");

/**
 * Check if a random map script uses multiple water terrains, and mark the
 * WaterNormal terrain as used if so.
 */
static bool uses_multiple_water_terrains(const std::string& source_code,
                                         UsedTerrainsMap& used_terrains) {
  if (!used_terrains[TerrainType::WaterNormal]) {
    used_terrains[TerrainType::WaterNormal] =
        std::regex_search(source_code, rxDlcWater4) ||
        std::regex_search(source_code, rxDlcWater5) ||
        std::regex_search(source_code, rxWater) ||
        std::regex_search(source_code, rxMedWater) ||
        std::regex_search(source_code, rxDeepWater);
  }
  return used_terrains[TerrainType::WaterNormal];
}

static const auto rxPlayerSetup = std::regex(R"(<PLAYER_SETUP>\s*(\r*)\n)");
static const auto rxIncludeDrs =
    std::regex(R"(#include_drs\s+random_map\.def\s*(\r*)\n)");

/**
 * Add some code to the map to change the trees on a certain terrain from AoC
 * trees to DLC trees using UserPatch effects.
 */
static void upgrade_trees(TerrainType usedTerrain, TerrainType oldTerrain,
                          std::string& map) {
  std::string new_tree_name;
  std::string old_tree_name;
  switch (usedTerrain) {
  case TerrainType::Forest:
    old_tree_name = "FOREST_TREE";
    break;
  case TerrainType::PalmDesert:
    old_tree_name = "PALMTREE";
    break;
  case TerrainType::SnowForest:
    old_tree_name = "SNOWPINETREE";
    break;
  default:
    break;
  }
  if (oldTerrain == TerrainType::DLC2DragonTreeForest) {
    new_tree_name = "DRAGONTREE";
  } else {
    new_tree_name = "DLC_RAINTREE";
  }
  if (map.find("<PLAYER_SETUP>") != std::string::npos)
    map =
        std::regex_replace(map, rxPlayerSetup,
                           "<PLAYER_SETUP>$1\n  "
                           "effect_amount GAIA_UPGRADE_UNIT " +
                               old_tree_name + " " + new_tree_name + " 0$1\n");
  else
    map =
        std::regex_replace(map, rxIncludeDrs,
                           "#include_drs random_map.def$1\n<PLAYER_SETUP>$1\n  "
                           "effect_amount GAIA_UPGRADE_UNIT " +
                               old_tree_name + " " + new_tree_name + " 0$1\n");
}

static const std::array<std::map<TerrainType, std::regex>, 6> terrain_groups =
    {{// The Order is important, see the TerrainGroup Enum!
      {},
      {{TerrainType::WaterNormal, std::regex(R"(\WMED_WATER\W)")},
       {TerrainType::WaterDeep, std::regex(R"(\WDEEP_WATER\W)")}},
      {},
      {{TerrainType::Grass, std::regex(R"(\WGRASS\W)")},
       {TerrainType::Desert3, std::regex(R"(\WDIRT3\W)")},
       {TerrainType::Desert, std::regex(R"(\WDIRT1\W)")},
       {TerrainType::Grass3, std::regex(R"(\WGRASS3\W)")},
       {TerrainType::Grass2, std::regex(R"(\WGRASS2\W)")},
       {TerrainType::Sand, std::regex(R"(\WDESERT\W)")},
       {TerrainType::Road, std::regex(R"(\WROAD\W)")},
       {TerrainType::Road2, std::regex(R"(\WROAD2\W)")},
       {TerrainType::FungusRoad, std::regex(R"(\WROAD3\W)")}},
      {{TerrainType::Forest, std::regex(R"(\WFOREST\W)")},
       {TerrainType::PalmDesert, std::regex(R"(\WPALM_DESERT\W)")},
       {TerrainType::SnowForest, std::regex(R"(\WSNOW_FOREST\W)")}},
      {{TerrainType::UnbuildableRock, std::regex(R"(\WDLC_ROCK\W)")},
       {TerrainType::Ice2, std::regex(R"(\WICE\W)")}}}};

static const std::vector<MapConvertData> terrain_replacements = {
    // slp_name, const_name_pattern, replaced_name_pattern, old_terrain_id,
    // new_terrain_id, terrain_group
    {"DRAGONFOREST.slp",
     {"DRAGONFORES", "DRAGONFOREST"},
     "DRAGONFOREST",
     TerrainType::DLC2DragonTreeForest,
     TerrainType::SnowForest,
     ForestTerrain},
    {"ACACIA_FOREST.slp",
     {"ACCACIA_FOREST", "ACACIA_FOREST", "ACACIAFORES"},
     "ACACIA_FOREST",
     TerrainType::DLC2AcaciaForest,
     TerrainType::DLC2Savannah,
     None},
    {"DLC_RAINFOREST.slp",
     {"DLC_RAINFOREST"},
     "DLC_RAINFOREST",
     TerrainType::DLC3Rainforest,
     TerrainType::Forest,
     ForestTerrain},
    {"BAOBAB.slp",
     {"BAOBABS", "BAOBAB_FOREST"},
     "BAOBAB_FOREST",
     TerrainType::DLC2BaobabForest,
     TerrainType::ImpassableCliffGrass,
     None},
    {"DLC_MANGROVESHALLOW.slp",
     {"DLC_MANGROVESHALLOW"},
     "DLC_MANGROVESHALLOW",
     TerrainType::DLC3WalkableShallowsMangrove,
     TerrainType::Desert2,
     None},
    {"DLC_MANGROVEFOREST.slp",
     {"DLC_MANGROVEFOREST"},
     "DLC_MANGROVEFOREST",
     TerrainType::DLC3MangroveForest,
     TerrainType::OakForest,
     None},
    {"DLC_NEWSHALLOW.slp",
     {"DLC_NEWSHALLOW"},
     "DLC_NEWSHALLOW",
     TerrainType::DLC3WalkableShallowsAzure,
     TerrainType::WalkableShallows,
     FixedTerrain},
    {"SAVANNAH.slp",
     {"SAVANNAH", "DLC_SAVANNAH"},
     "SAVANNAH",
     TerrainType::DLC2Savannah,
     TerrainType::Sand,
     LandTerrain},
    {"DIRT4.slp",
     {"DIRT4", "DLC_DIRT4"},
     "DIRT4",
     TerrainType::DLC2Dirt4,
     TerrainType::Desert3,
     LandTerrain},
    {"MOORLAND.slp",
     {"DLC_MOORLAND", "MOORLAND"},
     "DLC_MOORLAND",
     TerrainType::DLC2Moorland,
     TerrainType::Grass3,
     LandTerrain},
    {"CRACKEDIT.slp",
     {"CRACKEDIT"},
     "CRACKEDIT",
     TerrainType::DLC2CrackedDesert,
     TerrainType::SnowRoad,
     None},
    {"QUICKSAND.slp",
     {"QUICKSAND", "DLC_QUICKSAND"},
     "QUICKSAND",
     TerrainType::DLC2QuickSand,
     TerrainType::UnbuildableRock,
     FixedTerrain},
    {"BLACK.slp",
     {"BLACK", "DLC_BLACK"},
     "DLC_BLACK",
     TerrainType::DLC2ForgottenBlack,
     TerrainType::UnbuildableRock,
     FixedTerrain},
    {"DLC_BEACH2.slp",
     {"DLC_BEACH2"},
     "DLC_BEACH2",
     TerrainType::DLC3Beach2,
     TerrainType::Beach,
     FixedTerrain},
    {"DLC_BEACH3.slp",
     {"DLC_BEACH3"},
     "DLC_BEACH3",
     TerrainType::DLC3Beach3,
     TerrainType::Beach,
     FixedTerrain},
    {"DLC_BEACH4.slp",
     {"DLC_BEACH4"},
     "DLC_BEACH4",
     TerrainType::DLC3Beach4,
     TerrainType::Beach,
     FixedTerrain},
    {"DLC_DRYROAD.slp",
     {"DLC_DRYROAD"},
     "DLC_DRYROAD",
     TerrainType::DLC2DesertRoad,
     TerrainType::Road2,
     LandTerrain},
    {"DLC_WATER4.slp",
     {"DLC_WATER4"},
     "DLC_WATER4",
     TerrainType::DLC3WaterDeepOcean,
     TerrainType::WaterDeep,
     WaterTerrain},
    {"DLC_WATER5.slp",
     {"DLC_WATER5"},
     "DLC_WATER5",
     TerrainType::DLC3WaterAzure,
     TerrainType::WaterShallow,
     WaterTerrain},
    {"DLC_JUNGLELEAVES.slp",
     {"DLC_JUNGLELEAVES"},
     "DLC_JUNGLELEAVES",
     TerrainType::DLC3LeavesJungle,
     TerrainType::Leaves,
     LandTerrain},
    {"DLC_JUNGLEROAD.slp",
     {"DLC_JUNGLEROAD"},
     "DLC_JUNGLEROAD",
     TerrainType::DLC3RoadJungle,
     TerrainType::FungusRoad,
     LandTerrain},
    {"DLC_JUNGLEGRASS.slp",
     {"DLC_JUNGLEGRASS"},
     "DLC_JUNGLEGRASS",
     TerrainType::DLC3GrassJungle,
     TerrainType::Grass2,
     LandTerrain}};

static const std::map<int, std::string> terrain_file_names = {
    {TerrainType::Grass, "15001.slp"},
    {TerrainType::WaterShallow, "15002.slp"},
    {TerrainType::Beach, "15017.slp"},
    {TerrainType::Desert3, "15007.slp"},
    {TerrainType::WalkableShallows, "15014.slp"},
    {TerrainType::Leaves, "15011.slp"},
    {TerrainType::Desert, "15014.slp"},
    {TerrainType::Grass3, "15009.slp"},
    {TerrainType::Forest, "15011.slp"},
    {TerrainType::Grass2, "15008.slp"},
    {TerrainType::PalmDesert, "15010.slp"},
    {TerrainType::Sand, "15010.slp"},
    {TerrainType::SnowForest, "15029.slp"},
    {TerrainType::WaterDeep, "15015.slp"},
    {TerrainType::WaterNormal, "15016.slp"},
    {TerrainType::Road, "15018.slp"},
    {TerrainType::Road2, "15019.slp"},
    {TerrainType::Ice2, "15024.slp"},
    {TerrainType::FungusRoad, "15031.slp"},
    {TerrainType::UnbuildableRock, "15033.slp"}};

/**
 * Convert a random map script from HD Edition to WololoKingdoms. This involves
 * replacing constant values and adding terrain graphics for unsupported
 * terrains.
 *
 * Returns a map containing files that should be output in a ZR@ file. If it
 * only contains one file, the map can be saved as a plain rms file. The
 * consumer is responsible for checking if an scx file should be added. This can
 * be added to the returned map of files after the fact.
 */
static ZipRMSFiles
convert_map(std::string& map, const std::string& map_name,
            const std::map<std::string, fs::path>& terrain_graphics) {
  ZipRMSFiles map_files;
  UsedTerrainsMap used_terrains;

  /*
   * Search for specific constant names in the map.
   * Some constants have multiple spellings, which is why const_names is a
   * vector. Finding one is enough, so in that case we can break out of the
   * inner loop
   */
  for (auto& replacement : terrain_replacements) {
    for (auto& const_name : replacement.const_names) {
      if (map.find(const_name) != std::string::npos) {
        if (replacement.new_terrain_id < TerrainType::DLC2Savannah) {
          // 41 is also an expansion terrain, but that's okay, it's a fixed
          // replacement
          used_terrains.at(replacement.new_terrain_id) = true;
        }
        used_terrains.at(replacement.old_terrain_id) = true;
        break;
      }
    }
  }

  for (auto& replacement : terrain_replacements) {
    if (!used_terrains.at(replacement.old_terrain_id))
      continue;
    // Check if replacement candidate is already used
    TerrainType usedTerrain = replacement.new_terrain_id;
    // If it's one of the terrains with a shared slp, we need to search the
    // map for these other terrains too, else just the usedTerrain
    if (replacement.terrain_group > FixedTerrain &&
        is_terrain_used(usedTerrain, used_terrains, map,
                        terrain_groups[replacement.terrain_group])) {
      bool success = false;
      for (auto& [id, rx] : terrain_groups[replacement.terrain_group]) {
        if (used_terrains.at(id))
          continue;
        else if (is_terrain_used(id, used_terrains, map,
                                 terrain_groups[replacement.terrain_group])) {
          continue;
        }
        success = true;
        usedTerrain = id;
        used_terrains.at(id) = true;
        break;
      }
      if (!success && replacement.terrain_group == LandTerrain &&
          !is_terrain_used(TerrainType::Leaves, used_terrains, map,
                           terrain_groups[replacement.terrain_group])) {
        usedTerrain = TerrainType::Leaves; // Leaves is a last effort, usually
                                           // likely to be used already
        used_terrains[TerrainType::Leaves] = true;
      }
    }

    std::string terrain_const_name;
    if (replacement.const_names.size() == 1)
      terrain_const_name = replacement.const_names[0];
    else {
      terrain_const_name = "(";
      for (auto& const_name : replacement.const_names) {
        terrain_const_name += const_name + "|";
      }
      terrain_const_name[terrain_const_name.size() - 1] = ')';
    }
    if (usedTerrain != replacement.new_terrain_id) {
      map = std::regex_replace(map, std::regex(terrain_const_name),
                               "MY" + replacement.replaced_name);
      auto terrainConstDef =
          std::regex(R"(#const\sMY+)" + terrain_const_name + R"(\s+)" +
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
      auto terrainConstDef =
          std::regex(R"(#const\sMY+)" + terrain_const_name + R"(\s+)" +
                     std::to_string(replacement.old_terrain_id));
      map = std::regex_replace(map, terrainConstDef,
                               "#const " + replacement.replaced_name + " " +
                                   std::to_string(usedTerrain));
    }

    if (replacement.terrain_group == None ||
        (replacement.terrain_group == WaterTerrain &&
         uses_multiple_water_terrains(map, used_terrains)))
      continue;

    map_files[terrain_file_names.at(usedTerrain)] =
        read_file(terrain_graphics.at(replacement.slp_name));

    if (replacement.terrain_group == ForestTerrain) {
      upgrade_trees(usedTerrain, replacement.old_terrain_id, map);
    }
  }
  if (used_terrains.at(TerrainType::Desert2)) {
    map_files["15004.slp"] = read_file(terrain_graphics.at("15004.slp"));
    map_files["15005.slp"] = read_file(terrain_graphics.at("15005.slp"));
    map_files["15021.slp"] = read_file(terrain_graphics.at("15021.slp"));
    map_files["15022.slp"] = read_file(terrain_graphics.at("15022.slp"));
    map_files["15023.slp"] = read_file(terrain_graphics.at("15023.slp"));
  }

  map_files[map_name] = std::vector(map.begin(), map.end());
  return map_files;
}

void convert_maps(const fs::path& input_dir, const fs::path& output_dir,
                  const std::map<std::string, fs::path>& terrain_graphics,
                  bool replace) {
  // Find all maps to be converted
  std::vector<fs::path> map_names;
  for (const auto& entry : fs::directory_iterator(resolve_path(input_dir))) {
    auto extension = entry.path().extension();
    if (extension == ".rms") {
      map_names.push_back(entry.path());
    }
  }

  // Main Loop, Iterate through the maps to be converted
  for (auto& map_path : map_names) {
    /*
     * If the Map is already a ZR@ map, just copy filename
     * If not, check if a ZR@ map with that name exists in the target directory
     * and remove filename to be replaced by the newly converted map, if the replace
     * option is set to true. Else skip that map.
     */
    auto map_name = map_path.stem().string() + ".rms";
    auto map_prefix = map_name.substr(0, 3);
    if (map_prefix == "ZR@") {
      cfs::copy_file(map_path, output_dir / map_name,
                     fs::copy_options::update_existing);
      continue;
    } else if (map_prefix == "es_") {
      continue; // These maps are already added to Voobly with the es@ prefix,
                // no need to have this twice
    }
    if (cfs::exists(output_dir / map_path.filename()) ||
        cfs::exists(output_dir / ("ZR@" + map_path.filename().string()))) {
      if (replace)
        cfs::remove(output_dir / map_path.filename());
      else
        continue;
    }
    cfs::remove(output_dir / ("ZR@" + map_path.filename().string()));
    std::ifstream input(resolve_path(input_dir / map_path.filename()));
    std::string map = concat_stream(input);
    input.close();

    auto map_files = convert_map(map, map_name, terrain_graphics);
    if (map_name.substr(0, 3) == "rw_" || map_name.substr(0, 3) == "sm_") {
      auto scx_name = map_path.stem().string() + ".scx";
      map_files[scx_name] = read_file(input_dir / scx_name);
    }

    if (map_files.size() != 1) {
      std::ofstream outstream(output_dir / ("ZR@" + map_name),
                              std::ios_base::out | std::ios_base::binary);
      outstream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      ZRMapCreator zr_map(outstream);
      for (auto& [name, data] : map_files) {
        vecbuf buf(data);
        std::istream stream(&buf);
        zr_map.addFile(name, stream);
      }
      zr_map.end();
    } else {
      std::ofstream outstream(output_dir / map_name,
                              std::ios_base::out | std::ios_base::binary);
      outstream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      outstream.write(map_files[map_name].data(), map_files[map_name].size());
    }
  }
}
