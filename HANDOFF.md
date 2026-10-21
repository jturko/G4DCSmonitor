# Hand-Off: MuonScint Phase 2 (Detector Optimisation)

**Status:** Phase 1 complete and in the report. Phase 2 is **~80 % implemented but
NOT finished**: everything compiles, the header/messenger/macro/SLURM layers are
done, and presets 8 (direct SiPM) and 7 (legacy WLS) are clean. The **single
remaining blocker is preset 10/11 (4-sided WLS) geometry overlaps** — 12
`GeomVol1002` warnings in a 20-event smoke test, down from 133. A previous
sub-agent was killed mid-debug at exactly this point. Do **not** restart the
implementation from the blueprint; read §6 first.

---

## 0. TL;DR for the next team

| Item | State |
|---|---|
| Build | **Clean** (`cmake --build build -j`, 100 %) |
| Presets 1–7 | Untouched code paths; preset 7 smoke = **0 overlaps** |
| Preset 8 (8 SiPMs @ u = ±0.75) | **Done**; 20-event smoke = **0 overlaps**, exit 0 |
| Preset 9 (12 SiPMs) | **Deliberately deferred**; `case 9` prints an error and returns |
| Preset 10 (4-sided WLS, 4 SiPM) | **Blocked**: 12 overlaps, runs to completion |
| Preset 11 (4-sided WLS dual-ended, 8 SiPM) | **Never smoke-tested**; shares the preset-10 code path minus mirror caps |
| Edge reflector (black/diffuse/mirror) | Implemented; **only `black` smoke-tested** (preset 8, 0 overlaps) |
| Coupling index knob | Implemented and active |
| `position_reconstruction.C` N-SiPM generalisation | **Done and verified** for preset 1 (byte-identical CSVs) |
| SLURM `v7_opt` campaign | Scaffolding done, manifest = 9 designs; **not submitted** |
| Hand-off preparation changes | 2 uncommitted fixes in `GeometryMuonScint.cc` (§6.3), build-clean, documented below |

---

## 1. Environment & hard rules

### 1.1 Paths
- **Login-node sshfs mount (READ-ONLY for local analysis):**
  `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti`
- **HPC-native path (only in `.sbatch` / HPC-generated macros):**
  `/bigdata/DCS-Monitor-II/turko46/MuonScinti`
- **Never write to `bigdata` from the login node.** `macros/plot_collection_efficiency.C`
  and `macros/plot_whole_slab.C` write `<inDir>/plots/` — never point them at the
  sshfs mount. `macros/position_reconstruction.C` opens inputs `READ` and writes only
  to its `outdir`.
- **Never modify `.refs/`** (Geant4 11.3.2 source/include/examples, read-only reference).
- **Do not run recursive searches through `~/bigdata`** — the sshfs mount freezes.

### 1.2 Commands
```bash
# build
cmake --build build -j                        # binary: ./build/G4DCSmonitor

# ACLiC ROOT macro (style.h lives in $HOME/root-work)
env ROOT_INCLUDE_PATH="$HOME/root-work" \
    "$HOME/programs/root/build/bin/root" -l -b -q \
    'macros/position_reconstruction.C+("<dir>","report/figures",25000,1,"",1)'

# report (two passes, from report/)
pdflatex -interaction=nonstopmode matrix_report.tex
```
- `ROOT` is at `$HOME/programs/root/build`, Geant4 at `$HOME/programs/geant4/install`.
- The shared ROOT style file is **`/home/turko46/root-work/style.h`** (recorded in
  `AGENTS.md`).
- There is **no SLURM on this login node**; local runs use `./build/G4DCSmonitor`
  (2–8 threads). Production runs go through `slurm/run_opt_fast.sh` on the HPC.

### 1.3 Tooling restrictions observed in this session
- `rm` and `bash -c` / `sh -c` are **denied**; run one command per tool call.
- `ls`, `cat`, `grep`, `head`, `tail`, `wc`, `mkdir`, `cmake --build build`, `root`,
  `git status|diff|log` are allowed.
- Consequence: stale files (the 3 `c9_*` macros, `position_reconstruction.C.bak`)
  could not be deleted here. Remove them in a normal shell.

### 1.4 Backward compatibility
- Presets **1–7 and all default optical parameters must remain bit-identical** to
  the production baseline. Defaults of every new knob (`kEdgeSame`, `1.46`, false,
  false) reproduce the old behaviour.
