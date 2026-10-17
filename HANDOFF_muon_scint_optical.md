# Handoff — Config 4 (WLS fibre clamp): stuck-track diagnosis, analytic fibre fix, physics work list

Written for: a coding model continuing this work (possibly a smaller one). It is meant to
be **self-contained**: you should not need to re-derive anything below. Every statement is
tagged as **[verified]** (checked in code / Geant4 source / a run), **[measured]** (number
from a run listed in §12), or **[hypothesis]** (reasoned, not proven).

Repo: `/home/turko46/geant4-sims/G4DCSmonitor`, branch `1-muon-trigger-detector`.
Session date: 2026-10-16/17. This file **replaces** the previous handoff, whose central
conclusions were wrong (see §5.4).

---

## 0. TL;DR

1. **Stuck tracks (`GeomNav1002`) are caused by the tessellated meshes**: the faceted
   fibre, film-groove and clamp-channel solids. They are **not** caused by the coincident
   cladding↔grease surface. All 17 logged stuck positions sit on a facet edge: a vertex line
   of a 24-gon (clad, core, groove or channel) or the miter ring at a kink. Chance would put
   about 1.5×10⁻³ of points within 0.1 µm of an edge. **[verified]** (§5)
2. **Fix implemented (uncommitted): preset 7.** Fibres, film grooves and clamp channels are
   built from **analytic `G4CutTubs` legs joined by exact miter planes**. Geant4's overlap
   check finds **0 overlaps** at 1 nm (10⁵ points/volume). Old preset 5 flags every fibre as
   overlapping the film. **0 stuck tracks** in 2000 plate-muon events plus 40 fibre-axial
   stress events. CPU per event is **~15 % lower**. **[measured]** Not yet statistically proven: the
   old rate is ~3×10⁻⁴/event (≈0.6 expected in 2000), so ≥10⁴ events are needed (§8 step 8).
3. The user has said the analytic approach is the way forward. **Make it the production
   geometry** (§8 step 1).
4. **The physics model of Config 4 has several real problems.** The user's goal is
   **faithful physics / faithful reproduction of Rao et al. 2025**, *not* preserving old
   numbers, so changing the optics is fine. By impact:
   - **P1** Y-11 WLS absorption is ~30× too weak in the blue: 28 mm at the 430 nm peak,
     where it should be ~1 mm (plausible range 0.3–3 mm). Digitized Kuraray spectra are in
     §6 P1. Measured effect: 37.4 → 94.3 pe/evt. **[verified + measured]**
   - **P2** The "outboard mirror" is a skin on the *whole* clamp. That makes every channel
     wall and the grease-facing front a 95 % specular mirror, so the "black clamp" is really
     a mirror box. Mirror → white: 94.3 → 74.0 pe/evt; mirror → black: ×0.4.
     **[verified + measured]**
   - **P3** Rao Table III says **multi-clad**; the model is single-clad. Kuraray: trapping 5.4 % vs 3.1 %.
   - **P4** The read-out grease pad touches the scintillator's −Y face (1.45 × 4 mm²) at the
     corner. In the bare config this is a direct, fibre-free path to the SiPM: 21.8 % of
     bare-c4 p.e. are at λ < 440 nm, which WLS emission cannot produce. **[verified + measured]**
   - **P5** The SiPM PDE table is too high in the green: 0.40 at 478 nm vs datasheet 0.32. The
     digitized datasheet curve is in §6 P5. Note: this also changes configs 1–3.
   - **P6** The WLS time profile is `"delta"`, i.e. a fixed 7 ns delay instead of exponential.
   - P7–P10 are smaller geometry/material uncertainties (§6).
5. **Nothing is committed.** Do **not** commit unless the user asks. Temporary study
   switches (environment variables) are in the code (§7.3); replace them with proper
   messenger commands.

---

## 1. Rules of engagement (non-negotiable)

- **Never modify anything under `.refs/`** (read-only Geant4 reference):
  `.refs/source` → G4 11.3.2 source, `.refs/include` → installed headers,
  `.refs/examples` → official examples. Look things up there instead of guessing.
- **Do not commit / push** unless the user explicitly asks.
- Read `AGENTS.md` (repo root). It is dense and mostly right, but **§11 of this file lists
  statements in it that are wrong** and must be corrected.
- The user prefers **short sessions**: do one coherent step, verify it, then report back with
  numbers and ask before big or irreversible decisions (deleting code, changing configs 1–3
  physics, regenerating production data).
- Prefer one shell command per tool call (permission allow-list). Avoid `cd x && y`; use the
  tool's `workdir`.
- The bash tool **kills commands after 120 s**. Run anything longer **detached** (§2.3).

---

## 2. Environment, build, run

### 2.1 Build
- Geant4 11.3.2 at `~/programs/geant4/install`, ROOT 6 at `~/programs/root/build`.
  8 CPUs, **no SLURM** on this machine.
- `cmake --build build -j 8` → binary `build/G4DCSmonitor`. Ignore `GNUmakefile`.
- Run: `./build/G4DCSmonitor <macro.mac> [fileNum]` (positional args only; no flags).

### 2.2 Standard Config-4 smoke macro (centre position, glossy paper)
Save as e.g. `/tmp/opencode/s2/mu_p7.mac`. 400 events take ~12 min on 4 threads.
```
/run/numberOfThreads 4
/dcs-monitor/det/setPosition 0 0 0 mm
/dcs-monitor/det/setRotation 0 0 0
/dcs-monitor/det/muonScint/add
/dcs-monitor/det/muonScint/setSize 250 250 10 mm
/dcs-monitor/det/muonScint/preset 7
/dcs-monitor/det/muonScint/setReflector 3
/dcs-monitor/det/muonScint/setWrap true
/run/initialize
/dcs-monitor/gun/sourceMode GPS
/gps/particle mu-
/gps/ene/type Mono
/gps/ene/mono 4 GeV
/gps/pos/type Plane
/gps/pos/shape Square
/gps/pos/halfx 25 mm
/gps/pos/halfy 25 mm
/gps/ang/type planar
/analysis/setFileName /tmp/opencode/s2/mu_p7_c4_r3_centre.root
/gps/pos/centre 0 0 100 mm
/run/beamOn 400
```
Edge = `/gps/pos/centre 100 0 100 mm`, corner = `100 100 100 mm`.

### 2.3 Running long jobs (tool timeout workaround) [verified]
`/tmp/opencode/s2/bg.sh`:
```bash
#!/bin/bash
# usage: bg.sh <macro> <log>   -- run G4DCSmonitor fully detached in /tmp/opencode/s2
cd /tmp/opencode/s2 || exit 1
setsid nohup /usr/bin/time -v /home/turko46/geant4-sims/G4DCSmonitor/build/G4DCSmonitor "$1" > "$2" 2>&1 < /dev/null &
disown
echo "started $1 -> $2 (pid $!)"
```
Start with e.g. `DCS_WLS_PEAKABS_MM=1.0 /tmp/opencode/s2/bg.sh mu_p7.mac mu_p7.log`. Poll
with `tail -c 200 <log>` (progress bar) or `sleep 240` in a single tool call (timeout
≤ 300000 ms). The run is done when the log ends with `Exit status: 0`. Plain `nohup … &`
inside the tool **gets killed** when the tool call ends; `setsid` is required.

### 2.4 Analysis macro (pe/evt and wavelength split) [verified]
`/tmp/opencode/wlsfrac.C`. Recreate it if `/tmp` was wiped:
```cpp
#include <TFile.h>
#include <TTree.h>
#include <vector>
#include <set>
#include <cstdio>
void wlsfrac(const char* fn)
{
    TFile f(fn);
    auto* t = (TTree*)f.Get("sipmHits");
    if (!t) { printf("no sipmHits\n"); return; }
    std::vector<double>* wl = nullptr;
    int evt; int nDet;
    t->SetBranchAddress("wlDetected_nm", &wl);
    t->SetBranchAddress("evtNb", &evt);
    t->SetBranchAddress("nDetected", &nDet);
    std::set<int> evs;
    long nTot = 0, nBlue = 0, nMid = 0, nGreen = 0, nSum = 0;
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
        t->GetEntry(i);
        evs.insert(evt);
        nSum += nDet;
        if (!wl) continue;
        for (double w : *wl) {
            ++nTot;
            if (w < 440.) ++nBlue; else if (w < 480.) ++nMid; else ++nGreen;
        }
    }
    printf("%s\n  events=%zu  pe/evt=%.2f  (vec entries %ld)\n", fn, evs.size(),
           double(nSum)/evs.size(), nTot);
    if (nTot) printf("  lambda<440: %.3f   440-480: %.3f   >=480: %.3f\n",
           double(nBlue)/nTot, double(nMid)/nTot, double(nGreen)/nTot);
}
```
Run: `root -l -b -q '/tmp/opencode/wlsfrac.C("file.root")'`.
- pe/evt = Σ`nDetected` / number of distinct events. **Do not divide by rows.**
- `λ < 440 nm` p.e. are **certainly not WLS-shifted**: the WLS emission tables have zero
  weight there. With the production table, 440–480 nm is a mix of shifted and unshifted light.
