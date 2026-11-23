#include "SurfaceFluxSampler.hh"
#include "WSSAReader.hh"

#include "Randomize.hh"
#include "G4SystemOfUnits.hh"
#include "G4Exception.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4RunManager.hh"

#include "ProgressBar.hh"

#include "TFile.h"
#include "TTree.h"
#include "TError.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <queue>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <fstream>
#include <sstream>

#include "ROOT/RDataFrame.hxx"
#include "ROOT/RDF/RInterface.hxx"   // ROOT::RDF::RNode
#include "TROOT.h"                   // EnableImplicitMT / DisableImplicitMT / IsImplicitMTEnabled
#include "TStopwatch.h"
#include <thread>
#include <limits>

SurfaceFluxSampler& SurfaceFluxSampler::Instance()
{
    static SurfaceFluxSampler s;
    return s;
}

// Walker's alias method, O(N) build, O(1) sample.
void SurfaceFluxSampler::BuildAlias(Bucket& b)
{
    const std::size_t N = b.data.size();
    b.prob.assign(N, 0.0);
    b.alias.assign(N, 0);
    if (N == 0) return;

    long double sum = 0.0L;
    for (auto& c : b.data) sum += std::max<G4float>(c.weight, 0.f);
    if (sum <= 0.0L) {
        for (auto& c : b.data) c.weight = 1.f;
        sum = static_cast<long double>(N);
    }

    std::vector<G4double> p(N);
    for (std::size_t i = 0; i < N; ++i)
        p[i] = (b.data[i].weight > 0.f ? b.data[i].weight : 0.) * N / sum;

    std::queue<std::size_t> small, large;
    for (std::size_t i = 0; i < N; ++i)
        (p[i] < 1.0 ? small : large).push(i);

    while (!small.empty() && !large.empty()) {
        std::size_t s = small.front(); small.pop();
        std::size_t l = large.front(); large.pop();
        b.prob[s]  = p[s];
        b.alias[s] = static_cast<G4int>(l);
        p[l] = (p[l] + p[s]) - 1.0;
        (p[l] < 1.0 ? small : large).push(l);
    }
    while (!large.empty()) { b.prob[large.front()] = 1.0; large.pop(); }
    while (!small.empty()) { b.prob[small.front()] = 1.0; small.pop(); }
}

//....oooOO0OOooo  SINGLE-PASS loader (no rejection, no preprocessing)  oooOO...

