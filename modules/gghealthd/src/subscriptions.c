// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

// PoC: Poll-based lifecycle subscriptions via gg_supervisor get_status.

#include "subscriptions.h"
#include "sd_bus.h"
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/server.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define GGHEALTHD_MAX_SUBSCRIPTIONS 10

static struct {
    char component_name[128];
    uint32_t handle;
    bool active;
} slots[GGHEALTHD_MAX_SUBSCRIPTIONS];

static pthread_mutex_t slots_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t poll_thread;
static bool poll_running = false;

static bool is_terminal(GgBuffer state) {
    return gg_buffer_eq(state, GG_STR("RUNNING"))
        || gg_buffer_eq(state, GG_STR("FINISHED"))
        || gg_buffer_eq(state, GG_STR("BROKEN"));
}

static void *poll_loop(void *arg) {
    (void) arg;
    while (1) {
        usleep(500000); // 500ms
        pthread_mutex_lock(&slots_mtx);
        for (int i = 0; i < GGHEALTHD_MAX_SUBSCRIPTIONS; i++) {
            if (!slots[i].active) {
                continue;
            }
            GgBuffer name = { .data = (uint8_t *) slots[i].component_name,
                              .len = strlen(slots[i].component_name) };
            GgBuffer state = { 0 };
            GgError ret = get_lifecycle_state(name, &state);
            if (ret != GG_ERR_OK) {
                continue;
            }
            if (is_terminal(state)) {
                GG_LOGI(
                    "Lifecycle subscription: %s reached %.*s.",
                    slots[i].component_name,
                    (int) state.len,
                    state.data
                );
                ggl_sub_respond(
                    slots[i].handle,
                    gg_obj_map(GG_MAP(
                        gg_kv(GG_STR("component_name"), gg_obj_buf(name)),
                        gg_kv(GG_STR("lifecycle_state"), gg_obj_buf(state))
                    ))
                );
                // Don't deactivate — let the subscriber close
            }
        }
        pthread_mutex_unlock(&slots_mtx);
    }
    return NULL;
}

void gghealthd_unregister_lifecycle_subscription(void *ctx, uint32_t handle) {
    (void) ctx;
    pthread_mutex_lock(&slots_mtx);
    for (int i = 0; i < GGHEALTHD_MAX_SUBSCRIPTIONS; i++) {
        if (slots[i].active && slots[i].handle == handle) {
            slots[i].active = false;
            GG_LOGD(
                "Unregistered lifecycle subscription for %s.",
                slots[i].component_name
            );
            break;
        }
    }
    pthread_mutex_unlock(&slots_mtx);
}

GgError gghealthd_register_lifecycle_subscription(
    GgBuffer component_name, uint32_t handle
) {
    // Accept the subscription
    ggl_sub_accept(
        handle, gghealthd_unregister_lifecycle_subscription, NULL
    );

    // Immediate check — if already terminal, respond now
    GgBuffer state = { 0 };
    GgError ret = get_lifecycle_state(component_name, &state);
    if (ret == GG_ERR_OK && is_terminal(state)) {
        GG_LOGI(
            "Lifecycle subscription: %.*s already %.*s, responding immediately.",
            (int) component_name.len,
            component_name.data,
            (int) state.len,
            state.data
        );
        ggl_sub_respond(
            handle,
            gg_obj_map(GG_MAP(
                gg_kv(GG_STR("component_name"), gg_obj_buf(component_name)),
                gg_kv(GG_STR("lifecycle_state"), gg_obj_buf(state))
            ))
        );
        return GG_ERR_OK;
    }

    // Store for polling
    pthread_mutex_lock(&slots_mtx);
    for (int i = 0; i < GGHEALTHD_MAX_SUBSCRIPTIONS; i++) {
        if (!slots[i].active) {
            size_t copy_len
                = component_name.len < sizeof(slots[i].component_name) - 1
                ? component_name.len
                : sizeof(slots[i].component_name) - 1;
            memcpy(slots[i].component_name, component_name.data, copy_len);
            slots[i].component_name[copy_len] = '\0';
            slots[i].handle = handle;
            slots[i].active = true;
            pthread_mutex_unlock(&slots_mtx);
            GG_LOGD(
                "Registered lifecycle subscription for %.*s.",
                (int) component_name.len,
                component_name.data
            );
            return GG_ERR_OK;
        }
    }
    pthread_mutex_unlock(&slots_mtx);
    GG_LOGW("Subscription table full.");
    return GG_ERR_NOMEM;
}

void init_health_events(void) {
    if (!poll_running) {
        pthread_create(&poll_thread, NULL, poll_loop, NULL);
        pthread_detach(poll_thread);
        poll_running = true;
        GG_LOGD("Started lifecycle subscription poll thread.");
    }
}
