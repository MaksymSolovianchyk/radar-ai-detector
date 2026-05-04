"""
live_infer.py
-------------
Reads live radar feature data from STM32 over UART and runs
real-time inference using the trained Keras model.

Data flow:
    STM32 radar → UART → COM port → this script → Keras model → terminal output

Expected UART format from STM32 (CSV):
    timestamp_ms,f0,f1,f2,f3,f4,f5,f6,f7,f8,f9
    123456,10.2,5.1,2.0,8.3,4.1,15.4,30.5,0.0,12.1,3.4

Feature order (columns 1-10):
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

Usage:
    python live_infer.py --port COM4 \
                         --model output/radar_mlp.keras \
                         --normalization output/normalization.json

    # with TFLite model:
    python live_infer.py --port COM4 \
                         --model output/radar_mlp_int8.tflite \
                         --normalization output/normalization.json \
                         --tflite
"""

import argparse
import json
import sys
import time
import numpy as np

try:
    import serial
except ImportError:
    print("[ERROR] pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

# ── constants ─────────────────────────────────────────────────────────────────
FEATURE_COUNT = 10
NUM_FRAMES    = 32
CLASSES       = ["approaching", "idle", "receding"]


# ── serial helpers ─────────────────────────────────────────────────────────────
def parse_line(line: str):
    """
    Parse one CSV line from STM32 into a list of FEATURE_COUNT floats.
    Returns None if the line is invalid.
    """
    try:
        line = line.strip()
        if not line or line.startswith("-") or line.startswith("#"):
            return None
        parts = line.split(",")
        if len(parts) < FEATURE_COUNT + 1:
            return None
        values = [float(p.strip()) for p in parts[1: FEATURE_COUNT + 1]]
        if len(values) == FEATURE_COUNT:
            return values
    except (ValueError, IndexError):
        pass
    return None


# ── inference backends ─────────────────────────────────────────────────────────
def load_keras_model(model_path: str):
    import tensorflow as tf
    print(f"Loading Keras model: {model_path}")
    return tf.keras.models.load_model(model_path)


def predict_keras(model, X_norm: np.ndarray) -> np.ndarray:
    inp = X_norm[np.newaxis].astype(np.float32)   # (1, 32, 10)
    return model.predict(inp, verbose=0)[0]         # (3,)


def load_tflite_model(model_path: str):
    import tensorflow as tf
    print(f"Loading TFLite model: {model_path}")
    interpreter = tf.lite.Interpreter(model_path=model_path)
    interpreter.allocate_tensors()
    return interpreter


def predict_tflite(interpreter, X_norm: np.ndarray) -> np.ndarray:
    import tensorflow as tf
    input_details  = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inp = X_norm[np.newaxis].astype(np.float32)
    interpreter.set_tensor(input_details[0]["index"], inp)
    interpreter.invoke()
    return interpreter.get_tensor(output_details[0]["index"])[0]


# ── display ────────────────────────────────────────────────────────────────────
def print_prediction(probs: np.ndarray, frame_count: int) -> None:
    """Print a clear real-time prediction to the terminal."""
    pred_idx   = int(np.argmax(probs))
    pred_class = CLASSES[pred_idx]
    confidence = float(probs[pred_idx]) * 100.0

    # colour-code the output
    bar_length = 25
    bars = []
    for p in probs:
        filled = int(p * bar_length)
        bars.append("█" * filled + "░" * (bar_length - filled))

    print(f"\r[Frame {frame_count:05d}]  "
          f"► {pred_class.upper():12s} {confidence:5.1f}%  │  "
          f"app {bars[0]} {probs[0]*100:4.0f}%  "
          f"idle {bars[1]} {probs[1]*100:4.0f}%  "
          f"rec {bars[2]} {probs[2]*100:4.0f}%",
          end="", flush=True)


# ── main ───────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Live radar inference from STM32 UART."
    )
    parser.add_argument("--port",          required=True,
                        help="Serial port e.g. COM4 or /dev/tty.usbmodem1102")
    parser.add_argument("--model",         required=True,
                        help="Path to .keras or .tflite model file.")
    parser.add_argument("--normalization", required=True,
                        help="Path to normalization.json from training.")
    parser.add_argument("--baud",          type=int, default=576000,
                        help="Baud rate (default: 576000).")
    parser.add_argument("--tflite",        action="store_true",
                        help="Use TFLite interpreter instead of Keras.")
    args = parser.parse_args()

    # ── load normalization ────────────────────────────────────────────────────
    try:
        with open(args.normalization) as f:
            norm = json.load(f)
        mean = np.array(norm["mean"], dtype=np.float32)
        std  = np.array(norm["std"],  dtype=np.float32)
        print(f"Normalization loaded: {args.normalization}")
    except FileNotFoundError:
        print(f"[ERROR] Normalization file not found: {args.normalization}")
        sys.exit(1)

    # ── load model ────────────────────────────────────────────────────────────
    if args.tflite:
        model   = load_tflite_model(args.model)
        predict = lambda X: predict_tflite(model, X)
    else:
        model   = load_keras_model(args.model)
        predict = lambda X: predict_keras(model, X)

    # ── open serial port ──────────────────────────────────────────────────────
    try:
        ser = serial.Serial(args.port, args.baud, timeout=2)
    except serial.SerialException as e:
        print(f"[ERROR] Could not open {args.port}: {e}")
        sys.exit(1)

    time.sleep(2)
    ser.flushInput()

    print(f"\nListening on {args.port} at {args.baud} baud")
    print("Press Ctrl+C to stop\n")
    print("─" * 100)

    frame_count = 0

    try:
        while True:
            # ── collect NUM_FRAMES valid feature lines ─────────────────────
            frames = []
            while len(frames) < NUM_FRAMES:
                try:
                    raw  = ser.readline()
                    line = raw.decode("utf-8", errors="ignore")
                except Exception:
                    continue

                values = parse_line(line)
                if values is not None:
                    frames.append(values)

            # ── normalise ──────────────────────────────────────────────────
            X      = np.array(frames, dtype=np.float32)   # (32, 10)
            X_norm = (X - mean) / std                      # z-score

            # ── predict ────────────────────────────────────────────────────
            probs = predict(X_norm)

            # ── clamp to number of classes we have ─────────────────────────
            probs = probs[:len(CLASSES)]
            if probs.sum() > 0:
                probs = probs / probs.sum()   # renormalise if needed

            frame_count += 1
            print_prediction(probs, frame_count)

    except KeyboardInterrupt:
        print(f"\n\nStopped after {frame_count} predictions.")
        ser.close()


if __name__ == "__main__":
    main()
