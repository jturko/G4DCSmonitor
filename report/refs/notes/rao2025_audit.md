# Faithfulness audit — Rao et al., J. Appl. Phys. 138, 114504 (2025)

**Reference:** G. Rao, A. Bieberle, U. Hampel, M. Wagner, *Design studies on large
muon trigger detectors using silicon photomultiplier devices*, J. Appl. Phys.
**138**, 114504 (2025). doi:10.1063/5.0288270. Text extracted with
`pdftotext -layout report/refs/Rao2025.pdf`; page numbers below are the
`138, 114504-N` folios.

**Scope / method.** Read-only audit of the simulation code and the report
(`report/matrix_report.tex`) against the paper. The paper is an **experimental**
study (pulse-height / detection-efficiency measurements with a muon coincidence
detector, MCD); our work is a **Geant4 optical-transport** re-implementation. The
comparison is therefore made at three levels: (i) geometry/material inputs,
(ii) the optical/physics model, and (iii) the qualitative and quantitative
results. Where the paper reports a quantity our simulation was never designed to
produce (detection efficiency vs. threshold, dark counts, amplifier gain), the
verdict is **ABSENT**, not MISMATCH.

Verdict key: **MATCH** (faithful), **MISMATCH** (implementation differs from the
paper or the report contradicts the code), **UNSUPPORTED** (modelling choice not
specified/justified by the paper), **ABSENT** (paper claim not represented in our
work at all).

---

## 1. Scintillator plate

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| S1 | Plate $250\times250\times10$ mm³ | Abstract p.1; Table I p.3; Sec. II A p.3 | `macros/muon_scint_template.mac:29` (`setSize 250 250 10`); default `GeometryMuonScint.hh:124` | **MATCH** | Default header is 200×200×10; the macro overrides to 250. |
| S2 | Organic scintillator, UPS-92S (AdvTECH) | Table I p.3; Sec. II A p.3 | `GeometryMuonScint.cc:539` `G4_PLASTIC_SC_VINYLTOLUENE` (PVT) | **MATCH** | Class correct; not the exact vendor material. |
| S3 | Refractive index 1.58 | Table I p.3 | `GeometryMuonScint.cc:91-94` (flat 1.58); report tab:detparams | **MATCH** | |
| S4 | Density 1.05 g/cm³ | Table I p.3 | NIST PVT base ≈1.032 g/cm³; report tab:detparams line 304 | **MISMATCH** | Acknowledged in the report table (−1.7 %). |
| S5 | Light output 8000–10000 ph/MeV | Sec. II A p.3 | `GeometryMuonScint.cc:149` 10000/MeV | **MATCH** | Upper end of quoted range. |
| S6 | Peak emission 425 nm | Table I p.3 | `GeometryMuonScint.cc:82,132`; report fig:spectra_opt | **MATCH** | |
| S7 | Light attenuation 2000 mm | Table I p.3 | `GeometryMuonScint.cc:100-115` peak 3.8 m; report tab:detparams | **MISMATCH** | ~1.9× longer than the quoted value; report acknowledges. |
| S8 | Scintillation decay time 2.4 ns | Table I p.3 | `GeometryMuonScint.cc:158` 2.1 ns | **MISMATCH** | Minor (EJ-200 value); report acknowledges. |
| S9 | All sides polished | Table I p.3; Sec. I p.2 | polished dielectric surfaces throughout | **MATCH** | |
| S10 | Emission FWHM ~80 nm | (implied by Table I) | tabulated spectrum `GeometryMuonScint.cc:123-138` | **UNSUPPORTED** | Spectrum built from EJ-200/Amcrys datasheets, not the paper. |

