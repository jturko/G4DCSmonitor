# SiPM photodetection & WLS-fibre model — datasheet faithfulness audit

Read-only audit. Repo: `/home/turko46/geant4-sims/G4DCSmonitor`.
Date: 2026-10-15.

## Sources and extraction fidelity

| Source | What was extractable |
|---|---|
| `report/refs/AdvanSiD_NUV.pdf` (AdvanSiD Cefla, *NUV SiPMs*, Rev. 9, 5.2015) | **Full text** via `pdftotext -layout`. Numeric spec table on p. 2 is complete. The PDE(λ) curve is a raster figure (Fig. 8, p. 5 of the PDF; `pdfimages -list` confirms image objects) and has no text layer. |
| `report/refs/KurarayPSF.pdf` (Kuraray *Plastic Scintillating Fibers* catalogue) | **Only the intro/index page has a text layer** (PDF p. 2). PDF pp. 3–6 (the WLS fibre section, catalogue pp. P7–9) contain **no text layer at all** — the spectra/attenuation plots are vector outlines (page resources have no `/Font`) and the rest are raster images. No OCR engine (tesseract/easyocr/paddleocr) is installed, so **the Kuraray numeric absorption/emission/attenuation values could not be read from the datasheet directly.** |

Consequence for the WLS rows below: the datasheet establishes the *identity* of the part
(`Y-11(200)`, round, **single cladding**, 200 ppm WLS dye, available at Ø1.0 mm,
catalogue p. P1–2 / PDF p. 2) but its quantitative spectra are figures. Where a number is
needed, the audit cites **Rao2025 Table III** (the paper the report itself uses for these
numbers) and labels the Kuraray figure as the primary-but-unreadable location.

### Cross-check of the disputed Y-11 numbers
`report/refs/Rao2025.pdf`, Table III (via `pdftotext -layout`, lines ~238–247):
`Emission spectra peak 476 nm`, `Absorption spectra peak 430 nm`,
`Attenuation length >3500 mm`, `Cladding modality Multi-cladding`. These are exactly the
values the report prints in its `Ref. Rao2025` column, so the report's WLS reference
numbers trace to Rao, **not** to a text-verified Kuraray datasheet value.

---

## 1. SiPM — CEFLA S.C. ASD-NUV4S-P-40

Datasheet column locations: **p. 1** = features/ordering text; **p. 2** = "Geometrical,
Electrical, and Optical Typical Characteristics (Ta = 20 °C)"; **p. 4–5** = Fig. 8 PDE(λ),
Fig. 5 gain–OV (images).

Our values: `src/GeometryMuonScint.cc:369–390` (surface + `EFFICIENCY` table),
`:568–583` (SiPM bulk epoxy), `include/GeometryMuonScint.hh:126–132` (dimensions);
report claims `report/matrix_report.tex:324–331`, `:468–499`.

