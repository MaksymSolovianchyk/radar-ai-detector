"""
live_radar_plot.py
------------------
Live streaming visualization of radar CH0 (I) and CH1 (Q) channels
from STM32N6 via UART. Displays time domain plot similar to reference image
with Doppler frequency, velocity, and energy annotations.

Usage:
    python live_radar_plot.py --port COM3
    python live_radar_plot.py --port /dev/ttyUSB0 --baud 576000

Requirements:
    pip install pyserial matplotlib numpy
"""

import argparse
import threading
import time
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial

# ── Configuration ────────────────────────────────────────────────────────────
FS          = 3904          # ADC sample rate Hz
N_FFT       = 256           # FFT size
LAMBDA      = 0.01240       # radar wavelength m (24.2 GHz)
FSR         = 0.15          # ADC full-scale range V (gain=8)
STEP        = FSR / 8388608 # V per count
BUFFER_SIZE = 5000          # samples to display on x-axis
DC_GUARD    = 2             # FFT bins to ignore around DC

# ── Shared data ──────────────────────────────────────────────────────────────
ch1_data  = np.zeros(BUFFER_SIZE)
ch2_data  = np.zeros(BUFFER_SIZE)
write_idx = 0
lock      = threading.Lock()

# ── Helpers ──────────────────────────────────────────────────────────────────
def bytes_to_int24(b):
    val = b[0] << 16 | b[1] << 8 | b[2]
    if val & 0x800000:
        val -= 1 << 24
    return val


def compute_doppler(ch1_frame, ch2_frame):
    """Compute dominant Doppler frequency and velocity from one FFT frame."""
    win     = np.hamming(N_FFT)
    iq      = (ch1_frame + 1j * ch2_frame) * win
    X       = np.fft.fftshift(np.fft.fft(iq, N_FFT))
    mag     = np.abs(X)
    freqs   = np.fft.fftshift(np.fft.fftfreq(N_FFT, d=1.0/FS))

    # mask DC guard
    centre  = N_FFT // 2
    mag[centre - DC_GUARD : centre + DC_GUARD] = 0

    peak_idx = np.argmax(mag)
    fd       = freqs[peak_idx]
    velocity = fd * LAMBDA / 2.0

    # direction
    direction = "approaching" if fd > 0 else ("receding" if fd < 0 else "idle")

    # energy — sum of squared magnitudes (excluding DC guard)
    mask   = np.ones(N_FFT, dtype=bool)
    mask[centre - DC_GUARD : centre + DC_GUARD] = False
    energy = np.sum(mag[mask] ** 2) / (N_FFT ** 2)

    return fd, velocity, direction, energy


def parse_line(line: str):
    """
    Parse UART line. Supports two formats:
      Format A (raw binary decoded):  CH0: -23449 -0.000419V | CH1: -25575 -0.000457V
      Format B (voltage only):        -0.000419 -0.000457
    Returns (ch0_v, ch1_v) floats or None on parse error.
    """
    line = line.strip()
    try:
        if line.startswith("CH0:"):
            # Format A
            parts = line.replace("|", "").replace("V", "").split()
            # parts: CH0: raw_ch0 voltage CH1: raw_ch1 voltage
            ch0_v = float(parts[2])
            ch1_v = float(parts[5])
        else:
            # Format B — two floats separated by space
            parts = line.split()
            if len(parts) >= 2:
                ch0_v = float(parts[0])
                ch1_v = float(parts[1])
            else:
                return None
        return ch0_v, ch1_v
    except (ValueError, IndexError):
        return None


# ── Serial reader thread ──────────────────────────────────────────────────────
def reader_thread(port: str, baud: int):
    global write_idx, ch1_data, ch2_data

    print(f"Connecting to {port} at {baud} baud...")
    while True:
        try:
            ser = serial.Serial(port, baud, timeout=1)
            print(f"Connected to {port}")
            break
        except serial.SerialException as e:
            print(f"Cannot open port: {e}. Retrying in 2s...")
            time.sleep(2)

    leftover = b""
    while True:
        try:
            buf = ser.read(ser.in_waiting or 64)
        except serial.SerialException as e:
            print(f"Serial error: {e}")
            time.sleep(1)
            continue

        if not buf:
            time.sleep(0.005)
            continue

        buf = leftover + buf
        lines = buf.split(b"\n")
        leftover = lines[-1]   # incomplete line saved for next iteration

        for raw_line in lines[:-1]:
            result = parse_line(raw_line.decode("utf-8", errors="ignore"))
            if result is None:
                continue
            ch0_v, ch1_v = result
            with lock:
                ch1_data[write_idx] = ch0_v
                ch2_data[write_idx] = ch1_v
                write_idx = (write_idx + 1) % BUFFER_SIZE


