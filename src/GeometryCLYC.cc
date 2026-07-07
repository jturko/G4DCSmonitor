#include "DetectorConstruction.hh"
#include "DetectorMessenger.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Isotope.hh"
#include "G4Element.hh"

#include "G4Tubs.hh"
#include "G4GenericPolycone.hh"
#include "G4LogicalVolume.hh"
#include "G4AssemblyVolume.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"

#include "GeometryCLYC.hh"
#include "G4Region.hh"
#include "G4RegionStore.hh"

#include <vector>
#include <string>

GeometryCLYC::GeometryCLYC() :
    fCLYCAssembly(nullptr),
    fCrystalLog(nullptr), fCasingLog(nullptr), fReflectorLog(nullptr),
    fOpticalInterfaceLog(nullptr), fMagShieldLog(nullptr),
    fPMTWindowLog(nullptr), fPMTGlassLog(nullptr), fPMTVacuumLog(nullptr),
    fPMTBackLog(nullptr), fBaseLog(nullptr),
    fLiFCollimatorLog(nullptr), fPbCollimatorLog(nullptr),
    fPECollimatorLog(nullptr), fPEPlugLog(nullptr)
{
    fCrystalRadius   = 25./2. * mm;      // Ø25
    fCrystalLength   = 25.    * mm;      // 25 hgt
    fCasingThickness = 0.5    * mm;      // Al body wall 0.5 mm

    // optional front collimators / plug (unchanged defaults)
    fPbCollimatorInnerRadius = 29.4/2. * mm;
    fPbCollimatorOuterRadius = 40./2.  * mm;
    fPbCollimatorLength      = 50.     * mm;
    fLiFCollimatorInnerRadius = -1.*mm; fLiFCollimatorOuterRadius = -1.*mm; fLiFCollimatorLength = -1.*mm;
    fPECollimatorInnerRadius = 40./2.*mm; fPECollimatorOuterRadius = 50./2.*mm; fPECollimatorLength = 50.*mm;
    fPEPlugInnerRadius = 29./2.*mm; fPEPlugLipRadius = 50./2.*mm;
    fPEPlugInnerLength = 35.*mm;    fPEPlugLipLength = 10.*mm;

    // NEW fixed real-sensor package (drawing VS-0889-305)
    fSnoutOuterRadius          = 29./2. * mm;   // Ø29 protruding crystal-can snout
    fSnoutLength               = 15.  * mm;     // "15": snout height (crystal recessed above this)
    fReflectorThickness        = 1.5  * mm;     // Ø25 crystal + 1.5 refl + 0.5 Al = Ø29 snout
    fOpticalInterfaceThickness = 1.0  * mm;
    fMagShieldThickness        = 0.64 * mm;     // magnetic shield wall 0.64 mm
    fMagShieldOuterRadius      = 58.8/2. * mm;  // Ø58.8 envelope
    fTotalLength               = 125. * mm;
    fPMTRadius                 = 51./2. * mm;   // Ø51 photomultiplier
    fPMTGlassThickness         = 1.0  * mm;
    fBaseLength                = 15.  * mm;     // epoxy base region (covers PMT glass back)

    fCrystalMatName = "CLYC"; 
    fCasingMatName  = "G4_Al";
    fPbColMatName   = "G4_Pb";
    fLiFColMatName  = "LiF";
    fPEColMatName   = "G4_POLYETHYLENE";
    fPEPlugMatName  = "G4_POLYETHYLENE";
    fReflectorMatName        = "G4_TEFLON";
    fOpticalInterfaceMatName = "SiliconeOptical";
    fMagShieldMatName        = "MuMetal";
    fPMTGlassMatName         = "G4_Pyrex_Glass";
    fPMTVacuumMatName        = "G4_Galactic";
    fBaseMatName             = "Epoxy";

    fCrystalColour   = G4Colour(0.0,1.0,0.0,0.5);
    fCasingColour    = G4Colour(0.6,0.6,0.6,0.5);
    fPbColColour     = G4Colour(0.6,0.4,0.2,1.);
    fLiFColColour    = G4Colour(0.6,0.0,0.6,1.);
    fPEColColour     = G4Colour(0.0,1.0,1.0,1.);
    fPEPlugColour    = G4Colour(1.0,1.0,0.0,1.);
    fReflectorColour = G4Colour(1.0,1.0,1.0,0.6);
    fOpticalColour   = G4Colour(0.9,0.9,0.4,0.5);
    fMagShieldColour = G4Colour(0.3,0.3,0.35,0.55);
    fPMTColour       = G4Colour(0.7,0.8,1.0,0.4);
    fVacuumColour    = G4Colour(0.1,0.1,0.1,0.08);
    fBaseColour      = G4Colour(0.15,0.15,0.15,1.);
}