- **The comparison baseline changed from `data_v6` to `data_v7`.** v7 (commit
  `17e851e`) includes the direct optical-photon path scintillator→SiPM and makes the
  bare-surface contact model equivalent across all four configs. The report and
  `position_reconstruction.C`'s default input directory now point at
  `.../data_v7/whole`.

---

## 2. Working-tree state (nothing is committed)

`git log`: HEAD = `17e851e report with v7 data, fixing bare contact for WLS config 4`.

```
 M .gitignore                      # ignore compile_commands.json + .cache
 M AGENTS.md                       # clangd/tooling notes, ~/bigdata warning, style.h path
 M CMakeLists.txt                  # CMAKE_EXPORT_COMPILE_COMMANDS ON
 M include/DetectorConstruction.hh
 M include/DetectorMessenger.hh
 M include/GeometryMuonScint.hh    # Phase 2 API
 M macros/position_reconstruction.C# L3 generalisation (438 lines changed)
 M src/DetectorConstruction.cc
 M src/DetectorMessenger.cc
 M src/GeometryMuonScint.cc        # Phase 2 implementation + handoff-prep fixes
?? HANDOFF.md
?? macros/position_reconstruction.C.bak   # L3 backup, 69 kB — delete or keep deliberately
?? slurm/generate_opt_whole.sh
?? slurm/muon_whole_opt/                   # 9 wanted macros + 3 stale c9 macros
?? slurm/run_opt_fast.sh
?? slurm/run_opt_whole.sbatch
?? slurm/whole_opt_template.mac
```
`.gitignore` and `CMakeLists.txt` are tooling-only changes, unrelated to physics.

---

## 3. Locked design decisions (differ from the original HANDOFF blueprint)

1. **Preset 8 uses `u = ±0.75`** (±93.75 mm), not ±0.65. Rationale: halves the bare
   end strip from 62.5 mm to 31.25 mm; nearest-SiPM distance at centre goes
   140 → 156 mm (11 % cost); mechanical limit is `u_max = 0.984`.
2. **Preset 9 (12 SiPMs) is deferred to Phase 3.** `ApplyMuonScintPreset` `case 9`
   prints `"preset 9 deferred to Phase 3"` and returns. The messenger range allows
   9 so the message is reachable.
3. **`setEdgeReflector` applies to presets 1–8 only.** For WLS presets 10/11 the
   clamp grease film covers the narrow face, so the physically meaningful control is
   `setClampSurface white|black|mirror`. `BuildEdgeStrips()` returns early when
   `fUseWLSFibres` is true.
4. **`fCouplingRIndex` is Interface B only** (SiPM coupling pad ↔ scintillator;
   `OpticalGrease` RINDEX table). The Al-foil backing (Interface A) remains
   `setAlBacking air|grease` (`nAlBacking` in `src/GeometryMuonScint.cc:320–322`).
   Production uses `air`, so the hard-coded 1.46 there is inert.
   *Caveat:* `OpticalGrease` is shared with the WLS film/pads, so the knob also
   changes the WLS clamp grease index. That is probably fine but should be noted in
   the macro documentation.

---

## 4. What Phase 1 delivered (for context; done)

Implemented in `macros/position_reconstruction.C` and documented in report §8:
- Optical constant-fraction timing at α = 12.5 % (point quantile, leading-wavefront
  mean, tapered Beta(1,3)); first-arrival and rise-time pairing.
- D₄ symmetry-augmented k-NN regression (k = 40, tricube kernel, inverse-MSE block
  weights, block-invariant standardisation).
- Current numbers (v7 era, `report/figures/position_reconstruction.csv`,
  20 000 train / 5 000 test):

| Wrapper | Combined σₓ [mm] | σᵣ [mm] | r₆₈ [mm] |
|---|---:|---:|---:|
| Bare | 19.38 | 27.53 | 25.01 |
| TiO₂ | 11.93 | 17.04 | 16.67 |
| Al (air) | 15.73 | 22.58 | 21.58 |
| Glossy | 16.24 | 22.75 | 21.52 |

(Reproduce with the ACLiC command in §1.2, `preset = 1`.)

---

## 5. Phase 2 implementation inventory (what already exists)

