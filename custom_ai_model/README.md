# Radar AI Training Pipeline

Small supervised classification model for radar motion patterns, designed for eventual deployment on **STM32N6** via **STM32Cube.AI / STEdgeAI**.

---

## Project structure

```
radar_ai_training/
├── train.py            # Full training pipeline
├── infer.py            # Single-sample inference
├── model.py            # MLP and 1D-CNN architectures
├── dataset.py          # Data loading and normalisation
├── export_tflite.py    # TFLite float + INT8 export
├── requirements.txt
└── README.md
```

---

## Expected dataset layout

```
radar_dataset/
├── train/
│   ├── approaching/   sample_001.npy  sample_002.npy  …
│   ├── receding/      sample_001.npy  …
│   ├── idle/          sample_001.npy  …
│   └── mixed/         sample_001.npy  …
├── val/
│   ├── approaching/  …
│   ├── receding/     …
│   ├── idle/         …
│   └── mixed/        …
└── test/
    ├── approaching/  …
    ├── receding/     …
    ├── idle/         …
    └── mixed/        …
```

Each `.npy` file must have shape **(32, 10)**:

| axis-0 | axis-1 |
|--------|--------|
| 32 time steps (radar frames) | 10 engineered features per frame |

### Feature order (axis-1)

| Index | Feature name           | Description                              |
|-------|------------------------|------------------------------------------|
| 0     | approaching_energy     | Sum of magnitudes on approaching side    |
| 1     | receding_energy        | Sum of magnitudes on receding side       |
| 2     | approach_recede_ratio  | approaching_energy / receding_energy     |
| 3     | max_approach_peak      | Peak magnitude on approaching side       |
| 4     | max_recede_peak        | Peak magnitude on receding side          |
| 5     | total_energy           | Sum of all non-DC magnitudes             |
| 6     | peak_doppler           | Frequency (Hz) of strongest non-DC peak |
| 7     | velocity               | Estimated radial velocity (m/s)          |
| 8     | center_of_mass         | Weighted average Doppler frequency (Hz)  |
| 9     | spectral_width         | Weighted std deviation around CoM (Hz)   |

### Classes

| Label | Class name   |
|-------|--------------|
| 0     | approaching  |
| 1     | idle         |
| 2     | mixed        |
| 3     | receding     |

---

## 1. Install dependencies

```bash
# Create and activate a virtual environment (recommended)
python -m venv venv
source venv/bin/activate        # Linux / macOS
# venv\Scripts\activate         # Windows

pip install -r requirements.txt
```

Tested with Python 3.10 and TensorFlow 2.13.

---

## 2. Train the MLP model

```bash
python train.py \
    --dataset    radar_dataset \
    --model_type mlp \
    --epochs     80 \
    --batch_size 32 \
    --output_dir output
```

Output files created in `output/`:

```
output/
├── radar_mlp.keras          # Best Keras model checkpoint
├── normalization.json       # Per-feature mean and std (training set only)
├── training_curves.png      # Accuracy and loss plots
└── confusion_matrix.png     # Test-set confusion matrix
```

---

## 3. Train the 1D CNN model

```bash
python train.py \
    --dataset    radar_dataset \
    --model_type cnn1d \
    --epochs     80 \
    --batch_size 32 \
    --output_dir output
```

---

## 4. Export to TFLite (for STM32 deployment)

```bash
python export_tflite.py \
    --model         output/radar_mlp.keras \
    --dataset       radar_dataset \
    --normalization output/normalization.json \
    --output_dir    output
```

This creates:

```
output/
├── radar_mlp.tflite          # Float32 TFLite model
└── radar_mlp_int8.tflite     # Fully INT8 quantized model  ← preferred for STM32
```

---

## 5. Run inference on a single sample

### Using the Keras model

```bash
python infer.py \
    --model         output/radar_mlp.keras \
    --normalization output/normalization.json \
    --sample        radar_dataset/test/approaching/sample_001.npy
```

### Using the TFLite model

```bash
python infer.py \
    --model         output/radar_mlp_int8.tflite \
    --normalization output/normalization.json \
    --sample        radar_dataset/test/approaching/sample_001.npy \
    --tflite
```

### Example output

```
==================================================
  Sample : radar_dataset/test/approaching/sample_001.npy
  Result : APPROACHING  (94.3 % confidence)
==================================================
  Class probabilities:
    approaching    ██████████████████████████████  94.3 %
    idle           ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   2.1 %
    mixed          ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   2.8 %
    receding       ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   0.8 %
==================================================
```

Add `--verbose` to also see a per-feature summary of the input.

---

## STM32N6 deployment files

These two files must **both** be deployed to your STM32 project:

| File | Purpose |
|------|---------|
| `output/radar_mlp_int8.tflite` | Quantized model for STM32Cube.AI |
| `output/normalization.json` | Z-score normalisation parameters |

### Normalisation on the MCU (C pseudocode)

```c
// normalization.json contains mean[10] and std[10]
// Apply BEFORE feeding data to the TFLite model

float features_raw[32][10];   // collected from radar_features.c
float features_norm[32][10];  // input to TFLite

for (int t = 0; t < 32; t++) {
    for (int f = 0; f < 10; f++) {
        features_norm[t][f] = (features_raw[t][f] - mean[f]) / std[f];
    }
}
```

### STM32Cube.AI / STEdgeAI steps

1. Open STM32CubeIDE → X-CUBE-AI or STEdgeAI.
2. Import `radar_mlp_int8.tflite`.
3. Validate the model (check memory footprint fits STM32N6 RAM).
4. Generate C code.
5. Hard-code the `mean` and `std` arrays from `normalization.json` in your firmware.
6. Call the normalisation before each inference call.

---

## Model sizes (approximate)

| Model | Parameters | Float .tflite | INT8 .tflite |
|-------|-----------|---------------|--------------|
| MLP   | ~22 k     | ~90 KB        | ~25 KB       |
| CNN1D | ~6 k      | ~28 KB        | ~8 KB        |

The CNN1D model is recommended for STM32 deployment due to its smaller size.

---

## Reproducibility

All scripts use `SEED = 42` for Python, NumPy, and TensorFlow random generators.
