// style.h

#ifndef STYLE_H
#define STYLE_H

#include <TROOT.h>
#include <TStyle.h>
#include <TColor.h>
#include <TF1.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TH1.h>
#include <TString.h>
#include <iostream>
#include <vector>
#include <array>
#include <time.h>

///////////////////////////////////////
// ----- STYLE FUNCTIONS BEGIN ----- //
///////////////////////////////////////

// Function to set the plot style
void style(double text_size = 20, double margin = 0.15, int max_digits = 4) {

    // Global variables
    //double  text_size = 20;
    //int     text_font = 43;
    int     text_font = 132;   // (unused; fonts left at ROOT defaults below)

    // Create and set a custom style
    TStyle* myStyle = new TStyle("DoctoralStyle", "My Custom Style");
    gROOT->GetStyle("Modern")->Copy(*myStyle);

    // GL canvas
    myStyle->SetCanvasPreferGL(true);

    // Set global style options
    myStyle->SetCanvasColor(0);
    myStyle->SetFrameBorderMode(0);
    myStyle->SetPadBorderMode(0);
    myStyle->SetPadColor(0);
    myStyle->SetPadTickX(1);
    myStyle->SetPadTickY(1);
    myStyle->SetOptStat(0);
    myStyle->SetOptTitle(0);

    // Set histogram options
    //myStyle->SetHistLineColor(kBlue);
    //myStyle->SetHistLineWidth(2);
    //myStyle->SetHistFillColor(kBlue);
    //myStyle->SetHistFillStyle(3004);

    // Set marker options
    //myStyle->SetMarkerStyle(20);
    //myStyle->SetMarkerSize(1.0);
    //myStyle->SetMarkerColor(kRed);

    // Set line options
    myStyle->SetLineStyle(1);
    myStyle->SetLineWidth(1);
    myStyle->SetLineColor(kBlack);

    // Text sizes
    myStyle->SetTextSize (text_size);
    myStyle->SetTitleSize(text_size, "XYZ");
    myStyle->SetLabelSize(text_size, "XYZ");
    myStyle->SetLegendTextSize(text_size);

    // Fonts left at ROOT's built-in defaults (TextFont 62, Title/Label/Legend
    // 42, Helvetica).  Uncomment to force a specific face instead.
    //myStyle->SetTitleFont (text_font, "XYZ");
    //myStyle->SetLabelFont (text_font, "XYZ");
    //myStyle->SetLegendFont(text_font);
    //myStyle->SetTextFont  (text_font);

    // Legends
    myStyle->SetLegendBorderSize(1);

    // Pad margins
    //double margin = 0.15; // better for TH1/TGraphs
    //double margin = 0.25; // might be better for TH2s
    myStyle->SetPadLeftMargin  (margin);
    myStyle->SetPadBottomMargin(margin);
    myStyle->SetPadRightMargin (margin);
    myStyle->SetPadTopMargin   (margin);

    // Axis stuff 
    // Offsets
    myStyle->SetTitleOffset(1.25, "X");
    myStyle->SetTitleOffset(2.5, "Y");
    myStyle->SetLabelOffset(0.01, "XYZ");
    // Divisions
    myStyle->SetNdivisions(505, "XYZ");
    // Digits
    //myStyle->SetAxisMaxDigits(max_digits);


    // Palette
    myStyle->SetPalette(kViridis);
    myStyle->SetNumberContours(256);
       
    // Apply the style
    gROOT->SetStyle("DoctoralStyle");
    gROOT->ForceStyle();

}

///////////////////////////////////////
// ----- STYLE FUNCTIONS END ----- //
///////////////////////////////////////