| # | Property | Datasheet value (location) | Our value (file:line) | Verdict | Notes |
|---|---|---|---|---|---|
| S1 | Peak sensitivity λp | **420 nm** (p. 2 table; p. 1 text) | **425–450 nm** plateau (`GeometryMuonScint.cc:384–385`) | **MISMATCH (minor)** | PDE vector is `0.43` at 2.917 eV (425 nm) *and* 2.755 eV (450 nm); max is red-shifted ≥5 nm (up to 30 nm). Report table says "420–425 nm" (`matrix_report.tex:327`). |
| S2 | PDE peak value | **43 %** at λp, at +6 V OV, excl. afterpulse/crosstalk (p. 2) | **0.43** (`GeometryMuonScint.cc:385`) | **MATCH** (value) | Peak amplitude matches exactly. |
| S3 | PDE(λ) curve | Fig. 8 (PDF p. 4–5, raster, not digitized) | 11-pt piecewise-linear vector (`GeometryMuonScint.cc:381–385`) | **UNSUPPORTED** | Shape is a hand-built smooth approximation, not a digitization of Fig. 8. Plausible (monotone rise to ~430 nm, UV/red roll-off) but no point matches the datasheet figure. |
| S4 | Spectral response range | **350–900 nm** (p. 2 table; p. 1 "350 nm to 900 nm") | **350–600 nm** (`GeometryMuonScint.cc:381–382`: 2.067–3.543 eV) | **MISMATCH/UNSUPPORTED** | Red end truncated at 600 nm (PDE 0.20) — the whole 600–900 nm range is absent. Harmless for ~425–480 nm scint/WLS light but not a faithful full curve. |
| S5 | Effective active area | **4×4 mm²** (p. 2) | **4×4×0.6 mm³** box (`GeometryMuonScint.hh:127–129`) | **MATCH** | Area correct. |
| S6 | Cell count | **9340** (p. 2) | not modelled | **ABSENT** | Reported in table only (`matrix_report.tex:326`). |
| S7 | Cell size / pitch | **40 µm × 40 µm** (p. 2) | not modelled | **ABSENT** | — |
| S8 | Cell fill factor | **60 %** (p. 2) | not modelled | **ABSENT** | — |
| S9 | Cell capacitance | **90 fF** (p. 2) | not modelled; report prints **0.84 nF** (`matrix_report.tex:331`) | **ABSENT / derived** | 0.84 nF ≈ 9340 × 90 fF — a *derived* total, not a datasheet entry. |
| S10 | Quenching resistance | **800 kΩ** (p. 2) | not modelled | **ABSENT** | — |
| S11 | Recharge time constant | **70 ns** (p. 2; Fig. 10) | not modelled | **ABSENT** | — |
| S12 | Gain | **3.6×10⁶** @20 °C, +6 V OV (p. 2 table); p. 1 features say "10⁶" | report prints **10⁶** (`matrix_report.tex:331`) | **MISMATCH (minor)** | Report quotes the marketing "10⁶", not the spec-table 3.6×10⁶. Not used by the optical model. |
| S13 | Breakdown voltage | **typ. 26 V** (min 24, max 28) (p. 2); σBV 50 mV | not modelled | **ABSENT** (acknowledged) | Report states BV/gain not modelled (`matrix_report.tex:1290`). |
| S14 | BV temp. coefficient | **26 mV/°C** (p. 2) | not modelled | **ABSENT** (acknowledged) | `matrix_report.tex:1290–1291`. |
| S15 | Recommended overvoltage | **2–6 V** (p. 2) | not modelled | **ABSENT** | PDE table implicitly assumes one fixed OV (the +6 V value), so OV dependence is dropped. |
| S16 | Dark count rate | **<50–100 kHz/mm²** (p. 1, p. 2) | not modelled | **ABSENT** | — |
| S17 | Afterpulsing probability | **<4 %**; delayed crosstalk/afterpulse Fig. 9 | not modelled | **ABSENT** | PDE is the "not including afterpulse and crosstalk" value, so the model is self-consistent at the primary-PDE level. |
| S18 | Epoxy resin n | **1.5115** @589 nm (uncured; "guide only, not a spec") (p. 2) | **1.55** (`GeometryMuonScint.cc:580`) | **MISMATCH (minor)** | Report acknowledges (`matrix_report.tex:329`). Irrelevant to detection because the photocathode is a `dielectric_metal` Bernoulli sink; only the Fresnel at the boundary is affected. |
| S19 | Grease / coupling medium | not part of this SiPM datasheet | **0.1 mm, n=1.46** (`GeometryMuonScint.cc:552–565`, `GeometryMuonScint.hh:132`) | **UNSUPPORTED** (by this datasheet) | BC-630-class silicone grease (`matrix_report.tex:330`); no datasheet in `refs/` backs n=1.46. |

**Detection mechanism** — `dielectric_metal` + `EFFICIENCY = PDE(λ)`, `REFLECTIVITY = 0`
(`GeometryMuonScint.cc:375–390`), with `SetBoundaryInvokeSD(true)` (`PhysicsList.cc:158`):
**MATCH** to the report's described Geant4 mechanism and to the datasheet's definition of PDE
as the single-photoelectron detection probability. This is faithful.

---

## 2. WLS fibre — Kuraray Y-11(200)

Datasheet identity facts (Kuraray catalogue p. P1–2, PDF p. 2, text layer): the worked
example is **`Y-11(200), 0.5 mm D., 10000 m BJ` → "Round fiber, Single cladding, Non-S type
… concentration of WLS dye is 200 ppm."** Quantitative spectra are figures (PDF pp. 3–6,
no text layer). Rao2025 Table III supplies the numbers used by the report.

Our values: `src/GeometryMuonScint.cc:784–854` (core/clad materials),
`include/GeometryMuonScint.hh:163–176` (geometry defaults);
`src/PhysicsList.cc:153–154, 255–258`.
Report claims `report/matrix_report.tex:313–322`, `:452–466`.