GeometryCLYC::~GeometryCLYC() {}

G4int GeometryCLYC::Build()
{
    BuildMaterials();
    fCLYCAssembly = new G4AssemblyVolume();

    const G4double sPhi = 0.*deg, ePhi = 360.*deg;

    // AddPlacedVolume() takes the translation by NON-CONST REFERENCE -> it must
    // be a named lvalue, never a temporary. Reuse 'move' for every call.
    G4ThreeVector     move;
    G4RotationMatrix* norot = nullptr;

    G4NistManager* man = G4NistManager::Instance();
    G4Material* mCrystal = man->FindOrBuildMaterial(fCrystalMatName);
    G4Material* mCasing  = man->FindOrBuildMaterial(fCasingMatName);
    G4Material* mRefl    = man->FindOrBuildMaterial(fReflectorMatName);
    G4Material* mOpt     = man->FindOrBuildMaterial(fOpticalInterfaceMatName);
    G4Material* mMu      = man->FindOrBuildMaterial(fMagShieldMatName);
    G4Material* mGlass   = man->FindOrBuildMaterial(fPMTGlassMatName);
    G4Material* mVac     = man->FindOrBuildMaterial(fPMTVacuumMatName);
    G4Material* mBase    = man->FindOrBuildMaterial(fBaseMatName);
    G4Material* mLiFCol  = man->FindOrBuildMaterial(fLiFColMatName);
    G4Material* mPbCol   = man->FindOrBuildMaterial(fPbColMatName);
    G4Material* mPECol   = man->FindOrBuildMaterial(fPEColMatName);
    G4Material* mPEPlug  = man->FindOrBuildMaterial(fPEPlugMatName);

    // ==================================================================
    // OPTIONAL FRONT PARTS: PE plug + collimators (front face at z = 0)
    // ==================================================================
    if (fPEPlugLipLength > 0. || fPEPlugInnerLength > 0.) {
        std::vector<G4double> r = { 0., fPEPlugLipRadius, fPEPlugLipRadius, fPEPlugInnerRadius, fPEPlugInnerRadius, 0. };
        std::vector<G4double> z = { 0., 0., -fPEPlugLipLength, -fPEPlugLipLength,
                                    -fPEPlugLipLength-fPEPlugInnerLength, -fPEPlugLipLength-fPEPlugInnerLength };
        auto* s = new G4GenericPolycone("PE_plug_solid", sPhi, ePhi, r.size(), r.data(), z.data());
        fPEPlugLog = new G4LogicalVolume(s, mPEPlug, "PEPlugLog");
        fPEPlugLog->SetVisAttributes(new G4VisAttributes(true, fPEPlugColour));
        move = G4ThreeVector(0.,0.,0.);
        fCLYCAssembly->AddPlacedVolume(fPEPlugLog, move, norot);
    }
    if (fPECollimatorLength > 0.) {
        auto* s = new G4Tubs("PE_collimator_solid", fPECollimatorInnerRadius, fPECollimatorOuterRadius, fPECollimatorLength/2., sPhi, ePhi);
        fPECollimatorLog = new G4LogicalVolume(s, mPECol, "PECollimatorLog");
        fPECollimatorLog->SetVisAttributes(new G4VisAttributes(true, fPEColColour));
        move = G4ThreeVector(0., 0., -fPEPlugLipLength - fPECollimatorLength/2.0);
        fCLYCAssembly->AddPlacedVolume(fPECollimatorLog, move, norot);
    }
    if (fPbCollimatorLength > 0.) {
        auto* s = new G4Tubs("Pb_collimator_solid", fPbCollimatorInnerRadius, fPbCollimatorOuterRadius, fPbCollimatorLength/2., sPhi, ePhi);
        fPbCollimatorLog = new G4LogicalVolume(s, mPbCol, "PbCollimatorLog");
        fPbCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fPbColColour));
        move = G4ThreeVector(0., 0., -fPEPlugLipLength - fPbCollimatorLength/2.0);
        fCLYCAssembly->AddPlacedVolume(fPbCollimatorLog, move, norot);
    }
    if (fLiFCollimatorLength > 0.) {
        auto* s = new G4Tubs("LiF_collimator_solid", fLiFCollimatorInnerRadius, fLiFCollimatorOuterRadius, fLiFCollimatorLength/2., sPhi, ePhi);
        fLiFCollimatorLog = new G4LogicalVolume(s, mLiFCol, "LiFCollimatorLog");
        fLiFCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fLiFColColour));
        move = G4ThreeVector(0., 0., -fPEPlugLipLength - fLiFCollimatorLength/2.0);
        fCLYCAssembly->AddPlacedVolume(fLiFCollimatorLog, move, norot);
    }

    // ==================================================================
    // FIXED DETECTOR BODY (always built). Depth d measured backward from the
    // detector front plane Zf; world z = Zf - d.
    // ==================================================================
    const G4double Zf = -(fPEPlugLipLength + fPEPlugInnerLength);   // front plane
    auto Z  = [&](G4double d){ return Zf - d; };                    // depth -> world z
    auto zC = [&](G4double dF, G4double dB){ return Zf - 0.5*(dF+dB); };

    // radii
    const G4double Rc     = fCrystalRadius;                          // 12.5
    const G4double Rrefl  = Rc + fReflectorThickness;                // 14.0
    const G4double Rsnout = fSnoutOuterRadius;                       // 14.5 (Ø29)
    const G4double RsnIn  = Rsnout - fCasingThickness;               // 14.0
    const G4double RmuO   = fMagShieldOuterRadius;                   // 29.4
    const G4double RmuI   = RmuO - fMagShieldThickness;              // 28.76
    const G4double RwideO = RmuI;                                    // wide Al OD touches shield
    const G4double RwideI = RwideO - fCasingThickness;               // 28.26
    const G4double Rpmt   = fPMTRadius;                              // 25.5
    const G4double gT     = fPMTGlassThickness;                      // 1.0

    // depths
    const G4double dWin   = fCasingThickness;                        // 0.5  (Al window)
    const G4double dXtalF = fCasingThickness + fReflectorThickness;  // 2.0  (crystal front)
    const G4double dXtalB = dXtalF + fCrystalLength;                 // 27.0 (crystal back)
    const G4double dOptB  = dXtalB + fOpticalInterfaceThickness;     // 28.0
    const G4double dPmtF  = dOptB;                                   // 28.0
    const G4double dTot   = fTotalLength;                            // 125
    const G4double dPmtB  = dTot - fBaseLength;                      // 110
    const G4double dSnout = fSnoutLength;                            // 15  (snout height)
    const G4double dShoul = dSnout + fCasingThickness;               // 15.5 (shoulder top)

    // --- 1. Stepped aluminium body: Ø29 snout (protruding) + shoulder + wide wall.
    //        Front window is solid; crystal channel is open and passes THROUGH
    //        the shoulder so the crystal is RECESSED into the wide body. ------
    {
        // contour (r, depth), traced around the solid cross-section
        std::vector<G4double> r = { 0.,   Rsnout, Rsnout, RwideO, RwideO, RwideI, RwideI, RsnIn, RsnIn, 0.   };
        std::vector<G4double> d = { 0.,   0.,     dSnout, dSnout, dTot,   dTot,   dShoul, dShoul, dWin,  dWin };
        std::vector<G4double> z(d.size()); for (size_t i=0;i<d.size();++i) z[i]=Z(d[i]);
        auto* s = new G4GenericPolycone("Casing_solid", sPhi, ePhi, r.size(), r.data(), z.data());
        fCasingLog = new G4LogicalVolume(s, mCasing, "CasingLog");
        fCasingLog->SetVisAttributes(new G4VisAttributes(true, fCasingColour));
        move = G4ThreeVector(0.,0.,0.);
        fCLYCAssembly->AddPlacedVolume(fCasingLog, move, norot);
    }

    // --- 2. Reflector cup (front cap + full-length side wall, open toward PMT) ---
    {
        std::vector<G4double> r = { 0.,   Rrefl, Rrefl,  Rc,     Rc,     0.     };
        std::vector<G4double> d = { dWin, dWin,  dXtalB, dXtalB, dXtalF, dXtalF };
        std::vector<G4double> z(d.size()); for (size_t i=0;i<d.size();++i) z[i]=Z(d[i]);
        auto* s = new G4GenericPolycone("Reflector_solid", sPhi, ePhi, r.size(), r.data(), z.data());
        fReflectorLog = new G4LogicalVolume(s, mRefl, "ReflectorLog");
        fReflectorLog->SetVisAttributes(new G4VisAttributes(true, fReflectorColour));
        move = G4ThreeVector(0.,0.,0.);
        fCLYCAssembly->AddPlacedVolume(fReflectorLog, move, norot);
    }

    // --- 3. CLYC crystal (sensitive) -- straddles snout (2..15) and body (15..27) ---
    {
        auto* s = new G4Tubs("Crystal_solid", 0., Rc, fCrystalLength/2.0, sPhi, ePhi);
        fCrystalLog = new G4LogicalVolume(s, mCrystal, "CrystalLog");
        fCrystalLog->SetVisAttributes(new G4VisAttributes(true, fCrystalColour));
        G4Region* reg = G4RegionStore::GetInstance()->GetRegion("CLYCRegion", false);
        if (!reg) reg = new G4Region("CLYCRegion");
        reg->AddRootLogicalVolume(fCrystalLog);
        move = G4ThreeVector(0., 0., zC(dXtalF, dXtalB));
        fCLYCAssembly->AddPlacedVolume(fCrystalLog, move, norot);
    }

    // --- 4. Optical interface (crystal readout <-> PMT window) ---
    {
        auto* s = new G4Tubs("Optical_solid", 0., Rc, fOpticalInterfaceThickness/2.0, sPhi, ePhi);
        fOpticalInterfaceLog = new G4LogicalVolume(s, mOpt, "OpticalInterfaceLog");
        fOpticalInterfaceLog->SetVisAttributes(new G4VisAttributes(true, fOpticalColour));
        move = G4ThreeVector(0., 0., zC(dXtalB, dOptB));
        fCLYCAssembly->AddPlacedVolume(fOpticalInterfaceLog, move, norot);
    }

    // --- 5. PMT (Ø51): front window, side envelope, vacuum, back window ---
    //{
    //    auto* w = new G4Tubs("PMTWindow_solid", 0., Rpmt, gT/2.0, sPhi, ePhi);
    //    fPMTWindowLog = new G4LogicalVolume(w, mGlass, "PMTWindowLog");
    //    fPMTWindowLog->SetVisAttributes(new G4VisAttributes(true, fPMTColour));
    //    move = G4ThreeVector(0., 0., zC(dPmtF, dPmtF+gT));
    //    fCLYCAssembly->AddPlacedVolume(fPMTWindowLog, move, norot);

    //    auto* e = new G4Tubs("PMTGlass_solid", Rpmt-gT, Rpmt, (dPmtB-dPmtF-2*gT)/2.0, sPhi, ePhi);
    //    fPMTGlassLog = new G4LogicalVolume(e, mGlass, "PMTGlassLog");
    //    fPMTGlassLog->SetVisAttributes(new G4VisAttributes(true, fPMTColour));
    //    move = G4ThreeVector(0., 0., zC(dPmtF+gT, dPmtB-gT));
    //    fCLYCAssembly->AddPlacedVolume(fPMTGlassLog, move, norot);

    //    auto* v = new G4Tubs("PMTVacuum_solid", 0., Rpmt-gT, (dPmtB-dPmtF-2*gT)/2.0, sPhi, ePhi);
    //    fPMTVacuumLog = new G4LogicalVolume(v, mVac, "PMTVacuumLog");
    //    //fPMTVacuumLog->SetVisAttributes(new G4VisAttributes(true, fVacuumColour));
    //    fPMTVacuumLog->SetVisAttributes(G4VisAttributes::GetInvisible());
    //    move = G4ThreeVector(0., 0., zC(dPmtF+gT, dPmtB-gT));
    //    fCLYCAssembly->AddPlacedVolume(fPMTVacuumLog, move, norot);

    //    auto* b = new G4Tubs("PMTBack_solid", 0., Rpmt, gT/2.0, sPhi, ePhi);
    //    fPMTBackLog = new G4LogicalVolume(b, mGlass, "PMTBackLog");
    //    fPMTBackLog->SetVisAttributes(new G4VisAttributes(true, fPMTColour));
    //    move = G4ThreeVector(0., 0., zC(dPmtB-gT, dPmtB));
    //    fCLYCAssembly->AddPlacedVolume(fPMTBackLog, move, norot);
    //}
    {
        // Glass cup: closed front window, side wall (thk gT), closed back.
        // Contour traced (r, depth) around the solid glass cross-section:
        //   outer wall from front to back, then inner wall back to front,
        //   leaving a gT-thick window at dPmtF and a gT-thick back at dPmtB.
        std::vector<G4double> r = {
            0.,    Rpmt,  Rpmt,      Rpmt-gT,   Rpmt-gT,   0.
        };
        std::vector<G4double> d = {
            dPmtF, dPmtF, dPmtB,     dPmtB,     dPmtF+gT,  dPmtF+gT
        };
        std::vector<G4double> z(d.size());
        for (size_t i=0;i<d.size();++i) z[i]=Z(d[i]);

        auto* gs = new G4GenericPolycone("PMTGlass_solid", sPhi, ePhi,
                                         r.size(), r.data(), z.data());
        fPMTGlassLog = new G4LogicalVolume(gs, mGlass, "PMTGlassLog");
        fPMTGlassLog->SetVisAttributes(new G4VisAttributes(true, fPMTColour));
        move = G4ThreeVector(0.,0.,0.);
        fCLYCAssembly->AddPlacedVolume(fPMTGlassLog, move, norot);

        // Vacuum interior: single tube filling the hollow between window & back.
        const G4double vacLen = (dPmtB - gT) - (dPmtF + gT);   // interior depth
        auto* vs = new G4Tubs("PMTVacuum_solid", 0., Rpmt-gT, vacLen/2.0, sPhi, ePhi);
        fPMTVacuumLog = new G4LogicalVolume(vs, mVac, "PMTVacuumLog");
        //fPMTVacuumLog->SetVisAttributes(new G4VisAttributes(true, fVacuumColour));
        fPMTVacuumLog->SetVisAttributes(G4VisAttributes::GetInvisible());
        move = G4ThreeVector(0., 0., zC(dPmtF+gT, dPmtB-gT));
        fCLYCAssembly->AddPlacedVolume(fPMTVacuumLog, move, norot);
    }

    // --- 6. Epoxy base: covers PMT glass back, fits inside wide Al wall ---
    {
        auto* s = new G4Tubs("Base_solid", 0., RwideI, fBaseLength/2.0, sPhi, ePhi);
        fBaseLog = new G4LogicalVolume(s, mBase, "BaseLog");
        fBaseLog->SetVisAttributes(new G4VisAttributes(true, fBaseColour));
        move = G4ThreeVector(0., 0., zC(dPmtB, dTot));
        fCLYCAssembly->AddPlacedVolume(fBaseLog, move, norot);
    }

    // --- 7. Magnetic shield: front at depth 15 (top of snout), back at 125 ---
    {
        auto* s = new G4Tubs("MagShield_solid", RmuI, RmuO, (dTot-dSnout)/2.0, sPhi, ePhi);
        fMagShieldLog = new G4LogicalVolume(s, mMu, "MagShieldLog");
        fMagShieldLog->SetVisAttributes(new G4VisAttributes(true, fMagShieldColour));
        move = G4ThreeVector(0., 0., zC(dSnout, dTot));
        fCLYCAssembly->AddPlacedVolume(fMagShieldLog, move, norot);
    }

    G4cout << " -> GeometryCLYC (FIXED body): Ø" << 2*Rsnout/mm << " snout protrudes "
           << fSnoutLength/mm << " mm; crystal recessed from depth " << fSnoutLength/mm
           << " to " << dXtalB/mm << " mm into the wide body; mag-shield spans z="
           << Z(dSnout)/mm << " -> " << Z(dTot)/mm << " mm." << G4endl;
    return 1;
}

