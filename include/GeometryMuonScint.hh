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

// SiPM placement specifier.
//  edge:    0=+X, 1=-X, 2=+Y, 3=-Y    (the four narrow side faces of the slab)
//  uAlong:  position along the long axis of the edge in [-1, +1]
//  vAlong:  position along the thickness axis in [-1, +1] (usually 0 = centred)
struct SiPMSpec {
    G4int    edge   = 0;
    G4double uAlong = 0.0;
    G4double vAlong = 0.0;
};

enum ReflectorType {
    kNoReflector  = 0,
    kPaintTiO2    = 1,   // EJ-510 TiO2: Lambertian, ~95-98% reflectivity
    kAluminumFoil = 2,   // shiny side: mostly specular, ~88-92%
    kGlossyPaper  = 3,   // mixed, ~95% with sigma_alpha ~ 0.1 rad
    kTeflon       = 4    // PTFE wrap, ~99% Lambertian
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
    void SetSlabSize  (G4ThreeVector hxyz)        { fHalfSize = hxyz/2.; }   // half-lengths
    void SetSlabMaterialName(const G4String& n)   { fScintMatName = n; }

    void SetReflectorType(ReflectorType t)        { fReflectorType = t; }
    void SetReflectorWrap(G4bool wrap)            { fWrapWithReflector = wrap; }

    void SetSiPMSize(G4double sx, G4double sy)    { fSiPMHalfX = 0.5*sx; fSiPMHalfY = 0.5*sy; }
    void AddSiPM(const SiPMSpec& s)               { fSiPMs.push_back(s); }
    void ClearSiPMs()                             { fSiPMs.clear(); }

    // Convenience preset SiPM layouts (Configs 1..3 of Rao 2025)
    void PresetConfig_8SiPM_AllSides();      // Config 1
    void PresetConfig_4SiPM_OnePerSide();    // Config 2
    void PresetConfig_2SiPM_OneSide();       // Config 3
    void PresetConfig_4SiPM_Corners();       // 4 corners of the +X/-X edges

    // Accessors
    G4LogicalVolume*   GetScintLV()   const { return fScintLV; }
    G4VPhysicalVolume* GetScintPV()   const { return fScintPV; }
    G4LogicalVolume*   GetSiPMLV()    const { return fSiPMLV; }
    G4int              GetNumSiPMs()  const { return (G4int)fSiPMs.size(); }

  private:
    void BuildOpticalProperties(G4Material* scintMat);
    void BuildOpticalSurfaces();
    void PlaceSiPMs(G4LogicalVolume* parentLV);

    // dims (half-lengths). Default 200 x 200 x 10 mm^3.
    G4ThreeVector fHalfSize { 100.*CLHEP::mm, 100.*CLHEP::mm, 5.*CLHEP::mm };

    // SiPM half-sizes (4 x 4 mm^2 active face, 0.6 mm thick)
    G4double fSiPMHalfX = 2.0 * CLHEP::mm;
    G4double fSiPMHalfY = 2.0 * CLHEP::mm;
    G4double fSiPMHalfZ = 0.3 * CLHEP::mm;

    std::vector<SiPMSpec> fSiPMs;

    // Materials
    G4String fScintMatName = "EJ200";

    // Reflector
    ReflectorType fReflectorType     = kPaintTiO2;
    G4bool        fWrapWithReflector = true;

    // Built objects
    G4LogicalVolume*   fScintLV     = nullptr;
    G4VPhysicalVolume* fScintPV     = nullptr;
    G4LogicalVolume*   fSiPMLV      = nullptr;
    G4OpticalSurface*  fReflSurface = nullptr;

    G4OpticalSurface*  fSiPMSurface = nullptr;   // perfect-absorber boundary

};

#endif









