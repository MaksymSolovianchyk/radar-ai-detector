"""
model.py
--------
Two small model architectures for radar motion classification.

Input shape for both models: (32, 10)
    32 = time steps (radar frames)
    10 = engineered features per frame

Output: 4-class softmax  [approaching, idle, mixed, receding]

Both models are intentionally kept small for STM32N6 deployment
via STM32Cube.AI / STEdgeAI.

STM32 deployment note
---------------------
After training, convert the saved Keras model to TFLite using
export_tflite.py.  Both the .tflite model file AND the normalization
parameters (normalization.json) must be deployed to the STM32.
The MCU must apply the same z-score normalisation before inference.
"""

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
from typing import Tuple

NUM_CLASSES = 4
INPUT_SHAPE: Tuple[int, int] = (32, 10)   # (time_steps, features)


def build_mlp(input_shape: Tuple[int, int] = INPUT_SHAPE,
              num_classes: int = NUM_CLASSES) -> keras.Model:
    """
    Small MLP model.

    Architecture:
        Input  (32, 10)
        Flatten → 320-dim vector
        Dense(64, relu)
        Dropout(0.2)
        Dense(32, relu)
        Dense(4, softmax)

    Parameter count: ~22 k  — suitable for STM32 SRAM.

    Parameters
    ----------
    input_shape : (time_steps, features)
    num_classes : number of output classes

    Returns
    -------
    Compiled Keras model (untrainable until you call model.compile()).
    """
    inputs = keras.Input(shape=input_shape, name="radar_input")

    x = layers.Flatten(name="flatten")(inputs)                       # 320

    x = layers.Dense(64, activation="relu", name="dense_1")(x)
    x = layers.Dropout(0.2, name="dropout_1")(x)

    x = layers.Dense(32, activation="relu", name="dense_2")(x)

    outputs = layers.Dense(num_classes, activation="softmax",
                           name="output")(x)

    model = keras.Model(inputs=inputs, outputs=outputs, name="radar_mlp")
    return model


def build_cnn1d(input_shape: Tuple[int, int] = INPUT_SHAPE,
                num_classes: int = NUM_CLASSES) -> keras.Model:
    """
    Lightweight 1-D CNN model.

    Architecture:
        Input  (32, 10)
        Conv1D(16, kernel=3, relu, same padding)
        Conv1D(32, kernel=3, relu, same padding)
        GlobalAveragePooling1D
        Dense(32, relu)
        Dense(4, softmax)

    Parameter count: ~6 k  — very compact for STM32.

    Parameters
    ----------
    input_shape : (time_steps, features)
    num_classes : number of output classes

    Returns
    -------
    Compiled Keras model.
    """
    inputs = keras.Input(shape=input_shape, name="radar_input")

    x = layers.Conv1D(16, kernel_size=3, activation="relu",
                      padding="same", name="conv1")(inputs)           # (32, 16)

    x = layers.Conv1D(32, kernel_size=3, activation="relu",
                      padding="same", name="conv2")(x)                # (32, 32)

    x = layers.GlobalAveragePooling1D(name="gap")(x)                  # (32,)

    x = layers.Dense(32, activation="relu", name="dense_1")(x)

    outputs = layers.Dense(num_classes, activation="softmax",
                           name="output")(x)

    model = keras.Model(inputs=inputs, outputs=outputs, name="radar_cnn1d")
    return model


def compile_model(model: keras.Model,
                  learning_rate: float = 1e-3) -> keras.Model:
    """
    Compile a model with Adam + sparse categorical crossentropy.

    Parameters
    ----------
    model         : uncompiled Keras model
    learning_rate : initial learning rate for Adam

    Returns
    -------
    Compiled model (same object, mutated in place).
    """
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def get_model(model_type: str = "mlp") -> keras.Model:
    """
    Factory function — returns a compiled model by name.

    Parameters
    ----------
    model_type : "mlp" or "cnn1d"

    Returns
    -------
    Compiled Keras model ready for training.
    """
    model_type = model_type.lower()
    if model_type == "mlp":
        model = build_mlp()
    elif model_type == "cnn1d":
        model = build_cnn1d()
    else:
        raise ValueError(f"Unknown model_type '{model_type}'. Choose 'mlp' or 'cnn1d'.")

    model = compile_model(model)
    model.summary()
    return model
