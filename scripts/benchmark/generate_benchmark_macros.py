#!/usr/bin/env python3

from pathlib import Path
import math

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "macros" / "benchmark"

ANGLES = list(range(0, 360, 15))
HEIGHTS = [
    (430, "CLYC-2"), (630, "CLYC-1"),
    (780, "CLYC-2"), (980, "CLYC-1"),
    (1180, "CLYC-2"), (1380, "CLYC-1"),
    (1580, "CLYC-2"), (1780, "CLYC-1"),
    (1980, "CLYC-2"), (2180, "CLYC-1"),
    (2380, "CLYC-2"), (2580, "CLYC-1"),
    (2780, "CLYC-2"), (2980, "CLYC-1"),
    (3180, "CLYC-2"), (3380, "CLYC-1"),
]

FILES = {
    ("cask1", "gamma"): ("/home/turko46/data/wti/Cask1/Cask1_gamma.wssa", 22, 405872062),
    ("cask1", "neutron"): ("/home/turko46/data/wti/Cask1/Cask1_neutrons.wssa", 2112, 491077633),
    ("cask2", "gamma"): ("/home/turko46/data/wti/Cask2/Cask2_gamma.wssa", 22, 404226959),
    ("cask2", "neutron"): ("/home/turko46/data/wti/Cask2/Cask2_neutrons.wssa", 2112, 564324097),
    ("cask3", "gamma"): ("/home/turko46/data/wti/Cask3/Cask3_gamma.wssa", 22, 465772433),
    ("cask3", "neutron"): ("/home/turko46/data/wti/Cask3/Cask3_neutrons.wssa", 2112, 118939773),
}

SEEDS = {
    ("cask1", "gamma"): (1101, 1102),
    ("cask1", "neutron"): (1201, 1202),
    ("cask2", "gamma"): (2101, 2102),
    ("cask2", "neutron"): (2201, 2202),
    ("cask3", "gamma"): (3101, 3102),
    ("cask3", "neutron"): (3201, 3202),
}


def detector_block() -> str:
    lines = [
        "# CLYC detector defaults",
        "/dcs-monitor/det/clyc/setCrystalMaterial CLYC",
        "/dcs-monitor/det/clyc/setCrystalRadius 12.5 mm",
        "/dcs-monitor/det/clyc/setCrystalLength 25.0 mm",
        "/dcs-monitor/det/clyc/setCasingMaterial G4_Al",
        "/dcs-monitor/det/clyc/setCasingThickness 0.5 mm",
        "/dcs-monitor/det/clyc/setPbColMaterial G4_Pb",
        "/dcs-monitor/det/clyc/setPbColLength 50.0 mm",
        "/dcs-monitor/det/clyc/setPEColMaterial G4_POLYETHYLENE",
        "/dcs-monitor/det/clyc/setPEColLength 50.0 mm",
        "/dcs-monitor/det/clyc/setPEPlugLipLength 10.0 mm",
        "/dcs-monitor/det/clyc/setPEPlugInnerLength 35.0 mm",
        "",
    ]
    for height, sensor in HEIGHTS:
        lines.append(f"# sensor {sensor}, height {height} mm")
        for angle in ANGLES:
            x = 1350.0 * math.sin(math.radians(angle))
            y = 1350.0 * math.cos(math.radians(angle))
            z = height - 2040.0
            lines.extend([
                f"/dcs-monitor/det/setPosition {x:.6f} {y:.6f} {z:.6f} mm",
                f"/dcs-monitor/det/setRotation 0 -90 {90 - angle}",
                f"/dcs-monitor/det/meta/setScanPhi {angle}",
                f"/dcs-monitor/det/meta/setScanZ {height} mm",
                "/dcs-monitor/det/clyc/add",
            ])
        lines.append("")
    return "\n".join(lines)


def macro_text(cask: str, particle: str) -> str:
    path, pid, nps = FILES[(cask, particle)]
    tag = f"{cask}_{particle}"
    lines = [
        f"# WSSA benchmark: {tag}, 384 CLYC detector positions",
        "/control/verbose 0",
        "/run/verbose 0",
        "/tracking/verbose 0",
        "/event/verbose 0",
        "/process/verbose 0",
        "",
        "/run/numberOfThreads 32",
        "",
        "/dcs-monitor/det/setPosition 0 0 0 mm",
        "/dcs-monitor/det/setRotation 0 0 0",
        "/dcs-monitor/det/castor440/setTrunnions false",
        "/dcs-monitor/det/castor440/add",
        "",
        detector_block(),
        "/run/initialize",
        "",
        f"/random/setSeeds {SEEDS[(cask, particle)][0]} {SEEDS[(cask, particle)][1]}",
        "",
        f"/dcs-monitor/surf/wssaFile {path}",
        f"/dcs-monitor/surf/wssaPid {pid}",
        "/dcs-monitor/surf/wssaMaxEntries 0",
        "/dcs-monitor/surf/caskNum 0",
        "/dcs-monitor/gun/sourceMode CASTOR440_surface_from_WSSA",
        "",
        "/dcs-monitor/run/writePrimary false",
        "/dcs-monitor/run/writePrimaryOnlyOnHit false",
        "/dcs-monitor/run/writeSurfaceFlux false",
        "/dcs-monitor/run/writeFluxMap false",
        "",
        f"/analysis/setFileName benchmark_{tag}",
        "",
        f"/run/beamOn {nps}",
        "",
    ]
    return "\n".join(lines)


def smoke_text() -> str:
    lines = [
        "# Geometry smoke test: 384 CLYC positions, no WSSA replay",
        "/run/numberOfThreads 1",
        "/dcs-monitor/det/setPosition 0 0 0 mm",
        "/dcs-monitor/det/setRotation 0 0 0",
        "/dcs-monitor/det/castor440/setTrunnions false",
        "/dcs-monitor/det/castor440/add",
        "",
        detector_block(),
        "/run/initialize",
        "",
        "/dcs-monitor/gun/sourceMode GPS",
        "/gps/particle gamma",
        "/gps/ene/mono 0.662 MeV",
        "/gps/pos/type Point",
        "/gps/pos/centre 0 0 0 mm",
        "/gps/ang/type iso",
        "",
        "/dcs-monitor/run/writePrimary false",
        "/dcs-monitor/run/writeSurfaceFlux false",
        "/dcs-monitor/run/writeFluxMap false",
        "",
        "/analysis/setFileName smoke_geometry",
        "",
        "/run/beamOn 1",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for (cask, particle) in FILES:
        (OUT / f"run_{cask}_{particle}.mac").write_text(macro_text(cask, particle))
    (OUT / "smoke_geometry.mac").write_text(smoke_text())
    print(f"wrote {len(FILES)} benchmark macros and smoke_geometry.mac to {OUT}")


if __name__ == "__main__":
    main()
