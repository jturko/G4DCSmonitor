# Config 4 / WLS (single SiPM + fibre clamp)

Read on demand from `AGENTS.md`.


**Goal / paper basis:** `~/Documents/muon_paper.pdf` (extract with `pdftotext -layout`)
specifies **four Ø1.0 mm fibres in a 250 mm bar with ~1.0 mm channels**, far side "covered
with a reflective material", one SiPM viewing all four ends. Trust the paper body over
`wls_image_description.txt` (surrogate vision: its Ø/pitch/white-holder are unverified).
Chosen: paper text + **white clamp (default; `setClampSurface white|black|mirror`) with a
far-side reflector**. Production runs use the analytic preset 7 geometry with multi-clad fibres
and the digitized Kuraray Y-11 tables.

**Geometry / coupling:** single row of fibres running the full 250 mm in **y** on the **−X**
narrow face (`fWLSOnNegX`, default true, signs all x placements). A **thin 0.10 mm grease
film** (`fClampGap`) sits between the slab face and the clamp front; each fibre pokes
`fFibreProtrusion = 0.05 mm` into it (coincident clad↔grease boundary; the grease stays
continuous against the slab). The clamp is a `G4Box` with one Ø1.1 mm
(`fChannelRadius = 0.55`) bent channel per fibre. The outer sheath (n=1.42) sits over the
core/inner-clad (n=1.59/1.49) guide across the grease (n=1.46). Fibres are **world siblings**
of the slab (not clamp daughters), so
they can protrude past the clamp mouth without tripping the mother-containment check, and
clamp/grease booleans reuse the same footprints so the volumes stay disjoint. Built in
`BuildWLSFibres()` / `PlaceWLSFibres()` (`fUseWLSFibres`); `OpWLS` auto-enabled in
`PhysicsList::ConstructProcess()` when `GeometryMuonScint::UsesWLSWorld()` (flag set in
Build; geometry precedes physics).

**Fibre solid = analytic mitered `G4CutTubs` legs** (production, preset 7), one per straight
segment of the 4-point centreline `BentPath(x, zSpread, zBundle, Ly, conv, resL, ov)`: a
straight spread run at `fFibrePitch = 2.5 mm` (axes at z = ±1.25, ±3.75 mm: equal
0.75 mm plastic margins to the top/bottom faces, 1.5 mm between neighbouring fibres), a ramp
converging over `fFibreConvergeLen = 41 mm` (39 mm ramp + 2 mm stub, preserving the old ~3.3°
outer kink) to a touching `fBundlePitch = 1.0 mm` bundle (four +y ends bunch to a 4 mm bundle
facing the single SiPM), then a short stub before +y. `MakeCutTubeLegs` builds
each leg; `PlaceWLSFibres` places the legs as world siblings joined at exact miter planes
("analytic", `fFibreAnalytic`) — no facet edges, hence no stuck tracks. Preset 5 keeps the
legacy `G4TessellatedSolid` swept cylinder (`MakeSweptTube`) **for visualisation only**.
Multi-clad nesting: core (r=0.46) ⊂ inner PMMA clad (r=0.48) ⊂ outer fluoropolymer sheath
(r=0.50) ⊂ the leg solid. **Placement subtlety:** `fFibreCladLVs` holds *every* clad (outer
and inner) for `IsFibreLV`; the per-fibre world placement must index `fFibreOuterLVs[k]`.
Indexing `fFibreCladLVs[k]` with multi-clad on places only the inner+outer of fibres 0/1, so
the VRML/OpenGL view shows just two fibres. Paths are built slab-local in a canonical **+y-bundle / −y-mirror**
frame; `PlaceWLSFibres` then applies one `Rx180` (`flipRot`) to the film/clamp/fibre placements
so the read-out lands at slab-local **−y** (report-plan **top-left**) with the mirror caps at
**+y**. Do **not** reintroduce `G4Tubs`+`G4Orb` unions (HepPolyhedron `too many edges` →
10000-point VRML cloud).

