# Gateway LoRa RX Only

**Version:** v1.0.0
**Date:** 2026-02-27
**Status:** Running
**Target:** Nucleo-L476RG + LR1121 shield

---

## Purpose

Standalone LoRa receiver for demonstration. Receives binary frame v1 packets from edge nodes, decodes and validates sensor data, and prints human-readable output on the debug UART.

- SIM7000E NB-IoT modem code fully removed (no USART1 dependency, no modem init delay)
- LR1121 calibration workaround applied (RC64k-only fallback — see known issues)
- Intended for live demos to stakeholders showing end-to-end LoRa link

---

## LoRa Parameters

| Parameter        | Value                | Notes                                      |
|------------------|----------------------|--------------------------------------------|
| Frequency        | 868 094 500 Hz       | Measured edge TX centre freq (SX1262 offset −5500 Hz from nominal 868.1 MHz) |
| Spreading Factor | SF12                 | Maximum range, lowest data rate             |
| Bandwidth        | 125 kHz              |                                            |
| Coding Rate      | 4/5                  |                                            |
| Preamble         | 8 symbols            |                                            |
| Header Mode      | Explicit             | Length encoded in header                    |
| CRC              | On                   |                                            |
| IQ               | Standard             | No inversion                               |
| Sync Word        | 0x12                 | Private network                            |
| Payload Length   | 16 bytes             | Binary frame v1 (FSD §6.1)                 |
| TX Power         | 13 dBm               | Not used (RX only), configured by radio init |
| Fallback Mode    | STDBY_RC             |                                            |
| RX Boost         | Disabled             |                                            |
| RX Mode          | RX_CONTINUOUS (0xFFFFFF) | Never times out                        |

---

## Frame Structure — Binary Frame v1 (16 bytes, little-endian)

| Offset | Width | Field    | Wire Type     | Scale | Physical Range        |
|--------|-------|----------|---------------|-------|-----------------------|
| 0x00   | 1     | VER      | uint8_t       | —     | 1 (current)           |
| 0x01   | 1     | NODE     | uint8_t       | —     | 1..255                |
| 0x02   | 1     | SEQ      | uint8_t       | —     | 0..2                  |
| 0x03   | 4     | TIME     | uint32_t LE   | —     | epoch >= 1700000000   |
| 0x07   | 1     | SM_PCT   | uint8_t       | —     | 0..100 %              |
| 0x08   | 2     | ST_C     | int16_t LE    | /10   | -55.0..125.0 C        |
| 0x0A   | 2     | AT_C     | int16_t LE    | /10   | -40.0..85.0 C         |
| 0x0C   | 2     | AH_PCT   | uint16_t LE   | /10   | 0.0..100.0 %RH        |
| 0x0E   | 2     | VB_V     | uint16_t LE   | /100  | 0.00..20.00 V         |

**Validation:** Frame is valid iff `len == 16` and `buf[0] == 1`. Field ranges checked by `payload_validate()`.

---

## LR1121 Settings

| Property            | Value               |
|---------------------|----------------------|
| Chip Type           | LR1121 (0x03)        |
| Hardware Version    | 0x22                 |
| Firmware Version    | 0x0102               |
| Latest FW Available | 0x0103               |
| Shield Variant      | LR1110MB1DIS (compatible pinout) |
| Regulator Mode      | From shield config (DC-DC) |
| TCXO                | Present (supply + startup from shield config) |
| RF Switch           | DIO-controlled (shield config) |
| LF Clock            | From shield config   |
| Calibration         | RC64k only (0x01) — full cal (0x3F) hangs BUSY |
| SPI                 | SPI1 (D11/D12/D13, NSS=D7) |
| BUSY Pin            | D3                   |
| RESET Pin           | A0                   |
| IRQ Pin             | D5 (DIO, rising edge) |

### Calibration Workaround

Full calibration (`0x3F`) causes BUSY pin to stay HIGH indefinitely — PLL cannot lock (suspected TCXO startup issue). The firmware performs:

1. Send `calibrate(0x3F)`, poll BUSY with 5-second timeout
2. On timeout: reset chip, re-initialize without TCXO, retry `calibrate(0x01)` (RC64k only)
3. PLL auto-calibrates implicitly when `lr11xx_radio_set_rf_freq()` is called during radio init

See `reports/lr1121-calibration-hang.md` for full root cause analysis.

---

## Serial Output (Debug UART)

- **Interface:** USART2 via ST-LINK virtual COM port
- **Baud:** 921 600
- **Format:** 8-N-1

### Boot Sequence (expected)

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

### On Packet Reception (expected)

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

---

## Files

| Path | Description |
|------|-------------|
| `src/main_demo.c` | Main application (inline system_init + calibration workaround) |
| `src/payload_parser.c` | Binary frame v1 decoder + validator |
| `src/payload_parser.h` | Parser public API |
| `src/main_p2p_receiver.h` | RX timeout macro + common includes |
| `src/apps_configuration.h` | RF parameters snapshot |
| `src/Makefile` | Build + flash targets |
| `bin/p2p_receiver_demo.bin` | Compiled binary (ready to flash) |
| `proof/` | Proof of operation (photos/screenshots) |

### Flash Command

```
STM32_Programmer_CLI.exe --connect port=SWD --download bin/p2p_receiver_demo.bin 0x08000000 --start
```

---

## Known Issues

| Issue | Impact | Mitigation |
|-------|--------|------------|
| Full calibration (0x3F) hangs | Blocks init without workaround | RC64k fallback + implicit PLL cal |
| LR1121 FW 0x0102 (not latest) | May contribute to cal issue | Update to 0x0103 when available |
| ADC calibration skipped | RSSI/SNR may have reduced accuracy | Acceptable for demo |
| RX boost disabled | Slightly lower sensitivity | Can enable if range is insufficient |

---

## Proof of Operation

> Photo to be provided — place in `proof/` directory.

---

## Version History

| Version | Date       | Changes |
|---------|------------|---------|
| v1.0.0  | 2026-02-27 | Initial release. LoRa RX only, no modem, calibration workaround. |
