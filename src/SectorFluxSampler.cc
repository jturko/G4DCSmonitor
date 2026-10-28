#include "SectorFluxSampler.hh"

#include "DetectorConstruction.hh"
#include "Randomize.hh"
#include "G4Exception.hh"
#include "G4RunManager.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"

#include "ROOT/RDataFrame.hxx"
#include "TFile.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

const char* SelectionName(SectorFluxSampler::SelectionMode mode)
{
    switch (mode) {
        case SectorFluxSampler::kExplicitCells: return "explicit cellIds";
        case SectorFluxSampler::kZLayerSector:  return "zLayers/sectorId";
        case SectorFluxSampler::kAutoCells:     return "autoCells";
        case SectorFluxSampler::kAllEntries:
        default:                                return "all entries";
    }
}

struct DedupKey {
    std::uint64_t sourceKey;
    G4int         evtNb;

    bool operator==(const DedupKey& other) const
    {
        return sourceKey == other.sourceKey && evtNb == other.evtNb;
    }
};

struct DedupKeyHash {
    std::size_t operator()(const DedupKey& k) const
    {
        const std::size_t h1 = std::hash<std::uint64_t>{}(k.sourceKey);
        const std::size_t h2 = std::hash<G4int>{}(k.evtNb);
        return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

// sourceKey = sourceCask | sourceType<<8 | hexant<<16 | globalFuel<<19
//             | batchId<<27
std::uint64_t PackSourceKey(double sourceCask, double sourceType,
                            double hexant, double globalFuel,
                            double batchId)
{
    auto asU64 = [](double v) -> std::uint64_t {
        const long long n = static_cast<long long>(std::llround(v));
        return n > 0 ? static_cast<std::uint64_t>(n) : 0u;
    };

    return (asU64(sourceCask) & 0xffULL)
         | ((asU64(sourceType) & 0xffULL) << 8)
         | ((asU64(hexant) & 0x7ULL) << 16)
         | ((asU64(globalFuel) & 0xffULL) << 19)
         | ((asU64(batchId) & 0x1fffffffffULL) << 27);
}

// Select every (zLayer, sector) cell whose intersection with the cask-frame
// interval of the oriented detector box is non-empty.
//
// The box is a parallelepiped in the cask frame.  Its radial minimum is the
// minimum of sqrt(x^2+y^2) over its 12 edges; its radial maximum is at a
// corner.  Its phi interval is represented by the box-centre azimuth and the
// maximum wrapped deviation of its corners.
std::vector<G4int> SelectAutoCells(const DetectorConstruction& detector,
                                   G4int nSectors, G4int nZLayers,
                                   G4double zMinMm, G4double dzMm)
{
    if (detector.GetNumCASTOR440s() < 1) {
        G4Exception("SectorFluxSampler::DoLoad", "AutoCellsNoCask",
                    FatalException,
                    "autoCells requires target cask 0, but no CASTOR 440 was built.");
        return {};
    }

    const G4ThreeVector caskPos = detector.GetCASTOR440Position(0);
    G4RotationMatrix caskInv;
    if (auto* caskRot = detector.GetCASTOR440Rotation(0)) {
        caskInv = caskRot->inverse();
    }

    const G4double rIn  = 1325. * mm;
    const G4double rOut = 1775. * mm;
    const G4double zMin = zMinMm * mm;
    const G4double dz   = dzMm * mm;
    const G4double tol  = 1e-6 * mm;

    constexpr G4double kTwoPi = 2. * M_PI;

    std::vector<G4int> cells;
    std::unordered_set<G4int> seen;
    G4int nEnvelopes = 0;

    const std::vector<DetectorConstruction::DetEnvelope> envelopes =
        detector.BuildDetectorEnvelopes();
    for (const DetectorConstruction::DetEnvelope& env : envelopes) {
        // Automatic selection currently covers the CLYC assemblies only.
        if (env.type != "CLYC") continue;
        ++nEnvelopes;

        const G4double hx = env.halfLength.x();
        const G4double hy = env.halfLength.y();
        const G4double hz = env.halfLength.z();

        // Eight corners in the cask-local frame.
        G4ThreeVector corner[8];
        for (int ix = 0; ix < 2; ++ix) {
            for (int iy = 0; iy < 2; ++iy) {
                for (int iz = 0; iz < 2; ++iz) {
                    const G4ThreeVector localCorner(
                        ix ? hx : -hx, iy ? hy : -hy, iz ? hz : -hz);
                    const G4ThreeVector globalCorner =
                        env.center + env.rotation * localCorner;
                    corner[(ix << 2) | (iy << 1) | iz] =
                        caskInv * (globalCorner - caskPos);
                }
            }
        }

        G4double zBoxMin = std::numeric_limits<G4double>::max();
        G4double zBoxMax = std::numeric_limits<G4double>::lowest();
        G4double rMin2 = std::numeric_limits<G4double>::max();
        G4double rMax2 = std::numeric_limits<G4double>::lowest();

        for (const G4ThreeVector& c : corner) {
            zBoxMin = std::min(zBoxMin, c.z());
            zBoxMax = std::max(zBoxMax, c.z());
            const G4double r2 = c.x() * c.x() + c.y() * c.y();
            rMin2 = std::min(rMin2, r2);
            rMax2 = std::max(rMax2, r2);
        }

        auto processEdge = [&rMin2](const G4ThreeVector& a,
                                    const G4ThreeVector& b) {
            const G4double ax = a.x(), ay = a.y();
            const G4double dx = b.x() - ax, dy = b.y() - ay;
            const G4double qa = dx * dx + dy * dy;
            const G4double qb = 2. * (ax * dx + ay * dy);
            if (qa <= 0.) return;
            G4double u = -qb / (2. * qa);
            u = std::min(1., std::max(0., u));
            const G4double ex = ax + u * dx;
            const G4double ey = ay + u * dy;
            rMin2 = std::min(rMin2, ex * ex + ey * ey);
        };

        for (int iy = 0; iy < 2; ++iy) {
            for (int iz = 0; iz < 2; ++iz) {
                processEdge(corner[(0 << 2) | (iy << 1) | iz],
                            corner[(1 << 2) | (iy << 1) | iz]);
            }
        }
        for (int ix = 0; ix < 2; ++ix) {
            for (int iz = 0; iz < 2; ++iz) {
                processEdge(corner[(ix << 2) | (0 << 1) | iz],
                            corner[(ix << 2) | (1 << 1) | iz]);
            }
        }
        for (int ix = 0; ix < 2; ++ix) {
            for (int iy = 0; iy < 2; ++iy) {
                processEdge(corner[(ix << 2) | (iy << 1) | 0],
                            corner[(ix << 2) | (iy << 1) | 1]);
            }
        }

        const G4double rMin = std::sqrt(std::max(0., rMin2));
        const G4double rMax = std::sqrt(std::max(0., rMax2));

        if (rMax + tol < rIn || rMin - tol > rOut) continue;

        const G4ThreeVector localCentre =
            caskInv * (env.center - caskPos);
        const G4double centreRadius = std::hypot(localCentre.x(),
                                                 localCentre.y());
        const G4double phiCentre = std::atan2(localCentre.y(),
                                              localCentre.x());

        G4double phiDelta = 0.;
        if (centreRadius > 1e-9 * mm) {
            for (const G4ThreeVector& c : corner) {
                const G4double phi = std::atan2(c.y(), c.x());
                const G4double delta = std::remainder(phi - phiCentre, kTwoPi);
                phiDelta = std::max(phiDelta, std::fabs(delta));
            }
        } else {
            phiDelta = M_PI;
        }

        const G4double dPhi = kTwoPi / static_cast<G4double>(nSectors);
        std::vector<G4bool> sectorSelected(nSectors, false);
        if (phiDelta >= M_PI) {
            std::fill(sectorSelected.begin(), sectorSelected.end(), true);
        } else {
            const G4double reach = phiDelta + 0.5 * dPhi;
            for (G4int s = 0; s < nSectors; ++s) {
                const G4double sectorCentre =
                    (static_cast<G4double>(s) + 0.5) * dPhi;
                const G4double delta = std::remainder(
                    sectorCentre - phiCentre, kTwoPi);
                if (std::fabs(delta) <= reach + 1e-12) {
                    sectorSelected[s] = true;
                }
            }
        }

        for (G4int iz = 0; iz < nZLayers; ++iz) {
            const G4double layerLow = zMin + static_cast<G4double>(iz) * dz;
            const G4double layerHigh = layerLow + dz;
            // Inclusive overlap: a box that touches a layer boundary selects
            // both layers that share that boundary.
            if (zBoxMax + tol < layerLow || zBoxMin - tol > layerHigh) {
                continue;
            }
            for (G4int s = 0; s < nSectors; ++s) {
                if (!sectorSelected[s]) continue;
                const G4int cellId = iz * nSectors + s;
                if (seen.insert(cellId).second) cells.push_back(cellId);
            }
        }
    }

    if (nEnvelopes == 0) {
        G4Exception("SectorFluxSampler::DoLoad", "AutoCellsNoEnvelope",
                    FatalException,
                    "autoCells found no CLYC envelope in BuildDetectorEnvelopes().");
        return {};
    }

    std::sort(cells.begin(), cells.end());
    if (cells.empty()) {
        G4Exception("SectorFluxSampler::DoLoad", "AutoCellsEmpty",
                    FatalException,
                    "autoCells selected no scoring cell; check the detector "
                    "placement against the scoring shell.");
    }
    return cells;
}

} // namespace

SectorFluxSampler& SectorFluxSampler::Instance()
{
    static SectorFluxSampler s;
    return s;
}

void SectorFluxSampler::EnsureLoaded() const
{
    std::call_once(fLoadOnce, [this]() {
        const_cast<SectorFluxSampler*>(this)->DoLoad();
    });
}

void SectorFluxSampler::DoLoad()
{
    if (fSourceFile.empty()) {
        G4Exception("SectorFluxSampler::DoLoad", "NoFile", FatalException,
                    "No sector source file set. Use /dcs-monitor/bkg/sourceFile.");
        return;
    }

    // ------------------------------------------------------------------
    // One TFile pass: read sectorMeta and inspect which optional sectorFlux
    // branches exist.  RDataFrame opens the file again for the data pass.
    // ------------------------------------------------------------------
    G4bool hasCellId = false;
    G4bool hasZLayer = false;
    G4bool hasEvtNb = false;
    G4bool hasSourceCask = false;
    G4bool hasSourceType = false;
    G4bool hasHexant = false;
    G4bool hasGlobalFuel = false;
    G4bool hasBatchId = false;
    G4bool hasSectorId = false;

    G4bool hasNZLayers = false;
    G4bool hasZMin = false;
    G4bool hasZMax = false;
    G4bool hasDz = false;

    {
        std::unique_ptr<TFile> f(TFile::Open(fSourceFile.c_str(), "READ"));
        if (!f || f->IsZombie()) {
            G4Exception("SectorFluxSampler::DoLoad", "OpenFail", FatalException,
                        ("Cannot open " + fSourceFile).c_str());
            return;
        }

        auto* meta = dynamic_cast<TTree*>(f->Get("sectorMeta"));
        if (meta && meta->GetEntries() > 0) {
            Double_t tgen = 0.;
            Int_t nSectors = 0;
            Int_t nZLayers = 0;
            Double_t zMin = 0.;
            Double_t zMax = 0.;
            Double_t dz = 0.;

            G4bool hasTgen = false;
            G4bool hasNSectors = false;

            TBranch* tgenBranch = meta->GetBranch("T_gen");
            if (!tgenBranch) tgenBranch = meta->GetBranch("Tgen");
            if (tgenBranch) {
                meta->SetBranchAddress(tgenBranch->GetName(), &tgen);
                hasTgen = true;
            }
            if (meta->GetBranch("nSectors")) {
                meta->SetBranchAddress("nSectors", &nSectors);
                hasNSectors = true;
            }
            if (meta->GetBranch("nZLayers")) {
                meta->SetBranchAddress("nZLayers", &nZLayers);
                hasNZLayers = true;
            }
            if (meta->GetBranch("zMin_mm")) {
                meta->SetBranchAddress("zMin_mm", &zMin);
                hasZMin = true;
            }
            if (meta->GetBranch("zMax_mm")) {
                meta->SetBranchAddress("zMax_mm", &zMax);
                hasZMax = true;
            }
            if (meta->GetBranch("dz_mm")) {
                meta->SetBranchAddress("dz_mm", &dz);
                hasDz = true;
            }

            meta->GetEntry(0);

            if (hasTgen) fTgen = tgen;
            if (hasNSectors) fNSectors = static_cast<G4int>(nSectors);
            if (hasNZLayers) fNZLayers = static_cast<G4int>(nZLayers);
            if (hasZMin) fZMin = static_cast<G4double>(zMin);
            if (hasZMax) fZMax = static_cast<G4double>(zMax);
            if (hasDz) fDz = static_cast<G4double>(dz);
        }
        if (fNSectors <= 0) fNSectors = 8;

        auto* tree = dynamic_cast<TTree*>(f->Get(fTreeName.c_str()));
        if (!tree) {
            G4Exception("SectorFluxSampler::DoLoad", "NoTree", FatalException,
                        ("Cannot find tree '" + fTreeName + "' in " +
                         fSourceFile).c_str());
            return;
        }

        hasCellId     = tree->GetBranch("cellId") != nullptr;
        hasZLayer     = tree->GetBranch("zLayer") != nullptr;
        hasEvtNb      = tree->GetBranch("evtNb") != nullptr;
        hasSourceCask = tree->GetBranch("sourceCask") != nullptr;
        hasSourceType = tree->GetBranch("sourceType") != nullptr;
        hasHexant     = tree->GetBranch("hexant") != nullptr;
        hasGlobalFuel = tree->GetBranch("globalFuel") != nullptr;
        hasBatchId    = tree->GetBranch("batchId") != nullptr;
        hasSectorId   = tree->GetBranch("sectorId") != nullptr;
    }

    if (fTgenOverride > 0.) fTgen = fTgenOverride;

    // ------------------------------------------------------------------
    // Resolve the selection precedence before touching RDataFrame.
    // ------------------------------------------------------------------
    SelectionMode mode = kAllEntries;
    if (!fCellIds.empty()) {
        mode = kExplicitCells;
    } else if (!fZLayers.empty() || fSectorId >= 0) {
        mode = kZLayerSector;
    } else if (fAutoCells) {
        mode = kAutoCells;
    }
    fSelectionMode = mode;

    const G4bool metaValid = hasNZLayers && hasZMin && hasZMax && hasDz
                          && fNZLayers > 0 && fZMax > fZMin && fDz > 0.;

    if (mode == kExplicitCells || mode == kAutoCells) {
        if (!hasCellId) {
            G4Exception(
                "SectorFluxSampler::DoLoad", "NoCellIdBranch", FatalException,
                ("Selection by cellIds/autoCells requires a 'cellId' branch "
                 "in tree '" + fTreeName + "' of " + fSourceFile +
                 "; this tree has no such branch.").c_str());
            return;
        }
        if (!metaValid) {
            G4Exception(
                "SectorFluxSampler::DoLoad", "NoCellMeta", FatalException,
                ("Selection by cellIds/autoCells requires sectorMeta branches "
                 "nZLayers, zMin_mm, zMax_mm, and dz_mm with nZLayers > 0, "
                 "zMax_mm > zMin_mm, and dz_mm > 0; one or more are missing or "
                 "invalid in " + fSourceFile + ".").c_str());
            return;
        }
    }

    if (mode == kZLayerSector) {
        if (!fZLayers.empty() && !hasZLayer) {
            G4Exception(
                "SectorFluxSampler::DoLoad", "NoZLayerBranch", FatalException,
                ("Selection by zLayers requires a 'zLayer' branch in tree '" +
                 fTreeName + "' of " + fSourceFile +
                 "; this tree has no such branch.").c_str());
            return;
        }
        if (fSectorId >= 0 && !hasSectorId) {
            G4Exception(
                "SectorFluxSampler::DoLoad", "NoSectorBranch", FatalException,
                ("Sector selection requires a 'sectorId' branch in tree '" +
                 fTreeName + "' of " + fSourceFile +
                 "; this tree has no such branch.").c_str());
            return;
        }
    }

    // Explicit cells and automatic cells both filter on cellId.  The automatic
    // list is computed here, before the RDataFrame filter is applied.
    if (mode == kExplicitCells) {
        fSelectedCells = fCellIds;
    } else if (mode == kAutoCells) {
        auto* runManager = G4RunManager::GetRunManager();
        const auto* base = runManager
            ? runManager->GetUserDetectorConstruction() : nullptr;
        const auto* detector = dynamic_cast<const DetectorConstruction*>(base);
        if (!detector) {
            G4Exception("SectorFluxSampler::DoLoad", "AutoCellsNoDetector",
                        FatalException,
                        "autoCells requires a DetectorConstruction available "
                        "through G4RunManager.");
            return;
        }
        fSelectedCells = SelectAutoCells(*detector, fNSectors, fNZLayers,
                                         fZMin, fDz);
        if (fSelectedCells.empty()) return;
    }

    G4cout << "[SectorFluxSampler] loading " << fSourceFile
           << " (selection=" << SelectionName(mode)
           << ", sectorId=" << fSectorId
           << ", maxEntries=" << fMaxEntries
           << ", T_gen=" << fTgen << " s)" << G4endl;

    ROOT::RDataFrame df(fTreeName, fSourceFile);
    ROOT::RDF::RNode node = df;

    if (mode == kExplicitCells || mode == kAutoCells) {
        std::unordered_set<G4int> cellSet(fSelectedCells.begin(),
                                          fSelectedCells.end());
        node = node.Filter(
            [cellSet](double cellId) {
                return cellSet.count(
                    static_cast<G4int>(std::llround(cellId))) > 0;
            },
            {"cellId"});
    } else if (mode == kZLayerSector) {
        if (fSectorId >= 0) {
            const G4int requestedSector = fSectorId;
            node = node.Filter(
                [requestedSector](double sectorId) {
                    return static_cast<G4int>(std::llround(sectorId))
                           == requestedSector;
                },
                {"sectorId"});
        }
        if (!fZLayers.empty()) {
            std::unordered_set<G4int> zSet(fZLayers.begin(), fZLayers.end());
            node = node.Filter(
                [zSet](double zLayer) {
                    return zSet.count(
                        static_cast<G4int>(std::llround(zLayer))) > 0;
                },
                {"zLayer"});
        }
    }

    // maxEntries is a diagnostic cap on the filtered sample.  It biases the
    // unique sample, so it is applied before deduplication by design.
    if (fMaxEntries > 0) {
        node = node.Range(0, (ULong64_t)fMaxEntries);
    }

    // Bind optional columns through RDataFrame defaults when absent.  This
    // keeps the two ForeachSlot paths below from binding a missing branch.
    auto ensureOptionalColumn = [&node](G4bool present,
                                        const std::string& realName,
                                        const std::string& fallbackName) {
        if (present) return realName;
        node = node.Define(fallbackName, "0.0");
        return fallbackName;
    };

    const std::string evtNbCol = ensureOptionalColumn(
        hasEvtNb, "evtNb", "__sfs_default_evtNb");
    const std::string sourceCaskCol = ensureOptionalColumn(
        hasSourceCask, "sourceCask", "__sfs_default_sourceCask");
    const std::string sourceTypeCol = ensureOptionalColumn(
        hasSourceType, "sourceType", "__sfs_default_sourceType");
    const std::string hexantCol = ensureOptionalColumn(
        hasHexant, "hexant", "__sfs_default_hexant");
    const std::string globalFuelCol = ensureOptionalColumn(
        hasGlobalFuel, "globalFuel", "__sfs_default_globalFuel");
    const std::string batchIdCol = ensureOptionalColumn(
        hasBatchId, "batchId", "__sfs_default_batchId");
    const std::string zLayerCol = ensureOptionalColumn(
        hasZLayer, "zLayer", "__sfs_default_zLayer");

    std::vector<std::string> columns = {
        "pid", "ekin", "t", "x", "y", "z", "px", "py", "pz",
        evtNbCol, sourceCaskCol, sourceTypeCol, hexantCol,
        globalFuelCol, batchIdCol
    };

    const unsigned int nSlots = std::max(1u, df.GetNSlots());
    std::vector<std::vector<Entry>> perSlot(nSlots);

    if (hasCellId) {
        std::vector<std::string> cols = columns;
        cols.push_back("cellId");
        cols.push_back(zLayerCol);

        node.ForeachSlot(
            [&perSlot](unsigned int slot,
                       double pid, double ekin, double t,
                       double x, double y, double z,
                       double px, double py, double pz,
                       double evtNb,
                       double sourceCask, double sourceType,
                       double hexant, double globalFuel,
                       double batchId,
                       double cellId, double zLayer)
            {
                (void)zLayer; // cellId carries the same information.
                const double pMag = std::sqrt(px * px + py * py + pz * pz);
                if (pMag <= 0.) return;

                Entry e;
                e.pid = static_cast<G4int>(std::llround(pid));
                e.ekin = static_cast<G4float>(ekin);
                e.t = static_cast<G4float>(t);
                e.x = static_cast<G4float>(x);
                e.y = static_cast<G4float>(y);
                e.z = static_cast<G4float>(z);
                e.px = static_cast<G4float>(px / pMag);
                e.py = static_cast<G4float>(py / pMag);
                e.pz = static_cast<G4float>(pz / pMag);
                e.evtNb = static_cast<G4int>(std::llround(evtNb));
                e.sourceKey = PackSourceKey(sourceCask, sourceType, hexant,
                                            globalFuel, batchId);
                e.cellId = static_cast<G4int>(std::llround(cellId));
                perSlot[slot].push_back(e);
            },
            cols);
    } else {
        // Legacy path: the all-sector replay may run without cellId/zLayer.
        node.ForeachSlot(
            [&perSlot](unsigned int slot,
                       double pid, double ekin, double t,
                       double x, double y, double z,
                       double px, double py, double pz,
                       double evtNb,
                       double sourceCask, double sourceType,
                       double hexant, double globalFuel,
                       double batchId)
            {
                const double pMag = std::sqrt(px * px + py * py + pz * pz);
                if (pMag <= 0.) return;

                Entry e;
                e.pid = static_cast<G4int>(std::llround(pid));
                e.ekin = static_cast<G4float>(ekin);
                e.t = static_cast<G4float>(t);
                e.x = static_cast<G4float>(x);
                e.y = static_cast<G4float>(y);
                e.z = static_cast<G4float>(z);
                e.px = static_cast<G4float>(px / pMag);
                e.py = static_cast<G4float>(py / pMag);
                e.pz = static_cast<G4float>(pz / pMag);
                e.evtNb = static_cast<G4int>(std::llround(evtNb));
                e.sourceKey = PackSourceKey(sourceCask, sourceType, hexant,
                                            globalFuel, batchId);
                e.cellId = -1;
                perSlot[slot].push_back(e);
            },
            columns);
    }

    std::size_t total = 0;
    for (const auto& v : perSlot) total += v.size();

    std::vector<Entry> all;
    all.reserve(total);
    for (auto& v : perSlot) {
        all.insert(all.end(), std::make_move_iterator(v.begin()),
                   std::make_move_iterator(v.end()));
        std::vector<Entry>().swap(v);
    }

    fMLoaded = static_cast<long long>(all.size());

    // Deduplication key is (sourceKey, evtNb), not trackID.  Keep the entry
    // with the smallest t, then the smaller cellId, then the earlier load
    // order.  The per-event key removes repeated crossings of the same source
    // particle through internal cell boundaries.
    if (hasEvtNb) {
        std::unordered_map<DedupKey, std::size_t, DedupKeyHash> best;
        best.reserve(all.size());
        for (std::size_t i = 0; i < all.size(); ++i) {
            const DedupKey key{all[i].sourceKey, all[i].evtNb};
            auto it = best.find(key);
            if (it == best.end()) {
                best.emplace(key, i);
                continue;
            }
            const Entry& candidate = all[i];
            const Entry& current = all[it->second];
            if (candidate.t < current.t) {
                it->second = i;
            } else if (candidate.t == current.t &&
                       candidate.cellId < current.cellId) {
                it->second = i;
            }
        }

        std::vector<Entry> unique;
        unique.reserve(best.size());
        for (std::size_t i = 0; i < all.size(); ++i) {
            const DedupKey key{all[i].sourceKey, all[i].evtNb};
            auto it = best.find(key);
            if (it != best.end() && it->second == i) {
                unique.push_back(std::move(all[i]));
            }
        }
        fEntries = std::move(unique);
    } else {
        G4cout << "[SectorFluxSampler] no 'evtNb' branch; deduplication "
               << "skipped for this file." << G4endl;
        fEntries = std::move(all);
    }

    fM = static_cast<long long>(fEntries.size());
    fLoaded = true;

    const long long removed = fMLoaded - fM;
    G4cout << "[SectorFluxSampler] M_loaded=" << fMLoaded
           << ", M_unique=" << fM
           << ", removed=" << removed
           << ", T_gen=" << fTgen << " s"
           << ", selection=" << SelectionName(fSelectionMode)
           << G4endl;

    if (!fSelectedCells.empty()) {
        G4cout << "[SectorFluxSampler] selected cells:";
        for (G4int cellId : fSelectedCells) G4cout << " " << cellId;
        G4cout << G4endl;
    }

    if (mode == kZLayerSector && !fZLayers.empty()) {
        G4cout << "[SectorFluxSampler] selected zLayers:";
        for (G4int zLayer : fZLayers) G4cout << " " << zLayer;
        G4cout << " (sectorId=" << fSectorId << ")" << G4endl;
    }

    if (fEntries.empty()) {
        G4Exception("SectorFluxSampler::DoLoad", "NoEntries", FatalException,
                    "No sectorFlux entries matched the requested selection.");
    }
}

G4bool SectorFluxSampler::ForceLoad()
{
    EnsureLoaded();
    return fLoaded && !fEntries.empty();
}

bool SectorFluxSampler::Sample(G4ThreeVector& pos,
                               G4ThreeVector& dir,
                               G4double& ekin,
                               G4double& t,
                               G4int& pid) const
{
    EnsureLoaded();
    if (fEntries.empty()) return false;

    const std::size_t i =
        (std::size_t)(G4UniformRand() * (G4double)fEntries.size());
    const Entry& e = fEntries[i];

    pos  = G4ThreeVector(e.x * mm, e.y * mm, e.z * mm);
    dir  = G4ThreeVector(e.px, e.py, e.pz).unit();
    ekin = e.ekin * MeV;
    t    = e.t * s;
    pid  = e.pid;
    return true;
}

void SectorFluxSampler::StartRun(G4double measurement_time)
{
    if (!ForceLoad()) {
        G4Exception("SectorFluxSampler::StartRun", "LoadFail", FatalException,
                    "No usable sector flux entries loaded.");
        return;
    }
    if (fTgen <= 0.) {
        G4Exception("SectorFluxSampler::StartRun", "NoTgen", FatalException,
                    "T_gen is not available; set /dcs-monitor/bkg/tgen.");
        return;
    }
    if (measurement_time <= 0.) {
        G4Exception("SectorFluxSampler::StartRun", "BadTime", FatalException,
                    "Measurement time must be positive.");
        return;
    }

    const G4double tSec = measurement_time / s;
    long long n = fNumPrimariesOverride;
    if (n <= 0) {
        n = (long long)std::llround((G4double)fM * tSec / fTgen);
    }

    if (n < 1) {
        G4Exception("SectorFluxSampler::StartRun", "TooFew", JustWarning,
                    "Computed replay count < 1; not starting a run.");
        return;
    }

    constexpr long long kMaxBeamOn = 2147483647LL;
    if (n > kMaxBeamOn) n = kMaxBeamOn;

    G4cout << "[SectorFluxSampler::StartRun] M_loaded=" << fMLoaded
           << ", M_unique=" << fM
           << ", T_gen=" << fTgen << " s, T_replay=" << tSec
           << " s -> N_replay=" << n << " primaries" << G4endl;

    G4UImanager::GetUIpointer()->ApplyCommand("/run/beamOn " + std::to_string(n));
}
