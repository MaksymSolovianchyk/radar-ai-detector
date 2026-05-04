"""
train.py
--------
Full training pipeline for radar motion classification.

Input:  radar_dataset/  (see dataset.py for folder structure)
Output: output/
            radar_mlp.keras          (or radar_cnn1d.keras)
            normalization.json
            training_curves.png
            confusion_matrix.png

Usage examples
--------------
    # Train MLP (default)
    python train.py --dataset radar_dataset

    # Train 1D CNN with custom settings
    python train.py --dataset radar_dataset \
                    --model_type cnn1d \
                    --epochs 100 \
                    --batch_size 16 \
                    --output_dir output
"""

import os
import argparse
import random
import json
import numpy as np
import matplotlib
matplotlib.use("Agg")   # non-interactive backend — safe for servers
import matplotlib.pyplot as plt

import tensorflow as tf
from tensorflow import keras

from sklearn.metrics import confusion_matrix, classification_report
import seaborn as sns

from dataset import (
    load_dataset,
    compute_normalization,
    save_normalization,
    apply_normalization,
    CLASSES,
)
from model import get_model

# ── reproducibility ────────────────────────────────────────────────────────────
SEED = 42
os.environ["PYTHONHASHSEED"] = str(SEED)
random.seed(SEED)
np.random.seed(SEED)
tf.random.set_seed(SEED)


# ── plotting helpers ────────────────────────────────────────────────────────────
def plot_training_curves(history: keras.callbacks.History,
                         output_path: str) -> None:
    """Save accuracy and loss curves to a PNG file."""
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))

    # Accuracy
    axes[0].plot(history.history["accuracy"],     label="train acc")
    axes[0].plot(history.history["val_accuracy"], label="val acc")
    axes[0].set_title("Accuracy")
    axes[0].set_xlabel("Epoch")
    axes[0].set_ylabel("Accuracy")
    axes[0].legend()
    axes[0].grid(True)

    # Loss
    axes[1].plot(history.history["loss"],     label="train loss")
    axes[1].plot(history.history["val_loss"], label="val loss")
    axes[1].set_title("Loss")
    axes[1].set_xlabel("Epoch")
    axes[1].set_ylabel("Loss")
    axes[1].legend()
    axes[1].grid(True)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()
    print(f"Training curves saved: {output_path}")


def plot_confusion_matrix(y_true: np.ndarray,
                          y_pred: np.ndarray,
                          class_names: list,
                          output_path: str) -> None:
    """Save a labelled confusion matrix heatmap to a PNG file."""
    cm = confusion_matrix(y_true, y_pred)
    fig, ax = plt.subplots(figsize=(6, 5))
    sns.heatmap(
        cm,
        annot=True,
        fmt="d",
        cmap="Blues",
        xticklabels=class_names,
        yticklabels=class_names,
        ax=ax,
    )
    ax.set_xlabel("Predicted")
    ax.set_ylabel("True")
    ax.set_title("Confusion Matrix (Test Set)")
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()
    print(f"Confusion matrix saved: {output_path}")


# ── evaluation ─────────────────────────────────────────────────────────────────
def evaluate(model: keras.Model,
             X_test: np.ndarray,
             y_test: np.ndarray,
             output_dir: str) -> None:
    """
    Evaluate on the test set, print metrics, and save the confusion matrix.
    """
    print("\n── Test Set Evaluation ─────────────────────────────────────────")

    loss, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"  Test loss:     {loss:.4f}")
    print(f"  Test accuracy: {acc:.4f}  ({acc*100:.1f} %)")

    y_pred = np.argmax(model.predict(X_test, verbose=0), axis=1)

    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=CLASSES))

    plot_confusion_matrix(
        y_test, y_pred, CLASSES,
        os.path.join(output_dir, "confusion_matrix.png")
    )


