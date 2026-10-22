// opt_layouts.C -- plan views of the three distinct sensor geometries used by
// the v7_opt detector-optimisation campaign (report Sec. 10).
//
// The ten designs of macros/opt_designs.h differ in wrapper, clamp surface
// and sensor size, but they use only three *visible* geometries:
//
//   panel 1  preset 1   8 SiPMs, 2 per side at u = +-0.5  (62.5 mm from corner)
//   panel 2  preset 8   8 SiPMs, 2 per side at u = +-0.75 (31.25 mm from corner)
//   panel 3  preset 10  four-sided WLS clamp, 4 read-out SiPMs, one per side
//
// The panels are laid out as a 2 x 2 grid: the first three quadrants contain
// the three framed data panels, while the bottom-right quadrant carries a
// compact Geometry & Symbol Key (symbol swatches, scale notes, and preset
// cross-reference).  Every data frame is square and all three use the same
// mm-per-pixel scale.  The x-axis title is printed on every data panel and the
// y-axis title only on the left-most one; tick labels remain on every panel.
//
// Sensors are drawn at true scale: a black 4 x 4 mm square (filled) as
// installed, with a white outline so it separates from the grey plate fill and
// the clamp band.  Where a 6 mm large-sensor design exists (panels 1/2) the
// 6 x 6 mm footprint is shown as a solid grey square *behind* the black 4 mm
// marker, with a thick grey outline.  Panel 3 has no 6 mm design.
//
// Only read-out sensors are drawn.  The dual-ended preset 11 (designs 3 and 8)
// shares the panel-3 layout but adds a second read-out SiPM at the far end of
// each face; the panel-3 note points to the render panel for that variant.
//
// Frame convention: report plan view, top = +y, right = +x, i.e. the simulation
// frame rotated 90 deg clockwise, (x,y) -> (y,-x).  The sensor centres are the
// same values used by position_reconstruction.C / opt_maps.C.
//
// Every caption is opt::label(i), never a raw tag, and every piece of text is
// black (the per-design Petroff colours are deliberately not used here).  The
// plate carries a black outline and a grey fill that is clearly darker than
// white, so the 250 x 250 mm footprint is visible in all three panels.
//
// Usage (ROOT_INCLUDE_PATH must contain macros/ and the shared style header):
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//     $HOME/programs/root/build/bin/root -l -b -q \
//     'macros/opt_layouts.C("report/figures")'
//
// Analysis-only: no Geant4 dependencies.  Deterministic, no timestamps.

#include <TCanvas.h>
#include <TBox.h>
#include <TH1F.h>
#include <TLine.h>
#include <TLatex.h>
#include <TPad.h>
#include <TPolyLine.h>
#include <TStyle.h>
#include <TString.h>
#include <TSystem.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "opt_designs.h"   // design table, labels, and style.h

