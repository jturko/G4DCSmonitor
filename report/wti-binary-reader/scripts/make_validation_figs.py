#!/usr/bin/env python3
"""Copy the WSSA validation inputs and build the report figures.

Run from anywhere:

    python3 report/wti-binary-reader/scripts/make_validation_figs.py

The script expects the 200k ROOT-reader outputs in
``scripts/wssa/output`` and the supplied reference PNGs in
``/home/turko46/data/wti`` (override with ``WTI_DATA_ROOT``).
"""

import os
import re
import shutil
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from validate_wssa import load_reference, hist2d, bin_image, corr

REPORT = Path(__file__).resolve().parents[1]
REPO = REPORT.parents[1]
OUT = REPO / "scripts" / "wssa" / "output"
DATA_ROOT = Path(os.environ.get("WTI_DATA_ROOT", "/home/turko46/data/wti"))
DATA = REPORT / "data"
FIG = REPORT / "figures"

TAGS = [
    "Cask1_neutrons",
    "Cask1_gamma",
    "Cask2_neutrons",
    "Cask2_gamma",
    "Cask3_neutrons",
    "Cask3_gamma",
]

LABELS = [
    "C1 n",
    r"C1 $\gamma$",
    "C2 n",
    r"C2 $\gamma$",
    "C3 n",
    r"C3 $\gamma$",
]

COLORS = {
    "2D": "#1f4e79",
    "phi1": "#c0392b",
    "phi0": "#e6a0a0",
    "z": "#2e7d32",
    "image": "#555555",
    "mode1": "#c0392b",
    "mode0": "#e6a0a0",
}


def copy_inputs():
    DATA.mkdir(parents=True, exist_ok=True)
    FIG.mkdir(parents=True, exist_ok=True)

    for tag in TAGS:
        src = OUT / f"{tag}_rand200k_36x41_validation.txt"
        if not src.exists():
            raise SystemExit(f"missing validation file: {src}")
        shutil.copy2(src, DATA / src.name)

    copies = {
        OUT / "Cask3_neutrons_rand200k_36x41_energy.png":
            FIG / "fig_energy_Cask3_neutrons.png",
        OUT / "Cask3_gamma_rand200k_36x41_energy.png":
            FIG / "fig_energy_Cask3_gamma.png",
    }
    for src, dst in copies.items():
        if not src.exists():
            raise SystemExit(f"missing input image: {src}")
        shutil.copy2(src, dst)


def parse_validation(path):
    text = path.read_text()
    out = {}
    patterns = {
        "2d": r"2D corr mode1:\s*([-\d.]+)",
        "phi1": r"phi profile corr mode1:\s*([-\d.]+)",
        "phi0": r"phi profile corr mode0:\s*([-\d.]+)",
        "z": r"z profile corr:\s*([-\d.]+)",
        "zpeak": r"z peak: image=(\d+) mm, ours=(\d+) mm",
        "phpeak": r"phi peak: image=(\d+) deg, mode1=(\d+), mode0=(\d+)",
    }
    for key, pat in patterns.items():
        m = re.search(pat, text)
        if not m:
            raise SystemExit(f"cannot parse {key} in {path}")
        if key in ("2d", "phi1", "phi0", "z"):
            out[key] = float(m.group(1))
        else:
            out[key] = tuple(int(m.group(i)) for i in range(1, len(m.groups()) + 1))
    return text, out


def parse_block(text, header):
    lines = text.splitlines()
    try:
        i = next(i for i, line in enumerate(lines) if line.startswith(header))
    except StopIteration:
        raise SystemExit(f"cannot find profile block: {header}")
    rows = []
    for line in lines[i + 1:]:
        if not line.strip():
            break
        parts = line.split()
        if len(parts) < 3:
            break
        rows.append([float(x) for x in parts])
    return np.asarray(rows)


