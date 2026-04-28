# eNI — Product Overview

## What is eNI?

eNI (Embedded Neural Interface) is a comprehensive, open-source framework for building brain-computer interface (BCI) applications. It handles the complete pipeline from raw neural signal acquisition through decoded user intents to closed-loop neurofeedback — all in a single, zero-dependency C11 library.

## Who is it for?

- **BCI Researchers** — Record and analyze neural data with industry-standard formats (EDF+, BDF+, XDF)
- **Neurotechnology Companies** — Build production BCIs on a battle-tested, cross-platform foundation
- **Embedded Systems Engineers** — Deploy on resource-constrained devices with the lightweight Min layer
- **Application Developers** — Use Python, Rust, Java, or Node.js SDKs to build BCI-powered apps

## What does it do?

### Signal Acquisition
Connect to any neural device — EEG headsets, invasive electrode arrays, or simulated sources. Multi-device synchronization out of the box.

### Signal Processing
Real-time DSP with configurable filter chains (bandpass, notch, highpass, lowpass), artifact rejection (eye blinks, muscle, movement), and feature extraction (band power, spectral entropy, Hjorth parameters).

### Neural Decoding
Classify neural patterns into user intents using pluggable decoders. Supports ensemble decoding, model hot-swap without stopping, and confidence smoothing.

### Neurofeedback
Closed-loop stimulation with adaptive intensity modulation. Safety-checked with per-session limits, rate limiting, and emergency shutdown.

### Data Management
Record to EDF+, BDF+, XDF, or native ENI format. Replay recordings for offline analysis. Full annotation system with markers, epochs, and tags.

### Session Management
Complete session lifecycle with auto-calibration (impedance check → baseline → thresholds → validation), user profiles, and transfer learning across sessions.

## How is it different?

| Feature | eNI | BCI2000 | OpenBCI | BrainFlow | NeuroPype |
|---------|-----|---------|---------|-----------|-----------|
| Language | C11 | C++ | Python | C++ | Python |
| Dependencies | Zero | Many | Many | Moderate | Heavy |
| Embedded support | ✅ | ❌ | ❌ | ❌ | ❌ |
| Real-time safety | ✅ | Partial | ❌ | ❌ | ❌ |
| Thread safety | ✅ | Partial | ❌ | ✅ | ❌ |
| Cross-platform | ✅ | Windows | Linux | ✅ | ✅ |
| Data formats | 4 | 1 | 1 | 1 | 2 |
| Auto-calibration | ✅ | ❌ | ❌ | ❌ | ✅ |
| Neurofeedback | ✅ | ✅ | ❌ | ❌ | ✅ |
| Memory pools | ✅ | ❌ | ❌ | ❌ | ❌ |
| Health monitoring | ✅ | ❌ | ❌ | ❌ | ❌ |
