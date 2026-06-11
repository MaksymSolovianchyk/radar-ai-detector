# radar_collect.py
#
# Radar dataset collector for AI training.
# Receives binary FFT packets from STM32, accumulates frames into
# (64 x 96 x 1) patches and saves them as .npy files.
#
# Controls:
#   SPACE  — save current buffer as .npy (existing format)
#   p      — save current buffer as 224x224x3 PNG spectrogram image
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
from matplotlib.colors import Normalize
from matplotlib import cm

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
PATCH_BINS    = 256           # freq axis  (cols in patch)
FREQ_STEP     = 1

# Dataset output folder
DATASET_DIR   = "dataset"

CLASSES = [
    "idle",
    "approach",
    "recede",
    "hand_wave",
    "walking",
]

# Target samples per class
TARGET_SAMPLES = 300

# PNG output size
PNG_SIZE = 224   # pixels — output is PNG_SIZE x PNG_SIZE x 3

# Waterfall display rows
DISPLAY_ROWS  = 150

# Save modes — toggled by the mode button (top-right corner)
SAVE_MODES = ["integrated AI [.npy]", "image dataset [.png]"]
# ──────────────────────────────────────────────────────────────────────────────

# ── Shared state ──────────────────────────────────────────────────────────────
frame_ring   = np.zeros((PATCH_FRAMES, FFT_N), dtype=np.float32)
frame_idx    = 0
ring_lock    = threading.Lock()

display_wf   = np.full((DISPLAY_ROWS, FFT_N), -80.0, dtype=np.float32)

active_class  = 0
save_mode_idx = 0   # index into SAVE_MODES — toggled by the mode button
stats         = {"frames": 0, "bad": 0, "last_save": "—"}
counts        = {c: 0 for c in CLASSES}

# Make dataset folders — separate subfolders for npy and png
for c in CLASSES:
    os.makedirs(os.path.join(DATASET_DIR, "npy", c), exist_ok=True)
    os.makedirs(os.path.join(DATASET_DIR, "png", c), exist_ok=True)
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

                    mag_db = np.clip(
                        20.0 * np.log10(mag + EPSILON), -80.0, 0.0
                    ).astype(np.float32)

                    with ring_lock:
                        pos = frame_idx % PATCH_FRAMES
                        frame_ring[pos] = mag_db
                        frame_idx += 1

                        display_wf[:-1] = display_wf[1:]
                        display_wf[-1]  = mag_db

                    stats["frames"] += 1

        except serial.SerialException as e:
            print(f"[collector] Serial error: {e} — retrying in 2 s")
            time.sleep(2)


def build_patch() -> np.ndarray:
    """
    Snapshot the rolling buffer into a (64, 96, 1) float32 patch.
    Returns shape (PATCH_BINS, PATCH_FRAMES, 1) normalised [0, 1].
    """
    with ring_lock:
        idx = frame_idx
        ring_copy = frame_ring.copy()

    start   = idx % PATCH_FRAMES
    ordered = np.concatenate([ring_copy[start:], ring_copy[:start]], axis=0)
    sub     = ordered[:, ::FREQ_STEP]          # (PATCH_FRAMES, PATCH_BINS)
    patch   = sub.T                            # (PATCH_BINS, PATCH_FRAMES)
    patch   = (patch + 80.0) / 80.0
    patch   = np.clip(patch, 0.0, 1.0)
    patch   = patch[:, :, np.newaxis]          # (PATCH_BINS, PATCH_FRAMES, 1)
    return patch.astype(np.float32)


def build_spectrogram_rgb() -> np.ndarray:
    """
    Build a 224x224x3 uint8 RGB image from the current waterfall buffer.

    Steps:
      1. Use the display waterfall (DISPLAY_ROWS x FFT_N) in dB
      2. Normalise [-80, 0] dB → [0, 1]
      3. Apply inferno colormap → RGB float [0, 1]
      4. Resize to PNG_SIZE x PNG_SIZE using numpy (no PIL needed)
      5. Convert to uint8 [0, 255]

    The result matches the reference image style:
      - inferno colormap (black → purple → orange → yellow/white)
      - no axes, no labels, pure image data
      - 224 x 224 x 3 RGB
    """
    with ring_lock:
        wf = display_wf.copy()   # (DISPLAY_ROWS, FFT_N)

    # Normalise dB range to [0, 1]
    wf_norm = (wf + 80.0) / 80.0
    wf_norm = np.clip(wf_norm, 0.0, 1.0)

    # Apply inferno colormap — result is (DISPLAY_ROWS, FFT_N, 4) RGBA
    cmap      = cm.get_cmap("inferno")
    rgb_float = cmap(wf_norm)[:, :, :3]   # drop alpha → (H, W, 3)

    # Resize to PNG_SIZE x PNG_SIZE using nearest-neighbour
    # (no scipy/PIL needed — pure numpy index tricks)
    h, w    = rgb_float.shape[:2]
    row_idx = (np.linspace(0, h - 1, PNG_SIZE)).astype(int)
    col_idx = (np.linspace(0, w - 1, PNG_SIZE)).astype(int)
    resized = rgb_float[np.ix_(row_idx, col_idx)]   # (224, 224, 3)

    # Convert to uint8
    rgb_uint8 = (resized * 255).clip(0, 255).astype(np.uint8)
    return rgb_uint8