- `sipmHits` only gets rows for events in which the **primary muon entered the slab**.
  Muon-along-the-fibre stress runs therefore give no meaningful pe/evt; use them only to
  count stuck tracks and for timing.

### 2.5 Overlap test macro [verified]
```
/run/numberOfThreads 1
/dcs-monitor/det/setPosition 0 0 0 mm
/dcs-monitor/det/setRotation 0 0 0
/dcs-monitor/det/muonScint/add
/dcs-monitor/det/muonScint/setSize 250 250 10 mm
/dcs-monitor/det/muonScint/preset 7
/dcs-monitor/det/muonScint/setReflector 3
/dcs-monitor/det/muonScint/setWrap true
/run/initialize
/geometry/test/tolerance 1 nm
/geometry/test/resolution 100000
/geometry/test/run
```
Check: `grep -E "Overlap is detected|overlap at|G4Exception :" log` must print nothing.
Placement-time `CheckOverlaps` (`… OK!` lines) must also be clean. It takes ~1 min.

### 2.6 Counting stuck tracks
`grep -c "Stuck Track" <log>`. Positions: `grep -A6 "Stuck Track" <log>`. The classifier
script `/tmp/opencode/edgecheck.py` maps positions onto the fibre geometry (§5.2).

---

## 3. What Rao et al. 2025 say about Config 4 (the target)

Paper: `~/muon_paper.pdf` (= `report/refs/Rao2025.pdf`). Extract text with
`pdftotext -layout`. Render figures with `pdftoppm -f <page> -l <page> -r 200 -png`.

- **Sec. II A (text):** WLS fibres Y11(200) from Kuraray, Ø1 mm, cut to 250 mm. "the
  converted photons still produce electrical signals with approximately **30 % efficiency**"
  (the SiPM PDE for green light).
- **Table III:** Y-11(200): Ø1 mm, **Multi-cladding**, emission peak 476 nm, absorption peak
  430 nm, attenuation length > 3500 mm.
- **Config 4 text (p. 5):** "a single SiPM device is augmented with a WLS fiber clamp … A
  3D-printed or milled bar clamp … four fibers (Ø1.0 mm) are inserted and secured in a
  250 mm-long bar featuring channels that are approximately 1.0 mm wide. These channels are
  designed to hold the fibers securely within the 3D-printed black enclosure and to guide the
  collected optical photons toward the active surface of the SiPM [Fig. 4(b)]. **The side
  facing away from the SiPM has been covered with a reflective material.** … the gap
  between the WLS fiber clamp and the side face of the scintillator plate is **liberally
  filled with optical grease**."
- **Fig. 4(a)** (sketch, not to scale): 250 mm bar, 10 mm wide (= plate thickness), 4 fibres
  in a row, SiPM at one end; cross-section shows 4 circles in a 10 mm bar with "optical
  grease". **Fig. 4(b)** (photo): the bar looks **white / light-grey 3D-printed plastic**
  (visible print layers). The fibres converge to a tight bundle at the SiPM end.
- **Fig. 3(b) / Table IV:** a single SiPM "L1" at one end of a WLS bar along one plate edge;
  "Active area 2500 mm²" = the fibre side area (250 mm × 10 mm). Config 4 was measured
  **only with glossy paper** (reflector 3).
- **Comparability:** each config used a different transimpedance gain, so pulse heights
  (mV) **cannot be compared across configs** (the paper says so). Fig. 12 shows Config 4
  above Config 3 (glossy) at all positions, but only in mV. Fig. 13: DE ≈ 90 % at 400 mV.
  Conclusion: Config 4 has the "highest signal-to-noise ratio" and position-independent
  detection.
- **Our interpretation:** "the side facing away from the SiPM … reflective" = the **far end**
  of the bar/fibres. That is already modelled by Al mirror caps on the far fibre ends.

---

## 4. The current model (code as of this handoff)

### 4.1 Files
| What | Where (line numbers drift as you edit) |
|---|---|
| Hub: slab, optics, SiPMs, WLS clamp | `src/GeometryMuonScint.cc`, `include/GeometryMuonScint.hh` |
| Study env-var helpers `EnvDouble/EnvString` | `GeometryMuonScint.cc:41-67` (anonymous namespace) |
| Scintillator MPT | `BuildOpticalProperties` `:79` |
| Optical surfaces (wrap, coupling, SiPM PDE, holder) | `BuildOpticalSurfaces` `:212`; PDE table `:412` |
| `Build()` (calls `BuildWLSFibres` if `fUseWLSFibres`) | `:560` |
| `BentPath` (fibre centreline, 4 nodes) | `:730` |
| Legacy tessellated sweep `MakeSweptPrism`/`MakeSweptTube`/`MakeSweptSquare` | `:783`, `:837`, `:845` |
| Dead-end extruded variant `SweptSquareRegion2D`/`MakeExtrudedFibre`/`ExtrudedFibreRot` | `:857`, `:883`, `:957` |
| **New analytic builders** `CutTubeLeg`, `MakeCutTubeLegs`, `MakeCutTubeCutter` | `:897`, `:902`, `:941` |
| `BuildWLSFibres` (materials, fibres, film, clamp, caps) | `:966`; core MPT `:973-1062` (study switch `:1014`); clad `:1065`; `analytic` flag `:1153`; bundle pitch `:1158`; film `:1235`; clamp `:1289`; clamp mirror `:1336` |
| `PlaceWLSFibres` (placement + every optical border/skin) | `:1395`; clamp skin switch `:1443`; analytic leg placement `:1497`; pad `:1548`; borders `:1567-1610` |
| Presets | `PresetConfig_WLSFibreClamp` (5) `:1887`, `…Analytic` (7) `:1901`, `…Box` (6) `:1909` |
| Preset switch | `src/DetectorConstruction.cc` `ApplyMuonScintPreset` (cases 1–7) |
| Preset command (range 1..7) | `src/DetectorMessenger.cc:210-220` |
| OpWLS auto-enable, WLS time profile, `SetBoundaryInvokeSD(true)` | `src/PhysicsList.cc:153-158` |
| SiPM SD (writes `sipmHits`) | `src/MuonScintSiPMSD.cc` (`ProcessHits` `:115`, `EndOfEvent` `:187`) |
| `sipmHits` booking | `src/HistoManager.cc:131-148` |
| 10 000-step kill for optical photons | `src/SteppingAction.cc:64,169` |

### 4.2 Presets (`/dcs-monitor/det/muonScint/preset N`)
| N | Function | Meaning |
|---|---|---|
| 1 | `PresetConfig_8SiPM_AllSides` | Config 1, 8 SiPMs |
| 2 | `PresetConfig_4SiPM_OnePerSide` | Config 2, 4 SiPMs |
| 3 | `PresetConfig_2SiPM_OneSide` | Config 3, 2 SiPMs on −X |
| 4 | `PresetConfig_4SiPM_Corners` | not in the paper |
| 5 | `PresetConfig_WLSFibreClamp` | **Config 4, legacy tessellated round fibre** (production data so far) |
| 6 | `PresetConfig_WLSFibreClampBox` | diagnostic: **tessellated** square fibre (*not* analytic) |
| 7 | `PresetConfig_WLSFibreClampAnalytic` | **Config 4, analytic `G4CutTubs` fibre (new, recommended)** |

