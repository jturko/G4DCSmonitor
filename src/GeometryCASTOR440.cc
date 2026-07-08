#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Isotope.hh"
#include "G4Element.hh"
#include "G4Tubs.hh"
#include "G4Box.hh"
#include "G4Polyhedra.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4AssemblyVolume.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4Exception.hh"
#include "Randomize.hh"

#include "GeometryCASTOR440.hh"
#include <cmath>

static constexpr G4bool kCheckOverlaps = false;   // keep ON for the first build

//....oooOO0OOooo......

GeometryCASTOR440::GeometryCASTOR440() :
    fCASTORAssembly(nullptr), fCASTORBodyPhys(nullptr),
    fCASTORBodyLog(nullptr), fCavityLog(nullptr),
    fPrimaryLid1Log(nullptr), fPrimaryLid2Log(nullptr),
    fSecondaryLidLog(nullptr), fProtectiveLidLog(nullptr),
    fFuelCellLog(nullptr), fModeratorRodLog(nullptr),
    fFinLog(nullptr), fFinCutLog(nullptr), fTrunnionLog(nullptr)
{
    // ---------------- CASTOR 440/84 body (drawing-confirmed) ----------------
    fCaskHeight       = 4080. * mm;     // body length
    fCaskInnerRadius  =  900. * mm;     // shaft Ø1800
    fBottomThickness  =  366. * mm;
    fCaskOuterRadius  = 1270. * mm;     // Ø2540 at bottom of rib (= wall 370)
    fRibHeight        =   60. * mm;
    fFinTipRadius     = fCaskOuterRadius + fRibHeight;   // Ø2660
    fCavityHeight     = 3260. * mm;     // shaft length

    // ---------------- top closure (requested; auto-fit below) ----------------
    fTopIronGap        =   2. * mm;     // iron cap over top lid -> no coplanar faces
    fPrimaryLid1Thk    = 100. * mm;  fPrimaryLid1Rad  =  900. * mm;  // inner primary (steel)
    fPrimaryLid2Thk    = 200. * mm;  fPrimaryLid2Rad  = 1000. * mm;  // outer primary (steel)
    fSecondaryLidThk   = 125. * mm;  fSecondaryLidRad = 1150. * mm;  // secondary  (PE)
    fProtectiveLidThk  = 100. * mm;  fProtectiveLidRad = fCaskOuterRadius; // protective (steel)

    // Auto-fit the lid stack into the available budget while PRESERVING the
    // drawing body height (4080) and the PE-secondary thickness. The 71 mm of
    // excess is trimmed from the thickest steel layer (PrimaryLid2); if that is
    // not possible the whole stack is scaled. This is what removes the lid
    // overlap that the raw 525 mm stack produced.
    {
        const G4double lidBudget = fCaskHeight - fBottomThickness - fCavityHeight; // 454
        const G4double usable    = lidBudget - fTopIronGap;                        // 452
        if (usable <= 0.) {
            G4Exception("GeometryCASTOR440", "NoLidSpace", FatalException,
                        "Cavity + bottom leave no room for the lid stack.");
        }
        const G4double reqSum = fPrimaryLid1Thk + fPrimaryLid2Thk
                              + fSecondaryLidThk + fProtectiveLidThk;
        if (reqSum > usable) {
            const G4double excess = reqSum - usable;
            if (fPrimaryLid2Thk - excess > 1.*mm) {
                G4cout << " [CASTOR440] Lid stack " << reqSum/mm << " mm > budget "
                       << usable/mm << " mm: trimming PrimaryLid2 by "
                       << excess/mm << " mm (PE kept intact)." << G4endl;
                fPrimaryLid2Thk -= excess;
            } else {
                const G4double s = usable / reqSum;
                G4cout << " [CASTOR440] Lid stack too thick: scaling ALL lids by "
                       << s << "." << G4endl;
                fPrimaryLid1Thk *= s; fPrimaryLid2Thk *= s;
                fSecondaryLidThk *= s; fProtectiveLidThk *= s;
            }
        }
        fLidThickness = fPrimaryLid1Thk + fPrimaryLid2Thk
                      + fSecondaryLidThk + fProtectiveLidThk;
    }

    // ---------------- fuel-length envelope (PDF Table I) ----------------
    fActiveFuelLength = 2420. * mm;
    fTotalFuelLength  = 3217. * mm;
    fShroudLength     = fTotalFuelLength;

    // ---------------- wall moderator holes (Ø65, N=76, pitch Ø2180) ----------------
    fNModRods              = 76;
    fModRodRadius          = 65./2. * mm;
    fModRodRingRadius      = 1090.  * mm;   // pitch diameter 2180
    fModRodBottomClearance = 0.     * mm;   // 0 = flush with body bottom face

    // ---------------- cooling fins ----------------
    fFinThickness = 8.  * mm;
    fFinSpacing   = 25. * mm;

    // ---------------- trunnions ----------------
    fTrunnionRadius     = 90.  * mm;
    fTrunnionLength     = 120. * mm;
    fTrunnionAxialInset = 350. * mm;
    fTrunnionZ[0] = 0.; fTrunnionZ[1] = 0.;




    //// ---------------- basket cell / shroud ----------------
    //fAssyPitch        = 147.  * mm;
    //fAssyPhiStart     =  30.  * deg;
    //fCellApothem      = fAssyPitch/2.0 - 0.1*mm;   // 73.4
    //fBasketWall       = 0.5  * mm;
    //fShroudApothemOut = 72.0 * mm;                 // flat-to-flat 144 mm
    //fShroudWall       = 1.5  * mm;

    // ---------------- basket cell / shroud ----------------
    // Assembly SIZE (drawing-confirmed) is kept fixed; only the placement
    // PITCH is changed so the 84 assemblies spread out to fill the cavity.
    fShroudApothemOut = 72.0 * mm;                 // flat-to-flat 144 mm (correct)
    fShroudWall       = 1.5  * mm;
    fBasketWall       = 0.5  * mm;
    fAssyPhiStart     = 30.  * deg;

    // He cell now hugs a SINGLE assembly (basket wall over the shroud) instead
    // of being tied to the pitch. The gap between assemblies becomes cavity He.
    fCellApothem      = fShroudApothemOut + fBasketWall + 0.1*mm;   // 72.6 mm

    // Auto-fit the placement pitch to the cavity. The farthest retained lattice
    // site (a ring-5 edge site next to a removed corner) sits at sqrt(21)*pitch
    // from the axis; add the cell circumradius and keep a 15 mm clearance to
    // the R = 900 mm wall.
    {
        const G4double cellCircumR = fCellApothem / std::cos(CLHEP::pi/6.);      // ~83.8 mm
        const G4double clearance   = 15. * mm;
        const G4double maxCenterR  = fCaskInnerRadius - cellCircumR - clearance; // ~801 mm
        const G4double fitPitch    = maxCenterR / std::sqrt(21.0);               // ~175 mm
        const G4double minPitch    = 2.0*fCellApothem + 0.2*mm;                  // no overlap
        fAssyPitch = std::max(minPitch, fitPitch);
        G4cout << " [CASTOR440] Assembly placement pitch auto-fit to "
               << fAssyPitch/mm << " mm (cell F2F " << 2*fCellApothem/mm
               << " mm, outermost centre " << std::sqrt(21.0)*fAssyPitch/mm
               << " mm)." << G4endl;
    }





    // ---------------- axial assembly layout (cell-local) ----------------
    fNozzleBotLen = 380. * mm;
    fNozzleTopLen = 280. * mm;
    fRodLength    = 2557. * mm;
    fEndPlugLen   =  26. * mm;
    fPlenumLen    =  85. * mm;
    {
        const G4double cellHalf  = fTotalFuelLength/2.0;
        const G4double rodBottom = -cellHalf + fNozzleBotLen;
        const G4double rodTop    =  rodBottom + fRodLength;
        fRodCenterZ    = 0.5*(rodBottom + rodTop);
        const G4double activeBot = rodBottom + fEndPlugLen;
        fActiveCenterZ = activeBot + fActiveFuelLength/2.0;
    }

    // ---------------- spacer grids (PDF: 11) ----------------
    fNSpacerGrids     = 11;
    fSpacerGridHeight = 20.0 * mm;
    fSpacerBandRadial =  1.0 * mm;

    // ---------------- VVER-440 rod (PDF Table I) ----------------
    fNRodRings         = 6;
    fRodPitch          = 12.2 * mm;
    fCladOuterR        = 4.55 * mm;
    fCladInnerR        = 3.86 * mm;
    fPelletOuterR      = 3.80 * mm;
    fPelletHoleR       = 0.60 * mm;
    fCentralTubeOuterR = 5.15 * mm;
    fCentralTubeInnerR = 4.55 * mm;

    // ---------------- material names ----------------
    fCastIronMatName = "DuctileCastIron";
    fHeliumMatName   = "G4_He";
    fPEMatName       = "G4_POLYETHYLENE";
    fFuelMatName     = "UO2";
    fSteelMatName    = "G4_STAINLESS-STEEL";
    fCladMatName     = "Zr1Nb";
    fShroudMatName   = "Zr25Nb";

    GenerateFuelPositions();
    GenerateRodPositions();
}

