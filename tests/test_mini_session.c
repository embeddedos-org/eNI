// Minimal test to isolate session/calibration crash
#include <stdio.h>
#include <string.h>
#include "eni/session.h"
#include "eni/calibration.h"
#include "eni/profile.h"
#include "eni/transfer.h"
#include "eni/config.h"
#include "eni/log.h"
#include "eni_platform/platform.h"

static eni_session_t sess;
static eni_calibration_t cal;
static eni_profile_t prof;

int main(void)
{
    printf("step 1: platform_init\n"); fflush(stdout);
    eni_platform_init();

    printf("step 2: session_init\n"); fflush(stdout);
    memset(&sess, 0, sizeof(sess));
    eni_status_t rc = eni_session_init(&sess);
    printf("step 2 done: rc=%d\n", rc); fflush(stdout);

    printf("step 3: calibration_init\n"); fflush(stdout);
    memset(&cal, 0, sizeof(cal));
    rc = eni_calibration_init(&cal, 4, 256.0f);
    printf("step 3 done: rc=%d\n", rc); fflush(stdout);

    printf("step 4: impedance\n"); fflush(stdout);
    eni_calibration_start_impedance(&cal);
    float imp[4] = {10, 10, 10, 10};
    eni_calibration_submit_impedance(&cal, imp, 4);
    printf("step 4 done\n"); fflush(stdout);

    printf("step 5: baseline\n"); fflush(stdout);
    eni_calibration_start_baseline(&cal);
    for (int s = 0; s < 100; s++) {
        float samp[4] = {1.0f, 2.0f, 1.5f, 1.2f};
        eni_calibration_push_baseline_sample(&cal, samp, 4);
    }
    eni_calibration_finalize_baseline(&cal);
    printf("step 5 done\n"); fflush(stdout);

    printf("step 6: thresholds\n"); fflush(stdout);
    eni_calibration_compute_thresholds(&cal, 0.95f);
    printf("step 6 done\n"); fflush(stdout);

    printf("step 7: profile_save\n"); fflush(stdout);
    memset(&prof, 0, sizeof(prof));
    eni_profile_init(&prof, "test-user");
    rc = eni_profile_save(&prof, "test_profile_out.json");
    printf("step 7 done: rc=%d\n", rc); fflush(stdout);

    printf("step 8: cleanup\n"); fflush(stdout);
    eni_calibration_destroy(&cal);
    eni_session_destroy(&sess);

    printf("ALL DONE\n");
    return 0;
}
