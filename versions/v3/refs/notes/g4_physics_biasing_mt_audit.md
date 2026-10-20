# Geant4 physics-list / optical / importance-biasing / multithreading audit

Repo: `/home/turko46/geant4-sims/G4DCSmonitor`
Reference tree: `.refs/{source,examples,include}` (read-only, symlinks to Geant4
v11.3.2 install/source/examples; `.refs/source` is a symlink so `find` needs `-L`).

This is a **read-only** consistency audit: our code was inspected against the
Geant4 source/example tree. `.refs/` was not modified. Only this Markdown file
was written.

Verdict vocabulary: **CONSISTENT** (matches API/example intent), **INCONSISTENT**
(violates API/example contract), **DEAD-CODE** (compiled/declared but unreachable
or never constructed), **RISKY** (works today but outside the documented contract
or fragile under MT / future Geant4).

---

## 0. Executive answers

1. **Is the importance-biasing stack currently functional or dead code?**
   **DEAD CODE.** Both trigger points are commented out: the parallel world is
   never registered (`src/DetectorConstruction.cc:81`) and neither
   `G4ParallelWorldPhysics` nor `G4ImportanceBiasing` is registered
   (`src/PhysicsList.cc:122-133`). `G4DCSmonitor.cc:113` still passes
   `useImportanceBiasing=true`, so the post-`ConstructProcess` cleanup block
   (`src/PhysicsList.cc:262-290`) *does execute*, but it only searches for a
   process named `"BiasingWorld"` on the optical photon; none exists, so it is a
   no-op. `fGammaSampler`/`fNeutronSampler` are always `nullptr`.
   `GeometryParallelBiasing` is compiled (CMake globs `src/*.cc`) but never
   instantiated. `/dcs-monitor/det/useBiasing` and the other `setBiasing*`
   commands mutate detector state that nothing reads.

2. **Is the optical activation sequence correct?**
   **Mostly CONSISTENT, one RISKY detail.** The five `SetProcessActivation`
   calls in the constructor are in PreInit and therefore valid; the WLS
   auto-enable in `ConstructProcess` is applied at `G4State_Init`, which the
   runtime `IsLocked()` check permits even though the API comment says
   "PreInit> phase only". See §2.

3. **Are the `thread_local` patterns safe under MT?**
   **Yes, in the intended design.** All per-event mutable state is
   `G4ThreadLocal`/`thread_local` (`gOpticalDiag`, `RootManager` thread-local
   clones, hit allocators); SD members are per-thread because
   `ConstructSDandField()` runs on every worker; analysis uses the thread-local
   `G4AnalysisManager` singleton. The only MT-risky items are the
   dead `RootManager` sampling path and the fact that the parallel-world
   importance store (`G4IStore`) is `G4ThreadLocal` and populated only on the
   master (relevant only if biasing is revived). See §4.

---

## 1. Physics list: modular usage and registration order

### 1.1 `G4VModularPhysicsList` base usage
- **(a) Ours:** `include/PhysicsList.hh:41` `class PhysicsList : public G4VModularPhysicsList`;
  `src/PhysicsList.cc:92-174` ctor calls `RegisterPhysics(...)`; overrides
  `ConstructParticle`/`ConstructProcess`/`SetCuts`.
- **(b) G4:** `G4VModularPhysicsList::ConstructProcess()` calls `AddTransportation()`
  then `(*itr)->ConstructProcess()` for every registered constructor
  (`.refs/source/run/src/G4VModularPhysicsList.cc:134-142`): *"G4AutoLock l(&constructProcessMutex);
  AddTransportation(); for(auto itr...)(*itr)->ConstructProcess();"*.
  `PhysicsList::ConstructProcess()` correctly calls
  `G4VModularPhysicsList::ConstructProcess()` (`src/PhysicsList.cc:260`).
- **(c) Verdict:** CONSISTENT.
- **(d) Notes:** `ConstructParticle()` duplicates what Geant4's particle
  constructors do for a modular list (modular lists usually rely on each
  `G4VPhysicsConstructor::ConstructParticle`), but it is harmless and matches
  the old Hadr03 template header. No action needed.