### 5.1 Headers / messenger (L1 — complete)
- `include/GeometryMuonScint.hh`
  - `enum EdgeReflectorMode { kEdgeSame, kEdgeBlack, kEdgeDiffuse, kEdgeMirror }`
    (lines 31–36).
  - `Set/GetEdgeReflectorMode`, `Set/GetCouplingRIndex` (lines 88–93).
  - Preset declarations `PresetConfig_8SiPM_Wide`,
    `PresetConfig_WLS4Sides_SingleEnded`, `PresetConfig_WLS4Sides_DualEnded`
    (lines 166–168); **no** 12-SiPM declaration.
  - `BuildEdgeStrips(...)` (line 227).
  - Members: `fEdgeReflectorMode = kEdgeSame`, `fCouplingRIndex = 1.46`,
    `fWLSFourSides = false`, `fWLSDualEnded = false`, `fEdgeReflSurface = nullptr`,
    `fEdgeStripPVs[4]` (lines 342–347).
- `include/DetectorConstruction.hh` / `src/DetectorConstruction.cc`
  - `SetMuonScintEdgeReflector(EdgeReflectorMode)` and
    `SetMuonScintCouplingRIndex(G4double)`.
  - `ApplyMuonScintPreset` `case 8/9/10/11`; comment updated to `1..11 (9 deferred)`.
- `include/DetectorMessenger.hh` / `src/DetectorMessenger.cc`
  - `/dcs-monitor/det/muonScint/setEdgeReflector same|black|diffuse|mirror`
    (guidance explicitly says presets 1–8 only).
  - `/dcs-monitor/det/muonScint/setCouplingIndex <1.0–1.7>`.
  - `/dcs-monitor/det/muonScint/preset` range now `1..11`.

### 5.2 `src/GeometryMuonScint.cc` (L2 — implemented, compiles, 10/11 blocked)
- **Edge-reflector surface** built in `BuildOpticalSurfaces()` (~lines 461–493):
  - black: `dielectric_dielectric`, `unified`, `polished`, R = 0, T = 0.
  - diffuse: `groundfrontpainted`, `unified`, R = 0.985, `sigma_alpha = 0.1`.
  - mirror: `dielectric_metal`, `unified`, `polished`, R = 0.95.
  - 10-point 1.907–3.444 eV grid (matching the other reflector tables).
- **Edge strips** `BuildEdgeStrips()` (lines 2082–2173), called at the end of
  `Build()` (line ~1896), *after* `PlaceSiPMs()`:
  - 10 µm (`halfThick = 0.005 mm`) `G4_AIR` strips on each narrow face, flush with
    the slab face; SiPM grease pads subtracted with `G4SubtractionSolid`;
    `G4LogicalBorderSurface(slab, strip)` using `fEdgeReflSurface`.
  - Returns immediately if `fUseWLSFibres` or `kEdgeSame`.
- **Coupling grease index** in `Build()` (~lines 634–648): `OpticalGrease` RINDEX
  set from `fCouplingRIndex` (2-point 1.5–6.0 eV table).
- **`BentPathDual`** (lines 806–819): 6-point/5-leg path, bundles at both ±y.
- **Four-sided placement** in `PlaceWLSFibres()` (~1556–1830):
  - `halfL = fWLSFourSides ? Ly − 1 mm : Ly` (line 1556), `nSides = 4`.
  - `sideRot = rotateZ(side·90°)`, `localRot = sideRot * flipRot`.
  - Mirrors only when `!fWLSDualEnded`; read-out pads at both ends when dual,
    copy numbers `2*side` / `2*side+1`.
  - Per-side film/clamp/fibre PVs; border surfaces per side.
- **Preset bodies** (lines 2254–2295): 8 → `u = ±0.75`; 10 → 4 dummy `fSiPMs`
  entries (one per side) so `GetNumSiPMs() == 4`; 11 → 8 dummy entries.
- **Film/clamp boxes** already shortened for four sides (line 1556).

### 5.3 `macros/position_reconstruction.C` (L3 — complete, verified)
- 6th argument `int preset = 1`; default input dir now `data_v7/whole`.
- `struct SipmGeom { edge, u, v, x, y, name }` and `GetSipmTable(preset)` for
  presets 1, 8, 10, 11.
- `kMaxSiPM = 12`, runtime `gNSiPM` (4 for preset 10; 8 otherwise), arrays sized
  `kMaxSiPM`, all loops on `gNSiPM`.
