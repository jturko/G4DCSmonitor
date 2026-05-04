//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file PhysicsList.cc
/// \brief Implementation of the PhysicsList class
//

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "PhysicsList.hh"

#include "GammaNuclearPhysics.hh"
#include "GammaNuclearPhysicsLEND.hh"

#include "G4HadronElasticPhysicsHP.hh"
#include "G4HadronElasticPhysicsXS.hh"
#include "G4HadronInelasticQBBC.hh"
#include "G4HadronPhysicsFTFP_BERT_HP.hh"
#include "G4HadronPhysicsINCLXX.hh"
#include "G4HadronPhysicsQGSP_BIC.hh"
#include "G4HadronPhysicsQGSP_BIC_AllHP.hh"
#include "G4HadronPhysicsQGSP_BIC_HP.hh"
#include "G4HadronPhysicsShielding.hh"
#include "G4IonElasticPhysics.hh"
#include "G4IonINCLXXPhysics.hh"
#include "G4IonPhysicsPHP.hh"
#include "G4IonPhysicsXS.hh"
#include "G4IonQMDPhysics.hh"
#include "G4ThermalNeutrons.hh"
#include "G4NuclideTable.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"

// particles

#include "G4BaryonConstructor.hh"
#include "G4BosonConstructor.hh"
#include "G4IonConstructor.hh"
#include "G4LeptonConstructor.hh"
#include "G4MesonConstructor.hh"
#include "G4ShortLivedConstructor.hh"

#include "G4EmStandardPhysics.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4HadronElasticPhysicsHP.hh"
#include "G4HadronPhysicsQGSP_BIC_HP.hh"
#include "G4IonElasticPhysics.hh"
#include "G4IonPhysicsXS.hh"
#include "GammaNuclearPhysics.hh"
#include "G4NuclideTable.hh"

#include "G4RadioactiveDecayPhysics.hh"

// importance biasing
#include "G4ParallelWorldPhysics.hh"
#include "G4ImportanceBiasing.hh"
#include "G4GeometrySampler.hh"

// region-specific production cuts in CLYC
#include "G4Region.hh"
#include "G4RegionStore.hh"


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PhysicsList::PhysicsList(G4bool useImportanceBiasing) : fUseImportanceBiasing(useImportanceBiasing)
{
    G4int verb = 1;
    SetVerboseLevel(verb);

    // EM physics
    //RegisterPhysics(new G4EmStandardPhysics());
    RegisterPhysics(new G4EmStandardPhysics_option4());
    
    // hadron elastic physics
    //RegisterPhysics(new G4HadronElasticPhysicsXS(verb));
    RegisterPhysics(new G4HadronElasticPhysicsHP(verb));
    
    // hadron inelastic physics
    RegisterPhysics(new G4HadronPhysicsQGSP_BIC_HP(verb));
    //RegisterPhysics(new G4HadronPhysicsQGSP_BIC_AllHP(verb));
    
    // thermal neutron scattering below 4 eV
    RegisterPhysics(new G4ThermalNeutrons());

    // elastic ion scattering
    RegisterPhysics(new G4IonElasticPhysics(verb));
    
    // other stuff
    RegisterPhysics(new G4IonPhysicsXS(verb));
    RegisterPhysics(new GammaNuclearPhysics("gamma"));
    RegisterPhysics(new G4RadioactiveDecayPhysics());
    
    if (fUseImportanceBiasing) {
        RegisterPhysics(new G4ParallelWorldPhysics("BiasingWorld"));

        fGammaSampler = new G4GeometrySampler("BiasingWorld", "gamma");
        fGammaSampler->SetParallel(true);
        RegisterPhysics(new G4ImportanceBiasing(fGammaSampler, "BiasingWorld"));

        fNeutronSampler = new G4GeometrySampler("BiasingWorld", "neutron");
        fNeutronSampler->SetParallel(true);
        RegisterPhysics(new G4ImportanceBiasing(fNeutronSampler, "BiasingWorld"));
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PhysicsList::~PhysicsList()
{
    delete fGammaSampler;
    delete fNeutronSampler;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PhysicsList::ConstructParticle()
{
    G4BosonConstructor pBosonConstructor;
    pBosonConstructor.ConstructParticle();

    G4LeptonConstructor pLeptonConstructor;
    pLeptonConstructor.ConstructParticle();

    G4MesonConstructor pMesonConstructor;
    pMesonConstructor.ConstructParticle();

    G4BaryonConstructor pBaryonConstructor;
    pBaryonConstructor.ConstructParticle();

    G4IonConstructor pIonConstructor;
    pIonConstructor.ConstructParticle();

    G4ShortLivedConstructor pShortLivedConstructor;
    pShortLivedConstructor.ConstructParticle();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PhysicsList::SetCuts()
{
    // Global cuts (coarse, appropriate for cask shielding)
    SetCutValue( 1.0 * mm, "gamma"  );
    SetCutValue(10.0 * mm, "e-"     );   
    SetCutValue(10.0 * mm, "e+"     );   
    SetCutValue( 1.0 * um, "proton" );

    // FIX: Fine cuts for the CLYC detector region
    G4Region* clycRegion = G4RegionStore::GetInstance()->GetRegion("CLYCRegion", false);
    if (clycRegion) {
        G4ProductionCuts* clycCuts = new G4ProductionCuts();
        clycCuts->SetProductionCut(0.1 * mm, "gamma");
        clycCuts->SetProductionCut(0.1 * mm, "e-");
        clycCuts->SetProductionCut(0.1 * mm, "e+");
        clycCuts->SetProductionCut(0.1 * mm, "proton");
        clycRegion->SetProductionCuts(clycCuts);
        G4cout << " [PhysicsList] Applied fine production cuts (0.1 mm) to CLYCRegion" << G4endl;
    }
}

//void PhysicsList::SetCuts()
//{
//    //SetCutValue(0. * mm, "proton");
//    //SetCutValue(1. * um, "proton");
//    
//    SetCutValue(1.0 * mm, "gamma");
//    SetCutValue(1.0 * mm, "e-");   
//    SetCutValue(1.0 * mm, "e+");   
//    SetCutValue(1.0 * um, "proton");
//
//}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
