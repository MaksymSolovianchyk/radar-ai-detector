# Live FFT + Spectrogram
# Based on static_fft.py and FFT+controls+magnitude.py from radar_prj
# STM32 sends: "DATA: XX XX XX XX XX XX\r\n" (ASCII hex)
#
# Panels:
#   1. Time domain  (rolling window)
#   2. Single FFT slice  (Hamming, centred at 0 Hz)
#   3. Spectrogram / waterfall  (STFT, scrolling upward like the live script)

import re
import threading
import time
import serial
import numpy as np

import matplotlib
# Opens a real detached OS window.
# Requires tkinter — install if missing:
#   macOS:   brew install python-tk
#   Ubuntu:  sudo apt install python3-tk
# Swap "TkAgg" → "Qt5Agg" if you have PyQt5 instead.
matplotlib.use("TkAgg")

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Slider

# ── Config ────────────────────────────────────────────────────────────────────
PORT          = "/dev/tty.usbmodem1102"   # adjust to your port
BAUDRATE      = 576000

BUFFER_SIZE   = 8192     # total rolling sample buffer
FFT_LEN       = 1024     # points per FFT / STFT frame
HOP           = 256      # STFT hop size (samples between frames)
SAMPLING_RATE = 3904     # Hz
FSR           = 0.15     # full-scale range (V)
STEP          = FSR / (2**23)
EPSILON       = 1e-12

# How many STFT rows to show in the waterfall (history depth).
# Each row = HOP samples = HOP/SAMPLING_RATE seconds.
# e.g. 200 rows × 256/3904 s ≈ 13 s of history
WATERFALL_ROWS = 200

# Animation update interval (ms) — lower = smoother but heavier CPU
UPDATE_MS = 80
# ──────────────────────────────────────────────────────────────────────────────

# ── Shared state ──────────────────────────────────────────────────────────────
i_buf      = np.zeros(BUFFER_SIZE)
q_buf      = np.zeros(BUFFER_SIZE)
write_idx  = 0
buf_lock   = threading.Lock()

# Waterfall buffer: rows × FFT_LEN
waterfall  = np.full((WATERFALL_ROWS, FFT_LEN), -120.0)
wf_lock    = threading.Lock()

doppler_info = {"fd": 0.0, "v": 0.0, "direction": "—"}
# ──────────────────────────────────────────────────────────────────────────────


def bytes_to_int24(b0, b1, b2):
    val = (b0 << 16) | (b1 << 8) | b2
    if val & 0x800000:
        val -= 1 << 24
    return val


# ── Serial reader thread ──────────────────────────────────────────────────────
def reader_thread():
    global write_idx
    pattern = re.compile(
        r"DATA:\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})"
        r"\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})"
    )
    leftover = b""
    while True:
        try:
            ser = serial.Serial(PORT, BAUDRATE, timeout=1)
            ser.reset_input_buffer()
            print(f"Serial opened: {PORT}")
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                line = (leftover + raw).decode("ascii", errors="ignore").strip()
                leftover = b""
                m = pattern.search(line)
                if m:
                    b = [int(x, 16) for x in m.groups()]
                    i_val = bytes_to_int24(b[0], b[1], b[2]) * STEP
                    q_val = bytes_to_int24(b[3], b[4], b[5]) * STEP
                    with buf_lock:
                        i_buf[write_idx] = i_val
                        q_buf[write_idx] = q_val
                        write_idx = (write_idx + 1) % BUFFER_SIZE
        except serial.SerialException as e:
            print(f"Serial error: {e}  — retrying in 2 s")
            time.sleep(2)


# ── FFT / STFT worker thread ──────────────────────────────────────────────────
def fft_thread():
    win = np.hamming(FFT_LEN)
    while True:
        with buf_lock:
            idx = np.arange(write_idx - BUFFER_SIZE, write_idx) % BUFFER_SIZE
            i_snap = i_buf[idx].copy()
            q_snap = q_buf[idx].copy()

        # Single FFT on the most recent FFT_LEN samples
        seg_i = i_snap[-FFT_LEN:]
        seg_q = q_snap[-FFT_LEN:]
        sig   = (seg_i + 1j * seg_q) * win
        spec  = np.fft.fftshift(np.fft.fft(sig, n=FFT_LEN))
        mag   = np.abs(spec) + EPSILON
        fft_db = np.clip(20.0 * np.log10(mag / (np.max(mag) + EPSILON)), -120.0, 0.0)

        # Compute ONE new STFT row from the most recent FFT_LEN samples,
        # then scroll the waterfall up (oldest row drops off, newest appended).
        new_frame  = (i_snap[-FFT_LEN:] + 1j * q_snap[-FFT_LEN:]) * win
        new_mag    = np.abs(np.fft.fftshift(np.fft.fft(new_frame, n=FFT_LEN))) + EPSILON
        new_row_db = np.clip(20.0 * np.log10(new_mag / (np.max(new_mag) + EPSILON)),
                             -120.0, 0.0)
        with wf_lock:
            waterfall[:-1] = waterfall[1:]   # scroll up
            waterfall[-1]  = new_row_db       # newest row at bottom

        # Doppler estimate via instantaneous frequency
        sig_raw  = i_snap[-FFT_LEN:] + 1j * q_snap[-FFT_LEN:]
        phase    = np.unwrap(np.angle(sig_raw))
        dphase   = np.diff(phase)
        fd       = float(np.median((dphase / (2.0 * np.pi)) * SAMPLING_RATE))
        v        = (fd * 3e8) / (2.0 * 24e9)
        direction = "approaching" if fd > 0 else ("receding" if fd < 0 else "stationary")
        doppler_info.update({"fd": fd, "v": v, "direction": direction, "fft_db": fft_db})

        time.sleep(UPDATE_MS / 1000.0 * 0.5)   # run ~2× faster than display


