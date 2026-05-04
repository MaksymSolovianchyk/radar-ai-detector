# Project Configuration Tutorial in CubeMX

## Table of Contents
- [Project Configuration Tutorial in CubeMX](#project-configuration-tutorial-in-cubemx)
  - [SPI5 Configuration (Sensor Data)](#spi5-configuration-sensor-data)
  - [USART1 Configuration (Transmission Data)](#usart1-configuration-transmission-data-into-terminal)
  - [DMA Configuration](#dma-configuration)
  - [GPIO Configuration](#gpio-configuration)
  - [NVIC Configuration](#nvic-configuration-nvic)
- [Commands](#commands)

---

## Project Configuration Tutorial in CubeMX
<a id="project-configuration-tutorial-in-cubemx"></a>

---

<details open>
<summary><h3>SPI5 Configuration (Sensor Data) <a id="spi5-configuration-sensor-data"></a></h3></summary>

SPI5 is configured in the **AppliSecure** context.

- **Mode:** Master
- **Virtual type:** VM_MASTER
- **Direction:** 2 lines
- **Data size:** 8-bit
- **Clock phase:** 2nd edge
- **NSS:** Software
- **Baud rate prescaler:** 64
- **Calculated baud rate:** 1.5625 Mbit/s

**SPI5 pins**
- **SCK:** PE15
- **MOSI:** PG2
- **MISO:** PH8

![SPI5 Configuration](/docs/img/spi5_config.png)

</details>

---

<details open>
<summary><h3>USART1 Configuration (Transmission Data into Terminal) <a id="usart1-configuration-transmission-data-into-terminal"></a></h3></summary>

USART1 is configured in the **AppliSecure** context.

- **Mode:** Asynchronous
- **Baud rate:** 576000
- **Word length:** 8 bits
- **Parity:** None
- **Oversampling:** 8

**USART1 pins**
- **TX:** PE5
- **RX:** PE6

</details>

---

<details open>
<summary><h3>DMA Configuration <a id="dma-configuration"></a></h3></summary>

Select **GPDMA1**. In section **GPDMA1 Mode and Configuration** select Channel 0, 1, 2, 3 as **Standard Request**. In the **Configuration** section 4 channels will appear: **CH0–3**.

**Configured simple request DMA channels:**
- **Channel 0:** SPI5_RX
- **Channel 1:** SPI5_TX
- **Channel 2:** USART1_RX
- **Channel 3:** USART1_TX

**DMA transfer properties:**

<details>
<summary><b>Channel 0 — SPI5_RX</b></summary>

- **Request:** SPI5_RX
- **Direction:** Peripheral to Memory
- **Source data width:** Byte
- **Destination data width:** Byte
- **Source Address increment:** Disabled *(Fixed in .ioc)*
- **Source Allocated Port for Transfer:** Port 1
- **Destination Address increment:** Enabled *(Incremented in .ioc)*
- **Dest Allocated Port for Transfer:** Port 0
- **Circular mode:** Disabled

</details>

<details>
<summary><b>Channel 1 — SPI5_TX</b></summary>

- **Request:** SPI5_TX
- **Direction:** Memory to Peripheral
- **Source data width:** Byte
- **Destination data width:** Byte
- **Source Address increment:** Enabled *(Incremented in .ioc)*
- **Source Allocated Port for Transfer:** Port 0
- **Destination Address increment:** Disabled *(Fixed in .ioc)*
- **Dest Allocated Port for Transfer:** Port 1

</details>

<details>
<summary><b>Channel 2 — USART1_RX</b></summary>

- **Request:** USART1_RX
- **Direction:** Peripheral to Memory
- **Source data width:** Byte
- **Destination data width:** Byte
- **Source Address increment:** Disabled *(Fixed in .ioc)*
- **Source Allocated Port for Transfer:** Port 1
- **Destination Address increment:** Enabled *(Incremented in .ioc)*
- **Dest Allocated Port for Transfer:** Port 0

</details>

<details>
<summary><b>Channel 3 — USART1_TX</b></summary>

- **Request:** USART1_TX
- **Direction:** Memory to Peripheral
- **Source Address increment:** Enabled *(Incremented in .ioc)*
- **Source Allocated Port for Transfer:** Port 0
- **Destination Address increment:** Disabled *(Fixed in .ioc)*
- **Dest Allocated Port for Transfer:** Port 1

</details>

</details>

---

<details open>
<summary><h3>GPIO Configuration <a id="gpio-configuration"></a></h3></summary>

**AppliSecure GPIO outputs**

<details>
<summary><b>PA3 — ADC_SPI_CS1</b></summary>

- **Label:** ADC_SPI_CS1
- **Signal:** GPIO_Output
- **Default state:** GPIO_PIN_SET
- **Meaning:** Starts High

</details>

<details>
<summary><b>PB3 — SYNC_RESET</b></summary>

- **Label:** SYNC_RESET
- **Signal:** GPIO_Output
- **Default state:** GPIO_PIN_SET

</details>

<details>
<summary><b>PC9 — PSUEN</b></summary>

- **Label:** PSUEN
- **Signal:** GPIO_Output

</details>

**AppliSecure GPIO interrupt input**

<details>
<summary><b>PC11 — Interrupt Input</b></summary>

- **Signal:** GPXTI11
- **Mode:** GPIO_MODE_IT_FALLING

</details>

![GPIO Configuration](/docs/img/gpio_config.png)

</details>

---

<details open>
<summary><h3>NVIC1_S_Application Configuration (NVIC) <a id="nvic-configuration-nvic"></a></h3></summary>

**AppliSecure interrupts enabled:**
- `EXTI11_IRQn`
- `GPDMA1_Channel0_IRQn`
- `GPDMA1_Channel1_IRQn`
- `GPDMA1_Channel2_IRQn`
- `GPDMA1_Channel3_IRQn`
- `SPI5_IRQn`
- `USART1_IRQn`

</details>

---

## Commands
<a id="commands"></a>

To see sensor output in terminal:
```bash
picocom --baud 576000 --parity n --databits 8 --stopbits 1 /dev/tty.usbmodem1102
```

Where `/dev/tty.usbmodem1102` is the USB/USB-C port on your PC.

**Find your port:**
- **macOS:** `ls /dev/tty.*`
- **Windows:** `[System.IO.Ports.SerialPort]::getportnames()`
