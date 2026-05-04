# Static FFT + Spectrogram capture script
# Based on FFT+controls+magnitude.py from radar_prj
# STM32 sends: "DATA: XX XX XX XX XX XX\r\n" (ASCII hex)
#
# Panels produced:
#   1. Time domain  (I and Q waveforms)
#   2. Single FFT slice  (Hamming window, centred at 0 Hz)
#   3. Spectrogram / waterfall  (STFT, frequency × time, inferno colormap)
#      – same style as the live waterfall in the main script

import re
import serial
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors

# ── Config ────────────────────────────────────────────────────────────────────
PORT          = "/dev/tty.usbmodem1102"   # adjust to your port
BAUDRATE      = 576000

# Total samples to collect.  More samples = taller / higher-res spectrogram.
# Must be >= FFT_LEN.  e.g. 8192 gives ~8 rows of 1024-pt FFTs.
N_SAMPLES     = 8192

FFT_LEN       = 1024     # points per FFT slice (also used for the single FFT panel)
HOP           = 256      # step between successive STFT frames (overlap = FFT_LEN - HOP)

SAMPLING_RATE = 3904     # Hz
FSR           = 0.15     # full-scale range (V)
STEP          = FSR / (2**23)
EPSILON       = 1e-12

# Spectrogram colour limits (dB, normalised to 0 dB peak of the whole capture)
CLIM_MIN      = -60.0
CLIM_MAX      =   0.0
# ──────────────────────────────────────────────────────────────────────────────


def bytes_to_int24(b0, b1, b2):
    """Three individual byte values → signed 24-bit integer."""
    val = (b0 << 16) | (b1 << 8) | b2
    if val & 0x800000:
        val -= 1 << 24
    return val


def collect_samples(port, baud, n_samples):
    """Read ASCII hex lines from STM32 until n_samples are collected."""
    ch1, ch2 = [], []
    pattern = re.compile(
        r"DATA:\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})"
        r"\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})"
    )
    print(f"Opening {port} @ {baud} baud …")
    with serial.Serial(port, baud, timeout=2) as ser:
        ser.reset_input_buffer()
        print(f"Collecting {n_samples} samples …")
        while len(ch1) < n_samples:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("ascii", errors="ignore").strip()
            except Exception:
                continue
            m = pattern.search(line)
            if m:
                b = [int(x, 16) for x in m.groups()]
                ch1.append(bytes_to_int24(b[0], b[1], b[2]) * STEP)   # CH0 → I
                ch2.append(bytes_to_int24(b[3], b[4], b[5]) * STEP)   # CH1 → Q
                if len(ch1) % 256 == 0:
                    print(f"  {len(ch1)}/{n_samples}")
    print("Done collecting.")
    return np.array(ch1), np.array(ch2)


def compute_fft(i_data, q_data, n=None):
    """Single complex FFT with Hamming window → (freqs Hz, magnitude dB)."""
    if n is None:
        n = len(i_data)
    win = np.hamming(n)
    sig = (i_data[:n] + 1j * q_data[:n]) * win
    spectrum = np.fft.fftshift(np.fft.fft(sig, n=n))
    mag = np.abs(spectrum) + EPSILON
    mag_db = 20.0 * np.log10(mag / (np.max(mag) + EPSILON))
    freqs = np.linspace(-SAMPLING_RATE / 2, SAMPLING_RATE / 2, n)
    return freqs, mag_db


