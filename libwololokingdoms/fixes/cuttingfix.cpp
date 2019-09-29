#include "cuttingfix.h"
#include "wololo/datPatch.h"

namespace wololo {

void cuttingPatch(genie::DatFile* aocDat) {

  //TODO: Make sure the regular (cutting) unit remains at the normal ID

  // Civ Bonus Fix
  size_t const onagerID = 550;
  size_t const newOnagerID = 948;
  size_t const siegeOnagerTechID = 320;

  size_t const cuttingResearchID = 152;
  size_t const autoOnagerCuttingResearchID = 153;
  size_t const onagerResearchTechID = 154;
  size_t const disablingResearchID = 155;
  size_t const onagerTechID = 257;
  size_t const onagerEffectID = 247;

  size_t const onagerCuttingEffectID = 308;
  size_t const disablingTechID = 309;

  size_t const chemistryID = 47;
  size_t const furorCelticaID = 239;
  size_t const koreanTeamBonusID = 505;
  size_t const shinkichonID = 506;

  size_t const turkTechTreeID = 263;
  size_t const hunsTechTreeID = 448;

  genie::EffectCommand effect =
      aocDat->Effects[onagerEffectID].EffectCommands[0];

  aocDat->UnitHeaders[newOnagerID] = aocDat->UnitHeaders[onagerID];
  aocDat->UnitHeaders[onagerID].TaskList.erase(
      aocDat->UnitHeaders[onagerID].TaskList.begin() + 4);
  for (auto& civ : aocDat->Civs) {
    civ.Units[newOnagerID] = civ.Units[onagerID];
    // TODO replace onagerID in here with newOnagerID with the new data upate
    civ.Units[onagerID].Combat.BlastAttackLevel = 2;
    civ.Units[onagerID].LanguageDLLCreation += 205;
    civ.Units[onagerID].LanguageDLLHelp += 205;
    civ.Units[onagerID].LanguageDLLName += 205;
  }
  effect.UnitClassID = newOnagerID;
  aocDat->Effects[onagerCuttingEffectID].EffectCommands.push_back(effect);
  effect.TargetUnit = onagerID;
  aocDat->Effects[onagerCuttingEffectID].EffectCommands.push_back(effect);
  aocDat->Effects[onagerCuttingEffectID].Name = "Onager Cutting Tech";
  effect.TargetUnit = newOnagerID;
  aocDat->Effects[siegeOnagerTechID].EffectCommands.push_back(effect);
  effect.Type = 102;
  effect.Amount = cuttingResearchID;
  aocDat->Effects[disablingTechID].EffectCommands.push_back(effect);
  aocDat->Effects[turkTechTreeID].EffectCommands.push_back(effect);
  aocDat->Effects[hunsTechTreeID].EffectCommands.push_back(effect);
  effect.Amount = onagerResearchTechID;
  aocDat->Effects[disablingTechID].EffectCommands.push_back(effect);
  aocDat->Effects[turkTechTreeID].EffectCommands.push_back(effect);
  aocDat->Effects[hunsTechTreeID].EffectCommands.push_back(effect);
  effect.Amount = autoOnagerCuttingResearchID;
  aocDat->Effects[turkTechTreeID].EffectCommands.push_back(effect);
  aocDat->Effects[hunsTechTreeID].EffectCommands.push_back(effect);

  aocDat->Effects[chemistryID].EffectCommands.push_back(
      aocDat->Effects[chemistryID].EffectCommands[66]);
  aocDat->Effects[chemistryID].EffectCommands[69].TargetUnit = newOnagerID;

  aocDat->Effects[furorCelticaID].EffectCommands.push_back(
      aocDat->Effects[furorCelticaID].EffectCommands[0]);
  aocDat->Effects[furorCelticaID].EffectCommands[12].TargetUnit = newOnagerID;

  aocDat->Effects[koreanTeamBonusID].EffectCommands.push_back(
      aocDat->Effects[koreanTeamBonusID].EffectCommands[0]);
  aocDat->Effects[koreanTeamBonusID].EffectCommands[3].TargetUnit = newOnagerID;

  aocDat->Effects[shinkichonID].EffectCommands.push_back(
      aocDat->Effects[shinkichonID].EffectCommands[0]);
  aocDat->Effects[shinkichonID].EffectCommands.push_back(
      aocDat->Effects[shinkichonID].EffectCommands[1]);
  aocDat->Effects[shinkichonID].EffectCommands[12].TargetUnit = newOnagerID;
  aocDat->Effects[shinkichonID].EffectCommands[13].TargetUnit = newOnagerID;

  aocDat->Techs[cuttingResearchID] = aocDat->Techs[siegeOnagerTechID];
  aocDat->Techs[cuttingResearchID].RequiredTechs[1] = onagerTechID;
  aocDat->Techs[cuttingResearchID].RequiredTechCount = 2;
  aocDat->Techs[cuttingResearchID].EffectID = -1;
  aocDat->Techs[cuttingResearchID].ButtonID = 10;
  aocDat->Techs[cuttingResearchID].IconID = 5;
  aocDat->Techs[cuttingResearchID].Name = "Cutting Research";
  aocDat->Techs[cuttingResearchID].LanguageDLLDescription = 8440;
  aocDat->Techs[cuttingResearchID].LanguageDLLHelp = 107440;
  aocDat->Techs[cuttingResearchID].LanguageDLLName = 7440;
  aocDat->Techs[cuttingResearchID].LanguageDLLTechTree = 157440;

  aocDat->Techs[autoOnagerCuttingResearchID] = aocDat->Techs[siegeOnagerTechID];
  aocDat->Techs[autoOnagerCuttingResearchID].ResearchTime = 0;
  aocDat->Techs[autoOnagerCuttingResearchID].ResearchLocation = -1;
  aocDat->Techs[autoOnagerCuttingResearchID].EffectID = onagerCuttingEffectID;
  aocDat->Techs[autoOnagerCuttingResearchID].ResourceCosts[0].Amount = 0;
  aocDat->Techs[autoOnagerCuttingResearchID].ResourceCosts[0].Paid = 0;
  aocDat->Techs[autoOnagerCuttingResearchID].ResourceCosts[0].Type = -1;
  aocDat->Techs[autoOnagerCuttingResearchID].ResourceCosts[1].Amount = 0;
  aocDat->Techs[autoOnagerCuttingResearchID].ResourceCosts[1].Paid = 0;
  aocDat->Techs[autoOnagerCuttingResearchID].ResourceCosts[1].Type = -1;
  aocDat->Techs[autoOnagerCuttingResearchID].Name = "Auto Onager Cutting";
  aocDat->Techs[autoOnagerCuttingResearchID].LanguageDLLDescription = 0;
  aocDat->Techs[autoOnagerCuttingResearchID].LanguageDLLName = 0;

  aocDat->Techs[onagerResearchTechID] =
      aocDat->Techs[autoOnagerCuttingResearchID];
  aocDat->Techs[onagerResearchTechID].RequiredTechs[0] = cuttingResearchID;
  aocDat->Techs[onagerResearchTechID].Name = "Onager Cutting";
  aocDat->Techs[onagerResearchTechID].LanguageDLLDescription = 0;
  aocDat->Techs[onagerResearchTechID].LanguageDLLName = 0;

  aocDat->Techs[disablingResearchID] = aocDat->Techs[332];
  aocDat->Techs[disablingResearchID].EffectID = disablingTechID;
  aocDat->Techs[disablingResearchID].Name = "Cutting Research Disabler";
  aocDat->Techs[disablingResearchID].RequiredTechs[0] = 101;
  aocDat->Techs[disablingResearchID].RequiredTechCount = 1;
}

DatPatch cuttingFix = {
    &cuttingPatch, "Option of non-cutting onagers and cutting tech per map"};

} // namespace wololo