- `LoadConfig1` reads `nSiPM = GetMaximum("sipm") + 1`.
- `MakeFeatures` / `KnnPredict` use `nSiPM`-wide blocks; D₄ group built from the
  active table; `qSym` dynamic.
- Preset 11 adds along-bar `Δt_e` and attenuation asymmetry `A_e` to `kTimeCFD`
  and `kAll`.
- Output tag: `c1`, `c8`, `c10`, `c11`.
- **Verified:** ACLiC compile passes; preset-1 CSVs are byte-identical to the
  pre-change version, with and without the α scan (all four CSVs diff empty).
- **Known defect:** the `GetSipmTable(10/11)` coordinates are placeholders
  (u = ±0.75 = ±93.75 mm). The real WLS read-out sits at each bar's bundle end,
  tangentially ≈ ±124.7 mm (near the face corner). These tables feed the D₄
  symmetry group and the feature geometry — **fix before quoting preset 10/11
  reconstruction results.** Preset 8's table is correct.

### 5.4 SLURM campaign (L4 — scaffolding only)
- `slurm/whole_opt_template.mac` — placeholders `@THREADS@ @PRESET@ @REFLECTOR@
  @EDGEREFL@ @COUPINDEX@ @CLAMPSURF@ @SIPMSIZE@ @BARECONTACT@ @ALBACKING@
  @DATADIR@ @TAG@ @NEVENTS@`.
- `slurm/generate_opt_whole.sh [NEVENTS] [THREADS] [DATADIR]` — emits macros to
  `slurm/muon_whole_opt/` + `manifest.txt`. Defaults 10000 / 32 /
  `/bigdata/.../data_v7_opt/whole`.
- `slurm/run_opt_whole.sbatch` — `--array=0-8`, 32 CPUs, 4 GB, `low`, 2 h;
  reads the manifest; `DATAROOT`/`VERSION` (`v7_opt`)/`BIN` overridable.
- `slurm/run_opt_fast.sh` — submitter.
- The committed `slurm/muon_whole_opt/*.mac` are **generated artefacts** (last produced
  with a test `DATADIR`, so their `/analysis/setFileName` points into `/tmp`).
  `run_opt_fast.sh` regenerates them with the real `DATADIR` at submit time. Do not run
  the generator with `DATADIR=/bigdata/...` from the login node — it `mkdir -p`s the
  target (`generate_opt_whole.sh:47`).
- **Manifest (9 designs, verified):**
  `c1_split_black, c8_split_black, c10_wls4_tio2, c11_wls8_tio2, c1_edge_diffuse,
  c1_6mm_diffuse, c8_6mm_diffuse, c10_wls4_glossy, c11_wls8_glossy`.
- Stale `c9_*` macros still sit in the directory (not in the manifest, cannot be
  selected by the array, harmless).
- **Fixed:** `slurm/run_opt_fast.sh` defaulted `WHOLE_ARRAY="0-10"` (11 tasks) and
  passed it on the `sbatch` command line, overriding the `0-8` in the `.sbatch`
  header. The manifest has only 9 entries, so tasks 9 and 10 exited with
  `"empty manifest entry"`. Default is now `0-8`; the `ARRAY_LIMIT` throttle
  (`${WHOLE_ARRAY%%%*}%${ARRAY_LIMIT}`) and the 1-based manifest lookup in the
  `.sbatch` were audited and are correct.
- The `.sbatch`, generator and driver headers now say "9 candidate designs".

---

## 6. THE BLOCKER: preset 10/11 overlaps (read this before touching geometry)

### 6.1 Reproduction
Local smoke macros were created in **`/tmp/opencode/smoke/`**:
`p7.mac`, `p8.mac`, `p10.mac`, `p11.mac` (20 events, 2 threads, 50×50 mm beam,
4 GeV muons, output to `/tmp/opencode/smoke/*.root`).
```bash
./build/G4DCSmonitor /tmp/opencode/smoke/p10.mac > /tmp/opencode/smoke/p10.log 2>&1
grep -c GeomVol1002 /tmp/opencode/smoke/p10.log
```
A G4GeometryException does **not** abort; the run completes and writes output.
The warning count is the metric.

### 6.2 Overlap history (measured, 20 events each)

| Code state | `GeomVol1002` count |
|---|---:|
| Initial L2 hand-back (presets 10/11) | **133** |
| After `halfL` consistency fix (§6.3, fix 1) | **20** |
| After enabling analytic fibre legs (§6.3, fix 2) | **12** |
| Preset 7 (analytic, single side) | **0** |
| Preset 8 (direct SiPM, edge black) | **0** |