### 1.2 `SetCuts` and per-region cuts
- **(a) Ours:** `src/PhysicsList.cc:209-240` sets global cuts with `SetCutValue`
  and installs `G4ProductionCuts` on `"CLYCRegion"` / `"MuonScintRegion"` —
  after `G4VModularPhysicsList`'s base `SetCuts()` (base is called by the kernel,
  not chained here, which is fine: the kernel calls `SetCuts()` once —
  `.refs/source/run/src/G4RunManagerKernel.cc:580-588`).
- **(b) G4:** `G4RunManagerKernel::InitializePhysics()` calls
  `physicsList->SetCuts()` only on the master thread
  (`.refs/source/run/src/G4RunManagerKernel.cc:586-588`).
- **(c) Verdict:** CONSISTENT.
- **(d) Notes:** `GetRegion(..., false)` returns `nullptr` if the region is not
  yet built; the guarded `if (region)` keeps it safe.

---

## 2. Optical physics: `G4OpticalPhysics` + `G4OpticalParameters` activation

### 2.1 Activation flags set in the constructor
- **(a) Ours:** `src/PhysicsList.cc:139-170`:
  `optParams->SetProcessActivation("Cerenkov", true)`, `"Scintillation"`,
  `"OpAbsorption"`, `"OpRayleigh"`, `"OpBoundary"` true; `"OpMieHG"`,
  `"OpWLS"`, `"OpWLS2"` false; then `RegisterPhysics(new G4OpticalPhysics())`.
- **(b) G4:** `G4OpticalPhysics::ConstructProcess()` reads the flags and adds
  only the activated processes
  (`.refs/source/physics_lists/constructors/electromagnetic/src/G4OpticalPhysics.cc:102-124`,
  e.g. `if(params->GetProcessActivation("OpWLS")) pManager->AddDiscreteProcess(wls);`).
  `SetProcessActivation` is allowed while not locked; `IsLocked()` is false in
  `PreInit/Init/Idle` on the master
  (`.refs/source/processes/electromagnetic/utils/src/G4OpticalParameters.cc:161-189`,
  `:551-557`).
- **(c) Verdict:** CONSISTENT.
- **(d) Notes:** Defaults for `OpMieHG/OpWLS/OpWLS2` are **true**
  (`.refs/source/processes/electromagnetic/utils/src/G4OpticalParameters.cc:132-139`),
  so our three `false` calls are meaningful and correctly override the defaults.

### 2.2 WLS auto-enable inside `ConstructProcess`
- **(a) Ours:** `src/PhysicsList.cc:250-260`:
  `if (G4OpticalParameters::Instance() && GeometryMuonScint::UsesWLSWorld())
  G4OpticalParameters::Instance()->SetProcessActivation("OpWLS", true);`
  immediately before `G4VModularPhysicsList::ConstructProcess()`.
- **(b) G4:** `SetProcessActivation`'s comment states the method *"can only be
  called in PreInit> phase (before execution of ConstructProcess)"*
  (`G4OpticalParameters.cc:164-168`), but the enforcement is the runtime
  `IsLocked()` = `!IsMasterThread() || state ∉ {PreInit,Init,Idle}`
  (`G4OpticalParameters.cc:551-557`). Our call runs during `InitializePhysics`
  at `G4State_Init` on the master (`.refs/source/run/src/G4RunManager.cc:625-627`
  sets `G4State_Init`, then `:653` geometry, then `:669-684` physics), so the
  setter takes effect before `G4OpticalPhysics::ConstructProcess` reads it.
- **(c) Verdict:** **RISKY** (functional but outside the documented contract).
- **(d) Fix/notes:** Prefer setting activation flags in a code path executed in
  PreInit, or have `DetectorConstruction::Construct()` publish the WLS flag and
  an explicit `G4OpticalParameters` update driven by the geometry step. The
  current trick is correct for v11.3.2 but fragile if Geant4 tightens
  `IsLocked()` to PreInit-only, and it silently no-ops on worker threads
  (`IsLocked()` true → early return; the master already set the shared
  singleton, so behaviour is preserved by accident). Also relies on geometry
  being built before physics (true: `InitializeGeometry` precedes
  `InitializePhysics`).
  `sHasWLSFibres` is a plain `static G4bool`
  (`include/GeometryMuonScint.hh:100,201`; set in `src/GeometryMuonScint.cc:1070`),
  written once by the master during geometry build; workers only read it → no
  race.

