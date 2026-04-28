# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.0] - 2026-04-27

### Added

#### Platform Threading Primitives
- `eni_mutex_t` — init/lock/trylock/unlock/destroy across all platforms
- `eni_condvar_t` — init/wait/timedwait/signal/broadcast/destroy
- `eni_thread_t` — create/join/detach
- `eni_atomic_int_t` — init/load/store/fetch_add/fetch_sub/compare_exchange
- Platform implementations: Linux (pthreads), macOS (pthreads + mach_absolute_time), Windows (CRITICAL_SECTION/CONDITION_VARIABLE/CreateThread), POSIX (generic fallback), EoS (single-threaded stubs)

#### Framework Signal Processor (`eni_fw_signal_processor_t`)
- Multi-channel processing — up to 256 channels with per-channel epoch buffers
- Pluggable filter chain — `eni_fw_signal_processor_add_filter()` with lowpass, highpass, bandpass, notch types
- Thread-safe processing — all operations mutex-protected
- Artifact rejection — per-epoch severity check with configurable threshold
- New API: `init(num_channels, epoch_size, sample_rate, threshold)`, `add_filter()`, `process()`, `process_multi()`, `reset()`, `shutdown()`

#### Framework Decoder (`eni_fw_decoder_t`)
- Ensemble decoding — up to 4 models with weighted voting via `eni_fw_decoder_add_ensemble()`
- Model hot-swap — `eni_fw_decoder_swap_model()` replaces a decoder without stopping the pipeline
- Confidence smoothing — exponential moving average via `eni_fw_decoder_set_ema_alpha()`
- Temporal consistency — majority vote from decision history buffer

#### Framework Feedback (`eni_fw_feedback_t`)
- Adaptive intensity modulation — `ENI_FW_ADAPT_CONFIDENCE`, `ENI_FW_ADAPT_PERFORMANCE`, `ENI_FW_ADAPT_FATIGUE` modes
- Multi-output routing — up to 4 stimulator outputs via `eni_fw_feedback_add_output()`
- Dynamic rule management — `eni_fw_feedback_add_rule()`, `eni_fw_feedback_remove_rule()`
- Global intensity control — `eni_fw_feedback_set_intensity()`, `eni_fw_feedback_enable()`

#### Thread-Safe Stream Bus
- Blocking pop — `eni_fw_stream_bus_pop_wait(bus, ev, timeout_ms)` with condvar
- `eni_fw_stream_bus_destroy()` for cleanup

#### JSON Configuration
- `eni_json_parse()` — minimal recursive descent parser, zero dependencies, max nesting depth 32
- `eni_json_free()` — recursive cleanup of parsed tree
- `eni_json_get()`, `eni_json_get_string()`, `eni_json_get_number()`, `eni_json_get_bool()` — object field lookups
- `eni_config_load_file()` now auto-detects `.json` files and parses them alongside key=value format

#### Neural Data Formats
- **EDF+** — `eni_edf_open()`, `eni_edf_create()`, `eni_edf_read_record()`, `eni_edf_write_record()`, `eni_edf_read_annotations()`, `eni_edf_finalize()`, `eni_edf_close()`; digital↔physical conversion
- **BDF+** — `eni_bdf_open()`, `eni_bdf_create()`, `eni_bdf_read_record()`, `eni_bdf_write_record()`, `eni_bdf_finalize()`, `eni_bdf_close()`; 24-bit BioSemi format
- **XDF** — `eni_xdf_open()`, `eni_xdf_create()`, `eni_xdf_add_stream()`, `eni_xdf_read_samples()`, `eni_xdf_write_samples()`, `eni_xdf_write_clock_offset()`, `eni_xdf_finalize()`, `eni_xdf_close()`; multi-stream, clock offset correction
- **ENI Native** — `eni_native_open()`, `eni_native_create()`, `eni_native_write_samples()`, `eni_native_write_event()`, `eni_native_finalize()`; chunk-based, optimized for real-time streaming
- **Format Detection** — `eni_data_format_detect()`, `eni_data_format_detect_file()` — auto-detect from magic bytes
- **Annotation System** — `eni_annotation_add_marker()`, `eni_annotation_add_epoch()`, `eni_annotation_add_tag()`, `eni_annotation_sort_by_onset()`, `eni_annotation_find_in_range()`, `eni_annotation_merge()`