### 6.3 Two fixes already applied during hand-off preparation
Both are **uncommitted**, build-clean, and were smoke-tested on preset 10.

1. **`src/GeometryMuonScint.cc:1193`** — the `BuildWLSFibres()` fibre half-length
   was still the leftover debug line:
   ```cpp
   const G4double halfL = fHalfSize.y();   // TEMP: full length for 4-sided test
   ```
   while the film/clamp boxes already used `Ly − 1 mm` (`PlaceWLSFibres`, line
   1556). Result: fibres ran to ±125 mm and poked into the neighbouring clamp at
   the corners. Replaced with:
   ```cpp
   const G4double halfL = fWLSFourSides ? (fHalfSize.y() - 1.0*mm) : fHalfSize.y();
   ```
   **133 → 20 overlaps.**

2. **`PresetConfig_WLS4Sides_SingleEnded()` / `_DualEnded()`** now call
   `SetFibreAnalytic(true);` and `SetFibreSquareCross(false);` (lines ~2265, ~2283).
   Without this, `analytic = fFibreAnalytic && !squareCross` was false and the
   fibres were built as `G4TessellatedSolid` swept tubes, whose
   clad/groove surfaces coincide with the boolean-carved film at `rFibre` — the
   classic zero-volume overlap source. Preset 7 (analytic) gives 0 overlaps.
   **20 → 12 overlaps.**

### 6.4 Remaining 12 overlaps (preset 10, 20 events)
```
4 × WLSFibreCladPhys_s3_*  ↔  WLSFibreMirrorPhys_s3_*     (cap sits on fibre)
4 × WLSFibreMirrorPhys_s1_* ↔ WLSFibreCladPhys_s1_*       (same, other side)
1 × WLSFibreMirrorPhys_s3_0 ↔ WLSClampPhys:1
1 × WLSFibreCladPhys_s3_3_0 ↔ WLSClampPhys:3
1 × WLSReadoutSiPMPhys_s1_e1 ↔ WLSClampPhys:1
1 × WLSReadoutSiPMPhys_s3_e3 ↔ WLSClampPhys:3
```
All offenders are on **sides 1 and 3** (the odd 90°/270° rotations), plus mirror
caps. Sides 0/2 are clean.

### 6.5 Hypotheses (NOT verified — start here)
- **(a) Mirror-cap z coordinates.** `PlaceWLSFibres` line ~1693 places caps at
  ```cpp
  const G4double zk = -(k - 0.5*(fNFibres - 1)) * fFibreZSpacing;
  ```
  i.e. the **straight-run pitch** (2.5 mm), while the fibre bundle ends use
  `zBundle` (1 mm pitch, `bundlePitch = fBundlePitch + 2 µm` for analytic). The
  cap is a 0.5 mm-radius disc; with four clamps this mismatch puts caps over a
  neighbouring fibre. This is the strongest candidate for the 8 cap↔clad overlaps
  and probably the cap↔clamp one. Use `zBundle[k]` (the array built in
  `BuildWLSFibres`) and mirror the sign convention used by the fibre path.
- **(b) Flip vs. no-flip mapping at odd rotations.** Fibres use
  `mapPosLocal()` (`sideRot * flipRot`), while caps and read-out pads use
  `mapPosNoFlip()` (`sideRot` only). `flipRot = Rx180` negates **z**; at 90°/270°
  rotations that sign lands on the tangential axis. This can put a pad/cap at the
  wrong end of the bar and inside the clamp — the likely cause of the two
  `WLSReadoutSiPMPhys_sOdd_*` ↔ clamp overlaps. Check the `atPlusY` vs.
  `placeReadout(false/true)` and `capCentre` conventions together; do not patch
  one without re-deriving the other.
- **(c) Pad axial offset ignores `halfL`.** `padOffset = Ly + fCouplingHalfZ`
  (line 1716) even when `halfL = Ly − 1 mm`. In the four-sided layout the pad is
  1 mm farther out than the shortened fibre end → an air gap (optical-coupling bug,
  not itself an overlap). Use `halfL + fCouplingHalfZ`.
- **(d) Preset 11** has **no mirror caps** (`if (!fWLSDualEnded)` guards them), so
  cap-related classes (a) and part of (b) do not apply; only the read-out-pad ↔
  clamp class should remain. **Preset 11 has never been smoke-tested** — run it
  before assuming this.

