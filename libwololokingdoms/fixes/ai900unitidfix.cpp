#include "ai900unitidfix.h"
#include "genie/dat/DatFile.h"
#include "wololo/datPatch.h"

namespace wololo {

/*
 * In AOC, AIs can't properly interact with Units that have an ID over 899
 * This algorithm is a workaround that works by swapping the units and
 * searching the whole dat file for references to their IDs, changing them
 * accordingly, essentially doing in a second what would take hours of manual
 * work
 */

static std::vector<std::pair<int, int>> const unitsIDtoSwap = {
    {1103, 529}, // Fire Galley, Fire Ship
    {1104, 527}, // Demolition Raft, Demolition Ship NOTE: These two are special
                 // to make the tech tree work

    {1001, 106}, // Organ Gun, INFIL_D
    {949, 108},  // X Patch Trade Cart, JUNKX_D
    {1003, 114}, // Elite Organ Gun, LNGBT_D
    {1006, 183}, // Elite Caravel, TMISB
    {1007, 203}, // Camel Archer, VDML
    {1009, 208}, // Elite Camel Archer, TWAL
    {1010, 223}, // Genitour, VFREP_D
    {1012, 230}, // Elite Genitour, VMREP_D
    {1013, 260}, // Gbeto, OLD-FISH3
    {936, 412},  // Elephant, MONKX_S_D
    {1015, 418}, // Elite Gbeto, TROCK
    {1016, 453}, // Shotel Warrior, DOLPH4
    {1018, 459}, // Elite Shotel Warrior, FISH5
    {1103, 467}, // Fire Ship, Nonexistent
    {1105, 494}, // Siege Tower, CVLRY_D
    {1104, 653}, // Demolition Ship, HFALS_D
    //{947, 699}, // Cutting Mangonel, HSUBO_D
    {948, 701},  // Cutting Onager, HWOLF_D
    {1079, 732}, // Genitour placeholder, HKHAN_D
    {1021, 734}, // Feitoria, Nonexistent
    {1120, 760}, // Ballista Elephant, BHUSK_D
    {1155, 762}, // Imperial Skirmisher, BHUSKX_D
    {1134, 766}, // Elite Battle Ele, UPLUM_D
    {1132, 774}, // Battle Elephant, UCONQ_D
    {1131, 782}, // Elite Rattan Archer, HPOPE_D
    {1129, 784}, // Rattan Archer, HWITCH_D
    {1128, 811}, // Elite Arambai, HEROBOAR_D
    {1126, 823}, // Arambai, BOARJ_D
    {1125, 830}, // Elite Karambit, UWAGO_D
    {1123, 836}, // Karambit, HORSW_D
    {946, 848},  // Noncut Ballista Elephant, TDONK_D
    {1004, 861}, // Caravel, mkyby_D
    {1122, 891}  // Elite Ballista Ele, SGTWR_D
};

static void swapId(int32_t* val, int32_t id1, int32_t id2) {
  if (*val == id1) {
    *val = id2;
  } else if (*val == id2) {
    *val = id1;
  }
}

static void swapId(int16_t* val, int16_t id1, int16_t id2) {
  if (*val == id1) {
    *val = id2;
  } else if (*val == id2) {
    *val = id1;
  }
}

static void swapIdInCommon(genie::techtree::Common* common, int id1, int id2) {
  for (int i = 0; i < common->SlotsUsed; i++) {
    if (common->Mode[i] == 2) { // Unit
      swapId(&common->UnitResearch[i], id1, id2);
    }
  }
}

static void swapUnits(genie::DatFile* aocDat, int id1, int id2) {
  // First : swap the actual units
  std::swap(aocDat->UnitHeaders[id1], aocDat->UnitHeaders[id2]);
  for (auto& civ : aocDat->Civs) {
    /* switch all 3 ids around first*/
    civ.Units[id1].ID = id2;
    civ.Units[id1].CopyID = id2;
    civ.Units[id1].BaseID = id2;
    civ.Units[id2].ID = id1;
    civ.Units[id2].CopyID = id1;
    civ.Units[id2].BaseID = id1;
    std::swap(civ.Units[id1], civ.Units[id2]);
    std::swap(civ.UnitPointers[id1], civ.UnitPointers[id2]);
  }

  // Then : modify all references to these units

  // Iterate techs
  for (auto& effect : aocDat->Effects) {
    // Iterate effects of each tech
    for (auto& effectCommand : effect.EffectCommands) {
      switch (effectCommand.Type) {
      case 3: // upgrade unit (this ones uses 2 units hence the special case,
              // notice the absence of break)
        swapId(&effectCommand.UnitClassID, id1, id2);
      case 0: // attribute modifier
      case 2: // enable/disable unit
      case 4: // attribute modifier (+/-)
      case 5: // attribute modifier (*)
        swapId(&effectCommand.TargetUnit, id1, id2);
      }
    }
  }

  // Iterate tech tree ages
  for (auto& connection : aocDat->TechTree.TechTreeAges) {
    for (auto& connectedUnitId : connection.Units) {
      swapId(&connectedUnitId, id1, id2);
    }
  }

  // Iterate tech tree buildings
  for (auto& connection : aocDat->TechTree.BuildingConnections) {
    for (auto& connectedUnitId : connection.Units) {
      swapId(&connectedUnitId, id1, id2);
    }
    swapIdInCommon(&connection.Common, id1, id2);
  }

  // Iterate tech tree units
  for (auto& connection : aocDat->TechTree.UnitConnections) {
    swapId(&connection.ID, id1, id2);
    for (auto& connectedUnitId : connection.Units) {
      swapId(&connectedUnitId, id1, id2);
    }
    swapIdInCommon(&connection.Common, id1, id2);
  }

  // Iterate tech tree researches
  for (auto& connection : aocDat->TechTree.ResearchConnections) {
    for (auto& connectedUnitId : connection.Units) {
      swapId(&connectedUnitId, id1, id2);
    }
    swapIdInCommon(&connection.Common, id1, id2);
  }

  // Iterate through Civs Units to replace the dead unit graphic if necessary
  for (auto& civ : aocDat->Civs) {
    for (auto& unit : civ.Units) {
      swapId(&unit.DeadUnitID, id1, id2);
    }
  }

  // Iterate through Unit commands (e.g. villagers hunting animals)
  for (auto& unit : aocDat->UnitHeaders) {
    for (auto& task : unit.TaskList) {
      swapId(&task.UnitID, id1, id2);
    }
  }
}

static void ai900unitidPatch(genie::DatFile* aocDat) {
  for (const auto [id1, id2] : unitsIDtoSwap) {
    swapUnits(aocDat, id1, id2);
  }
}

DatPatch ai900UnitIdFix = {&ai900unitidPatch,
                           "AI can't use unit id over 900 workaround"};

} // namespace wololo
