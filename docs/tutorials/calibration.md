# Tutorial: Auto-Calibration

This tutorial walks through eNI's 4-stage automatic calibration pipeline.

## Overview

Before a BCI session can begin, the system needs to calibrate to the user's neural signals. eNI provides a standardized 4-stage calibration pipeline:

1. **Impedance Check** — Verify electrode contact quality
2. **Baseline Recording** — Capture resting-state neural activity (30 seconds)
3. **Threshold Computation** — Calculate per-channel detection thresholds
4. **Validation** — Test classification accuracy with real tasks

## Implementation

```c
#include "eni/calibration.h"
#include "eni/profile.h"

// Progress callback
void on_progress(eni_cal_stage_t stage, float progress, void *data) {
    const char *names[] = {"idle", "impedance", "baseline", "threshold", "validation", "complete", "failed"};
    printf("[%s] %.0f%%\n", names[stage], progress * 100.0f);
}

int main(void) {
    eni_calibration_t cal;
    eni_calibration_init(&cal, 8, 256.0f);  // 8 channels, 256 Hz
    eni_calibration_set_callback(&cal, on_progress, NULL);

    // ── Stage 1: Impedance Check ──
    eni_calibration_start_impedance(&cal);

    float impedances[8];
    measure_impedances(impedances, 8);  // Your device-specific function
    eni_calibration_submit_impedance(&cal, impedances, 8);

    // Check if all channels are under threshold (default: 50 kΩ)
    for (int i = 0; i < 8; i++) {
        if (impedances[i] > 50.0f) {
            printf("Warning: Channel %d impedance too high (%.1f kΩ)\n", i, impedances[i]);
        }
    }

    // ── Stage 2: Baseline (30 seconds) ──
    printf("Please relax for 30 seconds...\n");
    eni_calibration_start_baseline(&cal);

    int total_samples = 256 * 30; // 30 seconds at 256 Hz
    for (int s = 0; s < total_samples; s++) {
        float samples[8];
        read_samples(samples, 8);  // Your acquisition function
        eni_calibration_push_baseline_sample(&cal, samples, 8);
    }
    eni_calibration_finalize_baseline(&cal);

    // ── Stage 3: Compute Thresholds ──
    eni_calibration_compute_thresholds(&cal, 0.95f);  // 95th percentile

    // ── Stage 4: Validation ──
    printf("Starting validation trials...\n");
    eni_calibration_start_validation(&cal);

    for (int trial = 0; trial < 20; trial++) {
        bool correct = run_trial();  // Your paradigm function
        eni_calibration_submit_trial(&cal, correct);
    }

    // ── Finalize ──
    eni_profile_t profile;
    eni_profile_init(&profile, "user-001");

    eni_status_t rc = eni_calibration_finalize(&cal, &profile);
    if (rc == ENI_OK) {
        printf("Calibration succeeded! Accuracy: %.1f%%\n",
               eni_calibration_get_accuracy(&cal) * 100.0f);

        // Save profile for future sessions
        eni_profile_save(&profile, "profiles/user-001.json");
    } else {
        printf("Calibration failed. Please try again.\n");
    }

    eni_calibration_destroy(&cal);
    return 0;
}
```

## Loading a Previous Profile

```c
eni_profile_t profile;
eni_profile_load(&profile, "profiles/user-001.json");

// Apply saved calibration gains to signal processor
for (int ch = 0; ch < profile.num_channels; ch++) {
    printf("Ch %d: gain=%.4f offset=%.4f impedance=%.1f kΩ\n",
           ch, profile.channel_gains[ch],
           profile.channel_offsets[ch],
           profile.impedance_baseline[ch]);
}
```
