// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

// PoC: Replaced all sd_bus/systemd calls with coreBus calls to gg_supervisor.

#include "sd_bus.h"
#include <gg/arena.h>
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/client.h>
#include <stdint.h>

GgError get_lifecycle_state(GgBuffer component_name, GgBuffer *state) {
    uint8_t alloc_mem[512];
    GgArena alloc = gg_arena_init(
        (GgBuffer) { .data = alloc_mem, .len = sizeof(alloc_mem) }
    );
    GgObject result;
    GgError error;

    GgError ret = ggl_call(
        GG_STR("gg_supervisor"),
        GG_STR("get_status"),
        GG_MAP(
            gg_kv(GG_STR("component_name"), gg_obj_buf(component_name))
        ),
        &error,
        &alloc,
        &result
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE(
            "Failed to get status from gg_supervisor for %.*s (ret=%d).",
            (int) component_name.len,
            component_name.data,
            ret
        );
        return ret;
    }

    GgObject *state_obj;
    ret = gg_map_validate(
        gg_obj_into_map(result),
        GG_MAP_SCHEMA(
            { GG_STR("lifecycle_state"), GG_REQUIRED, GG_TYPE_BUF, &state_obj },
        )
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE("Invalid response from gg_supervisor get_status.");
        return GG_ERR_FAILURE;
    }

    *state = gg_obj_into_buf(*state_obj);
    return GG_ERR_OK;
}

GgError restart_component(GgBuffer component_name) {
    // PoC: just log — full restart requires version+phase which we don't have
    GG_LOGI(
        "restart_component called for %.*s (no-op in PoC).",
        (int) component_name.len,
        component_name.data
    );
    return GG_ERR_OK;
}

GgError reset_restart_counters(GgBuffer component_name) {
    // PoC: no-op — restart counters managed by gg-servicemgrd
    (void) component_name;
    return GG_ERR_OK;
}