### 2.3 `SetBoundaryInvokeSD`
- **(a) Ours:** `src/PhysicsList.cc:158` `optParams->SetBoundaryInvokeSD(true)`.
- **(b) G4:** parameter exists and defaults to `false`
  (`G4OpticalParameters.cc:129`); already used by the WLS/SiPM detection model
  documented in `AGENTS.md`.
- **(c) Verdict:** CONSISTENT.

---

## 3. Importance biasing stack

### 3.1 Parallel world registration
- **(a) Ours:** `src/DetectorConstruction.cc:79-81`:
  `//RegisterParallelWorld(new GeometryParallelBiasing("BiasingWorld", this));`
  (commented, inside the ctor).
- **(b) G4:** registration must happen before initialization;
  `G4RunManager::InitializeGeometry()` calls
  `nParallelWorlds = userDetector->ConstructParallelGeometries()`
  (`.refs/source/run/src/G4RunManager.cc:653`), which invokes each registered
  `G4VUserParallelWorld::Construct()`
  (`.refs/source/run/src/G4VUserDetectorConstruction.cc:62-70`). Calling
  `RegisterParallelWorld` after initialization would register a world that is
  never constructed. The ctor is the right place.
- **(c) Verdict:** DEAD-CODE (commented out). If restored, placement is
  CONSISTENT.
- **(d) Notes:** `AGENTS.md` already documents the disabling; consistent with
  the code.

### 3.2 Physics-list registration of the biasing constructors
- **(a) Ours:** `src/PhysicsList.cc:122-133` (commented):
  `G4ParallelWorldPhysics("BiasingWorld")`; two samplers
  `G4GeometrySampler("BiasingWorld","gamma")` / `"neutron"`, each
  `SetParallel(true)`; `G4ImportanceBiasing(sampler, "BiasingWorld")` for both.
- **(b) G4/examples:**
  - `B02/exampleB02.cc:98-110` uses exactly the parallel-world pattern:
    `G4GeometrySampler pgs(pdet->GetWorldVolume(), "neutron"); pgs.SetParallel(true);
    physicsList->RegisterPhysics(new G4ImportanceBiasing(&pgs, parallelName));
    physicsList->RegisterPhysics(new G4ParallelWorldPhysics(parallelName));`
    — note **`G4ImportanceBiasing` is registered *before*
    `G4ParallelWorldPhysics`**.
  - `B01/exampleB01.cc:103-108` uses the mass-geometry variant
    (`G4ImportanceBiasing(&mgs)` with no parallel name).
  - A sampler is **per particle**: `G4GeometrySampler` stores a single
    `G4String fParticleName`
    (`.refs/source/processes/biasing/importance/include/G4GeometrySampler.hh:86`)
    and `PrepareImportanceSampling` builds
    `G4ImportanceConfigurator(&istore->GetWorldVolume(), fParticleName, ...)`
    (`.refs/source/processes/biasing/importance/src/G4GeometrySampler.cc:111-114`),
    whose constructor builds `fPlacer(particlename)`
    (`.refs/source/processes/biasing/importance/src/G4ImportanceConfigurator.cc:54`)
    and `G4ProcessPlacer` is *"create a process placer for a particle type"*
    (`.refs/source/processes/biasing/management/include/G4ProcessPlacer.hh:50`).
- **(c) Verdict:** DEAD-CODE. If revived, the *two separate samplers* approach is
  **CONSISTENT** (a single sampler cannot fan out to `gamma`+`neutron`; a
  `SetParticle()`+re-`Prepare` per particle would be needed otherwise). The
  **registration order differs from the canonical B02 example** (we register
  `G4ParallelWorldPhysics` first, B02 registers it second): **RISKY/minor**.
- **(d) Fix/notes:** When reviving, follow B02 order:
  `RegisterPhysics(new G4ImportanceBiasing(sampler, name))` then
  `RegisterPhysics(new G4ParallelWorldPhysics(name))`. The ordering is not a
  hard requirement here (the parallel world geometry exists before
  `InitializePhysics` regardless), but matching the example avoids subtle
  process-ordering surprises. `G4ParallelWorldPhysics` must also be present for
  the importance process to navigate the parallel geometry.

### 3.3 Post-`ConstructProcess` "remove/deactivate parallel-world process" block
- **(a) Ours:** `src/PhysicsList.cc:262-290` searches the optical photon's
  process list for a process named `"BiasingWorld"` and, if found, calls
  `pmgr->SetProcessActivation(pwProc, false)`. The preceding commented block
  proposes `RemoveProcess`.
