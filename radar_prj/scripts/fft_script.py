import re
import struct
import threading
import time
import serial
import numpy as np
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Slider

# ── Config ────────────────────────────────────────────────────────────────────
PORT              = "/dev/tty.usbmodem1102"
BAUDRATE          = 576000
FFT_N             = 1024
SAMPLING_RATE     = 3904        # Hz
WATERFALL_ROWS    = 200         # rows of history shown
UPDATE_MS         = 100         # display refresh interval
DC_GUARD          = 9
EPSILON           = 1e-12
SYNC              = bytes([0x55, 0xAA])
PACKET_BYTES      = 2 + 2 + FFT_N * 4 + 1

# Temporal averaging — blend each new frame with the previous display value.
# 0.0 = no smoothing (raw),  0.8 = heavy smoothing (sluggish but clean).
SMOOTH_ALPHA      = 0.85

# Noise floor threshold in dB — bins below this are clamped to it.
# Reduces the speckle noise visible at high frequencies.
NOISE_FLOOR_DB    = -80.0
# ──────────────────────────────────────────────────────────────────────────────

# ── Shared state ──────────────────────────────────────────────────────────────
waterfall    = np.full((WATERFALL_ROWS, FFT_N), -120.0)
latest_mag   = np.zeros(FFT_N, dtype=np.float32)
smoothed_mag = np.full(FFT_N, -120.0, dtype=np.float64)   # temporal avg
wf_lock      = threading.Lock()
stats        = {"frames": 0, "bad": 0, "peak_hz": 0.0}
# ──────────────────────────────────────────────────────────────────────────────


def xor_checksum(data: bytes) -> int:
    cs = 0
    for b in data:
        cs ^= b
    return cs


def find_sync(ser: serial.Serial) -> bool:
    """Scan byte stream until 0x55 0xAA is found."""
    b = ser.read(1)
    while b:
        if b[0] == 0x55:
            b2 = ser.read(1)
            if b2 and b2[0] == 0xAA:
                return True
            b = b2
        else:
            b = ser.read(1)
    return False


def receive_packet(ser: serial.Serial):
    """
    Read one binary FFT packet.
    Returns float32 magnitude array or None on error.
    """
    if not find_sync(ser):
        return None

    # bin count
    hdr = ser.read(2)
    if len(hdr) < 2:
        return None
    n = struct.unpack("<H", hdr)[0]
    if n != FFT_N:
        return None

    # magnitude payload
    payload = ser.read(FFT_N * 4)
    if len(payload) < FFT_N * 4:
        return None

    # checksum
    cs_byte = ser.read(1)
    if not cs_byte:
        return None
    if cs_byte[0] != xor_checksum(payload):
        stats["bad"] += 1
        return None

    mag = np.frombuffer(payload, dtype="<f4").copy()
    return mag


def reader_thread():
    """Background thread: receive packets and update waterfall."""
    while True:
        try:
            with serial.Serial(PORT, BAUDRATE, timeout=2) as ser:
                ser.reset_input_buffer()
                print(f"Serial opened: {PORT}")
                while True:
                    mag = receive_packet(ser)
                    if mag is None:
                        continue

                    # convert to dB
                    mag_db = np.clip(
                        20.0 * np.log10(mag + EPSILON), -120.0, 0.0
                    )

                    # Apply noise floor threshold
                    mag_db = np.where(mag_db < NOISE_FLOOR_DB,
                                      NOISE_FLOOR_DB, mag_db)

                    # Temporal smoothing: exponential moving average
                    # smoothed = alpha * prev + (1-alpha) * new
                    with wf_lock:
                        smoothed_mag[:] = (SMOOTH_ALPHA * smoothed_mag
                                           + (1.0 - SMOOTH_ALPHA) * mag_db)
                        display_row = smoothed_mag.copy()

                    # Peak on raw (unsmoothed) so detection stays responsive
                    dc   = FFT_N // 2
                    mask = np.ones(FFT_N, dtype=bool)
                    mask[dc - DC_GUARD : dc + DC_GUARD + 1] = False
                    pk   = int(np.argmax(mag * mask))
                    stats["peak_hz"] = (pk - dc) * (SAMPLING_RATE / FFT_N)
                    stats["frames"] += 1

                    with wf_lock:
                        waterfall[:-1] = waterfall[1:]
                        waterfall[-1]  = display_row
                        latest_mag[:]  = display_row

        except serial.SerialException as e:
            print(f"Serial error: {e} — retrying in 2 s")
            time.sleep(2)


