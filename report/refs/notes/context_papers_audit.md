# Citation-accuracy audit — "context / application" references

Read-only audit of how `report/matrix_report.tex` (and `src/`) use five
context/application references. For each: where cited, exact claim, metadata
check against the source PDF title page, and whether the source supports the
claim.

Sources (extracted with `pdftotext -layout`):
`report/refs/{Gundacker2020,Anfimov2021,Alekseev2022,Bonechi2020,Korpachev2017}.pdf`.
`src/` contains **no** comments or citations naming any of these authors
(grepped `Gundacker|Anfimov|Alekseev|Bonechi|Korpachev|Heering|Chadeeva` → no hits).

Verdict legend: SUPPORTED / OVERSTATED / WRONG / NOT-FOUND.

---

## 1. `Gundacker2020`

- **Cited at:** `report/matrix_report.tex:132`
  > "Each SiPM~\cite{Gundacker2020} is modelled mechanically as a
  > $4 \times 4 \times \SI{0.6}{\milli\meter}$ box of ``SiPM epoxy'' …"
- **Metadata check:** `bibitem` (tex:1368–1373): "S. Gundacker, A. Heering, *The
  silicon photomultiplier: fundamentals and applications of a modern solid-state
  photon detector*, Phys. Med. Biol. **65**, 17TR01 (2020), doi:10.1088/1361-6560/ab7b2d".
  PDF title page: "Stefan Gundacker and Arjan Heering 2020 Phys. Med. Biol. 65
  17TR01", DOI `10.1088/1361-6560/ab7b2d`. → **MATCH**.
- **Claim-support verdict:** **SUPPORTED**. This is a generic background citation
  for "what a SiPM is"; the source is a topical review of SiPM fundamentals and
  applications. The sentence does not attribute a specific numeric claim to it.
- **Evidence:** PDF header/title block.

## 2. `Bonechi2020`

- **Cited at:** `report/matrix_report.tex:72`
  > "Plastic scintillators read out at their edges by SiPMs are attractive for
  > large-area, low-cost muon triggers and veto systems for cosmic-ray muon
  > tomography~\cite{Borozdin2003,Bonechi2020}."
- **Metadata check:** `bibitem` (tex:1434–1438): "L. Bonechi, R. D'Alessandro,
  A. Giammanco, *Atmospheric muons as an imaging tool*, Rev. Phys. **5**, 100038
  (2020), doi:10.1016/j.revip.2020.100038". PDF title page: "Atmospheric muons as
  an imaging tool — Lorenzo Bonechi, Raffaello D'Alessandro, Andrea Giammanco"
  (arXiv:1906.03934). → **MATCH**.
- **Claim-support verdict:** **SUPPORTED**. The review is exactly a muography
  ("cosmic-ray muon tomography") review and it explicitly states plastic
  scintillators + SiPM are attractive/low-cost and scalable.
- **Evidence:** §4.1 (p. ~14): "Plastic scintillators constitute an ideal choice
  … Robust muon trackers … based on fast-response plastic scintillator materials
  can be designed with a reasonable price/performance ratio (≈ 50 Ke/m²). These
  detectors have become even more attractive since the advent of the Silicon Photo
  Multiplier (SiPM), replacing the traditional photo-multipliers (PM) at a much
  lower cost…". The "trigger / veto" wording is also used in the review (top
  detector "used as a trigger, a bottom detector used as a veto", p. ~20).

## 3. `Korpachev2017`

- **Cited at:** `report/matrix_report.tex:80`
  > "…because the photon transport is a multi-bounce random walk that is
  > difficult to model analytically~\cite{Korpachev2017}."
- **Metadata check:** `bibitem` (tex:1361–1366): "S. Korpachev, M. Chadeeva,
  *Geant4 simulation of optical photon transport in scintillator tile with direct
  readout by silicon photomultiplier*, J. Phys. Conf. Ser. **798**, 012218 (2017)".
  PDF title block: same title (with "with direct readout…"), "S Korpachev and
  M Chadeeva", "J. Phys.: Conf. Ser. 798 (2017) 012218". → **MATCH**.
- **Claim-support verdict:** **OVERSTATED**. The paper is a Geant4
  optical-photon-transport simulation *validated against experiment*; it never
  states that photon transport is a "multi-bounce random walk" nor that it is
  "difficult to model analytically". No occurrence of "analytic", "random walk",
  "multi-bounce", "difficult" or "complex" in the extracted text. The citation is
  a reasonable example of "simulation is the right tool", but the analytic-
  intractability claim is the report's own reasoning, not the source's.
- **Evidence:** Abstract/§1 describe simulating tile response with Geant4 and
  comparing to data; no analytic-tractability statement anywhere.

## 4. `Alekseev2022`

- **Cited at:** `report/matrix_report.tex:222–223`
  > "Four \SI{1.0}{\milli\meter}-diameter Kuraray Y-11(200) wavelength-shifting
  > fibres~\cite{KurarayPSF,Alekseev2022} run the full \SI{250}{\milli\meter}
  > length …"
- **Metadata check:** `bibitem` (tex:1422–1426): "I. Alekseev *et al.*, *The
  performance of a new Kuraray wavelength shifting fiber YS-2*, J. Instrum. **17**,
  P01031 (2022)". PDF (arXiv:2112.00136v2) header: same title, authors "I.
  Alekseev, M. Danilov, V. Rusinov, E. Samigullin, D. Svirida, E. Tarkovsy",
  "Prepared for submission to JINST". → **MATCH** (author list correctly truncated
  to "I. Alekseev *et al.*").
