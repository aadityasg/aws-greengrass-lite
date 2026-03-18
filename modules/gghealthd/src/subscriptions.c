// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

// PoC: Replaced sd_bus_match_signal with ggl_subscribe to gg_supervisor.

#include "subscriptions.h"
#include "sd_bus.h"
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/client.h>
#include <ggl/core_bus/server.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define GGHEALTHD_MAX_SUBSCRIPTIONS 10

static struct {
    char component_name[128];
    uint32_t gg_health_handle; // handle from the gg_health subscriber
    uint32_t supervisor_handle; // handle from our subscription to gg_supervisor
    bool active;
} slots[GGHEALTHD_MAX_SUBSCRIPTIONS];

static GgError supervisor_response_callback(
    void *ctx, uint32_t handle, GgObject data
) {
    (void) handle;
    int idx = (int) (intptr_t) ctx;
    if (idx < 0 || idx >= GGHEALTHD_MAX_SUBSCRIPTIONS || !slots[idx].active) {
        return GG_ERR_OK;
    }

    // Forward the lifecycle state to the gg_health subscriber
    if (gg_obj_type(data) != GG_TYPE_MAP) {
        return GG_ERR_OK;
    }

    GgObject *state_obj;
    GgError ret = gg_map_validate(
        gg_obj_into_map(data),
        GG_MAP_SCHEMA(
            { GG_STR("lifecycle_state"), GG_REQUIRED, GG_TYPE_BUF, &state_obj },
        )
    );
    if (ret != GG_ERR_OK) {
        return GG_ERR_OK;
    }

    GgBuffer state = gg_obj_into_buf(*state_obj);
    GgBuffer name = { .data = (uint8_t *) slots[idx].component_name,
                      .len = strlen(slots[idx].component_name) };

    GG_LOGD(
        "Forwarding lifecycle event: %s is %.*s.",
        slots[idx].component_name,
        (int) state.len,
        state.data
    );

    ggl_sub_respond(
        slots[idx].gg_health_handle,
        gg_obj_map(GG_MAP(
            gg_kv(GG_STR("component_name"), gg_obj_buf(name)),
            gg_kv(GG_STR("lifecycle_state"), gg_obj_buf(state))
        ))
    );
    return GG_ERR_OK;
}

static void supervisor_close_callback(void *ctx, uint32_t handle) {
    (void) handle;
    int idx = (int) (intptr_t) ctx;
    if (idx >= 0 && idx < GGHEALTHD_MAX_SUBSCRIPTIONS) {
        GG_LOGD("Supervisor subscription closed for slot %d.", idx);
    }
}

GgError gghealthd_register_lifecycle_subscription(
    GgBuffer component_name, uint32_t handle
) {
    // Find free slot
    int idx = -1;
    for (int i = 0; i < GGHEALTHD_MAX_SUBSCRIPTIONS; i++) {
        if (!slots[i].active) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        GG_LOGW("Subscription table full.");
        return GG_ERR_NOMEM;
    }

    // Accept the gg_health subscription
    ggl_sub_accept(
        handle, gghealthd_unregister_lifecycle_subscription, NULL
    );

    // Check if already in terminal state — send immediate response
    GgBuffer state = { 0 };
    GgError ret = get_lifecycle_state(component_name, &state);
    if (ret == GG_ERR_OK) {
        if (gg_buffer_eq(state, GG_STR("RUNNING"))
            || gg_buffer_eq(state, GG_STR("FINISHED"))
            || gg_buffer_eq(state, GG_STR("BROKEN"))) {
            ggl_sub_respond(
                handle,
                gg_obj_map(GG_MAP(
                    gg_kv(
                        GG_STR("component_name"), gg_obj_buf(component_name)
                    ),
                    gg_kv(GG_STR("lifecycle_state"), gg_obj_buf(state))
                ))
            );
            return GG_ERR_OK;
        }
    }

    // Subscribe to gg_supervisor for lifecycle events
    size_t copy_len = component_name.len < sizeof(slots[idx].component_name) - 1
        ? component_name.len
        : sizeof(slots[idx].component_name) - 1;
    memcpy(slots[idx].component_name, component_name.data, copy_len);
    slots[idx].component_name[copy_len] = '\0';
    slots[idx].gg_health_handle = handle;
    slots[idx].active = true;

    GgError sub_error;
    ret = ggl_subscribe(
        GG_STR("gg_supervisor"),
        GG_STR("subscribe_to_lifecycle"),
        GG_MAP(
            gg_kv(GG_STR("component_name"), gg_obj_buf(component_name))
        ),
        supervisor_response_callback,
        supervisor_close_callback,
        (void *) (intptr_t) idx,
        &sub_error,
        &slots[idx].supervisor_handle
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE(
            "Failed to subscribe to gg_supervisor for %.*s.",
            (int) component_name.len,
            component_name.data
        );
        slots[idx].active = false;
        return ret;
    }

    return GG_ERR_OK;
}

void gghealthd_unregister_lifecycle_subscription(void *ctx, uint32_t handle) {
    (void) ctx;
    for (int i = 0; i < GGHEALTHD_MAX_SUBSCRIPTIONS; i++) {
        if (slots[i].active && slots[i].gg_health_handle == handle) {
            ggl_client_sub_close(slots[i].supervisor_handle);
            slots[i].active = false;
            GG_LOGD("Unregistered lifecycle subscription for %s.", slots[i].component_name);
            return;
        }
    }
}

void init_health_events(void) {
    // PoC: no sd_event loop needed — subscriptions go through coreBus
    GG_LOGD("init_health_events: using coreBus subscriptions (no sd_event).");
}