GeometryCASTOR440::~GeometryCASTOR440() {}

//....oooOO0OOooo......

void GeometryCASTOR440::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();
    if (nist->FindOrBuildMaterial("DuctileCastIron", false) != nullptr) return;

    G4Element* Fe = nist->FindOrBuildElement("Fe");
    G4Element* C  = nist->FindOrBuildElement("C");
    G4Element* Si = nist->FindOrBuildElement("Si");
    G4Element* Mn = nist->FindOrBuildElement("Mn");
    auto* dci = new G4Material("DuctileCastIron", 7.1*g/cm3, 4);
    dci->AddElement(Fe, 93.5*perCent); dci->AddElement(C, 3.5*perCent);
    dci->AddElement(Si, 2.5*perCent);  dci->AddElement(Mn, 0.5*perCent);

    // UO2 enriched to 4.4% U-235 (PDF Table I / Table III), ~95% TD.
    G4Isotope* U235 = new G4Isotope("U235", 92, 235, 235.0439*g/mole);
    G4Isotope* U238 = new G4Isotope("U238", 92, 238, 238.0508*g/mole);
    G4Element* Uenr = new G4Element("EnrichedU", "U", 2);
    Uenr->AddIsotope(U235, 4.4*perCent);
    Uenr->AddIsotope(U238, 95.6*perCent);
    G4Element* O = nist->FindOrBuildElement("O");
    auto* uo2 = new G4Material("UO2", 10.6*g/cm3, 2);   // PDF pellet density 10.5-10.7
    uo2->AddElement(Uenr, 1); uo2->AddElement(O, 2);

    G4Element* Zr = nist->FindOrBuildElement("Zr");
    G4Element* Nb = nist->FindOrBuildElement("Nb");
    auto* zr1 = new G4Material("Zr1Nb", 6.55*g/cm3, 2);   // E110 clad/end-plugs
    zr1->AddElement(Zr, 99.0*perCent); zr1->AddElement(Nb, 1.0*perCent);
    auto* zr25 = new G4Material("Zr25Nb", 6.55*g/cm3, 2); // E125 shroud
    zr25->AddElement(Zr, 97.5*perCent); zr25->AddElement(Nb, 2.5*perCent);
}

