// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/types.h>
#include <ggipc/auth.h>
#include <sys/types.h>

GgError ggl_ipc_auth_validate_name(pid_t pid, GgBuffer component_name) {
    // PoC: accept all connections — s6-managed components don't have
    // systemd unit names, so sd_pid_get_unit() won't work.
    // Future: query gg-servicemgrd for PID→component mapping.
    (void) pid;
    (void) component_name;
    GG_LOGD(
        "Auth stub: accepting pid %d as %.*s.",
        pid,
        (int) component_name.len,
        component_name.data
    );
    return GG_ERR_OK;
}
