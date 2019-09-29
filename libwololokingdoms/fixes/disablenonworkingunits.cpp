#include "disablenonworkingunits.h"
#include "wololo/datPatch.h"

namespace wololo {

void disableNonWorkingUnitsPatch(genie::DatFile* aocDat) {
  /*
   * Disabling units that are not supposed to show in the scenario editor
   */

  for (auto& civ : aocDat->Civs) {
    civ.Units[1119].HideInEditor = 1;
    civ.Units[1145].HideInEditor = 1;
    civ.Units[1147].HideInEditor = 1;
    civ.Units[1221].HideInEditor = 1;
    civ.Units[1401].HideInEditor = 1;
    for (size_t unitIndex = 1224; unitIndex <= 1400; unitIndex++) {
      civ.Units[unitIndex].HideInEditor = 1;
    }
  }
}

DatPatch disableNonWorkingUnits = {&disableNonWorkingUnitsPatch,
                                   "Hide units in the scenario editor"};

} // namespace wololo
