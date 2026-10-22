# Geant4 geometry / solids consistency audit

Read-only audit. Repo: `/home/turko46/geant4-sims/G4DCSmonitor`.
Date: 2026-10-15. Reference tree: `.refs/` → Geant4 v11.3.2 (read-only).

Scope: the custom solids and placements in the Config-4 WLS-fibre clamp and the
black holder — `G4TessellatedSolid` swept tubes, `G4SubtractionSolid` cutters,
mother-volume containment / `pSurfChk` overlap checks, and world-sibling
placement with `G4LogicalBorderSurface` / `G4LogicalSkinSurface`. For every
construct the entries below give (a) our site `file:line`, (b) the Geant4 rule or
example with source `file:line` + quote, (c) a verdict, (d) notes.

Method: static reading of `.refs/geant4/source` bespoke geometry code, plus arithmetic
re-derivation of every cutter footprint from the code + `include/GeometryMuonScint.hh`
defaults. No runtime Geant4 log containing `GeomSolids1001` / `GeomVol1002` was
found in the repo (`data_output/`, `*.log`, `slurm/`), so validity is argued
statically, not from a captured geometry-check run.

## Sources

| Source | What it fixes |
|---|---|
| `.refs/geant4/source/geometry/solids/specific/include/G4TessellatedSolid.hh:31-35` | Tessellated solid must be a fully enclosing facet set; only triangular/quadrangular facets. |
| `.refs/geant4/source/geometry/solids/specific/include/G4TessellatedSolid.hh:81-83` | `SetSolidClosed(true)` completes the solid. |
| `.refs/geant4/source/geometry/solids/specific/src/G4TessellatedSolid.cc:591-647` | `SetSolidClosed` → `CreateVertexList`/`SetExtremeFacets`/`Voxelize`/`CheckStructure`; issues `GeomSolids1001` warnings for negative volume / wrong facet orientation / holes. |
| `.refs/geant4/source/geometry/solids/specific/src/G4TessellatedSolid.cc:670-728` | `CheckStructure`: volume sum ⇒ sign; edges must pair with opposite orientation. |
| `.refs/geant4/source/geometry/solids/specific/src/G4TriangularFacet.cc:83-120` | Facet ctor rejects degenerate (too small / too narrow) triangles with `GeomSolids1001` warning and `fIsDefined=false`. |
| `.refs/geant4/source/geometry/solids/Boolean/include/G4SubtractionSolid.hh:30` | `G4SubtractionSolid` = A − B. |
| `.refs/geant4/source/geometry/solids/Boolean/src/G4SubtractionSolid.cc:130-152` | `BoundingLimits` returns **SolidA's** box untrimmed. |
| `.refs/geant4/source/geometry/volumes/include/G4PVPlacement.hh:62` | `pSurfChk if true activates check for overlaps with existing volumes.` |
| `.refs/geant4/source/geometry/volumes/src/G4PVPlacement.cc:61,88,115,141` | Ctor calls `CheckOverlaps()` when `pSurfChk`. |
| `.refs/geant4/source/geometry/volumes/src/G4PVPlacement.cc:243-526` | `CheckOverlaps`: `Sample point is not on the surface !` (:269-275), `Overlap with mother volume !` (:326-341), `Overlap with volume already placed !` (:469-484, :512-526), encapsulation check (:355-451). |
| `.refs/geant4/source/processes/optical/src/G4OpBoundaryProcess.cc:319-341` | `G4LogicalBorderSurface::GetSurface(pre,post)` is tried **first**; `G4LogicalSkinSurface` is only consulted if that returns null. |
| `.refs/geant4/source/geometry/volumes/src/G4LogicalBorderSurface.cc:43-60,102-112` | Border surface is stored/retrieved purely by the `(vol1,vol2)` physical-volume pair — independent of mother/daughter. |

**Not machine-verifiable from `.refs`:** the "avoid coincident/tangent faces in
booleans" rule. `grep -rin "coincident\|tangent" .refs/geant4/source` returns nothing;
`G4BooleanSolid`/`G4SubtractionSolid` perform no such check. It is an
Application-Developer-Guide practical rule. For this audit it was therefore
replaced by a direct arithmetic tangency test on each cutter (below).

---

## 1. Tessellated swept-tube fibres (`G4TessellatedSolid`)