void SurfaceFluxSampler::Load(const std::string& filename,
                              const std::string& treeName,
                              G4double /*R_mm*/, G4double /*H_mm*/,
                              G4double /*tol_mm*/)
{
    if (fLoaded) return;

    const G4double R     = fPendingR_mm;
    const G4double Hhalf = 0.5 * fPendingH_mm;

    // maxEntries (Range) is incompatible with IMT in RDataFrame, so the capped
    // debug path runs single-threaded; the full production path runs MT.
    const bool capped = (fSurfaceSourceMaxEntriesLoadedFromTree > 0);

    gErrorIgnoreLevel = kError;
    TStopwatch sw; sw.Start();

    // --- configure ROOT implicit MT for this scan only ---
    bool weEnabledMT = false;
    if (!capped) {
        if (!ROOT::IsImplicitMTEnabled()) {
            const unsigned int req = (fLoadThreads <= 0)
                                   ? 0u   // 0 => use all hardware threads
                                   : static_cast<unsigned int>(fLoadThreads);
            ROOT::EnableImplicitMT(req);
            weEnabledMT = true;
        }
    } else if (ROOT::IsImplicitMTEnabled()) {
        ROOT::DisableImplicitMT();         // Range() needs IMT off
    }

    // cheap entry count (reads metadata only) for reserve + logging
    Long64_t nTotal = 0;
    {
        std::unique_ptr<TFile> probe(TFile::Open(filename.c_str(), "READ"));
        if (probe && !probe->IsZombie())
            if (auto* tt = dynamic_cast<TTree*>(probe->Get(treeName.c_str())))
                nTotal = tt->GetEntries();
    }
    const Long64_t N = capped
        ? std::min<Long64_t>(fSurfaceSourceMaxEntriesLoadedFromTree, nTotal)
        : nTotal;

    G4cout << "[SurfaceFluxSampler] Scanning " << N << " entries from '"
           << filename << "' using "
           << (capped ? 1u
                      : (ROOT::IsImplicitMTEnabled()
                            ? std::max(1u, std::thread::hardware_concurrency()) : 1u))
           << " thread(s)..." << G4endl;

    // --- build the (optionally capped) dataframe ---
    ROOT::RDataFrame df(treeName, filename);
    ROOT::RDF::RNode node = df;
    if (capped) node = df.Range(0, N);

    const unsigned int nSlots = std::max(1u, df.GetNSlots());
    std::vector<std::vector<Crossing>> perSlot(nSlots);
    {
        const Long64_t guess = (N > 0 ? N : 0) / nSlots + 16;
        for (auto& v : perSlot) v.reserve((std::size_t)guess);
    }

    // --- the parallel scan: each slot fills its own buffer, no locks ---
    node.ForeachSlot(
        [&perSlot, R, Hhalf](unsigned int slot,
                             double pid,  double ekin,
                             double x,    double y,    double z,
                             double px,   double py,   double pz,
                             double w)
        {
            const double pMag = std::sqrt(px*px + py*py + pz*pz);
            if (pMag <= 0.) return;                       // guard, not a rejection

            const double rLoc     = std::hypot(x, y);
            const double distSide = std::fabs(rLoc - R);
            const double distCap  = std::fabs(Hhalf - std::fabs(z));

            Crossing c;
            c.pid    = (G4int)llround(pid);
            c.ekin   = (G4float)ekin;
            c.x      = (G4float)x;  c.y = (G4float)y;  c.z = (G4float)z;
            c.px     = (G4float)(px/pMag);
            c.py     = (G4float)(py/pMag);
            c.pz     = (G4float)(pz/pMag);
            c.weight = (G4float)w;
            c.surf   = (distSide <= distCap) ? 0 : ((z >= 0.) ? 1 : 2);

            perSlot[slot].push_back(c);
        },
        {"pid","ekin","x","y","z","px","py","pz","weight"});

    // --- single-threaded merge + tally ---
    std::size_t total = 0;
    for (auto& v : perSlot) total += v.size();

    fAll.data.clear();
    fAll.data.reserve(total);

    Long64_t nSide=0, nTop=0, nBot=0, nNeutron=0, nGamma=0;
    Double_t wSide=0, wTop=0, wBot=0;
    for (auto& v : perSlot) {
        for (auto& c : v) {
            fAll.data.push_back(c);
            if      (c.surf==0){ ++nSide; wSide+=c.weight; }
            else if (c.surf==1){ ++nTop;  wTop +=c.weight; }
            else               { ++nBot;  wBot +=c.weight; }
            if      (c.pid==2112) ++nNeutron;
            else if (c.pid==22)   ++nGamma;
        }
        std::vector<Crossing>().swap(v);   // free slot buffer promptly
    }

    // restore ROOT to single-threaded for the rest of the Geant4 job
    if (weEnabledMT) ROOT::DisableImplicitMT();

    BuildAlias(fAll);

    fKeptSide        = nSide;
    fKeptSideWeight  = wSide;
    fKeptWeightTotal = wSide + wTop + wBot;
    fLoaded = true;
    gErrorIgnoreLevel = kWarning;

    sw.Stop();
    G4cout << "[SurfaceFluxSampler] Loaded " << fAll.data.size()
           << " crossings in " << sw.RealTime() << " s"
           << " (n=" << nNeutron << ", gamma=" << nGamma << ")\n"
           << "   surfaces: side=" << nSide << " (w=" << wSide << "), "
           << "top=" << nTop << " (w=" << wTop << "), "
           << "bottom=" << nBot << " (w=" << wBot << ")\n"
           << "   total kept weight = " << fKeptWeightTotal << G4endl;
}

