"""
dataset.py
----------
Loads radar feature sequences from the folder structure:

    radar_dataset/
        train/
            approaching/  *.npy
            receding/     *.npy
            idle/         *.npy
            mixed/        *.npy
        val/  ...
        test/ ...

Each .npy file must have shape (32, 10):
    axis-0: 32 time steps (radar frames)
    axis-1: 10 engineered features per frame, in this order:
        0  approaching_energy
        1  receding_energy
        2  approach_recede_ratio
        3  max_approach_peak
        4  max_recede_peak
        5  total_energy
        6  peak_doppler
        7  velocity
        8  center_of_mass
        9  spectral_width
"""

import os
import json
import numpy as np
from pathlib import Path
from typing import Tuple, Dict, List, Optional

# ── constants ────────────────────────────────────────────────────────────────
CLASSES: List[str] = ["approaching", "idle", "receding"]   # alphabetical → label 0-3
EXPECTED_SHAPE: Tuple[int, int] = (32, 10)
FEATURE_NAMES: List[str] = [
    "approaching_energy",
    "receding_energy",
    "approach_recede_ratio",
    "max_approach_peak",
    "max_recede_peak",
    "total_energy",
    "peak_doppler",
    "velocity",
    "center_of_mass",
    "spectral_width",
]


# ── helpers ──────────────────────────────────────────────────────────────────
def _class_to_label(class_name: str) -> int:
    """Return the integer label for a class name."""
    if class_name not in CLASSES:
        raise ValueError(f"Unknown class '{class_name}'. Expected one of {CLASSES}.")
    return CLASSES.index(class_name)


def _load_split(split_dir: Path) -> Tuple[np.ndarray, np.ndarray]:
    """
    Load all .npy files from one split directory (train / val / test).

    Returns
    -------
    X : np.ndarray  shape (N, 32, 10)  float32
    y : np.ndarray  shape (N,)         int32
    """
    X_list, y_list = [], []
    skipped = 0

    for class_name in CLASSES:
        class_dir = split_dir / class_name
        if not class_dir.exists():
            print(f"  [WARNING] Class folder not found, skipping: {class_dir}")
            continue

        npy_files = sorted(class_dir.glob("*.npy"))
        if len(npy_files) == 0:
            print(f"  [WARNING] No .npy files in: {class_dir}")
            continue

        label = _class_to_label(class_name)

        for fpath in npy_files:
            try:
                arr = np.load(fpath).astype(np.float32)
            except Exception as e:
                print(f"  [ERROR] Could not load {fpath}: {e}  — skipping.")
                skipped += 1
                continue

            if arr.shape != EXPECTED_SHAPE:
                print(
                    f"  [ERROR] {fpath.name} has shape {arr.shape}, "
                    f"expected {EXPECTED_SHAPE}  — skipping."
                )
                skipped += 1
                continue

            X_list.append(arr)
            y_list.append(label)

    if len(X_list) == 0:
        raise RuntimeError(f"No valid samples found in {split_dir}")

    print(f"  Loaded {len(X_list)} samples from {split_dir}  ({skipped} skipped).")
    return np.stack(X_list, axis=0), np.array(y_list, dtype=np.int32)


# ── public API ────────────────────────────────────────────────────────────────
def load_dataset(
    dataset_root: str,
) -> Tuple[
    Tuple[np.ndarray, np.ndarray],
    Tuple[np.ndarray, np.ndarray],
    Tuple[np.ndarray, np.ndarray],
]:
    """
    Load train / val / test splits.

    Parameters
    ----------
    dataset_root : str
        Path to the root folder that contains train/, val/, test/ sub-folders.

    Returns
    -------
    (X_train, y_train), (X_val, y_val), (X_test, y_test)
    All X arrays have shape (N, 32, 10), dtype float32.
    All y arrays have shape (N,),         dtype int32.
    """
    root = Path(dataset_root)
    if not root.exists():
        raise FileNotFoundError(f"Dataset root not found: {root}")

    print("Loading training set …")
    train = _load_split(root / "train")
    print("Loading validation set …")
    val = _load_split(root / "val")
    print("Loading test set …")
    test = _load_split(root / "test")

    return train, val, test


def compute_normalization(X_train: np.ndarray) -> Dict[str, List[float]]:
    """
    Compute per-feature mean and std from the TRAINING set only.

    Parameters
    ----------
    X_train : np.ndarray  shape (N, 32, 10)

    Returns
    -------
    dict with keys 'mean' and 'std', each a list of 10 floats (one per feature).
    """
    # Reshape to (N*32, 10) to compute statistics across all time steps
    flat = X_train.reshape(-1, X_train.shape[-1])   # (N*32, 10)
    mean = flat.mean(axis=0).tolist()                # list of 10
    std  = flat.std(axis=0).tolist()                 # list of 10

    # Protect against zero std (constant feature)
    std = [s if s > 1e-8 else 1.0 for s in std]

    return {"mean": mean, "std": std, "feature_names": FEATURE_NAMES}


def save_normalization(params: Dict, path: str) -> None:
    """Save normalization parameters to a JSON file."""
    with open(path, "w") as f:
        json.dump(params, f, indent=2)
    print(f"Normalization parameters saved to: {path}")


def load_normalization(path: str) -> Dict:
    """Load normalization parameters from a JSON file."""
    with open(path, "r") as f:
        return json.load(f)


def apply_normalization(
    X: np.ndarray, params: Dict
) -> np.ndarray:
    """
    Z-score normalise using pre-computed mean / std.

    Parameters
    ----------
    X      : np.ndarray  shape (N, 32, 10)  or  (32, 10)  for single sample
    params : dict from load_normalization()

    Returns
    -------
    np.ndarray  same shape as X, float32
    """
    mean = np.array(params["mean"], dtype=np.float32)   # (10,)
    std  = np.array(params["std"],  dtype=np.float32)   # (10,)
    return (X.astype(np.float32) - mean) / std


def class_label(index: int) -> str:
    """Convert integer label back to class name."""
    return CLASSES[index]
