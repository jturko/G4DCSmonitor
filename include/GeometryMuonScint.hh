#ifndef GEOMETRYMUONSCINT_HH
#define GEOMETRYMUONSCINT_HH 1

#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"
#include <vector>

class G4LogicalVolume;
class G4VPhysicalVolume;
class G4OpticalSurface;
class G4Material;

struct SiPMSpec {
    G4int    edge   = 0;
    G4double uAlong = 0.0;
    G4double vAlong = 0.0;
};

enum ReflectorType {
    kNoReflector  = 0,
    kPaintTiO2    = 1,
    kAluminumFoil = 2,
    kGlossyPaper  = 3,
    kTeflon       = 4
};

class GeometryMuonScint
{
  public:
    GeometryMuonScint();
    ~GeometryMuonScint();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* worldLV,
                        const G4ThreeVector& pos,
                        G4RotationMatrix*    rot,
                        G4int                copyNo);

    // -------- setters ------------------------------------------------------
    void SetSlabSize  (G4ThreeVector hxyz)        { fHalfSize = hxyz/2.; }
    void SetSlabMaterialName(const G4String& n)   { fScintMatName = n; }

    void SetReflectorType(ReflectorType t)        { fReflectorType = t; }
    void SetReflectorWrap(G4bool wrap)            { fWrapWithReflector = wrap; }

    void SetSiPMSize(G4double sx, G4double sy)    { fSiPMHalfX = 0.5*sx; fSiPMHalfY = 0.5*sy; }
    void AddSiPM(const SiPMSpec& s)               { fSiPMs.push_back(s); }
    void ClearSiPMs()                             { fSiPMs.clear(); }

    void SetCouplingThickness(G4double t)         { fCouplingHalfZ = 0.5 * t; }

    // Presets
    void PresetConfig_8SiPM_AllSides();
    void PresetConfig_4SiPM_OnePerSide();
    void PresetConfig_2SiPM_OneSide();
    void PresetConfig_4SiPM_Corners();

    // Accessors
    G4LogicalVolume*   GetScintLV()  const { return fScintLV; }
    G4VPhysicalVolume* GetScintPV()  const { return fScintPV; }
    G4LogicalVolume*   GetSiPMLV()   const { return fSiPMLV; }
    G4LogicalVolume*   GetGreaseLV() const { return fGreaseLV; }
    G4int              GetNumSiPMs() const { return (G4int)fSiPMs.size(); }

  private:
    void BuildOpticalProperties(G4Material* scintMat);
    void BuildOpticalSurfaces();
    void PlaceSiPMs(G4LogicalVolume* worldLV,
                    const G4ThreeVector& slabPos,
                    G4RotationMatrix* slabRot,
                    G4int copyNo);

    // dims (half-lengths). Default 100x100x5 mm = 200x200x10 mm slab.
    G4ThreeVector fHalfSize { 100.*CLHEP::mm, 100.*CLHEP::mm, 5.*CLHEP::mm };

    // SiPM half-sizes (4 x 4 x 0.6 mm³)
    G4double fSiPMHalfX = 2.0 * CLHEP::mm;
    G4double fSiPMHalfY = 2.0 * CLHEP::mm;
    G4double fSiPMHalfZ = 0.3 * CLHEP::mm;

    // Coupling grease half-thickness (along the edge normal)
    G4double fCouplingHalfZ = 0.05 * CLHEP::mm;   // 0.1 mm pad

    std::vector<SiPMSpec> fSiPMs;

    // Materials
    G4String fScintMatName = "EJ200";

    // Reflector
    ReflectorType fReflectorType     = kPaintTiO2;
    G4bool        fWrapWithReflector = true;

    // Built objects
    G4LogicalVolume*   fScintLV   = nullptr;
    G4VPhysicalVolume* fScintPV   = nullptr;
    G4LogicalVolume*   fGreaseLV  = nullptr;
    G4LogicalVolume*   fSiPMLV    = nullptr;

    G4OpticalSurface*  fReflSurface     = nullptr;  // wrapping (skin)
    G4OpticalSurface*  fCouplingSurface = nullptr;  // slab<->grease override (just transparent)
    G4OpticalSurface*  fSiPMSurface     = nullptr;  // grease<->SiPM detector boundary
};

#endif

