# Embedded Edge AI Motion Classifier

This project is a real-time embedded motion classification system. An **STM32F4 BlackPill** reads acceleration data from an **ICM20602 IMU** over SPI and sends the data to an **ESP32** over UART. The ESP32 runs a small neural network with weights trained on a PC and predicts the performed motion as `O`, `Z`, or `I`.

```text
ICM20602 IMU -> STM32F4 BlackPill -> UART -> ESP32 -> Neural Network -> O / Z / I
```



This project is an **Embedded Edge AI** project. The model is trained on a PC, but inference runs locally on the ESP32, close to the sensor data source, without needing a PC or cloud connection during prediction.

## Hardware and Communication Flow

Main hardware blocks:

- **STM32F4 BlackPill**: sensor interface, GPIO/SPI/UART/DMA control, periodic data transmission.
- **ICM20602 IMU**: acceleration and gyroscope sensor.
- **ESP32**: UART receiver and neural network inference target.

The STM32 sends acceleration data to the ESP32 in centi-g format:

```text
accel_x*100,accel_y*100,accel_z*100\r\n
```

For example, `1.00 g` is sent as `100`.

## STM32F4 BlackPill Firmware

The STM32 side is responsible for reliable sensor acquisition and UART streaming. The code uses a clean GPIO abstraction through `GPIO_Config_t`, so SPI, UART, and chip-select pins are configured in a reusable way instead of being hardcoded repeatedly.

Main STM32 features:

- Register-level STM32F4 peripheral configuration.
- FreeRTOS task for periodic sensor sampling.
- ICM20602 connection over **SPI1**.
- SPI burst read of 14 consecutive IMU bytes.
- Sensor initialization, calibration, and raw-to-physical-unit conversion.
- UART2 transmission using DMA.
- 10 ms sampling period.

Relevant pin usage in the current code:

```text
SPI1 SCK  -> PB3
SPI1 MISO -> PB4
SPI1 MOSI -> PB5
ICM CS    -> PA15
UART2 TX  -> PA2
```

The STM32 reads acceleration and gyroscope data from the ICM20602, but the current neural network pipeline uses only the acceleration axes: X, Y, and Z.

## ESP32 Firmware

The ESP32 receives UART data, detects when a motion starts, collects a fixed-size input window, normalizes the data, and runs neural network inference.

Runtime configuration:

| Item | Value |
|---|---:|
| UART port | UART1 |
| Baud rate | 115200 |
| ESP32 RX / TX | GPIO16 / GPIO17 |
| Motion window | 180 samples |
| Input axes | X, Y, Z |
| Neural network input size | 540 |
| Motion trigger threshold | 18 cG |
| Post-prediction cooldown | 1200 ms |

The ESP32 does not classify every single UART sample. It first compares the acceleration difference between consecutive samples. When the delta passes the motion threshold, it starts collecting a 180-sample window. After the window is full, the data is normalized per axis using the window mean and standard deviation.

## Neural Network Inference

The neural network is intentionally small so it can run directly on the ESP32 without an external ML runtime.

Architecture:

```text
Input      : 540 values = 180 samples * 3 acceleration axes
Dense 1    : 540 -> 16
Activation : ReLU
Dense 2    : 16 -> 3
Output     : O / Z / I logits
```

Embedded class order:

```text
0 = O
1 = Z
2 = I
```

`neural_network.c` implements the embedded forward pass:

1. The 540-value normalized input vector is multiplied by `dense1_weights` and added to `dense1_biases`.
2. ReLU is applied to the 16 hidden neurons.
3. The hidden layer is multiplied by `dense2_weights` and added to `dense2_biases`.
4. The output layer produces three logits.
5. The class with the highest logit is selected using `argmax`.

`softmax` is used on the ESP32 side only to print readable probability values. The actual prediction is selected from the raw logits.

The trained parameters are exported into `model_weights.h`:

```c
const float dense1_weights[8640];
const float dense1_biases[16];
const float dense2_weights[48];
const float dense2_biases[3];
```

## Python Training Pipeline

The neural network is trained on a PC before being deployed to the ESP32. The training side contains an OOP-style neural network implementation, while the ESP32 side only keeps the lightweight forward inference function.

Python file roles:

- `serial_csv_auto_saver.py`: records IMU data from the serial port into CSV files.
- `data_preprocessing.py`: prepares the recorded CSV data for training.
- `ai_train.py`: trains the neural network and exports the final weights into C header format.

In `ai_train.py`, each recorded motion window is converted into a 540-value input vector. The code performs forward propagation, calculates the training error, applies backward propagation, updates the weights and biases, and finally exports the trained arrays used by the embedded C inference code.

Training dataset:

| Class | Recordings | Samples per recording | Axes | Input size |
|---|---:|---:|---:|---:|
| `O` | 20 | 180 | 3 | 540 |
| `Z` | 20 | 180 | 3 | 540 |
| `I` | 20 | 180 | 3 | 540 |

In total, 60 labeled gesture recordings were collected. Each recording contains 180 samples, and each sample contains X, Y, and Z acceleration values.

## Source Layout

```text
STM32
main.c                    FreeRTOS sampling task and UART data flow
ICM20602_driver.c         IMU initialization, burst read, calibration, scaling
communication.c           SPI1, UART2, DMA, and integer-to-string helpers
gpio_driver.c             Reusable GPIO configuration layer
system_clock.c            STM32F4 clock configuration
delay.c                   Timing helpers

ESP32
esp32_link.c              UART receive, motion detection, normalization, softmax, app_main
neural_network.c          Embedded neural network forward inference
neural_network.h          Neural network size definitions
model_weights.h           PC-trained exported weights and biases

Python
serial_csv_auto_saver.py  Serial data collection into CSV files
data_preprocessing.py     Dataset preparation
ai_train.py               OOP neural network training and weight export
```

## Authorship Note

For transparency:

In `esp32_link.c`, only the softmax, normalization, and `app_main` parts were written manually; the other parts were AI-generated.

In the Python files, only `ai_train.py` was written manually; the other Python files (`serial_csv_auto_saver.py`, `data_preprocessing.py`) were AI-generated.
## Video
`!Some of my movements in the video are out of sync; they don't match the video!`

- https://youtu.be/WerAh7AALbo
