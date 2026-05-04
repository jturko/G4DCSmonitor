#include <TFile.h>
#include <TTree.h>
#include <iostream>

void analyze_primary(const char* inputFileName = "tmp.root") {
    TFile* file = TFile::Open(inputFileName);
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Could not open file " << inputFileName << std::endl;
        return;
    }

    TTree* primary = (TTree*)file->Get("primary");
    if (!primary) {
        std::cerr << "Error: Could not retrieve TTree 'primary'" << std::endl;
        return;
    }

    Double_t pid;
    Double_t ekin;
    Double_t t;
    Double_t x;
    Double_t y;
    Double_t z;
    Double_t px;
    Double_t py;
    Double_t pz;
    Long64_t nEntries_primary = primary->GetEntries();
    std::cout << "Processing TTree: primary with " << nEntries_primary << " entries." << std::endl;

    // Set branch addresses
    primary->SetBranchAddress("pid", &pid);
    primary->SetBranchAddress("ekin", &ekin);
    primary->SetBranchAddress("t", &t);
    primary->SetBranchAddress("x", &x);
    primary->SetBranchAddress("y", &y);
    primary->SetBranchAddress("z", &z);
    primary->SetBranchAddress("px", &px);
    primary->SetBranchAddress("py", &py);
    primary->SetBranchAddress("pz", &pz);

    // Create histograms
    const int ndim = 3;
    string sdim[ndim] = { "x", "y", "z" };
    TH1F *h_pos[ndim], *h_mom[ndim];
    TH3F *h3_pos = new TH3F("h3_pos", "h3_pos", 100, -4000, 4000, 100, -4000, 4000, 100, -4000, 4000);
    TH2F *h2_pos_xz = new TH2F("h2_pos_xz", "h2_pos_xz", 8000, -4000, 4000, 8000, -4000, 4000);
    TH3F *h3_mom = new TH3F("h3_mom", "h3_mom", 110, -1.1, 1.1, 110, -1.1, 1.1, 110, -1.1, 1.1);
    for(int idim=0; idim<ndim; idim++) {
        h_pos[idim] = new TH1F(Form("h_pos_%s", sdim[idim].c_str()), Form("h_pos_%s", sdim[idim].c_str()), 4000, -4000, 4000);
        h_mom[idim] = new TH1F(Form("h_mom_%s", sdim[idim].c_str()), Form("h_mom_%s", sdim[idim].c_str()), 2200, -1.1, 1.1);
    }
    TH1F *h_ekin = new TH1F("h_ekin", "h_ekin", 1000, 0, 10);

    for (Long64_t i = 0; i < nEntries_primary; ++i) {
        primary->GetEntry(i);
        // Process data for each entry
        
        h_pos[0]->Fill(x);
        h_pos[1]->Fill(y);
        h_pos[2]->Fill(z);
        h3_pos->Fill(x,y,z);
        h2_pos_xz->Fill(x,z);    

        h_mom[0]->Fill(px);
        h_mom[1]->Fill(py);
        h_mom[2]->Fill(pz);
        h3_mom->Fill(px,py,pz);

        h_ekin->Fill(ekin);
        
    }
    
    for(int idim=0; idim<ndim; idim++) {
        new TCanvas;
        h_pos[idim]->Draw();
        
        new TCanvas;
        h_mom[idim]->Draw();
    }

    new TCanvas;
    h3_pos->GetXaxis()->SetTitle("x / mm");
    h3_pos->GetYaxis()->SetTitle("y / mm");
    h3_pos->GetZaxis()->SetTitle("z / mm");
    h3_pos->Draw("box2");
    
    new TCanvas;
    h2_pos_xz->Draw("colz");

    new TCanvas;
    h3_mom->GetXaxis()->SetTitle("x / unitless");
    h3_mom->GetYaxis()->SetTitle("y / unitless");
    h3_mom->GetZaxis()->SetTitle("z / unitless");
    h3_mom->Draw("box2");

    new TCanvas;
    h_ekin->Draw();
}