# ── Build figure ──────────────────────────────────────────────────────────────
freqs = np.linspace(-SAMPLING_RATE / 2, SAMPLING_RATE / 2, FFT_N)

fig = plt.figure(figsize=(12, 9))
fig.suptitle("Live FFT waterfall — STM32 binary packets", fontsize=11)
plt.subplots_adjust(left=0.08, right=0.93, top=0.93, bottom=0.16, hspace=0.40)

gs   = fig.add_gridspec(2, 2, width_ratios=[1, 0.025],
                        hspace=0.40, wspace=0.06,
                        top=0.93, bottom=0.16,
                        left=0.08, right=0.93)
ax_f = fig.add_subplot(gs[0, 0])
ax_w = fig.add_subplot(gs[1, 0])
ax_c = fig.add_subplot(gs[1, 1])

# FFT slice
line_fft, = ax_f.plot(freqs, np.full(FFT_N, -120.0), color="gold", lw=0.9)
vline_pk  = ax_f.axvline(0, color="red", lw=0.9, linestyle="--")
ax_f.set_xlim(-SAMPLING_RATE / 2, SAMPLING_RATE / 2)
ax_f.set_ylim(-100, 5)
ax_f.set_xlabel("Frequency (Hz)")
ax_f.set_ylabel("Magnitude (dB)")
ax_f.set_title(f"FFT slice  (N={FFT_N}  fs={SAMPLING_RATE} Hz)")
ax_f.grid(True, alpha=0.25)
peak_txt = ax_f.text(0.02, 0.93, "", transform=ax_f.transAxes,
                     fontsize=9, color="red")

# Waterfall
im = ax_w.imshow(
    waterfall,
    aspect="auto",
    origin="lower",
    extent=[-SAMPLING_RATE / 2, SAMPLING_RATE / 2, 0, WATERFALL_ROWS],
    cmap="inferno",
    vmin=-60, vmax=0,
    interpolation="nearest",
)
ax_w.set_xlabel("Frequency (Hz)")
ax_w.set_ylabel("Frame (oldest → newest)")
ax_w.set_title("Waterfall")
fig.colorbar(im, cax=ax_c, label="dB")

# Sliders
ax_sl_min = plt.axes([0.10, 0.07, 0.70, 0.025])
ax_sl_max = plt.axes([0.10, 0.03, 0.70, 0.025])
sl_min = Slider(ax_sl_min, "cmin (dB)", -120, 0, valinit=-60,  valstep=1)
sl_max = Slider(ax_sl_max, "cmax (dB)", -120, 0, valinit=0,    valstep=1)

def on_slider(_):
    im.set_clim(sl_min.val, sl_max.val)

sl_min.on_changed(on_slider)
sl_max.on_changed(on_slider)

# Info text
info_txt = fig.text(0.5, 0.005, "", ha="center", fontsize=9,
                    bbox=dict(facecolor="lightyellow", alpha=0.85,
                              boxstyle="round,pad=0.3"))


def update(_):
    with wf_lock:
        mag_snap = latest_mag.copy()
        wf_snap  = waterfall.copy()

    line_fft.set_ydata(mag_snap)
    pk_hz = stats["peak_hz"]
    vline_pk.set_xdata([pk_hz, pk_hz])
    peak_txt.set_text(f"Peak  {pk_hz:+.1f} Hz")
    im.set_data(wf_snap)
    im.set_clim(sl_min.val, sl_max.val)
    info_txt.set_text(
        f"Frames received: {stats['frames']}  |  "
        f"Bad packets: {stats['bad']}  |  "
        f"Peak: {pk_hz:+.1f} Hz  |  "
        f"colour [{sl_min.val:.0f}, {sl_max.val:.0f}] dB"
    )
    return line_fft, vline_pk, peak_txt, im, info_txt


threading.Thread(target=reader_thread, daemon=True).start()
ani = animation.FuncAnimation(fig, update, interval=UPDATE_MS,
                               blit=False, cache_frame_data=False)
plt.show()
