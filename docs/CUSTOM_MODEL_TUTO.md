# Radar Motion Classification — Complete Usage Guide

> **Hardware:** STM32N657 + InnoSenT SMR-313/333 (24 GHz) + ADS131M04 ADC  
> **Software:** Python 3.10/3.11 + TensorFlow + pyserial  
> **Classes:** `approaching` · `idle` · `receding`

---

## Table of Contents

1. [Project Structure](#1-project-structure)
2. [Installation](#2-installation)
3. [STM32 Firmware Setup](#3-stm32-firmware-setup)
4. [Step 1 — Collect Dataset](#4-step-1--collect-dataset)
5. [Step 2 — Split Dataset](#5-step-2--split-dataset)
6. [Step 3 — Train the Model](#6-step-3--train-the-model)
7. [Step 4 — Export to TFLite](#7-step-4--export-to-tflite)
8. [Step 5 — Live Inference from Sensor](#8-step-5--live-inference-from-sensor)
9. [Step 6 — Single Sample Inference](#9-step-6--single-sample-inference)
10. [Output Files Reference](#10-output-files-reference)
11. [Troubleshooting](#11-troubleshooting)
12. [STM32 Deployment](#12-stm32-deployment)
13. [Improving Accuracy](#13-improving-accuracy)

---

## 1. Project Structure

```
custom_ai_model/
  radar_ai_training/
  ├── collect.py          ← collect .npy samples from STM32 via UART
  ├── split_dataset.py    ← split train data into train/val/test
  ├── train.py            ← train MLP or CNN1D model
  ├── infer.py            ← run inference on a single .npy file
  ├── live_infer.py       ← live inference from COM port in real time
  ├── export_tflite.py    ← export model to TFLite float + INT8
  ├── dataset.py          ← data loading helper (not run directly)
  ├── model.py            ← model architectures (not run directly)
  ├── requirements.txt    ← Python dependencies
  └── README.md           ← this file
  
  radar_dataset/          ← created during data collection
  ├── train/
  │   ├── approaching/    sample_0000.npy  sample_0001.npy  ...
  │   ├── idle/
  │   └── receding/
  ├── val/
  │   ├── approaching/
  │   ├── idle/
  │   └── receding/
  └── test/
      ├── approaching/
      ├── idle/
      └── receding/
  
  output/                 ← created during training
  ├── radar_mlp.keras
  ├── radar_mlp.tflite
  ├── radar_mlp_int8.tflite
  ├── normalization.json
  ├── training_curves.png
  └── confusion_matrix.png
```

Each `.npy` file has shape **(32, 10)** — 32 radar frames × 10 features:

| Index | Feature name | Description |
|-------|-------------|-------------|
| 0 | approaching_energy | Sum of magnitudes on positive Doppler side |
| 1 | receding_energy | Sum of magnitudes on negative Doppler side |
| 2 | approach_recede_ratio | approaching / receding energy |
| 3 | max_approach_peak | Strongest approaching bin |
| 4 | max_recede_peak | Strongest receding bin |
| 5 | total_energy | Sum of all non-DC magnitudes |
| 6 | peak_doppler | Frequency of strongest peak (Hz) |
| 7 | velocity | Estimated radial velocity (m/s) |
| 8 | center_of_mass | Weighted mean Doppler frequency (Hz) |
| 9 | spectral_width | Weighted standard deviation (Hz) |

---

## 2. Installation

### Requirements
- Python **3.10** or **3.11** (not 3.12 or 3.14 — TensorFlow not supported)
- Windows, macOS, or Linux

### Check your Python version
```powershell
py --list
```
You need `-3.10-64` or `-3.11-64` in the list.

### Create virtual environment
```powershell
# Windows
py -3.11 -m venv radar_venv
radar_venv\Scripts\activate

# macOS / Linux
python3.11 -m venv radar_venv
source radar_venv/bin/activate
```

You should see `(radar_venv)` at the start of your prompt.

### Install dependencies
```powershell
pip install -r requirements.txt
pip install tensorboard pyserial
```

### Activate environment every session
```powershell
# Windows — run this every time you open a new terminal
radar_venv\Scripts\activate

# macOS / Linux
source radar_venv/bin/activate
```

---

## 3. STM32 Firmware Setup

Before collecting data the STM32 firmware must be flashed and outputting **CSV format** over UART.

### Example firmware output format
```
timestamp_ms,f0,f1,f2,f3,f4,f5,f6,f7,f8,f9
123456,10.2,5.1,2.0,8.3,4.1,15.4,30.5,0.0,12.1,3.4
```

### Check in `radar_features.c`
Make sure `RadarFeatures_Print` sends CSV not human-readable format:
```c
int len = snprintf(buf, sizeof(buf),
    "%lu,%.2f,%.2f,%.4f,%.2f,%.2f,%.2f,%.2f,%.4f,%.2f,%.2f\r\n",
    (unsigned long)ts_ms,
    features->approaching_energy,
    ...
```

### UART settings
| Setting | Value |
|---------|-------|
| Baud rate | 576000 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |

### Find your COM port
**Windows:** Device Manager → Ports (COM & LPT) → look for `STMicroelectronics STLink Virtual COM Port (COMx)`

**macOS:** 
```bash
ls /dev/tty.usbmodem*
```

### Verify data is arriving (optional)
**Windows — PuTTY:**
- Open PuTTY → Serial → COM4 → Speed 576000 → Open
- You should see CSV lines scrolling

**macOS — picocom:**
```bash
picocom --baud 576000 --parity n --databits 8 /dev/tty.usbmodem1102
# press Ctrl+A then Ctrl+X to exit
```

**Windows — PowerShell:**
```powershell
python -c "import serial; s=serial.Serial('COM4',576000,timeout=1); [print(s.readline().decode('utf-8','ignore').strip()) for _ in iter(int,1)]"
```

A healthy signal with a person in front looks like:
```
204134,1823.4,210.5,8.66,930.2,88.1,2041.3,45.7,1.04,38.2,12.4
```

An empty room (no target) looks like:
```
204134,0.07,0.07,0.98,0.00,0.00,0.14,167.75,0.0,−0.05,878.45
```
`total_energy < 1.0` means no real target is detected.

---

## 4. Step 1 — Collect Dataset

Run `collect.py` separately for each class. The script pauses before each sample so you have time to get into position.

### Find available COM ports
```powershell
python collect.py --list_ports
```

### Collect training data (50 samples per class)
```powershell
# Approaching — walk toward the sensor each time
python collect.py --port COM4 --class_name approaching --samples 50 --output_dir radar_dataset/train/approaching

# Receding — walk away from sensor each time
python collect.py --port COM4 --class_name receding --samples 50 --output_dir radar_dataset/train/receding

# Idle — stand still or no person in view
python collect.py --port COM4 --class_name idle --samples 50 --output_dir radar_dataset/train/idle
```

### What happens during collection
```
Sample 1/50  (file: sample_0000.npy)
  Get into position for 'approaching'.
  Press ENTER to start recording …
  Recording 32 frames … ........
  ✓  saved → radar_dataset/train/approaching/sample_0000.npy  shape=(32, 10)
     Mean features: app_e=1823.40  rec_e=210.50  ratio=8.66  total_e=2041.30  peak_dop=45.7 Hz
```

### Tips for good quality data

**Approaching:**
- Start 3-4 metres from sensor
- Walk at normal pace directly toward it
- Do not stop before 32 frames finish

**Receding:**
- Start directly in front of sensor
- Walk away at normal pace
- Keep walking until sample finishes

**Idle:**
- Either stand completely still at 2m distance
- Or leave sensor pointing at empty room

### Recommended dataset size

| Split | Samples per class | Total |
|-------|-----------------|-------|
| train | 100–200 | 300–600 |
| val | 20–30 | 60–90 |
| test | 20–30 | 60–90 |

Minimum to get started: **50 per class per split**.

### macOS command (different port format)
```bash
python collect.py --port /dev/tty.usbmodem1102 --class_name approaching --samples 50 --output_dir radar_dataset/train/approaching
```

---

## 5. Step 2 — Split Dataset

If you only collected data into `train/`, run this script once to automatically split it into `train/val/test`:

```powershell
python split_dataset.py
```

The script uses `DATASET_DIR = "radar_dataset"` by default. Edit the path inside the script if your dataset is elsewhere.

**What it does:**
- Takes 15% of train samples → moves to `val/`
- Takes another 15% → moves to `test/`
- Remaining 70% stays in `train/`

**Example output:**
```
approaching     → train=70  val=15  test=15
idle            → train=70  val=15  test=15
receding        → train=52  val=11  test=11
Done. Dataset split complete.
```

> **Note:** If you already collected separate val and test data with `collect.py` you do not need to run this script.

---

## 6. Step 3 — Train the Model

### Train MLP model (recommended for STM32 deployment)
```powershell
python train.py --dataset radar_dataset --model_type mlp --epochs 80 --batch_size 16 --output_dir output
```

### Train 1D CNN model (smaller, better for MCU)
```powershell
python train.py --dataset radar_dataset --model_type cnn1d --epochs 80 --batch_size 16 --output_dir output
```

### All available arguments
```
--dataset      Path to radar_dataset folder          (required)
--model_type   mlp or cnn1d                          (default: mlp)
--epochs       Maximum training epochs               (default: 80)
--batch_size   Mini-batch size                       (default: 32)
--output_dir   Where to save model and plots         (default: output)
```

### Expected training output
```
── Loading dataset ─────────────────────────────────
  Loaded 210 samples from radar_dataset\train
  Loaded 45 samples from radar_dataset\val
  Loaded 45 samples from radar_dataset\test
  approaching: 70 training samples
  idle:        70 training samples
  receding:    70 training samples

── Training  (max 80 epochs, batch 16) ──
Epoch 1/80 - loss: 1.09 - accuracy: 0.38 - val_accuracy: 0.42
Epoch 10/80 - loss: 0.61 - accuracy: 0.74 - val_accuracy: 0.71
...
Epoch 45/80 - loss: 0.18 - accuracy: 0.94 - val_accuracy: 0.89

── Test Set Evaluation ──────────────────────────────
  Test accuracy: 0.8978  (89.8%)

Classification Report:
              precision  recall  f1-score
  approaching    0.88     0.91     0.89
  idle           0.93     0.93     0.93
  receding       0.87     0.84     0.86
```

### Output files created
```
output/
├── radar_mlp.keras          ← best model checkpoint
├── normalization.json       ← mean and std for each feature
├── training_curves.png      ← accuracy and loss plots
└── confusion_matrix.png     ← test set confusion matrix
```

### What good training looks like
- Validation accuracy above **85%** = good
- Train accuracy much higher than val accuracy = overfitting → collect more data
- Both accuracies low = underfitting → train longer or add more data

---

## 7. Step 4 — Export to TFLite

```powershell
python export_tflite.py --model output\radar_mlp.keras --dataset radar_dataset --normalization output\normalization.json --output_dir output
```

### Output files created
```
output/
├── radar_mlp.tflite          ← float32 TFLite (93 KB)
└── radar_mlp_int8.tflite     ← INT8 quantized (29 KB) ← use this for STM32
```

### Verify the TFLite model works
```powershell
python infer.py --model output\radar_mlp_int8.tflite --normalization output\normalization.json --sample radar_dataset\test\approaching\sample_0000.npy --tflite
```

---

## 8. Step 5 — Live Inference from Sensor

This is the main script for real-time use. It reads live data from the STM32 and prints predictions continuously.

### Using Keras model
```powershell
python live_infer.py --port COM4 --model output\radar_mlp.keras --normalization output\normalization.json
```

### Using TFLite INT8 model
```powershell
python live_infer.py --port COM4 --model output\radar_mlp_int8.tflite --normalization output\normalization.json --tflite
```

### macOS
```bash
python live_infer.py --port /dev/tty.usbmodem1102 --model output/radar_mlp.keras --normalization output/normalization.json
```

### Example terminal output
```
Listening on COM4 at 576000 baud
Press Ctrl+C to stop

────────────────────────────────────────────────────────────────────────
[Frame 00001]  ► APPROACHING   94.1%  │  app ████████████████████░░░░░  94%  idle ░  3%  rec ░  3%
[Frame 00002]  ► APPROACHING   91.3%  │  app ███████████████████░░░░░░  91%  idle ░  5%  rec ░  4%
[Frame 00003]  ► IDLE           87.2%  │  app ░  8%  idle ████████████████████░░░░░  87%  rec ░  5%
```

### All arguments
```
--port             COM port e.g. COM4 or /dev/tty.usbmodem1102   (required)
--model            Path to .keras or .tflite model file           (required)
--normalization    Path to normalization.json                      (required)
--baud             Baud rate                                       (default: 576000)
--tflite           Use TFLite interpreter instead of Keras        (flag)
```

Press **Ctrl+C** to stop.

---

## 9. Step 6 — Single Sample Inference

Test the model on one saved `.npy` file:

### Keras model
```powershell
python infer.py --model output\radar_mlp.keras --normalization output\normalization.json --sample radar_dataset\test\approaching\sample_0000.npy
```

### TFLite model
```powershell
python infer.py --model output\radar_mlp_int8.tflite --normalization output\normalization.json --sample radar_dataset\test\approaching\sample_0000.npy --tflite
```

### With verbose feature summary
```powershell
python infer.py --model output\radar_mlp.keras --normalization output\normalization.json --sample radar_dataset\test\approaching\sample_0000.npy --verbose
```

### Example output
```
==================================================
  Sample : radar_dataset\test\approaching\sample_0000.npy
  Result : APPROACHING  (94.3 % confidence)
==================================================
  Class probabilities:
    approaching    ██████████████████████████████  94.3 %
    idle           ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   2.1 %
    receding       ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   3.6 %
==================================================
```

---

## 10. Output Files Reference

| File | Location | Purpose |
|------|----------|---------|
| `radar_mlp.keras` | `output/` | Full Keras model — use for Python inference |
| `radar_mlp.tflite` | `output/` | Float32 TFLite — for testing |
| `radar_mlp_int8.tflite` | `output/` | INT8 quantized — for STM32 deployment |
| `normalization.json` | `output/` | Feature mean + std — required for all inference |
| `training_curves.png` | `output/` | Training and validation accuracy/loss plots |
| `confusion_matrix.png` | `output/` | Test set confusion matrix |

> **Important:** `normalization.json` must always be used together with the model.
> The model gives wrong predictions without it.

---

## 11. Troubleshooting

### `ERROR: No matching distribution found for tensorflow`
Your Python version is not supported. Use Python 3.10 or 3.11:
```powershell
py -3.10 -m venv radar_venv
```

### `TBNotInstalledError`
TensorBoard not installed:
```powershell
pip install tensorboard
```

### `No rule to make target stm32n6xx_hal.c`
Drivers folder missing from repo. Copy from ST package:
```powershell
# find ST package
dir C:\Users\%USERNAME%\STM32Cube\Repository\ /b
# copy Drivers
xcopy /E /I "C:\Users\%USERNAME%\STM32Cube\Repository\STM32Cube_FW_N6_V1.3.0\Drivers" "radar_prj\Drivers"
```

### `Could not open port COM4`
- Check Device Manager for correct port number
- Close PuTTY or any other serial terminal using the port
- Try unplugging and replugging the USB cable

### `total_energy < 1.0` — all features near zero
STM32 is sending data but no real target is detected. Stand directly in front of the sensor and wave your hand. If still no signal check ADC wiring and SPI communication.

### Model accuracy below 70%
1. Verify `.npy` files contain real signal — check `total_energy` values
2. Make sure firmware outputs CSV format not human-readable format
3. Collect more samples — aim for 100+ per class
4. Collect in varied conditions — different distances and angles

### `ValueError: Number of classes does not match target_names`
Val or test folder is missing one class. Run `split_dataset.py` or collect the missing class:
```powershell
python collect.py --port COM4 --class_name approaching --samples 15 --output_dir radar_dataset/val/approaching
```

### `implicit declaration of function spiSendReceiveByte`
Add this stub to `ads131m0x.c` before the `HAL_SPI_AdcCallback` function:
```c
static uint8_t spiSendReceiveByte(uint8_t tx_byte)
{
    uint8_t rx_byte = 0;
    HAL_SPI_TransmitReceive(&hspi5, &tx_byte, &rx_byte, 1, HAL_MAX_DELAY);
    return rx_byte;
}
```

---

## 12. STM32 Deployment

To run inference directly on the STM32 without a PC:

### Step 1 — export INT8 TFLite
```powershell
python export_tflite.py --model output\radar_mlp.keras --dataset radar_dataset --normalization output\normalization.json --output_dir output
```

### Step 2 — import to STM32Cube.AI
1. Open STM32CubeIDE
2. Open X-CUBE-AI or STEdgeAI
3. Import `output/radar_mlp_int8.tflite`
4. Validate and generate C code

### Step 3 — add normalization to firmware
Open `normalization.json` and hard-code the values in C:
```c
// values from normalization.json
static const float NORM_MEAN[10] = { /* paste mean values here */ };
static const float NORM_STD[10]  = { /* paste std values here  */ };

// apply before inference
for (int t = 0; t < 32; t++)
    for (int f = 0; f < 10; f++)
        input[t][f] = (raw[t][f] - NORM_MEAN[f]) / NORM_STD[f];
```

### Memory footprint

| Model | Flash | RAM | Inference time |
|-------|-------|-----|---------------|
| MLP float32 | ~90 KB | ~5 KB | ~0.5 ms |
| MLP INT8 | ~25 KB | ~3 KB | ~0.3 ms |
| CNN1D float32 | ~28 KB | ~8 KB | ~1.2 ms |
| CNN1D INT8 | ~8 KB | ~4 KB | ~0.6 ms |

---

## 13. Improving Accuracy

### Most impactful — collect more data
| Samples per class | Expected accuracy |
|------------------|-------------------|
| 50 | 70–80% |
| 100 | 80–88% |
| 200+ | 88–95% |

### Recollect approaching data properly
- Start 3-4 metres from sensor
- Walk at normal pace — not too slow
- Vary distances and angles across samples
- Do NOT stop before sample recording finishes

### Try CNN1D instead of MLP
```powershell
python train.py --dataset radar_dataset --model_type cnn1d --epochs 80 --batch_size 16 --output_dir output
```
CNN1D captures temporal patterns across frames and often outperforms MLP by 3-8%.

### Add presence threshold in live_infer.py
Open `live_infer.py` and set `ENERGY_THRESHOLD = 5.0`. Frames with `total_energy` below this are reported as `NO TARGET` instead of forcing a class prediction on noise.

### Change number of classes
Edit `dataset.py` and `model.py`:
```python
# dataset.py
CLASSES = ["approaching", "idle", "receding"]   # 3 classes
# or
CLASSES = ["approaching", "idle", "receding", "mixed"]   # 4 classes

# model.py
NUM_CLASSES = 3   # must match number of classes above
```

---

## Quick Reference — All Commands

```powershell
# Setup
py -3.10 -m venv radar_venv
radar_venv\Scripts\activate
pip install -r requirements.txt
pip install tensorboard pyserial

# Collect data
python collect.py --port COM4 --class_name approaching --samples 50 --output_dir radar_dataset/train/approaching
python collect.py --port COM4 --class_name receding    --samples 50 --output_dir radar_dataset/train/receding
python collect.py --port COM4 --class_name idle        --samples 50 --output_dir radar_dataset/train/idle

# Split into train/val/test
python split_dataset.py

# Train
python train.py --dataset radar_dataset --model_type mlp --epochs 80 --batch_size 16 --output_dir output

# Export TFLite
python export_tflite.py --model output\radar_mlp.keras --dataset radar_dataset --normalization output\normalization.json --output_dir output

# Live inference from sensor
python live_infer.py --port COM4 --model output\radar_mlp.keras --normalization output\normalization.json

# Single file inference
python infer.py --model output\radar_mlp.keras --normalization output\normalization.json --sample radar_dataset\test\approaching\sample_0000.npy
```
