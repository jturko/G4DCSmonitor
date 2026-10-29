// consolidate_meta.C -- consolidate the per-chunk sectorMeta rows of a
// hadded p1.5 library file into ONE row with
//     T_gen = sum(T_gen_i)   and   M = sum(M_i).
//
// Why: SectorFluxSampler::DoLoad reads only sectorMeta entry 0.  A hadded
// file still carries one sectorMeta row per chunk (each with its own
// T_gen and M), so a replay of that file would use one chunk's T_gen and
// get the event count wrong by the chunk factor.  The merged library
// file must therefore carry a single row with the totals.
//
// usage:  root -b -q 'consolidate_meta.C("<merged.root>", <nchunks>)'
//
// Checks before rewriting (exit 1 on any failure, chunks must be kept):
//   - exactly <nchunks> rows
//   - every row: T_gen > 0, M >= 0
//   - all rows agree on nps, ratio, sourceCask, sourceType, hexant,
//     globalFuel, batchId, targetCask, R_in_mm, R_out_mm, halfZ_mm,
//     nSectors, nZLayers, nCells, zMin_mm, zMax_mm, dz_mm, sourceFile
//   - sum(M) == sectorFlux->GetEntries()
// After rewriting, re-reads the file and asserts one row with the sums,
// then prints CONSOLIDATE_OK for the caller to log.

#include <TFile.h>
#include <TTree.h>
#include <TDirectory.h>
#include <TSystem.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct MetaRow {
    Double_t T_gen;
    Long64_t M;
    Long64_t nps;
    Double_t ratio;
    Int_t    sourceCask;
    Int_t    sourceType;
    Int_t    hexant;
    Int_t    globalFuel;
    Int_t    batchId;
    Int_t    targetCask;
    Double_t R_in_mm;
    Double_t R_out_mm;
    Double_t halfZ_mm;
    Int_t    nSectors;
    Int_t    nZLayers;
    Int_t    nCells;
    Double_t zMin_mm;
    Double_t zMax_mm;
    Double_t dz_mm;
    std::string sourceFile;   // copied from sourceFilePtr after each GetEntry
};

bool gFailed = false;

void Fail(const std::string& msg)
{
    std::cerr << "consolidate_meta: FAIL: " << msg << std::endl;
    gFailed = true;
}

bool Eq(const MetaRow& a, const MetaRow& b)
{
    // Note: batchId increments across chunks (1..nChunks) so that
    // SectorFluxSampler's (sourceKey, evtNb) deduplication does not collide
    // across independent chunk runs whose evtNb restarts at 0.
    return a.nps == b.nps
        && a.ratio == b.ratio
        && a.sourceCask == b.sourceCask
        && a.sourceType == b.sourceType
        && a.hexant == b.hexant
        && a.globalFuel == b.globalFuel
        && a.targetCask == b.targetCask
        && a.R_in_mm == b.R_in_mm
        && a.R_out_mm == b.R_out_mm
        && a.halfZ_mm == b.halfZ_mm
        && a.nSectors == b.nSectors
        && a.nZLayers == b.nZLayers
        && a.nCells == b.nCells
        && a.zMin_mm == b.zMin_mm
        && a.zMax_mm == b.zMax_mm
        && a.dz_mm == b.dz_mm
        && a.sourceFile == b.sourceFile;
}

} // namespace