def make_scores():
    vals = []
    for tag in TAGS:
        _, v = parse_validation(DATA / f"{tag}_rand200k_36x41_validation.txt")
        vals.append(v)

    x = np.arange(len(TAGS))
    w = 0.26
    fig, (ax1, ax2) = plt.subplots(
        1, 2, figsize=(11.0, 4.2), gridspec_kw={"width_ratios": [1.35, 1.0]}
    )

    ax1.bar(x - w, [v["2d"] for v in vals], w, label="2D map",
            color=COLORS["2D"], edgecolor="black", linewidth=0.4)
    ax1.bar(x, [v["phi1"] for v in vals], w, label=r"$\varphi$ profile, mode 1",
            color=COLORS["phi1"], edgecolor="black", linewidth=0.4)
    ax1.bar(x + w, [v["z"] for v in vals], w, label="$z$ profile",
            color=COLORS["z"], edgecolor="black", linewidth=0.4)
    ax1.axhline(1.0, color="0.65", lw=0.8, ls="--")
    ax1.set_ylim(0.0, 1.08)
    ax1.set_ylabel("Pearson correlation")
    ax1.set_title("(a) mode 1: reference image vs reader")
    ax1.set_xticks(x)
    ax1.set_xticklabels(LABELS)
    ax1.legend(loc="lower right", fontsize=8, ncol=1)
    ax1.grid(axis="y", color="0.92", lw=0.6)

    ax2.bar(x - 0.18, [v["phi1"] for v in vals], 0.34,
            label=r"mode 1 (clockwise from $+y$)",
            color=COLORS["mode1"], edgecolor="black", linewidth=0.4)
    ax2.bar(x + 0.18, [v["phi0"] for v in vals], 0.34,
            label=r"mode 0 (counter-clockwise from $+x$)",
            color=COLORS["mode0"], edgecolor="black", linewidth=0.4)
    ax2.axhline(0.0, color="black", lw=0.8)
    ax2.set_ylim(-1.0, 1.08)
    ax2.set_ylabel(r"$\varphi$-profile Pearson correlation")
    ax2.set_title(r"(b) orientation comparison")
    ax2.set_xticks(x)
    ax2.set_xticklabels(LABELS)
    ax2.legend(loc="lower left", fontsize=7)
    ax2.grid(axis="y", color="0.92", lw=0.6)

    fig.tight_layout()
    fig.savefig(FIG / "fig_validation_scores.pdf")
    fig.savefig(FIG / "fig_validation_scores.png", dpi=220)
    plt.close(fig)


def make_maps():
    """Normalised 2x2 comparison of the Cask 3 maps.

    Both members of a pair are divided by their own maximum, so the colour
    scale is comparable even though the reference is a dose-rate map and the
    reader map is a sum of MCNP weights.
    """
    panels = [
        ("Cask3_neutrons", "Cask 3 neutrons"),
        ("Cask3_gamma", r"Cask 3 $\gamma$"),
    ]
    fig, axes = plt.subplots(
        2, 2, figsize=(10.8, 5.9), sharex=True, sharey=True
    )
    im = None
    for row, (tag, label) in enumerate(panels):
        image_path = DATA_ROOT / tag.split("_")[0] / f"{tag}.png"
        root_path = OUT / f"{tag}_rand200k_36x41.root"
        val, phi, z = load_reference(image_path)
        ours, xe, ze = hist2d(root_path, "hPhiZ_0_4000")
        ref = bin_image(val, phi, z, xe, ze)

        ref = np.ma.masked_invalid(ref / np.nanmax(ref))
        ours = np.ma.masked_invalid(ours / np.max(ours))

        ax = axes[row, 0]
        im = ax.pcolormesh(xe, ze, ref, shading="auto", cmap="turbo",
                           vmin=0.0, vmax=1.0)
        ax.set_title(f"(a) reference, {label}" if row == 0
                     else f"(c) reference, {label}", fontsize=9)

        ax = axes[row, 1]
        im = ax.pcolormesh(xe, ze, ours, shading="auto", cmap="turbo",
                           vmin=0.0, vmax=1.0)
        ax.set_title(f"(b) reader, {label}" if row == 0
                     else f"(d) reader, {label}", fontsize=9)
        r2d = corr(ref.filled(np.nan), ours.filled(np.nan))
        ax.text(0.02, 0.94, r"$r_{2D}=%.3f$" % r2d,
                transform=ax.transAxes, fontsize=8, va="top",
                bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none",
                          alpha=0.75))

        for ax in axes[row, :]:
            ax.set_xlim(0.0, 360.0)
            ax.set_ylim(0.0, 4000.0)
            ax.set_xticks(np.arange(0, 361, 90))
            ax.set_yticks(np.arange(0, 4001, 1000))
            ax.tick_params(labelsize=8)

    fig.supxlabel(r"angle $\varphi$ [deg]", fontsize=9)
    fig.supylabel("cask height $z$ [mm]", fontsize=9)
    fig.subplots_adjust(left=0.08, right=0.87, bottom=0.11, top=0.91,
                        wspace=0.16, hspace=0.28)
    cax = fig.add_axes([0.89, 0.16, 0.018, 0.68])
    fig.colorbar(im, cax=cax, label="normalised map value")
    fig.savefig(FIG / "fig_maps_Cask3.pdf")
    fig.savefig(FIG / "fig_maps_Cask3.png", dpi=220)
    plt.close(fig)


