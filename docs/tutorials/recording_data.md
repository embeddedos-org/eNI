# Tutorial: Recording Neural Data

This tutorial shows how to record neural data to EDF+ format using eNI.

## Overview

eNI's recording system uses a background thread to flush data to disk, ensuring the acquisition loop is never blocked by I/O.

## Step 1: Set Up Data Header

```c
#include "eni/data_format.h"
#include "eni/recorder.h"
#include "eni/edf.h"

// Configure the recording
eni_data_header_t header;
eni_data_header_init(&header);

strncpy(header.patient, "Subject-001", sizeof(header.patient) - 1);
strncpy(header.recording, "Motor Imagery Task", sizeof(header.recording) - 1);

header.num_channels = 8;
header.record_duration = 1.0; // 1 second per record

// Configure each channel
const char *labels[] = {"Fp1", "Fp2", "C3", "C4", "P3", "P4", "O1", "O2"};
for (int i = 0; i < 8; i++) {
    strncpy(header.channels[i].label, labels[i], ENI_DATA_MAX_LABEL - 1);
    strncpy(header.channels[i].physical_dim, "uV", 7);
    header.channels[i].physical_min = -3200.0;
    header.channels[i].physical_max = 3200.0;
    header.channels[i].digital_min = -32768;
    header.channels[i].digital_max = 32767;
    header.channels[i].samples_per_record = 256;
    header.channels[i].sample_rate = 256.0;
}
```

## Step 2: Start Recording

```c
eni_recorder_t recorder;
eni_recorder_init(&recorder, ENI_FORMAT_EDF, &header);
eni_recorder_start(&recorder, "recording_001.edf");
```

## Step 3: Push Samples During Acquisition

```c
// In your acquisition loop:
double samples[8]; // one sample per channel

while (acquiring) {
    // Get samples from your device...
    get_samples_from_device(samples, 8);

    // Push to recorder (thread-safe)
    eni_recorder_push_samples(&recorder, samples, 8);
}
```

## Step 4: Stop and Clean Up

```c
eni_recorder_stop(&recorder);    // Flushes remaining data
eni_recorder_destroy(&recorder); // Free resources
```

## Reading Back the Data

```c
eni_edf_file_t edf;
eni_edf_open(&edf, "recording_001.edf");

printf("Channels: %d\n", edf.header.num_channels);
printf("Records: %lld\n", (long long)edf.header.num_records);

// Read each record
for (int64_t r = 0; r < edf.header.num_records; r++) {
    double data[2048]; // 8 channels * 256 samples
    eni_edf_read_record(&edf, r, data, 2048);
    // Process data...
}

eni_edf_close(&edf);
```

## Adding Annotations

```c
#include "eni/annotation.h"

eni_annotation_list_t annotations;
eni_annotation_list_init(&annotations);

// Add event markers
eni_annotation_add_marker(&annotations, 10.0, "stimulus_onset");
eni_annotation_add_marker(&annotations, 10.5, "response");

// Add epochs
eni_annotation_add_epoch(&annotations, 10.0, 1.0, "trial_1");

// Tag annotations
eni_annotation_add_tag(&annotations, 1, "visual");

// Sort by time
eni_annotation_sort_by_onset(&annotations);
```