- **Claim-support verdict:** **SUPPORTED** (for the Y-11(200)/1 mm fibre fact
  only). The paper's subject is the *new* YS-2 fibre, but it explicitly measures
  and compares 1 mm Kuraray **Y-11(200)M** as the reference, so citing it for the
  Y-11(200) 1.0 mm WLS fibre is legitimate.
- **Evidence:** "the comparison of 1 mm Kuraray fibers: YS-2MJ Multi (YS-2) and
  Y-11(200)M Multi (Y-11)"; decay/light-yield/attenuation tables for Y-11. Caveat:
  the report cites this paper alongside `KurarayPSF` for fibre properties; the
  paper does not characterise the 250 mm length or groove/clamp geometry.

## 5. `Anfimov2021`

- **Cited at:** `report/matrix_report.tex:1290–1291`
  > "The SiPM breakdown-voltage and gain (which drive the PDE) are also
  > temperature-dependent~\cite{Anfimov2021}, an effect not modelled here."
- **Metadata check:** `bibitem` (tex:1375–1379): "N. Anfimov *et al.*, *Study of
  silicon photomultiplier performance at different temperatures*, Nucl. Instrum.
  Methods A **997**, 165162 (2021)". PDF (arXiv:2005.10665v1) header: "Study of
  Silicon Photomultiplier Performance at Different Temperatures", authors
  "N. Anfimov, D. Fedoseev, A. Rybnikov, A. Selyunin, S. Sokolov, A. Sotnikov",
  stamped "Preprint submitted to Nuclear Instruments and Methods in Physics
  Research A". → **MATCH** (venue and "et al." correct; volume/article number
  997/165162 is not printed on the arXiv preprint, so it cannot be confirmed from
  the PDF alone, but is consistent).
- **Claim-support verdict:** **OVERSTATED** (partially supported).
  - Breakdown-voltage temperature dependence: **supported** (Table 1 / Fig. 6
    show V_bd falling from ≈26.6 V at 23 °C to ≈23.8 V at −84 °C).
  - "gain … temperature-dependent": the paper does **not** measure gain vs
    temperature (only mentions amplifier gain / device gain figures of merit).
  - "(which drive the PDE)": this **conflicts with the paper's headline result** —
    the abstract and §3 state "No significant difference for the PDE was observed"
    / "there is no significant temperature dependence of the PDE for both
    samples." Thus the parenthetical implication that PDE is temperature-driven is
    contradicted by the cited source.
- **Evidence:** Abstract: "No significant difference for the PDE was observed."
  §3: "One can see that there is no significant temperature dependence of the PDE
  for both samples." Table 1 and Fig. 6: V_bd vs T.

---

## Prioritized citation problems

1. **`Anfimov2021` (tex:1290–1291) — OVERSTATED / misleading implication.**
   The parenthetical "(which drive the PDE)" is contradicted by the source, whose
   central finding is that **PDE has no significant temperature dependence**.
   The paper also does not measure *gain* vs temperature. Fix: either drop the
   "which drive the PDE" clause, or cite the source only for breakdown-voltage
   (and dark-count-rate) temperature dependence, and/or cite a SiPM-specific
   reference for the PDE/gain-vs-temperature link. This is the highest-priority
   issue because it inverts the cited work's main conclusion.

2. **`Korpachev2017` (tex:80) — OVERSTATED attribution.**
   The claim that photon transport "is a multi-bounce random walk that is
   difficult to model analytically" is not made in the source; Korpachev &
   Chadeeva is a Geant4-vs-data validation study. The citation supports only
   "optical photon transport is simulated in Geant4", not analytic intractability.
   Fix: reword to cite it as an example of Geant4 optical-photon-transport
   simulation, or attribute the analytic-difficulty statement to a different
   source / to the report itself.

3. **Metadata: no true MISMATCH found, but two entries rest on preprints.**
   `Anfimov2021` (tex:1375–1379) and `Alekseev2022` (tex:1422–1426) are cited
   with final journal volume/page numbers (NIM A 997 165162; JINST 17 P01031)
   that are **not present in the supplied PDFs** (both are arXiv/preprint copies
   stamped "Preprint submitted to …"). The numbering is consistent with the known
   published versions, but cannot be verified from the audited PDFs.
   `Bonechi2020` is likewise an arXiv preprint (published version: Rev. Phys. 5,
   100038, 2020); entry is consistent.

4. **Minor:** `Alekseev2022` is used for Y-11(200) fibre facts although the paper's
   subject is the newer YS-2; it is a valid reference for the 1 mm Y-11(200)
   comparison but does not document the 250 mm / clamp geometry. `Gundacker2020`
   and `Bonechi2020` claims are well supported and metadata matches.

### Summary table

| Key | Tex line | Metadata | Claim verdict |
|-----|----------|----------|---------------|
| Gundacker2020 | 132 | MATCH | SUPPORTED |
| Bonechi2020 | 72 | MATCH | SUPPORTED |
| Korpachev2017 | 80 | MATCH | OVERSTATED |
| Alekseev2022 | 222–223 | MATCH (preprint) | SUPPORTED |
| Anfimov2021 | 1290–1291 | MATCH (preprint) | OVERSTATED |
