"""
export_tflite.py
----------------
Converts a trained Keras model to:
    1. TFLite float32 model  (.tflite)
    2. TFLite INT8 fully-quantized model  (_int8.tflite)

The INT8 model is suitable for STM32Cube.AI / STEdgeAI deployment on STM32N6.

STM32 deployment checklist
--------------------------
Files you MUST copy to your STM32 project:
    ✓  radar_model_int8.tflite   (or float version if INT8 quality is poor)
    ✓  normalization.json        (mean + std for each of the 10 features)

On the MCU, before calling the model:
    1. Collect 32 frames of 10 features  → shape (32, 10)
    2. For each feature i (0..9):
           normalised[i] = (raw[i] - mean[i]) / std[i]
    3. Feed the normalised (32, 10) buffer to the TFLite interpreter.
    4. Read the 4-element softmax output → argmax → class label.

Usage
-----
    python export_tflite.py --model output/radar_mlp.keras \
                            --dataset radar_dataset \
                            --normalization output/normalization.json \
                            --output_dir output
"""

import os
import argparse
import numpy as np
import tensorflow as tf
from pathlib import Path

from dataset import load_dataset, load_normalization, apply_normalization


# ── representative dataset for INT8 calibration ──────────────────────────────
def make_representative_dataset(X_train: np.ndarray, num_samples: int = 200):
    """
    Generator that yields calibration samples for INT8 post-training quantization.

    TFLite's INT8 quantizer needs to observe the range of activations, so we
    feed a random subset of (normalised) training samples through the model.

    Parameters
    ----------
    X_train    : np.ndarray  shape (N, 32, 10)  already normalised
    num_samples: how many samples to use for calibration (200 is usually enough)
    """
    indices = np.random.choice(len(X_train),
                               size=min(num_samples, len(X_train)),
                               replace=False)

    def generator():
        for i in indices:
            # TFLite expects a list of input tensors, each with a batch dim
            sample = X_train[i : i + 1].astype(np.float32)   # (1, 32, 10)
            yield [sample]

    return generator


# ── export helpers ─────────────────────────────────────────────────────────────
def export_float_tflite(model: tf.keras.Model, output_path: str) -> None:
    """Export a standard float32 TFLite model."""
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_model = converter.convert()

    with open(output_path, "wb") as f:
        f.write(tflite_model)

    size_kb = os.path.getsize(output_path) / 1024
    print(f"Float TFLite saved: {output_path}  ({size_kb:.1f} KB)")


def export_int8_tflite(
    model: tf.keras.Model,
    X_train_norm: np.ndarray,
    output_path: str,
    num_calibration_samples: int = 200,
) -> None:
    """
    Export a fully INT8-quantized TFLite model.

    Both weights and activations are quantized.  Input/output tensors are kept
    as float32 for easier MCU integration (the TFLite runtime handles the
    float→int8 conversion internally).
    """
    converter = tf.lite.TFLiteConverter.from_keras_model(model)

    # Full integer quantization
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = make_representative_dataset(
        X_train_norm, num_calibration_samples
    )

    # Keep input/output as float so the MCU can feed raw float arrays
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type  = tf.float32
    converter.inference_output_type = tf.float32

    tflite_model = converter.convert()

    with open(output_path, "wb") as f:
        f.write(tflite_model)

    size_kb = os.path.getsize(output_path) / 1024
    print(f"INT8 TFLite saved:  {output_path}  ({size_kb:.1f} KB)")


def verify_tflite(tflite_path: str, X_sample: np.ndarray) -> np.ndarray:
    """
    Quick sanity-check: run one sample through the TFLite interpreter and
    return the predicted probabilities.

    Parameters
    ----------
    tflite_path : path to .tflite file
    X_sample    : np.ndarray  shape (32, 10)  normalised

    Returns
    -------
    np.ndarray  shape (4,)  softmax probabilities
    """
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()

    input_details  = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    inp = X_sample[np.newaxis].astype(np.float32)   # (1, 32, 10)
    interpreter.set_tensor(input_details[0]["index"], inp)
    interpreter.invoke()

    return interpreter.get_tensor(output_details[0]["index"])[0]   # (4,)


# ── main ───────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Export a trained Keras radar model to TFLite."
    )
    parser.add_argument("--model",         required=True,
                        help="Path to trained .keras model file.")
    parser.add_argument("--dataset",       required=True,
                        help="Path to radar_dataset root folder.")
    parser.add_argument("--normalization", required=True,
                        help="Path to normalization.json.")
    parser.add_argument("--output_dir",    default="output",
                        help="Directory to save .tflite files.")
    parser.add_argument("--calibration_samples", type=int, default=200,
                        help="Number of samples for INT8 calibration.")
    args = parser.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # ── load model ──────────────────────────────────────────────────────────
    print(f"Loading model: {args.model}")
    model = tf.keras.models.load_model(args.model)
    model.summary()

    # ── load and normalise training data (for INT8 calibration) ─────────────
    print("\nLoading training data for INT8 calibration …")
    (X_train, _), _, _ = load_dataset(args.dataset)
    norm_params = load_normalization(args.normalization)
    X_train_norm = apply_normalization(X_train, norm_params)

    # ── derive output file names from model name ─────────────────────────────
    stem = Path(args.model).stem          # e.g. "radar_mlp"
    float_path = str(out_dir / f"{stem}.tflite")
    int8_path  = str(out_dir / f"{stem}_int8.tflite")

    # ── export ───────────────────────────────────────────────────────────────
    print("\nExporting float32 TFLite …")
    export_float_tflite(model, float_path)

    print("\nExporting INT8 TFLite …")
    export_int8_tflite(model, X_train_norm, int8_path,
                       args.calibration_samples)

    # ── quick verification ───────────────────────────────────────────────────
    print("\nVerifying float TFLite on one sample …")
    probs = verify_tflite(float_path, X_train_norm[0])
    print(f"  Float  probabilities: {probs}")

    print("Verifying INT8 TFLite on one sample …")
    probs_int8 = verify_tflite(int8_path, X_train_norm[0])
    print(f"  INT8   probabilities: {probs_int8}")

    # ── deployment reminder ──────────────────────────────────────────────────
    print("\n" + "=" * 60)
    print("STM32 DEPLOYMENT FILES:")
    print(f"  Model (INT8):        {int8_path}")
    print(f"  Model (float):       {float_path}")
    print(f"  Normalization:       {args.normalization}")
    print("=" * 60)
    print("Remember: apply z-score normalisation on the MCU using")
    print("the mean/std values in normalization.json BEFORE inference.")
    print("=" * 60)


if __name__ == "__main__":
    main()
