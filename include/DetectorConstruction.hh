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
class GeometryHemiShield;
class GeometryPlastic;
class GeometryCASTOR440;
class GeometryHall;

class DetectorConstruction : public G4VUserDetectorConstruction
{
    public:
        DetectorConstruction(G4String biasParticle);
        ~DetectorConstruction() override;

    public:
        G4VPhysicalVolume* Construct() override;
        void ConstructSDandField() override;

    public:

        const G4VPhysicalVolume* GetWorld() { return fPWorld; }

        // Biasing
        void  SetUseBiasing(G4bool val) { fUseBiasing = val; }
        G4bool GetUseBiasing() const    { return fUseBiasing; }
        G4bool GetUseBiasingFromCLI() const    { return fUseBiasingFromCLI; }

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
        void SetCLYCCasingThickness(G4double val);

        void SetCLYCLiFCollimatorInnerRadius(G4double val);
        void SetCLYCLiFCollimatorOuterRadius(G4double val);
        void SetCLYCLiFCollimatorLength(G4double val);

        void SetCLYCPbCollimatorInnerRadius(G4double val);
        void SetCLYCPbCollimatorOuterRadius(G4double val);
        void SetCLYCPbCollimatorLength(G4double val);

        void SetCLYCPECollimatorInnerRadius(G4double val);
        void SetCLYCPECollimatorOuterRadius(G4double val);
        void SetCLYCPECollimatorLength(G4double val);

        void SetCLYCPEPlugLipRadius(G4double val);
        void SetCLYCPEPlugInnerRadius(G4double val);
        void SetCLYCPEPlugLipLength(G4double val);
        void SetCLYCPEPlugInnerLength(G4double val);

        void SetCLYCCrystalMaterialName(G4String val);
        void SetCLYCCasingMaterialName(G4String val);
        void SetCLYCLiFColMaterialName(G4String val);
        void SetCLYCPbColMaterialName(G4String val);
        void SetCLYCPEColMaterialName(G4String val);
        void SetCLYCPEPlugMaterialName(G4String val);

        // Plastic
        // Plastic (shadowed)
        void  AddPlastic();
        void  AddPlasticByCrystalCenter();
        G4int GetNumPlastic() const { return (G4int)fPlasticDetectors.size(); }

        void SetPlasticCrystalRadius(G4double v);
        void SetPlasticCrystalLength(G4double v);
        void SetPlasticCasingThickness(G4double v);
        void SetPlasticPbColInnerRadius(G4double v);
        void SetPlasticPbColOuterRadius(G4double v);
        void SetPlasticPbColLength(G4double v);
        void SetPlasticPEColInnerRadius(G4double v);
        void SetPlasticPEColOuterRadius(G4double v);
        void SetPlasticPEColLength(G4double v);
        void SetPlasticLiFColInnerRadius(G4double v);
        void SetPlasticLiFColOuterRadius(G4double v);
        void SetPlasticLiFColLength(G4double v);
        void SetPlasticShadowStandoff(G4double v);
        void SetPlasticShadowRadiusDet(G4double v);
        void SetPlasticShadowRadiusSrc(G4double v);
        void SetPlasticShadowBackLength(G4double v);
        void SetPlasticShadowBodyLength(G4double v);
        void SetPlasticShadowFrontLength(G4double v);
        void SetPlasticSnoutInnerRadius(G4double v);
        void SetPlasticSnoutOuterRadius(G4double v);
        void SetPlasticSnoutLength(G4double v);
        void SetPlasticBackShieldRadius(G4double v);
        void SetPlasticBackShieldLength(G4double v);
        void SetPlasticSideShieldInnerRadius(G4double v);
        void SetPlasticSideShieldOuterRadius(G4double v);
        void SetPlasticSideShieldLength(G4double v);
        void SetPlasticCrystalMaterialName(G4String v);
        void SetPlasticCasingMaterialName(G4String v);
        void SetPlasticPbColMaterialName(G4String v);
        void SetPlasticPEColMaterialName(G4String v);
        void SetPlasticLiFColMaterialName(G4String v);
        void SetPlasticShadowBackMaterialName(G4String v);
        void SetPlasticShadowBodyMaterialName(G4String v);
        void SetPlasticShadowFrontMaterialName(G4String v);
        void SetPlasticSnoutMaterialName(G4String v);
        void SetPlasticBackShieldMaterialName(G4String v);
        void SetPlasticSideShieldMaterialName(G4String v);