**Our site.** `GeometryMuonScint.cc:731-782` (`MakeSweptTube`): ring normals are
the miter bisector (`:740-748`), rings use a fixed transverse axis
`u=(1,0,0)` (`:750-759`), the side wall is two triangles per (segment, azimuth)
(`:762-770`), flat triangle-fan end caps are wound opposite (`:771-779`), then
`solid->SetSolidClosed(true)` (`:780`). Called for the clad `:904-905`, the core
`:910-911`, the grease grooves `:959-960` and the clamp channels `:1001-1002`
(24 azimuthal segments, `ringSeg=24`, `:872`).

**Geant4 rules.** Must fully enclose (`G4TessellatedSolid.hh:31-33`); only
triangular/quadrangular facets (`:34-35`); completeness declared via
`SetSolidClosed(true)` (`:81-83`), which runs `CheckStructure`
(`G4TessellatedSolid.cc:591-647,670-728`). `CheckStructure` requires
(a) positive signed volume `Σ area·(v0·n̂)` and (b) every edge shared by exactly
two facets with opposite orientation (`:705-706`).

**Analysis.**
- *Watertight + outward orientation:* each ring edge is used by segment `i-1`
  (`k2→k`) and segment `i` (`k→k2`); each quad diagonal is shared by its two
  triangles with opposite order; the fan cap boundary edges pair against the side
  wall. The +y cap fan `(path[n-1], ring[n-1][k], ring[n-1][k2])` yields normal
  `+y`, the −y cap `(path[0], ring[0][k2], ring[0][k])` yields `−y`; the side
  normal (`t×u`) points radially outward. So `CheckStructure` should return 0.
- *Facets non-degenerate:* triangle short edges ≈ `2r·sin(π/24)` ≈ `0.13` mm ≫
  `kCarTolerance` (~1e-9 mm), so the `G4TriangularFacet` ctor guard
  (`G4TriangularFacet.cc:83-120`) is never tripped.
- *No self-intersection / convexity:* `CheckStructure` does **not** test either;
  both are the user's responsibility. Here the geometry is a simple monotone
  swept tube (no self-crossing), and a bent tube is non-convex — allowed, since
  `G4TessellatedSolid` has no convexity requirement.
- *Miter ring is a circle, not an ellipse:* for a true miter joint the ring in
  the bisector plane should have semi-major `r/cos(θ/2)`. A constant-radius ring
  closes the surface exactly (same ring shared) but gives a tiny inward pinch at
  each kink; the bend here is ~2° (1.5→1.0 mm over 15 mm), negligible.

**Verdict: CONSISTENT.** The solids should pass `CheckStructure` (watertight,
outward, non-degenerate). The historical `G4Tubs`+`G4Orb` union failure
(`GeometryMuonScint.cc:718-723`) is a polyhedron-meshing/rendering limit, not a
Geant4 solids rule; replacing it with a tessellated swept tube is the right call.

---

## 2. Boolean cutters on tessellated tools

**Our sites.** Grease film grooves: `GeometryMuonScint.cc:940-965`
(`filmBox − Σ MakeSweptTube(rFibre)`), clamp channels: `:990-1007`
(`clampBox − Σ MakeSweptTube(fChannelRadius)`); each subtraction is chained with
`G4SubtractionSolid(name, solid, tool, nullptr, (0,0,0))` (`:961-963`,
`:1003-1005`). Holder shell: `BuildHolder` `:632-668`.

**Geant4 rules.** `G4SubtractionSolid` = A − B
(`G4SubtractionSolid.hh:30`); no coincident/tangent-face validation exists in
source; `BoundingLimits` conservatively returns A's box
(`G4SubtractionSolid.cc:130-152`).

**Arithmetic tangency / degeneracy test** (slab-local, plate half-extent
`fHalfSize = (125,125,5)` mm, `fClampGap=0.10`, `fFibreRadius=0.5`,
`fChannelRadius=0.55`, `fFibreProtrusion=0.05`, `fClampWallZ=2.0`,
`fFibrePitch=1.5`, `fBundlePitch=1.0`, `fFibreConvergeLen=15`; all from
`GeometryMuonScint.hh:124,132,149-175`):

| Cutter | Object | Contact with object face | Verdict |
|---|---|---|---|
| film groove, `r = rFibre = 0.5` (`:959-960`) | film box half-x `0.05`, centre `filmCx=125.05`; cutter axis `125.55` → local offset `−0.50` | cutter max local x `−0.50+0.50 = 0.00`; slab-side face at `+0.05` ⇒ **0.05 mm clearance**, no tangency | CONSISTENT |
| film groove *if* `r=fChannelRadius=0.55` | same | `−0.50+0.55 = +0.05` = slab-side face ⇒ **exactly tangent** | (rejected; `:947-949` documents this) |
| clamp channel, `r=0.55` (`:1003-1005`) | clamp box half-x `1.0`, front (slab-side) face local `+1.0` | cutter axis local `+0.55`, max `+1.10` ⇒ crosses transversally (0.45 mm from axis to face ≠ 0.55) | CONSISTENT |
| all cutters, axial ends | `cutterOv = 10 mm` (`:871`) extends the tool past `±Ly` | opening bounded by the object's own y-faces, never a tool face | CONSISTENT |
| clamp channels in bundle | `fBundlePitch=1.0 < 2·fChannelRadius=1.1` | channel cylinders **overlap** over the last ~15 mm (merge into a slot) | intended (`:975-977`); see discrepancy D2 |

