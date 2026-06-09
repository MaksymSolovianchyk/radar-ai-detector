"""
inspect_wav.py
--------------
Reads a radar WAV file saved by the STM32N6 and plots:
  1. Time domain — CH0 (I) and CH1 (Q) in volts
  2. FFT spectrum — magnitude vs frequency (Hz)
  3. Doppler spectrogram (waterfall)

Usage:
    python inspect_wav.py REC_0000.WAV
    python inspect_wav.py REC_0000.WAV --seconds 2    (plot only first 2s)

Requirements:
    pip install numpy matplotlib scipy
"""

import argparse
import sys
import numpy as np
import matplotlib.pyplot as plt
import scipy.io.wavfile as wav

# ── ADC constants ─────────────────────────────────────────────────────────────
FSR  = 0.15           # full-scale range V (gain=8)
STEP = FSR / 8388608  # V per count (FSR / 2^23)

N_FFT    = 256        # FFT size matching firmware
DC_GUARD = 2          # bins to ignore around DC


def load_wav(path):
    """Load WAV file and return (sample_rate, ch0_volts, ch1_volts)."""
    rate, data = wav.read(path)

    # scipy reads 24-bit WAV as int32
    if data.ndim == 1:
        print("WARNING: mono file detected — expected stereo (CH0 + CH1)")
        ch0 = data.astype(np.float32) * STEP
        ch1 = ch0.copy()
    else:
        ch0 = data[:, 0].astype(np.float32) * STEP
        ch1 = data[:, 1].astype(np.float32) * STEP

    return rate, ch0, ch1