## 2. SiPM (ASD-NUV4S-P-40)

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| P1 | Active area 4×4 mm² | Table II p.3; Abstract p.1 | `GeometryMuonScint.hh:127-128` (4×4); `:129` thickness 0.6 mm | **MATCH** | Thickness not given by paper. |
| P2 | 9340 cells | Table II p.3 | not modelled | **ABSENT** | Irrelevant to light transport. |
| P3 | Breakdown 26 V, overvoltage 2–6 V (2.5 V), gain 10⁶ | Table II p.3; Sec. II A | not modelled | **ABSENT** | No electronics/gain in the simulation. |
| P4 | Dark count rate 50–100 kHz/mm² | Table II p.3 | not modelled | **ABSENT** | |
| P5 | Peak sensitivity 420 nm | Table II p.3 | PDE plateau 425–450 nm `GeometryMuonScint.cc:381-385` | **MATCH** | Peak is ~5 nm red-shifted; report claims 420–425. |
| P6 | Photon detection efficiency 43 % | Table II p.3 | `GeometryMuonScint.cc:384-385` peak 0.43 | **MATCH** | |
| P7 | Epoxy refractive index 1.5115 | Table II p.3 | `GeometryMuonScint.cc:580` n=1.55 | **MISMATCH** | Acknowledged in report tab:detparams line 329. |
| P8 | PDE implemented as boundary `EFFICIENCY` | (mechanism) | `GeometryMuonScint.cc:375-390` dielectric_metal + `PhysicsList.cc:158` `SetBoundaryInvokeSD(true)` | **MATCH** | Correct Geant4 idiom; verified in report Sec. physics. |
| P9 | SiPMs electrically in parallel | Abstract/Sec. I p.2 | counts summed over SiPMs in analysis | **MATCH** | |

## 3. Detector configurations (T/B/L/R = top/bottom/left/right)

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| C1 | Config 1 = 8 SiPMs, TBLR1/TBLR2 (2 per side) | Fig. 3a; Table IV p.5 | `GeometryMuonScint.cc:1485-1493` (2/side at u=±0.5) | **MATCH** | Equal-area along-side offsets. |
| C2 | Config 2 = 4 SiPMs, TBLR1 (1 per side) | Table IV p.5 | `GeometryMuonScint.cc:1495-1506` (same ±0.5 offsets) | **MATCH** | Paper does not give positions; our choice is a reasonable reading. |
| C3 | Config 3 = 2 SiPMs, **T1, T2** (top face) | Table IV p.5 | `GeometryMuonScint.cc:1508-1514` — both on **−X** (edge 1) | **MATCH**/**MISMATCH** | See C5: in the report frame −X maps to “top”, so config 3 matches T; the mismatch is relative to config 4. |
| C4 | Config 4 = 1 SiPM, **L1** (left face) + WLS clamp | Table IV p.5; Fig. 3b, 4 | `GeometryMuonScint.cc:1524-1534`, `fWLSOnNegX=true` (:1532) — **−X** face | **MISMATCH** | Code places config 4 on the *same* narrow face as config 3; paper puts it on an **adjacent** face (L vs T). |
| C5 | Relative orientation config 3 vs config 4 | Table IV p.5 | both edges 1 / `fWLSOnNegX`, report lines 147-148 & 242 both “top (+y)” | **MISMATCH** | Consequence of C3+C4: the paper’s T and L are orthogonal; ours are collocated. |
| C6 | Config 4 active collection area 2500 mm² | Table IV footnote p.5 | clamp spans full 10 mm × 250 mm (`GeometryMuonScint.cc:990-991`); 4 fibres occupy ~4 mm | **UNSUPPORTED** | Fibre-only effective area is ~1000 mm²; 2500 mm² is the whole narrow face. |
| C7 | Black 3D-printed ABS enclosures with SiPM notches | Sec. II B p.4; III A p.7 | `GeometryMuonScint.cc:632-668` C:H, n=1.55, ABSLENGTH 1 µm; no notches | **MISMATCH**/ABSENT | Not an accurate ABS composition; no SiPM/electronics notches modelled. |
| C8 | Config 4 assembled with glossy paper only | Sec. III D p.8 | matrix/whole runs config 4 with **all four** reflectors | **UNSUPPORTED** | Extension beyond the paper (all other config-4 reflector cells are hypothetical). |