# ── Plot setup ────────────────────────────────────────────────────────────────
def build_plot():
    fig, ax = plt.subplots(figsize=(12, 4))
    fig.patch.set_facecolor("black")
    ax.set_facecolor("black")

    line1, = ax.plot([], [], color="#1f77b4", linewidth=0.8, label="Channel 1 (I)")
    line2, = ax.plot([], [], color="#ff7f0e", linewidth=0.8, label="Channel 2 (Q)")

    ax.set_xlim(0, BUFFER_SIZE)
    ax.set_ylim(-0.15, 0.15)
    ax.set_xlabel("fft slice", color="white", fontsize=11)
    ax.set_ylabel("Voltage (V)", color="white", fontsize=11)
    ax.set_title("time domain", color="white", fontsize=13)
    ax.tick_params(colors="white")
    for spine in ax.spines.values():
        spine.set_edgecolor("white")
    ax.legend(loc="upper right", facecolor="#1a1a1a", edgecolor="white",
              labelcolor="white", fontsize=9)

    # Annotation box (top-left, matches reference image style)
    info_text = ax.text(
        0.01, 0.97, "",
        transform=ax.transAxes,
        fontsize=9,
        verticalalignment="top",
        bbox=dict(boxstyle="round", facecolor="white", alpha=0.85),
        color="black"
    )

    return fig, ax, line1, line2, info_text


# ── Animation update ──────────────────────────────────────────────────────────
def update(frame, line1, line2, info_text):
    with lock:
        # Reorder buffer so oldest sample is at index 0
        idx  = write_idx
        d1   = np.roll(ch1_data, -idx)
        d2   = np.roll(ch2_data, -idx)

    x = np.arange(BUFFER_SIZE)
    line1.set_data(x, d1)
    line2.set_data(x, d2)

    # Compute Doppler on the most recent N_FFT samples
    recent_ch1 = d1[-N_FFT:]
    recent_ch2 = d2[-N_FFT:]

    if np.max(np.abs(recent_ch1)) > 1e-9:
        fd, vel, direction, energy = compute_doppler(recent_ch1, recent_ch2)
        info = (f"{direction}\n"
                f"fd = {fd:.1f} Hz\n"
                f"v = {vel:.2f} m/s, Energy={energy:.2e}")
    else:
        info = "waiting for data..."

    info_text.set_text(info)

    # Auto-scale y axis with small padding
    all_vals = np.concatenate([d1, d2])
    vmax = max(np.max(np.abs(all_vals)) * 1.3, 0.001)
    line1.axes.set_ylim(-vmax, vmax)

    return line1, line2, info_text


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Live radar IQ plot")
    parser.add_argument("--port",  default="COM3",   help="Serial port")
    parser.add_argument("--baud",  type=int, default=576000, help="Baud rate")
    parser.add_argument("--demo",  action="store_true",
                        help="Run with synthetic data (no hardware needed)")
    args = parser.parse_args()

    if args.demo:
        # Generate synthetic Doppler signal for testing without hardware
        print("Running in DEMO mode — no serial port needed.")
        def demo_thread():
            global write_idx, ch1_data, ch2_data
            t = 0.0
            while True:
                f_dop = 80 * np.sin(2 * np.pi * 0.3 * t)
                ch0_v = 0.06 * np.cos(2 * np.pi * f_dop * t) + \
                        0.005 * np.random.randn()
                ch1_v = 0.06 * np.sin(2 * np.pi * f_dop * t) + \
                        0.005 * np.random.randn()
                with lock:
                    ch1_data[write_idx] = ch0_v
                    ch2_data[write_idx] = ch1_v
                    write_idx = (write_idx + 1) % BUFFER_SIZE
                t += 1.0 / FS
                time.sleep(1.0 / FS)

        t = threading.Thread(target=demo_thread, daemon=True)
        t.start()
    else:
        t = threading.Thread(
            target=reader_thread, args=(args.port, args.baud), daemon=True)
        t.start()

    fig, ax, line1, line2, info_text = build_plot()

    ani = animation.FuncAnimation(
        fig, update,
        fargs=(line1, line2, info_text),
        interval=50,       # refresh every 50ms
        blit=True,
        cache_frame_data=False
    )

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