Macros that use Config 4 put `preset 5` (or `@PRESET@` = 5) in:
`macros/muon_scint_template.mac`, `macros/generate_muon_matrix.sh`, `macros/muon/scan_c4_r*.mac`,
`macros/muon_vis_wls*.mac`, `macros/muon_vis_hits_wls.mac`, `slurm/muon_scint_template*.mac`,
`slurm/generate_muon_matrix.sh`, `slurm/whole_slab_template.mac`, `slurm/generate_whole_slab.sh`,
`slurm/muon_whole/whole_c4_r*.mac`, `slurm/run_fast.sh`. If preset 5 itself becomes analytic
(recommended), none of these need editing.

### 4.3 Geometry (slab-local frame, mm) [verified]
- Slab 250 × 250 × 10: x, y ∈ [−125, 125], z ∈ [−5, 5]. n = 1.58.
- Clamp on the **−X** face (`fWLSOnNegX = true`).
- **Grease film** `WLSCoupleFilmLV` (`OpticalGrease`, n = 1.46): x ∈ [−125.1, −125.0],
  y ∈ [−125, 125], z ∈ [−5, 5] (`fClampGap = 0.1`), minus 4 grooves of radius `rFibre = 0.5`
  that follow the fibres.
- **Clamp** `WLSClampLV` (`WLSClampBlack`, n = 1.55, ABSLENGTH 1 µm): x ∈ [−127.1, −125.1]
  (`fClampWallZ = 2`), full y and z, minus 4 channels of radius `fChannelRadius = 0.55`
  (0.05 mm **air** annulus around each fibre, deliberate).
- **Fibres** (world siblings of the slab): radius 0.5, core radius 0.485
  (`fFibreCladFrac = 0.03`), axis at x = `fFibreAxisX` = −125.55. A fibre spans
  x ∈ [−126.05, −125.05], so it pokes 0.05 mm into the film (`fFibreProtrusion`): a thin
  **sliver of the cladding is wetted by grease** (coincident clad↔groove surface; physically
  correct).
- **Centreline `BentPath`** (built with the bundle at +y, then flipped): straight run at
  z = ±0.75, ±2.25 (`fFibrePitch = 1.5`) from y = −125 to y = 110; ramp to the bundle over
  `fFibreConvergeLen = 15` (y 110 → 123); bundle stub at z = ±0.5, ±1.5 (`fBundlePitch = 1.0`;
  **1.002 in the analytic build**, §5.3) from y = 123 to 125. Kink angle ≈ 3.3°.
- `PlaceWLSFibres` applies `flipRot = Rx(180°)` to the whole sub-assembly, so the **read-out
  bundle is at y = −125** and the mirror ends are at y = +125 (z signs also flip; the set of
  positions is symmetric).
- **Far-end caps:** `G4_Al` discs r = 0.5, half-length 0.05, at y = +125.05.
- **Read-out:** grease pad `fGreaseLV` (4 × 4 × 0.1, shared LV with configs 1–3) centred at
  (−125.55, −125.05, 0). It spans **x ∈ [−127.55, −123.55]**, y ∈ [−125.1, −125.0],
  z ∈ [−2, 2]. SiPM epoxy box 4 × 4 × 0.6 behind it (y ∈ [−125.7, −125.1]).
  ⚠ The pad overlaps the slab footprint in x ∈ [−125.0, −123.55] and therefore **touches
  the slab's −Y face** (1.45 × 4 mm²). It also touches the film's end face (same material).
  See P4.

### 4.4 Materials / optical tables (production values) [verified]
- Scintillator `EJ200` (stands in for UPS-92S): n = 1.58, 10 000 ph/MeV, τ = 2.1 ns,
  ABSLENGTH peak 3.8 m (2.5 m at 425 nm), emission peak 425 nm.
- Grease n = 1.46, ABSLENGTH 1 m. Clad (PMMA) n = 1.49, 20 m. Core (PS + Y-11) n = 1.59,
  ABSLENGTH 5 m flat.
- Core WLS (`:999-1010`): `WLSABSLENGTH` = 1e6 m at ≥ 500 nm; 0.60 m (480 nm), 0.20 (460),
  0.05 (440), 0.005 (420), 0.0005 (400), 0.00005 m (380). Monotonic, **no 430 nm peak**, and
  **28 mm at 430 nm** → far too weak (P1). `WLSCOMPONENT` peaks at 480 nm with weight up to
  650 nm. `WLSTIMECONSTANT` 7 ns.
- Clamp n = 1.55, ABSLENGTH 1 µm (absorbs whatever refracts into it).
- SiPM: `dielectric_metal`, `EFFICIENCY` = PDE table (0.43 peak), `REFLECTIVITY` 0.

### 4.5 Optical surfaces and how Geant4 picks them [verified]
Lookup order (`G4OpBoundaryProcess.cc:319-341`):
1. Border surface (prePV, postPV).
2. If the post volume is a daughter of the pre volume: skin of post LV, then skin of pre LV.
3. Otherwise: skin of pre LV, then skin of post LV.
4. No surface: polished `dielectric_dielectric` Fresnel/TIR from RINDEX.

If both materials are the same object → `SameMaterial`: the photon passes straight through
(`:429-434`). RINDEX is required on both sides for the default/polished branch, otherwise a
silent `NoRINDEX` kill (`:446-453`).

| Surface | Definition | Applied where |
|---|---|---|
| wrap `fReflSurface` | per reflector 0–4 | **skin** on slab LV (not for bare) |
| `fCouplingSurface` | polished dielectric_dielectric, no MPT (= pure Fresnel) | borders slab↔film; each read-out clad PV ↔ pad |
| `fSiPMSurface` | dielectric_metal, EFFICIENCY = PDE | border pad → SiPM |
| `fClampMirror` | dielectric_metal R = 0.95 | **skin on the clamp LV ⇒ ALL clamp faces** (P2) |
| `fFibreEndMirror` | dielectric_metal R = 0.90 | borders clad↔cap **and** skin on cap LV (covers core→cap) |

Implications:
- **film → clamp** and **channel air → clamp wall** both resolve to the clamp skin → mirror (P2).
- **film → pad** is `SameMaterial` → transmitted.
- **slab → pad** (bare): no border, no slab skin → Fresnel into the pad → SiPM (P4).
  With a wrap, the slab skin applies → reflected.
- **core → pad** at the read-out end: no border for (corePV, padPV) → default Fresnel, which
  is optically identical to `fCouplingSurface`.

---

## 5. Stuck tracks: diagnosis (corrected)

### 5.1 Symptom and historical rate [verified]
`*** G4Exception : GeomNav1002 … Stuck Track … not moving for 10 steps` (warning; the
navigator pushes the track 100·kCarTolerance = 100 nm and continues). Config 4 only. No
`GeomNav0003` (event abort) appears in any log.

Historical preset-5 counts:
- `slurm/logs/whole_c4_r{0,1,2,3}.local.log`: **1, 1, 7, 4** in 10 000 events each
  (100 tasks × 100 events).
- `slurm/logs/scan_c4_r{0,1,2,3}.local.log`: 0, 0, 1, 1 in ~1500 each.
- Previous session's A/B logs: 1 in ~2800.

Overall ≈ 3×10⁻⁴ per plate-muon event, highest for Al foil (7×10⁻⁴).

### 5.2 Where the stuck tracks are — all on facet edges [verified]
`/tmp/opencode/edgecheck.py` takes each logged position (slab-local, post-flip). For the
nearest fibre axis (x = −125.55; z from the straight/ramp/bundle centreline) it computes r and
θ, the distance to the nearest **24-gon vertex line** for the clad (r = 0.5), core (0.485) and
channel (0.55), and |y| distance to the kink nodes (|y| = 110, 123).

| log | volume | y | finding |
|---|---|---|---|
| whole_c4_r0 | WLSCoupleFilmPhys | 70.8 | clad vertex line, 5×10⁻⁹ mm (θ = 0°, the +x apex = groove bottom) |
| whole_c4_r1 | Film | −121.1 | clad vertex, 1.8×10⁻⁵ mm (in the ramp) |
| whole_c4_r2 | WLSFibreCladPhys_1 | 103.2 | **core** vertex, 5×10⁻⁷ mm |
| whole_c4_r2 | Film | −110.002 | miter ring at the y = −110 kink |
| whole_c4_r2 | Clad_0 | 31.3 | core vertex, 2×10⁻⁵ mm |
| whole_c4_r2 | Clad_0 | −34.6 | core vertex, 1×10⁻⁴ mm |
| whole_c4_r2 | pWorld | −109.990 | channel (r ≈ 0.546) miter ring |
| whole_c4_r2 | pWorld | −109.985 | channel miter ring |
| whole_c4_r2 | pWorld | 53.9 | clad vertex, 1.8×10⁻⁶ mm (in the air annulus) |
| whole_c4_r3 | Film | −118.6 | clad vertex, 1.8×10⁻⁵ mm (ramp) |
| whole_c4_r3 | Film | −110.000 | miter ring |
| whole_c4_r3 | pWorld | −109.995 | channel miter ring |
| whole_c4_r3 | pWorld | −109.995 | channel miter ring + channel vertex |
| scan_c4_r2 | Clad_0 | −5.7 | core vertex, 3×10⁻⁵ mm |
| scan_c4_r3 | Clad_2 | 28.0 | core vertex, 2×10⁻⁶ mm |
| prev. `clear.log` (4 µm air groove) | pWorld | 84.1 | clad vertex, 1.8×10⁻⁵ mm |
| prev. `co0.log` | WLSFibreCorePhys_3 | −85.3 | core vertex, 5×10⁻⁵ mm |