- **(b) G4:** `G4ParallelWorldPhysics::ConstructProcess` adds a
  `G4ParallelWorldProcess(namePhysics)` to **every** particle's process manager
  with `SetProcessOrderingToSecond(..., idxAlongStep)` and
  `SetProcessOrdering(..., idxPostStep, 9900)`
  (`.refs/source/physics_lists/constructors/limiters/src/G4ParallelWorldPhysics.cc:79-93`).
  Ordering semantics: lower ordering value is invoked earlier
  (`G4ProcessManager::FindInsertPosition` inserts before the first process whose
  ordering is `> ord`, `.refs/source/processes/management/src/G4ProcessManager.cc:383-405`;
  `ordLast = 9999`, `.refs/source/processes/management/include/G4ProcessManager.hh:87-91`).
  Therefore post-step = 9900 is invoked **last**, i.e. *after* `OpBoundary`
  (default `ordDefault = 1000`, `G4ProcessManager.hh:90`). The code comment
  *"was priority 9900, blocking OpBoundary"* (`src/PhysicsList.cc:285-287`) is
  **misleading**: 9900 does not put it ahead of `OpBoundary`.
- **(c) Verdict:** DEAD-CODE (no `"BiasingWorld"` process is ever created, so
  `pwProc` is always `nullptr`).
- **(d) Fix/notes:** If biasing is revived, verify empirically (with
  `/process/optical/boundary/verbose 2` and the optical budget) whether the
  parallel world process actually perturbs optical tracking before disabling it.
  The stated rationale is not supported by the ordering numbers. Also note the
  `G4VProcess::GetProcessName()` string match does correspond to the constructed
  name (`"BiasingWorld"`), so the lookup itself is correct.

### 3.4 `GeometryParallelBiasing` implementation
- **(a) Ours:** `src/GeometryParallelBiasing.cc:18-97`; header
  `include/GeometryParallelBiasing.hh:9`.
- **(b) G4:** `G4VUserParallelWorld` derived classes must *not* create the world
  and must obtain it via `GetWorld()`, which returns the framework-created
  parallel world
  (`.refs/source/run/include/G4VUserParallelWorld.hh:30-34,58-59`;
  `.refs/source/run/src/G4VUserParallelWorld.cc:47-53`). B02 builds nested
  volumes in the ghost world with `dummyMat = 0` (nullptr material) and fills the
  store itself
  (`.refs/examples/extended/biasing/B02/src/B02ImportanceDetectorConstruction.cc:81,107,312-356`).
  `G4IStore::AddImportanceGeometryCell` overloads match
  (`imp, const G4VPhysicalVolume&, G4int aRepNum=0`)
  (`.refs/source/geometry/biasing/include/G4IStore.hh:80-89`).
- **(c) Verdict:** DEAD-CODE; structurally **CONSISTENT** with B02 (nullptr
  material, nested tubs, `G4PVPlacement`, store population).
- **(d) Notes / latent issues if revived:**
  - Copy numbers `c*1000+i` and replica numbers look unique; nested shells
    (`T_{i-1}` radius & half-height strictly smaller than `T_i`) do not overlap;
    the core (`imp=1`) is inside `T_1` — geometry logic is sound.
  - Populating the store **inside `Construct()`** rather than in a
    `CreateImportanceStore()` called after `Initialize()` (as B02 does) still
    works because `Construct()` runs on the master before physics, and the
    master's `G4IStore` is the object later referenced by the configurator
    (see §4). It does, however, rely on the master-thread-first ordering.
  - `G4IStore` is `static G4ThreadLocal` (`.refs/source/geometry/biasing/include/G4IStore.hh:117`;
    `.refs/source/geometry/biasing/src/G4IStore.cc:49`). Only the master builds
    the parallel geometry, so the store is populated on the master. The
    `G4GeometrySampler`/`G4ImportanceConfigurator` capture a reference to that
    store object once (guarded by `static G4bool first` in
    `G4ImportanceBiasing::ConstructProcess`,
    `.refs/source/physics_lists/constructors/limiters/src/G4ImportanceBiasing.cc:90-104`),
    so worker processes read the master's store. This should work but is subtle;
    if biasing is revived, verify per-thread behaviour explicitly.

