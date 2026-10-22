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

// Edge reflector treatment for the uninstrumented narrow faces of the muon
// scintillator slab (presets 1-8).  Backward-compatible default is kEdgeSame.
enum EdgeReflectorMode {
    kEdgeSame    = 0,  // default: narrow edges use the slab's main fReflSurface skin
    kEdgeBlack   = 1,  // R=0 absorber on uninstrumented narrow edges (Stem 1: Position)
    kEdgeDiffuse = 2,  // R=0.985 Lambertian PTFE/TiO2 on narrow edges (Stem 2: Max PE)
    kEdgeMirror  = 3   // R=0.95 specular mirror on narrow edges
};

// Config-4 WLS clamp surface. A G4LogicalSkinSurface covers EVERY face of the
// clamp LV (channel walls and the grease-facing front included), so this is the
// single knob for the whole clamp body.
enum ClampSurfaceMode {
    kClampWhite  = 0,   // white 3D-printed plastic: Lambertian R = 0.85, absorbs 15 %
    kClampBlack  = 1,   // black plastic: no skin; the n=1.55 bulk with 1 um ABSLENGTH
    kClampMirror = 2    // legacy 95 % specular dielectric_metal
};

// Al-foil backing index.  The paper specifies grease applied selectively; the
// two modes bracket the construction: air over most of the foil (TIR retained)
// versus uniform grease contact (TIR cone widened to 67.5 deg).
enum AlBackingMode {
    kAlBackingGrease = 0,   // n = 1.46, current production model
    kAlBackingAir    = 1    // n = 1.0003, air-backed foil
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

    // 45-degree corner truncation of the slab (preset 12).  fCornerCut is the
    // leg removed along each square edge at every corner; the resulting 45-deg
    // face is fCornerCut*sqrt(2) wide.  0 (default) keeps the plain G4Box, so
    // every existing result is unchanged.  Must be set before Build(); preset
    // 12 resets it to 12 mm, so issue /dcs-monitor/det/muonScint/setCornerCut
    // AFTER the preset command if a different length is wanted.
    void SetCornerCut(G4double c)                 { fCornerCut = (c < 0.) ? 0. : c; }
    G4double GetCornerCut() const                 { return fCornerCut; }

    void SetReflectorType(ReflectorType t)        { fReflectorType = t; }
    void SetReflectorWrap(G4bool wrap)            { fWrapWithReflector = wrap; }

    // Bare plate: effective area fraction in contact with the black holder.
    // 0 = clean air gap (default), 1 = fully absorbing contact.  Implemented
    // as a mean-field Bernoulli loss at every bare face; see
    // BuildOpticalSurfaces().
    void SetBareContactFraction(G4double f)
    { fBareContactFraction = (f < 0.) ? 0. : (f > 1. ? 1. : f); }
    G4double GetBareContactFraction() const { return fBareContactFraction; }

    // Al-foil backing: uniform grease (default, n=1.46) or air (n=1.0003).
    void SetAlBackingMode(AlBackingMode m) { fAlBackingMode = m; }
    AlBackingMode GetAlBackingMode() const { return fAlBackingMode; }

    // Uninstrumented narrow-edge reflector (presets 1-8).  Default kEdgeSame
    // reproduces the v6 behaviour (edges use the main wrapping skin).
    void SetEdgeReflectorMode(EdgeReflectorMode m) { fEdgeReflectorMode = m; }
    EdgeReflectorMode GetEdgeReflectorMode() const { return fEdgeReflectorMode; }