Notes:
- Miter planes are tilted by half the kink (1.65°). At r = 0.55 the "ring" therefore spans
  |y| = 110 ± 0.016, which matches all the y ≈ −109.985…−110.002 points.
- **6 of 17 are on the core's vertex lines.** The core touches nothing but its own clad, so
  no "coincident surface" theory can explain them.
- **Statistics:** a facet is 2·0.5·sin 7.5° ≈ 130 µm wide. The fraction of the surface within
  0.1 µm of an edge is ≈ 1.5×10⁻³. Finding 11 of 11 non-miter points there by chance is
  impossible.
- The previous session's square-fibre (preset 6) points also sit on square-prism edges
  (core/clad corners), a groove corner, or a miter ring (5 of 6; one unexplained).

### 5.3 Mechanism [hypothesis, consistent with the G4 source]
- At a mesh edge, two facets with different normals meet (165° dihedral for the 24-gon).
  For a grazing optical photon, `G4OpBoundaryProcess` reflects or refracts using the normal
  the navigator returns (one facet's). The new direction can still point **out through the
  adjacent facet**. The next transport step then has length 0.
- `G4OpBoundaryProcess` does nothing on steps ≤ kCarTolerance (`StepTooSmall`,
  `G4OpBoundaryProcess.cc:187-206`), so nothing changes the direction. The navigator counts
  10 zero steps and pushes (`G4Navigator.cc:987-1062`).
- `G4TessellatedSolid` `Inside`/`DistanceToIn`/`DistanceToOut` near edges are also ambiguous
  at the tolerance level. Booleans of tessellated cutters (the film and clamp are
  `G4SubtractionSolid`s of a box minus tessellated tubes) compound this.
- An analytic cylinder has no edges along its length, so this cannot happen there.

### 5.4 Why the previous handoff got it wrong [verified]
- It claimed "analytic `G4Box` square fibre ≈ tessellated round". But preset 6 builds the
  square fibre with `MakeSweptSquare` → `MakeSweptPrism` → **`G4TessellatedSolid`**. The
  `G4ExtrudedSolid` variant falls back to tessellated navigation for its non-convex outline.
  **No previous test ever used an analytic fibre**, so "shape-independent" proved nothing
  about tessellation.
- The single point "exactly on the coincident surface" (r = 0.5000000007) is also exactly on
  the clad's θ = 0° vertex line.
- The +45 % from `grooveClear = 4 µm` is real optics (a vacuum annulus air-clads the fibre
  sliver), not evidence about stuck tracks. Do not use an air groove; the grease-wetted
  contact is physical.

### 5.5 The analytic fix (preset 7) [verified]
**`MakeCutTubeLegs(name, rmax, path)`** (`GeometryMuonScint.cc:902`) makes one `G4CutTubs`
per straight segment of the `BentPath` polyline:
- Local z = segment direction. Local x = slab x: every segment lies in a plane of constant
  slab x, so `ex = (1,0,0)` ⟂ all segments and there is no twist. `ey = ez × ex`.
  `R.set(ex, ey, ez)` (columns = images of the local axes, i.e. the **object** rotation).
- Cut normals are expressed in the leg's local frame (`Rinv * …`):
  - Polyline ends: perpendicular (`lowN = −ez`, `highN = +ez`).
  - Interior nodes: **both** neighbouring legs are cut by the same bisector plane, normal
    `m = (d_in + d_out).unit()`. The low end gets −m, the high end +m. This is what makes
    the joints exact and watertight.
- Half-length = ½ |segment|; leg centre = segment midpoint (slab-local, pre-flip).

**`MakeCutTubeCutter(name, rmax, path)`** (`:941`) is the union of the legs
(`G4DisplacedSolid` + `G4UnionSolid` with `G4Transform3D`, which are **active/object**
transforms). It is used to carve the film grooves (r = `rFibre + grooveClear`, with
`grooveClear = 0`) and the clamp channels (r = 0.55). Cutters are longer than the boxes
(`cutterOv = 10 mm` overshoot), so openings are bounded by the box faces.

**Fibre volumes (analytic):** per fibre k and leg s there is a clad LV
`WLSFibreCladLV_k_s` (`G4CutTubs` r = 0.5) with a daughter core `WLSFibreCorePhys_k_s`
(`G4CutTubs` r = 0.485, **same cut planes**, so the end faces are coplanar). The data are
stored in `fFibreSegs[k][s] = {cladLV, rot, pos}`. `fFibreCladLVs`/`fFibreCoreLVs` still get
every leg LV so `IsFibreLV()` (used by `SteppingAction::IsInMuonScint`) keeps working.

**Placement** (`PlaceWLSFibres` `:1497`): `G4PVPlacement(G4Transform3D(objRot, objPos), …)`
with `objRot = flipRot * leg.rot`, `objPos = flipRot * leg.pos`. If a slab rotation exists it
is applied on top, the same way `mapPos` does. PV names are `WLSFibreCladPhys_k_s`.
`capSidePVs[k]` = leg 0 (mirror end, +y after the flip); `padSidePVs[k]` = last leg (read-out).
The mirror and coupling borders use these.

**Bundle pitch +2 µm** (`:1158`): two exactly touching cylinders are tangent. Near the end of
the ramp the tilted legs (elliptical cross-section in z: half-width r/cos α) interpenetrated
by up to 0.31 µm (`GeomVol1002`). The analytic build therefore uses a bundle pitch of
1.002 mm, which is optically still touching. Side effect: the outer fibres' end faces stick
out 3 µm past the 4 mm pad edge in z (negligible area ~2×10⁻⁴ mm²). If you want that gone,
make the pad 4.01 mm wide or the SiPM a hair larger.

**Results** [measured] (§12):
- 0 overlaps at 1 nm tolerance / 10⁵ points. Legacy preset 5 reports `GeomVol1002`
  overlaps of every fibre with the film ("by 0 fm", 45–54 cases each: facet-level
  coincidence ambiguity; `/tmp/opencode/stress/geomtest_c4_round_final.log`).
- 0 stuck tracks in 2040 analytic events (2000 plate muons + 40 fibre-axial).
- CPU 2703 s vs 3170 s for 400 events.
- In the fibre-axial stress test, wall time was 69 s vs 162 s (40 events, incl. ~same init).

**pe/evt changed +34 %** (27.8 → 37.4, same optics tables). The extra light is all
WLS-shifted. [hypothesis] A true circle conserves the skew-ray invariant, so helical rays stay
trapped; a 24-gon's flats scramble it and leak light. That would make the analytic fibre the
more faithful one. Optional cheap test: set legacy `ringSeg` (`:1121`) to 96 in preset 5. If
pe/evt moves towards 37 the faceting explanation holds. Do this **before** deleting the
legacy code, if at all.

### 5.6 The hard multi-threaded stall [hypothesis]
Seen once in production (one worker at 100 % CPU forever); never reproduced. Optical photons
are already killed after 10 000 steps (`SteppingAction.cc:169`), so a step-by-step loop cannot
run forever. The stall must be inside a **single step computation** (navigation or solid
distance). Best guess: tessellated solids / booleans with tessellated operands, which the
analytic geometry removes. If a stall ever recurs with the analytic geometry, attach
`gdb -p <pid>` → `thread apply all bt` to the spinning thread and look for
`G4TessellatedSolid`, `G4SubtractionSolid`, `G4Navigator` frames.

---

## 6. Physics problems and recommended fixes (priority order)

### P1. Y-11 WLS absorption far too weak [verified + measured] — HIGH
**Evidence.**
- At the 430 nm peak, the production table gives an absorption length of 28 mm (5 mm at
  420 nm, 50 mm at 440, 200 mm at 460). For a 200 ppm dye in a 1 mm fibre this is far too
  long.
- Spectrum-averaged single-pass absorption across a mean core chord of 0.76 mm is only
  **11.5 %** (`/tmp/opencode/wlsabs3.py`). A large part of the blue light passes through the
  fibre unshifted, or is guided along it unshifted: 10.8 % of production p.e. are at
  λ < 440 nm.

**Data: Kuraray catalogue** `report/refs/KurarayPSF.pdf`, p. 9 (PDF page 6), "Absorption and
Emission Spectra", thickest curve = Y-11. Digitized this session from 1000-dpi renders;
reading accuracy ≈ ±0.02 in relative units and ±1 nm. Absorption is normalised to 1 at
430 nm; emission to 1 at 476 nm.

Relative absorption (λ nm : rel):
```
650:0  500:0  488:0  486:0.005  483:0.015  478:0.054  474:0.11  472:0.20  470:0.31
467:0.45  465:0.59  462:0.71  459:0.80  457:0.85  453:0.88  450:0.85  448:0.81
444:0.77  442:0.79  440:0.84  437:0.90  435:0.96  430:1.00  427:0.93  423:0.84
420:0.75  415:0.64  410:0.59  405:0.51  400:0.44  396:0.37  390:0.29  386:0.24
380:0.15  370:0.075  360:0.045  350:0.03
```
Intrinsic emission (λ nm : rel):
```
650:0  640:0  620:0.005  600:0.02  590:0.03  580:0.05  570:0.085  560:0.13  550:0.19
540:0.25  531:0.31  526:0.43  520:0.55  516:0.63  512:0.71  507:0.77  503:0.76
500:0.73  495:0.69  490:0.73  487:0.80  485:0.85  482:0.92  479:0.98  476:1.00
473:0.92  471:0.82  469:0.67  466:0.50  463:0.32  461:0.21  458:0.14  456:0.09
452:0.04  449:0.01  444:0
```
These exact tables are **already coded** in the study branch (`GeometryMuonScint.cc:1014ff`,
arrays `lamA/relA`, `lamE/relE`, E = 1239.84193/λ eV). `WLSABSLENGTH = L0/rel`, and 1e6 m
where rel = 0.

**Absolute scale L0 (absorption length at 430 nm).** The catalogue gives only shapes.
Estimates:
- (a) **Dye concentration:** 200 ppm by weight, molar mass ~350–400 g/mol → c ≈ 5–6×10⁻⁴
  mol/L. Peak ε of 1–5×10⁴ L mol⁻¹ cm⁻¹ (typical coumarin-class WLS dyes) gives
  **L0 ≈ 0.15–0.8 mm**.
- (b) **Self-absorption in the Kuraray emission-vs-length spectra** (catalogue p. 8,
  L = 10/30/100/300 cm; at 10 cm the 476 nm peak is already gone and the spectrum peaks at
  498 nm). Combined with the absorption tail at 478–485 nm, this gives L0 ≈ 1.3 mm or more.
  However, the tail values (rel 0.01–0.05) are at the limit of the plot's line width, so
  this is uncertain by a factor of a few (0.4–8 mm).
- (c) The Geant4 extended example `optical/wls` uses 1 mm in the band.

**Recommendation: L0 = 1.0 mm nominal; 0.3 mm and 3 mm as systematic variations.**
Single-pass absorption with the digitized shape (chord 0.76 mm): L0 = 0.3 → 0.62;
1.0 → **0.31**; 1.5 → 0.23; 3.0 → 0.125 (≈ today's 0.115).
**Measured** (analytic geometry, mirror clamp, centre, r3): production tables 37.4 pe/evt
→ digitized Kuraray L0 = 1.0 mm **94.3 pe/evt**. Unshifted fraction (λ < 440) drops from
7.8 % to 1.0 %.

**Energy-conservation rule for `G4OpWLS`** [verified, `G4OpWLS.cc:140-170`]: the secondary
energy is resampled up to 100 times until it is ≤ the absorbed energy; otherwise the photon
is dropped. With the Kuraray tables the absorption ends at 488 nm and > 70 % of the emission
weight lies at λ > 486 nm, so the rule is satisfied. Self-absorption of the 460–480 nm
emission edge is real physics and is now modelled.

**Interpolation caveat:** Geant4 interpolates the absorption **length** linearly in energy.
Between a rel = 0 point (1e6 m) and its neighbour, L jumps; keep the small rel = 0.005 point at
486 nm so the 483–488 nm tail stays smooth.

### P2. Clamp is a mirror on every face [verified + measured] — HIGH
- `new G4LogicalSkinSurface("WLSClampMirrorSkin", fClampLV, fClampMirror)` (`:1460`) puts
  the 95 % specular `dielectric_metal` on **all** clamp faces. Per §4.5, the channel walls
  (seen from the air annulus) and the film-facing front both resolve to this skin.
- Measured (§12):
  - Old approximate-shape knob, L0 = 0.3 mm: mirror **113.6** vs black **44.9** pe/evt
    (factor 2.5).
  - Digitized Kuraray tables, L0 = 1.0 mm: mirror **94.3** vs white (R = 0.85) **74.0**.
- Paper: the photo (Fig. 4b) shows a **white/light-grey printed clamp**. The text says the
  fibres are held "within the 3D-printed black enclosure" (the plates sit in black ABS
  enclosures), and "the side facing away from the SiPM" is reflective (= far end; already
  modelled by the Al caps).
- **Recommendation:** make the clamp surface a choice
  `/dcs-monitor/det/muonScint/setClampSurface white|black|mirror`:
  - `white` (**default, pending user confirmation**): `dielectric_dielectric`,
    `groundfrontpainted`, REFLECTIVITY 0.85 (Lambertian, absorbs 15 %).
  - `black`: no skin (n = 1.55 bulk with 1 µm ABSLENGTH absorbs).
  - `mirror`: legacy.

  This logic exists in the study switch (`:1443-1461`); move it to a member + messenger
  command.
- **Ask the user which colour the real clamp was.**

### P3. Single-clad model vs multi-clad fibre — MEDIUM
**Kuraray catalogue p. 3** (PDF page 3) [verified]:
| | Single cladding | Multi-cladding (M) |
|---|---|---|
| Layers | core PS n = 1.59 / clad PMMA n = 1.49 | core PS 1.59 / inner PMMA 1.49 / outer fluorinated polymer (FP) n = 1.42 |
| Thickness | T = 2 % of D (some cases 3 %) | To = Ti = 2 % of D (some cases 3 %) |
| NA | 0.55 | 0.72 |
| Trapping efficiency (one direction) | 3.1 % | 5.4 % |

Densities: PS 1.05, PMMA 1.19, FP 1.43 g/cm³.

- The catalogue's example "Y-11(200), 0.5 mm D … → single cladding" is only an ordering
  example. Y-11(200) is sold in both versions (M suffix = multi-clad). Rao Table III says
  multi-clad, so **there is no real conflict: use multi-clad.**
- **Recommended geometry (D = 1.0 mm):** core r = **0.46**, inner clad (PMMA) to r = **0.48**,
  outer clad (FP) to r = **0.50**. The outer diameter is unchanged, so the film grooves and
  clamp channels need no change.
- **Implementation with analytic legs:** call `MakeCutTubeLegs` three times with the same
  path (r = 0.50, 0.48, 0.46). Nest per leg: outer-clad LV (placed in world) ⊃ inner-clad
  PV ⊃ core PV, all at the identity transform (same frame, same cut planes → coplanar ends).
- **Material** `WLSFiberCladOuter`: C/F/H mix (composition is irrelevant optically), density
  1.43, RINDEX 1.42, ABSLENGTH 20 m.
- **Borders/skins:** keep the cap/pad borders on the **outermost** clad PV. The cap LV skin
  already covers the inner-clad→cap and core→cap crossings. The read-out uses default Fresnel
  for inner layers (optically identical to `fCouplingSurface`).
- Add `IsFibreLV` coverage for the new LVs (push them into `fFibreCladLVs`).
- Optics note: the outer clad (1.42) is **lower** than the grease (1.46). Light guided by the
  inner/outer-clad TIR is unaffected by the grease sliver. Geant4 handles this from RINDEX.
- **Toggle:** `/dcs-monitor/det/muonScint/setFibreMultiClad true|false`, default true.

### P4. Direct (non-WLS) paths into the read-out SiPM — MEDIUM (bare), LOW (wrapped)
- **(a) Slab ↔ read-out pad contact** [verified by geometry]. The pad spans
  x ∈ [−127.55, −123.55] at y ∈ [−125.1, −125.0], so it touches the slab's −Y face over
  x ∈ [−125.0, −123.55], z ∈ [−2, 2].
  - With a wrap, the slab skin reflects there.
  - **Bare (reflector 0): no surface** → scintillation light refracts straight into the pad
    and is detected. data_v3 bare c4 centre: **21.8 %** of p.e. at λ < 440 nm, and bare (32.3)
    reads *higher* than glossy (29.0) [measured].
  - **Fix:** for `kNoReflector` only, add borders slab→pad and pad→slab with a surface that
    models the real air gap and kills transmitted light:
    ```cpp
    auto* gap = new G4OpticalSurface("MuonScintPadAirGap");
    gap->SetModel(unified);
    gap->SetType(dielectric_dielectric);
    gap->SetFinish(polishedbackpainted);  // Fresnel/TIR vs backing n=1.0003, then "paint"
    auto* m = new G4MaterialPropertiesTable();
    G4double e2[2] = {1.5*eV, 6.0*eV}, nAir[2] = {1.0003, 1.0003}, r0[2] = {0., 0.};
    m->AddProperty("RINDEX", e2, nAir, 2);
    m->AddProperty("REFLECTIVITY", e2, r0, 2);   // transmitted light absorbed
    gap->SetMaterialPropertiesTable(m);
    new G4LogicalBorderSurface("MuonScint_SlabPadGap", fScintPV, padPV, gap);
    new G4LogicalBorderSurface("MuonScint_PadSlabGap", padPV, fScintPV, gap);
    ```
    Check: bare-c4 λ < 440 nm fraction should drop to ≲ 1 %.
- **(b) Film → pad** (same grease, `SameMaterial`). With realistic absorption (§12, L0 = 0.3)
  only 0.5 % (λ < 440) + 1.0 % (440–480) of p.e. are unshifted, i.e. ≲ 2 p.e./evt with the
  mirror clamp and less with a white/black one. This is physical (the grease merges).
  **Leave it.**
- **Optional diagnostic:** record each detected photon's creator process (`OpWLS` /
  `Scintillation` / `Cerenkov`).
  - In `MuonScintSiPMSD::ProcessHits` use
    `track->GetCreatorProcess()->GetProcessName()`. Extend
    `MuonScintHit::AddDetected(time, lam, pos)` with an int code.
  - Add a `thread_local std::vector<int> gDetectedCreator` next to `gDetectedTimes`
    (`include/OpticalDiagnostics.hh`) and book it with
    `CreateNtupleIColumn("creatorDetected", gDetectedCreator)` in `HistoManager.cc:148`.
  - Fill it in both `EndOfEvent` paths.

### P5. SiPM PDE too high in the green — MEDIUM (affects configs 1–3 too → ask the user)
AdvanSiD datasheet `report/refs/AdvanSiD_NUV.pdf`, p. 5, Fig. 8 (6 V overvoltage, 20 °C),
digitized this session (±0.5 % abs.). PDE as a fraction:
```
360:0.08  375:0.15  380:0.21  385:0.28  390:0.345  395:0.38  400:0.418  410:0.431
420:0.426  430:0.408  440:0.385  450:0.360  460:0.343  470:0.330  480:0.317
490:0.300  500:0.284  510:0.266  525:0.247  550:0.224  575:0.197  600:0.178
650:0.146  700:0.110  750:0.086
```
- Production table (`:412`): 0.43 at 425 and 450 nm, 0.40 at 478, 0.35 at 506.
  - This **overestimates WLS light by ~25 %** and 440–460 nm scintillation light by ~15 %.
  - It also cuts off at 600 nm.
- Rao's "≈30 % for converted photons" matches this 6 V curve at ~490 nm.
- Rao ran at 2.5 V OV (absolute PDE lower for all configs). Keep the 6 V curve and note it as
  a caveat.
- **Fix:** replace `ep[]/pde[]` with the table above (ascending energy).
- ⚠ Changes configs 1–3 results slightly; get user approval and plan to regenerate all data.

### P6. WLS time profile — LOW (timing figures only)
`PhysicsList.cc:154` `SetWLSTimeProfile("delta")` → `G4WLSTimeGeneratorProfileDelta`
returns the constant: every re-emission is delayed by exactly 7 ns [verified]. Use
`"exponential"`. Kuraray does not state τ; 7 ns is a literature value. Affects
`persistence_map_grid.C` / `wavelength_time_grid.C` figures.

### P7. Geometry uncertainties — LOW (ask the user for the clamp CAD/measurements)
- Straight-run fibre pitch 1.5 mm (the photo suggests the 4 fibres spread over more of the
  10 mm bar, maybe ~2 mm).
- Converge length 15 mm.
- Channel Ø1.1 mm vs paper "≈1.0 mm wide".
- Fibre protrusion 0.05 mm into the grease film.

All are parameters in `GeometryMuonScint.hh:195-202`. With the analytic builder, changing
pitch/converge length is safe as long as straight-run channels do not overlap
(pitch > 2·0.55) and the bundle pitch stays ≥ 2r + 2 µm.

### P8. Bare Config 4 has no black holder — LOW
`UsesBlackHolder()` excludes WLS (the holder would overlap the clamp), so bare c4 sits in open
air. Rao measured Config 4 only with glossy paper, so this only matters if bare c4 is
reported. Fix later by cutting the clamp/film footprint out of the holder cavity, or give the
holder a larger cavity on −X.

### P9. Scintillator parameters — LOW (affects all configs; out of scope unless asked)
Rao Table I (UPS-92S): attenuation 2000 mm, decay 2.4 ns, n = 1.58, 8000–10 000 ph/MeV. The
code uses EJ-200-like values (peak ABSLENGTH 3.8 m, 2.1 ns).

### P10. Core non-WLS attenuation — LOW
5 m flat vs Table III "> 3.5 m". Over ≤ 0.25 m the difference is ~2 %. Could set 3.5 m.

---

## 7. What is in the working tree (all uncommitted)

`git status`: modified `include/GeometryMuonScint.hh`, `src/DetectorConstruction.cc`,
`src/DetectorMessenger.cc`, `src/GeometryMuonScint.cc`; untracked this file.

### 7.1 From the previous session (keep or delete, see §8 step 1)
- `MakeSweptPrism` refactor + `CircleSection`/`SquareSection` (round path byte-identical to
  HEAD).
- `grooveClear` constant (= 0).
- Square-fibre preset 6 (`fFibreSquareCross`, `sqShrink = 0.98`).
- Extruded variant (`fFibreUseExtruded`, dead end).

### 7.2 From this session
- Analytic builders `CutTubeLeg`, `MakeCutTubeLegs`, `MakeCutTubeCutter`.
- `fFibreAnalytic` flag + getters/setters, `FibreSeg` struct + `fFibreSegs`.
- Analytic branches in `BuildWLSFibres` (fibres, film grooves, clamp channels, bundle pitch
  +2 µm) and in `PlaceWLSFibres` (leg placement, `capSidePVs`/`padSidePVs` for borders).
- `PresetConfig_WLSFibreClampAnalytic` (preset 7), `DetectorConstruction` case 7, messenger
  range 1..7 + guidance.
- Includes `G4CutTubs.hh`, `G4Transform3D.hh`, `<cstdlib>`, `<string>`.

### 7.3 Temporary study switches (environment variables; remove when making things permanent)
- `DCS_WLS_PEAKABS_MM=<L0 mm>` → core uses the digitized **Kuraray absorption AND emission**
  tables (P1) with absorption length L0 at 430 nm. Unset = production tables. Prints
  `STUDY: Y-11 Kuraray absorption+emission spectra …`.
  ⚠ During this session the branch first used a cruder approximate absorption shape with
  the production emission (runs `mu_p7wls`, `mu_p7wlsblk`). It was then replaced by the
  digitized tables (runs `mu_p7k1*`). The code now contains only the digitized version.
- `DCS_CLAMP_SURFACE=mirror|black|white` → clamp skin (P2). Unset = mirror (production).

---

## 8. Recommended implementation plan (do in order; report to the user after each step)

Each step: build → overlap test (§2.5) → 400-event centre smoke with r3 (§2.2) → record
pe/evt, λ split, stuck count, CPU in a table → report back.

**Step 0 — ask the user** (or use the defaults in brackets if they say "go ahead"):
1. Clamp colour: white or black? [white, R = 0.85]
2. OK to change the SiPM PDE (affects configs 1–3)? [yes, but do it as a separate step]
3. Make preset 5 analytic and delete legacy tessellated/square/extruded code? [yes]
4. Do they have the clamp CAD / measured fibre pitch? [no → keep 1.5 mm]

**Step 1 — analytic geometry becomes production.**
- `PresetConfig_WLSFibreClamp()` sets `SetFibreAnalytic(true)`.
- Remove preset 6/7 (messenger range back to 1..5, guidance text, `DetectorConstruction`
  cases), or keep 7 as an alias. Delete `MakeSweptPrism`/`MakeSweptTube`/`MakeSweptSquare`/
  `CircleSection`/`SquareSection`/`SweptSquareRegion2D`/`MakeExtrudedFibre`/
  `ExtrudedFibreRot`, the `fFibreSquareCross`/`fFibreUseExtruded` members and the
  square-cap branch, `grooveClear` (keep the film groove at exactly `rFibre`), and the
  non-analytic branches in `BuildWLSFibres`/`PlaceWLSFibres`.
- Keep `cladPVs` logic only through `capSidePVs`/`padSidePVs`.
- Remove `#include "G4TessellatedSolid.hh"`, `G4TriangularFacet.hh`, `G4ExtrudedSolid.hh`,
  `G4TwoVector.hh` if unused.
- *Optional, before deleting:* the `ringSeg = 96` test of §5.5.
- Accept when: overlap test clean; centre r3 pe/evt ≈ 37 ± 2 (production optics tables).

**Step 2 — Kuraray Y-11 spectra (P1).**
- Make the digitized tables the only core tables.
- Add a member `fWLSAbsLength430` (default 1.0 mm) + command
  `/dcs-monitor/det/muonScint/setWLSAbsLength <value> <unit>` (pre-init, like the other
  muonScint commands in `DetectorMessenger.cc:160-228`; forward through `DetectorConstruction`
  to the last-added slab).
- The core material is created once (`G4Material::GetMaterial("WLSFiberCore", false)` guard),
  so L0 must be known before the first `Build()`. A pre-init command is fine.
- Remove the `DCS_WLS_PEAKABS_MM` switch.
- Update the comment block above the core table (it still talks about "WSL01 abort", which
  does not exist in 11.3.2).

**Step 3 — clamp surface (P2).**
- Member `fClampSurfaceMode` + command `setClampSurface white|black|mirror` (default per the
  user). Move the white surface construction into `BuildWLSFibres` next to `fClampMirror`.
- Remove `DCS_CLAMP_SURFACE`. Fix the comments that call the clamp "black" while it carries
  a mirror skin.

**Step 4 — multi-clad fibre (P3).**
- As in §6 P3. Toggle `setFibreMultiClad` (default true).
- Accept when: overlap test clean (core/inner/outer coplanar ends are fine, as core/clad are
  today); pe/evt rises (expect up to ~1.7× on guided WLS light, less in practice).

**Step 5 — direct-path fix for bare (P4a)** + optional creator diagnostic (P4).
- Accept when: bare-c4 λ < 440 fraction ≲ 1 %.

**Step 6 — WLS time profile `"exponential"` (P6).** One line in `PhysicsList.cc:154`.

**Step 7 — PDE (P5), only with user approval.** Then all configs' data need regenerating.

**Step 8 — statistical stuck-track validation** (run in the background, ~1–1.5 h each on
8 threads):
- Copy `slurm/muon_whole/whole_c4_r2.mac` (Al foil; historically **7 stuck in 10 000**).
  Set `/run/beamOn 10000` (the checked-in file says 1000) and the output path to `/tmp`, and
  run with the analytic preset.
- 0 stuck → p ≈ e⁻⁷ ≈ 10⁻³ that nothing changed. Repeat with r3 (historically 4/10 000) if
  time allows.

**Step 9 — docs.**
- Update `AGENTS.md` (§11 list) and this handoff.
- Do **not** edit `report/matrix_report.tex` unless asked. It contains the stall caveat at
  `:1500` and Config-4 numbers/WLS descriptions that will be outdated; tell the user.
- Production data (`/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v3/`) must
  be regenerated for Config 4 (and for all configs if P5 is applied). Use
  `slurm/run_local.sh`. Ask before launching multi-hour jobs.

---

## 9. Geant4 facts verified in `.refs` (save yourself the lookup)

- **Rotation conventions** [verified]:
  - `G4PVPlacement(G4RotationMatrix* pRot, G4ThreeVector tlate, …)`: `pRot` is the
    **frame (passive) rotation**; the object rotation is `pRot⁻¹`.
  - `G4PVPlacement(const G4Transform3D&, …)`: **object (active)** transform.
  - Same for booleans: `G4UnionSolid(name, A, B, rotPtr, trans)` uses a frame rotation;
    `G4UnionSolid(name, A, B, G4Transform3D)` and `G4DisplacedSolid(name, s, G4Transform3D)`
    are active.
  - `Rx(180°)` is self-inverse, which is why the legacy code's mixing never mattered.
  - ⚠ Pre-existing latent inconsistency: the slab is placed with `rot` as a pointer (object
    = rot⁻¹), while `PlaceWLSFibres::mapPos` applies `slabRot` actively. Irrelevant for
    `setRotation 0 0 0` (all production); fix if a rotated slab with Config 4 is ever needed.
- **`HepRotation::set(colX, colY, colZ)`** sets the columns (= images of the local axes).
  `rotate(angle, axis)` takes the angle first.
- **`G4CutTubs(name, rmin, rmax, halfZ, sPhi, dPhi, lowNorm, highNorm)`**: `lowNorm`/`highNorm`
  are **outward** normals of the −z/+z cut planes in the solid's local frame (lowNorm has
  z < 0, highNorm has z > 0). The cut planes pass through (0, 0, ∓halfZ).
