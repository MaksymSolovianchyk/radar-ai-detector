# radar_collect.py
#
# Radar dataset collector for AI training.
# Receives binary FFT packets from STM32, accumulates frames into
# (64 x 96 x 1) patches and saves them as .npy files.
#
# Controls:
#   SPACE  — save current buffer as the ACTIVE class sample
#   1..9   — switch active class
#   q      — quit
#
# Packet format (matches fft.h / fft_live.py):
#   [0..1]  sync  0x55 0xAA
#   [2..3]  bin count uint16_le  (= FFT_N = 1024)
#   [4 .. 4+N*4-1]  N x float32 magnitudes (fftshifted, DC at centre)
#   [last]  XOR checksum of magnitude bytes

import os
import struct
import threading
import time
import serial
import numpy as np
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Button

# ── Config ────────────────────────────────────────────────────────────────────
PORT          = "/dev/tty.usbmodem1102"   # adjust to your port
BAUDRATE      = 576000
FFT_N         = 256
SAMPLING_RATE = 3904          # Hz
EPSILON       = 1e-12
SYNC          = bytes([0x55, 0xAA])
PACKET_BYTES  = 2 + 2 + FFT_N * 4 + 1

# Patch dimensions — must match AI model input
PATCH_FRAMES  = 16            # time axis  (rows in patch)
PATCH_BINS    = 256            # freq axis  (cols in patch)
FREQ_STEP     = 1   # = 16 — take every 16th bin

# Dataset output folder
DATASET_DIR   = "dataset"

CLASSES = [
    "idle",
    "approach",
    "recede",
    "hand_wave",
    "walking",
]

# Target samples per class — collector warns when reached
TARGET_SAMPLES = 300

# Waterfall display rows (visual only, independent of patch size)
DISPLAY_ROWS  = 150
# ──────────────────────────────────────────────────────────────────────────────

# ── Shared state ──────────────────────────────────────────────────────────────
# Rolling frame buffer — always holds last PATCH_FRAMES frames, full 1024 bins
frame_ring   = np.zeros((PATCH_FRAMES, FFT_N), dtype=np.float32)
frame_idx    = 0          # total frames received (never resets)
ring_lock    = threading.Lock()

# Display waterfall (separate from frame_ring — for visualisation only)
display_wf   = np.full((DISPLAY_ROWS, FFT_N), -80.0, dtype=np.float32)

active_class = 0          # index into CLASSES
stats        = {"frames": 0, "bad": 0, "last_save": "—"}
counts       = {c: 0 for c in CLASSES}   # samples saved per class

# Make dataset folders
for c in CLASSES:
    os.makedirs(os.path.join(DATASET_DIR, c), exist_ok=True)
# ──────────────────────────────────────────────────────────────────────────────


def xor_checksum(data: bytes) -> int:
    cs = 0
    for b in data:
        cs ^= b
    return cs


def find_sync(ser: serial.Serial) -> bool:
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
    if not find_sync(ser):
        return None
    hdr = ser.read(2)
    if len(hdr) < 2:
        return None
    n = struct.unpack("<H", hdr)[0]
    if n != FFT_N:
        return None
    payload = ser.read(FFT_N * 4)
    if len(payload) < FFT_N * 4:
        return None
    cs_byte = ser.read(1)
    if not cs_byte:
        return None
    if cs_byte[0] != xor_checksum(payload):
        stats["bad"] += 1
        return None
    return np.frombuffer(payload, dtype="<f4").copy()


def reader_thread():
    global frame_idx
    while True:
        try:
            with serial.Serial(PORT, BAUDRATE, timeout=2) as ser:
                ser.reset_input_buffer()
                print(f"[collector] Serial opened: {PORT}")
                while True:
                    mag = receive_packet(ser)
                    if mag is None:
                        continue

                    # Convert to dB, clip to [-80, 0]
                    mag_db = np.clip(
                        20.0 * np.log10(mag + EPSILON), -80.0, 0.0
                    ).astype(np.float32)

                    with ring_lock:
                        pos = frame_idx % PATCH_FRAMES
                        frame_ring[pos] = mag_db
                        frame_idx += 1

                        # Update display waterfall (scroll up)
                        display_wf[:-1] = display_wf[1:]
                        display_wf[-1]  = mag_db

                    stats["frames"] += 1

        except serial.SerialException as e:
            print(f"[collector] Serial error: {e} — retrying in 2 s")
            time.sleep(2)