def make_profiles():
    text, _ = parse_validation(DATA / "Cask3_neutrons_rand200k_36x41_validation.txt")
    z = parse_block(text, "z profile: z[mm] image ours")
    p = parse_block(text, "phi profile: phi[deg] image mode1 mode0")

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7.4, 6.4), sharex=False)

    ax1.plot(z[:, 0], z[:, 1], drawstyle="steps-mid",
             color=COLORS["image"], lw=1.6, label="reference image")
    ax1.plot(z[:, 0], z[:, 2], drawstyle="steps-mid",
             color=COLORS["2D"], lw=1.6, label="ROOT reader")
    ax1.set_xlim(0.0, 4000.0)
    ax1.set_ylim(0.0, 1.08)
    ax1.set_xlabel("cask height $z$ [mm]")
    ax1.set_ylabel("normalised mean")
    ax1.set_title(r"(a) $z$ profile, Cask~3 neutrons")
    ax1.legend(fontsize=8, loc="upper left")
    ax1.grid(color="0.92", lw=0.6)
    ax1.text(0.98, 0.93, r"$r_z = 0.992$", transform=ax1.transAxes,
             fontsize=9, va="top", ha="right",
             bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none",
                       alpha=0.85))

    ax2.plot(p[:, 0], p[:, 1], color=COLORS["image"], lw=1.6,
             label="reference image")
    ax2.plot(p[:, 0], p[:, 2], color=COLORS["mode1"], lw=1.6,
             label=r"mode 1, clockwise from $+y$")
    ax2.plot(p[:, 0], p[:, 3], color=COLORS["mode0"], lw=1.4, ls="--",
             label=r"mode 0, counter-clockwise from $+x$")
    ax2.set_xlim(0.0, 360.0)
    ax2.set_ylim(0.0, 1.08)
    ax2.set_xticks(np.arange(0, 361, 45))
    ax2.set_xlabel(r"angle $\varphi$ [deg]")
    ax2.set_ylabel("normalised mean")
    ax2.set_title(r"(b) $\varphi$ profile, Cask~3 neutrons")
    ax2.legend(fontsize=8, loc="lower center", ncol=1)
    ax2.grid(color="0.92", lw=0.6)
    ax2.text(0.02, 0.93, r"$r_\varphi = 0.983$ (mode 1)",
             transform=ax2.transAxes, fontsize=9, va="top",
             bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none",
                       alpha=0.85))

    fig.tight_layout()
    fig.savefig(FIG / "fig_validation_profiles.pdf")
    fig.savefig(FIG / "fig_validation_profiles.png", dpi=220)
    plt.close(fig)


def main():
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["DejaVu Serif"],
        "mathtext.fontset": "dejavuserif",
        "font.size": 9,
        "axes.titlesize": 9,
        "axes.labelsize": 9,
        "legend.fontsize": 8,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "axes.linewidth": 0.8,
    })
    copy_inputs()
    make_scores()
    make_maps()
    make_profiles()
    print(f"wrote figures in {FIG}")


if __name__ == "__main__":
    main()