- **`G4OpBoundaryProcess` with a non-backpainted `dielectric_dielectric` surface**
  (`:460-489`): `rand > R + T` → absorbed; `rand > R` → straight transmission; else
  Fresnel/TIR (`polished`), Lambertian (`groundfrontpainted`), plain reflection
  (`polishedfrontpainted`). Defaults with no MPT: R = 1, T = 0 → always Fresnel (this is
  `fCouplingSurface`).
- **`*backpainted`** needs the surface `RINDEX` (backing medium), otherwise a `NoRINDEX` kill
  (`:358-374`).
- **`SameMaterial`** pass-through (`:429-434`); **`StepTooSmall`** no-op (`:187-206`).
- **`G4OpWLS`** samples the emission up to 100× until E_sec ≤ E_abs, else drops the photon
  (`G4OpWLS.cc:140-170`). No `WSL01` fatal exists in 11.3.2.
- **`G4WLSTimeGeneratorProfileDelta::GenerateTime`** returns the time constant (fixed delay).
- **Navigator stuck logic** (`G4Navigator.cc:987-1062`): ≥ 10 consecutive zero steps → push by
  100·kCarTolerance (kCarTolerance = 1 nm → 100 nm) + `GeomNav1002`. At the abandon threshold
  → `GeomNav0003`, event aborted.
