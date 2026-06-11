# verify_fft.py
# Receives one FFT packet from STM32 and prints a full diagnostic.
# No firmware changes needed — reads the same binary stream.

import struct, serial, numpy as np, matplotlib.pyplot as plt

PORT      = "/dev/tty.usbmodem1102"
BAUD      = 576000
FFT_N     = 256
FS        = 3904
LAMBDA    = 0.01240   # 24.2 GHz

def read_one_packet(ser):
    # Find sync bytes 0x55 0xAA
    while True:
        b = ser.read(1)
        if b[0] == 0x55:
            if ser.read(1)[0] == 0xAA:
                break
    n = struct.unpack("<H", ser.read(2))[0]
    assert n == FFT_N, f"Expected {FFT_N} bins, got {n}"
    payload  = ser.read(FFT_N * 4)
    checksum = ser.read(1)[0]
    cs = 0
    for byte in payload: cs ^= byte
    assert cs == checksum, "Checksum mismatch!"
    return np.frombuffer(payload, dtype="<f4")

ser = serial.Serial(PORT, BAUD, timeout=3)
ser.reset_input_buffer()
print("Waiting for packet...")

mag   = read_one_packet(ser)
ser.close()

freqs = np.fft.fftshift(np.fft.fftfreq(FFT_N, d=1.0/FS))
vel   = freqs * LAMBDA / 2

# ── Print diagnostics ────────────────────────────────────────────────────────
print(f"Bins received  : {len(mag)}")
print(f"Min magnitude  : {mag.min():.6f}")
print(f"Max magnitude  : {mag.max():.6f}")
print(f"DC bin (centre): {mag[FFT_N//2]:.6f}")

peak_idx  = np.argmax(mag)
peak_freq = freqs[peak_idx]
peak_vel  = vel[peak_idx]
print(f"Peak bin       : {peak_idx}  ({peak_freq:+.1f} Hz  →  {peak_vel:+.3f} m/s)")

# ── Plot ─────────────────────────────────────────────────────────────────────
fig, axes = plt.subplots(1, 2, figsize=(12, 4))

axes[0].plot(freqs, mag, color="gold", lw=0.9)
axes[0].axvline(0, color="white", lw=0.8, ls="--", alpha=0.5)
axes[0].axvline(peak_freq, color="red", lw=1.2, ls="--",
                label=f"Peak {peak_freq:+.1f} Hz")
axes[0].set_facecolor("black")
axes[0].set_xlabel("Doppler Frequency (Hz)")
axes[0].set_ylabel("Magnitude")
axes[0].set_title("FFT Spectrum — one frame")
axes[0].legend()

axes[1].plot(vel, mag, color="cyan", lw=0.9)
axes[1].axvline(0, color="white", lw=0.8, ls="--", alpha=0.5)
axes[1].axvline(peak_vel, color="red", lw=1.2, ls="--",
                label=f"Peak {peak_vel:+.3f} m/s")
axes[1].set_facecolor("black")
axes[1].set_xlabel("Velocity (m/s)")
axes[1].set_ylabel("Magnitude")
axes[1].set_title("Velocity Spectrum — one frame")
axes[1].legend()

fig.patch.set_facecolor("black")
for ax in axes:
    ax.tick_params(colors="white")
    for spine in ax.spines.values():
        spine.set_edgecolor("white")

plt.tight_layout()
plt.show()