namespace {

// ---- geometry constants (src/GeometryMuonScint.cc) ------------------------
const double kHalf        = 125.0;   // slab half-extent [mm]
const double kSpan        = 140.0;   // drawn frame half-range [mm]
const double kSiPM4       = 4.0;     // installed sensor edge [mm]
const double kSiPM6       = 6.0;     // large-sensor variant edge [mm]
// Markers are drawn oversize: at the print width of a three-panel figure a
// true 4 mm sensor is under 2 pt and unreadable.  Positions stay exact; the
// legend states the exaggeration.
const double kMinMarker   = 10.0;    // minimum drawn sensor edge [mm]
const double kClampGap    = 0.10;    // slab <-> clamp grease film [mm]
const double kClampWall   = 2.0;     // clamp bar thickness along the normal [mm]
const double kFourSidedTrim = 1.0;   // clamp ends 1 mm short of the corners
const double kHalfL       = kHalf - kFourSidedTrim;
const double kCornerLeg   = 12.0;    // preset 12 corner-truncation leg [mm]

// ---- 2 x 2 frame layout, chosen from the 460.9 pt print width --------------
// Three panels of ~230 pt (2 x 2 grid, bottom-right cell empty): the canvas is
// 2600 px wide and 3150 px high.  All
// text is sized from the *printed* capital height.  At 460.9 pt over 2600 px,
// one pt is 2600/460.9 = 5.641 px; kText gives ~7.0 pt capitals and kTitleText
// ~7.2 pt bold capitals.  The design-list line step leaves a visible gap
// between the x-axis title and the first design line, and room for the longest
// list (panel 3: four designs with TiO2 subscripts).
const int    kCols       = 2;        // grid columns
const int    kRows       = 2;        // grid rows
const int    kCanvasW    = 2600;
const int    kCanvasH    = 3150;
const double kPrintWidth = 460.9;    // report text width [pt]
const double kPadDX      = 0.004;    // half gap between panels (canvas NDC)
const double kLeftMargin = 0.130;    // y title + tick labels (left column only)
const double kRightMargin = 0.130;   // symmetric, so the frame is centred in
                                     // the pad and all captions can sit at 0.5
const double kBottomMargin = 0.320;  // x title + tick labels + design list
const double kText       = 0.056;    // pad-relative -> >= 7 pt capitals
const double kTitleText  = 0.057;    // pad-relative -> > 7 pt bold capitals
const double kListY0     = 0.205;    // first design line (pad NDC)
const double kListStep   = 0.042;    // line spacing (pad NDC)
const double kXTitleDrop = 0.075;    // x title below frame bottom (pad NDC)
const double kYTitleInset = 0.085;   // y title left of frame (pad NDC)
const double kTitleGap   = 0.030;    // panel title above frame top (pad NDC)

// Report-frame sensor centres.  For presets 1/8 the sensors sit on the slab
// faces (u = +-0.5 / +-0.75 of the half-length); for preset 10 the analytic
// four-sided WLS placement is used, with the side numbering
// 0..3 = -X, -Y, +X, +Y (rotated by the usual report-frame map).  Only the
// read-out end of each fibre is an instrumented sensor, so preset 10 returns
// exactly four centres.
std::vector<std::pair<double, double>> sensorCentres(int preset, double half)
{
    const double H = half;
    std::vector<std::pair<double, double>> t;
    switch (preset) {
        case 1: {
            const double u = 0.5 * H;
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        }
        case 8: {
            const double u = 0.75 * H;
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        }
        case 12: {
            // Convex octagon: every square corner truncated with a 12 mm leg,
            // so the 45-degree face is 17.0 mm wide and its centre lies 6 mm
            // inside the square corner, at (+-119, +-119) mm.  Report-frame
            // order = sipmHits channel order:
            //   0 BR (+119,-119)  1 TR (+119,+119)
            //   2 TL (-119,+119)  3 BL (-119,-119)
            const double c = half - 0.5 * kCornerLeg;
            t = {{ c, -c}, { c,  c}, {-c,  c}, {-c, -c}};
            break;
        }
        case 10: {
            const double kRFibre        = 0.5;    // WLS fibre radius [mm]
            const double kFibreProtr    = 0.05;   // protrusion into the film
            const double kSiPMHalfX     = 0.5 * kSiPM4;
            const double kCouplingHalfZ = 0.05;   // read-out grease pad
            const double kSiPMHalfZ     = 0.3;    // SiPM half-thickness
            const double kPadClearance  = 0.05;

            const double faceInset = kClampGap + kRFibre - kFibreProtr;
            const double padShift  = kSiPMHalfX - faceInset + kPadClearance;
            const double padX      = -(half + faceInset) - padShift;
            const double ySiPM     = kHalfL + 2.0 * kCouplingHalfZ + kSiPMHalfZ;

            auto addEnd = [&](int side) {
                const double ye = -ySiPM;    // read-out end only
                double xl = 0.0, yl = 0.0;
                switch (side) {
                    case 0:  xl =  padX; yl =  ye;   break;
                    case 1:  xl = -ye;   yl =  padX; break;
                    case 2:  xl = -padX; yl = -ye;   break;
                    default: xl =  ye;   yl = -padX; break;
                }
                t.push_back({yl, -xl});     // report frame
            };
            for (int side = 0; side < 4; ++side) addEnd(side);
            break;
        }
        default:
            break;
    }
    return t;
}

// Printed capital height of a text object.  TLatex must be NDC-sized and gPad
// must already be the target pad; GetTextExtent() returns canvas pixels, which
// are converted to the report's printed points at kPrintWidth.
void printCapitalHeight(const char* tag, int font, double size, int canvasW)
{
    TLatex t;
    t.SetNDC();
    t.SetTextFont(font);
    t.SetTextSize(size);
    UInt_t w = 0, h = 0;
    t.GetTextExtent(w, h, "H");
    printf("opt_layouts: %-13s font %2d size %.3f -> capital %2u px = %.2f pt "
           "at %.1f pt print width\n",
           tag, font, size, h, h * kPrintWidth / canvasW, kPrintWidth);
}

struct PanelCfg {
    int preset;
    std::vector<int> designs;     // indices into opt::designs()
    const char* title;            // panel title (black, bold)
    std::vector<const char*> notes;   // extra black lines under the design list
};

// Pixel-space bounding boxes actually used by a panel, for the post-run checks.
struct PanelMetrics {
    double frameLeft = 0, frameRight = 0, frameTop = 0, frameBottom = 0;
    double textLeft = 0, textRight = 0, textTop = 0, textBottom = 0;
    double pxPerMM = 0;
};

void drawPanel(TPad* p, const std::vector<opt::Design>& ds, const PanelCfg& P,
               bool showXTitle, bool showYTitle, int canvasW, int canvasH,
               PanelMetrics& M)
{
    p->cd();

    if (P.preset == 1) {
        printCapitalHeight("axis/caption", 42, kText, canvasW);
        printCapitalHeight("panel title",  62, kTitleText, canvasW);
    }

    // ---- frame: square, equal mm scale on both axes ----------------------
    // Empty axis titles: the titles are drawn manually below, which keeps
    // their position independent of ROOT's axis-title offset heuristics.
    TH1F* frame = p->DrawFrame(-kSpan, -kSpan, kSpan, kSpan, ";;");
    frame->GetXaxis()->SetTitleSize(0.0);
    frame->GetYaxis()->SetTitleSize(0.0);
    frame->GetXaxis()->SetLabelSize(kText);
    frame->GetYaxis()->SetLabelSize(kText);
    frame->GetXaxis()->SetNdivisions(505);
    frame->GetYaxis()->SetNdivisions(505);

    // ---- slab: black outline, visibly grey fill (contrast ~1.45:1) -------
    const Color_t plateFill = (Color_t)TColor::GetColor("#d0d5dc");
    if (P.preset == 12) {
        // Convex octagon: square corners cut by the line |x|+|y| = 2*kHalf-12.
        const double a = kHalf - kCornerLeg;
        double xs[8] = { kHalf,  kHalf,     a,    -a, -kHalf, -kHalf,    -a,     a};
        double ys[8] = {    -a,      a, kHalf, kHalf,      a,     -a, -kHalf, -kHalf};
        TPolyLine* fill = new TPolyLine(8, xs, ys);
        fill->SetFillColor(plateFill);
        fill->SetFillStyle(1001);
        fill->SetLineColor(plateFill);
        fill->SetLineWidth(1);
        fill->Draw("F");
        // Separate transparent outline: a filled TPolyLine hides its own line.
        TPolyLine* edge = new TPolyLine(8, xs, ys);
        edge->SetFillStyle(0);
        edge->SetLineColor(kBlack);
        edge->SetLineWidth(2);
        edge->Draw("L SAME");
    } else {
        TBox* slab = new TBox(-kHalf, -kHalf, kHalf, kHalf);
        slab->SetFillColor(plateFill);
        slab->SetFillStyle(1001);
        slab->SetLineColor(plateFill);
        slab->SetLineWidth(1);
        slab->Draw("SAME");
        // ROOT paints a filled TBox after its outline, so a separate
        // transparent box is needed to make the black plate outline visible.
        TBox* slabEdge = new TBox(-kHalf, -kHalf, kHalf, kHalf);
        slabEdge->SetFillStyle(0);
        slabEdge->SetLineColor(kBlack);
        slabEdge->SetLineWidth(2);
        slabEdge->Draw("SAME");
    }

    TLine* hx = new TLine(-kHalf, 0., kHalf, 0.);
    TLine* vy = new TLine(0., -kHalf, 0., kHalf);
    hx->SetLineColor(kGray + 1); hx->SetLineStyle(2); hx->SetLineWidth(1);
    vy->SetLineColor(kGray + 1); vy->SetLineStyle(2); vy->SetLineWidth(1);
    hx->Draw("SAME"); vy->Draw("SAME");

    // ---- four-sided WLS clamp: a band along each narrow face -------------
    if (P.preset == 10) {
        const double bandIn  = kHalf + kClampGap;                 // 125.10 mm
        const double bandOut = bandIn + kClampWall;               // 127.10 mm
        const Color_t bandFill = (Color_t)TColor::GetColor("#b6bcc4");
        auto band = [&](double x0, double y0, double x1, double y1) {
            TBox* b = new TBox(x0, y0, x1, y1);
            b->SetFillColor(bandFill);
            b->SetFillStyle(1001);
            b->SetLineColor(kGray + 3);
            b->SetLineWidth(2);
            b->Draw("SAME");
        };
        band(-kHalfL, bandIn,  kHalfL, bandOut);    // top
        band(-kHalfL, -bandOut, kHalfL, -bandIn);   // bottom
        band(bandIn, -kHalfL, bandOut, kHalfL);     // right
        band(-bandOut, -kHalfL, -bandIn, kHalfL);   // left
    }

    // ---- sensors ----------------------------------------------------------
    const auto centres = sensorCentres(P.preset, kHalf);

    // A 6 mm footprint is meaningful only when this layout is used by a
    // large-sensor design; panel 3 therefore has no grey box at all.
    bool hasSixMM = false;
    for (int idx : P.designs)
        if (ds[idx].sipm_mm > 5.0) hasSixMM = true;

    for (const auto& centre : centres) {
        const double cx = centre.first;
        const double cy = centre.second;

        // Large-sensor variant: solid grey square *behind* the black marker,
        // with a thick outline so the 6 mm footprint reads at print size.
        // Explicit greys are used because the kGray+N indices are not
        // guaranteed to resolve to distinguishable shades in batch mode.
        if (hasSixMM) {
            const double m6 = std::max(2.5 * kSiPM6, kMinMarker);
            const Color_t sixFill = (Color_t)TColor::GetColor("#9fa5ac");
            const Color_t sixEdge = (Color_t)TColor::GetColor("#4a5058");
            TBox* g = new TBox(cx - 0.5 * m6, cy - 0.5 * m6,
                               cx + 0.5 * m6, cy + 0.5 * m6);
            g->SetFillColor(sixFill);
            g->SetFillStyle(1001);
            g->SetLineColor(sixFill);
            g->SetLineWidth(1);
            g->Draw("SAME");
            // Transparent outline box: a filled TBox hides its own outline.
            TBox* go = new TBox(cx - 0.5 * m6, cy - 0.5 * m6,
                                cx + 0.5 * m6, cy + 0.5 * m6);
            go->SetFillStyle(0);
            go->SetLineColor(sixEdge);
            go->SetLineWidth(2);
            go->Draw("SAME");
        }

        // Read-out sensor: filled black, with a white outline that separates
        // it from the grey plate fill and from the clamp band in panel 3.
        const double m4 = std::max(kSiPM4, kMinMarker);
        if (P.preset == 12) {
            // The corner-cut faces are at 45 deg.  An axis-aligned marker
            // would hide most of the 17 mm chamfer, so draw the square
            // aligned with the face and flush with its outer surface: the
            // 2.5x-oversize marker then leaves the same 3.5 mm margin on
            // either side that the real 4 mm sensor leaves.
            const double sx = (cx > 0.0 ? 1.0 : -1.0);
            const double sy = (cy > 0.0 ? 1.0 : -1.0);
            const double nx = sx / std::sqrt(2.0);   // face normal (outward)
            const double ny = sy / std::sqrt(2.0);
            const double tx = -ny;                   // along the face
            const double ty = nx;
            const double ox = cx - 0.5 * m4 * nx;    // centre, moved inward
            const double oy = cy - 0.5 * m4 * ny;
            const double h  = 0.5 * m4;
            Double_t xs[5] = {ox + h * (tx + nx), ox + h * (tx - nx),
                              ox - h * (tx + nx), ox - h * (tx - nx),
                              ox + h * (tx + nx)};
            Double_t ys[5] = {oy + h * (ty + ny), oy + h * (ty - ny),
                              oy - h * (ty + ny), oy - h * (ty - ny),
                              oy + h * (ty + ny)};
            TPolyLine* s = new TPolyLine(5, xs, ys);
            s->SetFillColor((Color_t)kBlack);
            s->SetFillStyle(1001);
            s->SetLineColor(kWhite);
            s->SetLineWidth(1);
            s->Draw("F");
            s->Draw("L");
        } else {
            TBox* s = new TBox(cx - 0.5 * m4, cy - 0.5 * m4,
                               cx + 0.5 * m4, cy + 0.5 * m4);
            s->SetFillColor((Color_t)kBlack);
            s->SetFillStyle(1001);
            s->SetLineColor((Color_t)kBlack);
            s->SetLineWidth(1);
            s->Draw("SAME");
            // Separate transparent outline so the white border is not painted
            // over by the black fill.
            TBox* so = new TBox(cx - 0.5 * m4, cy - 0.5 * m4,
                                cx + 0.5 * m4, cy + 0.5 * m4);
            so->SetFillStyle(0);
            so->SetLineColor(kWhite);
            so->SetLineWidth(1);
            so->Draw("SAME");
        }
    }

    // ---- manual axis titles ----------------------------------------------
    if (showXTitle) {
        TLatex tx;
        tx.SetNDC();
        tx.SetTextAlign(22);
        tx.SetTextFont(42);
        tx.SetTextSize(kText);
        tx.SetTextColor(kBlack);
        tx.DrawLatex(0.5, p->GetBottomMargin() - kXTitleDrop, "x [mm]");
    }
    if (showYTitle) {
        TLatex ty;
        ty.SetNDC();
        ty.SetTextAlign(22);
        ty.SetTextAngle(90);
        ty.SetTextFont(42);
        ty.SetTextSize(kText);
        ty.SetTextColor(kBlack);
        const double yc = 0.5 * (p->GetBottomMargin() + 1.0 - p->GetTopMargin());
        ty.DrawLatex(p->GetLeftMargin() - kYTitleInset, yc, "y [mm]");
    }

    // ---- panel title (black, bold), sitting just above the frame ---------
    TLatex t;
    t.SetNDC();
    t.SetTextAlign(22);
    t.SetTextFont(62);
    t.SetTextSize(kTitleText);
    t.SetTextColor(kBlack);
    t.DrawLatex(0.5, 1.0 - p->GetTopMargin() + kTitleGap, P.title);

    // ---- design list + note, black, under the frame ----------------------
    TLatex l;
    l.SetNDC();
    l.SetTextAlign(22);
    l.SetTextFont(42);
    l.SetTextSize(kText);
    l.SetTextColor(kBlack);

    const double padW = p->GetAbsWNDC() * canvasW;
    const double padH = p->GetAbsHNDC() * canvasH;
    const double padX0 = p->GetAbsXlowNDC() * canvasW;
    const double padY0 = p->GetAbsYlowNDC() * canvasH;
    const double xCenter = padX0 + 0.5 * padW;

    double textTop = -1.0e30, textBottom = 1.0e30;
    double textLeft = 1.0e30, textRight = -1.0e30;

    auto addTextBounds = [&](const char* s, double yNdc) {
        TLatex m;
        m.SetNDC();
        m.SetTextFont(42);
        m.SetTextSize(kText);
        UInt_t w = 0, h = 0;
        m.GetTextExtent(w, h, s);
        const double yc = padY0 + yNdc * padH;
        textLeft  = std::min(textLeft,  xCenter - 0.5 * w);
        textRight = std::max(textRight, xCenter + 0.5 * w);
        textTop   = std::max(textTop,   yc + 0.5 * h);
        textBottom = std::min(textBottom, yc - 0.5 * h);
    };

    // The step is adaptive: a label whose subscript descends below the
    // baseline needs more room than a plain one, and TLatex reports that in
    // its text extent.  Every line keeps at least kListStep, so the pitch is
    // uniform within a panel unless a subscript forces it wider.
    std::vector<const char*> lines;
    for (int idx : P.designs) lines.push_back(ds[idx].label);
    for (const char* note : P.notes) lines.push_back(note);

    // A label with a TLatex subscript ("TiO_{2}") descends below the cap box,
    // so a panel containing one uses a wider base pitch; the descent itself is
    // not part of the extent that GetTextExtent reports.
    bool hasSub = false;
    for (const char* t : lines)
        if (std::string(t).find("_{") != std::string::npos) hasSub = true;
    const double baseStep = kListStep + (hasSub ? 22.0 / padH : 0.0);

    // A panel whose labels carry a subscript starts its list slightly lower, so
    // the taller first line does not crowd the x-axis title above it.
    double y = kListY0 - (hasSub ? 10.0 / padH : 0.0);
    for (size_t i = 0; i < lines.size(); ++i) {
        l.DrawLatex(0.5, y, lines[i]);
        addTextBounds(lines[i], y);
        UInt_t w = 0, h = 0;
        l.GetTextExtent(w, h, lines[i]);
        // Fixed pitch in a subscript panel so the lines read as one block.
        const double step = hasSub ? baseStep : std::max(baseStep, (h + 12.0) / padH);
        y -= step;
    }

    // ---- diagnostics: pixel positions actually used by this pad ----------
    M.frameLeft  = padX0 + p->GetLeftMargin() * padW;
    M.frameRight = padX0 + (1.0 - p->GetRightMargin()) * padW;
    M.frameBottom = padY0 + p->GetBottomMargin() * padH;
    M.frameTop    = padY0 + (1.0 - p->GetTopMargin()) * padH;
    M.textLeft = textLeft; M.textRight = textRight;
    M.textTop = textTop;   M.textBottom = textBottom;
    M.pxPerMM = (M.frameRight - M.frameLeft) / (2.0 * kSpan);

    printf("opt_layouts: preset %2d frame px x=[%.0f,%.0f] (%.0f wide), "
           "y=[%.0f,%.0f] (%.0f high)  px/mm %.3f  4mm=%.1f px  6mm=%.1f px\n",
           P.preset, M.frameLeft, M.frameRight, M.frameRight - M.frameLeft,
           M.frameBottom, M.frameTop, M.frameTop - M.frameBottom,
           M.pxPerMM, kSiPM4 * M.pxPerMM, kSiPM6 * M.pxPerMM);
    printf("opt_layouts: preset %2d text block px x=[%.0f,%.0f], "
           "y=[%.0f,%.0f] (bottom %.0f px above canvas bottom)\n",
           P.preset, M.textLeft, M.textRight, M.textBottom, M.textTop,
           M.textBottom);
}

// Draw the compact Geometry & Symbol Key in the fourth quadrant (bottom-right cell).
// Provides an exact symbol key (4 mm active SiPM, 6 mm footprint, WLS clamp bar,
// scintillator plate), scale notes, and preset cross-reference without drawing an axis frame.
void drawKeyPanel(TPad* p, int canvasW, int canvasH, PanelMetrics& M)
{
    p->cd();
    p->Range(0, 0, 1, 1);

    const double padW = p->GetAbsWNDC() * canvasW;
    const double padH = p->GetAbsHNDC() * canvasH;
    const double padX0 = p->GetAbsXlowNDC() * canvasW;
    const double padY0 = p->GetAbsYlowNDC() * canvasH;

    // Palette & colors (consistent with drawPanel)
    const Color_t plateFill = (Color_t)TColor::GetColor("#d0d5dc");
    const Color_t sixFill   = (Color_t)TColor::GetColor("#9fa5ac");
    const Color_t sixEdge   = (Color_t)TColor::GetColor("#4a5058");
    const Color_t bandFill  = (Color_t)TColor::GetColor("#b6bcc4");
    const Color_t wlsFiber  = (Color_t)TColor::GetColor("#2ca02c");
    const Color_t lineGrey  = (Color_t)TColor::GetColor("#cbd0d8");

    const double fontTitleSz = kTitleText;  // 0.057 -> 8.88 pt
    const double fontHeadSz  = 0.046;       // 0.046 -> 7.28 pt
    const double fontBodySz  = 0.042;       // 0.042 -> 6.57 pt

    printCapitalHeight("key title",    62, fontTitleSz, canvasW);
    printCapitalHeight("key subhead",  62, fontHeadSz,  canvasW);
    printCapitalHeight("key body",     42, fontBodySz,  canvasW);

    // Title (aligned with panel titles at y = 1 - topMargin + kTitleGap = 0.965)
    const double topMargin = p->GetTopMargin();
    TLatex tTitle;
    tTitle.SetNDC();
    tTitle.SetTextAlign(22);
    tTitle.SetTextFont(62);
    tTitle.SetTextSize(fontTitleSz);
    tTitle.SetTextColor(kBlack);
    tTitle.DrawLatex(0.50, 1.0 - topMargin + kTitleGap, "Geometry & Symbol Key");

    // Top rule (matching the top of the frame in the left panel at 1.0 - topMargin = 0.935)
    const double xL = kLeftMargin;          // 0.130
    const double xR = 1.0 - kRightMargin;   // 0.870
    TLine* lTop = new TLine(xL, 1.0 - topMargin, xR, 1.0 - topMargin);
    lTop->SetLineColor(lineGrey);
    lTop->SetLineWidth(2);
    lTop->Draw();

    // Section 1 Header: Component Symbols
    TLatex tHead;
    tHead.SetNDC();
    tHead.SetTextAlign(12);
    tHead.SetTextFont(62);
    tHead.SetTextSize(fontHeadSz);
    tHead.SetTextColor(kBlack);
    tHead.DrawLatex(xL, 0.895, "COMPONENT SYMBOLS");

    // Helper to draw square in pixels
    auto drawSquare = [&](double cx, double cy, double sz_px, Color_t fill, Color_t line, int lw, bool outlineOnly) {
        double dy = sz_px / padH;
        double dx = sz_px / padW;
        TBox* b = new TBox(cx - 0.5 * dx, cy - 0.5 * dy, cx + 0.5 * dx, cy + 0.5 * dy);
        if (outlineOnly) {
            b->SetFillStyle(0);
            b->SetLineColor(line);
            b->SetLineWidth(lw);
        } else {
            b->SetFillColor(fill);
            b->SetFillStyle(1001);
            b->SetLineColor(line);
            b->SetLineWidth(lw);
        }
        b->Draw("SAME");
    };

    TLatex tLabel, tDesc;
    tLabel.SetNDC(); tLabel.SetTextAlign(12); tLabel.SetTextFont(62); tLabel.SetTextSize(fontBodySz); tLabel.SetTextColor(kBlack);
    tDesc.SetNDC();  tDesc.SetTextAlign(12); tDesc.SetTextFont(42); tDesc.SetTextSize(fontBodySz);  tDesc.SetTextColor(kBlack);

    const double xSwatch = 0.170;
    const double xText   = 0.225;

    // Track text bounds for diagnostics
    double textLeft = 1.0e30, textRight = -1.0e30;
    double textTop = -1.0e30, textBottom = 1.0e30;
    auto trackText = [&](const char* str, double xNdc, double yNdc, int font, double sz, int align) {
        TLatex m;
        m.SetNDC(); m.SetTextFont(font); m.SetTextSize(sz);
        UInt_t w = 0, h = 0;
        m.GetTextExtent(w, h, str);
        const double px = padX0 + xNdc * padW;
        const double py = padY0 + yNdc * padH;
        double xl = px, xr = px;
        if (align == 12) { xl = px; xr = px + w; }
        else if (align == 22) { xl = px - 0.5 * w; xr = px + 0.5 * w; }
        textLeft   = std::min(textLeft, xl);
        textRight  = std::max(textRight, xr);
        textTop    = std::max(textTop, py + 0.5 * h);
        textBottom = std::min(textBottom, py - 0.5 * h);
    };

    trackText("Geometry & Symbol Key", 0.50, 1.0 - topMargin + kTitleGap, 62, fontTitleSz, 22);
    trackText("COMPONENT SYMBOLS", xL, 0.895, 62, fontHeadSz, 12);

    // 1. Standard SiPM (4 mm)
    double y = 0.830;
    drawSquare(xSwatch, y, 36, kBlack, kBlack, 1, false);
    drawSquare(xSwatch, y, 36, kWhite, kWhite, 2, true);
    drawSquare(xSwatch, y, 38, kBlack, kBlack, 1, true);
    tLabel.DrawLatex(xText, y + 0.016, "4 #times 4 mm^{2} SiPM (active)");
    tDesc.DrawLatex(xText,  y - 0.016, "Read-out sensor (presets 1, 8, 10)");
    trackText("4 #times 4 mm^{2} SiPM (active)", xText, y + 0.016, 62, fontBodySz, 12);
    trackText("Read-out sensor (presets 1, 8, 10)", xText, y - 0.016, 42, fontBodySz, 12);

    // 2. Large 6 mm footprint
    y = 0.735;
    drawSquare(xSwatch, y, 54, sixFill, sixFill, 1, false);
    drawSquare(xSwatch, y, 54, sixEdge, sixEdge, 2, true);
    drawSquare(xSwatch, y, 36, kBlack, kBlack, 1, false);
    drawSquare(xSwatch, y, 36, kWhite, kWhite, 2, true);
    tLabel.DrawLatex(xText, y + 0.016, "6 #times 6 mm^{2} footprint");
    tDesc.DrawLatex(xText,  y - 0.016, "Large-area variant (C1/C8 diffuse)");
    trackText("6 #times 6 mm^{2} footprint", xText, y + 0.016, 62, fontBodySz, 12);
    trackText("Large-area variant (C1/C8 diffuse)", xText, y - 0.016, 42, fontBodySz, 12);

    // 3. Four-sided WLS clamp
    y = 0.640;
    {
        double barW = 75.0, barH = 34.0;
        double dy = barH / padH, dx = barW / padW;
        TBox* b = new TBox(xSwatch - 0.5 * dx, y - 0.5 * dy, xSwatch + 0.5 * dx, y + 0.5 * dy);
        b->SetFillColor(bandFill); b->SetFillStyle(1001); b->SetLineColor(sixEdge); b->SetLineWidth(2);
        b->Draw("SAME");
        TLine* f = new TLine(xSwatch - 0.5 * dx + 0.005, y, xSwatch + 0.5 * dx - 0.005, y);
        f->SetLineColor(wlsFiber); f->SetLineWidth(3);
        f->Draw("SAME");
    }
    tLabel.DrawLatex(xText, y + 0.016, "Four-sided WLS clamp");
    tDesc.DrawLatex(xText,  y - 0.016, "2 mm bar on 4 faces (preset 10)");
    trackText("Four-sided WLS clamp", xText, y + 0.016, 62, fontBodySz, 12);
    trackText("2 mm bar on 4 faces (preset 10)", xText, y - 0.016, 42, fontBodySz, 12);

    // 4. Scintillator plate
    y = 0.545;
    {
        double barW = 75.0, barH = 46.0;
        double dy = barH / padH, dx = barW / padW;
        TBox* b = new TBox(xSwatch - 0.5 * dx, y - 0.5 * dy, xSwatch + 0.5 * dx, y + 0.5 * dy);
        b->SetFillColor(plateFill); b->SetFillStyle(1001); b->SetLineColor(kBlack); b->SetLineWidth(2);
        b->Draw("SAME");
        TLine* hx = new TLine(xSwatch - 0.5 * dx, y, xSwatch + 0.5 * dx, y);
        TLine* vy = new TLine(xSwatch, y - 0.5 * dy, xSwatch, y + 0.5 * dy);
        hx->SetLineColor(kGray + 1); hx->SetLineStyle(2); hx->SetLineWidth(1);
        vy->SetLineColor(kGray + 1); vy->SetLineStyle(2); vy->SetLineWidth(1);
        hx->Draw("SAME"); vy->Draw("SAME");
    }
    tLabel.DrawLatex(xText, y + 0.016, "Scintillator plate (10 mm)");
    tDesc.DrawLatex(xText,  y - 0.016, "250 #times 250 mm^{2} UPS-92S");
    trackText("Scintillator plate (10 mm)", xText, y + 0.016, 62, fontBodySz, 12);
    trackText("250 #times 250 mm^{2} UPS-92S", xText, y - 0.016, 42, fontBodySz, 12);

    // Mid divider line (between symbols and scale notes)
    TLine* lMid = new TLine(xL, 0.465, xR, 0.465);
    lMid->SetLineColor(lineGrey);
    lMid->SetLineWidth(2);
    lMid->Draw();

    // Section 2 Header: Scale & Orientation
    tHead.DrawLatex(xL, 0.425, "SCALE & ORIENTATION");
    trackText("SCALE & ORIENTATION", xL, 0.425, 62, fontHeadSz, 12);

    TLatex tNote;
    tNote.SetNDC(); tNote.SetTextAlign(12); tNote.SetTextFont(42); tNote.SetTextSize(fontBodySz); tNote.SetTextColor(kBlack);

    tNote.DrawLatex(xL, 0.375, "Positions to scale; markers 2.5#times");
    trackText("Positions to scale; markers 2.5#times", xL, 0.375, 42, fontBodySz, 12);

    tNote.DrawLatex(xL, 0.330, "Report plan view: top = +y, right = +x");
    trackText("Report plan view: top = +y, right = +x", xL, 0.330, 42, fontBodySz, 12);

    // Divider line before preset reference
    TLine* lLow = new TLine(xL, 0.285, xR, 0.285);
    lLow->SetLineColor(lineGrey);
    lLow->SetLineWidth(2);
    lLow->Draw();

    // Section 3 Header: Preset Reference
    tHead.DrawLatex(xL, 0.245, "PRESET REFERENCE");
    trackText("PRESET REFERENCE", xL, 0.245, 62, fontHeadSz, 12);

    tNote.DrawLatex(xL, 0.195, "Preset 1:  8 SiPMs (u = #pm0.5)");
    trackText("Preset 1:  8 SiPMs (u = #pm0.5)", xL, 0.195, 42, fontBodySz, 12);

    tNote.DrawLatex(xL, 0.150, "Preset 8:  8 SiPMs (u = #pm0.75)");
    trackText("Preset 8:  8 SiPMs (u = #pm0.75)", xL, 0.150, 42, fontBodySz, 12);

    tNote.DrawLatex(xL, 0.105, "Preset 10: 4 WLS channels (1 / face)");
    trackText("Preset 10: 4 WLS channels (1 / face)", xL, 0.105, 42, fontBodySz, 12);

    tNote.DrawLatex(xL, 0.060, "Preset 11: 8 WLS ch (dual read-out)");
    trackText("Preset 11: 8 WLS ch (dual read-out)", xL, 0.060, 42, fontBodySz, 12);

    // Fill metrics
    M.frameLeft   = padX0 + xL * padW;
    M.frameRight  = padX0 + xR * padW;
    M.frameBottom = padY0 + p->GetBottomMargin() * padH;
    M.frameTop    = padY0 + (1.0 - topMargin) * padH;
    M.textLeft    = textLeft;
    M.textRight   = textRight;
    M.textTop     = textTop;
    M.textBottom  = textBottom;
    M.pxPerMM     = 0.0;

    printf("opt_layouts: key panel text block px x=[%.0f,%.0f], y=[%.0f,%.0f] "
           "(bottom %.0f px above canvas bottom)\n",
           M.textLeft, M.textRight, M.textBottom, M.textTop, M.textBottom);
    printf("opt_layouts: key panel horizontal bounds: left %.0f px (frame left %.0f px), "
           "right %.0f px (frame right %.0f px) -> inside frame\n",
           M.textLeft, M.frameLeft, M.textRight, M.frameRight);
}

}  // namespace