| # | Property | Datasheet value (location) | Our value (file:line) | Verdict | Notes |
|---|---|---|---|---|---|
| F1 | Grade / dye conc. | **Y-11(200) = 200 ppm** (catalogue p. P2) | 200 ppm **not modelled**; only optical signature (`GeometryMuonScint.cc:788–836`) | **PARTIAL** (acknowledged) | Code comment says concentration is intentionally not modelled; acceptable standard approach, but the spectral shape is then the only proxy for the dye loading. |
| F2 | Fibre diameter | Ø1.0 mm is an offered size (catalogue); Rao uses Ø1.0 mm | **Ø1.0 mm** core+clad (`GeometryMuonScint.hh:166`) | **MATCH** | Report `matrix_report.tex:314`. |
| F3 | Cladding modality | **Single cladding** for Y-11(200) (catalogue p. P2); Rao2025 Table III says **Multi-cladding** | **single PMMA clad** (`GeometryMuonScint.cc:838–854`) | **MATCH vs datasheet / MISMATCH vs Rao** | Genuine datasheet-vs-paper conflict. Code follows the datasheet default; report table (`matrix_report.tex:315`) honestly flags the difference. |
| F4 | Clad thickness | not given numerically | 3 % of radius (`GeometryMuonScint.hh:167`, `GeometryMuonScint.cc:864`) | **UNSUPPORTED** | Standard Kuraray single-clad ratio, but no datasheet number. |
| F5 | Core refractive index | not in datasheet text | **1.59** (`GeometryMuonScint.cc:802`) | **UNSUPPORTED (text)** | Canonical polystyrene core value; report marks reference as "---" (`matrix_report.tex:316`). |
| F6 | Clad refractive index | not in datasheet text | **1.49** (`GeometryMuonScint.cc:848`) | **UNSUPPORTED (text)** | Canonical PMMA value. |
| F7 | Absorption peak | **430 nm** (Rao2025 Table III; Kuraray spectra figure unreadable) | WLS onset at **480 nm**, monotonic increase to UV (min abs len at 380 nm) (`GeometryMuonScint.cc:817–821`) | **MISMATCH** | Code `WLSABSLENGTH` = 0.60 m @480, 0.20 @460, 0.05 @440, 0.005 @420, 0.0005 @400, 0.00005 @380 nm. It is *monotonic* toward the UV — there is **no 430 nm absorption peak and no UV fall-off**. The report table claim "430–460 nm (WLS onset)" (`matrix_report.tex:317`) does not match the code onset (~480 nm). |
| F8 | Emission peak | **476 nm** (Rao2025 Table III) | **480 nm** (`GeometryMuonScint.cc:825–828`: `WLSCOMPONENT`=1.00 at 2.583 eV) | **MISMATCH (minor)** | Code comment says "peak ~476 nm / 2.605 eV" (`:822`), but the tabulated peak is 2.583 eV = **480 nm**. Report is internally inconsistent: table 476 nm (`:318`), text 480 nm (`:343`), text 490 nm (`:459`), text "mean ~485 nm" (`:465`). |
| F9 | Emission band shape | broad, FWHM ≈70–80 nm (figure) | triangular 460→480→650 nm; zero ≤440 nm (`GeometryMuonScint.cc:825–828`) | **APPROXIMATION** | Not digitized; weights are hand-set. |
| F10 | Attenuation length | **>3500 mm** (Rao2025 Table III; Kuraray figure unreadable) | **5.0 m flat** bulk (`GeometryMuonScint.cc:806–808`) + lossless TIR; clad 20 m (`:849`) | **MATCH (optimistic)** | ~1.43× longer than the >3.5 m lower bound; no wavelength dependence and no Rayleigh/scattering term. Over a 250 mm fibre the difference is small (~5 % vs ~7 % bulk loss). |
| F11 | WLS decay time | **not stated in the Kuraray datasheet text**; literature Y-11 ≈7 ns | **7.0 ns** via `WLSTIMECONSTANT` (`GeometryMuonScint.cc:834`) | **UNSUPPORTED (datasheet)** | Value is literature, not datasheet. |
| F12 | WLS time profile | — | `SetWLSTimeProfile("delta")` (`PhysicsList.cc:154`) | **MISMATCH (semantics)** | With Geant4's `delta` generator (`G4WLSTimeGeneratorProfileDelta::GenerateTime` returns the constant), the 7 ns is applied as a **fixed delay**, not an exponential decay — no timing spread. `matrix_report.tex:320` presents it as a "decay time". |
| F13 | Channel bore | paper specifies "~1.0 mm channels" (per `AGENTS.md`; paper basis) | **Ø1.1 mm** (`GeometryMuonScint.hh:169`) | **MISMATCH (vs paper)** | Deliberate 0.05 mm air annulus to preserve TIR (`AGENTS.md`); correct for the model but larger than the paper's channel. |
| F14 | Fibre count / pitch | four Ø1.0 mm fibres, one row (Rao) | 4 fibres, 1.5 mm pitch → 1.0 mm bundle (`GeometryMuonScint.hh:165,170–171`) | **MATCH** | — |

