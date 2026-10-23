# Output, diagnostics and detected-photon visualisation

Read on demand from `AGENTS.md`.

**Diagnostics/output**: `include/OpticalDiagnostics.hh` (thread-local per-event optical
accounting + per-slab produced/detected + primary-muon first-entry x-y); `opticalStats`
ntuple (nScint/nCerenkov/nKilled/nDetected/nEscaped/nAlive). Toggle
`/dcs-monitor/run/writeOpticalFluxMap` (default **off**) books per-slab
`h2_optflux_xy_slab<i>` **track-length only** (1 mm cells); booking is in
`BeginOfRunAction` (needs geometry).

**Collection efficiency lives in `sipmHits`** (not H2 maps). `MuonScintSiPMSD::EndOfEvent`
writes one row per (event, SiPM) — including zero-detection rows — with fixed columns
evtNb, det, sipm, nDetected, tFirst, meanWavelength_nm, rmsWavelength_nm, weight, muonX,
muonY, nProduced, **tDetected, wlDetected_nm, creatorDetected** (the first two are
variable-length vector branches: per-photoelectron global arrival times [ns] and wavelengths
[nm], one entry per detected photoelectron, empty for zero-detection rows; `creatorDetected`
is a parallel vector<int> giving the creating process of each recorded photoelectron —
1 = OpWLS, 2 = Scintillation, 3 = Čerenkov, 0 = unknown — via the thread-local
`gDetectedTimes`/`gDetectedWavelengths`/`gDetectedCreator` buffers in
`include/OpticalDiagnostics.hh`, refilled per row by `MuonScintSiPMSD::EndOfEvent`, enabling
offline pulse-shape and process-mix studies).
`nProduced` = all optical photons created in the slab that event
(scint + Čerenkov). Rows are emitted only for slabs the muon entered
(`gOpticalDiag.muonEntrySet[det]`); legacy fallback (hit-only, muonX/Y = −9999) when
`fDetector == nullptr`. SD `EndOfEvent` runs before `EventAction::EndOfEventAction`
(`.refs/geant4/source/event/src/G4EventManager.cc:315` (`sdManager->TerminateCurrentEvent`) vs
`:323` (`userEventAction->EndOfEventAction`)), so it can read `gOpticalDiag`.
For pe/evt, sum `nDetected` over all SiPMs and divide by the number of events (`evtNb`),
**not** by row count — dividing by rows gives per-SiPM pe/evt (the two differ by the SiPM count).

**Detected-photon visualization**: `/dcs-monitor/run/showDetectedPhotonsOnly true` (default
**off**, `RunMessenger`) makes `EventAction::EndOfEventAction` prune the event's
`G4TrajectoryContainer` to the track IDs recorded by `MuonScintSiPMSD::ProcessHits`
(`gOpticalDiag.detectedTrackIDs`, cleared per event) **plus their full ancestor chain**
(walk `G4VTrajectory::GetParentID`). The ancestor walk matters for WLS: `G4OpWLS` kills the
creator optical photon, so its track ID is not in `detectedTrackIDs`; including parents keeps
that pre-WLS photon (and the primary muon) in the event. It is vis-only and requires
`/tracking/storeTrajectory 1`; G4VisManager draws the event *after* `EndOfEventAction`, so
the stored viewer sees the detected WLS photons, their creator optical photons and the muon;
unrelated undetected photons are removed. `macros/muon_movie_wls.mac` +
`macros/build_movie_wls.sh` use it for Config 4 (preset **7 analytic** fibres by default;
`WLS_PRESET=5` selects the legacy tessellated fibres, but their nested multi-clad cores 2/3
can be culled by the OpenGL viewer — the clads still draw, so prefer preset 7;
the black `WLSClampLV` is hidden, together with the coupling grease film
(`WLSCoupleFilmLV`) and read-out grease pads (`GreaseLV`): those `G4SubtractionSolid`
booleans render as a stipple-dot cloud over the fibres in the OGL viewer.
The camera is animated by `/dcs-monitor/run/movieCamera <time>` (master-only; worker copies
are skipped): it holds a wide view (target `0 0 0`, theta 55, zoom 1.0) through the muon
crossing, then eases over 1.5–12 ns to the fibre/SiPM close-up (target `-110 0 0 mm`, theta
80, zoom 3.5). The loop body is `macros/muon_movie_wls.loop` (separate from the shared quad
loop), glossy reflector, seed 12345/67890, display-only `LY_SCALE=0.4` for preset 5 / `0.1`
for preset 7, 0–80 ns, step 0.25 ns, tail 10 ns) →
`report/movie_wls/muon_movie_wls.{gif,mp4}` (270 frames after trimming). Detected photons
and their pre-WLS creators are drawn green, the muon red, delta electrons orange; all
detections are WLS secondaries except rare direct scintillation hits. WLS re-emission is
exponential (τ≈7 ns), so detections occur out to ~60–70 ns.
`macros/movie_trim.py` now takes `[pad] [color]`; use `green` for these frames (the
fibre geometry is pale/dark green and does not satisfy the pure-green mask; a moving camera
also invalidates the first-frame `green-delta` baseline). `green-delta` remains for
static-camera movies.