### 3.5 Dead messenger commands
- **(a) Ours:** `src/DetectorMessenger.cc:58-84,305-314` (`useBiasing`,
  `setBiasingShells`, `setBiasingInner/OuterRadius`, `setBiasingInnerHeight`);
  getters/setters `include/DetectorConstruction.hh:32-43,133-134`.
- **(b) G4:** commands are harmless but only matter if a parallel world reads
  the flags.
- **(c) Verdict:** DEAD-CODE (no consumer).
- **(d) Notes:** `AGENTS.md` documents this; keep or gate behind the same revive
  decision as §3.1-3.3.

---

## 4. Multithreading / shared state

### 4.1 Sensitive detector members
- **(a) Ours:** `include/MuonScintSiPMSD.hh:63-71`
  (`fHitsCollection`, `fSiPMHitIndexMap` plain members);
  `include/DCSMonitorHit.hh`/`MuonScintHit.hh` use
  `G4ThreadLocal ... Allocator`.
- **(b) G4:** every worker calls `userDetector->ConstructSDandField()`
  (`.refs/source/run/src/G4WorkerRunManager.cc:164-166`), which creates SDs in
  the **thread-local** `G4SDManager`
  (`.refs/source/digits_hits/detector/src/G4SDManager.cc:37`,
  `fSDManager` is `G4ThreadLocal`). Our `ConstructSDandField` creates new SD
  objects each call (`src/DetectorConstruction.cc:201-231`).
- **(c) Verdict:** CONSISTENT / safe. Each thread owns its `MuonScintSiPMSD`
  instance, so the non-atomic members are per-thread.
- **(d) Notes:** `fDetector` points at the shared `DetectorConstruction`
  (read-only use) and `postLV == ms->GetSiPMLV()` compares shared geometry
  pointers — safe.

### 4.2 Per-event optical diagnostics
- **(a) Ours:** `include/OpticalDiagnostics.hh:58`
  `inline G4ThreadLocal OpticalDiag gOpticalDiag;`; consumed in
  `src/MuonScintSiPMSD.cc` and `src/SteppingAction.cc`.
- **(b) G4:** thread-local diagnostic state is the correct MT pattern
  (compare `G4ParallelWorldProcess`'s `G4ThreadLocal` step pointer,
  `.refs/source/processes/scoring/src/G4ParallelWorldProcess.cc:50-52`).
- **(c) Verdict:** CONSISTENT / safe (C++17 inline variable + `thread_local`).
- **(d) Notes:** SD `EndOfEvent` ordering before `EventAction::EndOfEventAction`
  is already documented in `AGENTS.md` and matches
  `.refs/source/event/src/G4EventManager.cc:315` vs `:323`.

### 4.3 Analysis output
- **(a) Ours:** `src/MuonScintSiPMSD.cc:197-270` uses
  `G4AnalysisManager::Instance()`; `src/HistoManager.cc` books output.
- **(b) G4:** `G4AnalysisManager` = `G4GenericAnalysisManager`, a
  `G4ThreadLocalSingleton` (`.refs/source/analysis/factory/include/G4GenericAnalysisManager.hh:49,58`).
- **(c) Verdict:** CONSISTENT / safe (per-thread analysis manager, master merge).

### 4.4 `RootManager` singleton
- **(a) Ours:** `include/RootManager.hh:12-46` (function-local-static
  singleton, `std::atomic<bool> fInitialized`, `std::mutex fMutex`,
  `static thread_local unique_ptr` members); `src/RootManager.cc:7-8,19-30,32-123`.
- **(b) G4:** no direct analogue, but the pattern is conventional.
- **(c) Verdict:** **RISKY (and the sampling half is DEAD-CODE).**
  - `SampleEvent()` is never called anywhere (grep: only its own definition).
  - `Initialize()` calls are commented out in `G4DCSmonitor.cc:81-87`, so
    `RootManager` is used only for `GetFileNum()`
    (`src/HistoManager.cc:69-72`).
- **(d) Notes:** If revived under MT, the design is largely safe (lazy
  thread-local clone under `fMutex`, atomic `fInitialized`), but:
  `Cleanup()` deletes only `fMasterSparse`; thread-local clones are freed at
  thread exit, which is fine, but a `Cleanup()` on the master while workers still
  sample would be unsafe — only call it after the run manager is destroyed (it is,
  `G4DCSmonitor.cc:149-151`). No change needed for the current (file-num-only)
  usage.