//....oooOO0OOooo......

void GeometryCASTOR440::GenerateRodPositions()
{
    fRodPositions.clear();
    const G4double dx = fRodPitch, dy = fRodPitch*std::sqrt(3.0)/2.0;
    const G4int    R  = fNRodRings;
    const G4double cs = std::cos(fAssyPhiStart), sn = std::sin(fAssyPhiStart);
    for (G4int q = -R; q <= R; ++q) {
        const G4int r1 = std::max(-R, -q-R), r2 = std::min(R, -q+R);
        for (G4int r = r1; r <= r2; ++r) {
            const G4double xl = dx*(q + r/2.0), yl = dy*r;
            if (std::hypot(xl, yl) < 1e-6) continue;     // centre = central tube
            fRodPositions.emplace_back(xl*cs - yl*sn, xl*sn + yl*cs, 0.);
        }
    }
}

//....oooOO0OOooo......
//  One full VVER-440 assembly inside a He hexagonal cell:
//    basket separator | Zr-2.5%Nb shroud | bottom & top steel nozzles |
//    central Zr tube | 126 rods (Zr clad + annular UO2) | 11 steel grids
//....oooOO0OOooo......
G4LogicalVolume* GeometryCASTOR440::BuildFuelAssemblyCell()
{
    G4NistManager* man = G4NistManager::Instance();
    G4Material* He     = man->FindOrBuildMaterial(fHeliumMatName);
    G4Material* fuel   = man->FindOrBuildMaterial(fFuelMatName);
    G4Material* clad   = man->FindOrBuildMaterial(fCladMatName);
    G4Material* shroud = man->FindOrBuildMaterial(fShroudMatName);
    G4Material* steel  = man->FindOrBuildMaterial(fSteelMatName);

    if (fRodPositions.empty()) GenerateRodPositions();

    const G4double cellHalf = fTotalFuelLength/2.0;
    const G4double shroudIn = fShroudApothemOut - fShroudWall;   // 70.5
    G4double zC[2] = { -cellHalf, +cellHalf };
    G4double r0[2] = { 0., 0. };

    // ---- He cell envelope (placed 84x) ----
    G4double reCell[2] = { fCellApothem, fCellApothem };
    auto* cellSolid = new G4Polyhedra("FuelCell", fAssyPhiStart, 360.*deg,
                                      6, 2, zC, r0, reCell);
    fFuelCellLog = new G4LogicalVolume(cellSolid, He, "FuelCellLog");
    fFuelCellLog->SetVisAttributes(new G4VisAttributes(false));

    // ---- steel basket separator (hex annulus, full length) ----
    {
        const G4double bIn = fCellApothem - fBasketWall;
        G4double ri[2] = { bIn, bIn }, ro[2] = { fCellApothem, fCellApothem };
        auto* s = new G4Polyhedra("Basket", fAssyPhiStart, 360.*deg, 6, 2, zC, ri, ro);
        auto* l = new G4LogicalVolume(s, steel, "BasketLog");
        l->SetVisAttributes(new G4VisAttributes(G4Colour(0.40,0.40,0.50)));
        new G4PVPlacement(nullptr, {}, l, "BasketPhys", fFuelCellLog, false, 0, kCheckOverlaps);
    }

    // ---- Zr-2.5%Nb shroud (hex annulus, full length) ----
    {
        G4double ri[2] = { shroudIn, shroudIn };
        G4double ro[2] = { fShroudApothemOut, fShroudApothemOut };
        auto* s = new G4Polyhedra("Shroud", fAssyPhiStart, 360.*deg, 6, 2, zC, ri, ro);
        auto* l = new G4LogicalVolume(s, shroud, "ShroudLog");
        l->SetVisAttributes(new G4VisAttributes(G4Colour(0.70,0.70,0.80)));
        new G4PVPlacement(nullptr, {}, l, "ShroudPhys", fFuelCellLog, false, 0, kCheckOverlaps);
    }

    // ---- bottom & top steel nozzles (hex solids inside the shroud) ----
    {
        const G4double nozApo = shroudIn - 0.1*mm;
        {
            G4double zN[2] = { -fNozzleBotLen/2.0, +fNozzleBotLen/2.0 };
            G4double ro[2] = { nozApo, nozApo };
            auto* s = new G4Polyhedra("NozzleBot", fAssyPhiStart, 360.*deg, 6, 2, zN, r0, ro);
            auto* l = new G4LogicalVolume(s, steel, "NozzleBotLog");
            l->SetVisAttributes(new G4VisAttributes(G4Colour(0.55,0.55,0.60)));
            const G4double zc = -cellHalf + fNozzleBotLen/2.0;
            new G4PVPlacement(nullptr, {0,0,zc}, l, "NozzleBotPhys", fFuelCellLog, false, 0, kCheckOverlaps);
        }
        {
            G4double zN[2] = { -fNozzleTopLen/2.0, +fNozzleTopLen/2.0 };
            G4double ro[2] = { nozApo, nozApo };
            auto* s = new G4Polyhedra("NozzleTop", fAssyPhiStart, 360.*deg, 6, 2, zN, r0, ro);
            auto* l = new G4LogicalVolume(s, steel, "NozzleTopLog");
            l->SetVisAttributes(new G4VisAttributes(G4Colour(0.60,0.60,0.66)));
            const G4double zc = +cellHalf - fNozzleTopLen/2.0;
            new G4PVPlacement(nullptr, {0,0,zc}, l, "NozzleTopPhys", fFuelCellLog, false, 1, kCheckOverlaps);
        }
    }

    // ---- central tube (Zr), spanning the rod bundle ----
    {
        auto* s = new G4Tubs("CentralTube", fCentralTubeInnerR, fCentralTubeOuterR,
                             fRodLength/2.0, 0, 360.*deg);
        auto* l = new G4LogicalVolume(s, clad, "CentralTubeLog");
        l->SetVisAttributes(new G4VisAttributes(G4Colour(0.50,0.50,0.60)));
        new G4PVPlacement(nullptr, {0,0,fRodCenterZ}, l, "CentralTubePhys",
                          fFuelCellLog, false, 0, kCheckOverlaps);
    }

    // ---- 126 rods: Zr clad over full rod, annular UO2 over active zone ----
    auto* cladSolid = new G4Tubs("Clad", fCladInnerR, fCladOuterR, fRodLength/2.0, 0, 360.*deg);
    auto* cladLog   = new G4LogicalVolume(cladSolid, clad, "CladLog");
    cladLog->SetVisAttributes(new G4VisAttributes(G4Colour(0.70,0.70,0.80)));

    auto* pelletSolid = new G4Tubs("Pellet", fPelletHoleR, fPelletOuterR,
                                   fActiveFuelLength/2.0, 0, 360.*deg);
    auto* pelletLog   = new G4LogicalVolume(pelletSolid, fuel, "FuelLog");
    pelletLog->SetVisAttributes(new G4VisAttributes(G4Colour(1.0,0.0,0.0)));

    G4int rodNo = 0;
    for (const auto& p : fRodPositions) {
        new G4PVPlacement(nullptr, {p.x(), p.y(), fRodCenterZ},    cladLog,
                          "CladPhys", fFuelCellLog, false, rodNo, kCheckOverlaps);
        new G4PVPlacement(nullptr, {p.x(), p.y(), fActiveCenterZ}, pelletLog,
                          "FuelPhys", fFuelCellLog, false, rodNo, kCheckOverlaps);
        ++rodNo;
    }

    // ---- 11 spacer grids: thin steel hex rim bands in the rod/shroud gap ----
    {
        const G4double gridOut = shroudIn - 0.1*mm;
        const G4double gridIn  = gridOut - fSpacerBandRadial;
        G4double zG[2] = { -fSpacerGridHeight/2.0, +fSpacerGridHeight/2.0 };
        G4double ri[2] = { gridIn,  gridIn  };
        G4double ro[2] = { gridOut, gridOut };
        auto* s = new G4Polyhedra("SpacerGrid", fAssyPhiStart, 360.*deg, 6, 2, zG, ri, ro);
        auto* l = new G4LogicalVolume(s, steel, "SpacerGridLog");
        l->SetVisAttributes(new G4VisAttributes(G4Colour(0.45,0.45,0.50)));

        const G4double activeBot = fActiveCenterZ - fActiveFuelLength/2.0;
        for (G4int k = 0; k < fNSpacerGrids; ++k) {
            const G4double zk = activeBot
                              + (k + 0.5) * (fActiveFuelLength / fNSpacerGrids);
            new G4PVPlacement(nullptr, {0,0,zk}, l, "SpacerGridPhys",
                              fFuelCellLog, false, k, kCheckOverlaps);
        }
    }

    G4cout << " -> VVER-440 assembly: " << rodNo << " rods + central tube + 2 nozzles + "
           << fNSpacerGrids << " spacer grids (plenum " << fPlenumLen/mm << " mm)" << G4endl;
    return fFuelCellLog;
}