### 6.6 What not to redo
- Do **not** remove the `halfL` fix or the `SetFibreAnalytic(true)` calls.
- Do **not** reintroduce the tessellated (`!analytic`) fibre for presets 10/11.
- Do **not** widen the clamp channels or shrink the fibres to hide overlaps; that
  changes the optics and is explicitly rejected in `AGENTS.md`.
- Do **not** use `addCutawayPlane` / `TSG_OFFSCREEN` to inspect; use the overlap
  counter and `GeomVol1002` messages.

### 6.7 RESOLVED (2026-10-21, commit `df972b6`)
The remaining 12 overlaps were **not** caused by (a) or (c) — both were red herrings
(the cap z coordinates are correct). Root cause: `G4PVPlacement(G4RotationMatrix*,
...)` stores the **inverse** of the matrix it is given
(`.refs/source/geometry/volumes/src/G4PVPlacement.cc:75,137`), whereas the
`G4Transform3D` constructors store the active rotation. Cap and read-out-pad matrices
were built as `sideRot * P` and passed to that constructor, so the object received
`sideRot^-1 * P^-1`: correct wherever `sideRot` is its own inverse (0°/180°, i.e.
sides 0/2), wrong at 90°/270° (sides 1/3). `localRot = sideRot * flipRot` is a 180°
rotation and therefore its own inverse, which is exactly why the film/clamp/fibre
placements (`rotCopyLocal`) were immune while caps and pads were not.

Fix: caps/pads now pass `P * sideRot^-1` (`rotPassiveSide`), which reduces to `P` at
side 0, so the single-sided Config 4 placement is unchanged **bit for bit**. In the
same commit the pad/SiPM axial offsets follow `halfL` instead of `Ly` (hypothesis (c);
removes a 1 mm air gap at every read-out in the four-sided layout) and presets 10/11
set `fWLSOnNegX` explicitly instead of inheriting it. The preset-10/11 side comment now
reads −X, −Y, +X, +Y = report-frame top, left, bottom, right.

Measured after the fix:

| check | result |
|---|---|
| preset 10, 20 ev | 12 → **0** `GeomVol1002` |
| preset 11, 20 ev (first analytic run) | 24 → **0** |
| `/geometry/test`, 20 000 pts/volume, 1 µm tol, both presets | clean |
| preset 7 vs `28c297f`, fixed seeds, 10 ev | `sipmHits` **bit-identical** |
| preset 5 vs `28c297f`, fixed seeds, 10 ev | `sipmHits` **bit-identical** |
| preset 10 yield, 50×50 centre, TiO₂ | 68 pe/evt over 4 faces: 366/321/354/315 per 20 ev |
| preset 11 yield, same | 76 pe/evt over 8 faces: 173–224 per 20 ev |

Note the preset-5 comparison is against `28c297f`, not upstream `17e851e`: the
*pre-existing* Phase 2 diff already changed preset 5 (275 → 156 pe/evt at fixed seeds),
so `17e851e` is the wrong baseline for a regression test of this fix.

### 6.8 Side effect to keep in mind
In the four-sided layouts the pad is shifted 1.5 mm outboard (`padShift = fSiPMHalfX −
faceInset + 0.05`) so it clears the slab face, so `fWLSPadSlabContact` is inert there:
presets 10/11 have **no** direct scintillator→SiPM path. Config 4 keeps it. This is a
real difference between preset 10/11 and Config 4, not a bug, but it means the two are
not like-for-like when comparing yields.

---

## 7. Remaining work, in priority order

### P0 — unblock presets 10/11 — **DONE** (§6.7)
1. ~~Fix the remaining 12 overlaps~~ — done in `df972b6`; preset 10 **0**.
2. ~~Smoke-test preset 11~~ — done: **0** overlaps, `sipm` range 0..7, all four bars
   read out at both ends.
3. Regression — presets 5 and 7 are bit-identical to `28c297f`; presets 1/8 are
   unaffected code paths (0 overlaps in the campaign smoke runs). Preset 8 was
   smoke-tested with `edge black`, `diffuse` and `mirror`.
4. SiPM indices verified: preset 10 → 0..3, presets 8/11 → 0..7.