def compute_fft(signal, fs, n_fft):
    """Compute one-sided FFT magnitude spectrum."""
    win  = np.hamming(len(signal))
    X    = np.fft.fft(signal * win, n_fft)
    mag  = np.abs(X[:n_fft // 2]) * 2 / n_fft
    freq = np.fft.fftfreq(n_fft, d=1.0 / fs)[:n_fft // 2]
    return freq, mag


def compute_spectrogram(signal, fs, n_fft):
    """Compute Doppler spectrogram (waterfall) — full two-sided."""
    n_frames = len(signal) // n_fft
    spec     = np.zeros((n_fft, n_frames))
    win      = np.hamming(n_fft)

    for k in range(n_frames):
        frame        = signal[k * n_fft : (k + 1) * n_fft]
        X            = np.fft.fftshift(np.fft.fft(frame * win, n_fft))
        spec[:, k]   = np.abs(X)

    freq_ax  = np.fft.fftshift(np.fft.fftfreq(n_fft, d=1.0 / fs))
    time_ax  = np.arange(n_frames) * n_fft / fs
    return freq_ax, time_ax, spec


def dominant_doppler(ch0, ch1, fs, n_fft):
    """Compute dominant Doppler frequency from IQ signal."""
    n_frames = len(ch0) // n_fft
    fds, vels, directions = [], [], []
    lambda_ = 0.01240   # 24.2 GHz

    for k in range(n_frames):
        iq  = (ch0[k*n_fft:(k+1)*n_fft] +
               1j * ch1[k*n_fft:(k+1)*n_fft])
        win = np.hamming(n_fft)
        X   = np.fft.fftshift(np.fft.fft(iq * win, n_fft))
        mag = np.abs(X)

        centre = n_fft // 2
        mag[centre - DC_GUARD : centre + DC_GUARD] = 0

        peak    = np.argmax(mag)
        freqs   = np.fft.fftshift(np.fft.fftfreq(n_fft, d=1.0 / fs))
        fd      = freqs[peak]
        vel     = fd * lambda_ / 2
        fds.append(fd)
        vels.append(vel)
        directions.append("approaching" if fd > 0 else
                          ("receding"   if fd < 0 else "idle"))

    return fds, vels, directions


def main():
    parser = argparse.ArgumentParser(
        description="Inspect radar WAV file from STM32N6 SD card")
    parser.add_argument("wavfile", help="Path to WAV file (e.g. REC_0000.WAV)")
    parser.add_argument("--seconds", type=float, default=None,
                        help="Plot only first N seconds (default: all)")
    args = parser.parse_args()

    # ── Load ─────────────────────────────────────────────────────────────────
    print(f"Loading {args.wavfile} ...")
    try:
        fs, ch0, ch1 = load_wav(args.wavfile)
    except FileNotFoundError:
        print(f"ERROR: file not found: {args.wavfile}")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR reading WAV: {e}")
        sys.exit(1)

    total_samples = len(ch0)
    duration      = total_samples / fs

    print(f"  Sample rate : {fs} Hz")
    print(f"  Samples     : {total_samples}")
    print(f"  Duration    : {duration:.2f} seconds")
    print(f"  CH0 range   : {ch0.min():.6f} V to {ch0.max():.6f} V")
    print(f"  CH1 range   : {ch1.min():.6f} V to {ch1.max():.6f} V")

    # Trim to requested duration
    if args.seconds is not None:
        n = min(int(args.seconds * fs), total_samples)
        ch0 = ch0[:n]
        ch1 = ch1[:n]
        print(f"  Plotting first {args.seconds:.1f} seconds ({n} samples)")

    t = np.arange(len(ch0)) / fs * 1000   # time axis in ms

    # ── Doppler analysis ──────────────────────────────────────────────────────
    fds, vels, directions = dominant_doppler(ch0, ch1, fs, N_FFT)
    print(f"\n  Dominant Doppler (frame averages):")
    print(f"    Mean fd   : {np.mean(fds):.1f} Hz")
    print(f"    Mean vel  : {np.mean(vels):.3f} m/s")
    from collections import Counter
    dir_count = Counter(directions)
    print(f"    Directions: {dict(dir_count)}")

    # ── FFT of full signal ────────────────────────────────────────────────────
    freq_fft, mag_fft_ch0 = compute_fft(ch0, fs, N_FFT)
    _,        mag_fft_ch1 = compute_fft(ch1, fs, N_FFT)

    # ── Spectrogram ───────────────────────────────────────────────────────────
    freq_spec, time_spec, spec_ch0 = compute_spectrogram(ch0, fs, N_FFT)

    spec_dB = 20 * np.log10(spec_ch0 / (spec_ch0.max() + 1e-10) + 1e-10)

    # ── Plot ──────────────────────────────────────────────────────────────────
    fig, axes = plt.subplots(3, 1, figsize=(13, 10))
    fig.patch.set_facecolor("white")
    fig.suptitle(f"Radar WAV Inspection — {args.wavfile}\n"
                 f"fs={fs} Hz  |  {len(ch0)} samples  |  "
                 f"{len(ch0)/fs:.1f} s",
                 fontsize=12, fontweight="bold")

    # ── Plot 1: Time domain ───────────────────────────────────────────────────
    ax1 = axes[0]
    ax1.plot(t, ch0, color="#1f77b4", linewidth=0.6, label="CH0 (I)")
    ax1.plot(t, ch1, color="#ff7f0e", linewidth=0.6, label="CH1 (Q)",
             alpha=0.8)
    ax1.set_xlabel("Time (ms)", fontsize=10)
    ax1.set_ylabel("Voltage (V)", fontsize=10)
    ax1.set_title("Time Domain", fontsize=11, fontweight="bold")
    ax1.legend(loc="upper right", fontsize=9)
    ax1.grid(True, alpha=0.3)
    ax1.set_facecolor("#f8f8f8")

    # ── Plot 2: FFT spectrum ──────────────────────────────────────────────────
    ax2 = axes[1]
    ax2.plot(freq_fft, mag_fft_ch0, color="#1f77b4",
             linewidth=1.0, label="CH0 (I)")
    ax2.plot(freq_fft, mag_fft_ch1, color="#ff7f0e",
             linewidth=1.0, label="CH1 (Q)", alpha=0.8)

    # Mark dominant peak
    peak_idx = np.argmax(mag_fft_ch0)
    ax2.axvline(freq_fft[peak_idx], color="red", linestyle="--",
                linewidth=1.2, alpha=0.7,
                label=f"Peak: {freq_fft[peak_idx]:.1f} Hz")
    ax2.set_xlabel("Frequency (Hz)", fontsize=10)
    ax2.set_ylabel("Magnitude (V)", fontsize=10)
    ax2.set_title("FFT Spectrum", fontsize=11, fontweight="bold")
    ax2.legend(loc="upper right", fontsize=9)
    ax2.grid(True, alpha=0.3)
    ax2.set_facecolor("#f8f8f8")

    # ── Plot 3: Spectrogram (waterfall) ───────────────────────────────────────
    ax3 = axes[2]
    img = ax3.imshow(spec_dB,
                     aspect="auto",
                     origin="lower",
                     extent=[time_spec[0]*1000, time_spec[-1]*1000,
                             freq_spec[0],      freq_spec[-1]],
                     cmap="inferno",
                     vmin=-8, vmax=0)
    plt.colorbar(img, ax=ax3, label="Power (dB)")
    ax3.axhline(0, color="white", linestyle="--",
                linewidth=1.0, alpha=0.5, label="DC (0 Hz)")
    ax3.set_xlabel("Time (ms)", fontsize=10)
    ax3.set_ylabel("Doppler Frequency (Hz)", fontsize=10)
    ax3.set_title("Doppler Spectrogram (Waterfall)", fontsize=11,
                  fontweight="bold")
    ax3.legend(loc="upper right", fontsize=9)
    ax3.set_facecolor("black")

    plt.tight_layout()
    plt.show()

    print("\nPlot displayed. Close the window to exit.")


if __name__ == "__main__":
    main()