## 4. Reflector treatments and optical model

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| R1 | Reflectors: none / TiO₂ paint / Al foil / glossy paper | Sec. II B p.4; Fig. 2 | `GeometryMuonScint.hh:21-27` enum; `GeometryMuonScint.cc:210-355` | **MATCH** | |
| R2 | TiO₂ = EJ-510 white paint, diffuse | Sec. II B p.4 | `groundfrontpainted`, Lambertian, R≈0.955–0.983 (`:246-263`) | **MATCH** | Paint thickness (0.11 mm) not modelled. |
| R3 | Al = household foil (shiny side), wrapped with grease | Sec. II B p.4 | `polishedbackpainted`, RINDEX backing=1.46, R=0.904–0.925, specular spike=1 (`:280-300`) | **MATCH** | |
| R4 | Glossy paper = 300 gsm resin photo paper, no grease | Sec. II B p.4 | `polishedbackpainted`, air-gap RINDEX=1.0003, R=0.900–0.955 (`:308-328`) | **MATCH** | |
| R5 | Bare plate sits in a **black holder** | Sec. III A p.7; Sec. II B p.4 | holder shell + `fHolderSurface` absorber skin on the **holder LV** (`GeometryMuonScint.cc:632-668`, `:1283-1291`) | **MATCH** | |
| R6 | Report: bare loss is “effective optical contact / frustrated TIR”, “perfect-absorber skin **on the plate** (R=T=0)” | report latex lines 207-216 | code applies the absorber to the **holder LV**, not the slab; the plate keeps polished scint↔air Fresnel/TIR across a real 1.2 mm air gap (`:1275` no skin on slab) | **MISMATCH** | Report contradicts the code (and `AGENTS.md`, which agrees with the code). The stale comment at `GeometryMuonScint.cc:626-629` repeats the same error. This is the report’s stated mechanism for its headline result. |
| R7 | Report Table: Al R ≈ 0.84–0.91 | report tab:surfaces line 201 | code R = 0.904–0.925 (`GeometryMuonScint.cc:286-287`) | **MISMATCH** | Report table is stale relative to the code. |
| R8 | Al and glossy nearly identical and brightest | Sec. III C p.7 | report tab:eff/tab:positions: Al ≈ glossy highest | **MATCH** | |
| R9 | Bare plate gives lowest yield | Sec. III C p.7 | report → bare lowest | **MATCH** | |
| R10 | Ordering none < TiO₂ < Al ~ glossy | Fig. 10–11 p.8; Abstract p.1 | report abstract line 55; tab:eff | **MATCH** | Paper does not print an explicit TiO₂<Al ranking, but Fig. 10/11 imply it. |
| R11 | Configs “cannot be directly compared” (different amplifier/gain/capacitance) | Sec. II B p.5 | comparison is in raw photoelectrons (`sipmHits`), no caveat in report | **UNSUPPORTED** | Relevant when our report ranks configs 1–4 against each other. |

