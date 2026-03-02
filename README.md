# Gateway LoRa RX Only

Frozen firmware build for the **Nucleo-L476RG + Semtech LR1121 shield** gateway receiver. Listens for 16-byte binary LoRa packets from edge nodes, decodes and validates sensor data, and prints human-readable output on the debug UART.

Part of the [IoT Soil & Ambient Monitoring System](https://github.com/ounguder/iot-system-et).

## What This Firmware Does

1. **Boot** — initialises LR1121 radio with calibration workaround (RC64k fallback)
2. **Listen** — enters RX_CONTINUOUS mode on 868.094500 MHz (SF12/125kHz)
3. **Decode** — parses received 16-byte binary frame v1 (little-endian)
4. **Validate** — checks protocol version, frame length, and field ranges
5. **Print** — outputs human-readable sensor readings on USART2 (921600 baud)

This is a **demo build** — SIM7000E NB-IoT modem code has been fully stripped out. No MQTT, no cellular. Pure LoRa RX + serial display.

## LoRa Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 868 094 500 Hz (tuned to measured edge TX centre freq) |
| Spreading Factor | SF12 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Preamble | 8 symbols |
| CRC | On |
| Sync Word | 0x12 (private) |
| Payload | 16 bytes |
| RX Mode | RX_CONTINUOUS (never times out) |

## Binary Frame v1 (16 bytes, little-endian)

```
Offset  Width  Field     Type          Scale   Range
0x00    1      VER       uint8         —       1
0x01    1      NODE      uint8         —       1..255
0x02    1      SEQ       uint8         —       0..2
0x03    4      TIME      uint32 LE     —       epoch >= 1700000000
0x07    1      SM_PCT    uint8         —       0..100 %
0x08    2      ST_C      int16 LE      /10     -55.0..125.0 °C
0x0A    2      AT_C      int16 LE      /10     -40.0..85.0 °C
0x0C    2      AH_PCT    uint16 LE     /10     0.0..100.0 %RH
0x0E    2      VB_V      uint16 LE     /100    0.00..20.00 V
```

Frame is valid iff `len == 16` and `buf[0] == 1`. Field ranges checked by `payload_validate()`.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | STM32L476RG (Nucleo-64 board) |
| Radio | Semtech LR1121 evaluation shield (LR1110MB1DIS compatible pinout) |
| LR1121 FW | 0x0102 (HW: 0x22, Type: 0x03) |
| SPI | SPI1 — D11(MOSI) D12(MISO) D13(SCK) D7(NSS) |
| BUSY | D3 |
| RESET | A0 |
| IRQ | D5 (DIO, rising edge) |
| Debug UART | USART2 via ST-LINK virtual COM, 921600 baud, 8-N-1 |

## Repository Structure

```
├── README.md                   This file
├── FIRMWARE.md                 Detailed firmware documentation (v1.0.1)
├── src/
│   ├── main_demo.c             Main application (inline init + calibration workaround)
│   ├── payload_parser.c        Binary frame v1 decoder + validator
│   ├── payload_parser.h        Parser public API
│   ├── main_p2p_receiver.h     RX timeout macro + common includes
│   ├── apps_configuration.h    RF parameters snapshot
│   └── Makefile                Build + flash targets
├── bin/
│   └── p2p_receiver_demo.bin   Pre-compiled binary (ready to flash)
└── proof/
    └── *.png                   Proof-of-operation screenshots
```

## Quick Start

### Option A: Flash the pre-built binary

```
STM32_Programmer_CLI.exe --connect port=SWD --download bin/p2p_receiver_demo.bin 0x08000000 --start
```

Or use STM32CubeProgrammer GUI: connect via SWD, load `bin/p2p_receiver_demo.bin` at address `0x08000000`, program, and start.

### Option B: Build from source

**Prerequisites:**
- ARM GCC toolchain (`arm-none-eabi-gcc`)
- GNU Make
- The Semtech SWSD003 SDK (this repo contains only the project-specific source files)
- STM32CubeProgrammer for flashing

**Build:**

This firmware was built within the Semtech SWSD003 SDK tree. The source files in `src/` slot into:

```
SWSD003/lr11xx/apps/p2p_receiver/demo/
```

```bash
cd <SWSD003>/lr11xx/apps/p2p_receiver/demo
make clean && make
make flash   # requires ST-LINK connected
```

## LR1121 Calibration Fix (v1.0.1)

The v1.0.0 release had a calibration hang because the Makefile did not set `RADIO_SHIELD`. The Semtech SDK defaulted to `LR1110MB1DIS` (has TCXO), but the LR1121 shield has **no TCXO**. This caused `calibrate(0x3F)` to hang indefinitely waiting for a non-existent TCXO.

**Fix:** `RADIO_SHIELD = LR1121MB1DIS` added to the Makefile. Full calibration now succeeds in ~100 ms.

The inline safety net (5s BUSY timeout + RC64k fallback) in `main_demo.c` is retained but no longer triggered.

## Expected Serial Output

### Boot

```
===== LR11xx LoRa P2P Receiver [DEMO] =====
  Modem: DISABLED (demo build)
[DBG] shield_init... OK
SDK version: v2.4.0
LR11XX driver version: v2.5.2
[INIT] 1/9 through 9/9 ... (calibration with RC64k fallback)
Common parameters:
   Packet type   = LR11XX_RADIO_PKT_TYPE_LORA
   RF frequency  = 868094500 Hz
   Output power  = 13 dBm
LoRa modulation parameters:
   Spreading factor = LR11XX_RADIO_LORA_SF12
   Bandwidth        = LR11XX_RADIO_LORA_BW_125
   Coding rate      = LR11XX_RADIO_LORA_CR_4_5
--- Listening (RX_CONTINUOUS) ---
```

### On Packet Reception

```
========== RECEIVED FRAME ==========
  Node ID:             1
  Sequence:            0
  Timestamp (UTC):     1740700000
  Soil Moisture:       42%
  Soil Temperature:    18.50 C
  Air Temperature:     22.30 C
  Air Humidity:        55.20 %RH
  Battery Voltage:     3.72 V
  Status:              VALID
====================================
```

## Companion: Edge Node Transmitter

This gateway receives packets from **Heltec WiFi LoRa 32 V3** edge nodes.
See [edge_node_measure_and_tx](https://github.com/ounguder/-edge_node_measure_and_tx) for the edge node firmware.

## Known Issues

| Issue | Impact | Status |
|-------|--------|--------|
| ~~Full calibration (0x3F) hangs~~ | ~~Blocks init~~ | **FIXED v1.0.1** |
| LR1121 FW 0x0102 (not latest 0x0103) | Minor | Update when available |
| RX boost disabled | Slightly lower sensitivity | Can enable if range is insufficient |

## Version

**v1.0.1** — 2026-03-02

See [FIRMWARE.md](FIRMWARE.md) for full technical documentation including LR1121 settings, calibration details, and known issues.

## License

This is a frozen firmware snapshot for academic/demonstration purposes.
