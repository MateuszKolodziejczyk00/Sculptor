"""Generates FLIP (https://github.com/NVlabs/flip) error maps and statistics for the Closure Viewer reference tool.

Usage: python GenerateFlipImages.py <reference directory> <output directory> <image name> [<image name> ...]

For every <name> compares <reference directory>/<name> against <output directory>/New_<name>, writes
<output directory>/Flip_<name> and appends "<name> mean median p95 p99 max" to <output directory>/FlipStats.txt.
Requires: pip install flip-evaluator pillow numpy
"""

import os
import sys

import numpy as np
import flip_evaluator as flip
from PIL import Image


def LoadRGB(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0


def main():
    if len(sys.argv) < 4:
        print("Usage: GenerateFlipImages.py <reference directory> <output directory> <image name> [<image name> ...]")
        return 2

    referenceDirectory = sys.argv[1]
    outputDirectory = sys.argv[2]
    success = True

    statsPath = os.path.join(outputDirectory, "FlipStats.txt")
    if os.path.exists(statsPath):
        os.remove(statsPath)

    for name in sys.argv[3:]:
        referenceImage = LoadRGB(os.path.join(referenceDirectory, name))
        newImage = LoadRGB(os.path.join(outputDirectory, "New_" + name))

        try:
            magmaMap, _, _ = flip.evaluate(referenceImage, newImage, "LDR")
            Image.fromarray((np.clip(magmaMap, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)).save(os.path.join(outputDirectory, "Flip_" + name))

            errorMap, mean, _ = flip.evaluate(referenceImage, newImage, "LDR", applyMagma=False)
            errors = np.asarray(errorMap, dtype=np.float64).ravel()
            median, p95, p99 = np.percentile(errors, [50, 95, 99])

            with open(statsPath, "a") as statsFile:
                statsFile.write("{} {:.6f} {:.6f} {:.6f} {:.6f} {:.6f}\n".format(name, mean, median, p95, p99, errors.max()))
            print("FLIP {}: mean error {:.6f}".format(name, mean))
        except Exception as e:
            print("FLIP failed for {}: {}".format(name, e))
            success = False

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