///////////////////////////////////////
// ----- COLOR FUNCTIONS BEGIN ----- //
///////////////////////////////////////
// function to get colours evenly distributed across a palette
Color_t get_color(int index, int max_index_range, EColorPalette inpal = kViridis) {
    // Save initially-set color palette
    const int  old_ncols = gStyle->GetNumberOfColors();
    std::vector<int> old_pal(old_ncols);
    for(int i=0; i<old_ncols; i++)
        old_pal[i] = gStyle->GetColorPalette(i);

    // Ensure the index is within the valid range
    if (index < 0 || index >= max_index_range) {
        return -1; // Return an invalid color index
    }

    // Set input color palette
    gStyle->SetPalette(inpal);

    // Get the total number of colors in the palette
    const int nColors = gStyle->GetNumberOfColors();

    // Calculate the color index in the palette
    int colorIndex = index * (nColors - 1) / (max_index_range - 1);
    Color_t output = gStyle->GetColorPalette(colorIndex);

    // Reset the old palette
    gStyle->SetPalette(old_ncols, old_pal.data());

    // Get the color from the palette
    return output;
}

///   // see arxiv.org/pdf/2107.02270 for 6-color scheme
///   bool kP6_exists = false;
///   int  kP6_idx[6];
///   void makeP6() {
///       const int n = 6;
///       TColor * kP6[n];
///       double rr[n] = { 87,  248, 228, 150, 156, 122 };
///       double gg[n] = { 144, 156, 37,  74,  156, 33  };
///       double bb[n] = { 252, 32,  54,  139, 161, 221 };
///       for(int i=0; i<n; i++) {
///           kP6_idx[i] = TColor::GetFreeColorIndex();
///           kP6[i]     = new TColor(kP6_idx[i], rr[i]/255., gg[i]/255., bb[i]/255.);
///       }   
///   }
///   Color_t getP6(int i) {
///       if(i<0 || i>=6)
///           return -1;
///       if(!kP6_exists) {
///           makeP6();
///           kP6_exists = true;
///       }
///       return kP6_idx[i];
///   }
///   
///   // see arxiv.org/pdf/2107.02270 for 8-color scheme
///   bool kP8_exists = false;
///   int  kP8_idx[8];
///   void makeP8() {
///       const int n = 8;
///       TColor * kP8[n];
///       double rr[n] = {  24, 255, 201, 200, 173, 134,  87, 101 };
///       double gg[n] = {  69,  94,  31,  73, 173, 200, 141,  99 };
///       double bb[n] = { 251,   2,  22, 169, 125, 221, 255, 100 };
///       for(int i=0; i<n; i++) {
///           kP8_idx[i] = TColor::GetFreeColorIndex();
///           kP8[i]     = new TColor(kP8_idx[i], rr[i]/255., gg[i]/255., bb[i]/255.);
///       }   
///   }
///   Color_t getP8(int i) {
///       if(i<0 || i>=8)
///           return -1;
///       if(!kP8_exists) {
///           makeP8();
///           kP8_exists = true;
///       }
///       return kP8_idx[i];
///   }

#include "TColor.h"
#include <vector>
#include <array>

namespace PetroffPalette {

    // Generic builder: registers n colors once and caches their indices.
    template <size_t N>
    const std::array<int, N>& buildOrGet(
        const std::array<std::array<int,3>, N>& rgb)
    {
        static std::array<int, N> indices{};
        static bool built = false;
        if (!built) {
            for (size_t i = 0; i < N; ++i) {
                indices[i] = TColor::GetFreeColorIndex();
                new TColor(indices[i],
                           rgb[i][0]/255., rgb[i][1]/255., rgb[i][2]/255.);
            }
            built = true;
        }
        return indices;
    }

    // ---- 6-color scheme ----
    inline Color_t getP6(int i) {
        static const std::array<std::array<int,3>, 6> rgb = {{
            { 87, 144, 252}, {248, 156,  32}, {228,  37,  54},
            {150,  74, 139}, {156, 156, 161}, {122,  33, 221}
        }};
        const auto& idx = buildOrGet(rgb);
        return (i < 0 || i >= (int)idx.size()) ? (Color_t)-1 : (Color_t)idx[i];
    }

