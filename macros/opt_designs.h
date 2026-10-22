// opt_designs.h -- v7_opt design table, the single source of truth for every
// Phase-2 ("detector optimisation") analysis macro.
//
// The v7_opt campaign is NOT another reflector grid: it is ten independent
// detector *designs*, one whole-plate file each, differing in SiPM layout
// (preset), edge treatment, clamp surface and sensor size.  The file name
// carries the design tag:
//
//     <dir>/muon_whole_out_c<cfg>_r<refl>_<tag>.root
//
// so a macro must never glob "muon_whole_out_c*_r*" and assume one file per
// (config, reflector): several designs share (cfg, refl) and would be silently
// merged.  Use OptFile() below.
//
// The table is transcribed from slurm/generate_opt_whole.sh (the generator),
// which is the authority for the knobs actually baked into the macros; the
// 20-event preview yields live in HANDOFF.md §6.
//
// Everything is header-inline so the macros stay single-file `root -l -b -q`
// invocations.  Include it as
//
//     #include "opt_designs.h"
//
// and run with the repo's macros/ directory on the include path:
//
//     env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//         $HOME/programs/root/build/bin/root -l -b -q 'macros/<macro>.C("…")'
//
// (Cling does not resolve quoted includes relative to the including file, so
// the path variable is required.)  "style.h" is pulled by this header.

#ifndef OPT_DESIGNS_H
#define OPT_DESIGNS_H

#include "style.h"

#include <TColor.h>
#include <TString.h>

#include <vector>

namespace opt {

// Grouping used by the figures.  Stems follow the Phase-2 rationale:
//   position  -- Stem 1: black edges, pay yield for ballistic first-pass light
//   wls       -- four-sided WLS fibre read-out (the paper's config 3/4 idea)
//   maxpe     -- Stem 2: diffuse edges, maximise photoelectrons
//   control   -- the v7 production layout at the new coupling index
struct Design {
    const char* tag;      // design tag, part of the file name
    int         cfg;      // read-out config / output file id (1, 8, 10, 11)
    int         refl;     // 0 none, 1 TiO2, 2 Al, 3 glossy
    int         preset;   // DetectorConstruction preset
    const char* edge;     // same | black | diffuse | mirror
    const char* clamp;    // white | black | mirror (WLS presets only)
    double      sipm_mm;  // SiPM edge length [mm]
    int         nch;      // number of SiPM channels in sipmHits
    const char* stem;     // position | wls | maxpe | control | corner
    const char* label;    // short label for legends and tables
};

// Ten designs, in manifest / array order.
inline std::vector<Design> designs()
{
    return {
        {"c1_split_black",  1, 3,  1, "black",   "white",  4.0, 8, "position", "C1 black edges"},
        {"c8_split_black",  8, 3,  8, "black",   "white",  4.0, 8, "position", "C8 black edges"},
        {"c10_wls4_tio2",  10, 1, 10, "same",    "white",  4.0, 4, "wls",      "4-sided WLS (4 ch), TiO_{2}"},
        {"c11_wls8_tio2",  11, 1, 11, "same",    "white",  4.0, 8, "wls",      "4-sided WLS (8 ch, dual), TiO_{2}"},
        {"c1_edge_diffuse", 1, 3,  1, "diffuse", "white",  4.0, 8, "maxpe",    "C1 diffuse edges"},
        {"c1_6mm_diffuse",  1, 3,  1, "diffuse", "white",  6.0, 8, "maxpe",    "C1 6 mm, diffuse edges"},
        {"c8_6mm_diffuse",  8, 3,  8, "diffuse", "white",  6.0, 8, "maxpe",    "C8 6 mm, diffuse edges"},
        {"c10_wls4_glossy", 10, 3, 10, "same",   "mirror", 4.0, 4, "wls",      "4-sided WLS (4 ch), glossy"},
        {"c11_wls8_glossy", 11, 3, 11, "same",   "mirror", 4.0, 8, "wls",      "4-sided WLS (8 ch, dual), glossy"},
        {"c1_same_1p55",    1, 3,  1, "same",    "white",  4.0, 8, "control",  "C1 control, glossy"},
    };
}

// Absolute/relative path of one design's whole-plate file.
inline TString file(const char* dir, const Design& d)
{
    return TString::Format("%s/muon_whole_out_c%d_r%d_%s.root", dir, d.cfg, d.refl, d.tag);
}

inline const char* reflName(int r)
{
    switch (r) {
        case 0: return "none";
        case 1: return "TiO2";
        case 2: return "Al";
        case 3: return "glossy";
    }
    return "?";
}

// Refractive index of the coupling grease for this campaign: every design was
// run at n_coup = 1.55 (see HANDOFF.md §10), not the 1.46 default.
inline double couplingIndex(const Design&) { return 1.55; }

// Stable per-design colour for ten overlaid curves (PetroffPalette P10).
inline Color_t color(int i)
{
    if (i >= 0 && i < 10) return PetroffPalette::getP10(i);
    return (Color_t)-1;
}

// Plain-word name of a stem group, for legends and group headers.
inline const char* stemLabel(const char* key)
{
    const TString s(key);
    if (s == "position") return "ballistic (black edges)";
    if (s == "wls")      return "fibre read-out";
    if (s == "maxpe")    return "large sensor";
    if (s == "corner")   return "corner read-out";
    return "control";
}

// Colour by stem, for figures that group designs rather than enumerate them.
inline Color_t stemColor(const Design& d)
{
    const TString s(d.stem);
    if (s == "position") return PetroffPalette::getP6(0);
    if (s == "wls")      return PetroffPalette::getP6(1);
    if (s == "maxpe")    return PetroffPalette::getP6(2);
    if (s == "corner")   return PetroffPalette::getP6(4);
    return PetroffPalette::getP6(3);              // control
}

// The v7 production reference for the same physical layout (data_v7, glossy,
// n_coup = 1.46), used as the baseline in the comparison figures.
struct Reference {
    const char* what;
    double value;
    const char* note;
};
inline std::vector<Reference> v7References()
{
    return {
        {"C1 glossy whole-plate yield", 520.7, "data_v7, n_coup 1.46"},
        {"C1 glossy kappa_2D",           1.07, "data_v7"},
        {"C1 glossy sigma_x [mm]",      15.9,  "data_v7, all 24 features"},
        {"C1 TiO2 sigma_x [mm]",        11.9,  "data_v7, all 24 features"},
    };
}

// Per-event totals accumulated from the per-SiPM `sipmHits` rows.  Every event
// in which the muon entered the slab writes exactly `nch` rows (zero-detection
// rows included).  An event whose muon misses the slab -- e.g. it lands in a
// cut-away corner triangle of preset 12 -- writes NO rows at all, so the
// number of events is the number of DISTINCT evtNb values present in the tree,
// NOT max(evtNb)+1 and NOT the row count.
struct EventTotals {
    long long evt = -1;
    double    npe = 0;        // sum of nDetected over the channels
    double    nproduced = 0;  // optical photons created in the slab (per event)
    double    muonX = 0, muonY = 0;   // slab-local primary first-entry position
    int       nchan = 0;      // channels seen in this event
};

}  // namespace opt

#endif  // OPT_DESIGNS_H