#### Recording & Playback
- **Recorder** — `eni_recorder_init()`, `eni_recorder_start()`, `eni_recorder_push_samples()`, `eni_recorder_pause()`, `eni_recorder_resume()`, `eni_recorder_stop()`, `eni_recorder_destroy()`; thread-safe ring buffer with background flush thread
- **Player** — `eni_player_open()`, `eni_player_play()`, `eni_player_pause()`, `eni_player_stop()`, `eni_player_seek()`, `eni_player_set_speed()`, `eni_player_set_loop()`, `eni_player_close()`; auto-detect format, callback-based sample delivery

#### Session Management (`eni_session_t`)
- State machine: IDLE → CALIBRATING → RUNNING → PAUSED → STOPPED
- `eni_session_set_subject()`, `eni_session_set_description()`, `eni_session_set_meta()`, `eni_session_get_meta()`
- State change callback via `eni_session_set_callback()`
- Elapsed time tracking via `eni_session_elapsed_ms()`

#### User Profile System (`eni_profile_t`)
- Per-channel calibration storage (gains, offsets, impedance baselines)
- JSON serialization via `eni_profile_save()` / `eni_profile_load()`
- Session history tracking via `eni_profile_update_session()`

#### Auto-Calibration Pipeline (`eni_calibration_t`)
- 4-stage pipeline: impedance check → baseline (30s resting) → threshold (percentile-based) → validation (accuracy test)
- Progress callback via `eni_calibration_set_callback()`
- Automatic profile generation via `eni_calibration_finalize(cal, profile)`
- Failure detection — rejects calibrations with <60% validation accuracy

#### Transfer Learning (`eni_transfer_t`)
- Cross-session covariance alignment via `eni_transfer_fit_source()`, `eni_transfer_fit_target()`, `eni_transfer_compute_alignment()`
- Feature space normalization via `eni_transfer_apply()`

#### Multi-Device Management (`eni_device_manager_t`)
- `eni_device_manager_scan()` — discover available devices
- `eni_device_manager_connect()` / `eni_device_manager_disconnect()` — lifecycle management
- `eni_device_manager_sync_start()` — synchronized multi-device acquisition
- Thread-safe with mutex protection

#### Production Hardening
- **Error Recovery** — `eni_recovery_t` with exponential backoff + jitter; `eni_auto_reconnect_t` for automatic reconnection
- **Health Monitor** — `eni_health_monitor_t` with periodic checks, watchdog timer, health reports
- **Memory Pool** — `eni_mempool_t` fixed-size block allocator for real-time paths; thread-safe alloc/free

#### LSL Integration
- `eni_lsl_provider_t` — Lab Streaming Layer provider wrapping `eni_provider_t`

#### Language Bindings
- **Rust** — `eni` crate with FFI declarations + safe wrappers (`Config`, `Session`, `SignalProcessor`) with `Drop` for RAII cleanup
- **Java/Android** — `com.eos.eni.EniSession` JNI wrapper with `AutoCloseable`
- **Python** — `eni.data_formats` module: `read_edf()`, `write_edf()`, `read_xdf()` via ctypes

#### Documentation
- `docs/PRODUCT_OVERVIEW.md` — non-technical overview
- `docs/ARCHITECTURE.md` — system architecture with diagrams
- `docs/GETTING_STARTED.md` — quick start guide with code examples
- `docs/BUILDING.md` — all platforms, all CMake options
- `docs/COMPETITIVE_ANALYSIS.md` — feature comparison vs BCI2000, OpenBCI, BrainFlow, NeuroPype, MNE-Python
- `docs/tutorials/recording_data.md` — data recording tutorial
- `docs/tutorials/calibration.md` — auto-calibration tutorial