    // ---- 8-color scheme ----
    inline Color_t getP8(int i) {
        static const std::array<std::array<int,3>, 8> rgb = {{
            { 24,  69, 251}, {255,  94,   2}, {201,  31,  22}, {200,  73, 169},
            {173, 173, 125}, {134, 200, 221}, { 87, 141, 255}, {101,  99, 100}
        }};
        const auto& idx = buildOrGet(rgb);
        return (i < 0 || i >= (int)idx.size()) ? (Color_t)-1 : (Color_t)idx[i];
    }

    // ---- 10-color scheme ----
    inline Color_t getP10(int i) {
        static const std::array<std::array<int,3>, 10> rgb = {{
            { 63, 144, 218}, {255, 169,  14}, {189,  31,   1}, {148, 164, 162},
            {131,  45, 182}, {169, 107,  89}, {231,  99,   0}, {185, 172, 112},
            {113, 117, 129}, {146, 218, 221}
        }};
        const auto& idx = buildOrGet(rgb);
        return (i < 0 || i >= (int)idx.size()) ? (Color_t)-1 : (Color_t)idx[i];
    }

} // namespace PetroffPalette



// tweak of above for HZDR color palette
bool kPaletteHZDR_exists = false;
int  kPaletteHZDR_idx[8];
void MakePaletteHZDR() {
    const int n = 8;
    TColor * kPaletteHZDR[n];
    double rr[n] = {   0, 238, 101, 134, 201, 200, 173,  87 };
    double gg[n] = {  90, 115,  99, 200,  31,  73, 173, 141 };
    double bb[n] = { 154,  39, 100, 221,  22, 169, 125, 255 };
    for(int i=0; i<n; i++) {
        kPaletteHZDR_idx[i] = TColor::GetFreeColorIndex();
        kPaletteHZDR[i]     = new TColor(kPaletteHZDR_idx[i], rr[i]/255., gg[i]/255., bb[i]/255.);
    }   
}
Color_t GetPaletteHZDR(int i) {
    if(i<0 || i>8)
        return -1;
    if(!kPaletteHZDR_exists) {
        MakePaletteHZDR();
        kPaletteHZDR_exists = true;
    }
    return kPaletteHZDR_idx[i];
}

// used to get transparent palette (used for PSD plots in characterization paper)
// needs testing!! not sure if the rgb values are correct
void custom_palette(double r, double g, double b, double alpha=1.0) 
{
   static Int_t  colors[50];
   static Bool_t initialized = kFALSE;
 
   const int n = 2;
   Double_t Red[n]    = { r   , 0.00 };
   Double_t Green[n]  = { g   , 0.00 };
   Double_t Blue[n]   = { b   , b    };
   Double_t Length[n] = { 0.00, 1.00 };
 
   if(!initialized){
      Int_t FI = TColor::CreateGradientColorTable(n,Length,Red,Green,Blue,50,alpha);
      for (int i=0; i<50; i++) colors[i] = FI+i;
      initialized = kTRUE;
      return;
   }
   gStyle->SetPalette(50,colors,alpha);
}
///////////////////////////////////////
// ----- COLOR FUNCTIONS END ----- //
///////////////////////////////////////



///////////////////////////////////////
// ----- UTILITIES BEGIN ----- //
///////////////////////////////////////
static void ScaleAxis(TAxis *a, TF1* f)
{
  if (!a) return; // just a precaution
  if (a->GetXbins()->GetSize())
    {
      // an axis with variable bins
      // note: bins must remain in increasing order, hence the "Scale"
      // function must be strictly (monotonically) increasing
      TArrayD X(*(a->GetXbins()));
      //for(Int_t i = 0; i < X.GetSize(); i++) X[i] = Scale(X[i]);
      for(Int_t i = 0; i < X.GetSize(); i++) X[i] = f->Eval(X[i]);
      a->Set((X.GetSize() - 1), X.GetArray()); // new Xbins
    }
  else
    {
      // an axis with fix bins
      // note: we modify Xmin and Xmax only, hence the "Scale" function
      // must be linear (and Xmax must remain greater than Xmin)
      a->Set( a->GetNbins(),
              //Scale(a->GetXmin()), // new Xmin
              //Scale(a->GetXmax()) ); // new Xmax
              f->Eval(a->GetXmin()), // new Xmin
              f->Eval(a->GetXmax()) ); // new Xmax
    }
  return;
}