void consolidate_meta(const char* fname, Int_t nChunksExpected)
{
    std::unique_ptr<TFile> f(TFile::Open(fname, "UPDATE"));
    if (!f || f->IsZombie()) {
        Fail(std::string("cannot open ") + fname);
        gSystem->Exit(1);
    }

    auto* meta = dynamic_cast<TTree*>(f->Get("sectorMeta"));
    if (!meta) {
        Fail(std::string(fname) + " has no sectorMeta tree");
        gSystem->Exit(1);
    }

    MetaRow row{};
    meta->SetBranchAddress("T_gen",      &row.T_gen);
    meta->SetBranchAddress("M",          &row.M);
    meta->SetBranchAddress("nps",        &row.nps);
    meta->SetBranchAddress("ratio",      &row.ratio);
    meta->SetBranchAddress("sourceCask", &row.sourceCask);
    meta->SetBranchAddress("sourceType", &row.sourceType);
    meta->SetBranchAddress("hexant",     &row.hexant);
    meta->SetBranchAddress("globalFuel", &row.globalFuel);
    meta->SetBranchAddress("batchId",    &row.batchId);
    meta->SetBranchAddress("targetCask", &row.targetCask);
    meta->SetBranchAddress("R_in_mm",    &row.R_in_mm);
    meta->SetBranchAddress("R_out_mm",   &row.R_out_mm);
    meta->SetBranchAddress("halfZ_mm",   &row.halfZ_mm);
    meta->SetBranchAddress("nSectors",   &row.nSectors);
    meta->SetBranchAddress("nZLayers",   &row.nZLayers);
    meta->SetBranchAddress("nCells",     &row.nCells);
    meta->SetBranchAddress("zMin_mm",    &row.zMin_mm);
    meta->SetBranchAddress("zMax_mm",    &row.zMax_mm);
    meta->SetBranchAddress("dz_mm",      &row.dz_mm);
    // std::string branches are read through the address of a pointer
    std::string* sourceFilePtr = nullptr;
    meta->SetBranchAddress("sourceFile", &sourceFilePtr);

    const Long64_t nRows = meta->GetEntries();
    if (nRows != static_cast<Long64_t>(nChunksExpected)) {
        Fail("expected " + std::to_string(nChunksExpected) + " sectorMeta rows, found "
             + std::to_string(nRows));
        gSystem->Exit(1);
    }

    MetaRow ref{};
    Double_t tgenSum = 0.;
    Long64_t mSum = 0;
    std::vector<Long64_t> chunkM(static_cast<std::size_t>(nRows), 0);
    bool allSameBatchId = true;
    for (Long64_t i = 0; i < nRows; ++i) {
        meta->GetEntry(i);
        row.sourceFile = sourceFilePtr ? *sourceFilePtr : std::string();
        if (row.T_gen <= 0.) {
            Fail("row " + std::to_string(i) + ": T_gen <= 0 (" + std::to_string(row.T_gen) + ")");
        }
        if (row.M < 0) {
            Fail("row " + std::to_string(i) + ": M < 0");
        }
        if (i == 0) {
            ref = row;
        } else {
            if (!Eq(ref, row)) {
                Fail("row " + std::to_string(i) + ": provenance fields differ from row 0");
            }
            if (row.batchId != ref.batchId) {
                allSameBatchId = false;
            }
        }
        chunkM[static_cast<std::size_t>(i)] = row.M;
        tgenSum += row.T_gen;
        mSum += row.M;
    }

    auto* sf = dynamic_cast<TTree*>(f->Get("sectorFlux"));
    const Long64_t sfEntries = sf ? sf->GetEntries() : -1;
    if (sfEntries != mSum) {
        Fail("sectorFlux entries (" + std::to_string(sfEntries)
             + ") != sum of chunk M (" + std::to_string(mSum) + ")");
    }
    if (gFailed) {
        gSystem->Exit(1);
    }

    // If the chunks were generated with a constant batchId across >1 chunks
    // (legacy macro without per-chunk batchId 1..N), stamp batchId = 1..nRows
    // onto each chunk's range in sectorFlux so (sourceKey, evtNb) never collides
    // across chunks during replay deduplication.
    if (nRows > 1 && allSameBatchId && sf && sfEntries > 0 && sf->GetBranch("batchId")) {
        std::cout << "consolidate_meta: stamping per-chunk batchId (1.." << nRows
                  << ") onto " << sfEntries << " sectorFlux entries" << std::endl;
        Double_t batchVal = 0.;
        sf->SetBranchAddress("batchId", &batchVal);
        f->cd();
        TTree* sfFixed = sf->CloneTree(0);
        Long64_t entryIdx = 0;
        for (Long64_t c = 0; c < nRows; ++c) {
            const Long64_t count = chunkM[static_cast<std::size_t>(c)];
            for (Long64_t k = 0; k < count; ++k, ++entryIdx) {
                sf->GetEntry(entryIdx);
                batchVal = static_cast<Double_t>(c + 1);
                sfFixed->Fill();
            }
        }
        gDirectory->Delete("sectorFlux;*");
        sfFixed->Write("sectorFlux", TObject::kOverwrite);
        delete sfFixed;
        sf = dynamic_cast<TTree*>(f->Get("sectorFlux"));
    }

    // Rewrite: drop all sectorMeta cycles, write one consolidated row with
    // the same branch layout as RunAction.cc.
    f->cd();
    gDirectory->Delete("sectorMeta;*");

    auto* out = new TTree("sectorMeta", "p1.5 sector-source provenance (consolidated)");
    out->Branch("T_gen",      &ref.T_gen,      "T_gen/D");
    out->Branch("M",          &ref.M,          "M/L");
    out->Branch("nps",        &ref.nps,        "nps/L");
    out->Branch("ratio",      &ref.ratio,      "ratio/D");
    out->Branch("sourceCask", &ref.sourceCask, "sourceCask/I");
    out->Branch("sourceType", &ref.sourceType, "sourceType/I");
    out->Branch("hexant",     &ref.hexant,     "hexant/I");
    out->Branch("globalFuel", &ref.globalFuel, "globalFuel/I");
    out->Branch("batchId",    &ref.batchId,    "batchId/I");
    out->Branch("targetCask", &ref.targetCask, "targetCask/I");
    out->Branch("R_in_mm",    &ref.R_in_mm,    "R_in_mm/D");
    out->Branch("R_out_mm",   &ref.R_out_mm,   "R_out_mm/D");
    out->Branch("halfZ_mm",   &ref.halfZ_mm,   "halfZ_mm/D");
    out->Branch("nSectors",   &ref.nSectors,   "nSectors/I");
    out->Branch("nZLayers",   &ref.nZLayers,   "nZLayers/I");
    out->Branch("nCells",     &ref.nCells,     "nCells/I");
    out->Branch("zMin_mm",    &ref.zMin_mm,    "zMin_mm/D");
    out->Branch("zMax_mm",    &ref.zMax_mm,    "zMax_mm/D");
    out->Branch("dz_mm",      &ref.dz_mm,      "dz_mm/D");
    out->Branch("sourceFile", &ref.sourceFile);

    ref.T_gen = tgenSum;
    ref.M = mSum;
    out->Fill();
    out->Write("", TObject::kOverwrite);

    // Verify by re-reading from disk.
    f->Close();
    std::unique_ptr<TFile> fv(TFile::Open(fname, "READ"));
    if (!fv || fv->IsZombie()) {
        Fail("cannot re-open for verification");
        gSystem->Exit(1);
    }
    auto* mv = dynamic_cast<TTree*>(fv->Get("sectorMeta"));
    if (!mv || mv->GetEntries() != 1) {
        Fail("verification: sectorMeta rows != 1 after consolidation");
        gSystem->Exit(1);
    }
    MetaRow chk{};
    mv->SetBranchAddress("T_gen", &chk.T_gen);
    mv->SetBranchAddress("M",     &chk.M);
    mv->GetEntry(0);
    if (chk.T_gen != tgenSum || chk.M != mSum) {
        Fail("verification: stored T_gen/M differ from sums");
        gSystem->Exit(1);
    }
    fv->Close();

    std::cout << "CONSOLIDATE_OK file=" << fname
              << " T_gen=" << tgenSum
              << " M=" << mSum << std::endl;
    gSystem->Exit(0);
}