        // HemiShield
        void AddHemiShield();
        void SetHemiCavityRadius(G4double v);
        void SetHemiGammaThickness(G4double v);
        void SetHemiLinerThickness(G4double v);
        void SetHemiPEThickness(G4double v);
        void SetHemiFacePEThickness(G4double v);
        void SetHemiBoreRadius(G4double v);
        void SetHemiBoreOffsetY(G4double v);
        void SetHemiBoronMassFraction(G4double v);
        void SetHemiGammaMaterialName(G4String v);
        void SetHemiLinerMaterialName(G4String v);
        void SetHemiFacePEMaterialName(G4String v);
        void SetHemiGammaCollimatorThickness(G4double v);
        void SetHemiGammaCollimatorDiameter(G4double v);
        void SetHemiGammaCollimatorMaterialName(G4String v);

        // CASTOR 440
        void AddCASTOR440();
        const std::vector<G4LogicalVolume*> GetLCASTOR440s() { return fLCASTOR440s; }

        // Experimental hall
        void AddHall();
        void SetHallLength(G4double v);
        void SetHallFloorTopZ(G4double v);
        void SetHallFloorThickness(G4double v);
        void SetHallWallInnerY(G4double v);
        void SetHallWallThickness(G4double v);
        void SetHallWallHeight(G4double v);
        void SetHallCeilingThickness(G4double v);
        void SetHallUseFloor(G4bool v);
        void SetHallUseWalls(G4bool v);
        void SetHallUseCeiling(G4bool v);
        void SetHallFloorMaterialName(G4String v);
        void SetHallWallMaterialName(G4String v);
        void SetHallCeilingMaterialName(G4String v);


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

        G4double GetWorldSize() const { return fWorldXYZ; }

        // meta scan-labels, applied to SUBSEQUENTLY added detectors
        void SetMetaScanPhi(G4double v) { fMetaScanPhiDeg = v; }   // deg
        void SetMetaScanZ  (G4double v) { fMetaScanZmm    = v; }   // internal (mm)
        void ClearMetaScan();

        // global crystal centre of a plastic detector (post-normalisation)
        G4ThreeVector GetPlasticCrystalPosition(G4int index) const;

        struct DetMeta {
            G4int         id;
            G4String      type;            // "CLYC" | "plastic"
            G4int         typeIndex;
            G4String      placementMode;   // "frontFace" | "crystalCenter"
            G4ThreeVector anchorPos;       // macro-set position
            G4ThreeVector rotDeg;          // macro-set rotation (raw degrees)
            G4ThreeVector crystalCenter;   // actual global crystal centre
            G4double      scanPhiDeg;      // -9999 if unset
            G4double      scanZmm;         // -9999 if unset
        };
        std::vector<DetMeta> BuildDetectorMeta() const;


    private:
        DetectorMessenger* fDetectorMessenger = nullptr;

        G4bool   fUseBiasingFromCLI = false;
        
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

        std::vector<GeometryPlastic*>     fPlasticDetectors;
        std::vector<G4ThreeVector>        fPlasticPositions;
        std::vector<G4RotationMatrix*>    fPlasticRotations;
        std::vector<G4bool>               fPlasticPlaceByCrystalCenter;

        std::vector<GeometryCASTOR440*>   fCASTOR440Detectors;
        std::vector<G4ThreeVector>        fCASTOR440Positions;
        std::vector<G4RotationMatrix*>    fCASTOR440Rotations;
        std::vector<G4LogicalVolume*>     fLCASTOR440s;
        
        std::vector<GeometryHemiShield*>  fHemiShieldDetectors;
        std::vector<G4ThreeVector>        fHemiShieldPositions;
        std::vector<G4RotationMatrix*>    fHemiShieldRotations;

        std::vector<GeometryHall*>        fHallDetectors;
        std::vector<G4ThreeVector>        fHallPositions;
        std::vector<G4RotationMatrix*>    fHallRotations;


    private:
        void DefineMaterials();
        G4VPhysicalVolume* ConstructVolumes();

        // meta bookkeeping (captured at add time, never mutated)
        G4double fMetaScanPhiDeg = -9999.;
        G4double fMetaScanZmm    = -9999.;

        std::vector<G4ThreeVector> fCLYCAnchorPos;
        std::vector<G4ThreeVector> fCLYCRotDeg;
        std::vector<G4double>      fCLYCScanPhiDeg;
        std::vector<G4double>      fCLYCScanZmm;

        std::vector<G4ThreeVector> fPlasticAnchorPos;
        std::vector<G4ThreeVector> fPlasticRotDeg;
        std::vector<G4double>      fPlasticScanPhiDeg;
        std::vector<G4double>      fPlasticScanZmm;


};

#endif

