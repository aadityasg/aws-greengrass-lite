// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <servicemgrd.h>
#include <gg/error.h>
#include <gg/log.h>
#include <ggl/nucleus/init.h>
#include <ggl/process.h>
#include <stddef.h>
#include <sys/stat.h>

#define S6_SCAN_DIR "/run/s6-services"

static GglProcessHandle svscan_handle = { -1 };

static GgError start_s6_svscan(void) {
    mkdir(S6_SCAN_DIR, 0755);

    const char *argv[] = { "s6-svscan", S6_SCAN_DIR, NULL };
    GgError ret = ggl_process_spawn(argv, NULL, &svscan_handle);
    if (ret != GG_ERR_OK) {
        GG_LOGE("Failed to spawn s6-svscan.");
        return ret;
    }
    GG_LOGI("Started s6-svscan on %s (pid %d).", S6_SCAN_DIR, svscan_handle.val);
    return GG_ERR_OK;
}

int main(void) {
    ggl_nucleus_init();

    GgError ret = start_s6_svscan();
    if (ret != GG_ERR_OK) {
        return 1;
    }

    ret = run_servicemgrd();
    if (ret != GG_ERR_OK) {
        return 1;
    }
}
