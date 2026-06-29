#ifndef GEOMETRYCASTOR440_HH
#define GEOMETRYCASTOR440_HH 1

#include "G4VUserDetectorConstruction.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include <vector>
#include <cmath>

class G4AssemblyVolume;
class G4VPhysicalVolume;
class G4LogicalVolume;

class GeometryCASTOR440
{
  public:
    GeometryCASTOR440();
    ~GeometryCASTOR440();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* expHallLog, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo);

    G4LogicalVolume*   GetCASTORLog()  { return fCASTORBodyLog;  }
    G4LogicalVolume*   GetFinLog()     { return fFinLog;         }
    G4LogicalVolume*   GetFinCutLog()  { return fFinCutLog;      }
    G4VPhysicalVolume* GetCASTORPhys() { return fCASTORBodyPhys; }

    void          GenerateFuelPositions();
    void          GenerateRodPositions();
    G4ThreeVector GetFuelPosition(G4int index) const;
    G4int         GetNumFuelAssemblies() const { return (G4int)fFuelPositions.size(); }

    // ---- parameter interface used by PrimaryGeneratorAction / SurfaceFluxSampler ----
    G4double GetCaskHeight()           const { return fCaskHeight;       }
    G4double GetCaskInnerRadius()      const { return fCaskInnerRadius;  }
    G4double GetCaskOuterRadius()      const { return fCaskOuterRadius;  }
    G4double GetFinTipRadius()         const { return fFinTipRadius;     }
    G4double GetCavityHeight()         const { return fCavityHeight;     }
    G4double GetLidThickness()         const { return fLidThickness;     }
    G4double GetBottomThickness()      const { return fBottomThickness;  }
    G4double GetActiveFuelLength()     const { return fActiveFuelLength; }
    G4double GetTotalFuelLength()      const { return fTotalFuelLength;  }

    G4double GetFuelHexCircumRadius()  const { return fShroudApothemOut / std::cos(CLHEP::pi/6.); }
    G4double GetFuelHexPhiStart()      const { return fAssyPhiStart;     }
    G4double GetFuelAssemblyPitch()    const { return fAssyPitch;        }

    G4double GetCavityZOffsetInBody()  const {
        return -fCaskHeight/2.0 + fBottomThickness + fCavityHeight/2.0;
    }

    // Uniform vertex inside the UO2 of the requested assembly, in cask-body frame.
    G4ThreeVector SampleUniformPointInFuel(G4int fuelIdx) const;

  private:
    void             BuildMaterials();
    G4LogicalVolume* BuildFuelAssemblyCell();   // one full VVER-440 assembly

    G4AssemblyVolume*  fCASTORAssembly;
    G4VPhysicalVolume* fCASTORBodyPhys;

    G4LogicalVolume* fCASTORBodyLog;
    G4LogicalVolume* fCavityLog;
    G4LogicalVolume* fPrimaryLid1Log;
    G4LogicalVolume* fPrimaryLid2Log;
    G4LogicalVolume* fSecondaryLidLog;
    G4LogicalVolume* fProtectiveLidLog;
    G4LogicalVolume* fFuelCellLog;
    G4LogicalVolume* fModeratorRodLog;
    G4LogicalVolume* fFinLog;
    G4LogicalVolume* fFinCutLog;
    G4LogicalVolume* fTrunnionLog;

    // ---- cask body (drawing-confirmed) ----
    G4double fCaskHeight, fCaskInnerRadius, fCaskOuterRadius;
    G4double fRibHeight, fFinTipRadius;
    G4double fCavityHeight, fBottomThickness;

    // ---- top closure: 2 steel primaries (diff. radii) + PE secondary + steel protective ----
    G4double fPrimaryLid1Thk,  fPrimaryLid1Rad;
    G4double fPrimaryLid2Thk,  fPrimaryLid2Rad;
    G4double fSecondaryLidThk, fSecondaryLidRad;   // PE
    G4double fProtectiveLidThk,fProtectiveLidRad;
    G4double fLidThickness;                        // sum (after auto-fit)
    G4double fTopIronGap;                          // iron cap above top lid (anti z-fighting)

    // ---- fuel-length envelope ----
    G4double fActiveFuelLength, fTotalFuelLength;

    // ---- wall moderator holes ----
    G4int    fNModRods;
    G4double fModRodRadius, fModRodRingRadius, fModRodBottomClearance;

    // ---- cooling fins ----
    G4double fFinThickness, fFinSpacing;

    // ---- trunnions ----
    G4double fTrunnionRadius, fTrunnionLength, fTrunnionAxialInset;
    G4double fTrunnionZ[2];

    // ---- basket / assembly cell (apothem = G4Polyhedra tangent radius) ----
    G4double fAssyPitch, fAssyPhiStart, fCellApothem, fBasketWall,
             fShroudApothemOut, fShroudWall;

    // ---- axial assembly structure (cell-local) ----
    G4double fShroudLength;
    G4double fNozzleBotLen, fNozzleTopLen;
    G4double fRodLength, fEndPlugLen, fPlenumLen;
    G4double fRodCenterZ, fActiveCenterZ;

    // ---- spacer grids ----
    G4int    fNSpacerGrids;
    G4double fSpacerGridHeight, fSpacerBandRadial;

    // ---- VVER-440 rod ----
    G4int    fNRodRings;
    G4double fRodPitch, fCladOuterR, fCladInnerR, fPelletOuterR,
             fPelletHoleR, fCentralTubeOuterR, fCentralTubeInnerR;

    // ---- materials ----
    G4String fCastIronMatName, fHeliumMatName, fPEMatName, fFuelMatName,
             fSteelMatName, fCladMatName, fShroudMatName;

    std::vector<G4ThreeVector> fFuelPositions;  // 84 assembly centres (cavity frame)
    std::vector<G4ThreeVector> fRodPositions;   // 126 rod centres (cell frame)
};

#endif