**Booleans — avoid tangency.** The grease film and clamp channels are carved with analytic
mitered `G4CutTubs` cutters (`MakeCutTubeCutter`); the film groove is **concentric to each
fibre at `rFibre`**, NOT the larger Ø1.1 channel radius: that cutter's cylinder would be
tangent to the film's slab-side face, giving a degenerate boolean that silently breaks the
topmost channel and hangs the `TSG_OFFSCREEN` renderer. **Cutter overshoot:** every cutter is
`fHalfSize.y() + 10 mm` long so each opening is bounded by the object's own faces, never by a
face coincident with the tool (coincident faces leave degenerate polygons). **Channels must not
overlap in the straight run:** `fFibrePitch = 2.5 mm` (> 2·channel radius) keeps a solid clamp
web between neighbours (Rao Fig. 4a). In the converging bundle they *do* meet —
`fBundlePitch = 1.0 mm` < 2·`fChannelRadius` (1.1 mm) — so the four channels merge into a
single slot near the bundle (the last few mm of the 39 mm ramp), by design.

**The Ø1.1 channel / Ø1.0 fibre clearance (0.05 mm air annulus) is deliberate — do NOT make
it snug.** The real Y-11(200) is multi-clad, so the model builds an **outer fluoropolymer
sheath (n=1.42)** over the **PMMA inner clad (n=1.49)** and core (n=1.59) — toggle
`/dcs-monitor/det/muonScint/setFibreMultiClad` (default on). A snug fibre touching the n=1.55
clamp would kill the low-index gap and lose the guided light. The paper specifies grease only
in the plate↔clamp gap, not around the fibres.

**Tessellated vs analytic (vis-only) and the 2026-10-21 placement fix.**
`/dcs-monitor/det/muonScint/setFibreAnalytic <bool>` (pre-init, issue it **after** `preset`)
selects analytic mitered `G4CutTubs` fibre legs and cutters (`true`, production; presets
7/10/11) or the `G4TessellatedSolid` swept-prism path (`false`; presets 5/6). The analytic
clamp/film subtraction results have **no polyhedron**, so VRML/OpenGL fall back to a point
cloud (10 000 markers per failed solid); the tessellated path draws as a solid. The switch is
ignored for square-section fibres (always tessellated) and does not change the optical model
of the analytic path. VRML exports: `vrml/gen_all_presets.sh` and
`vrml/gen_v7opt_designs.sh` (gitignored output); the plain `wls_preset10/11*.wrl` files are
tessellated, the `*_analytic.wrl` ones production geometry but clouded.
The tessellated fibre placement was **wrong until 2026-10-21**: it passed
`rotCopyLocal(baseFibreRot)` with `baseFibreRot = localRot`, and `localRot` is an involution,
so the matrix was `I` — every side's fibres stayed in the construction frame (bundle at +y
under the mirror cap, pad on the spread end) instead of being flipped and distributed. Fixed
by placing with `G4Transform3D(fibreRot, fibrePos)` (round: `localRot`; square:
`localRot*ExtrudedFibreRot()`), the analytic-branch convention. **All earlier preset 5/6
results (and the groove-clearance / round-vs-box A/B) were taken on misoriented fibres and are
invalid.** Post-fix A/B, 100 ev centre glossy 4 GeV: preset 5 = 66.1 vs preset 7 = 81.9
pe/evt — the residual gap is the tessellated square-section grooves / 24-gon, not placement.
The film/clamp placements still use `rotCopyLocal` and keep a latent non-identity-`slabRot`
bug (harmless while `setRotation` is unused): do not reorient the WLS clamp with
`setRotation` until that is fixed.

**Read-out / clamp skin:** all four fibre read-out ends carry the `fCouplingSurface` border to
the shared read-out pad (belt-and-braces: with no surface the default is Fresnel, optically
similar). Far (**+y**, after the placement flip) ends are full-disc `G4_Al` mirror caps; the
read-out (**−y**) ends meet the shared grease pad + single SiPM. The clamp LV skin covers
**every** clamp face (channel walls and grease-facing front included), not just the outboard
one: default **white Lambertian R=0.85** (`setClampSurface white|black|mirror`; `mirror` = the
legacy all-faces 95 % specular `dielectric_metal` skin, `black` = no skin).

