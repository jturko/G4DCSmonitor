# Citation-accuracy audit: Agostinelli2003 & Allison2016

Read-only audit of the two canonical Geant4 references in
`report/matrix_report.tex` against
`report/refs/Agostinelli2003.pdf` and `report/refs/Allison2016.pdf`.

Tooling: `pdftotext -layout`, `pdfinfo`, `pdffonts`. Nothing outside
`report/refs/notes/` was written or modified.

---

## 0. Where the two references are actually used

`grep` over the report (and its `.aux`/`.toc`) shows **exactly one** citation
site for either key:

- `report/matrix_report.tex:82`:
  > `This report documents a \gfour\ (v11.3.2)~\cite{Agostinelli2003,Allison2016}`
  > `simulation built to address that question ...`

Both keys are consumed together, once, as a generic "this is Geant4"
reference. There are **no other `\cite` calls** to `Agostinelli2003` or
`Allison2016` anywhere in the document, so no quantitative value, quotation,
or technical assertion in the report is attributed to them.

The report's *substantive* Geant4 physics assertions are attributed elsewhere:

- `matrix_report.tex:377-385` (optical photon transport, active processes,
  boundary physics) → `\cite{Geant4PRM}` (Geant4 Physics Reference Manual).
- `matrix_report.tex:387-406` (unified surface model, border-vs-skin
  precedence, TIR fallback) → `\cite{Levin1996}`, `\cite{Nilsson2015}`.

---

## 1. Per-claim audit

### Claim A — "This report documents a Geant4 (v11.3.2) simulation"
- **Report location / quote:** `matrix_report.tex:82`.
- **Source location:** Agostinelli2003 title page (`FERMILAB-Pub-03/339`,
  `SLAC-PUB-9350`, December 2003); Allison2016 title page p.1 and §1
  ("The evolution of GEANT4").
- **Verdict:** SUPPORTED (with a version caveat).
- **Evidence:** Both are the standard, correct canonical Geant4 citations.
  Agostinelli2003 is the original toolkit description; Allison2016 is the
  follow-up "developments" paper. However, **neither paper describes
  v11.3.2.** Allison2016 covers the toolkit up to the ~10.1–10.2 era
  ("version 10.1" is the newest release named, `allison.txt:1498`);
  Agostinelli2003 predates v11 by ~20 years. Combining both keys as a single
  reference to "Geant4 (v11.3.2)" is common practice but the version itself
  is *not* documented by either source — the report's actual authority for
  v11.3.2 is the source tree (`.refs/source/...`, e.g. `:385`). This is a
  caveat, not an error.