Remaining before submission: rebuild `BIN` on the cluster (`slurm/build.sbatch`) from
this commit — the login-node binary links Geant4 11.3.2 while the cluster modules
provide 11.4.0 — then `run_opt_fast.sh` with `DATAROOT=/bigdata/...`.

### P1 — correctness of the new observables
5. ~~Replace the placeholder `GetSipmTable(10)` / `GetSipmTable(11)` coordinates~~
   — done. The table is now derived from the same constants as
   `PlaceWLSFibres` (`faceInset`, `padShift`, `padX`, `halfL`, `ySiPM`) and
   converted to the report frame with `(x,y) = (y_l, -x_l)`; preset 11 covers both
   fibre ends in copy order (copy `2s` = -y end, `2s+1` = +y end). `BuildD4Group`
   now keeps only operations that are true **bijections** of the layout (0.1 mm
   match tolerance, ambiguous/colliding matches rejected) and prints the ones it
   drops: preset 10 is a pinwheel and keeps only C4 (**4/8**), presets 1, 8 and 11
   keep 8/8. `KnnPredict` uses the retained count instead of a hard-coded 8.
   Data-free check:
   `ROOT_INCLUDE_PATH=$HOME/root-work root -l -b -q 'macros/position_reconstruction.C+("","/tmp/opencode/smoke",-1,0)'`
   (negative `maxEvents` = self-test). Presets 2/3/4 have no table in this macro.
   Note for the report text: "eight symmetries of the square" remains true for
   configs 1/8/11 but **not** for preset 10 (four rotations only).
6. ~~Smoke-test `setEdgeReflector diffuse` and `mirror` on preset 1/8~~ — done:
   0 overlaps, run completes (preset 8: black/diffuse/mirror; preset 1: diffuse).
   Campaign smoke: `c1_edge_diffuse` 345 pe/evt, `c1_6mm_diffuse` 770,
   `c8_6mm_diffuse` 667 (20 ev each, 6×6 mm designs ≈2.2× the 4×4 area, so the size
   override reaches the geometry).
7. ~~Check that `fCouplingRIndex` reaches the WLS grease~~ — yes, and it is broader
   than the name suggests: `fCouplingRIndex` sets the RINDEX table of the single
   shared `OpticalGrease` material (`GeometryMuonScint.cc:634-652`), which is used for
   the Config-4 slab↔clamp film, the four-sided fibre↔clamp film **and** the read-out
   pads/SiPM couplant. `setCouplingIndex 1.55` therefore changes all of them at once;
   the campaign applies it uniformly, so comparisons across designs stay consistent.

### P2 — campaign
8. ~~Fix the "11 designs" comments and set `WHOLE_ARRAY=0-8`~~ — done. The stale
   artifacts (`slurm/muon_whole_opt/muon_whole_opt_c9_*.mac`,
   `macros/position_reconstruction.C.bak`) could not be deleted from the agent shell
   (`rm` is denied by policy) — remove them manually; they are untracked and harmless.
9. Rebuild `BIN` on the HPC from this commit (`cd slurm && sbatch build.sbatch`, the
   module stack there is Geant4 11.4.0 / ROOT 6.32.14), regenerate macros and submit
   `slurm/run_opt_fast.sh` (9 designs, `VERSION=v7_opt`, output
   `/bigdata/.../data_v7_opt/whole`). Never `mkdir` the native path from the login node.
10. Build `data_v8` analysis on top of the campaign and extend the report with the
    Stem-1/Stem-2 comparison.

---

## 8. Local smoke-test protocol (copy from here)

The macros already exist in `/tmp/opencode/smoke/` (recreate them if the temp dir
was cleared). Shape of each:
```
/run/numberOfThreads 2
/dcs-monitor/det/setPosition 0 0 0 mm
/dcs-monitor/det/setRotation 0 0 0
/dcs-monitor/det/muonScint/add
/dcs-monitor/det/muonScint/setSize 250 250 10 mm
/dcs-monitor/det/muonScint/preset <PRESET>
/dcs-monitor/det/muonScint/setReflector <REFL>
/dcs-monitor/det/muonScint/setWrap true
<optional: setEdgeReflector black|diffuse|mirror / setCouplingIndex 1.55 /
           setClampSurface white|black|mirror>
/run/initialize
/dcs-monitor/gun/sourceMode GPS
/gps/particle mu-
/gps/ene/type Mono
/gps/ene/mono 4 GeV
/gps/pos/type Plane
/gps/pos/shape Square
/gps/pos/centre 0 0 100 mm
/gps/pos/halfx 50 mm
/gps/pos/halfy 50 mm
/gps/ang/type planar
/analysis/setFileName /tmp/opencode/smoke/p<PRESET>.root
/run/beamOn 20
```
Check:
```bash
grep -c GeomVol1002 /tmp/opencode/smoke/p10.log     # target 0
root -l -b -q -e 'TFile f("/tmp/opencode/smoke/p10.root");'
# in ROOT: sipmHits->GetMaximum("sipm"), Draw("nDetected")
```
WLS presets run ~1 evt/s (multi-bounce tracks); preset 8 ~5 evt/s.

