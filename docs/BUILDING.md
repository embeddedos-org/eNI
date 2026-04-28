# Building eNI

## Supported Platforms

| Platform | Compiler | Status |
|----------|----------|--------|
| Linux x86_64 | GCC 7+, Clang 6+ | ✅ Full support |
| Linux ARM64 | GCC 7+ | ✅ Full support |
| macOS x86_64 | Clang 12+ | ✅ Full support |
| macOS ARM64 (Apple Silicon) | Clang 12+ | ✅ Full support |
| Windows x64 | MSVC 2019+ | ✅ Full support |
| FreeBSD/OpenBSD | Clang | ✅ POSIX fallback |
| EoS (embedded) | ARM GCC | ✅ Min layer |

## Prerequisites

- **CMake** 3.16 or newer
- **C11 compiler** (GCC 7+, Clang 6+, MSVC 2019+)
- **POSIX threads** (automatic on Linux/macOS; Windows uses native threading)

### Optional

- **Python 3.8+** — for Python bindings
- **Rust 1.56+** — for Rust bindings
- **JDK 11+** — for Java/Android bindings
- **Node.js 16+** — for Node.js SDK

## Build Commands

### Minimal Build

```bash
cmake -B build
cmake --build build
```

### Full Build (All Features)

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DENI_BUILD_DSP=ON \
    -DENI_BUILD_NN=ON \
    -DENI_BUILD_DECODER=ON \
    -DENI_BUILD_STIMULATOR=ON \
    -DENI_BUILD_DATA_FORMATS=ON \
    -DENI_BUILD_SESSION=ON \
    -DENI_BUILD_TESTS=ON \
    -DENI_PROVIDER_EEG=ON \
    -DENI_PROVIDER_STIMULATOR_SIM=ON

cmake --build build -j$(nproc)
```

### Debug Build with Sanitizers

```bash
cmake -B build-debug \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
    -DENI_BUILD_TESTS=ON \
    -DENI_BUILD_DSP=ON
cmake --build build-debug
```

### Using Presets

```bash
cmake --preset linux-debug -DENI_BUILD_TESTS=ON
cmake --build build/linux-debug
```

## Running Tests

```bash
# All tests
ctest --test-dir build --output-on-failure

# Unit tests only
ctest --test-dir build -L unit

# Integration tests only
ctest --test-dir build -L integration

# Specific test
ctest --test-dir build -R eni_json_tests
```

## CMake Options

### Feature Flags

| Option | Default | Description |
|--------|---------|-------------|
| `ENI_BUILD_DSP` | OFF | Digital signal processing |
| `ENI_BUILD_NN` | OFF | Neural network inference |
| `ENI_BUILD_DECODER` | OFF | Neural decoders |
| `ENI_BUILD_STIMULATOR` | OFF | Stimulation & feedback |
| `ENI_BUILD_DATA_FORMATS` | OFF | EDF+/BDF+/XDF/ENI I/O |
| `ENI_BUILD_SESSION` | OFF | Session/calibration/profiles |
| `ENI_BUILD_TESTS` | OFF | Test suite |

### Platform Selection

| Option | Default | Description |
|--------|---------|-------------|
| `ENI_PLATFORM_LINUX` | auto | Force Linux platform |
| `ENI_PLATFORM_MACOS` | auto | Force macOS platform |
| `ENI_PLATFORM_WINDOWS` | auto | Force Windows platform |
| `ENI_PLATFORM_EOS` | OFF | Embedded EoS platform |

### Provider Selection

| Option | Default | Description |
|--------|---------|-------------|
| `ENI_PROVIDER_EEG` | OFF | Non-invasive EEG provider |
| `ENI_PROVIDER_STIMULATOR_SIM` | OFF | Simulated stimulator |

## Cross-Compilation

### ARM64 Linux

```bash
cmake -B build-arm64 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux.cmake \
    -DENI_PLATFORM_LINUX=ON
cmake --build build-arm64
```

### Embedded (EoS)

```bash
cmake -B build-eos \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
    -DENI_PLATFORM_EOS=ON
cmake --build build-eos
```

## Installation

```bash
cmake --install build --prefix /usr/local
```

This installs:
- Headers to `<prefix>/include/`
- Libraries to `<prefix>/lib/`