//   void SurfaceFluxSampler::Load(const std::string& filename,
//                                 const std::string& treeName,
//                                 G4double /*R_mm*/, G4double /*H_mm*/,
//                                 G4double /*tol_mm*/)
//   {
//       if (fLoaded) return;
//   
//       std::unique_ptr<TFile> f(TFile::Open(filename.c_str(), "READ"));
//       if (!f || f->IsZombie()) {
//           G4Exception("SurfaceFluxSampler::Load", "OpenFail", FatalException,
//                       ("Cannot open " + filename).c_str());
//           return;
//       }
//       auto* t = dynamic_cast<TTree*>(f->Get(treeName.c_str()));
//       if (!t) {
//           G4Exception("SurfaceFluxSampler::Load", "NoTree", FatalException,
//                       ("No tree " + treeName + " in " + filename).c_str());
//           return;
//       }
//   
//       gErrorIgnoreLevel = kError;
//       t->SetCacheSize(512LL * 1024 * 1024);
//   
//       // Read only what we need: skip 't' and 'evtNb' to cut I/O.
//       Double_t pid, ekin, x, y, z, px, py, pz, w;
//       t->SetBranchStatus("*", 0);
//       for (auto br : {"pid","ekin","x","y","z","px","py","pz","weight"})
//           t->SetBranchStatus(br, 1);
//       t->AddBranchToCache("pid",1);   t->AddBranchToCache("ekin",1);
//       t->AddBranchToCache("x",1);     t->AddBranchToCache("y",1);
//       t->AddBranchToCache("z",1);     t->AddBranchToCache("px",1);
//       t->AddBranchToCache("py",1);    t->AddBranchToCache("pz",1);
//       t->AddBranchToCache("weight",1);
//       t->StopCacheLearningPhase();
//   
//       t->SetBranchAddress("pid",    &pid);
//       t->SetBranchAddress("ekin",   &ekin);
//       t->SetBranchAddress("x",      &x);
//       t->SetBranchAddress("y",      &y);
//       t->SetBranchAddress("z",      &z);
//       t->SetBranchAddress("px",     &px);
//       t->SetBranchAddress("py",     &py);
//       t->SetBranchAddress("pz",     &pz);
//       t->SetBranchAddress("weight", &w);
//   
//       const Long64_t N = (fSurfaceSourceMaxEntriesLoadedFromTree > 0 &&
//                           fSurfaceSourceMaxEntriesLoadedFromTree <= t->GetEntries())
//                        ? fSurfaceSourceMaxEntriesLoadedFromTree : t->GetEntries();
//   
//       fAll.data.clear();
//       fAll.data.reserve(N);
//   
//       const G4double R     = fPendingR_mm;
//       const G4double Hhalf = 0.5 * fPendingH_mm;
//   
//       Long64_t nSide=0, nTop=0, nBot=0, nNeutron=0, nGamma=0, nDegenerate=0;
//       Double_t wSide=0, wTop=0, wBot=0;
//   
//       ProgressBar prog(N);
//       for (Long64_t i = 0; i < N; ++i) {
//           if (i % 1000000 == (1000000-1)) prog.Print(i);
//           t->GetEntry(i);
//   
//           const G4double pMag = std::sqrt(px*px + py*py + pz*pz);
//           if (pMag <= 0.) { ++nDegenerate; continue; }   // not a rejection, just guard
//   
//           const G4double rLoc = std::hypot(x, y);
//   
//           // Classify by NEAREST surface (every entry is a real crossing, so we
//           // never drop one; this just decides how Sample() will smear it).
//           const G4double distSide = std::fabs(rLoc - R);
//           const G4double distCap  = std::fabs(Hhalf - std::fabs(z));
//           G4int surf;
//           if (distSide <= distCap) surf = 0;          // cylindrical side
//           else                     surf = (z >= 0.) ? 1 : 2;  // top / bottom cap
//   
//           Crossing c;
//           c.pid    = static_cast<G4int>(pid);
//           c.ekin   = static_cast<G4float>(ekin);
//           c.x      = (G4float)x;  c.y = (G4float)y;  c.z = (G4float)z;
//           c.px     = (G4float)(px/pMag);
//           c.py     = (G4float)(py/pMag);
//           c.pz     = (G4float)(pz/pMag);
//           c.weight = static_cast<G4float>(w);
//           c.surf   = surf;
//           fAll.data.push_back(c);
//   
//           if      (surf==0){ ++nSide; wSide+=w; }
//           else if (surf==1){ ++nTop;  wTop +=w; }
//           else             { ++nBot;  wBot +=w; }
//           if      (c.pid==2112) ++nNeutron;
//           else if (c.pid==22)   ++nGamma;
//       }
//   
//       BuildAlias(fAll);
//   
//       fKeptSide        = nSide;
//       fKeptSideWeight  = wSide;
//       fKeptWeightTotal = wSide + wTop + wBot;
//       fLoaded = true;
//       gErrorIgnoreLevel = kWarning;
//   
//       G4cout << "[SurfaceFluxSampler] Single-pass load of " << fAll.data.size()
//              << " crossings (n=" << nNeutron << ", gamma=" << nGamma
//              << (nDegenerate ? (", skipped degenerate=" + std::to_string(nDegenerate)) : std::string())
//              << ")\n"
//              << "   surfaces: side=" << nSide << " (w=" << wSide << "), "
//              << "top=" << nTop << " (w=" << wTop << "), "
//              << "bottom=" << nBot << " (w=" << wBot << ")\n"
//              << "   total kept weight = " << fKeptWeightTotal << G4endl;
//       t->PrintCacheStats();
//   }

