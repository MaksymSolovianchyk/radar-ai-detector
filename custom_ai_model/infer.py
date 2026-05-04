"""
infer.py
--------
Run inference on a single .npy radar feature file using a trained model.

The script:
    1. Loads the .npy file  (expected shape: (32, 10))
    2. Applies the saved z-score normalisation
    3. Runs the Keras model (or TFLite model if --tflite flag is set)
    4. Prints the predicted class and per-class probabilities

Feature order expected in the .npy file:
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

Usage
-----
    # Keras model
    python infer.py --model    output/radar_mlp.keras \
                   --normalization output/normalization.json \
                   --sample    radar_dataset/test/approaching/sample_001.npy

    # TFLite float model
    python infer.py --model    output/radar_mlp.tflite \
                   --normalization output/normalization.json \
                   --sample    radar_dataset/test/approaching/sample_001.npy \
                   --tflite

    # TFLite INT8 model
    python infer.py --model    output/radar_mlp_int8.tflite \
                   --normalization output/normalization.json \
                   --sample    radar_dataset/test/approaching/sample_001.npy \
                   --tflite
"""

import argparse
import sys
import numpy as np

from dataset import (
    load_normalization,
    apply_normalization,
    class_label,
    CLASSES,
    EXPECTED_SHAPE,
    FEATURE_NAMES,
)


# ── inference backends ─────────────────────────────────────────────────────────
def infer_keras(model_path: str, X: np.ndarray) -> np.ndarray:
    """
    Run inference using a saved Keras model.

    Parameters
    ----------
    model_path : path to .keras file
    X          : np.ndarray shape (32, 10) normalised

    Returns
    -------
    np.ndarray shape (4,)  softmax probabilities
    """
    import tensorflow as tf

    model = tf.keras.models.load_model(model_path)
    inp = X[np.newaxis].astype(np.float32)          # add batch dim → (1, 32, 10)
    probs = model.predict(inp, verbose=0)[0]         # (4,)
    return probs


def infer_tflite(model_path: str, X: np.ndarray) -> np.ndarray:
    """
    Run inference using a TFLite model (float32 or INT8).

    Parameters
    ----------
    model_path : path to .tflite file
    X          : np.ndarray shape (32, 10) normalised

    Returns
    -------
    np.ndarray shape (4,)  softmax probabilities
    """
    import tensorflow as tf

    interpreter = tf.lite.Interpreter(model_path=model_path)
    interpreter.allocate_tensors()

    input_details  = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    inp = X[np.newaxis].astype(np.float32)           # (1, 32, 10)
    interpreter.set_tensor(input_details[0]["index"], inp)
    interpreter.invoke()
    return interpreter.get_tensor(output_details[0]["index"])[0]   # (4,)


# ── display ────────────────────────────────────────────────────────────────────
def print_results(probs: np.ndarray, sample_path: str) -> None:
    """Print a clear, human-readable inference result."""
    predicted_idx   = int(np.argmax(probs))
    predicted_class = class_label(predicted_idx)
    confidence      = float(probs[predicted_idx]) * 100.0

    print("\n" + "=" * 50)
    print(f"  Sample : {sample_path}")
    print(f"  Result : {predicted_class.upper()}  ({confidence:.1f} % confidence)")
    print("=" * 50)
    print("  Class probabilities:")
    for i, name in enumerate(CLASSES):
        bar_len = int(probs[i] * 30)
        bar     = "█" * bar_len + "░" * (30 - bar_len)
        print(f"    {name:<14} {bar}  {probs[i]*100:5.1f} %")
    print("=" * 50)


def print_feature_summary(X_raw: np.ndarray) -> None:
    """Print a compact summary of the raw (un-normalised) features."""
    print("\n  Feature summary (mean over 32 frames):")
    means = X_raw.mean(axis=0)
    for i, (name, val) in enumerate(zip(FEATURE_NAMES, means)):
        print(f"    {i:2d}  {name:<24} {val:10.4f}")


# ── main ────────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Run radar motion inference on a single .npy sample."
    )
    parser.add_argument(
        "--model", required=True,
        help="Path to .keras model file, or .tflite file if --tflite is set."
    )
    parser.add_argument(
        "--normalization", required=True,
        help="Path to normalization.json saved during training."
    )
    parser.add_argument(
        "--sample", required=True,
        help="Path to a single .npy feature file with shape (32, 10)."
    )
    parser.add_argument(
        "--tflite", action="store_true",
        help="Use TFLite interpreter instead of Keras model."
    )
    parser.add_argument(
        "--verbose", action="store_true",
        help="Also print a per-feature summary of the input sample."
    )
    args = parser.parse_args()

    # ── load sample ──────────────────────────────────────────────────────────
    try:
        X_raw = np.load(args.sample).astype(np.float32)
    except FileNotFoundError:
        print(f"[ERROR] Sample file not found: {args.sample}")
        sys.exit(1)
    except Exception as e:
        print(f"[ERROR] Could not load sample: {e}")
        sys.exit(1)

    if X_raw.shape != EXPECTED_SHAPE:
        print(
            f"[ERROR] Sample shape is {X_raw.shape}, expected {EXPECTED_SHAPE}."
        )
        sys.exit(1)

    # ── load normalization ────────────────────────────────────────────────────
    try:
        norm_params = load_normalization(args.normalization)
    except FileNotFoundError:
        print(f"[ERROR] Normalization file not found: {args.normalization}")
        sys.exit(1)

    # ── normalise ─────────────────────────────────────────────────────────────
    X_norm = apply_normalization(X_raw, norm_params)   # (32, 10)

    # ── optional: show raw feature values ────────────────────────────────────
    if args.verbose:
        print_feature_summary(X_raw)

    # ── run inference ─────────────────────────────────────────────────────────
    if args.tflite:
        probs = infer_tflite(args.model, X_norm)
    else:
        probs = infer_keras(args.model, X_norm)

    # ── display results ───────────────────────────────────────────────────────
    print_results(probs, args.sample)


if __name__ == "__main__":
    main()