## 5. WLS fibre clamp (config 4)

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| W1 | Fibre Ø 1.0 mm | Table III p.3; Sec. II A | `GeometryMuonScint.hh:166` `fFibreRadius=0.5 mm` | **MATCH** | |
| W2 | Multi-cladding | Table III p.3 | single PMMA clad n=1.49 (`:167`, `:838-854`) | **MISMATCH** | Acknowledged; changes TIR margin. |
| W3 | Y-11 emission peak 476 nm | Table III p.3 | `WLSCOMPONENT` peak ≈480 nm (`:825-828`) | **MATCH** | ~4 nm; report says 476–480. |
| W4 | Y-11 absorption peak 430 nm | Table III p.3 | `WLSABSLENGTH` shortest near 420–440 nm (`:817-821`) | **MATCH** | Slight blue shift of the onset. |
| W5 | Attenuation length >3500 mm | Table III p.3 | core bulk loss 5 m, re-emission guided by TIR (`:806-808`) | **MATCH** | Qualitative. |
| W6 | Fibres cut to 250 mm | Sec. II A p.3 | run the full plate length (`:865`) | **MATCH** | |
| W7 | Four fibres in the clamp | Sec. II B p.5 | `fNFibres=4` (`:165`) | **MATCH** | |
| W8 | Channels ≈1.0 mm wide | Sec. II B p.5 | channel Ø1.1 mm (`:169`) with 0.05 mm air annulus | **MISMATCH** | Deliberate 0.1 mm enlargement; documented in report §4. |
| W9 | Far side covered with reflective material | Sec. II B p.5 | far-end Al mirror caps + clamp outboard mirror skin (`:1027-1068`, `:1216-1225`) | **MATCH** | |
| W10 | Gap between clamp and plate filled with optical grease | Sec. II B p.5 | 0.10 mm grease film (`:168`, `:925-967`) | **MATCH** | |
| W11 | Single SiPM views all four fibre ends | Sec. II B p.5; Fig. 4 | read-out grease pad + single SiPM (`:1180-1198`) | **MATCH** | |
| W12 | 1.5 mm straight pitch → 1.0 mm bundle | (not in paper) | `fFibrePitch=1.5`, `fBundlePitch=1.0` (`:170-171`) | **UNSUPPORTED** | Invented to keep a clamp web; paper gives no pitch. |
| W13 | Clamp bar 250 mm long, one row | Sec. II B p.5 | `G4Box(clampHalfX, fHalfSize.y(), fHalfSize.z())` (`:990-991`) | **MATCH** | |

## 6. Beam, irradiation regions, source

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| B1 | ROI active area 50×50 mm² (MCD) | Abstract p.1; Sec. II C p.6 | `macros/muon_scint_template.mac:49-51` halfx/y=25 mm | **MATCH** | |
| B2 | Regions: centre / edge / corner | Sec. II C p.6; Fig. 10 | macro:55 (0,0), :63 (100,0), :68 (100,100) sim frame | **MATCH** | Exact coordinates are our choice; equal-area, fully on plate. |
| B3 | Vertical muon beam | **not stated** — paper uses cosmic muons + MCD | GPS `planar`, default momentum (0,0,−1) (`macros/muon_scint_template.mac:43-51`; G4 default confirmed in `G4SPSAngDistribution.cc:42`) | **UNSUPPORTED** | Reasonable idealisation of an MCD-selected near-vertical muon; not in the paper. |
| B4 | 4 GeV muon energy | Intro p.1 quotes ~4 GeV *mean sea-level* energy | `/gps/ene/mono 4 GeV` (macro:46) | **UNSUPPORTED** | Paper never specifies a simulated energy. |
| B5 | Edge patch on the instrumented-face side | (implied by ROI choice) | edge/corner placed opposite the single instrumented face | **UNSUPPORTED** | Modelling choice; report justifies from Fig. 3. |

## 7. Results / quantitative claims