//....oooOO0OOooo  CSV loader for MCNP surface-crossing data  oooOO0OOooo....
//
// Expected CSV columns (tab or comma separated, optional header line):
//   pid  x  y  z  u  v  w  erg  wgt
// Units: cm, MeV, unitless weight.
// MCNP pid mapping: 1 -> neutron (PDG 2112), 2 -> gamma (PDG 22).
// Positions are converted cm -> mm; direction (u,v,w) is normalised.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SurfaceFluxSampler::LoadCSV(const std::string& filename,
                                 G4double R_mm,
                                 G4double H_mm,
                                 G4double /*tol_mm*/)
{
    if (fLoaded) return;

    const G4double Hhalf = 0.5 * H_mm;
    const G4double cm2mm = 10.0;

    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        G4Exception("SurfaceFluxSampler::LoadCSV", "OpenFail", FatalException,
                    ("Cannot open CSV file: " + filename).c_str());
        return;
    }

    TStopwatch sw; sw.Start();

    std::string line;
    long long nLines = 0, nNeutron = 0, nGamma = 0, nSkipped = 0;
    long long nSide = 0, nTop = 0, nBot = 0;
    G4double wSide = 0, wTop = 0, wBot = 0;

    fAll.data.clear();

    // Helper: strip trailing comma/tab from a token
    auto stripSep = [](std::string& tok) {
        while (!tok.empty() && (tok.back() == ',' || tok.back() == '\t'))
            tok.pop_back();
    };

    // Helper: parse one line into a Crossing, return true on success
    auto parseLine = [&](const std::string& ln, Crossing& out) -> bool {
        std::istringstream iss(ln);
        std::string pidStr;
        if (!(iss >> pidStr)) return false;
        stripSep(pidStr);

        double pid_raw;
        try { pid_raw = std::stod(pidStr); }
        catch (...) { return false; }

        double fields[8];
        for (int i = 0; i < 8; ++i) {
            std::string tok;
            if (!(iss >> tok)) return false;
            stripSep(tok);
            try { fields[i] = std::stod(tok); }
            catch (...) { return false; }
        }

        const double x = fields[0], y = fields[1], z = fields[2];
        const double u = fields[3], v = fields[4], w = fields[5];
        const double erg = fields[6], wgt = fields[7];

        // MCNP pid -> PDG code
        G4int pdgPid;
        if (pid_raw == 1.0)      pdgPid = 2112;   // neutron
        else if (pid_raw == 2.0) pdgPid = 22;     // gamma
        else return false;

        // Direction normalisation
        const G4double pMag = std::sqrt(u*u + v*v + w*w);
        if (pMag <= 0.) return false;

        // Convert cm -> mm.
        // The MCNP CSV places the cask on the floor: its axis runs from
        // z=0 (bottom) to z=H (top). The cask-local frame used everywhere else
        // (TTree loader, Sample(), the G4Tubs body) is centered at the origin,
        // spanning z in [-H/2, +H/2]. Shift z down by H/2 to map the floor-
        // based data into that centered frame.
        const G4double x_mm = x * cm2mm;
        const G4double y_mm = y * cm2mm;
        const G4double z_mm = z * cm2mm - Hhalf;

        // Surface classification (same logic as TTree loader)
        const G4double rLoc     = std::hypot(x_mm, y_mm);
        const G4double distSide = std::fabs(rLoc - R_mm);
        const G4double distCap  = std::fabs(Hhalf - std::fabs(z_mm));
        G4int surf = (distSide <= distCap) ? 0 : ((z_mm >= 0.) ? 1 : 2);

        out.pid    = pdgPid;
        out.ekin   = (G4float)erg;
        out.x      = (G4float)x_mm;
        out.y      = (G4float)y_mm;
        out.z      = (G4float)z_mm;
        out.px     = (G4float)(u / pMag);
        out.py     = (G4float)(v / pMag);
        out.pz     = (G4float)(w / pMag);
        out.weight = (G4float)wgt;
        out.surf   = surf;
        return true;
    };

    bool headerChecked = false;
    while (std::getline(ifs, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Detect header line on first data attempt
        if (!headerChecked) {
            headerChecked = true;
            std::istringstream test(line);
            std::string firstTok;
            test >> firstTok;
            stripSep(firstTok);
            try {
                std::size_t pos;
                std::stod(firstTok, &pos);
                if (pos != firstTok.size()) throw std::invalid_argument("trailing");
            } catch (...) {
                continue;   // header line, skip
            }
        }

        Crossing c;
        if (!parseLine(line, c)) { ++nSkipped; continue; }

        fAll.data.push_back(c);
        ++nLines;

        if (c.surf == 0)      { ++nSide; wSide += c.weight; }
        else if (c.surf == 1) { ++nTop;  wTop  += c.weight; }
        else                  { ++nBot;  wBot  += c.weight; }
        if (c.pid == 2112)    ++nNeutron;
        else                  ++nGamma;
    }

    ifs.close();

    if (fAll.data.empty()) {
        G4Exception("SurfaceFluxSampler::LoadCSV", "NoData", FatalException,
                    ("No valid entries found in CSV file: " + filename).c_str());
        return;
    }

    BuildAlias(fAll);

    fKeptSide        = nSide;
    fKeptSideWeight  = wSide;
    fKeptWeightTotal = wSide + wTop + wBot;
    fLoaded = true;

    sw.Stop();
    G4cout << "[SurfaceFluxSampler::LoadCSV] Loaded " << fAll.data.size()
           << " crossings from '" << filename << "' in " << sw.RealTime()
           << " s (n=" << nNeutron << ", gamma=" << nGamma
           << (nSkipped ? (", skipped=" + std::to_string(nSkipped)) : std::string())
           << ")\n"
           << "   surfaces: side=" << nSide << " (w=" << wSide << "), "
           << "top=" << nTop << " (w=" << wTop << "), "
           << "bottom=" << nBot << " (w=" << wBot << ")\n"
           << "   total kept weight = " << fKeptWeightTotal << G4endl;
}