- **GPS default polarization is the zero vector.** If you ever shoot optical photons with GPS
  (e.g. a photon-gun stress test), set a polarization ⟂ momentum in
  `PrimaryGeneratorAction`. Otherwise `G4OpBoundaryProcess` produces NaNs.
- Geant4 example with multi-clad Y-11-like fibres: `.refs/examples/extended/optical/wls`
  (`WLSDetectorConstruction.cc:570-577`: each clad 3 % of core radius; `WLSMaterials.cc`:
  core 1.60, clad1 1.49, clad2 1.42, WLS band 1 mm).

---

## 10. Dead ends — do not repeat

1. Anything tessellated for fibres / grooves / channels. Facet edges are the stuck-track
   source (§5).
2. `G4ExtrudedSolid` fibre: the non-convex outline falls back to tessellated navigation;
   stuck rate got worse (7×10⁻³).
3. `G4Tubs` + `G4Orb` unions at kinks: HepPolyhedron fails (10 000-point cloud, 18 MB VRML).
   Mitered `G4CutTubs` needs no spheres.
4. An air/vacuum groove around the fibre (`grooveClear > 0`) to "de-coincide" surfaces: it
   changes the physics (air-clads the sliver, +45 %) and was never the stuck cause. The
   grease-wetted contact is physical; keep the groove at exactly `rFibre`.