def compute_spectrogram(i_data, q_data, fft_len, hop):
    """
    Short-time Fourier transform of the complex IQ signal.
    Returns:
        freqs  – 1-D array of centre frequencies (Hz), 0-centred
        times  – 1-D array of frame centre times (s)
        sxx_db – 2-D array [n_frames × fft_len] in dB (normalised to global peak)
    """
    n_total = len(i_data)
    win = np.hamming(fft_len)
    frames = []
    starts = range(0, n_total - fft_len + 1, hop)
    for s in starts:
        seg = (i_data[s:s + fft_len] + 1j * q_data[s:s + fft_len]) * win
        spectrum = np.fft.fftshift(np.fft.fft(seg, n=fft_len))
        frames.append(np.abs(spectrum) + EPSILON)

    sxx = np.array(frames)                         # [n_frames, fft_len]
    global_max = np.max(sxx)
    sxx_db = 20.0 * np.log10(sxx / global_max)
    sxx_db = np.clip(sxx_db, -120.0, 0.0)

    freqs = np.linspace(-SAMPLING_RATE / 2, SAMPLING_RATE / 2, fft_len)
    # Centre time of each frame (seconds)
    times = np.array([s + fft_len // 2 for s in starts]) / SAMPLING_RATE
    return freqs, times, sxx_db


def doppler_estimate(i_data, q_data):
    sig   = i_data + 1j * q_data
    phase = np.unwrap(np.angle(sig))
    dphase = np.diff(phase)
    fd = np.median((dphase / (2.0 * np.pi)) * SAMPLING_RATE)
    v  = (fd * 3e8) / (2.0 * 24e9)
    direction = "approaching" if fd > 0 else ("receding" if fd < 0 else "stationary")
    return fd, v, direction


def plot(i_data, q_data):
    freqs_fft, mag_db = compute_fft(i_data, q_data, n=FFT_LEN)
    freqs_sg, times_sg, sxx_db = compute_spectrogram(i_data, q_data, FFT_LEN, HOP)
    fd, v, direction = doppler_estimate(i_data, q_data)

    fig = plt.figure(figsize=(13, 11))
    fig.suptitle(
        f"Static capture  –  {N_SAMPLES} samples @ {SAMPLING_RATE} Hz"
        f"  |  FFT N={FFT_LEN}  hop={HOP}",
        fontsize=12
    )

    gs = fig.add_gridspec(3, 2, width_ratios=[1, 0.03],
                          hspace=0.40, wspace=0.08)
    ax_t  = fig.add_subplot(gs[0, 0])
    ax_f  = fig.add_subplot(gs[1, 0])
    ax_sg = fig.add_subplot(gs[2, 0])
    ax_cb = fig.add_subplot(gs[2, 1])   # colourbar axis

    # ── 1. Time domain ───────────────────────────────────────────────────────
    t_ms = np.arange(len(i_data)) / SAMPLING_RATE * 1000
    ax_t.plot(t_ms, i_data, linewidth=0.7, label="CH0 (I)")
    ax_t.plot(t_ms, q_data, linewidth=0.7, label="CH1 (Q)", alpha=0.8)
    ax_t.set_xlabel("Time (ms)")
    ax_t.set_ylabel("Amplitude (V)")
    ax_t.set_title("Time domain")
    ax_t.legend(fontsize=8)
    ax_t.grid(True, alpha=0.25)

    # ── 2. Single FFT slice ──────────────────────────────────────────────────
    ax_f.plot(freqs_fft, mag_db, color="gold", linewidth=0.9)
    ax_f.set_xlabel("Frequency (Hz)")
    ax_f.set_ylabel("Magnitude (dB)")
    ax_f.set_title(f"Single FFT slice  (Hamming, N={FFT_LEN})")
    ax_f.set_xlim(-SAMPLING_RATE / 2, SAMPLING_RATE / 2)
    ax_f.set_ylim(-80, 5)
    ax_f.grid(True, alpha=0.25)
    # peak marker
    pk_idx  = np.argmax(mag_db)
    pk_freq = freqs_fft[pk_idx]
    ax_f.axvline(pk_freq, color="red", linestyle="--", linewidth=0.9,
                 label=f"Peak {pk_freq:.1f} Hz")
    ax_f.legend(fontsize=8)

    # ── 3. Spectrogram / waterfall ───────────────────────────────────────────
    # extent: [freq_min, freq_max, time_min, time_max]  (time in ms for readability)
    t_min_ms = times_sg[0]  * 1000
    t_max_ms = times_sg[-1] * 1000
    f_min    = freqs_sg[0]
    f_max    = freqs_sg[-1]

    im = ax_sg.imshow(
        sxx_db,                          # [n_frames × fft_len]
        aspect="auto",
        origin="lower",
        extent=[f_min, f_max, t_min_ms, t_max_ms],
        cmap="inferno",
        vmin=CLIM_MIN,
        vmax=CLIM_MAX,
        interpolation="nearest",
    )
    ax_sg.set_xlabel("Frequency (Hz)")
    ax_sg.set_ylabel("Time (ms)")
    ax_sg.set_title(f"Spectrogram  (STFT, hop={HOP} samples = {HOP/SAMPLING_RATE*1000:.1f} ms)")
    ax_sg.set_xlim(-SAMPLING_RATE / 2, SAMPLING_RATE / 2)

    # Colourbar
    cb = fig.colorbar(im, cax=ax_cb)
    cb.set_label("Power (dB)", fontsize=8)

    # ── Doppler info banner ───────────────────────────────────────────────────
    info = (f"Doppler  fd = {fd:.2f} Hz  |  v ≈ {v:.4f} m/s  |  {direction}"
            f"  |  colour range [{CLIM_MIN}, {CLIM_MAX}] dB")
    fig.text(0.5, 0.005, info, ha="center", fontsize=10,
             bbox=dict(facecolor="lightyellow", alpha=0.85, boxstyle="round,pad=0.3"))

    plt.savefig("static_fft.png", dpi=150, bbox_inches="tight")
    print("Saved  static_fft.png")
    plt.show()


if __name__ == "__main__":
    i_data, q_data = collect_samples(PORT, BAUDRATE, N_SAMPLES)
    plot(i_data, q_data)

