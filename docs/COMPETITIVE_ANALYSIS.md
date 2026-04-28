# eNI Competitive Analysis

## Feature Comparison Matrix

| Feature | eNI | BCI2000 | OpenBCI | BrainFlow | NeuroPype | MNE-Python |
|---------|-----|---------|---------|-----------|-----------|------------|
| **Core Language** | C11 | C++ | Python | C++/Python | Python | Python |
| **External Dependencies** | Zero | Borland VCL | Many | Moderate | Heavy | Heavy |
| **Embedded Support** | ✅ Native | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Real-time Capable** | ✅ | ✅ | ❌ | Partial | ❌ | ❌ |
| **Thread Safety** | ✅ Full | Partial | ❌ | ✅ | ❌ | ❌ |
| **Memory Determinism** | ✅ Pools | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Cross-Platform** | 5 platforms | Windows | Linux | 4 platforms | 3 platforms | 3 platforms |
| **Data Formats** | EDF+/BDF+/XDF/ENI | BCI2000 | OpenBCI | Custom | EDF | EDF/BDF/FIF |
| **Auto-Calibration** | ✅ 4-stage | ❌ | ❌ | ❌ | ✅ | ❌ |
| **Neurofeedback** | ✅ Adaptive | ✅ | ❌ | ❌ | ✅ | ❌ |
| **Transfer Learning** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Model Hot-Swap** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Health Monitoring** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Error Recovery** | ✅ Auto-reconnect | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Multi-Language SDK** | C/Python/Rust/Java/JS | C++ | Python | C++/Python/C#/Java | Python | Python |
| **License** | MIT | GPL | MIT | MIT | Commercial | BSD |

## Key Differentiators

### 1. Zero Dependencies
eNI's core has zero external dependencies. All parsers (JSON, EDF+, BDF+, XDF) are hand-written in C11. This makes it deployable on any platform with a C compiler.

### 2. Embedded-First Design
The Min layer runs on bare-metal ARM with no heap allocation required. No other BCI framework supports truly resource-constrained targets.

### 3. Production-Grade Safety
- Memory pool allocator for deterministic allocation
- Health monitoring with watchdog
- Exponential backoff auto-reconnect
- Stimulation safety with per-session limits and emergency shutdown

### 4. Ensemble Decoding with Hot-Swap
Run multiple decoder models simultaneously with weighted voting. Swap models without stopping the pipeline — no other BCI framework supports this.

### 5. Comprehensive Data Format Support
Native support for all major neural data formats (EDF+, BDF+, XDF) plus a custom high-performance streaming format. Thread-safe recording with background flush.

### 6. Transfer Learning
Built-in cross-session covariance alignment for reducing calibration time. Users can start a new session with a previously calibrated profile.
