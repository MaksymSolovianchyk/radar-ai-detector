# npy_to_jpg.py
#
# Converts radar .npy patches to 8-bit grayscale JPEG files.
#
# Reads from:   dataset/<class>/sample_XXXX.npy
#               shape (64, 96, 1) float32, values in [0, 1]
#
# Writes to:    tf_dataset/<class>/sample_XXXX.jpg
#               8-bit grayscale JPEG, quality=95
#
# Output folder structure is ready to point at directly in
# ST modelzoo image_classification user_config.yaml:
#
#   tf_dataset/
#   ├── idle/
#   │   ├── sample_0000.jpg
#   │   └── ...
#   └── hand_wave/
#       ├── sample_0000.jpg
#       └── ...
#
# JPEG quality=95 keeps compression artefacts below the natural
# noise floor of the radar data — safe for training.
#
# Requirements:
#   pip install numpy Pillow

import os
import numpy as np
from PIL import Image

# ── Config ────────────────────────────────────────────────────────────────────
DATASET_DIR  = "dataset"       # source folder with .npy class subfolders
OUTPUT_DIR   = "tf_dataset"    # destination folder for ST modelzoo
JPEG_QUALITY = 95              # 95 = near-lossless, safe for radar data
# ──────────────────────────────────────────────────────────────────────────────


def convert_class(class_name: str, src_dir: str, dst_dir: str) -> int:
    """Convert all .npy files in src_dir to JPEG in dst_dir."""
    os.makedirs(dst_dir, exist_ok=True)

    files = sorted([f for f in os.listdir(src_dir) if f.endswith(".npy")])
    if not files:
        print(f"  {class_name}: no .npy files found — skipping")
        return 0

    converted = 0
    skipped   = 0

    for fname in files:
        src_path = os.path.join(src_dir, fname)
        dst_name = fname.replace(".npy", ".jpg")
        dst_path = os.path.join(dst_dir, dst_name)

        # ── Load ──────────────────────────────────────────────────────────────
        patch = np.load(src_path)   # (64, 96, 1) float32, values [0, 1]

        # ── Squeeze channel dim ───────────────────────────────────────────────
        if patch.ndim == 3 and patch.shape[2] == 1:
            patch = patch[:, :, 0]      # → (64, 96)
        elif patch.ndim == 2:
            pass                        # already (64, 96)
        else:
            print(f"    WARNING: unexpected shape {patch.shape} "
                  f"in {fname} — skipping")
            skipped += 1
            continue

        # ── Transpose: freq×time → time×freq ─────────────────────────────────
        # (64 freq, 96 time) → (96 time, 64 freq)
        # rows    = time  (top = oldest, bottom = newest)
        # columns = freq  (left = most negative Hz, right = most positive Hz)
        patch = patch.T             # (96, 64)

        # ── Normalise and convert to uint8 ────────────────────────────────────
        patch    = np.clip(patch, 0.0, 1.0)
        patch_u8 = (patch * 255.0).round().astype(np.uint8)

        # ── Save as 8-bit grayscale JPEG ──────────────────────────────────────
        img = Image.fromarray(patch_u8, mode="L")
        img.save(dst_path, format="JPEG", quality=JPEG_QUALITY,
                 subsampling=0)     # subsampling=0 preserves grayscale quality
        converted += 1

    status = f"{converted} converted"
    if skipped:
        status += f", {skipped} skipped"
    print(f"  {class_name:15s}  {status}  →  {dst_dir}/")
    return converted


def main():
    if not os.path.isdir(DATASET_DIR):
        print(f"ERROR: '{DATASET_DIR}/' folder not found.")
        print("Run radar_collect.py first to collect .npy samples.")
        return

    # Find class folders — skip any that end with -png or -jpg
    all_entries = sorted(os.listdir(DATASET_DIR))
    class_dirs  = [
        d for d in all_entries
        if os.path.isdir(os.path.join(DATASET_DIR, d))
        and not d.endswith("-png")
        and not d.endswith("-jpg")
    ]

    if not class_dirs:
        print(f"No class folders found in '{DATASET_DIR}/'")
        return

    print(f"\nClasses found:  {class_dirs}")
    print(f"Output folder:  {OUTPUT_DIR}/")
    print(f"JPEG quality:   {JPEG_QUALITY}")
    print(f"\nConverting .npy → JPEG ...\n")

    total = 0
    for cls in class_dirs:
        src_dir = os.path.join(DATASET_DIR, cls)
        dst_dir = os.path.join(OUTPUT_DIR, cls)
        total  += convert_class(cls, src_dir, dst_dir)

    # ── Summary ───────────────────────────────────────────────────────────────
    print(f"\nDone — {total} files converted total.")
    print()
    print("Output structure:")
    print(f"  {OUTPUT_DIR}/")
    for cls in class_dirs:
        dst_dir   = os.path.join(OUTPUT_DIR, cls)
        jpg_count = len([f for f in os.listdir(dst_dir)
                         if f.endswith(".jpg")]) if os.path.exists(dst_dir) else 0
        print(f"  ├── {cls}/")
        print(f"  │   └── {jpg_count} .jpg files")
    print()
    print("ST modelzoo user_config.yaml — set this path:")
    print(f"  dataset:")
    print(f"    training_path: <path_to>/{OUTPUT_DIR}")


if __name__ == "__main__":
    main()