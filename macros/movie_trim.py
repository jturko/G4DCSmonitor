#!/usr/bin/env python3
"""Trim trailing photon-free frames from a muon_movie frame set.

The time sweep is run out to a safe terminal time (~100 ns) so that every
optical photon has been absorbed/killed, but a given event usually goes dark
well before that (measured last-photon times ~40-60 ns).  Rendering the empty
remainder wastes GIF space and leaves a long black tail, so this drops every
frame after the last one that still contains an optical-photon trajectory.

Photon trajectories are drawn pure blue (drawByParticleID: opticalphoton blue);
the volume surface fill, holder and black clock text do not satisfy
(r<110 & g<110 & b>200), so the test separates light frames from dark ones.

Usage: movie_trim.py <frame_dir> <stem> [pad]
  pad = extra dark frames kept after the last light frame (default 4).
"""
import os
import sys
import glob

import numpy as np
from PIL import Image


def has_photons(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.int16)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return bool(((r < 110) & (g < 110) & (b > 200)).any())


def main():
    if len(sys.argv) < 3:
        print("usage: movie_trim.py <frame_dir> <stem> [pad]", file=sys.stderr)
        return 1
    frame_dir, stem = sys.argv[1], sys.argv[2]
    pad = int(sys.argv[3]) if len(sys.argv) > 3 else 4

    files = sorted(glob.glob(os.path.join(frame_dir, f"{stem}_*.png")))
    if not files:
        print("movie_trim: no PNG frames found; nothing to do")
        return 0

    active = [i for i, f in enumerate(files) if has_photons(f)]
    if not active:
        print("movie_trim: no photon pixels found; keeping all frames")
        return 0

    keep = min(len(files), active[-1] + 1 + pad)
    removed = 0
    for f in files[keep:]:
        os.remove(f)
        removed += 1
    print(f"movie_trim: last light frame = {active[-1]}, "
          f"keeping {keep}/{len(files)} (removed {removed} dark frames)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
