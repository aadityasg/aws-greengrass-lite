// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "s6_backend.h"
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <ggl/process.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static GgError build_service_dir(
    GgBuffer component_name, char *out, size_t out_len
) {
    int ret = snprintf(
        out,
        out_len,
        "%s/%s%.*s",
        S6_SCAN_DIR,
        S6_SERVICE_PREFIX,
        (int) component_name.len,
        component_name.data
    );
    if (ret < 0 || (size_t) ret >= out_len) {
        return GG_ERR_RANGE;
    }
    return GG_ERR_OK;
}

static GgError write_run_script(
    const char *service_dir,
    GgBuffer component_name,
    GgBuffer version,
    GgBuffer phase
) {
    char run_path[PATH_MAX];
    snprintf(run_path, sizeof(run_path), "%s/run", service_dir);

    FILE *f = fopen(run_path, "w");
    if (f == NULL) {
        GG_LOGE("Failed to create run script at %s.", run_path);
        return GG_ERR_FAILURE;
    }
    fprintf(
        f,
        "#!/bin/sh\nexec recipe-runner -n %.*s -v %.*s -p %.*s\n",
        (int) component_name.len,
        component_name.data,
        (int) version.len,
        version.data,
        (int) phase.len,
        phase.data
    );
    fclose(f);
    chmod(run_path, 0755);
    return GG_ERR_OK;
}

static GgError s6_rescan(void) {
    const char *argv[] = { "s6-svscanctl", "-a", S6_SCAN_DIR, NULL };
    return ggl_process_call(argv, NULL);
}

static GgError wait_for_supervise(const char *service_dir) {
    char status_path[PATH_MAX];
    snprintf(status_path, sizeof(status_path), "%s/supervise/status", service_dir);
    for (int i = 0; i < 20; i++) {
        if (access(status_path, F_OK) == 0) {
            return GG_ERR_OK;
        }
        usleep(100000); // 100ms
    }
    GG_LOGE("Timed out waiting for supervise dir in %s.", service_dir);
    return GG_ERR_FAILURE;
}

GgError s6_start_component(
    GgBuffer component_name, GgBuffer version, GgBuffer phase
) {
    char service_dir[PATH_MAX];
    GgError ret = build_service_dir(component_name, service_dir, sizeof(service_dir));
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Create service directory with down file (don't auto-start yet)
    mkdir(service_dir, 0755);

    char down_path[PATH_MAX];
    snprintf(down_path, sizeof(down_path), "%s/down", service_dir);
    FILE *df = fopen(down_path, "w");
    if (df != NULL) {
        fclose(df);
    }

    ret = write_run_script(service_dir, component_name, version, phase);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Tell s6-svscan to pick up the new service directory
    ret = s6_rescan();
    if (ret != GG_ERR_OK) {
        GG_LOGE("s6-svscanctl rescan failed.");
        return ret;
    }

    // Wait for s6-supervise to create the supervise/ directory
    ret = wait_for_supervise(service_dir);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Remove down file and bring service up
    unlink(down_path);
    const char *up_argv[] = { "s6-svc", "-u", service_dir, NULL };
    ret = ggl_process_call(up_argv, NULL);
    if (ret != GG_ERR_OK) {
        GG_LOGE("s6-svc -u failed for %s.", service_dir);
        return ret;
    }

    GG_LOGI(
        "Started component %.*s via s6 at %s.",
        (int) component_name.len,
        component_name.data,
        service_dir
    );
    return GG_ERR_OK;
}

GgError s6_stop_component(GgBuffer component_name) {
    char service_dir[PATH_MAX];
    GgError ret = build_service_dir(component_name, service_dir, sizeof(service_dir));
    if (ret != GG_ERR_OK) {
        return ret;
    }

    if (access(service_dir, F_OK) != 0) {
        GG_LOGD(
            "Service dir %s does not exist, nothing to stop.", service_dir
        );
        return GG_ERR_OK;
    }

    // Bring service down
    const char *down_argv[] = { "s6-svc", "-d", service_dir, NULL };
    (void) ggl_process_call(down_argv, NULL);

    // Exit the supervisor
    const char *exit_argv[] = { "s6-svc", "-x", service_dir, NULL };
    (void) ggl_process_call(exit_argv, NULL);

    // Brief wait for supervisor to exit
    usleep(500000);

    // Remove service directory
    const char *rm_argv[] = { "rm", "-rf", service_dir, NULL };
    ret = ggl_process_call(rm_argv, NULL);
    if (ret != GG_ERR_OK) {
        GG_LOGE("Failed to remove service dir %s.", service_dir);
        return ret;
    }

    // Rescan
    ret = s6_rescan();

    GG_LOGI(
        "Stopped component %.*s, removed %s.",
        (int) component_name.len,
        component_name.data,
        service_dir
    );
    return GG_ERR_OK;
}
