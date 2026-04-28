# eNI Architecture

## Overview

eNI (Embedded Neural Interface) is a layered C11 framework for building brain-computer interfaces. It follows a strict separation of concerns with four main layers.

## System Layers

```
┌──────────────────────────────────────────────────────────────┐
│                      Application Layer                        │
│  CLI tools, GUI, SDKs (Python, Rust, Java, Node.js)          │
├──────────────────────────────────────────────────────────────┤
│                      Framework Layer                          │
│  Multi-channel DSP, ensemble decoding, adaptive feedback      │
│  Orchestrator, stream bus, service locator, health monitor    │
├──────────────────────────────────────────────────────────────┤
│                         Min Layer                             │
│  Single-channel DSP, single decoder, basic feedback           │
│  Lightweight pipeline for resource-constrained targets        │
├──────────────────────────────────────────────────────────────┤
│                       Common Layer                            │
│  Types, events, config, JSON, logging, DSP, NN, decoders     │
│  Data formats, sessions, calibration, profiles, recovery      │
├──────────────────────────────────────────────────────────────┤
│                    Platform Abstraction                        │
│  Threading, atomics, time, sleep — per-platform impl          │
└──────────────────────────────────────────────────────────────┘
```

## Key Design Patterns

### VTable OOP
All pluggable components (decoders, stimulators, providers) use C vtable pattern:
```c
typedef struct {
    const char  *name;
    eni_status_t (*init)(decoder_t *dec, const config_t *cfg);
    eni_status_t (*decode)(decoder_t *dec, const features_t *f, result_t *r);
    void         (*shutdown)(decoder_t *dec);
} decoder_ops_t;
```

### Service Locator
The framework layer uses service registration for loose coupling between components.

### Pipeline/Tick Pattern
Processing flows through a pipeline: `Provider → Signal Processor → Decoder → Feedback`

### Thread Safety
All framework-layer components are protected by platform-abstracted mutexes. The stream bus supports blocking pop with condition variables.

## Data Flow

```
Neural Device → Provider → Stream Bus → Signal Processor → Decoder → Intent Events
                                                                          ↓
                                                              Feedback Controller
                                                                          ↓
                                                              Stimulator Output
```

## Module Dependency Graph

```
eni_platform ← eni_common ← eni_min ← eni_framework
                    ↑                        ↑
              eni_providers            applications
```

## Threading Model

- **Main thread**: Session management, orchestrator tick loop
- **Provider threads**: One per data source (e.g., USB reader)
- **Recorder thread**: Background flush to disk
- **Player thread**: Playback at configurable speed

All shared state is protected by `eni_mutex_t`. The `eni_fw_stream_bus_t` uses mutex + condvar for producer-consumer blocking.

## Configuration

Supports both key=value INI files and JSON:

```json
{
    "variant": "framework",
    "mode": "features_intent",
    "confidence_threshold": 0.85,
    "dsp": {
        "epoch_size": 256,
        "sample_rate": 512,
        "artifact_threshold": 100.0
    },
    "decoder": {
        "model_path": "models/p300.bin",
        "num_classes": 4
    }
}
```