void opt_layouts(const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    const std::vector<opt::Design> ds = opt::designs();

    // Ten designs, three visible geometries.  Panel 1 carries the three C1
    // designs (black, diffuse, 6 mm diffuse) plus the control; panel 2 the two
    // C8 designs; panel 3 all four fibre designs.
    std::vector<PanelCfg> panels = {
        {1,  {0, 4, 5, 9}, "preset 1, 8 SiPMs, u = #pm0.5",
                           {}},
        {8,  {1, 6},       "preset 8, 8 SiPMs, u = #pm0.75",
                           {"Grey outline: 6 mm footprint"}},
        {10, {2, 7, 3, 8}, "preset 10, four-sided WLS", {}}
    };

    TCanvas* c = new TCanvas("c_opt_layouts", "v7_opt sensor geometries",
                             kCanvasW, kCanvasH);
    c->SetFillColor(kWhite);

    // ROOT clamps the requested canvas size, so derive the square-frame top
    // margin from the size actually used.
    const int CW = c->GetWw();
    const int CH = c->GetWh();
    const double padW = (1.0 / kCols - 2.0 * kPadDX) * CW;
    const double padH = (1.0 / kRows - 2.0 * kPadDX) * CH;
    const double frameW = (1.0 - kLeftMargin - kRightMargin) * padW;
    const double topMargin = 1.0 - kBottomMargin - frameW / padH;
    printf("opt_layouts: canvas %d x %d px; pad %.0f x %.0f px; frame target "
           "%.0f px; top margin %.3f\n", CW, CH, padW, padH, frameW, topMargin);
    if (topMargin < 0.05)
        printf("opt_layouts: WARNING top margin %.3f is too small for the "
               "panel title\n", topMargin);

    std::vector<PanelMetrics> metrics(panels.size());

    for (int i = 0; i < (int)panels.size(); ++i) {
        const int col = i % kCols;
        const int row = i / kCols;
        const double x0 = col / (double)kCols + kPadDX;
        const double x1 = (col + 1) / (double)kCols - kPadDX;
        const double y0 = 1.0 - (row + 1) / (double)kRows + kPadDX;
        const double y1 = 1.0 - row / (double)kRows - kPadDX;

        c->cd();                      // parent pad must be the canvas, not the
                                      // pad drawn in the previous iteration
        TPad* p = new TPad(TString::Format("p_opt_layout_%d", i), "",
                           x0, y0, x1, y1);
        p->SetLeftMargin(kLeftMargin);
        p->SetRightMargin(kRightMargin);
        p->SetBottomMargin(kBottomMargin);
        p->SetTopMargin(topMargin);   // frame is square (see the saved PNG)
        p->SetTickx();
        p->SetTicky();
        p->SetFrameLineWidth(1);
        p->Draw();
        // The x title is printed on every panel; the y title only on the
        // left-column panels.
        drawPanel(p, ds, panels[i], true, col == 0, CW, CH, metrics[i]);
    }

    // ---- quadrant 4 (bottom-right): Geometry & Symbol Key ----------------
    PanelMetrics keyMetrics;
    {
        const int col = 1;
        const int row = 1;
        const double x0 = col / (double)kCols + kPadDX;
        const double x1 = (col + 1) / (double)kCols - kPadDX;
        const double y0 = 1.0 - (row + 1) / (double)kRows + kPadDX;
        const double y1 = 1.0 - row / (double)kRows - kPadDX;

        c->cd();
        TPad* pKey = new TPad("p_opt_key", "", x0, y0, x1, y1);
        pKey->SetFillColor(kWhite);
        pKey->SetLeftMargin(kLeftMargin);
        pKey->SetRightMargin(kRightMargin);
        pKey->SetBottomMargin(kBottomMargin);
        pKey->SetTopMargin(topMargin);
        pKey->Draw();
        drawKeyPanel(pKey, CW, CH, keyMetrics);
    }

    // ---- confirm that all ten design labels were listed ------------------
    std::vector<int> seen(ds.size(), 0);
    for (const PanelCfg& P : panels)
        for (int idx : P.designs) seen[idx]++;

    bool allListed = true;
    for (size_t i = 0; i < ds.size(); ++i) {
        if (seen[i] != 1) allListed = false;
        printf("opt_layouts: design %2zu  %-34s  drawn %d time(s)\n",
               i, ds[i].label, seen[i]);
    }
    printf("opt_layouts: all %zu design names listed exactly once: %s\n",
           ds.size(), allListed ? "yes" : "NO");

    // ---- layout clearances ------------------------------------------------
    // Within a row: the widest line of panel 1's text block must stay inside
    // its own pad and must not reach panel 2's pad.
    const double padRight0 = (1.0 / kCols - kPadDX) * CW;
    const double padLeft1  = (1.0 / kCols + kPadDX) * CW;
    printf("opt_layouts: horizontal clearance: panel-1 text right %.0f px, "
           "panel-1 pad right %.0f px -> %.0f px inside its pad\n",
           metrics[0].textRight, padRight0,
           padRight0 - metrics[0].textRight);
    printf("opt_layouts: horizontal gap: panel-1 text right %.0f px, "
           "panel-2 pad left %.0f px -> %.0f px\n",
           metrics[0].textRight, padLeft1,
           padLeft1 - metrics[0].textRight);
    // Between rows: panel 1 is top-left, panel 3 immediately below it; the
    // bottom of panel 1's caption block must stay above panel 3's *title*,
    // which is drawn kTitleGap*padH above its frame top.
    const double title3Top = metrics[2].frameTop + kTitleGap * padH;
    printf("opt_layouts: vertical clearance: panel-1 text bottom %.0f px, "
           "panel-3 title top %.0f px -> %.0f px\n",
           metrics[0].textBottom, title3Top,
           metrics[0].textBottom - title3Top);
    printf("opt_layouts: panel-3 (bottom-left) text bottom is %.0f px above "
           "the canvas bottom; its frame bottom is %.0f px above it\n",
           metrics[2].textBottom, metrics[2].frameBottom);
    // Vertical clearance: panel 2 is top-right, key panel immediately below it; the
    // bottom of panel 2's caption block must stay above the key panel's title,
    // which is drawn kTitleGap*padH above its frame top.
    const double titleKeyTop = keyMetrics.frameTop + kTitleGap * padH;
    printf("opt_layouts: vertical clearance: panel-2 text bottom %.0f px, "
           "key title top %.0f px -> %.0f px\n",
           metrics[1].textBottom, titleKeyTop,
           metrics[1].textBottom - titleKeyTop);
    printf("opt_layouts: key panel (bottom-right) text bottom is %.0f px above "
           "the canvas bottom; its frame bottom is %.0f px\n",
           keyMetrics.textBottom, keyMetrics.frameBottom);

    c->SaveAs(TString::Format("%s/fig_design_layouts.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_design_layouts.png", outdir));
    printf("opt_layouts: wrote %s/fig_design_layouts.{pdf,png}\n", outdir);
}