**WLS process wiring** — `OpWLS` auto-enabled only when WLS geometry is present
(`PhysicsList.cc:255–258`), driven by the core's `WLSABSLENGTH`/`WLSCOMPONENT`/
`WLSTIMECONSTANT`: **MATCH** to the report's description (`matrix_report.tex:452–466`).
The `WLSCOMPONENT`-must-be-below-absorption rule documented in `AGENTS.md` is correctly
implemented (emission weight zero at/below the 460 nm absorption edge).

---

## 3. Datasheet features deliberately not used (summary)

- SiPM: cell count/pitch/fill-factor, cell capacitance, quenching resistance, recharge
  time, gain, breakdown voltage + temperature coefficient, overvoltage range, dark count
  rate, afterpulsing/crosstalk — the device is treated purely as a `PDE(λ)` Bernoulli
  sink. **All acknowledged** in `matrix_report.tex:1287–1293` and table row `:331`.
- SiPM: the full 600–900 nm PDE and the Fig. 8 curve shape.
- Fibre: dye concentration (ppm), the multi-cladding option, and a wavelength-dependent
  attenuation length.

---

## Top discrepancies (prioritized)

1. **Y-11 emission peak is numerically inconsistent across code and report.**
   Code tabulates the peak at **480 nm** (`src/GeometryMuonScint.cc:825–828`) while its own
   comment says 476 nm (`:822`); the report says 476 nm (`report/matrix_report.tex:318`),
   480 nm (`:343`), 490 nm (`:459`) and "mean ~485 nm" (`:465`). Datasheet (via Rao2025
   Table III) = 476 nm. **MISMATCH.**
2. **Y-11 WLS absorption shape does not reproduce the 430 nm absorption peak.**
   `WLSABSLENGTH` is monotonic (strongest at 380 nm, absent emission), onset ≈480 nm
   (`src/GeometryMuonScint.cc:817–821`), whereas the reference peak is 430 nm; the report
   table states "430–460 nm (WLS onset)" (`report/matrix_report.tex:317`), which the code
   does not implement. **MISMATCH.**
3. **SiPM PDE(λ) curve is a reconstruction, and its peak is red-shifted.**
   Datasheet peak 420 nm / 43 % (AdvanSiD p. 2; Fig. 8 p. 4–5). Code peaks at 0.43 at
   **425 and 450 nm** with a hand-built shape (`src/GeometryMuonScint.cc:381–385`).
   Amplitude MATCH, curve + peak location **MISMATCH/UNSUPPORTED**.
4. **SiPM spectral range truncated.** Datasheet 350–900 nm (p. 2); code stops at 600 nm
   (`src/GeometryMuonScint.cc:381–382`). **MISMATCH.**
5. **WLS decay time semantics.** 7 ns is set (`:834`) but the `delta` profile
   (`src/PhysicsList.cc:154`) applies it as a fixed delay, not a decay; and the Kuraray
   datasheet does not state 7 ns at all. **UNSUPPORTED + MISMATCH.**
6. **Cladding modality conflict.** Kuraray Y-11(200) is **single-clad** (catalogue p. P2)
   while Rao2025 Table III says multi-clad; code implements single PMMA clad
   (`src/GeometryMuonScint.cc:838–854`), report flags it (`report/matrix_report.tex:315`).
   **Datasheet-vs-paper mismatch (code follows datasheet).**
7. **SiPM gain misquoted in the report.** Spec table = 3.6×10⁶ (p. 2); report prints 10⁶
   (`report/matrix_report.tex:331`). **MISMATCH (minor).**
8. **SiPM epoxy n = 1.55 vs datasheet 1.5115** (`src/GeometryMuonScint.cc:580`;
   datasheet p. 2, "guide only"). **MISMATCH (minor).**
9. **Fibre attenuation artificially flat and long.** 5.0 m flat vs ">3500 mm"
   wavelength-dependent (`src/GeometryMuonScint.cc:806–808`). **MATCH but optimistic.**
10. **Channel Ø1.1 mm vs paper ~1.0 mm** (`include/GeometryMuonScint.hh:169`) —
    deliberate air annulus, deviates from the experimental channel. **MISMATCH vs paper.**

### Verdict count
MATCH/accept: 5 (S2, S5, F2, F3-vs-datasheet, F14, plus mechanism rows)
MISMATCH: 9 (S1, S4, S12, S18, F7, F8, F12, F13, plus S3 curve)
UNSUPPORTED (no datasheet basis): 6 (S3, S19, F4, F5, F6, F11)
ABSENT (not modelled, mostly acknowledged): 11 (S6–S11, S13–S17)
