#include "GeometryCAD.hh"

#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4GDMLParser.hh"
#include "G4Material.hh"

GeometryCAD::GeometryCAD() {}
GeometryCAD::~GeometryCAD() {}

// G4int GeometryCAD::Build()
// {
//     // validate=false: the FreeCAD file's xsi:noNamespaceSchemaLocation points at
//     // a CERN URL; on an offline compute node a validating Read() would try to
//     // fetch it and fail. Reading with validation off avoids that dependency.
//     G4GDMLParser parser;
//     parser.Read(fFileName, /*validate=*/false);
// 
//     fCADLog = parser.GetVolume(fVolumeName);   // pull out ONLY the part LV
//     G4String logName = "CADLog_" + fVolumeName;
//     fCADLog->SetName(logName);
//     if (!fCADLog) {
//         G4ExceptionDescription ed;
//         ed << "GDML volume \"" << fVolumeName << "\" not found in \""
//            << fFileName << "\". Check the <volume name=...> in the .gdml.";
//         G4Exception("GeometryCAD::Build", "NoVolume", FatalException, ed);
//         return 0;
//     }
// 
//     fCADLog->SetVisAttributes(
//         new G4VisAttributes(true, G4Colour(0.0, 0.7, 0.9, 0.6)));
// 
//     G4cout << " -> GeometryCAD: imported \"" << fVolumeName << "\" from "
//            << fFileName << " (material="
//            << (fCADLog->GetMaterial() ? fCADLog->GetMaterial()->GetName()
//                                       : G4String("<null>"))
//            << ")." << G4endl;
//     return 1;
// }

G4int GeometryCAD::Build()
{
    fCADLog = G4LogicalVolumeStore::GetInstance()->GetVolume(fVolumeName, false);
    if (!fCADLog) {
        G4ExceptionDescription ed;
        ed << "GDML volume \"" << fVolumeName << "\" not found. "
           << "Was its file read? Check <volume name=...>.";
        G4Exception("GeometryCAD::Build", "NoVolume", FatalException, ed);
        return 0;
    }
    fCADLog->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.,0.7,0.9,0.6)));
    return 1;
}

void GeometryCAD::PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                                G4RotationMatrix* rotate, G4int copyNo)
{
    G4String physName = "CADPhys_" + fVolumeName;
    new G4PVPlacement(rotate, move, fCADLog, physName,
                      worldLog, false, copyNo, /*checkOverlaps=*/true);
}