void GeometryCLYC::PlaceDetector(G4LogicalVolume* logic_world, G4ThreeVector move,
                                 G4RotationMatrix* rotate, G4int copyNo)
{
    fCLYCAssembly->MakeImprint(logic_world, move, rotate, copyNo, /*surfCheck*/true);
}

void GeometryCLYC::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();
    if (nist->FindOrBuildMaterial("CLYC", false) != nullptr) return;

    G4Element* el_Cs = nist->FindOrBuildElement("Cs");
    G4Element* el_Y  = nist->FindOrBuildElement("Y");
    G4Element* el_Cl = nist->FindOrBuildElement("Cl");
    G4Element* el_F  = nist->FindOrBuildElement("F");
    G4Element* el_C  = nist->FindOrBuildElement("C");
    G4Element* el_H  = nist->FindOrBuildElement("H");
    G4Element* el_O  = nist->FindOrBuildElement("O");
    G4Element* el_Si = nist->FindOrBuildElement("Si");
    G4Element* el_Ni = nist->FindOrBuildElement("Ni");
    G4Element* el_Fe = nist->FindOrBuildElement("Fe");
    G4Element* el_Mo = nist->FindOrBuildElement("Mo");
    G4Element* el_B  = nist->FindOrBuildElement("B");

    G4Isotope* Li6 = new G4Isotope("Li6", 3, 6, 6.015*g/mole);
    G4Isotope* Li7 = new G4Isotope("Li7", 3, 7, 7.016*g/mole);
    G4Element* el_Li_enr = new G4Element("Enriched Lithium", "Li", 2);
    el_Li_enr->AddIsotope(Li6, 95.*perCent);
    el_Li_enr->AddIsotope(Li7,  5.*perCent);
    G4Element* el_TS_H = new G4Element("TS_H_of_Polyethylene", "H_POLYETHYLENE", 1.0, 1.0079*g/mole);

    nist->FindOrBuildMaterial("G4_Al");
    nist->FindOrBuildMaterial("G4_Pb");
    nist->FindOrBuildMaterial("G4_POLYETHYLENE");
    nist->FindOrBuildMaterial("G4_AIR");
    nist->FindOrBuildMaterial("G4_TEFLON");
    nist->FindOrBuildMaterial("G4_Pyrex_Glass");
    nist->FindOrBuildMaterial("G4_Galactic");

    G4Material* clyc = new G4Material("CLYC", 3.31*g/cm3, 4);
    clyc->AddElement(el_Cs, 2); clyc->AddElement(el_Li_enr, 1);
    clyc->AddElement(el_Y, 1);  clyc->AddElement(el_Cl, 6);

    G4Material* lif = new G4Material("LiF", 2.635*g/cm3, 2);
    lif->AddElement(el_Li_enr, 1); lif->AddElement(el_F, 1);

    G4Material* pehd = new G4Material("PEHD", 0.96*g/cm3, 2);
    pehd->AddElement(el_TS_H, 2); pehd->AddElement(el_C, 1);
    G4Material* pehdb = new G4Material("PEHD_borated", 1.01*g/cm3, 2);
    pehdb->AddMaterial(pehd, 95.*perCent); pehdb->AddElement(el_B, 5.*perCent);

    if (nist->FindOrBuildMaterial("SiliconeOptical", false) == nullptr) {
        auto* s = new G4Material("SiliconeOptical", 1.03*g/cm3, 4);
        s->AddElement(el_C,2); s->AddElement(el_H,6); s->AddElement(el_O,1); s->AddElement(el_Si,1);
    }
    if (nist->FindOrBuildMaterial("MuMetal", false) == nullptr) {
        auto* m = new G4Material("MuMetal", 8.7*g/cm3, 3);
        m->AddElement(el_Ni,80.*perCent); m->AddElement(el_Fe,15.*perCent); m->AddElement(el_Mo,5.*perCent);
    }
    if (nist->FindOrBuildMaterial("Epoxy", false) == nullptr) {
        auto* e = new G4Material("Epoxy", 1.2*g/cm3, 3);
        e->AddElement(el_C,21); e->AddElement(el_H,25); e->AddElement(el_O,5);
    }
}

G4ThreeVector GeometryCLYC::GetCrystalCenterLocal() const
{
    const G4double zc = -fPEPlugLipLength - fPEPlugInnerLength
                        - fCasingThickness - fReflectorThickness
                        - (fCrystalLength / 2.0);
    return G4ThreeVector(0., 0., zc);
}

