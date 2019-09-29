#include "feitoriafix.h"
#include "wololo/datPatch.h"

namespace wololo {

void feitoriaPatch(genie::DatFile* aocDat) {
  float const feitoriaCooldown = 1;
  float const woodAdded = 0.8;
  float const foodAdded = 0.8;
  float const goldAdded = 0.45;
  float const stoneAdded = 0.25;

  int16_t const feitoriaId = 1021;          // 960
  int16_t const woodAnnexId = 1391;         // 957
  int16_t const foodAnnexId = 1392;         // 956
  int16_t const goldAnnexId = 1393;         // 959
  int16_t const stoneAnnexId = 1394;        // 958
  int16_t const woodTrickleId = 1395;       // 952
  int16_t const foodTrickleId = 1396;       // 951
  int16_t const goldTrickleId = 1397;       // 954
  int16_t const stoneTrickleId = 1398;      // 954
  int16_t const feitoriaDeadStackId = 1399; // 955
  int16_t const feitoriaStackId = 1400;     // 949

  for (auto& civ : aocDat->Civs) {
    civ.Units[feitoriaStackId] = civ.Units[feitoriaId];
    civ.Units[woodAnnexId] = civ.Units[feitoriaId];
    civ.Units[foodAnnexId] = civ.Units[feitoriaId];
    civ.Units[goldAnnexId] = civ.Units[feitoriaId];
    civ.Units[stoneAnnexId] = civ.Units[feitoriaId];
    civ.Units[woodTrickleId] = civ.Units[feitoriaId];
    civ.Units[foodTrickleId] = civ.Units[feitoriaId];
    civ.Units[goldTrickleId] = civ.Units[feitoriaId];
    civ.Units[stoneTrickleId] = civ.Units[feitoriaId];

    civ.Units[feitoriaStackId].Name = "Feitoria";
    civ.Units[woodAnnexId].Name = "AA-A Wood Annex";
    civ.Units[foodAnnexId].Name = "AA-A Food Annex";
    civ.Units[goldAnnexId].Name = "AA-A Stone Annex";
    civ.Units[stoneAnnexId].Name = "AA-A gold Annex";
    civ.Units[woodTrickleId].Name = "Wood Trickle";
    civ.Units[foodTrickleId].Name = "Food Trickle";
    civ.Units[goldTrickleId].Name = "Gold Trickle";
    civ.Units[stoneTrickleId].Name = "Stone Trickle";

    civ.Units[feitoriaId].Building.StackUnitID = feitoriaStackId;
    civ.Units[feitoriaId].Building.DisappearsWhenBuilt = 1;
    civ.Units[feitoriaId].HideInEditor = 1;

    civ.Units[feitoriaStackId].DyingGraphic = -1;
    civ.Units[feitoriaStackId].UndeadGraphic = -1;
    civ.Units[feitoriaStackId].DeadUnitID = feitoriaDeadStackId;
    civ.Units[feitoriaStackId].OcclusionMode = 6;
    civ.Units[feitoriaStackId].Creatable.TrainLocationID = 0;
    civ.Units[feitoriaStackId].ResourceStorages[2].Type = -1;
    civ.Units[feitoriaStackId].ResourceStorages[2].Amount = 0;
    civ.Units[feitoriaStackId].ResourceStorages[2].Paid = 0;
    civ.Units[feitoriaStackId].Building.HeadUnit = feitoriaId;
    civ.Units[feitoriaStackId].Building.Annexes[0].UnitID = woodAnnexId;
    civ.Units[feitoriaStackId].Building.Annexes[1].UnitID = foodAnnexId;
    civ.Units[feitoriaStackId].Building.Annexes[2].UnitID = goldAnnexId;
    civ.Units[feitoriaStackId].Building.Annexes[3].UnitID = stoneAnnexId;
    for (int i = 0; i < 4; i++) {
      civ
          .Units[feitoriaStackId]
          .Building.Annexes[i]
          .Misplacement = {0.5, 0.5};
    }

    // dead unit
    civ.Units[feitoriaDeadStackId].Type = 80;
    civ.Units[feitoriaDeadStackId].Class = 3;
    civ.Units[feitoriaDeadStackId].DyingGraphic = 42;
    civ.Units[feitoriaDeadStackId].UndeadGraphic = -1;
    civ.Units[feitoriaDeadStackId].HitPoints = 0;
    civ.Units[feitoriaDeadStackId].ResourceDecay = 1;
    civ.Units[feitoriaDeadStackId].Enabled = 0;
    civ.Units[feitoriaDeadStackId].HideInEditor = 1;
    civ.Units[feitoriaDeadStackId].DeadUnitID = 148; // Rubble 8 x 8

    // CRASH -> INVALID NAMES for new units ?

    // annex
    genie::Unit annex;
    annex.Type = 80;
    annex.Class = 3;
    annex.HitPoints = -1;
    annex.HideInEditor = 1;
    // wood
    annex.DeadUnitID = woodTrickleId;
    civ.Units[woodAnnexId] = annex;
    // food
    annex.DeadUnitID = foodTrickleId;
    civ.Units[foodAnnexId] = annex;
    // gold
    annex.DeadUnitID = goldTrickleId;
    civ.Units[goldAnnexId] = annex;
    // stone
    annex.DeadUnitID = stoneTrickleId;
    civ.Units[stoneAnnexId] = annex;

    // trickle
    genie::Unit trickle;
    trickle.Type = 30;
    trickle.Class = 11;
    trickle.HitPoints = 0;
    trickle.ResourceDecay = 1;
    trickle.CanBeBuiltOn = 1;
    trickle.HideInEditor = 1;
    trickle.ResourceStorages[0].Type = 12; // Corpse Decay Time
    trickle.ResourceStorages[0].Amount = feitoriaCooldown; // time it takes to generate ressources again
    trickle.ResourceStorages[0].Paid = 0; // Decayable ressource
    trickle.ResourceStorages[1].Paid = 1; // Stored after death also
    // wood
    trickle.ResourceStorages[1].Type = 0; // Wood storage
    trickle.ResourceStorages[1].Amount = woodAdded;
    trickle.DeadUnitID = woodTrickleId;
    civ.Units[woodTrickleId] = trickle;
    // food
    trickle.ResourceStorages[1].Type = 1; // Food storage
    trickle.ResourceStorages[1].Amount = foodAdded;
    trickle.DeadUnitID = foodTrickleId;
    civ.Units[foodTrickleId] = trickle;
    // gold
    trickle.ResourceStorages[1].Type = 3; // Gold storage
    trickle.ResourceStorages[1].Amount = goldAdded;
    trickle.DeadUnitID = goldTrickleId;
    civ.Units[goldTrickleId] = trickle;
    // stone
    trickle.ResourceStorages[1].Type = 2; // Stone storage
    trickle.ResourceStorages[1].Amount = stoneAdded;
    trickle.DeadUnitID = stoneTrickleId;
    civ.Units[stoneTrickleId] = trickle;

    // Fix IDs
    civ.Units[feitoriaStackId].ID = feitoriaStackId;
    civ.Units[woodAnnexId].ID = woodAnnexId;
    civ.Units[foodAnnexId].ID = foodAnnexId;
    civ.Units[goldAnnexId].ID = goldAnnexId;
    civ.Units[stoneAnnexId].ID = stoneAnnexId;
    civ.Units[woodTrickleId].ID = woodTrickleId;
    civ.Units[foodTrickleId].ID = foodTrickleId;
    civ.Units[goldTrickleId].ID = goldTrickleId;
    civ.Units[stoneTrickleId].ID = stoneTrickleId;

    civ.Units[feitoriaStackId].CopyID = feitoriaStackId;
    civ.Units[woodAnnexId].CopyID = woodAnnexId;
    civ.Units[foodAnnexId].CopyID = foodAnnexId;
    civ.Units[goldAnnexId].CopyID = goldAnnexId;
    civ.Units[stoneAnnexId].CopyID = stoneAnnexId;
    civ.Units[woodTrickleId].CopyID = woodTrickleId;
    civ.Units[foodTrickleId].CopyID = foodTrickleId;
    civ.Units[goldTrickleId].CopyID = goldTrickleId;
    civ.Units[stoneTrickleId].CopyID = stoneTrickleId;

    civ.Units[feitoriaStackId].BaseID = feitoriaStackId;
    civ.Units[woodAnnexId].BaseID = woodAnnexId;
    civ.Units[foodAnnexId].BaseID = foodAnnexId;
    civ.Units[goldAnnexId].BaseID = goldAnnexId;
    civ.Units[stoneAnnexId].BaseID = stoneAnnexId;
    civ.Units[woodTrickleId].BaseID = woodTrickleId;
    civ.Units[foodTrickleId].BaseID = foodTrickleId;
    civ.Units[goldTrickleId].BaseID = goldTrickleId;
    civ.Units[stoneTrickleId].BaseID = stoneTrickleId;
  }
}

DatPatch feitoriaFix = {&feitoriaPatch, "Feitoria fix"};

} // namespace wololo