- The film/clamp/groove solids are built directly in slab-local coordinates and
  the tools are pre-shifted (`:957`, `:999`) with `nullptr` rotation, so no
  operand transforms are involved.
- `BoundingLimits` returning the untrimmed A box is conservative, never wrong.

**Verdict: CONSISTENT / FRAGILE-artifact.** No degenerate/tangent boolean face
by construction; the deliberate coincident **clad↔grease** surface is a
volume-volume contact (fibre placed as a world sibling, not a boolean face), and
the chained subtraction of overlapping bundle channels is a legal union of cut
regions. The usual fragility of booleans on tessellated tools is present but
avoided by the overshoot + non-tangency discipline.

---

## 3. Holder shell boolean

**Our site.** `GeometryMuonScint.cc:653-664`: `cavity = slab + fHolderGap` on all
six faces, `outer = cavity + fHolderWall (2 mm)`, `shell = outer − cavity`.

**Analysis.** Two concentric `G4Box` operands with a 2 mm wall — no coincident or
tangent faces, no tessellated tool. `UsesBlackHolder()` gates this to the bare,
non-WLS configuration so the Config-4 clamp never intersects it (AGENTS.md).

**Verdict: CONSISTENT.**

---

## 4. Mother-volume containment / `pSurfChk`

**Our sites.** Slab `surfCheck=true` (`GeometryMuonScint.cc:1257,1260-1267`);
holder `true` (`:1284-1286`); film/clamp/fibres/caps/pad/SiPM all `true`
(`:1125-1128,1131-1133,1156-1160,1174-1177,1191-1198`); core-in-clad `false`
(`:918-919`).

**Geant4 rule.** `pSurfChk` activates `CheckOverlaps`
(`G4PVPlacement.hh:62`; `G4PVPlacement.cc:61/88/115/141`), which samples surface
points and tests mother containment (`:326-341`) plus every already-placed sister
(`:469-484`), with an encapsulation fallback (`:355-451`).

**Analysis.** The world is a `G4Box` of half-size `10 m` (`DetectorConstruction.cc:74,133`)
and every WLS/holder volume lies well within ±5 m in slab-local coordinates
(fibres at `x≈−125.55`, `y∈[−125,125]`, `z∈[−2.25,2.25]`), so mother containment
holds. Touching sisters (film slab-side face at the slab face, cap/pad against
the fibre ends) register as `kSurface`, not `kOutside`, so no volume overlap is
reported. The core-in-clad placement skips the check (`false`), which is safe by
construction (core radius `rCore=0.485 < rFibre=0.5`, same centreline) but is the
one unverified containment (see D1).

**Verdict: CONSISTENT.**

---

## 5. World-sibling placement + border/skin precedence

**Our site.** Fibres/caps/pad/SiPM are siblings of the slab in the world
(comment `GeometryMuonScint.cc:1139-1146`), not daughters of the clamp. Optical
borders: slab↔film `:1203-1208`, clad↔cap `:1219-1224`, clad↔pad (all four,
`:1233-1240`), pad↔SiPM `:1242-1245`; skins: cap mirror `:1217-1218`, clamp
mirror `:1134-1136`, holder `:1287-1290`.

**Geant4 rules.** A border surface is keyed purely by the `(vol1,vol2)` PV pair
(`G4LogicalBorderSurface.cc:43-60,102-112`), so two world siblings can carry one
regardless of mother. At a boundary, `G4OpBoundaryProcess` uses the border first
and only falls back to a skin when no border matches
(`G4OpBoundaryProcess.cc:319-341`).

**Analysis.** Placing the fibres as world siblings is *required*: they protrude
`fFibreProtrusion` past the clamp mouth into the grease film (`:884-886`) and sit
in a milled void (outside the clamp solid), so as clamp daughters they would fail
mother containment. The slab's mother is the world and so is each fibre's, so the `slab↔film` and `clad↔pad`
borders match the correct PV pair. The `WLSFibreCapMirrorSkin` on the cap LV is
intentionally overridden on the `(clad,cap)` pairs by the explicit borders,
while covering the otherwise surface-less core→cap crossing — consistent with the
precedence rule quoted above.