# ── Build figure ──────────────────────────────────────────────────────────────
fig = plt.figure(figsize=(13, 11))
fig.suptitle("Live FFT + Spectrogram", fontsize=12)

# Leave room at bottom for sliders
plt.subplots_adjust(left=0.08, right=0.93, top=0.93, bottom=0.18, hspace=0.40)

gs = fig.add_gridspec(3, 2, width_ratios=[1, 0.025],
                      hspace=0.40, wspace=0.06,
                      top=0.93, bottom=0.18,
                      left=0.08, right=0.93)
ax_t  = fig.add_subplot(gs[0, 0])
ax_f  = fig.add_subplot(gs[1, 0])
ax_sg = fig.add_subplot(gs[2, 0])
ax_cb = fig.add_subplot(gs[2, 1])

freqs = np.linspace(-SAMPLING_RATE / 2, SAMPLING_RATE / 2, FFT_LEN)
t_ms  = np.arange(BUFFER_SIZE) / SAMPLING_RATE * 1000   # full buffer timeline

# Time domain
line_i, = ax_t.plot(t_ms, np.zeros(BUFFER_SIZE), lw=0.7, label="CH0 (I)")
line_q, = ax_t.plot(t_ms, np.zeros(BUFFER_SIZE), lw=0.7, label="CH1 (Q)", alpha=0.8)
ax_t.set_xlim(0, t_ms[-1])
ax_t.set_ylim(-FSR, FSR)
ax_t.set_xlabel("Time (ms)")
ax_t.set_ylabel("Amplitude (V)")
ax_t.set_title("Time domain  (rolling)")
ax_t.legend(fontsize=8, loc="upper right")
ax_t.grid(True, alpha=0.25)

# FFT slice
line_fft, = ax_f.plot(freqs, np.full(FFT_LEN, -120.0), color="gold", lw=0.9)
vline_peak = ax_f.axvline(0, color="red", lw=0.9, linestyle="--")
ax_f.set_xlim(-SAMPLING_RATE / 2, SAMPLING_RATE / 2)
ax_f.set_ylim(-80, 5)
ax_f.set_xlabel("Frequency (Hz)")
ax_f.set_ylabel("Magnitude (dB)")
ax_f.set_title(f"FFT slice  (Hamming, N={FFT_LEN})")
ax_f.grid(True, alpha=0.25)
peak_label = ax_f.text(0.02, 0.93, "", transform=ax_f.transAxes,
                       fontsize=8, color="red")

# Spectrogram
im = ax_sg.imshow(
    waterfall,
    aspect="auto",
    origin="lower",
    extent=[-SAMPLING_RATE / 2, SAMPLING_RATE / 2, 0, WATERFALL_ROWS],
    cmap="inferno",
    vmin=-60, vmax=0,
    interpolation="nearest",
)
ax_sg.set_xlabel("Frequency (Hz)")
ax_sg.set_ylabel("Frame index (oldest → newest)")
ax_sg.set_title(f"Spectrogram  (STFT hop={HOP})")
ax_sg.set_xlim(-SAMPLING_RATE / 2, SAMPLING_RATE / 2)
fig.colorbar(im, cax=ax_cb, label="dB")

# Info text
info_text = fig.text(0.5, 0.13, "", ha="center", fontsize=10,
                     bbox=dict(facecolor="lightyellow", alpha=0.85,
                               boxstyle="round,pad=0.3"))

# ── Sliders ───────────────────────────────────────────────────────────────────
ax_sl_min = plt.axes([0.10, 0.07, 0.70, 0.025])
ax_sl_max = plt.axes([0.10, 0.03, 0.70, 0.025])
sl_min = Slider(ax_sl_min, "cmin (dB)", -120, 0, valinit=-60,  valstep=1)
sl_max = Slider(ax_sl_max, "cmax (dB)", -120, 0, valinit=0,    valstep=1)

def on_slider(_):
    im.set_clim(sl_min.val, sl_max.val)

sl_min.on_changed(on_slider)
sl_max.on_changed(on_slider)


# ── Animation update ──────────────────────────────────────────────────────────
def update(_frame):
    with buf_lock:
        idx   = np.arange(write_idx - BUFFER_SIZE, write_idx) % BUFFER_SIZE
        i_out = i_buf[idx].copy()
        q_out = q_buf[idx].copy()

    # Time domain
    line_i.set_ydata(i_out)
    line_q.set_ydata(q_out)

    # FFT slice
    fft_db = doppler_info.get("fft_db", np.full(FFT_LEN, -120.0))
    line_fft.set_ydata(fft_db)
    pk_idx  = int(np.argmax(fft_db))
    pk_freq = freqs[pk_idx]
    vline_peak.set_xdata([pk_freq, pk_freq])
    peak_label.set_text(f"Peak  {pk_freq:.1f} Hz")

    # Spectrogram
    with wf_lock:
        im.set_data(waterfall.copy())

    # Info banner
    fd  = doppler_info.get("fd", 0.0)
    v   = doppler_info.get("v",  0.0)
    drn = doppler_info.get("direction", "—")
    info_text.set_text(
        f"fd = {fd:.2f} Hz  |  v ≈ {v:.4f} m/s  |  {drn}"
        f"  |  colour [{sl_min.val:.0f}, {sl_max.val:.0f}] dB"
    )

    return line_i, line_q, line_fft, vline_peak, peak_label, im, info_text


# ── Start threads and animation ───────────────────────────────────────────────
threading.Thread(target=reader_thread, daemon=True).start()
threading.Thread(target=fft_thread,    daemon=True).start()

ani = animation.FuncAnimation(fig, update, interval=UPDATE_MS, blit=False, cache_frame_data=False)
plt.show()