#### Testing
- 13 new test files, 100+ test cases
- Integration test: end-to-end calibration + session workflow (7 scenarios)
- Stress tests: concurrent calibration (4 threads), multi-device sync, stream bus producer/consumer contention, memory pool concurrent alloc/free (7 scenarios)
- Coverage gap tests achieving 100% on types.c, log.c, event.c, policy.c

### Changed
- `eni_fw_signal_processor_t` — replaced typedef alias to `eni_min_signal_processor_t` with full multi-channel struct
- `eni_fw_decoder_t` — replaced typedef alias to `eni_min_decoder_t` with ensemble decoder struct
- `eni_fw_feedback_t` — replaced typedef alias to `eni_min_feedback_t` with adaptive feedback struct
- `eni_fw_stream_bus_t` — added `eni_mutex_t`, `eni_condvar_t`, `bool initialized` fields
- `eni_config_load_file()` — now supports JSON files in addition to key=value format
- `platform/CMakeLists.txt` — auto-detects macOS (APPLE) and links Threads library
- `framework/CMakeLists.txt` — links `eni_platform`, includes `health.c`
- `common/CMakeLists.txt` — includes JSON, data formats, session, recovery, mempool sources

### Fixed
- **Buffer overflow in `edf.c` and `bdf.c`** — `read_fixed()` wrote `\0` at `buf[len]` when the buffer was exactly `len` bytes, causing out-of-bounds write on fields like `patient[80]`
- **Dead code in `player.c`** — `eni_player_play()` set `state = PLAYING` then checked `state != PAUSED` (always true); resume-from-pause path was unreachable
- **Infinite loop in `recorder.c`** — `recorder_flush_thread()` would loop forever if `samples_per_record` was 0
- **Memory leak in `json.c`** — `parse_object()` and `parse_array()` didn't update `count` on error paths, so `eni_json_free()` skipped already-parsed nested children
- **Memory leak in `config.c`** — `config_load_json()` didn't call `eni_json_free()` on parse failure, leaking partial JSON tree

### Breaking Changes
- `eni_fw_signal_processor_init()` — signature changed from `(sp, epoch_size, sample_rate, threshold)` to `(sp, num_channels, epoch_size, sample_rate, threshold)`
- `eni_fw_signal_processor_process()` — signature changed to accept channel index and per-sample processing
- `eni_fw_decoder_init()` — now initializes `eni_fw_decoder_t` (not `eni_min_decoder_t`)
- `eni_fw_feedback_init()` — now initializes `eni_fw_feedback_t` (not `eni_min_feedback_t`)
- `eni_fw_stream_bus_t` struct layout changed (added threading fields); must call `eni_fw_stream_bus_destroy()` for cleanup

## [0.1.0] - 2026-03-31

### Added
- Initial release of eni (Embedded Neural Interface)
- Multi-provider BCI framework (Neuralink, EEG, simulator, generic)
- Neuralink adapter: 1024-channel acquisition at 30kHz sampling
- DSP pipeline: bandpass filter, spike detection, feature extraction
- Neural network decoder for intent classification
- Stimulator interface with safety checks and charge-balance verification
- Event/feedback system for closed-loop BCI
- EIPC bridge for intent routing to EAI/EoS
- ENI-Min lightweight runtime for resource-constrained devices
- ENI-Framework industrial-grade platform with signal processor
- Complete CI/CD pipeline with nightly, weekly, EoSim sanity, and simulation test runs
- Full cross-platform support (Linux, Windows, macOS)
- ISO/IEC standards compliance documentation
- MIT license

[Unreleased]: https://github.com/embeddedos-org/eNI/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/embeddedos-org/eNI/compare/v0.1.0...v0.3.0
[0.1.0]: https://github.com/embeddedos-org/eNI/releases/tag/v0.1.0
