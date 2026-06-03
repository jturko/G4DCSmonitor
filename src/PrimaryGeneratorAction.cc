#include "PrimaryGeneratorAction.hh"

#include "DetectorConstruction.hh"
#include "GeometryCASTOR440.hh"
#include "HistoManager.hh"
#include "RootManager.hh"
#include "PrimaryGeneratorMessenger.hh"
#include "RunAction.hh"
#include "SurfaceFluxSampler.hh"

#include "G4Event.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include "G4PhysicalVolumeStore.hh"

#include "G4RunManager.hh"
#include "G4UImanager.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::PrimaryGeneratorAction(DetectorConstruction* det)
    : fDetector(det)
{
    fPrimaryGeneratorMessenger = new PrimaryGeneratorMessenger(this);
    fParticleGun = new G4ParticleGun(1);
    fGPS         = new G4GeneralParticleSource;
    fSourceMode  = kGPS;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
    delete fParticleGun;
    delete fGPS;
    delete fPrimaryGeneratorMessenger;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void 
PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
    switch (fSourceMode) {
        case kGPS:
            fGPS->GeneratePrimaryVertex(anEvent);
            break;

        case kCASTOR440_surface:
            GenerateVertexCASTOR440SurfaceFlux(anEvent);
            break;
    
        case kCASTOR440_surface_from_TTree:
            GenerateVertexCASTOR440SurfaceFromTree(anEvent);
            break;

        case kCASTOR440_fuel:
            GenerateVertexCASTOR440FuelFlux(anEvent);
            break;

        case kCASTOR440_fuel_biased:
            GenerateVertexCASTOR440FuelFluxWithGeomBias(anEvent);
            break;

        default:
            G4Exception("PrimaryGeneratorAction::GeneratePrimaries()",
                        "UnknownSourceMode", FatalException,
                        "Unknown source mode.");
            return;
    }

    // Optional primary-vertex ntuple
    if (RunAction::WritePrimaryTree) {
        G4PrimaryVertex* vertex =
            anEvent->GetPrimaryVertex(anEvent->GetNumberOfPrimaryVertex() - 1);
        if (!vertex) return;

        G4PrimaryParticle* primary = vertex->GetPrimary(0);
        if (!primary) return;

        G4int         particle = primary->GetPDGcode();
        G4double      ekin     = primary->GetKineticEnergy();
        G4double      time     = vertex->GetT0();
        G4ThreeVector pos      = vertex->GetPosition();
        G4ThreeVector mom      = primary->GetMomentumDirection();

        G4AnalysisManager* analysis = G4AnalysisManager::Instance();
        const G4int idx = 0;
        analysis->FillNtupleDColumn(idx, 0, particle);
        analysis->FillNtupleDColumn(idx, 1, ekin);
        analysis->FillNtupleDColumn(idx, 2, time);
        analysis->FillNtupleDColumn(idx, 3, pos.x());
        analysis->FillNtupleDColumn(idx, 4, pos.y());
        analysis->FillNtupleDColumn(idx, 5, pos.z());
        analysis->FillNtupleDColumn(idx, 6, mom.x());
        analysis->FillNtupleDColumn(idx, 7, mom.y());
        analysis->FillNtupleDColumn(idx, 8, mom.z());
        analysis->AddNtupleRow(idx);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void 
PrimaryGeneratorAction::SetSourceMode(SourceMode mode)
{
    fSourceMode = mode;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Surface flux from the cylindrical body of a CASTOR 440. Reads the cask
// geometry directly so that any future change to fCaskHeight / fFinTipRadius
// in GeometryCASTOR440 propagates here automatically.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void 
PrimaryGeneratorAction::GenerateVertexCASTOR440SurfaceFlux(G4Event* anEvent)
{
    const G4int numCasks = fDetector->GetNumCASTOR440s();
    if (fCaskNum < 0 || fCaskNum >= numCasks) {
        G4Exception("PrimaryGeneratorAction", "InvalidCaskIndex",
                    FatalException, "Cask index out of range.");
        return;
    }

    GeometryCASTOR440* cask    = fDetector->GetCASTOR440(fCaskNum);
    G4ThreeVector      caskPos = fDetector->GetCASTOR440Position(fCaskNum);
    G4RotationMatrix*  caskRot = fDetector->GetCASTOR440Rotation(fCaskNum);

    const G4double R = cask->GetCaskOuterRadius();   // outer radial extent
    const G4double H = cask->GetCaskHeight();

    const G4double areaTop   = M_PI * R * R;
    const G4double areaSide  = 2.0 * M_PI * R * H;
    const G4double areaTotal = 2.0 * areaTop + areaSide;

    const G4double randArea = G4UniformRand() * areaTotal;
    G4ThreeVector  localPos, localNormal;

    if (randArea < areaTop) {
        const G4double r   = R * std::sqrt(G4UniformRand());
        const G4double phi = 2.0 * M_PI * G4UniformRand();
        localPos    = G4ThreeVector(r * std::cos(phi), r * std::sin(phi),  H/2.0);
        localNormal = G4ThreeVector(0., 0.,  1.);
    } else if (randArea < 2.0 * areaTop) {
        const G4double r   = R * std::sqrt(G4UniformRand());
        const G4double phi = 2.0 * M_PI * G4UniformRand();
        localPos    = G4ThreeVector(r * std::cos(phi), r * std::sin(phi), -H/2.0);
        localNormal = G4ThreeVector(0., 0., -1.);
    } else {
        const G4double z   = H * (G4UniformRand() - 0.5);
        const G4double phi = 2.0 * M_PI * G4UniformRand();
        localPos    = G4ThreeVector(R * std::cos(phi), R * std::sin(phi), z);
        localNormal = G4ThreeVector(std::cos(phi), std::sin(phi), 0.);
    }

    G4ThreeVector globalPos    = localPos;
    G4ThreeVector globalNormal = localNormal;
    if (caskRot) {
        globalPos.transform(*caskRot);
        globalNormal.transform(*caskRot);
    }
    globalPos += caskPos;

    // Cosine-weighted hemispherical emission about the outward surface normal.
    const G4double cosTheta = G4UniformRand();
    const G4double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
    const G4double phiEmit  = 2.0 * M_PI * G4UniformRand();
    G4ThreeVector  globalDir(sinTheta * std::cos(phiEmit),
                             sinTheta * std::sin(phiEmit),
                             cosTheta);
    globalDir.rotateUz(globalNormal);

    fParticleGun->SetParticlePosition(globalPos);
    fParticleGun->SetParticleMomentumDirection(globalDir);

    fParticleGun->GeneratePrimaryVertex(anEvent);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Shared primitives
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4ThreeVector 
PrimaryGeneratorAction::SetVertexPositionInFuel()
{
    // 1) Validate cask index
    const G4int numCasks = fDetector->GetNumCASTOR440s();
    if (fCaskNum < 0 || fCaskNum >= numCasks) {
        G4Exception("PrimaryGeneratorAction::SetVertexPositionInFuel",
                    "InvalidCaskIndex", FatalException,
                    "Configured cask index is out of range.");
        return G4ThreeVector();
    }

    // 2) Validate fuel index against actual count from the geometry
    GeometryCASTOR440* cask = fDetector->GetCASTOR440(fCaskNum);
    const G4int numFuel = cask ? cask->GetNumFuelAssemblies() : 0;
    if (fFuelNum < 0 || fFuelNum >= numFuel) {
        G4ExceptionDescription ed;
        ed << "Fuel index " << fFuelNum
           << " out of range [0, " << numFuel << ").";
        G4Exception("PrimaryGeneratorAction::SetVertexPositionInFuel",
                    "InvalidFuelIndex", FatalException, ed);
        return G4ThreeVector();
    }

    // 3) Delegate the actual sampling to the geometry (single source of truth)
    return fDetector->SampleUniformGlobalPositionInFuel(fCaskNum, fFuelNum);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void 
PrimaryGeneratorAction::SetVertexDirectionIsotropic()
{
    const G4double cosTheta = 2.0 * G4UniformRand() - 1.0;
    const G4double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
    const G4double phi      = 2.0 * M_PI * G4UniformRand();
    fParticleGun->SetParticleMomentumDirection(
        G4ThreeVector(sinTheta * std::cos(phi),
                      sinTheta * std::sin(phi),
                      cosTheta));
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4double
PrimaryGeneratorAction::SetVertexDirectionIsotropicWithGeomBias(const G4ThreeVector& vertexPos)
{
    const G4int numCLYC = fDetector->GetNumCLYC();
    if (numCLYC == 0) {
        G4Exception("PrimaryGeneratorAction::SetVertexDirectionIsotropicWithGeomBias",
                    "NoCLYC", FatalException,
                    "No CLYC detectors found to bias the primary direction towards.");
        return 1.0;
    }

    // Uniformly select one of the CLYC detectors to aim at.
    const G4int          clycIndex = (G4int)(G4UniformRand() * numCLYC);
    //const G4ThreeVector  clycPos   = fDetector->GetCLYCPosition(clycIndex); // front face of PE plug
    const G4ThreeVector  clycPos   = fDetector->GetCLYCCrystalPosition(clycIndex); // CLYC crystal COM

    // Vector from the primary vertex to the chosen CLYC detector.
    const G4ThreeVector  toDet    = clycPos - vertexPos;
    const G4double       distance = toDet.mag();

    // Half-opening angle of the cone that exactly circumscribes the CLYC
    // bounding sphere, seen from the vertex position.
    G4double sinThetaMax = (distance > 0.) ? fCLYCBoundingRadius / distance : 1.0;
    if (sinThetaMax > 1.0) sinThetaMax = 1.0;              // vertex inside sphere
    const G4double cosThetaMax = std::sqrt(1.0 - sinThetaMax * sinThetaMax);

    // Sample a direction uniformly in solid angle within that cone
    // (cosTheta uniform in [cosThetaMax, 1]).
    const G4double cosTheta = 1.0 - G4UniformRand() * (1.0 - cosThetaMax);
    const G4double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
    const G4double phi      = 2.0 * M_PI * G4UniformRand();

    G4ThreeVector localDir(sinTheta * std::cos(phi),
                           sinTheta * std::sin(phi),
                           cosTheta);
    // Rotate so the cone axis points from the vertex to the CLYC.
    localDir.rotateUz(toDet.unit());

    fParticleGun->SetParticleMomentumDirection(localDir);

    // ------------------------------------------------------------------
    // Statistical weight.
    // Biased PDF for the direction is uniform in the cone of solid angle
    // Omega_cone = 2*pi*(1 - cosThetaMax), and we pick 1 of numCLYC detectors
    // with probability 1/numCLYC.
    //   p_bias(Omega) = (1/numCLYC) * 1/Omega_cone
    // True (isotropic) PDF is 1/(4*pi).
    //   weight = p_true / p_bias
    //          = (1/(4*pi)) / ((1/numCLYC) * 1/(2*pi*(1-cosThetaMax)))
    //          = numCLYC * (1 - cosThetaMax) / 2
    // ------------------------------------------------------------------
    const G4double fractionalSolidAngle = 0.5 * (1.0 - cosThetaMax);
    const G4double weight               = numCLYC * fractionalSolidAngle;

    return weight;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Unbiased fuel flux: uniform vertex in the requested fuel assembly with an
// isotropic emission direction. Implemented entirely in terms of the shared
// primitives above, so any change in GeometryCASTOR440 dimensions is picked
// up automatically.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void 
PrimaryGeneratorAction::GenerateVertexCASTOR440FuelFlux(G4Event* anEvent)
{
    const G4ThreeVector vertexPos = SetVertexPositionInFuel();
    fParticleGun->SetParticlePosition(vertexPos);
    SetVertexDirectionIsotropic();
    
    if (fUseWattSpectrum) {
        G4ParticleDefinition* neutron = G4ParticleTable::GetParticleTable()->FindParticle("neutron");
        fParticleGun->SetParticleDefinition(neutron);        
        fParticleGun->SetParticleEnergy(SampleWattSpectrum());
    }

    fParticleGun->GeneratePrimaryVertex(anEvent);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Biased fuel flux: same spatial sampling as the unbiased mode, but the
// emission direction is constrained to a cone that bounds the selected CLYC
// detector. The correct statistical weight is attached to the primary vertex.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void
PrimaryGeneratorAction::GenerateVertexCASTOR440FuelFluxWithGeomBias(G4Event* anEvent)
{
    // 1. Shared spatial sampling.
    const G4ThreeVector vertexPos = SetVertexPositionInFuel();
    fParticleGun->SetParticlePosition(vertexPos);

    // 2. Directional bias towards CLYC; returns the required statistical weight.
    const G4double weight = SetVertexDirectionIsotropicWithGeomBias(vertexPos);
    
    // 2.5 If Watt spectrum, override particle type + energy
    if (fUseWattSpectrum) {
        G4ParticleDefinition* neutron = G4ParticleTable::GetParticleTable()->FindParticle("neutron");
        fParticleGun->SetParticleDefinition(neutron);        
        fParticleGun->SetParticleEnergy(SampleWattSpectrum());
    }

    // 3. Create the primary vertex in the event...
    fParticleGun->GeneratePrimaryVertex(anEvent);

    // 4. ...and stamp it with the correct weight.
    G4PrimaryVertex* vertex =
        anEvent->GetPrimaryVertex(anEvent->GetNumberOfPrimaryVertex() - 1);
    if (vertex) vertex->SetWeight(weight);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Watt fission spectrum sampler.
//
// Implements the Cashwell–Everett rejection algorithm (LA-9721-MS, also used
// by MCNP). For parameters a [energy], b [1/energy]:
//     K = 1 + b/(8a)
//     L = (K + sqrt(K^2 - 1)) / a
//     M = a*L - 1
//   loop:
//     x = -ln(xi1), y = -ln(xi2)
//     accept if (y - M*(x+1))^2 <= b*L*x
//   return E = a*L*x
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
G4double PrimaryGeneratorAction::SampleWattSpectrum() const
{
    const G4double a = fWattA;
    const G4double b = fWattB;

    if (a <= 0. || b <= 0.) {
        G4Exception("PrimaryGeneratorAction::SampleWattSpectrum",
                    "InvalidWattParams", FatalException,
                    "Watt parameters a and b must be strictly positive.");
        return 0.;
    }

    const G4double K = 1.0 + (b / (8.0 * a));
    const G4double L = (K + std::sqrt(K * K - 1.0)) / a;
    const G4double M = a * L - 1.0;

    // Hard cap on rejection iterations as a safety net.
    for (G4int trial = 0; trial < 10000; ++trial) {
        const G4double x = -std::log(G4UniformRand());
        const G4double y = -std::log(G4UniformRand());
        const G4double t = y - M * (x + 1.0);
        if (t * t <= b * L * x) {
            return a * L * x;     // sampled neutron kinetic energy
        }
    }

    G4Exception("PrimaryGeneratorAction::SampleWattSpectrum",
                "WattRejectionStuck", JustWarning,
                "Watt sampler exceeded 10000 rejections; returning mean energy.");
    return 2.0 * a;   // fallback ~ <E> for typical fission Watts
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorAction::GenerateVertexCASTOR440SurfaceFromTree(G4Event* event)
{
    auto& s = SurfaceFluxSampler::Instance();

    // Push only the cylinder dimensions of the current cask. Placement is
    // *not* needed here: the input tree is already in cask-local coordinates.
    if (auto* cask = fDetector->GetCASTOR440(fCaskNum)) {
        s.SetGeometryParameters(cask->GetCaskOuterRadius() / CLHEP::mm,
                                cask->GetCaskHeight()      / CLHEP::mm,
                                /*tol mm*/ 2.0);
    } else {
        G4Exception("PrimaryGeneratorAction", "NoCask", FatalException,
                    "Cask not constructed; did /run/initialize run before /run/beamOn?");
        return;
    }

    G4ThreeVector posLocal, dirLocal;
    G4double      ekin, weight;
    G4int         pid;

    if (!s.Sample(posLocal, dirLocal, ekin, weight, pid)) {
        G4Exception("PrimaryGeneratorAction", "SurfaceSampleFail",
                    FatalException, "No matching crossings in sampler.");
        return;
    }

    // Local (step-1 cask frame) -> global (step-2 cask frame).
    G4ThreeVector globalPos = posLocal;
    G4ThreeVector globalDir = dirLocal;
    if (auto* rot = fDetector->GetCASTOR440Rotation(fCaskNum)) {
        globalPos.transform(*rot);
        globalDir.transform(*rot);
    }
    globalPos += fDetector->GetCASTOR440Position(fCaskNum);

    auto* def = G4ParticleTable::GetParticleTable()->FindParticle(pid);
    if (!def) {
        G4ExceptionDescription ed; ed << "Unknown PDG code " << pid;
        G4Exception("PrimaryGeneratorAction", "BadPID", FatalException, ed);
        return;
    }

    fParticleGun->SetParticleDefinition(def);
    fParticleGun->SetParticleEnergy(ekin * MeV);
    fParticleGun->SetParticlePosition(globalPos);
    fParticleGun->SetParticleMomentumDirection(globalDir.unit());
    fParticleGun->GeneratePrimaryVertex(event);

    auto* v = event->GetPrimaryVertex(event->GetNumberOfPrimaryVertex() - 1);
    if (v) v->SetWeight(weight);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