**Far-end mirror needs a cap skin, not just clad borders.** The borders are registered only
for the `(cladPV[k], capPV[k])` pair, but guided light sits in the **core**, which is a
daughter of the clad; the `core→cap` crossing therefore had *no* surface. Since `G4_Al` has no
`RINDEX`, the default-Fresnel path killed every core photon reaching the mirror with a silent
`NoRINDEX` (measured: 17/40 events, exactly the core→cap crossings; A/B on cfg4/refl0 showed
centre 0.186→0.260 pe/evt, +40%; edge +24%, corner +19%). Fix: `new G4LogicalSkinSurface(
"WLSFibreCapMirrorSkin", fMirrorCapLV, fFibreEndMirror)` — a skin on the cap LV covers every
face not already border-ed (core→cap, cap→world); borders take precedence, so the existing
`(clad,cap)` borders are unaffected. Verified: `NoRINDEX`=0, core→cap now `SpikeReflection`.
`DielectricMetal()` uses the surface REFLECTIVITY and never needs the metal's `RINDEX`.

**WLS core tables = digitized Kuraray Y-11 (P1).** The core `WLSABSLENGTH` is the digitized
Kuraray Y-11 absorption band (finite only in the blue, ~430 nm peak) and `WLSCOMPONENT` the
digitized Y-11 emission, both fed unconditionally (no env switch). The absolute scale is set by
`fWLSAbsLength430` (L(430 nm), default **1.0 mm**; systematic band 0.3–3 mm) via
`/dcs-monitor/det/muonScint/setWLSAbsLength <value> <unit>` (pre-init). `G4OpWLS` samples the
secondary from `WLSCOMPONENT` and requires E_sec ≤ E_absorbed, so keep `WLSABSLENGTH`
effectively transparent at and below the emission onset and zero `WLSCOMPONENT` at/above that
onset (see `.refs/geant4/examples/extended/optical/wls/src/WLSMaterials.cc`). **In the v11.3.2 tree
there is no `WSL01` fatal at all** — a grep of the whole source/install trees finds no such
message, and `G4OpWLS` merely drops the photon after 100 failed samples — so this is a
data/correctness rule, not a crash expected here. Our tables satisfy it (≈69 % of the emission
weight lies below the absorption onset; failure chance ≈10⁻⁵¹), though our `WLSCOMPONENT`
extends ~0.23 eV **above** the `WLSABSLENGTH` onset (mild self-absorption), unlike
`WLSMaterials.cc`, and the emission peak (2.583 eV / 480 nm) sits **on** the absorption onset —
so do not claim "no overlap".

**WLS re-emission is exponential** (`SetWLSTimeProfile("exponential")`, τ ≈ 7 ns), not the old
`"delta"` fixed-delay profile. This only affects the timing histograms
(`persistence_map_grid.C` / `wavelength_time_grid.C`), not yields.

**Known caveat (documented, not chased):** cfg4 collects ~67 pe/evt (whole-plate mean, glossy;
the post-P1–P6 nominal: analytic + Kuraray L0=1 mm + white clamp + multi-clad + digitized
PDE) vs cfg3 ~140 pe/evt, i.e. cfg4 < cfg3, whereas paper Fig. 12 shows cfg4 > cfg3. The paper
quotes ~30 % efficiency for the WLS-converted photons, so a large loss in that path is expected
and is **not** reproduced; a quantitative light budget has never been made. This is the
largest open item of the study (report §10). (The old production
`data_v3` value was 29.0.) The paper itself warns the configs "cannot be directly compared"
(different SiPM count / gain / input capacitance), and its cfg4 value is a pulse height over a
2500 mm² fibre-augmented effective area, not raw pe. The geometry is structurally faithful.

**Figures / vis:** collection comes from the `sipmHits` tree
(`plot_collection_efficiency.C` / `plot_whole_slab.C`). `report_figures.C::fig_wavelength_cfg`
is a 2×2 per-reflector overlay of cfg1–4: c1–3 ≈ 457.0–457.4 nm for the v6 data (blue
scintillation), c4 ≈ 509.5 nm green-shifted (confirms `G4OpWLS`). Vis `macros/muon_vis_wls.mac` / `muon_vis_wls_closeup.mac`
→ `report/figures/muon_event_display_wls.{png,wrl}`; colours = scint light blue, grease
amber, fibre core/clad pale/dark green, clamp dark grey, SiPM red.

**Rendering gotchas:** use `TSG_OFFSCREEN` + `transparency true` orbit views;
`addCutawayPlane` is unreliable headless (blank frames) — avoid it. `zoomTo ~2.5` ≈ whole
plate (world 10 m).