//....oooOO0OOooo......

G4int GeometryCASTOR440::Build()
{
    G4cout << " -> Constructing GeometryCASTOR440 (CASTOR 440/84 + VVER-440)..." << G4endl;

    BuildMaterials();
    fCASTORAssembly = new G4AssemblyVolume();

    G4NistManager* man = G4NistManager::Instance();
    G4Material* dci   = man->FindOrBuildMaterial(fCastIronMatName);
    G4Material* He    = man->FindOrBuildMaterial(fHeliumMatName);
    G4Material* pe    = man->FindOrBuildMaterial(fPEMatName);
    G4Material* steel = man->FindOrBuildMaterial(fSteelMatName);
    G4RotationMatrix* norot = nullptr;

    // --- CAST-IRON BODY (name preserved for surface-flux logic) ---
    auto* bodySolid = new G4Tubs("CastorBody", 0., fCaskOuterRadius,
                                 fCaskHeight/2.0, 0., 360.*deg);
    fCASTORBodyLog = new G4LogicalVolume(bodySolid, dci, "CastorBodyLog");
    fCASTORBodyLog->SetVisAttributes(new G4VisAttributes(G4Colour(0.20,0.45,0.85)));

    const G4double cavityZ = GetCavityZOffsetInBody();

    // --- HELIUM CAVITY ---
    auto* cavSolid = new G4Tubs("Cavity", 0., fCaskInnerRadius,
                                fCavityHeight/2.0, 0., 360.*deg);
    fCavityLog = new G4LogicalVolume(cavSolid, He, "CavityLog");
    auto* cavVis = new G4VisAttributes(G4Colour(0.9,0.9,0.9,0.1));
    fCavityLog->SetVisAttributes(cavVis);
    new G4PVPlacement(0, {0,0,cavityZ}, fCavityLog, "CavityPhys",
                      fCASTORBodyLog, false, 0, kCheckOverlaps);

    // --- TOP CLOSURE: 2 steel primaries (diff. radii) -> PE secondary -> steel protective ---
    G4double zTop = cavityZ + fCavityHeight/2.0;
    auto placeDisc = [&](const G4String& n, G4Material* m, G4double thk, G4double rad,
                         G4Colour col, G4LogicalVolume** keep) {
        auto* s = new G4Tubs(n, 0., rad, thk/2.0, 0., 360.*deg);
        auto* l = new G4LogicalVolume(s, m, n+"Log");
        l->SetVisAttributes(new G4VisAttributes(col));
        new G4PVPlacement(0, {0,0,zTop+thk/2.0}, l, n+"Phys",
                          fCASTORBodyLog, false, 0, kCheckOverlaps);
        zTop += thk; if (keep) *keep = l;
    };
    placeDisc("PrimaryLid1",  steel, fPrimaryLid1Thk,  fPrimaryLid1Rad,  G4Colour(0.55,0.55,0.60), &fPrimaryLid1Log);
    placeDisc("PrimaryLid2",  steel, fPrimaryLid2Thk,  fPrimaryLid2Rad,  G4Colour(0.50,0.50,0.56), &fPrimaryLid2Log);
    placeDisc("SecondaryLid", pe,    fSecondaryLidThk, fSecondaryLidRad, G4Colour(0.60,0.20,0.80), &fSecondaryLidLog);
    placeDisc("ProtectiveLid",steel, fProtectiveLidThk,fProtectiveLidRad,G4Colour(0.30,0.55,0.90), &fProtectiveLidLog);

    // top of the protective lid now ends at (zTop). With the auto-fit budget,
    // zTop = body-top - fTopIronGap, so the lid face is NOT coplanar with the
    // body face -> no z-fighting on the top seal.

    // --- 84 VVER-440 ASSEMBLIES (centred axially in the cavity) ---
    G4LogicalVolume* cellLV = BuildFuelAssemblyCell();
    for (size_t i = 0; i < fFuelPositions.size(); ++i)
        new G4PVPlacement(nullptr, fFuelPositions[i], cellLV, "FuelCellPhys",
                          fCavityLog, false, (G4int)i, kCheckOverlaps);

    // --- 76 PE MODERATOR HOLES (drilled in the iron wall, flush with bottom) ---
    const G4double modRodLength = fCaskHeight - fLidThickness - 2.*mm;
    const G4double modRodZ      = -fCaskHeight/2.0 + fModRodBottomClearance
                                + modRodLength/2.0;
    auto* rodSolid = new G4Tubs("ModRod", 0., fModRodRadius,
                                modRodLength/2.0, 0., 360.*deg);
    fModeratorRodLog = new G4LogicalVolume(rodSolid, pe, "ModRodLog");
    fModeratorRodLog->SetVisAttributes(new G4VisAttributes(G4Colour(0.60,0.20,0.80)));
    for (G4int i = 0; i < fNModRods; ++i) {
        const G4double a = i * (360.*deg/fNModRods);
        new G4PVPlacement(0, {fModRodRingRadius*std::cos(a),
                              fModRodRingRadius*std::sin(a), modRodZ},
                          fModeratorRodLog, "ModRodPhys",
                          fCASTORBodyLog, false, i, kCheckOverlaps);
    }

    // --- TRUNNION z-levels (lower & upper) ---
    fTrunnionZ[0] = cavityZ - fCavityHeight/2.0 + fTrunnionAxialInset;
    fTrunnionZ[1] = cavityZ + fCavityHeight/2.0 - fTrunnionAxialInset;

    // --- COOLING FINS: full fins + shaved fins around the trunnion pads ---
    auto* finFull = new G4Tubs("Fin", fCaskOuterRadius, fFinTipRadius,
                               fFinThickness/2.0, 0., 360.*deg);
    fFinLog = new G4LogicalVolume(finFull, dci, "FinLog");
    fFinLog->SetVisAttributes(new G4VisAttributes(G4Colour(0.20,0.45,0.85)));

    const G4double slotHalfX = (fFinTipRadius - fCaskOuterRadius)/2.0 + 20.*mm;
    const G4double slotCenX  = (fFinTipRadius + fCaskOuterRadius)/2.0;
    const G4double slotHalfY = fTrunnionRadius + 10.*mm;
    auto* slot    = new G4Box("FinSlot", slotHalfX, slotHalfY, fFinThickness);
    auto* finCut1 = new G4SubtractionSolid("FinCut1", finFull, slot, nullptr, {  slotCenX, 0, 0});
    auto* finCut  = new G4SubtractionSolid("FinCut",  finCut1, slot, nullptr, { -slotCenX, 0, 0});
    fFinCutLog = new G4LogicalVolume(finCut, dci, "FinCutLog");
    fFinCutLog->SetVisAttributes(new G4VisAttributes(G4Colour(0.20,0.45,0.85)));

    const G4double cutWindow = fTrunnionRadius + fFinThickness;
    const G4double finTop = cavityZ + fCavityHeight/2.0;
    const G4double finBot = cavityZ - fCavityHeight/2.0;
    const G4int    nFins  = (G4int)std::floor((finTop - finBot)/fFinSpacing);
    const G4double startZ = finBot + 0.5*fFinSpacing;
    G4int nCut = 0;
    for (G4int i = 0; i < nFins; ++i) {
        const G4double fz = startZ + i*fFinSpacing;
        const bool nearTrun = (std::fabs(fz - fTrunnionZ[0]) < cutWindow) ||
                              (std::fabs(fz - fTrunnionZ[1]) < cutWindow);
        if (nearTrun) ++nCut;
        G4ThreeVector finPos(0.,0.,fz);
        fCASTORAssembly->AddPlacedVolume(nearTrun ? fFinCutLog : fFinLog,
                                         finPos, norot);
    }

    // --- 4 TRUNNIONS through the shaved fin pads ---
    auto* trunSolid = new G4Tubs("Trunnion", 0., fTrunnionRadius,
                                 fTrunnionLength/2.0, 0., 360.*deg);
    fTrunnionLog = new G4LogicalVolume(trunSolid, steel, "TrunnionLog");
    fTrunnionLog->SetVisAttributes(new G4VisAttributes(G4Colour(0.40,0.40,0.40)));
    const G4double trR = fCaskOuterRadius + fTrunnionLength/2.0;
    const G4double trPhi[2] = { 0.*deg, 180.*deg };
    for (G4int iz = 0; iz < 2; ++iz)
        for (G4int ip = 0; ip < 2; ++ip) {
            auto* r = new G4RotationMatrix(); r->rotateY(90.*deg); r->rotateZ(trPhi[ip]);
            G4ThreeVector trPos(trR*std::cos(trPhi[ip]), trR*std::sin(trPhi[ip]), fTrunnionZ[iz]);
            fCASTORAssembly->AddPlacedVolume(fTrunnionLog, trPos, r);
        }

    G4ThreeVector castorBodyPos(0.,0.,0.);
    fCASTORAssembly->AddPlacedVolume(fCASTORBodyLog, castorBodyPos, norot);

    G4cout << " -> CASTOR440/84 done: " << fNModRods << " PE holes, lid stack "
           << fLidThickness/mm << " mm, " << nFins << " fins (" << nCut
           << " shaved), 4 trunnions, " << (int)fFuelPositions.size()
           << " assemblies" << G4endl;
    return 1;
}

