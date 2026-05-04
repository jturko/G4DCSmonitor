#include "PrimaryGeneratorAction.hh"

#include "DetectorConstruction.hh"
#include "GeometryCASTOR440.hh"
#include "HistoManager.hh"
#include "RootManager.hh"
#include "PrimaryGeneratorMessenger.hh"
#include "RunAction.hh"

#include "G4Event.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include "G4PhysicalVolumeStore.hh"

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

    // 3. Create the primary vertex in the event...
    fParticleGun->GeneratePrimaryVertex(anEvent);

    // 4. ...and stamp it with the correct weight.
    G4PrimaryVertex* vertex =
        anEvent->GetPrimaryVertex(anEvent->GetNumberOfPrimaryVertex() - 1);
    if (vertex) vertex->SetWeight(weight);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