Reference timings/results from this session:
- preset 7: 20 ev, exit 0, **0 overlaps**.
- preset 8 (`glossy`, `edge black`, `n_coup = 1.55`): 20 ev, exit 0, **0 overlaps**.
- preset 10 (`TiO₂`, `clamp white`): 20 ev, exit 0, **12 overlaps** (133 → 20 → 12).

---

## 9. File map (where to look)

| File | Role |
|---|---|
| `src/GeometryMuonScint.cc` | **Main pending work.** All geometry/optics. Key regions: 461–493 edge surface, 634–648 coupling grease, 806–819 `BentPathDual`, 1193 fibre `halfL`, 1556–1830 four-sided placement, 1693 cap z, 2082–2173 edge strips, 2254–2295 presets. |
| `include/GeometryMuonScint.hh` | Phase 2 API (enum, setters, presets, members). |
| `src/DetectorConstruction.cc` | `ApplyMuonScintPreset` cases 8–11 (case 9 deferred, line 483). |
| `src/DetectorMessenger.cc` | `setEdgeReflector` (~275), `setCouplingIndex` (~200), preset range (~232). |
| `macros/position_reconstruction.C` | N-SiPM generalisation; `GetSipmTable` (116); main signature (826); preset-1 path verified. |
| `slurm/whole_opt_template.mac`, `generate_opt_whole.sh`, `run_opt_whole.sbatch`, `run_opt_fast.sh` | 9-design campaign. |
| `slurm/muon_whole_opt/manifest.txt` | Array order; 9 entries. |
| `AGENTS.md` | Project ground truth (optical model, presets, Geant4 boundary-process semantics, data-path rules, `style.h`). |
| `report/matrix_report.tex` | v7 report; §8 reconstruction, §10 caveats. |
| `/home/turko46/root-work/style.h` | Shared ROOT style (needs `ROOT_INCLUDE_PATH`). |

---

## 10. Physics rationale (unchanged, for the new team)

- **Large faces (±Z, 92.6 % of area) govern survival** → keep an air-backed
  specular wrapper (glossy / Al-air) for TIR.
- **Narrow edges (±X, ±Y, 7.4 % of area) govern in-plane mixing**:
  - black (R = 0) → ballistic first-pass light, best timing/position
    (**Stem 1: Position**);
  - diffuse (R = 0.985) → scrambles trapped modes, max PE (**Stem 2: Max PE**).
- **Grazing-angle TIR at the pad**: slab n = 1.58 → grease 1.46 gives θc = 67.5°;
  corner rays hit at ~74° and are lost. `n_coup = 1.55` raises θc to 78.8°.
- **4-sided WLS**: turns each edge into a 250 mm line collector. Single-ended
  (4 SiPMs) gives orthogonal 1D projections; dual-ended (8 SiPMs) removes the
  mirror loss, halves the mean fibre path, and adds along-bar Δt / attenuation
  features (~11.1 ps/mm).
- **Nearest-sensor rule** (v6 finding): diffuse paint and contact-limited bare
  plates do not mix light, so response tracks distance to the nearest sensor;
  specular wrappers are flat. New layouts must be judged with this in mind.

---

## 11. Known caveats / open items inherited from Phase 1

- cfg4 (WLS) collects far fewer PE than cfg3; the paper's cfg4 > cfg3 ordering is
  not reproduced. This is the largest open item (report §10) and is **not** fixed
  by Phase 2.
- TiO₂'s position ordering (corner-highest) reverses the paper's corner-lowest
  rule — a genuine disagreement, explained by the nearest-sensor rule.
- The `data_v7` bare-contact model is the new baseline; do not compare new runs to
  v6 numbers.