//....oooOO0OOooo  WSSA loader for MCNP binary surface-source data  oooOO0OOooo
//
// The WSSA file stores a surface crossing in each particle record.  A single
// sequential pass reads all h.nrsw records and keeps a uniform reservoir of at
// most fWSSAMaxEntries records.  The alias table then samples the retained
// records in proportion to their weights, and Sample() returns weight = 1, as
// for the CSV and TTree paths.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SurfaceFluxSampler::LoadWSSA(const std::string& filename,
                                  G4double /*R_mm*/,
                                  G4double H_mm,
                                  G4double /*tol_mm*/)
{
    if (fLoaded) return;

    const G4double Hhalf = 0.5 * H_mm;
    const G4double cm2mm = 10.0;

    wssa::Reader reader;
    std::string err;
    if (!reader.open(filename, err)) {
        G4Exception("SurfaceFluxSampler::LoadWSSA", "OpenFail", FatalException,
                    ("Cannot open WSSA file: " + filename + " (" + err + ")").c_str());
        return;
    }
    if (!reader.readHeader(err)) {
        G4Exception("SurfaceFluxSampler::LoadWSSA", "HeaderFail", FatalException,
                    ("Cannot read WSSA header: " + err).c_str());
        return;
    }

    const wssa::Header& h = reader.header();
    if (h.nrsw <= 0 || h.particleRecordBytes <= 0) {
        G4Exception("SurfaceFluxSampler::LoadWSSA", "NoRecords", FatalException,
                    "WSSA file contains no particle records.");
        return;
    }

    // Particle type: explicit PDG code, otherwise infer from the file name.
    G4int pdg = fWSSAPid;
    if (pdg != 22 && pdg != 2112) {
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (lower.find("gamma") != std::string::npos) {
            pdg = 22;
        } else if (lower.find("neutron") != std::string::npos) {
            pdg = 2112;
        } else {
            G4Exception("SurfaceFluxSampler::LoadWSSA", "PidUnknown", FatalException,
                        ("Cannot infer the particle type from '" + filename +
                         "'. Set /dcs-monitor/surf/wssaPid 2112 (neutron) "
                         "or 22 (gamma).").c_str());
            return;
        }
    }

    // Map WSSA surface identifiers to the side/top/bottom classification used
    // by Sample().  The files in use store the side as MCNP type 15 (cylinder
    // parallel to z) and the top as type 4 (plane normal to z).
    std::map<int, G4int> surfById;
    for (const auto& s : h.surfaces) {
        if (s.type == 15) {
            surfById[s.jss] = 0;                                  // side
        } else if (s.type == 4) {
            surfById[s.jss] = (s.coeff >= 0.0) ? 1 : 2;            // top/bottom
        } else {
            surfById[s.jss] = 0;
        }
    }

    const long long Nmax = (fWSSAMaxEntries > 0)
        ? std::min<long long>(fWSSAMaxEntries, h.nrsw)
        : h.nrsw;

    std::vector<Crossing> reservoir;
    reservoir.reserve(static_cast<std::size_t>(
        std::min<long long>(Nmax, 10000000LL)));

    std::mt19937_64 rng(12345);
    long long nValid = 0;
    long long nDegenerate = 0;
    long long nUnknownSurf = 0;
    long long nNonPositiveW = 0;
    G4double sumW = 0.0;

    TStopwatch sw; sw.Start();
    wssa::Record rec;
    for (int64_t i = 0; i < h.nrsw; ++i) {
        if (!reader.next(rec, err)) {
            G4ExceptionDescription ed;
            ed << "WSSA read stopped after " << i << " of " << h.nrsw
               << " records: " << err;
            G4Exception("SurfaceFluxSampler::LoadWSSA", "ReadFail",
                        FatalException, ed);
            return;
        }

        const double u = rec.u;
        const double v = rec.v;
        const double w = rec.w();
        const double pMag = std::sqrt(u*u + v*v + w*w);
        if (!(pMag > 0.0)) {
            ++nDegenerate;
            continue;
        }

        Crossing c;
        c.pid    = pdg;
        c.ekin   = static_cast<G4float>(rec.erg);
        c.x      = static_cast<G4float>(rec.x * cm2mm);
        c.y      = static_cast<G4float>(rec.y * cm2mm);
        c.z      = static_cast<G4float>(rec.z * cm2mm - Hhalf);
        c.px     = static_cast<G4float>(u / pMag);
        c.py     = static_cast<G4float>(v / pMag);
        c.pz     = static_cast<G4float>(w / pMag);

        const double wgt = std::max<double>(rec.wgt, 0.0);
        if (rec.wgt <= 0.0) ++nNonPositiveW;
        c.weight = static_cast<G4float>(wgt);

        const auto it = surfById.find(rec.surfaceId());
        if (it != surfById.end()) {
            c.surf = it->second;
        } else {
            // Unknown identifier: treat as side so Sample() keeps the recorded
            // position instead of projecting onto the cask end cap.
            c.surf = 0;
            ++nUnknownSurf;
        }

        ++nValid;
        if (static_cast<long long>(reservoir.size()) < Nmax) {
            reservoir.push_back(c);
            sumW += wgt;
        } else if (Nmax > 0) {
            // Algorithm R: replace a uniformly chosen slot with probability
            // Nmax / nValid.
            std::uniform_int_distribution<long long> dist(0, nValid - 1);
            const long long j = dist(rng);
            if (j < Nmax) {
                sumW += wgt - std::max<double>(reservoir[j].weight, 0.0);
                reservoir[j] = c;
            }
        }

        if (i > 0 && (i % 10000000LL) == 0) {
            G4cout << "[SurfaceFluxSampler::LoadWSSA] read " << i
                   << " / " << h.nrsw << " records (" << reservoir.size()
                   << " retained)" << G4endl;
        }
    }

    if (reservoir.empty()) {
        G4Exception("SurfaceFluxSampler::LoadWSSA", "NoData", FatalException,
                    "No valid WSSA particle records found.");
        return;
    }

    fAll.data = std::move(reservoir);
    BuildAlias(fAll);

    // The reservoir sample is uniform, so scaling its weight sum by
    // nValid / N_sample estimates the total weight of the whole file.
    fKeptWeightTotal = (static_cast<long long>(fAll.data.size()) < nValid)
        ? sumW * static_cast<G4double>(nValid) /
              static_cast<G4double>(fAll.data.size())
        : sumW;

    fKeepRecordedPosition = true;
    fLoaded = true;

    const long long nSide = std::count_if(
        fAll.data.begin(), fAll.data.end(),
        [](const Crossing& c) { return c.surf == 0; });
    const long long nTop = std::count_if(
        fAll.data.begin(), fAll.data.end(),
        [](const Crossing& c) { return c.surf == 1; });
    const long long nBottom = std::count_if(
        fAll.data.begin(), fAll.data.end(),
        [](const Crossing& c) { return c.surf == 2; });

    sw.Stop();
    G4cout << "[SurfaceFluxSampler::LoadWSSA] Loaded " << fAll.data.size()
           << " of " << nValid << " valid records (file declares " << h.nrsw
           << ", pid=" << pdg << ") in " << sw.RealTime() << " s\n"
           << "   retained weight sum = " << sumW
           << ", estimated full-file weight = " << fKeptWeightTotal << '\n'
           << "   surfaces: side=" << nSide << ", top=" << nTop
           << ", bottom=" << nBottom << '\n';
    if (nDegenerate || nUnknownSurf || nNonPositiveW) {
        G4cout << "   skipped/marked: degenerate direction=" << nDegenerate
               << ", unknown surface=" << nUnknownSurf
               << ", non-positive weight=" << nNonPositiveW << G4endl;
    }
}