def save_sample():
    """Save one .npy patch for the active class."""
    if frame_idx < PATCH_FRAMES:
        print(f"[collector] Need {PATCH_FRAMES} frames first "
              f"(have {frame_idx}) — keep waiting")
        return

    label   = CLASSES[active_class]
    patch   = build_patch()
    out_dir = os.path.join(DATASET_DIR, "npy", label)
    idx     = len([f for f in os.listdir(out_dir) if f.endswith(".npy")])
    path    = os.path.join(out_dir, f"sample_{idx:04d}.npy")
    np.save(path, patch)
    counts[label] += 1
    stats["last_save"] = f"npy/{label}/sample_{idx:04d}.npy"
    print(f"[collector] NPY saved: {path}  ({counts[label]}/{TARGET_SAMPLES})")


def save_sample_png():
    """
    Save the current 16-frame patch as a 224x224x3 PNG.
    Uses frame_ring (PATCH_FRAMES=16) — same data as the .npy save.
    """
    if frame_idx < PATCH_FRAMES:
        print(f"[collector] Need {PATCH_FRAMES} frames first — keep waiting")
        return

    label = CLASSES[active_class]
    out_dir = os.path.join(DATASET_DIR, "png", label)
    idx = len([f for f in os.listdir(out_dir) if f.endswith(".png")])
    path = os.path.join(out_dir, f"sample_{idx:04d}.png")

    # ── Build image from frame_ring (16 x 256), same as .npy patch ──────────
    with ring_lock:
        ring_copy = frame_ring.copy()
        start = frame_idx % PATCH_FRAMES

    # Unroll ring buffer — oldest frame first
    ordered = np.concatenate([ring_copy[start:], ring_copy[:start]], axis=0)
    # ordered shape: (16, 256) in dB

    # Normalise [-80, 0] dB → [0, 1]
    wf_norm = np.clip((ordered + 80.0) / 80.0, 0.0, 1.0)

    # Apply inferno colormap → (16, 256, 3) RGB uint8
    rgba = cm.inferno(wf_norm)
    rgb = (rgba[:, :, :3] * 255).astype(np.uint8)

    # Render to 224x224 — matplotlib stretches the 16x256 patch to fill
    fig_tmp = plt.figure(figsize=(224 / 100, 224 / 100), dpi=100)
    ax_tmp = fig_tmp.add_axes([0, 0, 1, 1])
    ax_tmp.imshow(rgb, aspect="auto", interpolation="nearest", origin="lower")
    ax_tmp.axis("off")
    fig_tmp.savefig(path, dpi=100, bbox_inches="tight",
                    pad_inches=0, format="png")
    plt.close(fig_tmp)

    counts[label] += 1
    stats["last_save"] = f"png/{label}/sample_{idx:04d}.png"
    print(f"[collector] PNG saved: {path}  patch=({PATCH_FRAMES}x{FFT_N}) → 224x224x3")


# ── Build UI ──────────────────────────────────────────────────────────────────
freqs = np.linspace(-SAMPLING_RATE / 2, SAMPLING_RATE / 2, FFT_N)

fig, axes = plt.subplots(
    2, 1, figsize=(12, 9),
    gridspec_kw={"height_ratios": [1, 2]}
)
fig.suptitle(
    f"Radar Dataset Collector — mode: {SAVE_MODES[save_mode_idx]}",
    fontsize=12, fontweight="bold"
)
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
ax_w.set_title("Waterfall — press SPACE to save")
fig.colorbar(im, ax=ax_w, label="dB", fraction=0.025, pad=0.01)

# Status bar
status_txt = fig.text(
    0.5, 0.155, "Waiting for frames...",
    ha="center", fontsize=9,
    bbox=dict(facecolor="lightyellow", alpha=0.9, boxstyle="round,pad=0.3")
)