def build_patch() -> np.ndarray:
    """
    Snapshot the rolling buffer into a (64, 96, 1) float32 patch.

    Steps:
      1. Unroll ring buffer so oldest frame is first (shape 96 x 1024)
      2. Subsample frequency axis 1024 → 64  (take every 16th bin)
      3. Transpose to (64 x 96)  — freq x time
      4. Normalise [-80, 0] dB → [0.0, 1.0]
      5. Add channel dim → (64, 96, 1)
    """
    with ring_lock:
        idx = frame_idx
        ring_copy = frame_ring.copy()

    # Unroll: oldest row first
    start = idx % PATCH_FRAMES
    ordered = np.concatenate([ring_copy[start:], ring_copy[:start]], axis=0)
    # ordered shape: (96, 1024)

    # Subsample frequency: take every FREQ_STEP bin
    sub = ordered[:, ::FREQ_STEP]          # (96, 64)

    # Transpose → (64, 96)  freq × time
    patch = sub.T

    # Normalise to [0, 1]
    patch = (patch + 80.0) / 80.0
    patch = np.clip(patch, 0.0, 1.0)

    # Add channel dim
    patch = patch[:, :, np.newaxis]        # (64, 96, 1)
    return patch.astype(np.float32)


def save_sample():
    """Save one patch for the active class."""
    global active_class

    if frame_idx < PATCH_FRAMES:
        print(f"[collector] Need {PATCH_FRAMES} frames first "
              f"(have {frame_idx}) — keep waiting")
        return

    label   = CLASSES[active_class]
    patch   = build_patch()
    out_dir = os.path.join(DATASET_DIR, label)
    idx     = len(os.listdir(out_dir))
    path    = os.path.join(out_dir, f"sample_{idx:04d}.npy")
    np.save(path, patch)
    counts[label] += 1
    stats["last_save"] = f"{label}/sample_{idx:04d}.npy"
    print(f"[collector] Saved {path}  ({counts[label]}/{TARGET_SAMPLES})")


# ── Build UI ──────────────────────────────────────────────────────────────────
freqs = np.linspace(-SAMPLING_RATE / 2, SAMPLING_RATE / 2, FFT_N)

fig, axes = plt.subplots(
    2, 1, figsize=(12, 9),
    gridspec_kw={"height_ratios": [1, 2]}
)
fig.suptitle("Radar Dataset Collector", fontsize=12, fontweight="bold")
plt.subplots_adjust(left=0.08, right=0.98, top=0.91, bottom=0.22, hspace=0.40)

ax_f, ax_w = axes

# FFT slice panel
line_fft, = ax_f.plot(freqs, np.full(FFT_N, -80.0), color="gold", lw=0.9)
ax_f.set_xlim(-SAMPLING_RATE / 2, SAMPLING_RATE / 2)
ax_f.set_ylim(-85, 5)
ax_f.set_xlabel("Frequency (Hz)")
ax_f.set_ylabel("Magnitude (dB)")
ax_f.set_title(f"Live FFT  (N={FFT_N}  fs={SAMPLING_RATE} Hz)")
ax_f.grid(True, alpha=0.25)

# Waterfall panel
im = ax_w.imshow(
    display_wf,
    aspect="auto",
    origin="lower",
    extent=[-SAMPLING_RATE / 2, SAMPLING_RATE / 2, 0, DISPLAY_ROWS],
    cmap="inferno",
    vmin=-80, vmax=0,
    interpolation="nearest",
)
ax_w.set_xlabel("Frequency (Hz)")
ax_w.set_ylabel("Frame (oldest → newest)")
ax_w.set_title("Waterfall — position cursor here and press SPACE to save")
fig.colorbar(im, ax=ax_w, label="dB", fraction=0.025, pad=0.01)

