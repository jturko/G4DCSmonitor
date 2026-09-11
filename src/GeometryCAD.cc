#include "GeometryCAD.hh"

#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4GDMLParser.hh"
#include "G4Material.hh"

#include <fstream>
#include <regex>
#include <set>

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

// G4int GeometryCAD::Build()
// {
//     fCADLog = G4LogicalVolumeStore::GetInstance()->GetVolume(fVolumeName, false);
//     if (!fCADLog) {
//         G4ExceptionDescription ed;
//         ed << "GDML volume \"" << fVolumeName << "\" not found. "
//            << "Was its file read? Check <volume name=...>.";
//         G4Exception("GeometryCAD::Build", "NoVolume", FatalException, ed);
//         return 0;
//     }
//     fCADLog->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.,0.7,0.9,0.6)));
//     return 1;
// }

G4int GeometryCAD::Build()
{
    auto* store = G4LogicalVolumeStore::GetInstance();
    
    G4cout << " --> Running GeometryCAD::Build(), logical volumes are available:" << G4endl;
    for (auto* lv : *store)
        if (lv) G4cerr << "    - " << lv->GetName() << G4endl;
    
    fCADLog = store->GetVolume(fVolumeName, false);
    if (!fCADLog) {
        G4cerr << "[GeometryCAD] Volume '" << fVolumeName
               << "' not found. " << store->size()
               << " logical volumes are available:" << G4endl;
        for (auto* lv : *store)
            if (lv) G4cerr << "    - " << lv->GetName() << G4endl;
        G4ExceptionDescription ed;
        ed << "GDML volume \"" << fVolumeName << "\" not found (see list above).";
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


void GeometryCAD::DumpGDMLNames(const G4String& file)
{
    std::ifstream in(file);
    if (!in) { G4cerr << "[GDML] cannot open " << file << G4endl; return; }

    std::stringstream ss; ss << in.rdbuf();
    const std::string txt = ss.str();

    std::set<std::string> solidDefs, solidRefs, volDefs;

    // <solid name="..."> : any tag that carries a name inside <solids>...</solids>
    auto solStart = txt.find("<solids>");
    auto solEnd   = txt.find("</solids>");
    if (solStart != std::string::npos && solEnd != std::string::npos) {
        std::string solids = txt.substr(solStart, solEnd - solStart);
        std::regex  re(R"(<[A-Za-z]+\s+name=\"([^\"]+)\")");
        for (std::sregex_iterator it(solids.begin(), solids.end(), re), end; it != end; ++it)
            solidDefs.insert((*it)[1]);
    }
    // every <solidref ref="...">
    {
        std::regex re(R"(<solidref\s+ref=\"([^\"]+)\")");
        for (std::sregex_iterator it(txt.begin(), txt.end(), re), end; it != end; ++it)
            solidRefs.insert((*it)[1]);
    }
    // every <volume name="...">
    {
        std::regex re(R"(<volume\s+name=\"([^\"]+)\")");
        for (std::sregex_iterator it(txt.begin(), txt.end(), re), end; it != end; ++it)
            volDefs.insert((*it)[1]);
    }

    G4cout << "\n===== RAW GDML SCAN of '" << file << "' =====" << G4endl;
    G4cout << "  <volume> names defined (" << volDefs.size() << "):" << G4endl;
    for (const auto& v : volDefs)   G4cout << "     volume : " << v << G4endl;
    G4cout << "  <solid>  names defined (" << solidDefs.size() << "):" << G4endl;
    for (const auto& s : solidDefs) G4cout << "     solid  : " << s << G4endl;
    G4cout << "  <solidref> requested   (" << solidRefs.size() << "):" << G4endl;
    for (const auto& r : solidRefs) G4cout << "     ref    : " << r << G4endl;

    G4cout << "  >>> solidref with NO matching solid definition:" << G4endl;
    bool bad = false;
    for (const auto& r : solidRefs)
        if (!solidDefs.count(r)) { G4cerr << "     MISSING: " << r << G4endl; bad = true; }
    if (!bad) G4cout << "     (none - all solidrefs resolve)" << G4endl;
    G4cout << "=================================================\n" << G4endl;
}