| # | Claim | Paper location | Our implementation | Verdict | Notes |
|---|-------|----------------|--------------------|---------|-------|
| Q1 | **Config 4 is the most efficient** collection | Abstract p.1; Conclusion p.11 | report tab:eff & abstract: config 4 is the **lowest** (e.g. 1.5e-3 vs config 3 8.2e-3) | **MISMATCH** | Central contradiction; report/AGENTS acknowledge it. |
| Q2 | Config 4 > Config 3 (pulse height) | Sec. III D p.8, Fig. 12 | report: config 4 < config 3 in every reflector | **MISMATCH** | Same as Q1, position-resolved. |
| Q3 | Corner gives the lowest yield **for all configs** | Sec. III C p.7 | report tab:positions: for config 1, **corner is highest** for bare (2.8e-4) and TiO₂ (1.27e-2); centre highest for Al/glossy | **MISMATCH** | Report comment (§caveats) flags only the TiO₂ reversal; the bare reversal is also contrary to the paper. |
| Q4 | Reflective wrappers homogenise the response | Sec. III C p.7 | report: Al/glossy flat to <4 % / κ₂D≈1.06–1.5 | **MATCH** | |
| Q5 | Config 4 detection efficiency up to 90 % at 400 mV | Sec. III D p.8; Conclusion | not modelled | **ABSENT** | Needs amplifier/threshold model. |
| Q6 | Config 1–3 DE ≥80 % at 400 mV with reflector | Sec. III C p.7 | not modelled | **ABSENT** | |
| Q7 | Config 4 DE >99 % at 600 mV | Sec. III E p.10, Fig. 15 | not modelled | **ABSENT** | |
| Q8 | Threshold 400 mV / 10-photon discriminant / dark-count suppression | Sec. II B, III B pp.4,7 | not modelled | **ABSENT** | |
| Q9 | Config 4 position-independent DE up to 90 % | Conclusion p.11 | report: config 4 shows the **largest** position contrast (κ₂D=138) | **MISMATCH** | Opposite of the paper’s “position-independent”. |
| Q10 | “Single SiPM + WLS = cost-effective, modular, mass-producible” | Abstract; Conclusion | design conclusion, not a simulation result | **UNSUPPORTED** | Our study does not evaluate manufacturability. |

---

## Paper claims NOT represented in our code

- Every **electrical / DAQ** result: detection-efficiency vs. threshold curves
  (Figs. 11, 13, 15), the 400 mV and 600 mV operating points, the >99 % DE of
  config 4, dark-count spectra/rates (Fig. 9), the LTC6268-10 transimpedance
  amplifier and CAEN VX2740 readout, gain/capacitance/overvoltage.
- The **10-photon dark-count discriminant** and dark-count suppression analysis.
- The paper’s headline claim that **config 4 (single SiPM + WLS) is the most
  efficient** — our simulation finds the opposite (Q1/Q2/Q9).
- **Pulse-height spectra** (mV); the report substitutes photoelectrons.
- SiPM **9340 cells**, breakdown voltage, cell capacitance.
- The **multi-cladding** of the Kuraray Y-11(200) fibre.
- The paper’s **corner-lowest** position trend (Q3).
- The explicit caveat that the four configs “cannot be directly compared”.

## Our features NOT present in the paper

- Full-plate **whole-slab uniform irradiation**, normalised efficiency maps and
  the `κ₂D` two-dimensional contrast / polar `ε(r,φ)` analysis.
- `opticalStats` per-event photon budget, per-slab track-length flux maps, and
  the `sipmHits` per-SiPM scheme.
- Config 4 evaluated with **bare / TiO₂ / Al** wrappers (paper only does glossy).
- Config 2 SiPM positions pinned at ±0.5 (paper gives none).
- Config 4 **single-clad** n=1.49 fibre, Ø1.1 mm channels with a 0.05 mm air
  annulus, 1.5 mm straight pitch, 15 mm convergence, 0.05 mm protrusion.
- Scintillator **Cherenkov** production, Birks 0.126 mm/MeV, 0.9 ns rise time,
  custom emission/absorption/refractive-index tables from EJ-200/UPS-923A
  datasheets.
- `kTeflon` reflector and `PresetConfig_4SiPM_Corners` (unused in the paper’s
  matrix).
- G4 `G4EmStandardPhysics_option4` / QGSP_BIC_HP hadronic physics.

---

## Top discrepancies (prioritised)

1. **Config 4 is the worst, not the best collector (Q1/Q2/Q9).**
   Paper abstract p.1 & Conclusion p.11 single out config 4 as the most
   efficient, position-independent design (DE up to 90 %); our report
   (`figures/fig_eff_bars`, tab:eff line 669) and `GeometryMuonScint.cc:1524-1534`
   give config 4 the lowest efficiency and the largest position contrast
   (κ₂D=138, report §wholeunif). This is the single largest result-level
   disagreement.

