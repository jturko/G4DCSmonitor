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

    // Multiplicative scale on the scintillation light yield. Default 1.0 (the
    // physical 10,000 ph/MeV). Values <1 are intended ONLY for event-display
    // visualisation, where the ~2e4 photons/event otherwise render as a solid
    // blob; they must never be used for production runs.
    void SetLightYieldScale(G4double s)           { fLightYieldScale = s; }

    // Config 4 (Rao et al.): single SiPM read out through a WLS fibre clamp.
    // When enabled the four surges over the +X narrow face are replaced by a
    // black clamp bar holding four Y-11(200) WLS fibres read out by one SiPM.
    void SetUseWLSFibres(G4bool on)      { fUseWLSFibres = on; }
    G4bool UsesWLSFibres() const         { return fUseWLSFibres; }
    void SetNumWLSFibres(G4int n)        { fNFibres = n; }

    // Presets
    void PresetConfig_8SiPM_AllSides();
    void PresetConfig_4SiPM_OnePerSide();
    void PresetConfig_2SiPM_OneSide();
    void PresetConfig_4SiPM_Corners();
    void PresetConfig_WLSFibreClamp();   // Config 4

    // Accessors
    G4LogicalVolume*   GetScintLV()  const { return fScintLV; }
    G4VPhysicalVolume* GetScintPV()  const { return fScintPV; }
    G4ThreeVector      GetHalfSize() const { return fHalfSize; }
    G4LogicalVolume*   GetSiPMLV()   const { return fSiPMLV; }
    G4LogicalVolume*   GetGreaseLV() const { return fGreaseLV; }
    G4LogicalVolume*   GetHolderLV() const { return fHolderLV; }
    // Config 4 uses fNFibres *distinct* bent fibre volumes (each has its own
    // spread->bundle kink), so these return the full list rather than one LV.
    const std::vector<G4LogicalVolume*>& GetFibreLVs()     const { return fFibreCoreLVs; }
    const std::vector<G4LogicalVolume*>& GetFibreCladLVs() const { return fFibreCladLVs; }
    G4LogicalVolume*   GetClampLV()   const { return fClampLV; }
    G4LogicalVolume*   GetFibreGreaseLV() const { return fGrease4LV; }

    // True if lv is any Config-4 fibre core or cladding volume.
    G4bool IsFibreLV(const G4LogicalVolume* lv) const
    {
        for (auto* v : fFibreCoreLVs) if (v == lv) return true;
        for (auto* v : fFibreCladLVs) if (v == lv) return true;
        return false;
    }
    G4int              GetNumSiPMs() const { return (G4int)fSiPMs.size(); }

    // Set true by any geometry that instantiates WLS fibres, so the physics
    // list can auto-enable G4OpWLS before ConstructProcess (geometry is built
    // first, so the flag is reliable by then).
    static G4bool UsesWLSWorld() { return sHasWLSFibres; }

    // true when the bare (kNoReflector) configuration is enclosed by the
    // black 3D-printed holder that absorbs light at the non-instrumented faces.
    // The WLS fibre clamp (Config 4) replaces the +X SiPMs, so the bare holder
    // is not used there (it would also overlap the clamp body).
    G4bool UsesBlackHolder() const
    { return fReflectorType == kNoReflector && !fUseWLSFibres; }

  private:
    void BuildOpticalProperties(G4Material* scintMat);
    void BuildOpticalSurfaces();
    void BuildHolder();
    void BuildWLSFibres();
    void PlaceSiPMs(G4LogicalVolume* worldLV,
                    const G4ThreeVector& slabPos,
                    G4RotationMatrix* slabRot,
                    G4int copyNo);
    void PlaceWLSFibres(G4LogicalVolume* worldLV,
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

    // Scintillation light-yield scale (1.0 = physical). Viz-only override.
    G4double      fLightYieldScale   = 1.0;

    // Black holder dimensions. The cavity must enclose the plate AND the
    // grease/SiPM stack (protruding ~0.7 mm past the faces), so fHolderGap is
    // the clearance from the plate face to the holder wall.
    G4double fHolderGap      = 1.2 * CLHEP::mm;  // plate face -> holder wall
    G4double fHolderWall     = 2.0 * CLHEP::mm;  // holder wall thickness

    // ---- Config 4: WLS fibre clamp ----------------------------------------
    // A black 3D-printed/milled bar clamp sits on the -X narrow face (the side
    // opposite the edge/corner MCD regions). Its slab-facing face carries one
    // full-length channel per fibre. Four Y-11(200) fibres run the full 250 mm
    // plate length seated in those channels; a thin optical-grease film fills
    // the slab<->clamp gap in front of the channel mouths, and each fibre pokes
    // slightly out of its channel into that grease. The channel bottoms stop
    // short of the slab, so the grease stays a continuous layer. The fibres'
    // far (-y) ends are mirrored and their near (+y) ends are bundled onto a
    // single SiPM. A specular reflective layer covers the clamp side facing
    // away from the slab.
    G4bool fUseWLSFibres = false;
    G4bool fWLSOnNegX    = true;              // Config 4 clamp side: true = -X
    G4int  fNFibres      = 4;                 // four Ø1.0 mm fibres
    G4double fFibreRadius   = 0.5 * CLHEP::mm; // Ø1.0 mm core+clad
    G4double fFibreCladFrac = 0.03;            // cladding thickness fraction
    G4double fClampGap      = 0.10 * CLHEP::mm;// thin grease film slab<->clamp
    G4double fChannelRadius = 0.55 * CLHEP::mm;// holder channel radius (Ø1.1, Ø1.0 fibre + air gap)
    G4double fFibrePitch    = 1.50 * CLHEP::mm;// straight-section centre-to-centre pitch
    G4double fBundlePitch   = 1.00 * CLHEP::mm;// bundled pitch at SiPM (fibres touching)
    G4double fFibreConvergeLen = 15.0 * CLHEP::mm;// straight channels stop this far
                                                 // short of the SiPM; fibres bundle here
    G4double fFibreProtrusion = 0.05 * CLHEP::mm; // fibre proud of channel mouth
    G4double fClampWallZ    = 2.0 * CLHEP::mm; // clamp bar thickness (x)

    // Fibre read-out layout in the slab-local frame (set by BuildWLSFibres).
    G4double fFibreAxisX    = 0.;              // fibre centre x (slab-local)
    G4double fFibreZSpacing = 1.5 * CLHEP::mm; // straight-section z spacing

    // Built objects
    G4LogicalVolume*   fScintLV     = nullptr;
    G4VPhysicalVolume* fScintPV     = nullptr;
    G4LogicalVolume*   fGreaseLV    = nullptr;
    G4LogicalVolume*   fSiPMLV      = nullptr;
    G4LogicalVolume*   fHolderLV    = nullptr;
    std::vector<G4LogicalVolume*> fFibreCoreLVs;
    std::vector<G4LogicalVolume*> fFibreCladLVs;
    G4LogicalVolume*   fClampLV     = nullptr;
    G4LogicalVolume*   fGrease4LV   = nullptr;
    G4LogicalVolume*   fMirrorCapLV = nullptr;
    G4Material*        fClampMat    = nullptr;

    G4OpticalSurface*  fReflSurface     = nullptr;  // wrapping (skin)
    G4OpticalSurface*  fCouplingSurface = nullptr;  // slab<->grease override (just transparent)
    G4OpticalSurface*  fSiPMSurface     = nullptr;  // grease<->SiPM detector boundary
    G4OpticalSurface*  fHolderSurface   = nullptr;  // slab<->black holder (absorber)
    G4OpticalSurface*  fClampMirror     = nullptr;  // clamp backing (specular)
    G4OpticalSurface*  fFibreEndMirror  = nullptr;  // far fibre ends (mirror)

    static G4bool sHasWLSFibres;   // set true when WLS geometry is built
};

#endif

