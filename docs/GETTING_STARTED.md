# Getting Started with eNI

## Prerequisites

- C11 compiler (GCC 7+, Clang 6+, MSVC 2019+)
- CMake 3.16+
- POSIX threads (Linux/macOS) or Windows SDK

## Building from Source

### Linux

```bash
git clone <repo> eNI && cd eNI

# Full build with all features
cmake -B build \
    -DENI_BUILD_DSP=ON \
    -DENI_BUILD_NN=ON \
    -DENI_BUILD_DECODER=ON \
    -DENI_BUILD_STIMULATOR=ON \
    -DENI_BUILD_DATA_FORMATS=ON \
    -DENI_BUILD_SESSION=ON \
    -DENI_BUILD_TESTS=ON

cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### macOS

```bash
cmake -B build -DENI_PLATFORM_MACOS=ON \
    -DENI_BUILD_DSP=ON -DENI_BUILD_TESTS=ON
cmake --build build
```

### Windows

```powershell
cmake -B build -DENI_PLATFORM_WINDOWS=ON `
    -DENI_BUILD_DSP=ON -DENI_BUILD_TESTS=ON
cmake --build build --config Release
```

## Your First BCI Application

### 1. Initialize the system

```c
#include "eni/common.h"
#include "eni/session.h"
#include "eni_platform/platform.h"

eni_platform_init();

eni_config_t config;
eni_config_load_file(&config, "my_config.json");

eni_session_t session;
eni_session_init(&session);
eni_session_set_subject(&session, "participant-001");
```

### 2. Set up signal processing

```c
#include "eni_fw/signal_processor.h"

eni_fw_signal_processor_t sp;
eni_fw_signal_processor_init(&sp, 8, 256, 512, 100.0f);

// Add 50 Hz notch filter (power line noise)
eni_fw_filter_config_t notch = {
    .type = ENI_FW_FILTER_NOTCH,
    .low_hz = 50.0f,
};
eni_fw_signal_processor_add_filter(&sp, &notch);

// Add 1-40 Hz bandpass
eni_fw_filter_config_t bp = {
    .type = ENI_FW_FILTER_BANDPASS,
    .low_hz = 1.0f,
    .high_hz = 40.0f,
};
eni_fw_signal_processor_add_filter(&sp, &bp);
```

### 3. Run calibration

```c
#include "eni/calibration.h"

eni_calibration_t cal;
eni_calibration_init(&cal, 8, 512.0f);
eni_calibration_start_impedance(&cal);
// ... collect impedance data ...
eni_calibration_start_baseline(&cal);
// ... collect 30s resting data ...
eni_calibration_finalize_baseline(&cal);
eni_calibration_compute_thresholds(&cal, 0.95f);
```

### 4. Record data

```c
#include "eni/recorder.h"
#include "eni/edf.h"

eni_data_header_t hdr;
eni_data_header_init(&hdr);
hdr.num_channels = 8;
hdr.record_duration = 1.0;
// ... set channel info ...

eni_recorder_t rec;
eni_recorder_init(&rec, ENI_FORMAT_EDF, &hdr);
eni_recorder_start(&rec, "session_001.edf");
// ... push samples during acquisition ...
eni_recorder_stop(&rec);
```

### 5. Clean up

```c
eni_fw_signal_processor_shutdown(&sp);
eni_session_stop(&session);
eni_session_destroy(&session);
eni_calibration_destroy(&cal);
eni_recorder_destroy(&rec);
```

## CMake Options Reference

| Option | Default | Description |
|--------|---------|-------------|
| `ENI_BUILD_DSP` | OFF | Enable DSP signal processing |
| `ENI_BUILD_NN` | OFF | Enable neural network inference |
| `ENI_BUILD_DECODER` | OFF | Enable neural decoder |
| `ENI_BUILD_STIMULATOR` | OFF | Enable stimulation/feedback |
| `ENI_BUILD_DATA_FORMATS` | OFF | Enable EDF+/BDF+/XDF/ENI format I/O |
| `ENI_BUILD_SESSION` | OFF | Enable session/calibration/profile |
| `ENI_BUILD_TESTS` | OFF | Build test suite |
| `ENI_PLATFORM_LINUX` | auto | Force Linux platform |
| `ENI_PLATFORM_MACOS` | auto | Force macOS platform |
| `ENI_PLATFORM_WINDOWS` | auto | Force Windows platform |
| `ENI_PLATFORM_EOS` | OFF | Build for EoS embedded platform |

## Next Steps

- Read [ARCHITECTURE.md](ARCHITECTURE.md) for system design details
- Explore `tests/` for usage examples
- Check `bindings/` for Python, Rust, and Java wrappers