    // Coupling-grease refractive index for SiPM read-out pads (default 1.46).
    void SetCouplingRIndex(G4double n)             { fCouplingRIndex = n; }
    G4double GetCouplingRIndex() const             { return fCouplingRIndex; }

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
    // clamp bar (white printed plastic by default) holding four Y-11(200) WLS
    // fibres read out by one SiPM.
    void SetUseWLSFibres(G4bool on)      { fUseWLSFibres = on; }
    G4bool UsesWLSFibres() const         { return fUseWLSFibres; }
    void SetNumWLSFibres(G4int n)        { fNFibres = n; }
    // Fibre cross-section: round Ø1.0 mm (default) or a flat-faced square of the
    // same width. The square replaces the 24-gon's shallow ~165 deg facet edges
    // (a documented G4Navigator stuck-track trigger) with 90 deg edges and makes
    // the clamp/grease booleans purely planar, at the cost of some optical
    // fidelity. Same spacing/kink/bend as the round fibres.
    void SetFibreSquareCross(G4bool on)  { fFibreSquareCross = on; }
    G4bool FibreSquareCross() const      { return fFibreSquareCross; }
    // Build the square fibre as a G4ExtrudedSolid instead of the tessellated
    // swept square. EXPERIMENTAL: the swept (Y,Z) outline is non-convex (a tiny
    // reflex notch at the bend), so G4ExtrudedSolid only supplies analytic
    // distance along the extrusion axis and falls back to its tessellated base
    // for the in-plane direction -- measured to INCREASE the stuck-track rate,
    // so it is off by default. Kept for reference/further experiments.
    void SetFibreUseExtruded(G4bool on)  { fFibreUseExtruded = on; }
    G4bool FibreUseExtruded() const      { return fFibreUseExtruded; }
    // Build the round fibres (and the film groove / clamp channel cutters) from
    // ANALYTIC G4CutTubs segments, one per straight leg of the BentPath, joined
    // by exact mitered cut planes. Same centreline/kinks as the tessellated
    // fibre, but no facet edges: every logged GeomNav1002 stuck track of the
    // tessellated model sits on a facet edge (24-gon vertex line or miter ring).
    void SetFibreAnalytic(G4bool on)     { fFibreAnalytic = on; }
    G4bool FibreAnalytic() const         { return fFibreAnalytic; }
    // Multi-clad fibre: outer fluoropolymer (n=1.42) sheath over the PMMA
    // (n=1.49) inner clad, as in the real Kuraray multi-clad specification.
    // false = the legacy single PMMA clad (production before this change).
    void SetFibreMultiClad(G4bool on)    { fFibreMultiClad = on; }
    G4bool FibreMultiClad() const        { return fFibreMultiClad; }
    // Y-11 WLS absorption length at the 430 nm absorption peak. The shape is the
    // digitized Kuraray spectrum; this sets the absolute scale (default 1.0 mm,
    // systematic band 0.3-3 mm).
    void SetWLSAbsLength430(G4double l)  { fWLSAbsLength430 = l; }
    G4double WLSAbsLength430() const     { return fWLSAbsLength430; }
    // Config-4 clamp body surface (white / black / mirror).
    void SetClampSurfaceMode(ClampSurfaceMode m) { fClampSurfaceMode = m; }
    ClampSurfaceMode ClampSurfaceMode_() const   { return fClampSurfaceMode; }
    // Config-4 direct optical contact between the slab's -Y face and the
    // read-out SiPM grease pad where the 4x4 mm pad overlaps the slab edge.
    // Default true (polished Fresnel coupling via fCouplingSurface); false
    // blocks direct slab<->pad transmission so the SiPM only views the fibres.
    void SetWLSPadSlabContact(G4bool on)         { fWLSPadSlabContact = on; }
    G4bool WLSPadSlabContact() const             { return fWLSPadSlabContact; }

    // Presets
    void PresetConfig_8SiPM_AllSides();
    void PresetConfig_4SiPM_OnePerSide();
    void PresetConfig_2SiPM_OneSide();
    void PresetConfig_4SiPM_Corners();
    void PresetConfig_4SiPM_CornerCut(); // Preset 12: 4 SiPMs on 45-deg corner faces
    void PresetConfig_WLSFibreClamp();   // Config 4
    void PresetConfig_WLSFibreClampBox();// Config 4, square (box) fibre cross-section
    void PresetConfig_WLSFibreClampAnalytic(); // Config 4, analytic G4CutTubs fibres

