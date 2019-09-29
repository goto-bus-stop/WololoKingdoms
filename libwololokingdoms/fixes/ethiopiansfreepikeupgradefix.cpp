#include "ethiopiansfreepikeupgradefix.h"
#include "wololo/datPatch.h"

namespace wololo {

void ethiopiansPikePatch(genie::DatFile* aocDat) {
  /*
   * In AOC, free tech effects must be in the civ tech tree in order to work
   */

  const size_t FREE_PIKE_HALB_TECH_ID = 616;
  const size_t ETHIOPIAN_TECH_TREE_TECH_ID = 48;

  for (auto& command : aocDat->Effects[FREE_PIKE_HALB_TECH_ID].EffectCommands) {
    aocDat->Effects[ETHIOPIAN_TECH_TREE_TECH_ID].EffectCommands.push_back(command); // copy the effects into the ethiopians tech tree
  }
}

DatPatch ethiopiansFreePikeUpgradeFix = {
    &ethiopiansPikePatch,
    "Ethiopians free pike/halbs upgrades not working fix"};

} // namespace wololo
