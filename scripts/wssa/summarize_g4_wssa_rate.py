#!/usr/bin/env python3
"""Summarise the p1 surface-weight sums and compare with the WSSA rates.

Inputs, in --indir:
  neutron_Watt.csv, gamma_662keV.csv, gamma_1173keV.csv,
  gamma_1332keV.csv, isotope_Eu154.csv
  Each is written by compare_g4_wssa_rate.C.

The WSSA sum-of-weights values are read from
  --wssa-summary report/wti-binary-reader/data/full_validation_summary.csv

Outputs, in --outdir:
  g4_p1_surface_rates.csv
  g4_p1_source_summary.csv
  g4_wssa_source_rate_comparison.csv
"""

import argparse
import csv
import os

SOURCES = [
    "neutron_Watt",
    "gamma_662keV",
    "gamma_1173keV",
    "gamma_1332keV",
    "isotope_Eu154",
]

# Estimated source rate per assembly, from slurm/generate_p15_macros.sh.
# These are estimates, not an inventory calculation.
LAMBDA = {
    "neutron_Watt": 4.16e6,
    "gamma_662keV": 1.4e14,
    "gamma_1173keV": 1.0e11,
    "gamma_1332keV": 1.0e11,
    "isotope_Eu154": 7.0e10,
}

N_ASSEMBLIES = 84  # 14 base fuels x 6 hexants

# Number of MCNP source histories in each WSSA file, from the header |np|
# (verified with include/WSSAReader.hh: last record nps equals |np|).
WSSA_NP = {
    ("C1", "gamma"): 1e11,
    ("C1", "neutron"): 1e8,
    ("C2", "gamma"): 1e11,
    ("C2", "neutron"): 1e8,
    ("C3", "gamma"): 5e11,
    ("C3", "neutron"): 1e8,
}


