// verify_meta.C -- sanity-check one merged p1.5 library file.
//
// usage:  root -b -q 'verify_meta.C("<merged.root>", <deep 0|1>)'
//
// Prints one machine-greppable line:
//   VERIFY_OK   file=... tgen=... M=... nps=... ratio=... sourceCask=...
//               sourceType=... hexant=... globalFuel=... batchId=...
//               nCells=... [cellsNonZero=k/nCells]
//   VERIFY_FAIL file=... flags=a,b,...
//
// Hard-fail flags:
//   MISSING_META  no sectorMeta tree
//   META_ROWS=n   sectorMeta rows != 1 (merged files must be consolidated)
//   TGEN          T_gen <= 0
//   MISSING_FLUX  no sectorFlux tree
//   M_MISMATCH    sectorFlux->GetEntries() != M
//   NCELLS        nCells != nZLayers * nSectors
//   CELLID_RANGE  (deep only) cellId outside [0, nCells)
//
// cellsNonZero (deep only) is informational: with few sampled events a
// cell can legitimately have no entry.

#include <TFile.h>
#include <TTree.h>
#include <TSystem.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

void verify_meta(const char* fname, Int_t deep = 0)
{
    std::vector<std::string> flags;

    Double_t tgen = 0.;  Long64_t m = 0;  Long64_t nps = 0;  Double_t ratio = 0.;
    Int_t sourceCask = 0, sourceType = 0, hexant = 0, globalFuel = 0, batchId = 0;
    Int_t nSectors = 0, nZLayers = 0, nCells = 0;
    Long64_t sfEntries = -1;
    Int_t cellsNonZero = -1;

    std::unique_ptr<TFile> f(TFile::Open(fname, "READ"));
    if (!f || f->IsZombie()) {
        std::cout << "VERIFY_FAIL file=" << fname << " flags=OPEN_FAIL" << std::endl;
        gSystem->Exit(1);
    }

    auto* meta = dynamic_cast<TTree*>(f->Get("sectorMeta"));
    if (!meta) {
        flags.push_back("MISSING_META");
    } else {
        const Long64_t rows = meta->GetEntries();
        if (rows != 1) flags.push_back("META_ROWS=" + std::to_string(rows));
        if (rows >= 1) {
            meta->SetBranchAddress("T_gen",      &tgen);
            meta->SetBranchAddress("M",          &m);
            meta->SetBranchAddress("nps",        &nps);
            meta->SetBranchAddress("ratio",      &ratio);
            meta->SetBranchAddress("sourceCask", &sourceCask);
            meta->SetBranchAddress("sourceType", &sourceType);
            meta->SetBranchAddress("hexant",     &hexant);
            meta->SetBranchAddress("globalFuel", &globalFuel);
            meta->SetBranchAddress("batchId",    &batchId);
            meta->SetBranchAddress("nSectors",   &nSectors);
            meta->SetBranchAddress("nZLayers",   &nZLayers);
            meta->SetBranchAddress("nCells",     &nCells);
            meta->GetEntry(0);
            if (tgen <= 0.) flags.push_back("TGEN");
            if (nCells != nZLayers * nSectors) flags.push_back("NCELLS");
        }
    }

    auto* sf = dynamic_cast<TTree*>(f->Get("sectorFlux"));
    if (!sf) {
        flags.push_back("MISSING_FLUX");
    } else {
        sfEntries = sf->GetEntries();
        if (sfEntries != m) flags.push_back("M_MISMATCH");
    }

    if (deep && sf && nCells > 0) {
        std::vector<char> seen(static_cast<std::size_t>(nCells), 0);
        TTreeReader r("sectorFlux", f.get());
        // cellId is stored as a double column (FillNtupleDColumn) in the
        // sectorFlux tree; round it like SectorFluxSampler does.
        TTreeReaderValue<Double_t> cellId(r, "cellId");
        if (r.GetTree() && sf->GetBranch("cellId")) {
            while (r.Next()) {
                const Int_t cid = static_cast<Int_t>(std::llround(*cellId));
                if (cid < 0 || cid >= nCells) {
                    flags.push_back("CELLID_RANGE");
                    break;
                }
                seen[static_cast<std::size_t>(cid)] = 1;
            }
            cellsNonZero = 0;
            for (const char s : seen) cellsNonZero += s;
        }
    }

    std::cout << (flags.empty() ? "VERIFY_OK " : "VERIFY_FAIL ")
              << "file=" << fname
              << " tgen=" << tgen
              << " M=" << m
              << " nps=" << nps
              << " ratio=" << ratio
              << " sourceCask=" << sourceCask
              << " sourceType=" << sourceType
              << " hexant=" << hexant
              << " globalFuel=" << globalFuel
              << " batchId=" << batchId
              << " nSectors=" << nSectors
              << " nZLayers=" << nZLayers
              << " nCells=" << nCells;
    if (cellsNonZero >= 0) {
        std::cout << " cellsNonZero=" << cellsNonZero << "/" << nCells;
    }
    if (!flags.empty()) {
        std::cout << " flags=";
        for (std::size_t i = 0; i < flags.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << flags[i];
        }
    }
    std::cout << std::endl;

    gSystem->Exit(flags.empty() ? 0 : 1);
}
