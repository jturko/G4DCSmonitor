#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"
#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"

class G4LogicalVolume;
class G4Material;

class DetectorMessenger;
class GeometryCLYC;
class GeometryCASTOR440;
class GeometryMuonScint;

class DetectorConstruction : public G4VUserDetectorConstruction
{
    public:
        DetectorConstruction();
        ~DetectorConstruction() override;

    public:
        G4VPhysicalVolume* Construct() override;
        void ConstructSDandField() override;

    public:

        const G4VPhysicalVolume* GetWorld() { return fPWorld; }

        // Biasing
        void  SetUseBiasing(G4bool val) { fUseBiasing = val; }
        G4bool GetUseBiasing() const    { return fUseBiasing; }

        void     SetNShells(G4int n)               { fNShells = n; }
        G4int    GetNShells() const                { return fNShells; }

        void     SetBiasingInnerRadius (G4double r){ fBiasRMin = r; }
        void     SetBiasingOuterRadius (G4double r){ fBiasRMax = r; }
        void     SetBiasingInnerHeight (G4double h){ fBiasHMin = h; }
        G4double GetBiasingInnerRadius () const    { return fBiasRMin; }
        G4double GetBiasingOuterRadius () const    { return fBiasRMax; }
        G4double GetBiasingInnerHeight () const    { return fBiasHMin; }

        // Placement
        void SetPosition(G4ThreeVector pos) { fPosition = pos; }
        void SetRotation(G4ThreeVector rot) { fRotation = rot; }

        // CLYC
        void  AddCLYC();
        void  AddCLYCByCrystalCenter();
        G4int GetNumCLYC() const                          { return fCLYCDetectors.size(); }
        G4ThreeVector GetCLYCPosition(G4int index) const  { return fCLYCPositions[index]; } // global position of CLYC assembly (front face of PE plug)
        G4ThreeVector GetCLYCCrystalPosition(G4int index) const; // global position of CLYC crystal center

        void SetCLYCCrystalRadius(G4double val);
        void SetCLYCCrystalLength(G4double val);
        void SetCLYCAlumThickness(G4double val);

        void SetCLYCLiFCollimatorInnerRadius(G4double val);
        void SetCLYCLiFCollimatorOuterRadius(G4double val);
        void SetCLYCLiFCollimatorLength(G4double val);

        void SetCLYCPbCollimatorInnerRadius(G4double val);
        void SetCLYCPbCollimatorOuterRadius(G4double val);
        void SetCLYCPbCollimatorLength(G4double val);

        void SetCLYCPEHDCollimatorInnerRadius(G4double val);
        void SetCLYCPEHDCollimatorOuterRadius(G4double val);
        void SetCLYCPEHDCollimatorLength(G4double val);

        void SetCLYCPEPlugLipRadius(G4double val);
        void SetCLYCPEPlugInnerRadius(G4double val);
        void SetCLYCPEPlugLipLength(G4double val);
        void SetCLYCPEPlugInnerLength(G4double val);

        void SetCLYCCrystalMaterialName(G4String val);
        void SetCLYCAlumMaterialName(G4String val);
        void SetCLYCLiFMaterialName(G4String val);
        void SetCLYCPbMaterialName(G4String val);
        void SetCLYCPEHDMaterialName(G4String val);

        // CASTOR 440
        void AddCASTOR440();
        const std::vector<G4LogicalVolume*> GetLCASTOR440s() { return fLCASTOR440s; }

        G4int GetNumCASTOR440s() const { return fCASTOR440Detectors.size(); }
        G4ThreeVector     GetCASTOR440Position(G4int index) const { return fCASTOR440Positions[index]; }
        G4RotationMatrix* GetCASTOR440Rotation(G4int index) const { return fCASTOR440Rotations[index]; }

        // Direct access to the underlying GeometryCASTOR440 instance (used by
        // the primary generator to query parameters such as active fuel
        // length, hex circumradius, fuel-assembly count, etc.).
        GeometryCASTOR440* GetCASTOR440(G4int index) const {
            if (index < 0 || index >= (G4int)fCASTOR440Detectors.size()) return nullptr;
            return fCASTOR440Detectors[index];
        }

        //// Legacy helper (kept for back-compat). pointInFuel is interpreted as
        //// a point in the fuel polyhedra local frame; the cavity Z offset is
        //// taken from the cask geometry so it stays in sync.
        //G4ThreeVector GetCASTOR440FuelGlobalPosition(G4int caskIndex,
        //                                             G4int fuelIndex,
        //                                             G4ThreeVector pointInFuel) const;

        // Sample a uniformly distributed global position inside the requested
        // fuel assembly of the requested cask. This is the single primitive
        // that should be used by ALL primary generators that need a vertex
        // inside a fuel assembly (biased and unbiased alike).
        G4ThreeVector SampleUniformGlobalPositionInFuel(G4int caskIndex,
                                                        G4int fuelIndex) const;




        // u-sim
        void  AddMuonScint();                         // place one at fPosition / fRotation
        G4int GetNumMuonScints() const                { return (G4int)fMuonScints.size(); }
        GeometryMuonScint* GetMuonScint(G4int i) const{ return (i>=0 && i<(G4int)fMuonScints.size()) ? fMuonScints[i] : nullptr; }

        void SetMuonScintSize(G4ThreeVector half_mm);
        void SetMuonScintReflector(G4int  type);      // 0..4 (enum mirror)
        void SetMuonScintWrap    (G4bool on);
        void SetMuonScintSiPMSize(G4double sx, G4double sy);
        void AddMuonScintSiPM    (G4int edge, G4double u, G4double v);
        void ClearMuonScintSiPMs ();
        void ApplyMuonScintPreset(G4int presetId);    // 1..4 (Configs)

    private:
        DetectorMessenger* fDetectorMessenger = nullptr;

        G4bool   fUseBiasing = false;
        G4int    fNShells    = 10;
        G4double fBiasRMin   =  900. * CLHEP::mm;
        G4double fBiasRMax   = 1270. * CLHEP::mm;
        G4double fBiasHMin   = 3500. * CLHEP::mm;

        G4ThreeVector fPosition;
        G4ThreeVector fRotation;

        G4VPhysicalVolume* fPWorld = nullptr;
        G4LogicalVolume*   fLWorld = nullptr;
        G4double           fWorldXYZ;

        std::vector<GeometryCLYC*>        fCLYCDetectors;
        std::vector<G4ThreeVector>        fCLYCPositions;
        std::vector<G4RotationMatrix*>    fCLYCRotations;
        std::vector<G4bool>               fCLYCPlaceByCrystalCenter;

        std::vector<GeometryCASTOR440*>   fCASTOR440Detectors;
        std::vector<G4ThreeVector>        fCASTOR440Positions;
        std::vector<G4RotationMatrix*>    fCASTOR440Rotations;
        std::vector<G4LogicalVolume*>     fLCASTOR440s;

        // u-sim
        std::vector<GeometryMuonScint*>   fMuonScints;
        std::vector<G4ThreeVector>        fMuonScintPositions;
        std::vector<G4RotationMatrix*>    fMuonScintRotations;


    private:
        void DefineMaterials();
        G4VPhysicalVolume* ConstructVolumes();
};

#endif

