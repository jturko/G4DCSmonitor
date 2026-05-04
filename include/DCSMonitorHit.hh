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
//
/// \file B2/B2a/include/TrackerHit.hh
/// \brief Definition of the B2::TrackerHit class

#ifndef DCSMonitorHit_h
#define DCSMonitorHit_h 1

#include "G4Allocator.hh"
#include "G4THitsCollection.hh"
#include "G4Threading.hh"
#include "G4ThreeVector.hh"
#include "G4VHit.hh"

/// Tracker hit class
///
/// It defines data members to store the trackID, chamberNb, energy deposit,
/// and position of charged particles in a selected volume:
/// - fTrackID, fChamberNB, fEdep, fPos

class DCSMonitorHit : public G4VHit
{
    public:
        DCSMonitorHit() = default;
        DCSMonitorHit(const DCSMonitorHit&) = default;
        ~DCSMonitorHit() override = default;

        // operators
        DCSMonitorHit& operator=(const DCSMonitorHit&) = default;
        G4bool operator==(const DCSMonitorHit&) const;

        inline void* operator new(size_t);
        inline void operator delete(void*);

        // methods from base class
        void Draw() override;
        void Print() override;

        // Set methods
        void SetTrackID(G4int track) { fTrackID = track; };
        void SetEdep(G4double de) { fEdep = de; };
        void AddEdep(G4double de) { fEdep += de; };
        void SetPos(G4ThreeVector xyz) { fPos = xyz; };
        void SetTime(G4double t) { fTime = t; };
        void SetPID(G4int pid) { fPID = pid; };
        void SetDetNum(G4int n){ fDetNum = n; };
        void SetWeight(G4double w){ fWeight = w; };

        // Get methods
        G4int GetTrackID() const { return fTrackID; };
        G4double GetEdep() const { return fEdep; };
        G4ThreeVector GetPos() const { return fPos; };
        G4double GetTime() const { return fTime; };
        G4int GetPID() const { return fPID; };
        G4int GetDetNum() const { return fDetNum; };
        G4double GetWeight() const { return fWeight; };

    private:
        G4int           fTrackID = -1;
        G4double        fEdep = 0.;
        G4ThreeVector   fPos;
        G4double        fTime = 0.;
        G4int           fPID = -1;
        G4int           fDetNum = -1;
        G4double        fWeight = 1.0;

};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

using DCSMonitorHitsCollection = G4THitsCollection<DCSMonitorHit>;

extern G4ThreadLocal G4Allocator<DCSMonitorHit>* DCSMonitorHitAllocator;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

inline void* DCSMonitorHit::operator new(size_t)
{
    if (!DCSMonitorHitAllocator) DCSMonitorHitAllocator = new G4Allocator<DCSMonitorHit>;
    return (void*)DCSMonitorHitAllocator->MallocSingle();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

inline void DCSMonitorHit::operator delete(void* hit)
{
    DCSMonitorHitAllocator->FreeSingle((DCSMonitorHit*)hit);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
