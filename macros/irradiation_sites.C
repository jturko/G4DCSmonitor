// irradiation_sites.C -- schematic plan view of the three irradiation sites
// (regions of interest) used by the matrix scan, drawn on the 250x250 mm
// scintillator footprint using the report (paper) frame: top=+y, bottom=-y,
// left=-x, right=+x.
//
// The vertical 50x50 mm GPS beam is placed fully on the plate at three
// positions, each irradiating an equal-area patch, matching the 50x50 mm MCD
// active area of Rao et al. (J. Appl. Phys. 138, 114504):
//   ROI 1 centre       : patch centre (  0,   0) mm  -> region [-25, 25]^2
//   ROI 2 bottom-middle: patch centre (  0,-100) mm  -> region [-25,25]x[-125,-75]
//   ROI 3 bottom-right : patch centre (100,-100) mm  -> region [75,125]x[-125,-75]
//
// Usage:
//   root -l -b -q 'macros/irradiation_sites.C("report/figures")'

#include <TCanvas.h>
#include <TBox.h>
#include <TLine.h>
#include <TLatex.h>
#include <TPad.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include <vector>

// Shared ROOT style (fonts, ticks, title conventions).  Located via
// ROOT_INCLUDE_PATH (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {
const double kHalf = 125.0;      // slab half-extent [mm]
const double kPatch = 25.0;      // beam half-width [mm] (50x50 mm patch)

struct Site { double x, y; const char* label; };

const std::vector<Site>& sites()
{
    static const std::vector<Site> s = {
        {   0.0,    0.0, "ROI 1 (centre)"},
        {   0.0, -100.0, "ROI 2 (edge)"},
        { 100.0, -100.0, "ROI 3 (corner)"},
    };
    return s;
}
}  // namespace

void irradiation_sites(const char* outdir = "report/figures")
{
    style(0.045);                 // fonts / ticks / title conventions
    gStyle->SetOptStat(0);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_irradiation_sites", "irradiation sites", 700, 680);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.05);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.10);

    const double span = kHalf + 20.0;
    TH1F* frame = gPad->DrawFrame(-span, -span, span, span,
                                  "Irradiation sites;#font[132]{#it{x}} [mm];#font[132]{#it{y}} [mm]");
    frame->GetXaxis()->SetTitleSize(0.045);
    frame->GetYaxis()->SetTitleSize(0.045);
    frame->GetXaxis()->SetLabelSize(0.040);
    frame->GetYaxis()->SetLabelSize(0.040);
    frame->GetXaxis()->SetTitleOffset(1.30);
    frame->GetYaxis()->SetTitleOffset(1.45);

    TBox* slab = new TBox(-kHalf, -kHalf, kHalf, kHalf);
    slab->SetFillColorAlpha(kAzure - 9, 0.16);
    slab->SetLineColor(kGray + 2);
    slab->SetLineWidth(2);
    slab->Draw("SAME");

    TLine* hx = new TLine(-kHalf, 0., kHalf, 0.);
    TLine* vy = new TLine(0., -kHalf, 0., kHalf);
    hx->SetLineColor(kGray + 1); hx->SetLineStyle(2); hx->SetLineWidth(1);
    vy->SetLineColor(kGray + 1); vy->SetLineStyle(2); vy->SetLineWidth(1);
    hx->Draw("SAME"); vy->Draw("SAME");

    // Beam patches: filled squares marking the 50x50 mm illuminated regions.
    const int cols[3] = {PetroffPalette::getP6(0),
                         PetroffPalette::getP6(1),
                         PetroffPalette::getP6(2)};
    int i = 0;
    for (const auto& s : sites()) {
        TBox* p = new TBox(s.x - kPatch, s.y - kPatch, s.x + kPatch, s.y + kPatch);
        p->SetFillColorAlpha(cols[i], 0.45);
        p->SetLineColor(cols[i]);
        p->SetLineWidth(2);
        p->Draw("SAME");
        ++i;
    }

    // Labels, all sitting *above* the patch top with the same vertical
    // offsets (12 mm for the coord line, 28 mm for the ROI title) so the
    // three ROIs read consistently.  ROI 1 sits in the upper half of the
    // plate (room above); ROI 2 and 3 sit in the lower half but their
    // labels stay in the band between the patch top (y = -75 mm) and the
    // y = 0 axis, matching ROI 1's spacing.
    const double lx[3] = {  0.0,   0.0,  95.0};
    const double ly[3] = { 53.0, -47.0, -47.0};
    const int al[3] = {22, 22, 22};
    for (size_t k = 0; k < sites().size(); ++k) {
        TLatex* t = new TLatex(lx[k], ly[k], sites()[k].label);
        t->SetTextSize(0.038);
        t->SetTextAlign(al[k]);
        t->SetTextColor(kBlack);
        t->Draw("SAME");
    }

    // Coordinates of each patch centre, printed 12 mm above the patch top
    // (between the ROI title and the box) for all three sites.
    const double cx[3] = { 0.0,   0.0, 100.0};
    const double cy[3] = {37.0, -63.0, -63.0};
    for (size_t k = 0; k < sites().size(); ++k) {
        TLatex* t = new TLatex(cx[k], cy[k],
                               TString::Format("(%.0f, %.0f)", sites()[k].x, sites()[k].y));
        t->SetTextSize(0.034);
        t->SetTextAlign(21);
        t->SetTextColor(kBlack);
        t->Draw("SAME");
    }

    c->SaveAs(TString::Format("%s/irradiation_sites.pdf", outdir));
    c->SaveAs(TString::Format("%s/irradiation_sites.png", outdir));
    printf("irradiation_sites: wrote %s/irradiation_sites.{pdf,png}\n", outdir);
}