    // Phase-2 presets (stubs for link; bodies implemented in L2).
    void PresetConfig_8SiPM_Wide();            // Preset 8: 8 SiPMs, 2/side at u = +-0.75
    void PresetConfig_WLS4Sides_SingleEnded(); // Preset 10: 4-sided WLS clamps, 1 SiPM/side (4 SiPMs)
    void PresetConfig_WLS4Sides_DualEnded();   // Preset 11: 4-sided WLS clamps, 2 SiPMs/side (8 SiPMs)

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
    // The WLS fibre clamp (Config 4) replaces the -X SiPMs, so the bare holder
    // is not used there (it would also overlap the clamp body).
    G4bool UsesBlackHolder() const
    { return fReflectorType == kNoReflector && !fUseWLSFibres; }

    // true when the mean-field plate/holder contact loss (f) applies. This is
    // a property of the bare plate facing the black enclosure, so it applies
    // to every bare layout, including the WLS fibre clamp (Config 4): its bare
    // faces (+X, +-Y, +-Z outside the read-out pad) contact the enclosure just
    // like Configs 1-3. The -X face is coupled to the clamp through the grease
    // film border surface, which overrides this skin there.
    G4bool UsesBareContactModel() const
    { return fReflectorType == kNoReflector && fBareContactFraction > 0.; }

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
    void BuildEdgeStrips(G4LogicalVolume* worldLV,
                         const G4ThreeVector& slabPos,
                         G4RotationMatrix* slabRot);

    // dims (half-lengths). Default 100x100x5 mm = 200x200x10 mm slab.
    G4ThreeVector fHalfSize { 100.*CLHEP::mm, 100.*CLHEP::mm, 5.*CLHEP::mm };

    // Corner truncation leg [mm]; 0 = square G4Box (default, legacy geometry).
    G4double fCornerCut = 0.;

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
    G4double      fBareContactFraction = 0.0;   // 0 = air gap, 1 = full contact
    AlBackingMode fAlBackingMode     = kAlBackingGrease;

    // Scintillation light-yield scale (1.0 = physical). Viz-only override.
    G4double      fLightYieldScale   = 1.0;

    // Black holder dimensions. The cavity must enclose the plate AND the
    // grease/SiPM stack (protruding ~0.7 mm past the faces), so fHolderGap is
    // the clearance from the plate face to the holder wall.
    G4double fHolderGap      = 1.2 * CLHEP::mm;  // plate face -> holder wall
    G4double fHolderWall     = 2.0 * CLHEP::mm;  // holder wall thickness

    // ---- Config 4: WLS fibre clamp ----------------------------------------
    // A 3D-printed/milled bar clamp (white by default; see fClampSurfaceMode)
    // sits on the -X narrow face (the side
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
    G4bool fFibreSquareCross = false;         // false = round Ø1.0 mm, true = square
    G4bool fFibreUseExtruded = false;         // square fibre via G4ExtrudedSolid (experimental)
    G4bool fFibreAnalytic    = false;         // round fibre from G4CutTubs segments
    G4bool fFibreMultiClad   = true;          // outer n=1.42 sheath over the n=1.49 clad
    G4double fWLSAbsLength430 = 1.0 * CLHEP::mm; // Y-11 WLS absorption length at 430 nm
    ClampSurfaceMode fClampSurfaceMode = kClampWhite; // clamp body surface (white default)
    G4bool   fWLSPadSlabContact = true;          // direct slab<->read-out grease pad contact (default on)
    G4double fFibreRadius   = 0.5 * CLHEP::mm; // Ø1.0 mm core+clad
    G4double fFibreCladFrac = 0.03;            // cladding thickness fraction
    G4double fClampGap      = 0.10 * CLHEP::mm;// thin grease film slab<->clamp
    G4double fChannelRadius = 0.55 * CLHEP::mm;// holder channel radius (Ø1.1, Ø1.0 fibre + air gap)
    // Equal plastic margin across the 10 mm clamp/plate height: for four
    // Ø1.0 mm fibres, W = 4D + 2N*x with x = 0.75 mm -> centre pitch
    // D + 2x = 2.50 mm (axes at z = ±1.25, ±3.75 mm; 1.5 mm between
    // neighbouring fibre surfaces, 0.75 mm from the outer surfaces to the
    // top/bottom faces).
    G4double fFibrePitch    = 2.50 * CLHEP::mm;// straight-section centre-to-centre pitch
    G4double fBundlePitch   = 1.00 * CLHEP::mm;// bundled pitch at SiPM (fibres touching)
    // The bundle stub (last resL = 2 mm) is unchanged.  The ramp is lengthened
    // so the outermost fibre keeps the old ~3.3 deg kink: 2.25 mm of transverse
    // travel over a 39 mm ramp -> fFibreConvergeLen = 39 mm + 2 mm stub.
    G4double fFibreConvergeLen = 41.0 * CLHEP::mm;// straight channels stop this far
                                                 // short of the SiPM; fibres bundle here
    G4double fFibreProtrusion = 0.05 * CLHEP::mm; // fibre proud of channel mouth
    G4double fClampWallZ    = 2.0 * CLHEP::mm; // clamp bar thickness (x)