std::vector<TPad*> SplitCanvas(TCanvas* c = NULL, int nx = 3, int ny = 1, double margin_x = 0.1, double margin_y = 0.1) {
    if(!c) {
        c = new TCanvas;
    }

    std::vector<TPad*> outPads;
    
    int    c_dx = c->GetWw();
    double m_dx = margin_x * c_dx;
    double f_dx = (c_dx - 2*m_dx) / nx;
    
    int    c_dy = c->GetWh();
    double m_dy = margin_y * c_dy;
    double f_dy = (c_dy - 2*m_dy) / ny;
    
    double p_ml, p_mr, p_mt, p_mb;
    double p_x0 = 0, p_y0 = 0, p_x1 = 0, p_y1 = 0;

    // loop over y
    //for(int iy=0; iy<ny; iy++) {
    for(int iy=ny-1; iy>=0; iy--) {
        cout << "Row " << iy << ":" << endl;
        // calculate pady positions
        if(iy==0) {     // bottom
            p_y0 = 0;
            p_y1 = m_dy + f_dy;
        }
        else if(iy==ny-1) {  // top
            p_y0 = m_dy + (ny-1)*f_dy;
            p_y1 = c_dy;
        }
        else {
            p_y0 = m_dy + iy    *f_dy;
            p_y1 = m_dy + (iy+1)*f_dy;
        }
    
        // special case nx = 2
        if(iy==ny-1 && ny < 2)
            p_y1 = c_dy;
        
        // loop over x
        for(int ix=0; ix<nx; ix++) {
            // calculate pad x positions 
            if(ix==0) {     // left
                p_x0 = 0;
                p_x1 = m_dx + f_dx;
            }
            else if(ix==nx-1) {  // right
                p_x0 = m_dx + (nx-1)*f_dx;
                p_x1 = c_dx;
            }
            else {
                p_x0 = m_dx + ix    *f_dx;
                p_x1 = m_dx + (ix+1)*f_dx;
            }
            
            // special case nx = 2
            if(ix==nx-1 && nx < 2)
                p_x1 = c_dx;
        
            
            // set margins
            p_ml = p_mr = p_mb = p_mt = 0.01;
            if(ix==0) {     // left
                p_ml = m_dx / (m_dx + f_dx);
            }
            if(ix==nx-1) {  // right
                p_mr = m_dx / (m_dx + f_dx);
            }
            if(iy==0) {     // bottom
                p_mb = m_dy / (m_dy + f_dy);
            }
            if(iy==ny-1) {  // top
                p_mt = m_dy / (m_dy + f_dy);
            }
        
            // create the TPad
            c->cd();
            cout << Form("Creating TPad with NDC coords: (%.2f, %.2f) -> (%.2f, %.2f) ", p_x0/c_dx, p_y0/c_dy, p_x1/c_dx, p_y1/c_dy);
            cout << Form("w/ margins %.2f, %.2f, %.2f, %.2f \n", p_ml, p_mr, p_mb, p_mt);
            TPad* pad = new TPad(Form("%s_x%d_y%d", c->GetName(), ix, iy), Form("%s_x%d_y%d", c->GetName(), ix, iy), p_x0/c_dx, p_y0/c_dy, p_x1/c_dx, p_y1/c_dy);
            pad->SetMargin(p_ml, p_mr, p_mb, p_mt);
            pad->Draw();
            outPads.push_back(pad);
        }
    }
    
    return outPads;
}

