# eNI — Neural Interface Adapter

[![CI](https://github.com/embeddedos-org/eNI/actions/workflows/ci.yml/badge.svg)](https://github.com/embeddedos-org/eNI/actions/workflows/ci.yml)
[![CodeQL](https://github.com/embeddedos-org/eNI/actions/workflows/codeql.yml/badge.svg)](https://github.com/embeddedos-org/eNI/actions/workflows/codeql.yml)
[![Scorecard](https://github.com/embeddedos-org/eNI/actions/workflows/scorecard.yml/badge.svg)](https://github.com/embeddedos-org/eNI/actions/workflows/scorecard.yml)
[![Release](https://github.com/embeddedos-org/eNI/actions/workflows/release.yml/badge.svg)](https://github.com/embeddedos-org/eNI/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

eNI (CMake project `ENI`) is the neural-interface adapter of the EmbeddedOS
ecosystem, written in C. It integrates brain-computer-interface (BCI) and
biosignal/sensor input for the EoS AI layer: acquiring data from BCI hardware and
simulators through a provider model, running it through DSP, a neural decoder,
and an on-device neural-network inference engine, and driving a
stimulator/feedback path. It reads and writes neural data formats (EDF+, BDF+,
XDF, and a native ENI format) and includes session management and calibration.

eNI is part of the **EmbeddedOS**
([embeddedos-org](https://github.com/embeddedos-org)) ecosystem, alongside
[EoS](https://github.com/embeddedos-org/eos) (OS core),
[eBoot](https://github.com/embeddedos-org/eBoot) (secure bootloader), and
[eAI](https://github.com/embeddedos-org/eAI) (embedded AI). Project version 0.3.0.

## What's inside

| Path | Contents |
|------|----------|
| `providers/` | Input providers: `eeg`, `lsl` (Lab Streaming Layer), `neuralink`, `simulator`, `wireless`, `stimulator_sim`, `generic`, `template` |
| `common/` | Core signal path: `dsp`, `nn` (neural network), `feedback`, `tinyml`, `math`, `hal` |
| `min/` | ENI-Min lightweight runtime |
| `framework/` | ENI-Framework platform |
| `platform/` | Platform adapters: `linux`, `macos`, `windows`, `posix`, `eos` |
| `cli/` | `eni` command-line tool |
| `integrations/` | Optional `ros2`, `oni` (ONI compliance), `eipc_streaming` |
| `models/` | Model assets and metadata |
| `bindings/`, `sdk/` | Language bindings (C++, Java, Python, Rust) and SDKs (Node.js, Python) |
| `sim/` | Simulation environment |
| `gui/` | Front-end (`package.json` + `src/`, `backend/`) |
| `eni/`, `network/` | Python package (`core.py`) and an IP-based geolocation helper (`ip_geolocator.py`) used as a location fallback when GPS lock is lost |
| `tests/` | Unit, functional, performance, and simulation tests |

## Build

Requires CMake ≥ 3.16 and a C compiler. `CMakePresets.json` defines the presets
used by CI.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Selected build options

| Option | Default | Meaning |
|--------|---------|---------|
| `ENI_BUILD_MIN` | `ON` | Build the ENI-Min runtime |
| `ENI_BUILD_FRAMEWORK` | `ON` | Build the ENI-Framework platform |
| `ENI_BUILD_CLI` | `ON` | Build the CLI |
| `ENI_BUILD_DSP` | `ON` | Build the DSP signal-processing module |
| `ENI_BUILD_DECODER` | `ON` | Build the neural decoder |
| `ENI_BUILD_NN` | `ON` | Build the neural-network inference engine |
| `ENI_BUILD_STIMULATOR` | `ON` | Build the stimulator/feedback subsystem |
| `ENI_BUILD_DATA_FORMATS` | `ON` | Build neural data-format I/O (EDF+/BDF+/XDF/ENI) |
| `ENI_BUILD_SESSION` | `ON` | Build session management and calibration |
| `ENI_BUILD_TESTS` | `OFF` | Build unit tests |
| `ENI_ONNX_ENABLED` / `ENI_BUILD_TFLITE` | `OFF` | Enable ONNX / TFLite Micro model support |
| `ENI_BUILD_ROS2` / `ENI_BUILD_ONI` | `OFF` | Build ROS 2 / ONI integrations |
| `ENI_WIRELESS_ENABLED` | `OFF` | Enable the wireless BCI provider |

The build also wires in sanitizer, coverage, static-analysis, and memcheck
helpers from `cmake/`.

## Test

```bash
cmake -B build -DENI_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Python-driven suites
python run_all_tests.py
```

## Docs

See [`docs/`](docs/): `ARCHITECTURE.md`, `BUILDING.md`, `GETTING_STARTED.md`,
`PRODUCT_OVERVIEW.md`, and `tutorials/`.

## License

Licensed under the [MIT License](LICENSE).
