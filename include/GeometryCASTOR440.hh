#ifndef GEOMETRYCASTOR440_HH
#define GEOMETRYCASTOR440_HH 1

#include "G4VUserDetectorConstruction.hh"
#include "G4Colour.hh"
#include <vector>

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
    G4LogicalVolume*   GetFinLog()     { return fFinLog;  }
    G4VPhysicalVolume* GetCASTORPhys() { return fCASTORBodyPhys; }

    void          GenerateFuelPositions();
    G4ThreeVector GetFuelPosition(G4int index) const;
    G4int         GetNumFuelAssemblies() const { return (G4int)fFuelPositions.size(); }

    // -------------------------------------------------------------------------
    // Geometry parameter accessors (the interface used by PrimaryGeneratorAction
    // so that any change of dimensions here automatically propagates).
    // -------------------------------------------------------------------------
    G4double GetCaskHeight()           const { return fCaskHeight;        }
    G4double GetCaskInnerRadius()      const { return fCaskInnerRadius;   }
    G4double GetCaskOuterRadius()      const { return fCaskOuterRadius;   }
    G4double GetFinTipRadius()         const { return fFinTipRadius;      }
    G4double GetCavityHeight()         const { return fCavityHeight;      }
    G4double GetLidThickness()         const { return fLidThickness;      }
    G4double GetBottomThickness()      const { return fBottomThickness;   }
    G4double GetActiveFuelLength()     const { return fActiveFuelLength;  }
    G4double GetTotalFuelLength()      const { return fTotalFuelLength;   }

    // Hexagonal fuel-assembly parameters (must match values used in Build()).
    // fAssyApothem is the value passed as G4Polyhedra rOuter -> circumscribed
    // (vertex) radius of the hexagon.
    G4double GetFuelHexCircumRadius()  const { return fAssyApothem;       }
    G4double GetFuelHexPhiStart()      const { return fAssyPhiStart;      }
    G4double GetFuelAssemblyPitch()    const { return fAssyPitch;         }

    // Z-coordinate of the centre of CavityLog inside CastorBodyLog
    G4double GetCavityZOffsetInBody()  const {
        return -fCaskHeight/2.0 + fBottomThickness + fCavityHeight/2.0;
    }

    // Sample a uniformly distributed point inside the specified fuel assembly,
    // returned in the **cask body local frame** (i.e. before the cask global
    // placement transform is applied).
    G4ThreeVector SampleUniformPointInFuel(G4int fuelIdx) const;

  private:
    void BuildMaterials();

    G4AssemblyVolume*  fCASTORAssembly;

    G4VPhysicalVolume* fCASTORBodyPhys;

    G4LogicalVolume* fCASTORBodyLog;
    G4LogicalVolume* fCavityLog;
    G4LogicalVolume* fLidLog;
    G4LogicalVolume* fFuelAssemblyLog;
    G4LogicalVolume* fModeratorRodLog;
    G4LogicalVolume* fFinLog;

    // Cask dimensions
    G4double fCaskHeight;
    G4double fCaskInnerRadius;
    G4double fCaskOuterRadius;
    G4double fFinTipRadius;
    G4double fCavityHeight;
    G4double fLidThickness;
    G4double fBottomThickness;
    G4double fActiveFuelLength;
    G4double fTotalFuelLength;

    // Hexagonal fuel-assembly parameters
    G4double fAssyApothem;   // -> G4Polyhedra rOuter (circumradius of the hex)
    G4double fAssyPhiStart;  // -> G4Polyhedra phiStart (orientation)
    G4double fAssyPitch;     // basket pitch (centre-to-centre)

    // Materials
    G4String fCastIronMatName;
    G4String fHeliumMatName;
    G4String fPEMatName;
    G4String fFuelMatName;
    G4String fSteelMatName;

    std::vector<G4ThreeVector> fFuelPositions;
};

#endif

