# Geant4 optical boundary process — reference

Read on demand from `AGENTS.md`.

**G4OpBoundaryProcess ground truth** (`.refs/geant4/source/processes/optical`): a
`G4LogicalBorderSurface` overrides a `G4LogicalSkinSurface` at the same boundary;
`RINDEX` is required on **material-1 always** (`G4OpBoundaryProcess.cc:288-306`) and on
material-2 only for a `polished`/`ground` `dielectric_dielectric` boundary (`:427-454`,
else `NoRINDEX` kill); `*backpainted` finishes instead need the **surface** backing `RINDEX`
(`:356-374`), and `*frontpainted` needs neither. `polished`/`ground`
dielectric_dielectric → Fresnel/TIR; `groundfrontpainted` → rand-vs-R (Lambertian, no
TIR); `polishedbackpainted`/`groundbackpainted` → dielectric + backing REFLECTIVITY; no
surface object → lossless TIR default. `sigma_alpha` on `groundbackpainted` **does** reach
the TIR test — the smeared facet normal sets the incidence cosine (`:1124-1137`) — so it is
inert only when all three unified constants are zero (pure Lambertian, as in our Teflon).
**A `NoRINDEX` kill is silent** (prints only at verbose>1, `G4OpBoundaryProcess.cc:448`),
so its absence in a normal run log proves nothing — count it with
`/process/optical/boundary/verbose 2` or via the `opticalStats` budget.
