## Muon-trigger detector simulation — status update

Hi @colleague1, @colleague2, @colleague3,

Quick update on the muon-trigger scintillator study (`1-muon-trigger-detector`); then I'd
like to set up a short in-person meeting to go through the results and agree next steps.

**Headline**

- The full matrix (4 SiPM layouts × 4 reflectors × 3 beam positions — 48 runs / 48k events)
  and a whole-slab scan (25k events per config) are complete. The simulation reproduces the
  paper's ordering **bare ≪ TiO₂ < Al ≈ glossy** in every layout.
- Key physics point: the bare plate is *worst* because it sits in effective optical contact
  with the black holder (n ≈ 1.55 vs scintillator 1.58), so TIR is frustrated and trapped
  light leaks into the wall. Modelling that (rather than a free-standing polished plate) is
  what flips bare from best to worst.
- Reflective wrappers homogenise the response (3-patch position contrast κ < 4%;
  whole-slab 2-D contrast κ₂D ≤ 2.5), while bare shows a strong centre→edge rise and a
  corner collapse (κ₂D ≈ 9.5 for config 1, rising to ≈ 138 for the single-SiPM config 4).
- Config 4 (single SiPM + WLS fibre clamp) is working and green-shifted to ~485 nm,
  confirming the Y-11 wavelength shift.

**Config 4 — WLS fibre readout geometry.** New VRML renders: the full event view (an optical
photon is the red track) and the fibre holder + fibres + SiPM.

![Config-4 WLS geometry](WLS_PANEL_URL)
<!-- attach: report/figures/wls_vrml_2panel.png -->

**One muon, four reflectors.** The same 30 MeV event rendered for bare / TiO₂ / Al / glossy
and time-sliced frame by frame, so the light propagation can be compared side by side.

![Four-reflector muon animation](QUAD_GIF_URL)
<!-- attach: report/movie_quad/muon_movie_quad.gif -->

*(mp4 version: [muon_movie_quad.mp4](QUAD_MP4_URL))* <!-- attach: report/movie_quad/muon_movie_quad.mp4 -->

**Light collection efficiency.** Overall efficiency across the matrix (first) and the
full-plate map for config 1 (second).

![Overall collection efficiency](EFF_BARS_URL)
<!-- attach: report/figures/fig_eff_bars.png -->

![Whole-slab efficiency map, config 1](WHOLE_SLAB_URL)
<!-- attach: report/figures/whole_slab_cfg1.png -->

**Full report (PDF):** [matrix_report.pdf](REPORT_PDF_URL)
<!-- attach: report/matrix_report.pdf -->

**Toward a meeting.** I'd like to discuss (i) the absolute normalisation (PDE / grease /
reflectivity assumptions), (ii) the reversed centre/edge/corner trend in the diffuse TiO₂
case, and (iii) next steps. Would <day, time> work?

Thanks,
J.