void SaveAllToPDF(const char* outputFileName) {
    TIter next(gROOT->GetListOfCanvases());
    std::vector<TCanvas*> canvases;

    // Collect all canvases first
    TCanvas* canvas;
    while ((canvas = (TCanvas*)next())) {
        canvases.push_back(canvas);
    }

    if (canvases.empty()) return; // No canvases to save

    // Save first canvas with '('
    canvases[0]->SaveAs(Form("%s(", outputFileName));

    // Save middle canvases
    for (size_t i = 1; i < canvases.size() - 1; ++i) {
        canvases[i]->SaveAs(Form("%s", outputFileName));
    }

    // Save last canvas with ')'
    if (canvases.size() > 1) {
        canvases.back()->SaveAs(Form("%s)", outputFileName));
    }
}

#endif // STYLE_H









#ifndef PROGBAR_HH 
#define PROGBAR_HH 1

#include <time.h>

using namespace std;

class ProgressBar {
  public:
    ProgressBar(ULong64_t max_events, int readout_interval=1, int bar_width=40) {
        fMaxEvents = max_events;
        fTimeInterval = readout_interval;
        fLastEvent = 0;
        time(&fLastTime);

        fBarWidth = bar_width;

        fN = 120;
        fRates.resize(fN);
        for(int i=0; i<fN; i++) {
            fRates[i] = 0;
        }
        fNextToReplace = 0;

        fCounter = 0;
    }
    ~ProgressBar() {}

    void SetMaxEvents(ULong64_t n) { fMaxEvents = n; }
    void SetPrintInterval(Int_t time) { fTimeInterval = time; }
    bool Print(ULong64_t event) {
        time(&fThisTime);
        bool printing = false;
        if((difftime(fThisTime,fLastTime) >= fTimeInterval) || event == (fMaxEvents-1)) {  
            printing = true;
            double percent = 1.*event/fMaxEvents;
            double rate;
            double thisRate = ((double)(event-fLastEvent))/difftime(fThisTime,fLastTime);
            if(fCounter < 2) {
               rate = thisRate;
            }
            else { 
                rate = Rate();
            }
            int timeLeftSeconds = ((double)(fMaxEvents-event))/rate;
            int timeLeftMinutes = timeLeftSeconds/60;
            int timeLeftHours   = timeLeftMinutes/60;
            fLastTime = fThisTime;
            fLastEvent = event;

            fRates[fNextToReplace] = thisRate;
            if(fNextToReplace < fN-1)
              fNextToReplace++;
            else
              fNextToReplace = 0;  

            cout << "\33[2K\r  -> [";
            for(int i=0; i<fBarWidth; i++) {
                if(i<percent*fBarWidth) cout << "=";
                else                    cout << " ";
            }
            if( event == (fMaxEvents-1))
                cout << "] all done!" << flush << endl;
            else
                cout << "] processing evt " << event+1 << " / " << fMaxEvents << " (" << Form("%.1f",(100*percent)) << "%), " << timeLeftHours << "h " 
                     << timeLeftMinutes%60 << "m " << timeLeftSeconds%60 << "s left @ " << (int)rate << " evts/s" << flush;
            fCounter++;
        }
        return printing;
    }

    double Rate() {
        double rate = 0;
        int counter = 0;
        for(int i=0; i<fN; i++) {
            if(fRates[i] > 0) {
                rate += fRates[i];
                counter++;
            }
        }
        rate /= double(counter);
        return rate;
    }

  private:
    ULong64_t fMaxEvents;
    ULong64_t fLastEvent;
    time_t fLastTime;
    time_t fThisTime;
    Int_t fTimeInterval;

    Int_t fBarWidth;

    ULong64_t fCounter;

    int                     fN;
    std::vector<double>     fRates;
    int                     fNextToReplace;

};

#endif