5. Film groove at `fChannelRadius` (0.55): tangent to the film's slab-side face → degenerate
   boolean (and with tessellated cutters it hung the renderer).
6. Exactly touching analytic bundle (pitch = 2r): µm interpenetration in the ramp → keep
   +2 µm.
7. Treating the clamp skin as "outboard only": skins cover every face of an LV. Use a
   **border** surface if you ever need a single face.

---

## 11. Statements in `AGENTS.md` that are wrong / outdated (fix in Step 9)

1. *"Black holder … the `R=T=0` constants are **not** what kills it … `G4OpBoundaryProcess`
   takes the pure Fresnel/TIR branch and ignores REFLECTIVITY/TRANSMITTANCE"* → **wrong**.
   `G4OpBoundaryProcess.cc:460-466`: with R = T = 0 every photon reaching the holder skin is
   absorbed at the surface. The outcome is the same (photon dies), the mechanism is not.
2. *"black clamp … carries a `dielectric_metal` specular mirror skin on its outboard face"*
   → the skin covers **all** clamp faces (P2).
3. Config-4 / WLS section: *"Fibre solid = `G4TessellatedSolid` swept mitered cylinder"*,
   *"Tessellated booleans are fragile"* → replace with the analytic description (§5.5).
4. *"Sources conflict on cladding: the Kuraray catalogue lists Y-11(200) as single
   cladding"* → it only uses Y-11(200) single-clad in an ordering example; multi-clad exists
   (P3).
