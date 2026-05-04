"""
collect.py
----------
Collects radar feature frames from the STM32 over UART and saves them
as .npy files ready for training.

What this script does:
    1. Opens the serial port at the specified baud rate
    2. Waits for you to press ENTER before each sample
    3. Reads exactly --frames consecutive valid feature lines from UART
    4. Saves each sample as a .npy file with shape (32, 10)
    5. Repeats until --samples samples are collected

Expected UART format from STM32 firmware (radar_features.c CSV mode):
    timestamp_ms,f0,f1,f2,f3,f4,f5,f6,f7,f8,f9
    123456,10.2,5.1,2.0,8.3,4.1,15.4,30.5,0.0,12.1,3.4

Feature order (columns 1-10, column 0 is timestamp):
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

Usage examples
--------------
    # Collect 50 'approaching' training samples
    python collect.py --port /dev/tty.usbmodem1102 \
                      --class_name approaching \
                      --samples 50 \
                      --output_dir radar_dataset/train/approaching

    # Collect 15 'idle' validation samples
    python collect.py --port /dev/tty.usbmodem1102 \
                      --class_name idle \
                      --samples 15 \
                      --output_dir radar_dataset/val/idle

    # Collect on Windows COM port
    python collect.py --port COM5 \
                      --class_name receding \
                      --samples 50 \
                      --output_dir radar_dataset/train/receding

Recommended sample counts
--------------------------
    train/  : 50-100 samples per class
    val/    : 15-20  samples per class
    test/   : 15-20  samples per class

Install dependency
------------------
    pip install pyserial
"""

import argparse
import os
import sys
import time
import numpy as np

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] pyserial is not installed.")
    print("        Run:  pip install pyserial")
    sys.exit(1)

# ── constants ─────────────────────────────────────────────────────────────────
FEATURE_COUNT  = 10    # number of features per frame (must match firmware)
CLASSES        = ["approaching", "receding", "idle", "mixed"]
EXPECTED_SHAPE = (32, 10)   # (frames, features) — must match model input