2. **Report’s bare-plate mechanism contradicts the code (R6).**
   `report/matrix_report.tex:207-216` claims “effective optical contact”,
   “frustrated TIR” and a “perfect-absorber skin on the plate (R=T=0)”. The code
   puts the absorber on the **holder LV** (`GeometryMuonScint.cc:1287-1291`) and
   leaves the plate as polished scint↔air across a real 1.2 mm air gap
   (`GeometryMuonScint.cc:1275`), exactly as `AGENTS.md` describes. The stale
   comment at `GeometryMuonScint.cc:626-629` repeats the error. The headline
   “bare is suppressed by frustrated TIR” narrative is therefore mis-described.

3. **Config 3 and config 4 share the same narrow face (C4/C5).**
   Paper Table IV p.5: config 3 = **T1,T2** (top) and config 4 = **L1** (left) —
   adjacent faces. Code: config 3 on edge 1 (−X, `GeometryMuonScint.cc:1508-1514`)
   and config 4 also on −X (`fWLSOnNegX=true`, `:1532`). The report even labels
   both as the top (+y) edge (report lines 147-148, 242).

4. **Centre/edge/corner trend reverses the paper for bare and TiO₂ (Q3).**
   Paper Sec. III C p.7: corner is lowest for **all** configurations. Report
   tab:positions: corner is **highest** for bare and TiO₂ at config 1
   (2.8e-4 and 1.27e-2). The report acknowledges only the TiO₂ reversal; the
   bare reversal and the `AGENTS.md` claim of “centre > edge > corner for all
   configs 1–4” are inconsistent with the report’s own numbers.

5. **Material/geometry parameter deviations (S4, S7, S8, P7, W2, W8, R7).**
   Density 1.032 vs 1.05 g/cm³; attenuation 3.8 m vs 2 m; decay 2.1 vs 2.4 ns;
   SiPM epoxy n=1.55 vs 1.5115; single vs multi-clad fibre; Ø1.1 vs ~1.0 mm
   channels; and report tab:surfaces quotes Al R=0.84–0.91 while the code uses
   0.904–0.925. Individually small, collectively a systematic drift; most are
   acknowledged in the report’s parameter table.

---

## Verdict counts

Counting the **66** tabulated claims (S1–S10, P1–P9, C1–C8, R1–R11, W1–W13,
B1–B5, Q1–Q10):

- **MATCH: 35** — S1, S2, S3, S5, S6, S9; P1, P5, P6, P8, P9; C1, C2, C3;
  R1, R2, R3, R4, R5, R8, R9, R10; W1, W3, W4, W5, W6, W7, W9, W10, W11, W13;
  B1, B2; Q4. *C3 matches the paper’s T-face only in the report frame; see C5.*
- **MISMATCH: 15** — S4, S7, S8; P7; C4, C5, C7; R6, R7; W2, W8; Q1, Q2, Q3,
  Q9. *(Q1–Q3, Q9 are result-level; C7 straddles MISMATCH/ABSENT.)*
- **UNSUPPORTED: 9** — S10; C6, C8; R11; W12; B3, B4, B5; Q10.
- **ABSENT: 7** — P2, P3, P4; Q5, Q6, Q7, Q8. (The holder-notches sub-point of
  C7 is likewise absent, but C7 is counted once, under MISMATCH.)

## Overall assessment

The **geometry skeleton** — plate dimensions/material class, SiPM footprint and
PDE, the four reflector treatments, the WLS-clamp concept, the 50×50 mm² MCD
patch size, and the qualitative ordering `none ≪ TiO₂ < Al ≈ glossy` — is
faithfully reproduced. The **divergences concentrate in three places**: (i) the
**result-level ranking** of config 4 (worst here vs. best in the paper), (ii) the
**bare-holder optical mechanism** as described by the report versus what the code
actually builds, and (iii) **fine material/fibre parameters** plus the placement
of configs 3 and 4 on the same face. All the paper’s **electrical/DAQ** results
(thresholds, dark counts, detection efficiencies) are outside the scope of this
optical-only simulation and are absent by design.