const SurfaceFluxSampler::Bucket*
SurfaceFluxSampler::GetBucket(G4int /*pid*/) const { return &fAll; }

G4int SurfaceFluxSampler::NumEntries(G4int pid) const
{
    auto* b = GetBucket(pid);
    return b ? static_cast<G4int>(b->data.size()) : 0;
}

G4ThreeVector
SurfaceFluxSampler::SmearDirectionCone(const G4ThreeVector& dir,
                                       G4double sigma) const
{
    const G4double th = std::fabs(G4RandGauss::shoot(0., sigma));
    const G4double ph = 2.0 * M_PI * G4UniformRand();
    G4ThreeVector d(std::sin(th)*std::cos(ph),
                    std::sin(th)*std::sin(ph),
                    std::cos(th));
    d.rotateUz(dir);
    return d;
}

bool SurfaceFluxSampler::Sample(G4ThreeVector& posLocal,
                                G4ThreeVector& dirLocal,
                                G4double&      ekin,
                                G4double&      weight,
                                G4int&         pid) const
{
    EnsureLoaded();

    const Bucket* b = GetBucket(fRequestedPid);
    if (!b || b->data.empty()) return false;

    const std::size_t N = b->data.size();
    const G4double u    = G4UniformRand() * N;
    const std::size_t i = static_cast<std::size_t>(u);
    const G4double frac = u - i;
    const std::size_t k = (frac < b->prob[i]) ? i : b->alias[i];
    const auto& c = b->data[k];

    G4double xL = c.x, yL = c.y, zL = c.z;
    G4ThreeVector dir(c.px, c.py, c.pz);

    if (c.surf == 0) {
        // -------------------- cylindrical SIDE --------------------
        if (fSmearPhi > 0. || fSmearZ > 0.) {
            const G4double rLoc = std::hypot(c.x, c.y);
            G4double phi0 = std::atan2(c.y, c.x);
            if (fSmearPhi > 0.) phi0 += G4RandGauss::shoot(0., fSmearPhi);
            if (fSmearZ   > 0.) zL   += G4RandGauss::shoot(0., fSmearZ);
            const G4double zMax = 0.5*fPendingH_mm - fPendingTol_mm;
            if (zL >  zMax) zL =  zMax;
            if (zL < -zMax) zL = -zMax;
            xL = rLoc * std::cos(phi0);
            yL = rLoc * std::sin(phi0);
        }
        if (fSmearAngle > 0.) {
            G4ThreeVector d = SmearDirectionCone(dir, fSmearAngle);
            const G4double invR = 1.0 / std::hypot(xL, yL);
            if (d.x()*(xL*invR) + d.y()*(yL*invR) > 0.) dir = d;
        }
    } else {
        // ----------------- circular END-CAP (top=1, bottom=2) -----------------
        // The TTree/CSV sources store cap crossings at the cask-local cap
        // plane.  A WSSA file stores the exact crossing position, which can
        // lie slightly outside the Geant4 cap, so keep it there.
        const G4double sign = (c.surf == 1) ? +1.0 : -1.0;
        if (!fKeepRecordedPosition) zL = sign * (0.5 * fPendingH_mm);
        if (fSmearZ > 0.) {                       // reuse fSmearZ as in-plane sigma
            xL = c.x + G4RandGauss::shoot(0., fSmearZ);
            yL = c.y + G4RandGauss::shoot(0., fSmearZ);
            const G4double rMax = fPendingR_mm - fPendingTol_mm;
            const G4double rLoc = std::hypot(xL, yL);
            if (rLoc > rMax && rLoc > 0.) { xL *= rMax/rLoc; yL *= rMax/rLoc; }
        }
        if (fSmearAngle > 0.) {
            G4ThreeVector d = SmearDirectionCone(dir, fSmearAngle);
            if (sign * d.z() > 0.) dir = d;
        }
    }

    G4double E = c.ekin;
    if (fSmearEfrac > 0.) {
        const G4double fr = 1.0 + G4RandGauss::shoot(0., fSmearEfrac);
        if (fr > 0.) E *= fr;
    }

    posLocal = G4ThreeVector(xL, yL, zL);
    dirLocal = dir.unit();

    // C6 hexant reuse: rotate this crossing about the cask axis (+z).
    // Equivalent to rotating cask0 by the same angle, since the cask is
    // cylindrically symmetric and primaries are emitted outward.
    if (fSourceRotZ != 0.) {
        posLocal.rotateZ(fSourceRotZ);
        dirLocal.rotateZ(fSourceRotZ);
    }
    ekin     = E;
    weight   = 1.0;          // alias table already absorbs the input weights
    pid      = c.pid;
    return true;
}

