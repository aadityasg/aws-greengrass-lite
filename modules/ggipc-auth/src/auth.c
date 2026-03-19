// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <dirent.h>
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <ggipc/auth.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define S6_SCAN_DIR "/run/s6-services"
#define S6_SERVICE_PREFIX "ggl."

static int read_ppid(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }
    char line[256];
    int ppid = -1;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "PPid:\t%d", &ppid) == 1) {
            break;
        }
    }
    fclose(f);
    return ppid;
}

// Read /proc/<pid>/cmdline and check if it's s6-supervise for a component
static bool check_s6_supervise(
    pid_t pid, GgBuffer component_name
) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }
    char cmdline[256] = { 0 };
    size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
    fclose(f);
    if (n == 0) {
        return false;
    }

    if (strstr(cmdline, "s6-supervise") == NULL) {
        return false;
    }

    // Second arg is service dir name (after first NUL)
    char *svc_name = cmdline + strlen(cmdline) + 1;
    // Strip prefix
    size_t prefix_len = strlen(S6_SERVICE_PREFIX);
    if (strncmp(svc_name, S6_SERVICE_PREFIX, prefix_len) == 0) {
        svc_name += prefix_len;
    }

    return strlen(svc_name) == component_name.len
        && memcmp(svc_name, component_name.data, component_name.len) == 0;
}

GgError ggl_ipc_auth_validate_name(pid_t pid, GgBuffer component_name) {
    // Walk up the process tree to find an s6-supervise parent
    pid_t check_pid = pid;
    for (int depth = 0; depth < 5; depth++) {
        int ppid = read_ppid(check_pid);
        if (ppid <= 1) {
            break;
        }
        if (check_s6_supervise((pid_t) ppid, component_name)) {
            GG_LOGD(
                "Auth: PID %d verified as %.*s via s6-supervise (PID %d).",
                pid,
                (int) component_name.len,
                component_name.data,
                ppid
            );
            return GG_ERR_OK;
        }
        check_pid = (pid_t) ppid;
    }

    GG_LOGD(
        "Auth: PID %d not under s6-supervise for %.*s, rejecting.",
        pid,
        (int) component_name.len,
        component_name.data
    );
    return GG_ERR_FAILURE;
}
