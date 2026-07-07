# ATtiny85 TX/RX Sequencer for HF LDMOS Power Amplifier

ATtiny85-based TX/RX sequencer for a 1KW HF LDMOS solid-state power amplifier. Provides safe relay sequencing, overdrive protection, and hardware watchdog supervision.

## Features

- Correct TX/RX relay sequencing to protect both the LDMOS transistor and the transceiver
- Overdrive input protection: blocks bias if input power exceeds threshold
- Hardware watchdog (250ms): returns to safe RX state if firmware hangs
- Fail-safe design: all relays return to bypass (RX) on power loss or STBY mode

## Hardware Integration

This sequencer is designed to work with the following boards:

- **SP6GK TX-RX Relay Board** — handles RF switching (RM85 relays), input SWR bridge, and overpower detection ([GitHub](https://github.com/SP6GK/TXRX_Relay_Board))
- **S21RC LDMOS SSPA Controller V2.1 Lite** — manages LDMOS bias, display, and secondary protections ([GitHub](https://github.com/s21rc/ldmos_sspa_controller_v2.1_lite))
- **DXWorld-e Protection Board** — hardware overcurrent, overvoltage, and VSWR protection via BTS50085 (30µs cutoff)

## Pin Mapping

| Pin | Arduino | Direction | Function |
|-----|---------|-----------|----------|
| PB0 | D0 | INPUT | OVERDRIVE — HIGH_PWR_IN from SP6GK board (active HIGH) |
| PB1 | D1 | OUTPUT | RF_RELAY_IN — TRX input relay via BC547→SP6GK PNP (HIGH=active) |
| PB2 | D2 | OUTPUT | PTT_OUT — PTT to S21RC controller (HIGH=TX, 12V) |
| PB3 | D3 | INPUT | PTT_IN — Operator PTT, active LOW (internal pull-up) |
| PB4 | D4 | OUTPUT | RF_RELAY_OUT — ANT output relay via BC547→SP6GK PNP (HIGH=active) |
| PB5 | — | — | RESET — do not use |

## TX Sequence

```
1. ANT output relay → TX     (no RF load, bias off)
2. Wait 10ms
3. TRX input relay  → TX
4. Wait 10ms
5. PTT_OUT → HIGH            (S21RC activates bias, keys transceiver)
6. Wait 15ms                 (bias stabilization margin)
→ RF active
```

## RX Sequence

```
1. PTT_OUT → LOW             (S21RC cuts bias, stops transceiver)
2. Wait 8ms                  (RF decay)
3. TRX input relay  → RX
4. Wait 10ms
5. ANT output relay → RX
```

## Overdrive Protection

The SP6GK board includes an overpower detector with adjustable threshold (via potentiometer) that drives the HIGH_PWR_IN signal HIGH when input power exceeds the set level.

When overdrive is detected during TX:

1. PTT_OUT is pulled LOW immediately
2. S21RC interprets this as end of TX and cuts LDMOS bias
3. RF relays remain in TX position (operator still holds PTT)
4. **Bias is not restored until the operator releases PTT** (manual reset required)

## STBY/OPER Switch

A normally-closed switch in series with PTT_IN provides STBY mode entirely in hardware:

- **OPER**: switch closed → normal operation
- **STBY**: switch open → PTT_IN never goes LOW → no TX sequence, bias never activated
- Relay coil power is also cut in STBY → RM85 relays return to NC (bypass) → antenna connected directly to transceiver

This is a fully fail-safe design: cable break or switch failure defaults to STBY/bypass.

## Bias Relay

The LDMOS gate bias line is switched by a dedicated relay controlled by the S21RC controller. The relay coil current is negligible (LDMOS gate is capacitive, leakage in µA range). A small signal relay (SRD-12VDC, HK19F, etc.) is sufficient.

The relay driver uses a BC547 NPN transistor in common-emitter configuration driven by PTT_OUT via the S21RC controller output, providing full galvanic isolation between the control circuitry and the LDMOS gate bias network.

## Watchdog

The ATtiny85 internal watchdog is enabled with a 250ms timeout. If the firmware hangs for any reason, the micro resets and `setup()` immediately drives all outputs to the safe RX state:

```
RF_RELAY_OUT → LOW  (ANT relay to bypass)
RF_RELAY_IN  → LOW  (TRX relay to bypass)
PTT_OUT      → LOW  (S21RC in RX, bias off)
```

## Requirements

- **Arduino IDE** with ATtiny85 board support ([ATTinyCore](https://github.com/SpenceKonde/ATTinyCore) recommended)
- **Clock**: 8MHz internal oscillator (no external crystal needed)
- **Fuses**: default — do not burn RSTDISBL (PB5/RESET must remain as reset pin)

## Programming

Use any ISP programmer (USBasp, Arduino as ISP) via the SPI header on PB0/PB1/PB2/PB5.

Board settings in Arduino IDE:
```
Board:     ATtiny85
Clock:     8 MHz (internal)
Programmer: USBasp (or Arduino as ISP)
```

## License

MIT License — feel free to use and modify for your own amplifier builds.