def read_source(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            if r["entries"] == "ERROR":
                continue
            rows.append(
                {
                    "source": r["source"],
                    "base": int(r["base"]),
                    "nEvts": int(r["nEvts"]),
                    "lambda": float(r["lambda"]),
                    "path": r["path"],
                    "entries": int(r["entries"]),
                    "sumW_gamma": float(r["sumW_gamma"]),
                    "sumW_neutron": float(r["sumW_neutron"]),
                    "r_gamma": float(r["r_gamma"]),
                    "r_neutron": float(r["r_neutron"]),
                }
            )
    return rows


def read_wssa(path):
    out = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            tag = r["tag"]
            if tag == "Cask1_gamma":
                out[("C1", "gamma")] = float(r["sumW"])
            elif tag == "Cask1_neutrons":
                out[("C1", "neutron")] = float(r["sumW"])
            elif tag == "Cask2_gamma":
                out[("C2", "gamma")] = float(r["sumW"])
            elif tag == "Cask2_neutrons":
                out[("C2", "neutron")] = float(r["sumW"])
            elif tag == "Cask3_gamma":
                out[("C3", "gamma")] = float(r["sumW"])
            elif tag == "Cask3_neutrons":
                out[("C3", "neutron")] = float(r["sumW"])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--indir", default="/tmp/opencode/g4rate")
    ap.add_argument(
        "--wssa-summary",
        default="report/wti-binary-reader/data/full_validation_summary.csv",
    )
    ap.add_argument("--outdir", default="report/wti-binary-reader/data")
    args = ap.parse_args()

    all_rows = []
    for s in SOURCES:
        all_rows.extend(read_source(os.path.join(args.indir, s + ".csv")))

    os.makedirs(args.outdir, exist_ok=True)

    with open(os.path.join(args.outdir, "g4_p1_surface_rates.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "source",
                "base",
                "nEvts",
                "lambda",
                "entries",
                "sumW_gamma",
                "sumW_neutron",
                "r_gamma",
                "r_neutron",
                "path",
            ]
        )
        for r in all_rows:
            w.writerow(
                [
                    r["source"],
                    r["base"],
                    r["nEvts"],
                    r["lambda"],
                    r["entries"],
                    r["sumW_gamma"],
                    r["sumW_neutron"],
                    r["r_gamma"],
                    r["r_neutron"],
                    r["path"],
                ]
            )

    source_summary = []
    for s in SOURCES:
        rows = [r for r in all_rows if r["source"] == s]
        sr_g = sum(r["r_gamma"] for r in rows)
        sr_n = sum(r["r_neutron"] for r in rows)
        source_summary.append(
            {
                "source": s,
                "n_bases": len(rows),
                "lambda": LAMBDA[s],
                "sum_r_gamma": sr_g,
                "sum_r_neutron": sr_n,
                "R_gamma": 6.0 * LAMBDA[s] * sr_g,
                "R_neutron": 6.0 * LAMBDA[s] * sr_n,
            }
        )

    with open(os.path.join(args.outdir, "g4_p1_source_summary.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "source",
                "n_bases",
                "lambda",
                "sum_r_gamma",
                "sum_r_neutron",
                "R_gamma",
                "R_neutron",
            ]
        )
        for r in source_summary:
            w.writerow(
                [
                    r["source"],
                    r["n_bases"],
                    r["lambda"],
                    r["sum_r_gamma"],
                    r["sum_r_neutron"],
                    r["R_gamma"],
                    r["R_neutron"],
                ]
            )

    R_gamma_total = sum(r["R_gamma"] for r in source_summary)
    R_neutron_total = sum(r["R_neutron"] for r in source_summary)

    # Source rates are per particle type.  The gamma source rate includes the
    # three gamma lines and Eu154; the neutron source rate is the Watt source.
    S_G4_gamma = N_ASSEMBLIES * sum(
        LAMBDA[s] for s in ("gamma_662keV", "gamma_1173keV",
                            "gamma_1332keV", "isotope_Eu154")
    )
    S_G4_neutron = N_ASSEMBLIES * LAMBDA["neutron_Watt"]

    r_avg_gamma = R_gamma_total / S_G4_gamma
    r_avg_neutron = R_neutron_total / S_G4_neutron

    wssa = read_wssa(args.wssa_summary)

    comparison = []
    for cask in ("C1", "C2", "C3"):
        for particle, R_tot, S_g4, r_avg in (
            ("gamma", R_gamma_total, S_G4_gamma, r_avg_gamma),
            ("neutron", R_neutron_total, S_G4_neutron, r_avg_neutron),
        ):
            W_sum = wssa[(cask, particle)]
            np_hist = WSSA_NP[(cask, particle)]
            W = W_sum / np_hist
            comparison.append(
                {
                    "cask": cask,
                    "particle": particle,
                    "W_sum": W_sum,
                    "np": np_hist,
                    "W_per_history": W,
                    "R_g4": R_tot,
                    "S_g4_per_s": S_g4,
                    "r_avg": r_avg,
                    # W/r_avg has units of source particles per source
                    # history, not particles per second.
                    "source_particles_per_history": W / r_avg,
                }
            )

    with open(
        os.path.join(args.outdir, "g4_wssa_source_rate_comparison.csv"), "w", newline=""
    ) as f:
        w = csv.writer(f)
        w.writerow(
            [
                "cask",
                "particle",
                "W_sum",
                "np",
                "W_per_history",
                "R_g4",
                "S_g4_per_s",
                "r_avg",
                "source_particles_per_history",
            ]
        )
        for r in comparison:
            w.writerow(
                [
                    r["cask"],
                    r["particle"],
                    r["W_sum"],
                    r["np"],
                    r["W_per_history"],
                    r["R_g4"],
                    r["S_g4_per_s"],
                    r["r_avg"],
                    r["source_particles_per_history"],
                ]
            )

    print("Per-source p1 surface rate (84 assemblies):")
    print(f"{'source':18s} {'lambda':>10s} {'sum r_gamma':>12s} {'R_gamma':>10s} {'sum r_n':>12s} {'R_neutron':>10s}")
    for r in source_summary:
        print(
            f"{r['source']:18s} {r['lambda']:10.3e} {r['sum_r_gamma']:12.4e} "
            f"{r['R_gamma']:10.3e} {r['sum_r_neutron']:12.4e} {r['R_neutron']:10.3e}"
        )
    print()
    print(f"R_gamma_total = {R_gamma_total:.6e} /s")
    print(f"R_neutron_total = {R_neutron_total:.6e} /s")
    print(f"S_G4_gamma = {S_G4_gamma:.6e} gamma/s")
    print(f"S_G4_neutron = {S_G4_neutron:.6e} neutron/s")
    print(f"r_avg_gamma = {r_avg_gamma:.6e}")
    print(f"r_avg_neutron = {r_avg_neutron:.6e}")
    print()
    print("WSSA comparison:")
    print(
        f"{'cask':4s} {'particle':8s} {'W_sum':>10s} {'|np|':>10s} "
        f"{'W/history':>10s} {'R_g4':>10s} {'S_g4/s':>10s} "
        f"{'r_avg':>11s} {'src/history':>12s}"
    )
    for r in comparison:
        print(
            f"{r['cask']:4s} {r['particle']:8s} {r['W_sum']:10.3e} "
            f"{r['np']:10.3e} {r['W_per_history']:10.3e} {r['R_g4']:10.3e} "
            f"{r['S_g4_per_s']:10.3e} {r['r_avg']:11.3e} "
            f"{r['source_particles_per_history']:12.3e}"
        )


if __name__ == "__main__":
    main()