### 4.5 Plain static WLS flag
- **(a) Ours:** `include/GeometryMuonScint.hh:100,201`,
  `src/GeometryMuonScint.cc:45,1070`.
- **(b) G4:** geometry is built on the master; workers receive the shared world.
- **(c) Verdict:** CONSISTENT (write-once-before-workers, read-only after).
- **(d) Notes:** Would become RISKY if geometry were rebuilt per thread after
  workers start; not the case here.

---

## 5. Prioritized top discrepancies

| # | Severity | Item | Where | Verdict |
|---|----------|------|-------|---------|
| 1 | High | Importance biasing fully disabled: parallel world + `G4ParallelWorldPhysics` + `G4ImportanceBiasing` all commented; samplers always `nullptr`; messenger flags have no consumer | `DetectorConstruction.cc:81`, `PhysicsList.cc:122-133`, `G4DCSmonitor.cc:113`, `DetectorMessenger.cc:58-84` | DEAD-CODE |
| 2 | Medium | WLS auto-enable calls `SetProcessActivation` during `ConstructProcess` (Init), outside the documented PreInit-only contract | `PhysicsList.cc:255-256` vs `G4OpticalParameters.cc:164-168,551-557` | RISKY |
| 3 | Medium | Deactivation rationale for the parallel-world process is wrong (post-step 9900 is *last*, not ahead of `OpBoundary`); the code is a no-op today | `PhysicsList.cc:283-288` vs `G4ProcessManager.cc:383-405` | DEAD-CODE + misleading comment |
| 4 | Low | Revived biasing should register `G4ImportanceBiasing` before `G4ParallelWorldPhysics` (B02 order); our commented block is reversed | `PhysicsList.cc:124-133` vs `B02/exampleB02.cc:106-108` | RISKY |
| 5 | Low | `GeometryParallelBiasing` populates the `G4ThreadLocal` `G4IStore` from master-only `Construct()`; correct today but subtle under MT | `GeometryParallelBiasing.cc:21-96`, `G4IStore.hh:117` | RISKY (if revived) |
| 6 | Low | `RootManager` sampling (`Initialize`/`SampleEvent`) is dead; singleton retained only for `fileNum` | `RootManager.cc:32-123`, `G4DCSmonitor.cc:81-87` | DEAD-CODE |
| 7 | Info | `ConstructParticle()` redundantly constructs all particle families for a modular list | `PhysicsList.cc:186-205` | CONSISTENT (harmless) |

---

## 6. What is consistent and should not be "fixed"

- Optical process defaults correctly overridden; `G4OpticalPhysics` registered
  last in the ctor (`PhysicsList.cc:139-170`).
- Parallel-world registration (if uncommented) is placed in the
  `DetectorConstruction` ctor, i.e. before `runManager->Initialize()`
  (`DetectorConstruction.cc:81`) — matches
  `.refs/source/run/src/G4RunManager.cc:653` and B02.
- Two separate `G4GeometrySampler`s for `gamma` and `neutron`
  (`PhysicsList.cc:126,130`) — required, since a sampler targets one particle.
- `thread_local`/`G4ThreadLocal` usage is the correct MT idiom; SDs are
  constructed per worker; `G4AnalysisManager` is thread-local.
- No `RINDEX`/material misuse relevant to this audit; see the separate
  `sipm_wls_audit.md` for boundary-surface details.

---

## 7. Suggested minimal actions (read-only audit recommendations)

1. Either **delete** the biasing scaffolding (samplers, `GeometryParallelBiasing`,
   messenger commands, the `fUseImportanceBiasing` plumbing and the
   `ConstructProcess` scan) or **clearly re-enable** it following
   `B02/exampleB02.cc` (register the parallel world in the ctor; register
   `G4ImportanceBiasing` then `G4ParallelWorldPhysics`), and update `AGENTS.md`.
2. Move the WLS `SetProcessActivation("OpWLS", true)` out of the Init-state
   window, or add a comment documenting the reliance on `IsLocked()` accepting
   `G4State_Init`, and re-test after any Geant4 upgrade.
3. Correct the `PhysicsList.cc:285-287` comment: ordering 9900 is invoked last.
4. If `RootManager` sampling stays unused, drop the phase-space half and keep
   only `SetFileNum`/`GetFileNum`, or document it as a dormant facility.

*Generated by a read-only audit; no file under `.refs/` or any source file was
modified. Only this report was written.*