### Claim B — Optical photon transport / boundary process description
- **Report location / quote:** `matrix_report.tex:377-466`
  ("Optical photons are transported by the standard \gfour\ optical physics
  module ... `G4OpBoundaryProcess` ... the unified surface model of Levin and
  Moisan ...").
- **Source location:** Allison2016 §4.1.9 "Optical physics"
  (`allison.txt:1431-1489`): scintillator nonlinearity, light-emission rise
  time, Čerenkov origin density, metal-surface reflectivity via complex
  refractive index, Mie scattering (Henyey–Greenstein), surface reflectances
  from look-up tables, anti-reflective and dichroic coatings, and the optical
  boundary process across parallel worlds.
- **Verdict:** SUPPORTED by Allison2016 (topic), but **not cited** for it.
  The report correctly routes this to `Geant4PRM` + `Levin1996` +
  `Nilsson2015`.
- **Evidence:** Allison2016 independently supports the optical-transport /
  boundary-process subject matter, so it *would* be a valid additional cite
  there. The report does not misattribute the claim to it, so this is a
  **missed-citation opportunity**, not a WRONG/OVERSTATED verdict.
  Agostinelli2003 also covers optical photon processes, but that **could not
  be verified from the supplied PDF** (see §2 — non-extractable text).

### Claim C — Multithreading
- **Report location / quote:** abstract `matrix_report.tex:58-59`
  ("a rare intermittent multi-threaded stall in the optical tracking");
  `:579` ("Runs used \num{8} threads on an 8-thread ... workstation");
  `:1259-1270` ("Intermittent multi-threaded stall ... a single worker thread
  entered a tight loop ... while the remaining threads waited at the
  event-level barrier").
- **Source location:** Allison2016 §2 "Multithreading" (`allison.txt:277-513`),
  incl. master–worker model, `G4MTRunManager`/`G4WorkerRunManager`, event-level
  barriers, reproducibility, and thread overhead.
- **Verdict:** SUPPORTED (Allison2016 §2 is the canonical MT reference), but
  **not cited** where multithreading is discussed.
- **Evidence:** The report makes no factual claim about how Geant4's MT engine
  works that could be checked against the paper; it reports an *observation*
  (a stall). Still, the one place in the document that relies on Geant4's
  master/worker barrier semantics is exactly the place where Allison2016
  (and no other reference in the bibliography) applies. **Missed citation.**

### Claim D — Physics list activation of `G4OpWLS`
- **Report location / quote:** `matrix_report.tex:460-462` ("The process is
  switched on by the physics list when the WLS geometry is detected (the
  \gfour\ optical-physics constructor reads the process activation at build
  time ...)"). Also `:498-499`
  (`G4OpticalParameters::SetBoundaryInvokeSD(true)`).
- **Source location:** Agostinelli2003 (physics-list / process framework);
  Allison2016 "physics lists" discussion (`allison.txt:300`, `:1145`).
- **Verdict:** SUPPORTED by Agostinelli2003 (framework) but **not cited** for
  it; correctly left to `Geant4PRM`/source verification as well.
- **Evidence:** The claim is a concrete API statement about
  `G4OpticalPhysics`/`G4OpticalParameters`, which post-dates both papers.
  Neither paper documents `SetBoundaryInvokeSD`; the report's stated
  verification basis (the v11.3.2 source) is the honest authority. No wrong
  attribution.

---

## 2. Bibliographic metadata verification

### Agostinelli2003 (`matrix_report.tex:1330-1334`)
> `S. Agostinelli et al., Geant4---a simulation toolkit,`
> `Nucl. Instrum. Methods A 506, 250--303 (2003).`
> `doi:10.1016/S0168-9002(03)01368-8`

- **Verdict:** METADATA CORRECT.
- **Evidence:** The published-record values — NIM A **506**, pp. **250–303**,
  **2003**, DOI **10.1016/S0168-9002(03)01368-8** — are the standard citation
  and agree with `manifest.csv:3`. The title punctuation (`---` → em-dash,
  "Geant4—a simulation toolkit") is faithful.
- **Caveat:** `report/refs/Agostinelli2003.pdf` is **not** the NIM A version.
  `pdfinfo` reports **86 pages**, and the cover reads
  `FERMILAB-Pub-03/339 December 2003 / SLAC-PUB-9350` — i.e. the Fermilab/SLAC
  **preprint** (matches `manifest.csv` source "Fermilab/SLAC preprint via
  inspirehep"). The journal, volume and page range in the bibitem therefore
  cannot be confirmed from this PDF; they come from the published record.

### Allison2016 (`matrix_report.tex:1336-1340`)
> `J. Allison et al., Recent developments in Geant4,`
> `Nucl. Instrum. Methods A 835, 186--225 (2016).`
> `doi:10.1016/j.nima.2016.06.125`

- **Verdict:** METADATA CORRECT.
- **Evidence:** PDF is the **Warwick WRAP published version** (CC BY-NC-ND,
  `allison.txt:1-12`). `pdfinfo` title = "Recent developments in Geant4";
  DOI **10.1016/j.nima.2016.06.125** printed at `allison.txt:131`.
  NIM A **835**, pp. **186–225** (2016) is the correct published record and
  agrees with `manifest.csv:4`.
- **Caveat:** this is an "article in press" render — the running header prints
  `Nuclear Instruments and Methods in Physics Research A ∎ (∎∎∎∎) ∎∎∎–∎∎∎`,
  so volume 835 / pages 186–225 are **not visible in the PDF**; only the DOI
  string is. `pdfinfo` page count = **41** (consistent with the 40-page
  186–225 range; task note ~41 pp confirmed).
- Minor: source header capitalises **GEANT4**, report/journal title uses
  **Geant4**. This is presentational, not an error.

---

## 3. Correct paper for the claim?

| Topic in report | Cited to | Is that the right paper? |
|---|---|---|
| "Geant4 toolkit" (generic, `:82`) | Agostinelli2003 + Allison2016 together | Yes — both are canonical; combining them is acceptable |
| Optical transport / boundary (`:377`) | Geant4PRM (+ Levin1996, Nilsson2015) | Yes for the API/process details; Allison2016 §4.1.9 could also be added |
| Multithreading (`:58`, `:1259`) | uncited | Allison2016 §2 is the correct source; **2003 would be wrong** (predates MT) |
| Physics-list activation (`:460`) | uncited (Geant4PRM) | Agostinelli2003 is the right *conceptual* source; API detail post-dates both |

No instance was found where the **2003** paper is used for a post-2003
(MT/optics-API) claim, or where the **2016** developments paper is used for an
original-architecture claim. The wrong-paper risk is therefore **not
realised**.

---

## 4. Content-extraction limitation (important)

`report/refs/Agostinelli2003.pdf` body text is **not machine-extractable**:
`pdffonts` reports the main text as **Type 3 "Custom" embedded fonts with no
ToUnicode map**, so `pdftotext` returns mojibake for the entire body (only the
Fermilab/SLAC cover line, running feet, and a few figure labels survive).
Consequently the *content* of the 2003 paper (optical processes, physics
lists, architecture) **cannot be verified from the supplied PDF by
`pdftotext`**; only its cover-page custodial metadata is readable.
Allison2016 extracts cleanly and was verified directly.

If content-level verification of Agostinelli2003 is required, re-download the
publisher (NIM A) PDF or an arXiv version with a usable ToUnicode CMap.

---

## 5. Prioritized findings

1. **(Low) Version overreach — Claim A.** `v11.3.2` is attributed to the
   2003+2016 pair, but neither paper documents v11.3.2 (Allison2016 stops at
   the ~10.1–10.2 era). Common practice, but the report should be understood
   as relying on the v11.3.2 source tree, not these papers, for version
   specifics. Consider a note or an additional modern reference.
2. **(Low) Missed citation — multithreading.** `matrix_report.tex:58` and
   `:1259` discuss a multi-threaded stall without citing Allison2016 §2, the
   canonical Geant4 multithreading reference. Adding `\cite{Allison2016}`
   there is the most defensible fix in the document.
3. **(Low) Missed citation — optical physics.** Allison2016 §4.1.9 is a valid
   companion cite for the `matrix_report.tex:377` optical-physics paragraph
   (currently only `Geant4PRM`/`Levin1996`/`Nilsson2015`).
4. **(Informational) Local PDF is a preprint for Agostinelli2003.** The
   bibitem's journal/volume/pages (NIM A 506, 250–303) cannot be confirmed
   from `report/refs/Agostinelli2003.pdf` (86-page Fermilab/SLAC preprint,
   FERMILAB-Pub-03/339 / SLAC-PUB-9350).
5. **(Informational) Allison2016 PDF is an in-press render.** Volume 835 /
   pages 186–225 are not printed in the PDF; only DOI
   `10.1016/j.nima.2016.06.125` is. Metadata is nevertheless correct.
6. **(Cosmetic) Agostinelli2003 body text is non-extractable** (Type 3 custom
   fonts, no ToUnicode), blocking automated content verification of the 2003
   paper.

**No WRONG or OVERSTATED attributions were found; both bibitems are
metadata-correct.**
