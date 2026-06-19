#include "make_assembly_heatmaps.C"
#include "make_assembly_spectra.C"

void ScaleHistogramsToGlobalMax(TH2* histograms[], int size) {
    if (size <= 0) {
        std::cerr << "Error: The array size must be greater than zero!" << std::endl;
        return;
    }

    double globalMax = -std::numeric_limits<double>::max();
    double globalMin =  std::numeric_limits<double>::max();

    // Step 1: Find the true global min/max from bin contents
    for (int i = 0; i < size; ++i) {
        if (!histograms[i]) continue;

        // Reset any user-defined scale so Get{Max,Min}imum()
        // recompute from the actual bin contents (key fix after Clone/Add)
        histograms[i]->SetMaximum();   // -> -1111 ("unset")
        histograms[i]->SetMinimum();   // -> -1111 ("unset")

        double localMax = histograms[i]->GetMaximum();
        double localMin = histograms[i]->GetMinimum();

        if (localMax > globalMax) globalMax = localMax;
        if (localMin < globalMin) globalMin = localMin;   // fixed: '<' not '>'
    }

    if (globalMax <= globalMin) {
        std::cerr << "Error: invalid global range (max <= min)!" << std::endl;
        return;
    }

    // Step 2: Apply the common scale to every histogram
    for (int i = 0; i < size; ++i) {
        if (histograms[i]) {
            histograms[i]->SetMaximum(globalMax);
            histograms[i]->SetMinimum(globalMin);
        }
    }

    std::cout << "Histograms scaled to global range ["
              << globalMin << ", " << globalMax << "]" << std::endl;
}

void SetDivergingPalette(int nContours = 255) {
    const int nStops = 3;

    // position of each color stop along [0,1]
    Double_t stops[nStops] = { 0.00, 0.50, 1.00 };

    //                         neg     zero    pos
    Double_t red[nStops]   = { 0.13,  1.00,  0.70 };
    Double_t green[nStops] = { 0.27,  1.00,  0.05 };
    Double_t blue[nStops]  = { 0.65,  1.00,  0.15 };

    TColor::CreateGradientColorTable(nStops, stops, red, green, blue, nContours);
    gStyle->SetNumberContours(nContours);
}

void plot_plastic_heatmap_diff() {
    SetDivergingPalette();

    DetectorType typeDet = kPlastic;
    string       typePart= "neutron";

    const int ncask = 4;
    const int nconfig = 2;
    string tconfig[nconfig] = { "withShadowBar", "withoutShadowBar" };
    TH2* h2_counts_total[nconfig][ncask];
    TH2* h2_diff[ncask];

    for(int icask=0; icask<ncask; ++icask) {
            for(int iconfig=0; iconfig<nconfig; iconfig++) {
            string fname = string(Form("heatmaps_plastic_%s_%s_cask%d.root", typePart.c_str(), tconfig[iconfig].c_str(), icask));
            auto f = TFile::Open(fname.c_str());
            if(!f || f->IsZombie()) {
                cout << " file not found: " << fname << ", creating via make_assembly_heatmap.C script..." << endl;
                make_assembly_heatmaps(Form("../data/plastic/%s/all_casks/cask%d/", tconfig[iconfig].c_str(), icask), 
                                       typePart, typeDet, fname.c_str());
                f = TFile::Open(fname.c_str()); 
            }
            h2_counts_total[iconfig][icask] = (TH2*)f->Get("h2_counts_total");
            h2_counts_total[iconfig][icask]->SetDirectory(nullptr);

            make_assembly_spectra(fname.c_str(), Form("spectra_plastic_%s_%s_cask%d.root", typePart.c_str(), tconfig[iconfig].c_str(), icask)); 
        }
        h2_diff[icask] = (TH2Poly*)h2_counts_total[1][icask]->Clone(Form("h2_diff_cask%d", icask));
        h2_diff[icask]->SetDirectory(nullptr);
        h2_diff[icask]->Add(h2_counts_total[0][icask], -1.0);
    }
    
    ScaleHistogramsToGlobalMax(h2_diff, ncask);
    //for(int icask=0; icask<ncask; ++icask) {
    //    //double maxAbs = std::max(std::fabs(h2_diff[icask]->GetMinimum()),
    //    //                         std::fabs(h2_diff[icask]->GetMaximum()));
    //    //h2_diff[icask]->GetZaxis()->SetRangeUser(-maxAbs, +maxAbs);
    //    new TCanvas;
    //    h2_diff[icask]->Draw("colz text");
    //}
    TCanvas* c = new TCanvas;
    c->SetWindowSize(1200,1200);
    c->Modified();
    c->Update();
    c->Divide(3,3);
    int order[ncask] = { 4, 2, 8, 6 };
    for(int icask=0; icask<ncask; ++icask) {
        double maxAbs = std::max(std::fabs(h2_diff[icask]->GetMinimum()),
                                 std::fabs(h2_diff[icask]->GetMaximum()));
        h2_diff[icask]->GetZaxis()->SetRangeUser(-maxAbs, +maxAbs);
        c->cd(order[icask]);
        h2_diff[icask]->Draw("colz text");
    }

    for(int iconfig=0; iconfig<nconfig; iconfig++) {
        ScaleHistogramsToGlobalMax(h2_counts_total[iconfig], ncask);
        TCanvas* c2 = new TCanvas;
        c2->SetWindowSize(1200,1200);
        c2->Modified();
        c2->Update();
        c2->Divide(3,3);
        for(int icask=0; icask<ncask; ++icask) {
            double maxAbs = std::max(std::fabs(h2_counts_total[iconfig][icask]->GetMinimum()),
                                     std::fabs(h2_counts_total[iconfig][icask]->GetMaximum()));
            h2_counts_total[iconfig][icask]->GetZaxis()->SetRangeUser(-maxAbs, +maxAbs);
            c2->cd(order[icask]);
            h2_counts_total[iconfig][icask]->Draw("colz text");
        }
    }

}


