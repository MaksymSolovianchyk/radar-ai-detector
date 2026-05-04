---

<details open>
<summary><h2>Live FFT + Spectrogram Script.py</h2></summary>

<details open>
<summary><h3>Example Output</h3></summary>

![Py output](/docs/img/py_example.png)

</details>

---

<details open>
<summary><h3>Purpose</h3></summary>

This script provides **real-time visualization and analysis** of I/Q data received from an STM32 over UART.

It displays:
- Time-domain signal
- Frequency spectrum (FFT)
- Spectrogram (STFT)
- Doppler-based velocity estimation

</details>

---

<details open>
<summary><h3>How It Works</h3></summary>

- **Serial thread:** Reads and parses incoming data
- **Processing thread:** Computes FFT, spectrogram, and Doppler
- **GUI:** Updates plots in real time using matplotlib

</details>

---

<details open>
<summary><h3>Key Parameters</h3></summary>

<details open>
<summary><b>Communication</b></summary>

- `PORT` – Serial port (e.g. `/dev/tty.usbmodem1102`)
- `BAUDRATE` – Must match STM32

</details>

<details open>
<summary><b>Signal Processing</b></summary>

- `SAMPLING_RATE` – Defines frequency axis
- `FFT_LEN` – FFT size (resolution vs speed)
- `HOP` – Spectrogram time resolution

</details>

<details open>
<summary><b>Buffers</b></summary>

- `BUFFER_SIZE` – Time-domain window length
- `WATERFALL_ROWS` – Spectrogram history

</details>

<details open>
<summary><b>Scaling</b></summary>

- `FSR`, `STEP` – Convert ADC values to voltage

</details>

<details open>
<summary><b>Visualization</b></summary>

- `UPDATE_MS` – Plot refresh rate
- `cmin / cmax` – Spectrogram color range

</details>

</details>

---

<details open>
<summary><h3>Output</h3></summary>

- Real-time signal plots
- Dominant frequency detection
- Doppler frequency (`fd`)
- Estimated velocity (`v`) and direction

</details>

---

<details open>
<summary><h3>Summary</h3></summary>

A real-time tool for **signal analysis and Doppler estimation** using STM32-acquired I/Q data.

</details>

</details>

---

<details open>
<summary><h2>Live FFT on STM32 FFT_SCRIPT.PY</h2></summary>

![Py output2](/docs/img/ftt-on-stm1.png)

</details>

---

<details open>
<summary><h2>Radar Dataset Collector — radar_collect.py</h2></summary>

<details open>
<summary><h3>Example Output</h3></summary>

![Radar Collector Output](/docs/img/radar_collect_example.png)

</details>

---

<details open>
<summary><h3>Purpose</h3></summary>

This script collects **labeled radar FFT frames** from an STM32 over UART and saves them as `.npy` patches for AI model training.

It displays:
- Live FFT frequency slice
- Rolling waterfall (spectrogram history)
- Per-class sample counters
- Buffer fill progress

</details>

---

<details open>
<summary><h3>How It Works</h3></summary>

- **Reader thread:** Continuously receives binary FFT packets from STM32, validates checksum, converts to dB and writes into a rolling ring buffer
- **Ring buffer:** Always holds the last `PATCH_FRAMES` frames — when you press SPACE it snapshots this buffer into a patch
- **Patch builder:** Unrolls the ring buffer, subsamples the frequency axis, normalises to `[0, 1]` and shapes into `(64, 96, 1)` — the AI model input format
- **GUI:** Updates FFT slice and waterfall in real time using matplotlib; class buttons and keyboard shortcuts control labeling

</details>

---

<details open>
<summary><h3>Controls</h3></summary>

| Key | Action |
|-----|--------|
| `SPACE` | Save current buffer as a sample for the active class |
| `1` – `9` | Switch active class |
| `q` | Quit |

Class buttons are also clickable in the GUI — the active class is highlighted in blue.

</details>

---

<details open>
<summary><h3>Packet Format</h3></summary>

Matches `fft.h` / `fft_live.py`:

| Bytes | Content |
|-------|---------|
| `[0..1]` | Sync bytes `0x55 0xAA` |
| `[2..3]` | Bin count `uint16_le` (= `FFT_N`) |
| `[4 .. 4+N×4-1]` | N × `float32` magnitudes (fftshifted, DC at centre) |
| `[last]` | XOR checksum of magnitude bytes |

</details>

---

<details open>
<summary><h3>Key Parameters</h3></summary>

<details open>
<summary><b>Communication</b></summary>

- `PORT` – Serial port (e.g. `/dev/tty.usbmodem1102`)
- `BAUDRATE` – Must match STM32 (`576000`)

</details>

<details open>
<summary><b>Signal</b></summary>

- `FFT_N` – Number of FFT bins per frame (`256`)
- `SAMPLING_RATE` – Defines frequency axis (`3904 Hz`)

</details>

<details open>
<summary><b>Patch Dimensions (must match AI model input)</b></summary>

- `PATCH_FRAMES` – Number of time frames per patch (`16` rows)
- `PATCH_BINS` – Frequency bins per patch (`256` cols)
- `FREQ_STEP` – Frequency subsampling step

</details>

<details open>
<summary><b>Dataset</b></summary>

- `DATASET_DIR` – Output folder (`dataset/`)
- `CLASSES` – List of gesture/motion labels (`idle`, `approach`, `recede`, `hand_wave`, `walking`)
- `TARGET_SAMPLES` – Collector warns when this count is reached per class (`300`)

</details>

<details open>
<summary><b>Visualization</b></summary>

- `DISPLAY_ROWS` – Waterfall history rows (visual only, independent of patch size)
- `UPDATE_MS` – Animation refresh interval (`150 ms`)

</details>

</details>

---

<details open>
<summary><h3>Patch Shape & Normalisation</h3></summary>

Each saved `.npy` file is a `float32` array of shape `(64, 96, 1)`:

1. Ring buffer unrolled → oldest frame first → shape `(PATCH_FRAMES, FFT_N)`
2. Frequency axis subsampled by `FREQ_STEP` → `(PATCH_FRAMES, 64)`
3. Transposed → `(64, PATCH_FRAMES)` — frequency × time
4. Normalised: `(dB + 80) / 80` → `[0.0, 1.0]`
5. Channel dim added → `(64, 96, 1)`

</details>

---

<details open>
<summary><h3>Output</h3></summary>

Samples are saved to `dataset/<class>/sample_XXXX.npy`.

On exit a summary is printed:
```
── Dataset summary ───────────────────────────────
  idle          42 samples
  approach      38 samples
  recede        35 samples
  hand_wave     50 samples
  walking       29 samples
  TOTAL        194 samples
  Saved to: /your/path/dataset/
─────────────────────────────────────────────────
```

</details>

---

<details open>
<summary><h3>Summary</h3></summary>

A real-time labeled data collection tool that turns live STM32 radar FFT streams into ready-to-train `.npy` patches, with a GUI for fast class switching and sample capture.

</details>

</details>