//      #ifndef GEOMETRYMUONSCINT_HH
//      #define GEOMETRYMUONSCINT_HH 1
//      
//      #include "G4ThreeVector.hh"
//      #include "G4RotationMatrix.hh"
//      #include "globals.hh"
//      #include <vector>
//      
//      class G4LogicalVolume;
//      class G4VPhysicalVolume;
//      class G4AssemblyVolume;
//      class G4OpticalSurface;
//      class G4Material;
//      
//      // SiPM placement specifier.
//      //  edge:    0=+X, 1=-X, 2=+Y, 2=-Y    (the four 1 x L cm^2 narrow side faces, 
//      //                                      the slab is L x W x T with T as thickness)
//      //  uAlong:  position along the long axis of the edge in [-1, +1]   (-1 = "near" corner, +1 = "far")
//      //  vAlong:  position along the thickness axis in [-1, +1]          (usually 0 = centred)
//      struct SiPMSpec {
//          G4int    edge   = 0;
//          G4double uAlong = 0.0;
//          G4double vAlong = 0.0;
//      };
//      
//      enum ReflectorType {
//          kNoReflector  = 0,
//          kPaintTiO2    = 1,   // EJ-510 TiO2: Lambertian, ~95-98% reflectivity
//          kAluminumFoil = 2,   // shiny side: mostly specular, ~88-92%
//          kGlossyPaper  = 3,   // mixed, ~95% with sigma_alpha ~ 0.1 rad
//          kTeflon       = 4    // bonus: PTFE wrap, ~99% Lambertian
//      };
//      
//      class GeometryMuonScint
//      {
//        public:
//          GeometryMuonScint();
//          ~GeometryMuonScint();
//      
//          G4int Build();
//          void  PlaceDetector(G4LogicalVolume* worldLV,
//                              const G4ThreeVector& pos,
//                              G4RotationMatrix*    rot,
//                              G4int                copyNo);
//      
//          // -------- setters (all PreInit, applied to the slab being constructed) ----
//          void SetSlabSize  (G4ThreeVector hxyz)   { fHalfSize = hxyz/2.; }   // half-lengths
//          void SetSlabMaterialName(const G4String& n) { fScintMatName = n; }
//      
//          void SetReflectorType(ReflectorType t)   { fReflectorType = t; }
//          void SetReflectorWrap(G4bool wrap)       { fWrapWithReflector = wrap; }
//      
//          void SetSiPMSize(G4double sx, G4double sy){ fSiPMHalfX = 0.5*sx; fSiPMHalfY = 0.5*sy; }
//          void AddSiPM(const SiPMSpec& s)          { fSiPMs.push_back(s); }
//          void ClearSiPMs()                        { fSiPMs.clear(); }
//      
//          // Convenience preset SiPM layouts (Configs 1..3 of Rao 2025)
//          void PresetConfig_8SiPM_AllSides();      // Config 1
//          void PresetConfig_4SiPM_OnePerSide();    // Config 2
//          void PresetConfig_2SiPM_OneSide();       // Config 3
//          void PresetConfig_4SiPM_Corners();       // 4 corners of one big face's edges
//      
//          // Accessors
//          G4LogicalVolume* GetScintLV() const      { return fScintLV; }
//          G4int            GetNumSiPMs() const     { return (G4int)fSiPMs.size(); }
//          G4LogicalVolume* GetSiPMLV()  const      { return fSiPMLV; }   // shared LV for all SiPMs
//      
//        private:
//          void BuildOpticalProperties(G4Material* scintMat);
//          void BuildOpticalSurfaces();
//          void PlaceSiPMs(G4LogicalVolume* parentLV);
//          void WrapWithReflectorSurface(G4VPhysicalVolume* slabPV);
//      
//          // dims (defaults: 200 x 200 x 10 mm^3 -- close to 250x250x10 in Rao 2025; 
//          //                 user can scale to 200x200x10 cm^3 trigger size)
//          G4ThreeVector fHalfSize;     // (Lx/2, Ly/2, Lz/2)
//      
//          // Defaults: assume the long slab axes are X and Y, thickness is Z.
//          // The four narrow side faces are then 2*Ly x 2*Lz at +-X, and 2*Lx x 2*Lz at +-Y.
//      
//          // SiPM
//          G4double fSiPMHalfX = 2.0 * CLHEP::mm;     // 4 x 4 mm^2 active area
//          G4double fSiPMHalfY = 2.0 * CLHEP::mm;
//          G4double fSiPMHalfZ = 0.3 * CLHEP::mm;     // dummy thickness for the volume
//      
//          std::vector<SiPMSpec> fSiPMs;
//      
//          // Materials
//          G4String fScintMatName = "EJ200";    // we'll register this in BuildOpticalProperties
//      
//          // Reflector
//          ReflectorType fReflectorType    = kPaintTiO2;
//          G4bool        fWrapWithReflector = true;
//      
//          // Built objects
//          G4AssemblyVolume* fAssembly = nullptr;
//          G4LogicalVolume*  fScintLV  = nullptr;
//          G4LogicalVolume*  fSiPMLV   = nullptr;
//          G4OpticalSurface* fReflSurface = nullptr;
//      };
//      
//      #endif
//      
