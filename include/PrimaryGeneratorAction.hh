#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4ParticleGun.hh"
#include "G4GeneralParticleSource.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"

#include "THnSparse.h"
#include "TROOT.h"

class G4Event;
class DetectorConstruction;
class PrimaryGeneratorMessenger;

enum SourceMode {
    kGPS,
    kCASTOR440_surface,
    kCASTOR440_fuel,
    kCASTOR440_fuel_biased
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    PrimaryGeneratorAction(DetectorConstruction*);
    ~PrimaryGeneratorAction() override;

  public:
    void GeneratePrimaries(G4Event*) override;

    G4ParticleGun*           GetParticleGun() { return fParticleGun; }
    G4GeneralParticleSource* GetGPS()         { return fGPS; }

    // set the vertex generation mode
    void SetSourceMode(SourceMode mode);

    // setters for fuel generator modes to pick a cask/fuel assembly to sample positions from
    void SetCaskNum (G4int val) { fCaskNum  = val; }
    void SetFuelNum (G4int val) { fFuelNum  = val; }

    // setters for CLYC bounding sphere radius used in geometric biasing
    void     SetCLYCBoundingRadius(G4double r) { fCLYCBoundingRadius = r; }
    G4double GetCLYCBoundingRadius() const     { return fCLYCBoundingRadius; }

    // setters/getters for Watt spectrum generator
    void     SetUseWattSpectrum(G4bool v) { fUseWattSpectrum = v; }
    G4bool   GetUseWattSpectrum() const   { return fUseWattSpectrum; }
    void     SetWattA(G4double a)         { fWattA = a; }   // [energy]
    void     SetWattB(G4double b)         { fWattB = b; }   // [1/energy]
    G4double GetWattA() const             { return fWattA; }
    G4double GetWattB() const             { return fWattB; }
    G4double SampleWattSpectrum() const;

  private:
    G4ParticleGun*             fParticleGun = nullptr;
    G4GeneralParticleSource*   fGPS         = nullptr;
    DetectorConstruction*      fDetector    = nullptr;
    PrimaryGeneratorMessenger* fPrimaryGeneratorMessenger = nullptr;

    SourceMode fSourceMode;

    // source mode vertex generators
    void GenerateVertexCASTOR440SurfaceFlux(G4Event* event);            // CASTOR surface flux
    void GenerateVertexCASTOR440FuelFlux(G4Event* event);               // CASTOR fuel flux (isotropic)
    void GenerateVertexCASTOR440FuelFluxWithGeomBias(G4Event* event);   // CASTOR fuel flux (isotropic w. geom CLYC bias)

    // calculate/set the vertex position
    G4ThreeVector SetVertexPositionInFuel();

    // calculate/set the vertex momentum direction
    void          SetVertexDirectionIsotropic();
    G4double      SetVertexDirectionIsotropicWithGeomBias(const G4ThreeVector& vertexPos);

    // Source / fuel selection
    G4int    fCaskNum  = 0;
    G4int    fFuelNum  = 0;

    // Default bounding sphere radius used by the geometric bias
    G4double fCLYCBoundingRadius = 150.0 * mm;

    // Watt spectrum config. Defaults: U-235 thermal-fission Watt parameters.
    G4bool   fUseWattSpectrum = false;
    G4double fWattA = 0.988 * MeV;        // a
    G4double fWattB = 2.249 / MeV;        // b

};

#endif

