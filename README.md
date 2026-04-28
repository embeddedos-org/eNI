# eNI — Embedded Neural Interface Framework

> **World-class, cross-platform, production-ready neural interface framework in C11**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-C11-brightgreen.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

eNI is a modular, real-time neural interface framework designed for brain-computer interface (BCI) applications. It provides everything needed to build production-grade neural interfaces — from raw signal acquisition to decoded intents and closed-loop neurofeedback.

## Key Features

- **Multi-channel signal processing** — Up to 256 channels with pluggable filter chains, artifact rejection, and full DSP pipeline
- **Neural decoding** — Ensemble decoders with model hot-swap, confidence smoothing, and temporal consistency
- **Closed-loop neurofeedback** — Adaptive feedback with safety-checked stimulation and intensity modulation
- **Neural data formats** — Read/write EDF+, BDF+, XDF, and native ENI format; annotation system
- **Session management** — Full lifecycle state machine with calibration pipeline and user profiles
- **Thread-safe architecture** — Platform-abstracted mutexes, condition variables, atomics, and threads
- **Cross-platform** — Linux, macOS, Windows, POSIX, EoS (embedded)
- **Zero external dependencies** — Pure C11 core; optional integrations gated by CMake
- **Production hardening** — Health monitoring, watchdog, memory pools, error recovery with exponential backoff
- **Multi-language SDKs** — C (native), Python, Rust, Java/Android, Node.js

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Applications / SDKs                   │
│  Python │ Rust │ Java │ Node.js │ CLI │ GUI              │
├─────────────────────────────────────────────────────────┤
│                   ENI Framework Layer                    │
│  Signal Processor │ Decoder │ Feedback │ Stream Bus      │
│  Orchestrator │ Router │ Service │ Health Monitor        │
├─────────────────────────────────────────────────────────┤
│                     ENI Min Layer                        │
│  Filter │ Normalizer │ Mapper │ Decoder │ Feedback       │
├─────────────────────────────────────────────────────────┤
│                    Common Libraries                      │
│  DSP │ NN │ Events │ Config │ JSON │ Logging │ Policy    │
│  Data Formats │ Session │ Calibration │ Recovery │ Pool  │
├─────────────────────────────────────────────────────────┤
│                   Platform Abstraction                   │
│  Linux │ macOS │ Windows │ POSIX │ EoS (embedded)       │
│  Threading │ Atomics │ Time │ Sleep                      │
└─────────────────────────────────────────────────────────┘
```

## Quick Start

### Building

```bash
# Linux
cmake -B build -DENI_BUILD_DSP=ON -DENI_BUILD_DECODER=ON \
      -DENI_BUILD_STIMULATOR=ON -DENI_BUILD_DATA_FORMATS=ON \
      -DENI_BUILD_SESSION=ON -DENI_BUILD_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# macOS
cmake -B build -DENI_PLATFORM_MACOS=ON ...

# Windows
cmake -B build -DENI_PLATFORM_WINDOWS=ON ...
```

### Minimal C Example

```c
#include "eni/common.h"
#include "eni/session.h"
#include "eni_fw/signal_processor.h"
#include "eni_fw/decoder.h"

int main(void) {
    // Initialize session
    eni_session_t session;
    eni_session_init(&session);
    eni_session_set_subject(&session, "user-001");
    eni_session_start(&session);

    // Set up signal processor (8 channels, 256-sample epochs, 512 Hz)
    eni_fw_signal_processor_t sp;
    eni_fw_signal_processor_init(&sp, 8, 256, 512, 100.0f);

    // Add bandpass filter (1-40 Hz)
    eni_fw_filter_config_t filter = {
        .type = ENI_FW_FILTER_BANDPASS,
        .low_hz = 1.0f,
        .high_hz = 40.0f,
    };
    eni_fw_signal_processor_add_filter(&sp, &filter);

    // Process samples and extract features...
    // ...

    eni_fw_signal_processor_shutdown(&sp);
    eni_session_stop(&session);
    eni_session_destroy(&session);
    return 0;
}
```

### Python Example

```python
from eni.data_formats import read_edf, write_edf

# Read an EDF+ file
header, records, annotations = read_edf("recording.edf")
print(f"Channels: {header.num_channels}, Records: {header.num_records}")

# Process data...
```

## Project Structure

```
eNI/
├── common/          # Shared libraries (DSP, NN, events, config, data formats)
├── framework/       # Full framework (signal processor, decoder, feedback, health)
├── min/             # Minimal layer (lightweight BCI pipeline)
├── platform/        # Platform abstraction (Linux, macOS, Windows, POSIX, EoS)
├── providers/       # Data providers (simulator, EEG, LSL)
├── bindings/        # Language bindings (Python, Rust, Java)
├── sdk/             # SDKs (Node.js)
├── tests/           # Comprehensive test suite
├── docs/            # Documentation
├── cli/             # Command-line tools
├── gui/             # GUI applications
└── tools/           # Development tools
```

## Supported Data Formats

| Format | Read | Write | Description |
|--------|------|-------|-------------|
| EDF+   | ✅   | ✅    | European Data Format — standard for EEG/PSG |
| BDF+   | ✅   | ✅    | BioSemi 24-bit format |
| XDF    | ✅   | ✅    | Extensible Data Format — multi-stream with clock sync |
| ENI    | ✅   | ✅    | Native format — optimized for real-time streaming |

## Calibration Pipeline

eNI includes a 4-stage auto-calibration pipeline:

1. **Impedance Check** — Verify electrode contact quality
2. **Baseline Recording** — 30-second resting state (eyes open/closed)
3. **Threshold Computation** — Percentile-based per-channel thresholds
4. **Validation** — Accuracy test with real-time classification

## License

MIT License — see [LICENSE](LICENSE) for details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