void SurfaceFluxSampler::EnsureLoaded() const
{
    std::call_once(fLoadOnce, [this]() {
        const_cast<SurfaceFluxSampler*>(this)->DoLoad();
    });
}

void SurfaceFluxSampler::DoLoad()
{
    if (fPendingR_mm <= 0. || fPendingH_mm <= 0.) {
        G4Exception("SurfaceFluxSampler::DoLoad", "NoGeom", FatalException,
                    "Geometry parameters not configured (SetGeometryParameters).");
        return;
    }

    if (fInputFormat == 1) {
        // ---- CSV input ----
        if (fPendingCSVFile.empty()) {
            G4Exception("SurfaceFluxSampler::DoLoad", "NoFile", FatalException,
                        "No CSV file set. Use /dcs-monitor/surf/csvFile <path>.");
            return;
        }
        G4cout << "[SurfaceFluxSampler] Lazy load (CSV) on thread "
               << G4Threading::G4GetThreadId() << " : " << fPendingCSVFile << G4endl;
        LoadCSV(fPendingCSVFile, fPendingR_mm, fPendingH_mm, fPendingTol_mm);
    } else if (fInputFormat == 2) {
        // ---- WSSA binary input ----
        if (fPendingWSSAFile.empty()) {
            G4Exception("SurfaceFluxSampler::DoLoad", "NoFile", FatalException,
                        "No WSSA file set. Use /dcs-monitor/surf/wssaFile <path>.");
            return;
        }
        G4cout << "[SurfaceFluxSampler] Lazy load (WSSA) on thread "
               << G4Threading::G4GetThreadId() << " : " << fPendingWSSAFile << G4endl;
        LoadWSSA(fPendingWSSAFile, fPendingR_mm, fPendingH_mm, fPendingTol_mm);
    } else {
        // ---- TTree input (original path) ----
        if (fPendingFile.empty()) {
            G4Exception("SurfaceFluxSampler::DoLoad", "NoFile", FatalException,
                        "No source file set. Use /dcs-monitor/surf/sourceFile <path>.");
            return;
        }
        G4cout << "[SurfaceFluxSampler] Lazy load on thread "
               << G4Threading::G4GetThreadId() << " : " << fPendingFile << G4endl;
        Load(fPendingFile, fPendingTree, fPendingR_mm, fPendingH_mm, fPendingTol_mm);
    }

    fLoaded = true;
    G4cout << "[SurfaceFluxSampler] Done loading; run can start." << G4endl;
}