    // Fibre read-out layout in the slab-local frame (set by BuildWLSFibres).
    G4double fFibreAxisX    = 0.;              // fibre centre x (slab-local)
    G4double fFibreZSpacing = 2.5 * CLHEP::mm; // straight-section z spacing

    // Built objects
    G4LogicalVolume*   fScintLV     = nullptr;
    G4VPhysicalVolume* fScintPV     = nullptr;
    G4LogicalVolume*   fGreaseLV    = nullptr;
    G4LogicalVolume*   fSiPMLV      = nullptr;
    G4LogicalVolume*   fHolderLV    = nullptr;
    std::vector<G4LogicalVolume*> fFibreCoreLVs;
    std::vector<G4LogicalVolume*> fFibreCladLVs;
    // Outer cladding LV of each non-analytic fibre, in fibre order (k = 0..N-1).
    // Separate from fFibreCladLVs, which also contains the inner clads for
    // IsFibreLV(); indexing that list was placing only fibres 0 and 1.
    std::vector<G4LogicalVolume*> fFibreOuterLVs;
    // Analytic fibre: per fibre, one clad LV (with its core daughter) per
    // straight leg, plus that leg's object transform in the (pre-flip)
    // slab-local construction frame. Leg 0 = mirror end, last = read-out end.
    struct FibreSeg {
        G4LogicalVolume*  cladLV = nullptr;
        G4RotationMatrix  rot;      // object (local -> slab) rotation
        G4ThreeVector     pos;      // leg centre, slab-local (pre-flip)
    };
    std::vector<std::vector<FibreSeg>> fFibreSegs;
    G4LogicalVolume*   fClampLV     = nullptr;
    G4LogicalVolume*   fGrease4LV   = nullptr;
    G4LogicalVolume*   fMirrorCapLV = nullptr;
    G4Material*        fClampMat    = nullptr;

    G4OpticalSurface*  fReflSurface     = nullptr;  // wrapping (skin)
    G4OpticalSurface*  fCouplingSurface = nullptr;  // slab<->grease override (just transparent)
    G4OpticalSurface*  fSiPMSurface     = nullptr;  // grease<->SiPM detector boundary
    G4OpticalSurface*  fHolderSurface   = nullptr;  // slab<->black holder (absorber)
    G4OpticalSurface*  fClampMirror     = nullptr;  // clamp backing (specular)
    G4OpticalSurface*  fClampWhite      = nullptr;  // clamp body (white Lambertian, R=0.85)
    G4OpticalSurface*  fFibreEndMirror  = nullptr;  // far fibre ends (mirror)

    // Phase-2 state: edge reflector, coupling grease index, 4-sided WLS flags.
    EdgeReflectorMode fEdgeReflectorMode = kEdgeSame;
    G4double          fCouplingRIndex    = 1.46;
    G4bool            fWLSFourSides      = false;
    G4bool            fWLSDualEnded      = false;
    G4OpticalSurface* fEdgeReflSurface   = nullptr;
    G4VPhysicalVolume* fEdgeStripPVs[4] = {nullptr, nullptr, nullptr, nullptr};

    static G4bool sHasWLSFibres;   // set true when WLS geometry is built
};

#endif