# ── helpers ───────────────────────────────────────────────────────────────────
def list_serial_ports() -> None:
    """Print all available serial ports to help the user find the right one."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("  No serial ports found.")
    else:
        for p in ports:
            print(f"  {p.device:25s}  {p.description}")


def parse_csv_line(line: str):
    """
    Parse one CSV line from the STM32 into a list of FEATURE_COUNT floats.

    Expected format:
        timestamp_ms,f0,f1,f2,...,f9
        123456,10.2,5.1,2.0,...

    Returns a list of FEATURE_COUNT floats, or None if the line is invalid.
    """
    try:
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("-"):
            return None   # skip header lines and separator lines

        parts = line.split(",")

        # We expect at least FEATURE_COUNT + 1 columns (timestamp + features)
        if len(parts) < FEATURE_COUNT + 1:
            return None

        # Column 0 is timestamp — skip it, take columns 1..FEATURE_COUNT
        values = [float(p.strip()) for p in parts[1: FEATURE_COUNT + 1]]

        if len(values) == FEATURE_COUNT:
            return values

    except (ValueError, IndexError):
        pass

    return None


def next_sample_index(output_dir: str) -> int:
    """
    Find the next available sample index so we never overwrite existing files.
    Scans for existing sample_XXXX.npy files and returns max_index + 1.
    """
    existing = [
        f for f in os.listdir(output_dir)
        if f.startswith("sample_") and f.endswith(".npy")
    ]
    if not existing:
        return 0
    indices = []
    for name in existing:
        try:
            idx = int(name.replace("sample_", "").replace(".npy", ""))
            indices.append(idx)
        except ValueError:
            pass
    return max(indices) + 1 if indices else 0


# ── core collection loop ──────────────────────────────────────────────────────
def collect(
    port:       str,
    baud:       int,
    class_name: str,
    num_frames: int,
    num_samples: int,
    output_dir: str,
    timeout:    float,
) -> None:
    """
    Main collection loop.

    Parameters
    ----------
    port        : serial port name  e.g. /dev/tty.usbmodem1102  or  COM5
    baud        : baud rate  (must match STM32 firmware)
    class_name  : one of approaching / receding / idle / mixed
    num_frames  : frames per sample  (32 — must match model input)
    num_samples : how many samples to collect this session
    output_dir  : folder where .npy files will be saved
    timeout     : serial read timeout in seconds
    """
    os.makedirs(output_dir, exist_ok=True)
    sample_idx = next_sample_index(output_dir)

    print(f"\n{'='*55}")
    print(f"  Radar Data Collector")
    print(f"{'='*55}")
    print(f"  Port        : {port}  @  {baud} baud")
    print(f"  Class       : {class_name}")
    print(f"  Frames/sample: {num_frames}")
    print(f"  Samples     : {num_samples}")
    print(f"  Output dir  : {output_dir}")
    print(f"  Starting at : sample_{sample_idx:04d}.npy")
    print(f"{'='*55}\n")

    # Open serial port
    try:
        ser = serial.Serial(port, baud, timeout=timeout)
    except serial.SerialException as e:
        print(f"[ERROR] Could not open port {port}: {e}")
        print("\nAvailable ports:")
        list_serial_ports()
        sys.exit(1)

    # Give STM32 time to stabilise after serial open (resets on some boards)
    print("Waiting 2 s for STM32 to stabilise …")
    time.sleep(2)
    ser.flushInput()
    print("Ready.\n")

    collected = 0

    try:
        while collected < num_samples:
            # ── prompt user before each sample ────────────────────────────
            print(f"Sample {collected + 1}/{num_samples}  "
                  f"(file: sample_{sample_idx:04d}.npy)")
            print(f"  Get into position for '{class_name}'.")
            input("  Press ENTER to start recording …")

            # ── flush stale data ──────────────────────────────────────────
            ser.flushInput()

            # ── collect num_frames valid lines ────────────────────────────
            frames      = []
            bad_lines   = 0
            max_bad     = 200   # give up after this many consecutive bad lines

            print(f"  Recording {num_frames} frames …", end=" ", flush=True)

            while len(frames) < num_frames:
                raw = ser.readline()

                try:
                    line = raw.decode("utf-8", errors="ignore")
                except Exception:
                    bad_lines += 1
                    continue

                values = parse_csv_line(line)

                if values is not None:
                    frames.append(values)
                    bad_lines = 0   # reset bad-line counter on success

                    # Show progress dots
                    if len(frames) % 4 == 0:
                        print(".", end="", flush=True)
                else:
                    bad_lines += 1
                    if bad_lines >= max_bad:
                        print(f"\n  [WARNING] {max_bad} consecutive unparseable "
                              f"lines. Check UART format and baud rate.")
                        bad_lines = 0

            # ── save sample ───────────────────────────────────────────────
            arr      = np.array(frames, dtype=np.float32)   # (32, 10)
            filename = f"sample_{sample_idx:04d}.npy"
            filepath = os.path.join(output_dir, filename)

            # Sanity check shape before saving
            if arr.shape != (num_frames, FEATURE_COUNT):
                print(f"\n  [ERROR] Unexpected array shape {arr.shape}, "
                      f"expected ({num_frames}, {FEATURE_COUNT}). Discarding.")
                continue

            np.save(filepath, arr)
            print(f"  ✓  saved → {filepath}  shape={arr.shape}")

            # Print a quick feature summary so you can sanity-check the values
            means = arr.mean(axis=0)
            print(f"     Mean features: "
                  f"app_e={means[0]:.2f}  rec_e={means[1]:.2f}  "
                  f"ratio={means[2]:.2f}  total_e={means[5]:.2f}  "
                  f"peak_dop={means[6]:.1f} Hz")
            print()

            sample_idx += 1
            collected  += 1

    except KeyboardInterrupt:
        print(f"\n\nInterrupted by user after {collected} samples.")

    finally:
        ser.close()

    # ── summary ───────────────────────────────────────────────────────────────
    print(f"\n{'='*55}")
    print(f"  Collection complete.")
    print(f"  Saved {collected} samples to: {output_dir}")
    print(f"{'='*55}\n")


# ── main ───────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Collect radar feature data from STM32 UART and save as .npy files."
    )
    parser.add_argument(
        "--port", required=True,
        help="Serial port, e.g. /dev/tty.usbmodem1102  or  COM5"
    )
    parser.add_argument(
        "--baud", type=int, default=576000,
        help="Baud rate (default: 576000 — must match STM32 firmware)."
    )
    parser.add_argument(
        "--class_name", required=True, choices=CLASSES,
        help="Motion class label for this recording session."
    )
    parser.add_argument(
        "--frames", type=int, default=32,
        help="Number of frames per sample (default: 32 — must match model input)."
    )
    parser.add_argument(
        "--samples", type=int, default=50,
        help="Number of samples to collect this session (default: 50)."
    )
    parser.add_argument(
        "--output_dir", required=True,
        help="Folder to save .npy files, e.g. radar_dataset/train/approaching"
    )
    parser.add_argument(
        "--timeout", type=float, default=5.0,
        help="Serial read timeout in seconds (default: 5.0)."
    )
    parser.add_argument(
        "--list_ports", action="store_true",
        help="List available serial ports and exit."
    )
    args = parser.parse_args()

    if args.list_ports:
        print("Available serial ports:")
        list_serial_ports()
        sys.exit(0)

    collect(
        port        = args.port,
        baud        = args.baud,
        class_name  = args.class_name,
        num_frames  = args.frames,
        num_samples = args.samples,
        output_dir  = args.output_dir,
        timeout     = args.timeout,
    )


if __name__ == "__main__":
    main()