//....oooOO0OOooo......

void GeometryCASTOR440::PlaceDetector(G4LogicalVolume* world, G4ThreeVector move,
                                      G4RotationMatrix* rotate, G4int copyNo)
{
    fCASTORAssembly->MakeImprint(world, move, rotate, copyNo, kCheckOverlaps);
}

//....oooOO0OOooo......
//  84-assembly basket: 5-ring hex lattice (91 sites) minus centre and 6 corners.
//....oooOO0OOooo......
void GeometryCASTOR440::GenerateFuelPositions()
{
    const G4double pitch = fAssyPitch;
    const G4double dx = pitch, dy = pitch*std::sqrt(3.0)/2.0;
    const G4int    R  = 5;
    fFuelPositions.clear(); fFuelPositions.reserve(84);
    for (G4int q = -R; q <= R; ++q) {
        const G4int r1 = std::max(-R,-q-R), r2 = std::min(R,-q+R);
        for (G4int r = r1; r <= r2; ++r) {
            const G4double x = dx*(q+r/2.0), y = dy*r;
            if (std::hypot(x,y) < 1e-6) continue;
            const G4double rmax = R*pitch;
            if (std::abs(std::hypot(x,y)-rmax) < 1e-3) {
                G4double ang = std::atan2(y,x)*180.0/CLHEP::pi; if (ang<0) ang+=360.;
                bool corner=false;
                for (G4double a=0.; a<360.; a+=60.) if (std::abs(ang-a)<1.0){corner=true;break;}
                if (corner) continue;
            }
            fFuelPositions.emplace_back(x,y,0.);
        }
    }
    //G4cout << " -> Generated " << (int)fFuelPositions.size() << " fuel positions" << G4endl;
}