# ── main ────────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Train a radar motion classification model."
    )
    parser.add_argument(
        "--dataset", required=True,
        help="Path to the radar_dataset root folder."
    )
    parser.add_argument(
        "--model_type", default="mlp", choices=["mlp", "cnn1d"],
        help="Model architecture: 'mlp' (default) or 'cnn1d'."
    )
    parser.add_argument(
        "--epochs", type=int, default=80,
        help="Maximum number of training epochs (default: 80)."
    )
    parser.add_argument(
        "--batch_size", type=int, default=32,
        help="Mini-batch size (default: 32)."
    )
    parser.add_argument(
        "--output_dir", default="output",
        help="Directory for saved model, normalization, and plots (default: output/)."
    )
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    # ── 1. Load data ─────────────────────────────────────────────────────────
    print("\n── Loading dataset ─────────────────────────────────────────────")
    (X_train, y_train), (X_val, y_val), (X_test, y_test) = load_dataset(args.dataset)

    print(f"\n  Train: {X_train.shape}  labels: {y_train.shape}")
    print(f"  Val:   {X_val.shape}    labels: {y_val.shape}")
    print(f"  Test:  {X_test.shape}   labels: {y_test.shape}")

    # Class distribution
    for i, name in enumerate(CLASSES):
        n = int((y_train == i).sum())
        print(f"    {name}: {n} training samples")

    # ── 2. Normalisation (computed from train only) ───────────────────────────
    print("\n── Computing normalisation ─────────────────────────────────────")
    norm_params = compute_normalization(X_train)

    norm_path = os.path.join(args.output_dir, "normalization.json")
    save_normalization(norm_params, norm_path)

    X_train_n = apply_normalization(X_train, norm_params)
    X_val_n   = apply_normalization(X_val,   norm_params)
    X_test_n  = apply_normalization(X_test,  norm_params)

    # ── 3. Build model ────────────────────────────────────────────────────────
    print(f"\n── Building model: {args.model_type} ───────────────────────────")
    model = get_model(args.model_type)

    # ── 4. Callbacks ─────────────────────────────────────────────────────────
    model_filename = f"radar_{args.model_type}.keras"
    model_path = os.path.join(args.output_dir, model_filename)

    callbacks = [
        # Save the best model (by val_accuracy)
        keras.callbacks.ModelCheckpoint(
            filepath=model_path,
            monitor="val_accuracy",
            save_best_only=True,
            verbose=1,
        ),
        # Stop early if val_loss stops improving
        keras.callbacks.EarlyStopping(
            monitor="val_loss",
            patience=15,
            restore_best_weights=True,
            verbose=1,
        ),
        # Reduce LR when val_loss plateaus
        keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss",
            factor=0.5,
            patience=7,
            min_lr=1e-6,
            verbose=1,
        ),
        # TensorBoard logs (optional, does not require TensorBoard to be running)
        keras.callbacks.TensorBoard(
            log_dir=os.path.join(args.output_dir, "tb_logs"),
            histogram_freq=0,
        ),
    ]

    # ── 5. Train ─────────────────────────────────────────────────────────────
    print(f"\n── Training  (max {args.epochs} epochs, batch {args.batch_size}) ──")
    history = model.fit(
        X_train_n, y_train,
        validation_data=(X_val_n, y_val),
        epochs=args.epochs,
        batch_size=args.batch_size,
        callbacks=callbacks,
        verbose=1,
    )

    # ── 6. Load best checkpoint, evaluate ────────────────────────────────────
    print(f"\nLoading best checkpoint: {model_path}")
    best_model = keras.models.load_model(model_path)

    evaluate(best_model, X_test_n, y_test, args.output_dir)

    # ── 7. Save plots ─────────────────────────────────────────────────────────
    print("\n── Saving plots ────────────────────────────────────────────────")
    plot_training_curves(
        history,
        os.path.join(args.output_dir, "training_curves.png")
    )

    # ── 8. Summary ────────────────────────────────────────────────────────────
    print("\n── Output files ────────────────────────────────────────────────")
    print(f"  Keras model:        {model_path}")
    print(f"  Normalization:      {norm_path}")
    print(f"  Training curves:    {args.output_dir}/training_curves.png")
    print(f"  Confusion matrix:   {args.output_dir}/confusion_matrix.png")
    print("\nNext step — export to TFLite:")
    print(f"  python export_tflite.py --model {model_path} \\")
    print(f"         --dataset {args.dataset} \\")
    print(f"         --normalization {norm_path} \\")
    print(f"         --output_dir {args.output_dir}")


if __name__ == "__main__":
    main()