# ── Buttons ───────────────────────────────────────────────────────────────────
BTN_Y      = 0.09
BTN_HEIGHT = 0.055
BTN_WIDTH  = 0.11
BTN_GAP    = 0.012
btn_start_x = 0.05

btn_axes = []
btn_objs = []

for i, cls in enumerate(CLASSES):
    ax_b = plt.axes([
        btn_start_x + i * (BTN_WIDTH + BTN_GAP),
        BTN_Y, BTN_WIDTH, BTN_HEIGHT
    ])
    color = "deepskyblue" if i == active_class else "lightgray"
    b = Button(ax_b, f"[{i+1}] {cls}", color=color, hovercolor="skyblue")
    btn_axes.append(ax_b)
    btn_objs.append(b)

# Save button — single button, mode shown in title
ax_save = plt.axes([
    btn_start_x + len(CLASSES) * (BTN_WIDTH + BTN_GAP),
    BTN_Y, BTN_WIDTH + 0.03, BTN_HEIGHT
])
btn_save = Button(ax_save, "[ SPACE ] SAVE",
                  color="limegreen", hovercolor="green")

# Mode toggle button — small, top-right corner (matches screenshot)
ax_mode = plt.axes([0.88, BTN_Y, 0.10, BTN_HEIGHT])
btn_mode = Button(ax_mode, f"P",
                  color="steelblue", hovercolor="dodgerblue")

# Instructions
fig.text(
    0.5, 0.025,
    "Keys:  1–{n}  switch class   |   SPACE  save sample   |   P  toggle mode   |   q  quit".format(
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


def toggle_mode(_=None):
    global save_mode_idx
    save_mode_idx = (save_mode_idx + 1) % len(SAVE_MODES)
    fig.suptitle(
        f"Radar Dataset Collector — mode: {SAVE_MODES[save_mode_idx]}",
        fontsize=12, fontweight="bold"
    )
    # Highlight mode button colour to indicate current mode
    if save_mode_idx == 0:
        btn_mode.ax.set_facecolor("steelblue")
    else:
        btn_mode.ax.set_facecolor("darkorange")
    fig.canvas.draw_idle()
    print(f"[collector] Mode switched to: {SAVE_MODES[save_mode_idx]}")


def save_current():
    """Save in whichever mode is currently active."""
    if SAVE_MODES[save_mode_idx] == "integrated AI [.npy]":
        save_sample()
    else:
        save_sample_png()


# Wire class buttons
for i in range(len(CLASSES)):
    btn_objs[i].on_clicked(lambda _, idx=i: set_active(idx))

# Wire save and mode buttons
btn_save.on_clicked(lambda _: save_current())
btn_mode.on_clicked(toggle_mode)


def on_key(event):
    if event.key == "q":
        plt.close("all")
    elif event.key == " ":
        save_current()
    elif event.key == "p":
        toggle_mode()
    elif event.key in [str(i + 1) for i in range(len(CLASSES))]:
        set_active(int(event.key) - 1)


fig.canvas.mpl_connect("key_press_event", on_key)


def update(_):
    with ring_lock:
        last_row = frame_ring[(frame_idx - 1) % PATCH_FRAMES].copy()
        wf_snap  = display_wf.copy()

    line_fft.set_ydata(last_row)
    im.set_data(wf_snap)

    count_str = "  ".join(
        f"{c}: {counts[c]}/{TARGET_SAMPLES}" for c in CLASSES
    )
    buf_pct = min(100, int(frame_idx / PATCH_FRAMES * 100))
    mode_lbl = SAVE_MODES[save_mode_idx]
    out_dir  = os.path.join(
        DATASET_DIR,
        "npy" if save_mode_idx == 0 else "png",
        CLASSES[active_class]
    )

    status_txt.set_text(
        f"Mode: {mode_lbl}   |   Active class: [{active_class+1}] {CLASSES[active_class]}   |   "
        f"Buffer: {buf_pct}%  ({frame_idx}/{PATCH_FRAMES} frames)   |   "
        f"Output: {out_dir}   |   Last save: {stats['last_save']}\n"
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
for fmt in ("npy", "png"):
    print(f"  Format: {fmt}")
    for c in CLASSES:
        d = os.path.join(DATASET_DIR, fmt, c)
        n = len([f for f in os.listdir(d) if f.endswith(f".{fmt}")])
        print(f"    {c:12s}  {n:4d} samples")
        if fmt == "npy":
            total += n
print(f"\n  NPY TOTAL  {total:4d} samples")
print(f"  Saved to: {os.path.abspath(DATASET_DIR)}/")
print("─────────────────────────────────────────────────")