#!/usr/bin/env python3
"""Compare a ROOT phi-z histogram with a supplied reference PNG heatmap.

The reference images all use the same layout:
  plot frame: x 177..1346, y 69..653
  colorbar:    x 1375..1433, y 69..653
  colorbar high (red) at the top, low (blue) at the bottom.

The reference is a dose-rate map; the ROOT histogram is a summed-weight map.
Only the morphology (z and phi structure) is compared.
"""

import argparse

import numpy as np
from PIL import Image
from scipy.spatial import cKDTree

import ROOT


def load_reference(path):
    a = np.asarray(Image.open(path).convert("RGB"))
    h, w, _ = a.shape
    dark = a.max(axis=2) < 80
    rows = dark.sum(axis=1)
    cols = dark.sum(axis=0)
    rlines = [i for i, v in enumerate(rows) if v > 0.5 * w]
    clines = [i for i, v in enumerate(cols) if v > 0.5 * h]
    if len(rlines) < 2 or len(clines) < 4:
        raise RuntimeError(f"cannot detect frame in {path}")
    y0, y1 = rlines[0], rlines[1]
    x0, x1 = clines[0], clines[1]
    cbx0, cbx1 = clines[2], clines[3]

    cbx = (cbx0 + cbx1) // 2
    cb = a[y0 + 1:y1, cbx, :].astype(float)
    cb_val = np.linspace(1.0, 0.0, len(cb))  # top=1, bottom=0

    tree = cKDTree(cb)
    heat = a[y0 + 1:y1, x0 + 1:x1, :].astype(float)
    dist, idx = tree.query(heat.reshape(-1, 3), k=1)
    val = cb_val[idx].reshape(heat.shape[:2])
    dist = dist.reshape(heat.shape[:2])

    white = np.all(heat > 245, axis=2)
    black = np.all(heat < 50, axis=2)
    good = (~white) & (~black) & (dist < 35.0)
    val = np.where(good, val, np.nan)

    nx = x1 - x0 - 1
    ny = y1 - y0 - 1
    phi = (np.arange(nx) + 0.5) / nx * 360.0
    z = (ny - (np.arange(ny) + 0.5)) / ny * 4000.0
    return val, phi, z


def bin_image(val, phi, z, xedges, yedges):
    """Average the image value in the supplied histogram bin edges."""
    nx = len(xedges) - 1
    ny = len(yedges) - 1
    out = np.full((ny, nx), np.nan)
    for i in range(ny):
        zsel = (z >= yedges[i]) & (z < yedges[i + 1])
        if i == ny - 1:
            zsel = (z >= yedges[i]) & (z <= yedges[i + 1])
        if not np.any(zsel):
            continue
        for j in range(nx):
            psel = (phi >= xedges[j]) & (phi < xedges[j + 1])
            if j == nx - 1:
                psel = (phi >= xedges[j]) & (phi <= xedges[j + 1])
            sub = val[np.ix_(zsel, psel)]
            if np.any(np.isfinite(sub)):
                out[i, j] = np.nanmean(sub)
    return out


def hist2d(root_path, name):
    f = ROOT.TFile.Open(str(root_path))
    h = f.Get(name)
    if not h:
        raise RuntimeError(f"{name} not found in {root_path}")
    nx = h.GetNbinsX()
    ny = h.GetNbinsY()
    xedges = np.array([h.GetXaxis().GetBinLowEdge(i + 1) for i in range(nx)] +
                      [h.GetXaxis().GetBinUpEdge(nx)])
    yedges = np.array([h.GetYaxis().GetBinLowEdge(i + 1) for i in range(ny)] +
                      [h.GetYaxis().GetBinUpEdge(ny)])
    out = np.zeros((ny, nx))
    for i in range(ny):
        for j in range(nx):
            out[i, j] = h.GetBinContent(j + 1, i + 1)
    return out, xedges, yedges


def corr(a, b):
    a = np.asarray(a, float).ravel()
    b = np.asarray(b, float).ravel()
    m = np.isfinite(a) & np.isfinite(b)
    a = a[m]
    b = b[m]
    if len(a) < 3 or np.std(a) == 0 or np.std(b) == 0:
        return float("nan")
    return float(np.corrcoef(a, b)[0, 1])


def norm(x):
    x = np.asarray(x, float)
    m = np.nanmax(x)
    return x / m if m > 0 else x


def center(edges):
    return 0.5 * (edges[:-1] + edges[1:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--root", required=True)
    args = ap.parse_args()

    val, phi, z = load_reference(args.image)

    # Main map: hPhiZ_0_4000 has 80 z bins over 0-4000 mm.
    ours80, xe80, ze80 = hist2d(args.root, "hPhiZ_0_4000")
    img80 = bin_image(val, phi, z, xe80, ze80)
    print(f"2D map bins: {len(xe80)-1} phi x {len(ze80)-1} z (0-4000 mm)")
    print(f"reference finite bins: {int(np.isfinite(img80).sum())} / {img80.size}")
    print(f"2D corr mode1: {corr(img80, ours80):.4f}")

    # Orientation: use hPhiZ and hPhiZ_alt at their native 36 x 41 binning.
    ours41, xe41, ze41 = hist2d(args.root, "hPhiZ")
    alt41, _, _ = hist2d(args.root, "hPhiZ_alt")
    img41 = bin_image(val, phi, z, xe41, ze41)
    print(f"orientation map bins: {len(xe41)-1} phi x {len(ze41)-1} z")
    print(f"2D corr mode1 (native): {corr(img41, ours41):.4f}")
    print(f"2D corr mode0 (native): {corr(img41, alt41):.4f}")

    # Profiles: phi from the native 41-z map, z from the 80-bin map.
    img_phi = np.nanmean(img41, axis=0)
    img_z = np.nanmean(img80, axis=1)
    our_phi = np.mean(ours41, axis=0)
    alt_phi = np.mean(alt41, axis=0)
    our_z = np.mean(ours80, axis=1)
    print(f"phi profile corr mode1: {corr(img_phi, our_phi):.4f}")
    print(f"phi profile corr mode0: {corr(img_phi, alt_phi):.4f}")
    print(f"z profile corr: {corr(img_z, our_z):.4f}")

    phic = center(xe41)
    zc80 = center(ze80)
    print(f"z peak: image={zc80[np.nanargmax(img_z)]:.0f} mm, "
          f"ours={zc80[np.argmax(our_z)]:.0f} mm")
    print(f"phi peak: image={phic[np.nanargmax(img_phi)]:.0f} deg, "
          f"mode1={phic[np.argmax(our_phi)]:.0f}, "
          f"mode0={phic[np.argmax(alt_phi)]:.0f}")

    print("\nz profile: z[mm] image ours")
    for i in range(len(zc80)):
        print(f"{zc80[i]:8.1f} {norm(img_z)[i]:8.4f} {norm(our_z)[i]:8.4f}")
    print("\nphi profile: phi[deg] image mode1 mode0")
    for i in range(len(phic)):
        print(f"{phic[i]:8.1f} {norm(img_phi)[i]:8.4f} "
              f"{norm(our_phi)[i]:8.4f} {norm(alt_phi)[i]:8.4f}")


if __name__ == "__main__":
    main()