G4ThreeVector GeometryCASTOR440::GetFuelPosition(G4int index) const
{
    if (index>=0 && index<(G4int)fFuelPositions.size()) return fFuelPositions[index];
    return G4ThreeVector();
}

//....oooOO0OOooo......
//  Uniform vertex inside the UO2 of the requested assembly, in cask-body frame.
//  A rod is chosen uniformly, then a point is area-weighted in the pellet
//  annulus [holeR, pelletOuterR] over the active column.
//....oooOO0OOooo......
G4ThreeVector GeometryCASTOR440::SampleUniformPointInFuel(G4int fuelIdx) const
{
    if (fuelIdx<0 || fuelIdx>=(G4int)fFuelPositions.size()) {
        G4cerr << " [SampleUniformPointInFuel] bad fuelIdx=" << fuelIdx << G4endl;
        return G4ThreeVector();
    }
    if (fRodPositions.empty())
        const_cast<GeometryCASTOR440*>(this)->GenerateRodPositions();

    const G4int    nRod = (G4int)fRodPositions.size();
    const G4int    ir   = std::min(nRod-1, (G4int)(G4UniformRand()*nRod));
    const G4ThreeVector& rod = fRodPositions[ir];

    const G4double rin2 = fPelletHoleR*fPelletHoleR;
    const G4double rou2 = fPelletOuterR*fPelletOuterR;
    const G4double rr   = std::sqrt(rin2 + G4UniformRand()*(rou2 - rin2));
    const G4double phi  = 2.0*CLHEP::pi*G4UniformRand();
    const G4double lx   = rod.x() + rr*std::cos(phi);
    const G4double ly   = rod.y() + rr*std::sin(phi);
    const G4double lz   = fActiveCenterZ + (G4UniformRand()-0.5)*fActiveFuelLength;

    return fFuelPositions[fuelIdx] + G4ThreeVector(lx, ly, lz)
         + G4ThreeVector(0., 0., GetCavityZOffsetInBody());
}