**Verdict: CONSISTENT.**

---

## 6. Nested core-in-clad solid (coincident end faces)

**Our site.** Core placed at the clad origin (`GeometryMuonScint.cc:918-919`);
both solids are built from the *same* `BentPath` (`:901-902`), so the core's flat
end caps and the clad's flat end caps lie in the **same two planes**, and the
fibre end discs are also coincident with the mirror-cap near face (`:1173`) and
the read-out pad face (`:1185-1193`).

**Analysis.** A daughter sharing a planar face with its mother, and a
triple-coincident fibre/clad/cap plane, is legal but a known navigation-ambiguity
hazard (no volume overlap; `Inside` = `kSurface`). It is intrinsic to a flush
core/clad fibre and works here per the reported `NoRINDEX=0` fix, but it is the
least robust part of the assembly. The core placement uses `pSurfChk=false`, so
this coincidence is not exercised by `CheckOverlaps`.

**Verdict: FRAGILE (working).**

---

## Explicit answers to the three questions

1. **Is our `G4TessellatedSolid` usage valid?** **Yes (CONSISTENT).** Each swept
   tube is a closed, outward-oriented, non-degenerate triangular facet set that
   should pass `CheckStructure`; only triangular facets are used, and
   `SetSolidClosed(true)` is called. The miter uses a circular ring (minor,
   negligible geometric approximation at ~2° bends). Caveat: validity is argued
   statically — no captured `GeomSolids1001`-free run.
2. **Are the boolean cutters free of degenerate/coincident faces?** **Yes
   (CONSISTENT).** Every cutter clears the object's own faces: the film groove
   leaves 0.05 mm to the slab-side face (the `fChannelRadius` variant would be
   exactly tangent and is correctly avoided), the clamp channels cross their
   front face transversally, and all cutters overshoot axially by 10 mm so the
   openings are bounded by the object, not the tool. The only coincidences are
   *volume-volume* contacts (clad↔grease, core/clad/cap end planes), which are
   not boolean-face coincidences.
3. **Is world-sibling placement legal and correctly surfaced?** **Yes
   (CONSISTENT).** World siblings are contained in the 10 m world; border
   surfaces are keyed by PV pair and therefore work across siblings; and the
   border-over-skin precedence at `G4OpBoundaryProcess.cc:319-341` means the
   explicit borders win over the cap/clamp skins exactly as the code assumes.

---

## Top discrepancies (prioritized)

- **D1 — Coincident nested/multi-volume faces (FRAGILE, not caught).**
  Core↔clad and fibre↔clad↔cap end planes are exactly coincident; the core
  placement skips `pSurfChk`, and Geant4's checks do not test navigation
  ambiguity. Legal and reportedly working, but the least robust construct;
  a few-µm end recess or an explicit comment would harden it.
  (`GeometryMuonScint.cc:901-919,1173,1185-1193`.)
- **D2 — AGENTS/report statement "channels must not overlap" is over-general.**
  `fFibrePitch (1.5) > 2·fChannelRadius (1.1)` keeps a web only in the straight
  run; the channels *do* overlap (merge into a slot) over the last ~15 mm because
  `fBundlePitch (1.0) < 1.1`. This is intended (`GeometryMuonScint.cc:975-977`);
  the report already scopes it ("Over most of the length ... a solid web ...
  and converge over the last 15 mm", `report/matrix_report.tex:224-228`), but the
  blanket rule in `AGENTS.md` should be read as straight-run-only.
- **D3 — No runtime geometry-check evidence.** `GeomSolids1001`
  (`G4TessellatedSolid.cc:622-642`) and `GeomVol1002`
  (`G4PVPlacement.cc:269-341`) outputs were neither found nor exercised in
  `data_output/`, logs, or `slurm/`; correctness here rests on static reasoning.
  A short debug run (`/run/initialize` with overlays on) would close the loop.
- **D4 — Miter ring modelled as a circle, not an ellipse (minor).** Slight inward
  pinch at each fibre kink (`GeometryMuonScint.cc:750-759`); ~2° bend ⇒ negligible.
- **D5 — Thin `.refs/geant4/examples` support.** No `G4TessellatedSolid` under
  `.refs/geant4/examples`, and `extended/geometry` only has `transforms` /
  `vecGeomNavigation`; example-practice evidence is limited to the class
  implementation and `extended/optical/{wls,LXe}`.
