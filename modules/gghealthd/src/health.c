// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

// PoC: Replaced all sd_bus/systemd calls with coreBus calls to gg_supervisor.

#include "health.h"
#include "bus_client.h"
#include "sd_bus.h"
#include "subscriptions.h"
#include <assert.h>
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <ggl/nucleus/constants.h>
#include <stdbool.h>
#include <stdint.h>

GgError gghealthd_get_status(GgBuffer component_name, GgBuffer *status) {
    assert(status != NULL);
    if (component_name.len > GGL_COMPONENT_NAME_MAX_LEN) {
        GG_LOGE("component_name too long");
        return GG_ERR_RANGE;
    }

    if (gg_buffer_eq(component_name, GG_STR("gghealthd"))) {
        *status = GG_STR("RUNNING");
        return GG_ERR_OK;
    }

    GgError err = verify_component_exists(component_name);
    if (err != GG_ERR_OK) {
        return err;
    }

    return get_lifecycle_state(component_name, status);
}

GgError gghealthd_update_status(GgBuffer component_name, GgBuffer status) {
    // PoC: no-op — s6 doesn't use sd_notify for readiness
    GG_LOGD(
        "update_status: %.*s → %.*s (no-op in PoC).",
        (int) component_name.len,
        component_name.data,
        (int) status.len,
        status.data
    );
    return GG_ERR_OK;
}

GgError gghealthd_get_health(GgBuffer *status) {
    assert(status != NULL);
    // TODO: check all root components
    *status = GG_STR("HEALTHY");
    return GG_ERR_OK;
}

GgError gghealthd_restart_component(GgBuffer component_name) {
    if (component_name.len > GGL_COMPONENT_NAME_MAX_LEN) {
        GG_LOGE("component_name too long");
        return GG_ERR_RANGE;
    }

    GgError err = verify_component_exists(component_name);
    if (err != GG_ERR_OK) {
        return err;
    }

    return restart_component(component_name);
}

GgError gghealthd_init(void) {
    GG_LOGI("gghealthd initialized (PoC: no sd_bus, using gg_supervisor).");
    init_health_events();
    return GG_ERR_OK;
}