5. *"Known caveat: cfg4 collects ~17 pe/evt (centre) vs cfg3 ~164"* → outdated
   (data_v3 r3 = 29.0; analytic 37.4; with Kuraray tables see §12).
6. WLS table description (onset 480 nm, emission 480 nm peak, "≈69 % of emission weight
   below onset") → replace with the Kuraray tables (P1).
7. *"Ø1.1 + air = 15.0 pe/ev; snug Ø1.0 = 8.8"* → measured with the all-faces mirror clamp;
   re-measure after P2 if the channel clearance is revisited.
8. Stall / `GeomNav1002` text → replace with §5.
9. *"Read-out bug: all four cladPVs need the fCouplingSurface border … otherwise lossless
   TIR"* → doubtful: with no surface the default is Fresnel (identical to `fCouplingSurface`).
   Unverified history; keep the borders anyway (harmless).

---

## 12. Measurements from this session

All runs: Config 4, reflector 3 (glossy), wrap on, 4 GeV μ⁻, 50 × 50 mm beam at the
**centre**, 400 events, 4 threads, outputs in `/tmp/opencode/s2/`. CPU = user time.

| run | geometry | core WLS tables | clamp skin | pe/evt | λ<440 | 440–480 | ≥480 | CPU [s] | stuck |
|---|---|---|---|---|---|---|---|---|---|
| `mu_p5` | legacy tessellated (preset 5) | production | mirror | **27.81** | 10.8 % | 33.8 % | 55.4 % | 3170 | 0 |
| `mu_p7` | analytic (preset 7) | production | mirror | **37.38** | 7.8 % | 29.0 % | 63.2 % | 2703 | 0 |
| `mu_p7wls` | analytic | *approx.* Kuraray absorption shape, L0 = 0.3 mm, production emission | mirror | **113.64** | 0.5 % | 1.0 % | 98.5 % | 2452 | 0 |
| `mu_p7wlsblk` | analytic | same as above | black (none) | **44.94** | 0.6 % | 1.0 % | 98.5 % | 1538 | 0 |
| `mu_p7k1` | analytic | **digitized Kuraray abs + emission, L0 = 1.0 mm** | mirror | **94.32** | 1.0 % | 3.1 % | 95.9 % | 2693 | 0 |
| `mu_p7k1w` | analytic | digitized Kuraray, L0 = 1.0 mm | **white R = 0.85** | **73.98** | 1.1 % | 2.9 % | 96.0 % | 2204 | 0 |

Reading the table:
- Analytic geometry alone: ×1.34.
- Realistic Y-11 absorption at the recommended L0 = 1 mm: a further ×2.5 (37.4 → 94.3).
- White instead of mirror clamp: ×0.78 (94.3 → 74.0).
- With black (measured only with the older L0 = 0.3 approx. table): ×0.40.

The expected new nominal (analytic + Kuraray L0 = 1 mm + white clamp, before multi-clad and
the PDE fix) is therefore **≈ 74 pe/evt** at the centre, vs 27.8–29.0 in production. The
multi-clad change should raise it and the PDE fix lower it by ~20 %.

Fibre-axial stress (muon along fibre 1's core, 40 events, 4 threads, `stress_p5/p7.mac`):
0 stuck both; wall time 162 s (preset 5) vs 69 s (preset 7).

Production reference (`data_v3/matrix`, preset 5, 1000 events, centre):
| reflector | pe/evt | λ<440 | 440–480 | ≥480 |
|---|---|---|---|---|
| r0 bare | 32.33 | **21.8 %** | 38.2 % | 40.0 % |
| r1 TiO₂ | 6.43 | 9.6 % | 31.4 % | 59.1 % |
| r2 Al | 13.46 | 11.4 % | 31.9 % | 56.8 % |
| r3 glossy | 29.03 | 10.7 % | 33.3 % | 56.0 % |

Single-pass absorption (`/tmp/opencode/wlsabs3.py`): production 0.115; Kuraray shape with
L0 = 0.1/0.3/0.5/1.0/1.5/3.0 mm → 0.76/0.62/0.49/0.31/0.23/0.125.

---

## 13. Reference pointers

- Paper: `~/muon_paper.pdf`. Datasheets: `report/refs/KurarayPSF.pdf` (pp. 3, 8, 9 are the
  useful ones), `report/refs/AdvanSiD_NUV.pdf` (p. 2 table, p. 5 Fig. 8 PDE).
- Earlier audits (useful, partly superseded): `report/refs/notes/sipm_wls_audit.md`,
  `g4_boundary_process_audit.md`, `g4_optical_mpt_wls_audit.md`, `rao2025_audit.md`.
- Geant4: `.refs/source/processes/optical/src/G4OpBoundaryProcess.cc`, `G4OpWLS.cc`,
  `G4WLSTimeGeneratorProfileDelta.cc`; `.refs/source/geometry/navigation/src/G4Navigator.cc`;
  `.refs/include/G4CutTubs.hh`; `.refs/examples/extended/optical/wls/`.
- Session scratch: `/tmp/opencode/s2/` (macros, logs, ROOT files, `bg.sh`),
  `/tmp/opencode/wlsfrac.C`, `/tmp/opencode/edgecheck.py`, `/tmp/opencode/wlsabs3.py`,
  previous session `/tmp/opencode/stress/`. `/tmp` may be wiped; the essentials are
  reproduced above.

---

### One-paragraph summary
- **Diagnosis.** Config 4's stuck tracks come from facet edges of tessellated solids (the
  fibres and the film/clamp booleans built from tessellated cutters): all 17 logged positions
  sit on 24-gon vertex lines or kink miter rings. The coincident clad↔grease surface is not
  the cause, and is physically correct.
- **Fix.** Preset 7 rebuilds fibres, grooves and channels from analytic mitered `G4CutTubs`
  legs: overlap-free at 1 nm, 0 stuck in 2040 events, 15 % faster. It should become the
  production preset 5.
- **Physics, by impact.** Y-11 absorption must follow the digitized Kuraray spectra
  (L0 ≈ 1 mm at 430 nm instead of 28 mm). The clamp must stop being an all-faces mirror
  (white or black). The fibre should be multi-clad (Kuraray: 2 %+2 % of D, n 1.49/1.42). The
  bare-config pad↔slab contact must be blocked. The green PDE should follow the datasheet,
  and the WLS delay should be exponential.
- **Process.** Implement in the order of §8, verify each step with the overlap test and a
  400-event smoke run, and report to the user between steps.
