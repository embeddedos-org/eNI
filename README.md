# 🧠 ENI — Embedded Neural Interface

[![CI](https://github.com/embeddedos-org/eni/actions/workflows/ci.yml/badge.svg)](https://github.com/embeddedos-org/eni/actions/workflows/ci.yml)
[![Nightly](https://github.com/embeddedos-org/eni/actions/workflows/nightly.yml/badge.svg)](https://github.com/embeddedos-org/eni/actions/workflows/nightly.yml)
[![Release](https://github.com/embeddedos-org/eni/actions/workflows/release.yml/badge.svg)](https://github.com/embeddedos-org/eni/actions/workflows/release.yml)
[![Version](https://img.shields.io/github/v/tag/embeddedos-org/eni?label=version)](https://github.com/embeddedos-org/eni/releases/latest)

**Real-time neural, BCI, and assistive-input integration layer for EoS.**

ENI provides a standardized, vendor-neutral framework to integrate brain-computer interfaces (BCI), neural decoders, and assistive input systems into the EoS embedded OS platform.

---

## ⚡ Quick Start

```bash
git clone https://github.com/embeddedos-org/eni.git
cd eni

# Build (minimal + framework + neuralink)
cmake -B build -DENI_BUILD_TESTS=ON
cmake --build build

# Minimal only (MCU targets)
cmake -B build -DENI_BUILD_MIN=ON -DENI_BUILD_FRAMEWORK=OFF

# Without Neuralink
cmake -B build -DENI_PROVIDER_NEURALINK=OFF

# With EIPC integration
cmake -B build -DENI_EIPC_ENABLED=ON

# Cross-compile for ARM
cmake -B build-arm -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_SYSTEM_NAME=Linux
cmake --build build-arm
```

---

## 🏗 Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    ENI Neural Interface                        │
│                                                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐│
│  │   Neuralink   │  │  Simulator   │  │  Generic / Custom    ││
│  │  1024ch 30kHz │  │  Test data   │  │  Vendor-agnostic     ││
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────────────┘│
│         │                  │                  │                │
│  ┌──────▼──────────────────▼──────────────────▼───────────┐   │
│  │              Provider Framework                         │   │
│  │         eni_provider_ops_t (init/read/deinit)          │   │
│  └──────────────────────┬─────────────────────────────────┘   │
│                         │                                      │
│  ┌──────────────────────▼─────────────────────────────────┐   │
│  │              Common Layer                               │   │
│  │  Events · Policy · Config · Logging · EIPC Bridge       │   │
│  │  DSP · Neural Net · Decoder · Feedback · Stimulator     │   │
│  └──────────────────────┬─────────────────────────────────┘   │
│                         │                                      │
│  ┌─────────────┐  ┌────▼────────┐                             │
│  │  ENI-Min    │  │ ENI-Framework│                             │
│  │  (MCU)      │  │ (App Proc)   │                             │
│  │  filter     │  │ connectors   │                             │
│  │  normalizer │  │ pipeline     │                             │
│  │  mapper     │  │ orchestrator │                             │
│  │  tool bridge│  │              │                             │
│  └─────────────┘  └─────────────┘                             │
│         │                  │                                   │
│  ┌──────▼──────────────────▼──────────────────────────────┐   │
│  │              EIPC Bridge → EAI Agent                    │   │
│  │         Neural intents → AI tool calls                  │   │
│  └────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

---

## 🔧 Configurations

| Config | Option | Description |
|---|---|---|
| **Minimal** | `ENI_BUILD_MIN=ON` | Lightweight runtime for resource-constrained MCUs. Input normalization, signal filtering, event mapping, tool bridge. Minimal RAM footprint. |
| **Framework** | `ENI_BUILD_FRAMEWORK=ON` | Full industrial platform with connectors, pipeline orchestration, advanced signal processing. For application processors (RPi, i.MX8M, x86). |

Both configurations share the common library (events, providers, policy, DSP, neural net, decoder, feedback, EIPC bridge).

---

## 🧬 Neuralink Support

ENI includes a **Neuralink adapter** (`providers/neuralink/`) that provides:

- **1024-channel electrode support** at 30kHz sampling rate
- **4 operating modes**: raw data, decoded signals, intent classification, motor output
- **Signal processing**: bandpass filter (0.5–300Hz), 60Hz notch filter, baseline calibration
- **Intent decoder**: classifies neural energy into `idle`, `attention`, `motor_intent`, `motor_execute`
- **Calibration**: automatic baseline computation over configurable duration
- **Streaming API**: packet-based data acquisition with callback support

```c
#include "neuralink.h"

eni_nl_config_t cfg = {
    .mode = ENI_NL_MODE_INTENT,
    .channels = 256,
    .sample_rate = 30000,
    .filter_enabled = 1,
    .bandpass_low_hz = 0.5f,
    .bandpass_high_hz = 300.0f,
    .auto_calibrate = 1,
    .on_intent = my_intent_handler,
};

eni_neuralink_init(&cfg);
eni_neuralink_connect("neuralink-n1-001");
eni_neuralink_calibrate(5000);
eni_neuralink_start_stream();

while (running) {
    eni_nl_packet_t pkt;
    eni_neuralink_read_packet(&pkt);
    char intent[32];
    float confidence;
    eni_neuralink_decode_intent(&pkt, intent, sizeof(intent), &confidence);
}
```

---

## 🔌 Providers

| Provider | Description | Status |
|---|---|---|
| **neuralink** | Neuralink BCI adapter — 1024 channels, intent decoding, calibration | ✅ Implemented |
| **eeg** | EEG headset provider — multi-channel EEG with FFT band-power analysis | ✅ Implemented |
| **simulator** | BCI signal simulator for testing without hardware | ✅ Implemented |
| **stimulator_sim** | Stimulation output simulator with safety interlocks | ✅ Implemented |
| **generic** | Generic neural signal decoder — vendor-agnostic | ✅ Implemented |

New adapters can be added by implementing the `eni_provider_ops_t` interface:

```c
typedef struct {
    const char *name;
    int  (*init)(void *ctx);
    int  (*read)(void *ctx, void *buf, int len);
    void (*deinit)(void *ctx);
} eni_provider_ops_t;
```

---

## 🧪 Common Layer Services

| Module | Header | Description |
|---|---|---|
| **DSP** | `eni/dsp.h` | Digital signal processing — FIR/IIR filters, FFT, power spectral density |
| **Neural Net** | `eni/nn.h` | Lightweight neural network — dense layers, ReLU, softmax, inference |
| **Decoder** | `eni/decoder.h` | Intent decoder — feature extraction, classification, confidence scoring |
| **Feedback** | `eni/feedback.h` | Haptic/visual/audio neurofeedback — closed-loop BCI training |
| **Stimulator** | `eni/stimulator.h` | Electrical stimulation output — waveform generation, charge balancing |
| **Stim Safety** | `eni/stim_safety.h` | Stimulation safety — charge limits, impedance checks, emergency stop |
| **Events** | `eni/event.h` | Event system — neural events, state changes, error notifications |
| **Config** | `eni/config.h` | Configuration management — profiles, runtime tuning |

---

## 📂 Repository Structure

```
eni/
├── common/                   # Shared library
│   ├── include/eni/          #   Types, events, config, DSP, NN, decoder, feedback
│   └── src/                  #   Implementation
├── min/                      # Minimal runtime (MCU targets)
│   ├── include/eni_min/      #   Signal processor, decoder, feedback, service
│   └── src/                  #   Lightweight implementations
├── framework/                # Full framework (app processors)
│   ├── include/eni_fw/       #   Signal processor, decoder, feedback
│   └── src/                  #   Advanced implementations
├── providers/                # Hardware adapters
│   ├── neuralink/            #   Neuralink 1024-channel BCI
│   ├── eeg/                  #   EEG headset provider
│   ├── simulator/            #   Signal simulator
│   └── stimulator_sim/       #   Stimulation simulator
├── cli/                      # CLI tool (main.c)
├── tests/                    # Unit tests (7 test suites)
└── CMakeLists.txt            # Build configuration
```

---

## 🧪 Tests

```bash
cmake -B build -DENI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| Test | Covers |
|---|---|
| `test_dsp` | FIR/IIR filters, FFT, spectral analysis |
| `test_nn` | Dense layers, activations, forward pass |
| `test_decoder` | Feature extraction, intent classification |
| `test_provider_eeg` | EEG provider init, channel read, band power |
| `test_stimulator` | Waveform generation, charge balancing |
| `test_stim_safety` | Charge limits, impedance checks, emergency stop |
| `test_bci_pipeline` | End-to-end BCI pipeline (acquire → process → classify → act) |
| `test_event_feedback` | Event dispatch, feedback loop |

---

## 🚀 CI/CD

| Workflow | Schedule | Coverage |
|----------|----------|----------|
| **CI** | Every push/PR | Build matrix (Linux × Windows × macOS) + tests |
| **Nightly** | 2:00 AM UTC daily | Full build + test + cross-compile + regression report |
| **Weekly** | Monday 6:00 AM UTC | Comprehensive build + dependency audit |
| **EoSim Sanity** | 4:00 AM UTC daily | EoSim install validation (3 OS × 3 Python) + simulation |
| **Simulation Test** | 3:00 AM UTC daily | QEMU/EoSim platform simulation |
| **Release** | Tag `v*.*.*` | Validate → cross-compile → GitHub Release with artifacts |

---

## Related Projects

| Project | Repository | Purpose |
|---|---|---|
| **eos** | [embeddedos-org/eos](https://github.com/embeddedos-org/eos) | Embedded OS — HAL, RTOS kernel, services |
| **eai** | [embeddedos-org/eai](https://github.com/embeddedos-org/eai) | AI layer — receives ENI intents via EIPC |
| **eipc** | [embeddedos-org/eipc](https://github.com/embeddedos-org/eipc) | IPC transport between ENI and EAI |
| **eboot** | [embeddedos-org/eboot](https://github.com/embeddedos-org/eboot) | Bootloader — secure boot, A/B slots |
| **ebuild** | [embeddedos-org/ebuild](https://github.com/embeddedos-org/ebuild) | Build system — SDK generator, packaging |
| **eosim** | [embeddedos-org/eosim](https://github.com/embeddedos-org/eosim) | Multi-architecture simulator |

## Standards Compliance

This project is part of the EoS ecosystem and aligns with international standards including ISO/IEC/IEEE 15288:2023, ISO/IEC 12207, ISO/IEC/IEEE 42010, ISO/IEC 25000, ISO/IEC 25010, ISO/IEC 27001, ISO/IEC 15408, IEC 61508, ISO 26262, DO-178C, FIPS 140-3, POSIX (IEEE 1003), WCAG 2.1, and more. See the [EoS Compliance Documentation](https://github.com/embeddedos-org/.github/tree/master/docs/compliance) for full details.

## 📜 License

MIT License — see [LICENSE](LICENSE) for details.