bool SurfaceFluxSampler::ForceLoad()
{
    EnsureLoaded();
    return fLoaded && !fAll.data.empty();
}

void SurfaceFluxSampler::StartRun(G4double measurement_time)
{
    if (!ForceLoad()) {
        G4Exception("SurfaceFluxSampler::StartRun", "LoadFail", FatalException,
                    "No usable surface crossings loaded.");
        return;
    }
    if (fKeptWeightTotal <= 0. || fNumPrimaries <= 0 ||
        fDecayRate <= 0. || measurement_time <= 0.) {
        G4ExceptionDescription ed;
        ed << "Invalid input(s): keptWeightTotal=" << fKeptWeightTotal
           << ", numPrimaries=" << fNumPrimaries
           << ", decayRate[1/s]=" << fDecayRate
           << ", measTime[s]=" << measurement_time / CLHEP::s;
        G4Exception("SurfaceFluxSampler::StartRun", "BadParams",
                    FatalException, ed);
        return;
    }

    fLastMeasurementTime = measurement_time;
    const G4double ratio = fKeptWeightTotal / static_cast<G4double>(fNumPrimaries);
    const G4double numDecays    = (measurement_time / CLHEP::s) * fDecayRate;
    const G4double numPrimaries = numDecays * ratio;

    G4cout << "[SurfaceFluxSampler::StartRun] ratio = " << ratio
           << "  (" << fKeptWeightTotal << " weighted crossings / "
           << fNumPrimaries << " primaries)\n"
           << "  decays in " << measurement_time / CLHEP::s << " s = " << numDecays << '\n'
           << "  primaries to generate = " << numPrimaries << G4endl;

    if (numPrimaries < 1.0) {
        G4Exception("SurfaceFluxSampler::StartRun", "TooFew", JustWarning,
                    "Computed numPrimaries < 1; not starting a run.");
        return;
    }


    //if (numPrimaries >= 1e9) {
    //    G4Exception("SurfaceFluxSampler::StartRun", "TooMany", FatalException,
    //                "More than 1e9 primaries requested; check inputs.");
    //    return;
    //}
    //const long long Nrun = static_cast<long long>(numPrimaries);
    //G4UImanager::GetUIpointer()->ApplyCommand("/run/beamOn " + std::to_string(Nrun));
    // /run/beamOn takes a G4int (32-bit signed), so the true ceiling on the
    // number of events in ONE run is INT_MAX. If the physics-derived count
    // exceeds that, clamp to INT_MAX and warn -- do NOT abort. A
    // FatalException here would kill the whole process and lose every
    // remaining run in the 72-run macro.
    constexpr long long kMaxBeamOn =
        static_cast<long long>(std::numeric_limits<G4int>::max());   // 2147483647

    long long Nrun = static_cast<long long>(numPrimaries);
    if (Nrun > kMaxBeamOn) {
        G4ExceptionDescription ed;
        ed << "Computed numPrimaries = " << numPrimaries
           << " exceeds the /run/beamOn ceiling of " << kMaxBeamOn
           << "; clamping to " << kMaxBeamOn << " for this run.";
        G4Exception("SurfaceFluxSampler::StartRun", "ClampBeamOn",
                    JustWarning, ed);
        Nrun = kMaxBeamOn;
    }

    G4UImanager::GetUIpointer()->ApplyCommand("/run/beamOn " + std::to_string(Nrun));


}