# Status bar
status_txt = fig.text(
    0.5, 0.155, "Waiting for frames...",
    ha="center", fontsize=9,
    bbox=dict(facecolor="lightyellow", alpha=0.9, boxstyle="round,pad=0.3")
)

# Class buttons — one per class, highlighted when active
BTN_Y      = 0.07
BTN_HEIGHT = 0.055
BTN_WIDTH  = 0.13
BTN_GAP    = 0.015
btn_start_x = 0.08

btn_axes   = []
btn_objs   = []

for i, cls in enumerate(CLASSES):
    ax_b = plt.axes([
        btn_start_x + i * (BTN_WIDTH + BTN_GAP),
        BTN_Y, BTN_WIDTH, BTN_HEIGHT
    ])
    color = "deepskyblue" if i == active_class else "lightgray"
    b = Button(ax_b, f"[{i+1}] {cls}", color=color, hovercolor="skyblue")
    btn_axes.append(ax_b)
    btn_objs.append(b)

# Save button
ax_save = plt.axes([
    btn_start_x + len(CLASSES) * (BTN_WIDTH + BTN_GAP),
    BTN_Y, BTN_WIDTH + 0.02, BTN_HEIGHT
])
btn_save = Button(ax_save, "[ SPACE ] SAVE", color="limegreen", hovercolor="green")

# Instructions text
fig.text(
    0.5, 0.015,
    "Keys:  1–{n}  switch class   |   SPACE  save sample   |   q  quit".format(
        n=len(CLASSES)
    ),
    ha="center", fontsize=8, color="gray"
)


def set_active(i):
    global active_class
    active_class = i
    for j, b in enumerate(btn_objs):
        b.ax.set_facecolor("deepskyblue" if j == i else "lightgray")
        b.color = "deepskyblue" if j == i else "lightgray"
    fig.canvas.draw_idle()


# Wire class buttons
for i in range(len(CLASSES)):
    btn_objs[i].on_clicked(lambda _, idx=i: set_active(idx))

# Wire save button
btn_save.on_clicked(lambda _: save_sample())


def on_key(event):
    if event.key == "q":
        plt.close("all")
    elif event.key == " ":
        save_sample()
    elif event.key in [str(i + 1) for i in range(len(CLASSES))]:
        set_active(int(event.key) - 1)


fig.canvas.mpl_connect("key_press_event", on_key)


def update(_):
    with ring_lock:
        # Latest frame for FFT slice = last written row
        last_row = frame_ring[(frame_idx - 1) % PATCH_FRAMES].copy()
        wf_snap  = display_wf.copy()

    line_fft.set_ydata(last_row)
    im.set_data(wf_snap)

    # Build count string
    count_str = "  ".join(
        f"{c}: {counts[c]}/{TARGET_SAMPLES}" for c in CLASSES
    )
    progress = frame_idx - PATCH_FRAMES
    buf_pct  = min(100, int(frame_idx / PATCH_FRAMES * 100))

    status_txt.set_text(
        f"Active class: [{active_class+1}] {CLASSES[active_class]}   |   "
        f"Buffer: {buf_pct}%  ({frame_idx}/{PATCH_FRAMES} frames)   |   "
        f"Last save: {stats['last_save']}\n"
        f"{count_str}"
    )

    return line_fft, im, status_txt


threading.Thread(target=reader_thread, daemon=True).start()
ani = animation.FuncAnimation(
    fig, update, interval=150, blit=False, cache_frame_data=False
)
plt.show()

# ── Summary on exit ───────────────────────────────────────────────────────────
print("\n── Dataset summary ──────────────────────────────")
total = 0
for c in CLASSES:
    n = len(os.listdir(os.path.join(DATASET_DIR, c)))
    print(f"  {c:12s}  {n:4d} samples")
    total += n
print(f"  {'TOTAL':12s}  {total:4d} samples")
print(f"  Saved to: {os.path.abspath(DATASET_DIR)}/")
print("─────────────────────────────────────────────────")