#include "berbersutfix.h"
#include "wololo/datPatch.h"

namespace wololo {

static void berbersUTPatch(genie::DatFile* aocDat) {
  /*
   * Thanks to UP 1.5 Kasbah works natively, the regeneration works with the
   * hero ability and just needs slight adjustments
   */

  const size_t KASBAH_ID = 608;

  // imperial age
  std::vector<genie::EffectCommand> effectsToAdd;
  for (auto& effect : aocDat->Effects[KASBAH_ID].EffectCommands) {
    // set a hero attribute for regen
    effect.Type = 0;         // set attribute
    effect.AttributeID = 40; // hero attribute
    effect.Amount = 4.0f;    // regen

    // add an attribute to modify the timer
    genie::EffectCommand newCommand = effect;
    newCommand.AttributeID = 45;
    newCommand.Amount = 4.0f;
    effectsToAdd.push_back(newCommand);
  }
  auto& kasbahCommands = aocDat->Effects[KASBAH_ID].EffectCommands;
  kasbahCommands.insert(kasbahCommands.end(), effectsToAdd.begin(), effectsToAdd.end());
}

DatPatch berbersUTFix = {&berbersUTPatch,
                         "Berbers unique technologies alternative"};

} // namespace wololo
