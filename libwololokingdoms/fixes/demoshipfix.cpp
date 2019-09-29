#include "demoshipfix.h"
#include "wololo/datPatch.h"

namespace wololo {

void demoshipPatch(genie::DatFile* aocDat) {
  /*
   * There are a few hardcoded detonation units, UP 1.5 offers a way to do it
   * without However, it does full damage in the entire blast radius
   */

  size_t const demoRaftUnitId = 653;
  size_t const demoShipUnitId = 527;
  size_t const heavyDemoShipUnitId = 528;
  // size_t const darkAgeTechId = 104;

  for (auto& civ : aocDat->Civs) {
    civ.Units[demoRaftUnitId].Creatable.HeroMode = 96;
    civ.Units[demoShipUnitId].Creatable.HeroMode = 96;
    civ.Units[heavyDemoShipUnitId].Creatable.HeroMode = 96;
  }
  /*genie::TechageEffect effect = genie::TechageEffect();
      effect.Type = 0; // set attribute
      effect.A = demoShipUnitId;
      effect.C = 40;
      effect.D = 96;
      aocDat->Techages[darkAgeTechId].Effects.push_back(effect);
  effect.A = heavyDemoShipUnitId;
  aocDat->Techages[demoRaftTechId].Effects.push_back(effect);
  effect.A = demoRaftUnitId;
  aocDat->Techages[demoRaftTechId].Effects.push_back(effect);*/
}

DatPatch demoShipFix = {&demoshipPatch, "Demolition ships not exploding fix"};

} // namespace wololo
