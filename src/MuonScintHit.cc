//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "MuonScintHit.hh"

#include "G4Circle.hh"
#include "G4Colour.hh"
#include "G4UnitsTable.hh"
#include "G4VVisManager.hh"
#include "G4VisAttributes.hh"

#include <iomanip>

G4ThreadLocal G4Allocator<MuonScintHit>* MuonScintHitAllocator = nullptr;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4bool MuonScintHit::operator==(const MuonScintHit& right) const
{
    return (this == &right);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void MuonScintHit::Draw()
{
    //G4VVisManager* pVVisManager = G4VVisManager::GetConcreteInstance();
    //if (pVVisManager && fNDetected > 0) {
    //    G4Circle circle(fPosFirst);
    //    circle.SetScreenSize(3.);
    //    circle.SetFillStyle(G4Circle::filled);
    //    G4VisAttributes attribs(G4Colour::Yellow());
    //    circle.SetVisAttributes(attribs);
    //    pVVisManager->Draw(circle);
    //}
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void MuonScintHit::Print()
{
    G4cout << "  det "  << fDetNum
           << "  sipm " << fSiPMNum
           << "  nDet " << std::setw(5) << fNDetected
           << "  nInc " << std::setw(5) << fNIncident
           << "  tFirst " << std::setw(7) << G4BestUnit(fTFirst, "Time")
           << "  <lam> "  << std::setw(6) << GetMeanWavelength() << " nm"
           << "  posFirst " << std::setw(7) << G4BestUnit(fPosFirst, "Length")
           << "  w " << std::setw(7) << fWeight << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

