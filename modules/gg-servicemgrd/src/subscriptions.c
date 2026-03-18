// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "subscriptions.h"
#include "s6_backend.h"
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

static struct {
    char name[128];
    uint32_t handle;
    bool active;
} subs[MAX_SUBSCRIPTIONS];

static pthread_mutex_t subs_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t poll_thread;
static bool poll_running = false;

static void sub_on_close(void *ctx, uint32_t handle) {
    (void) ctx;
    pthread_mutex_lock(&subs_mtx);
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subs[i].active && subs[i].handle == handle) {
            subs[i].active = false;
            GG_LOGD("Subscription closed for %s.", subs[i].name);
            break;
        }
    }
    pthread_mutex_unlock(&subs_mtx);
}

static bool is_terminal(GgBuffer state) {
    return gg_buffer_eq(state, GG_STR("RUNNING"))
        || gg_buffer_eq(state, GG_STR("FINISHED"))
        || gg_buffer_eq(state, GG_STR("BROKEN"));
}

static void *poll_loop(void *arg) {
    (void) arg;
    while (1) {
        usleep(500000); // 500ms
        pthread_mutex_lock(&subs_mtx);
        for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
            if (!subs[i].active) {
                continue;
            }
            GgBuffer name = { .data = (uint8_t *) subs[i].name,
                              .len = strlen(subs[i].name) };
            GgBuffer state = { 0 };
            GgError ret = s6_get_status(name, &state);
            if (ret != GG_ERR_OK) {
                continue;
            }
            if (is_terminal(state)) {
                ggl_sub_respond(
                    subs[i].handle,
                    gg_obj_map(GG_MAP(
                        gg_kv(GG_STR("component_name"), gg_obj_buf(name)),
                        gg_kv(GG_STR("lifecycle_state"), gg_obj_buf(state))
                    ))
                );
            }
        }
        pthread_mutex_unlock(&subs_mtx);
    }
    return NULL;
}

void subscriptions_init(void) {
    if (!poll_running) {
        pthread_create(&poll_thread, NULL, poll_loop, NULL);
        pthread_detach(poll_thread);
        poll_running = true;
    }
}

GgError subscriptions_add(GgBuffer component_name, uint32_t handle) {
    ggl_sub_accept(handle, sub_on_close, NULL);

    // Immediate response if already terminal
    GgBuffer state = { 0 };
    GgError ret = s6_get_status(component_name, &state);
    if (ret == GG_ERR_OK && is_terminal(state)) {
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
    pthread_mutex_lock(&subs_mtx);
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (!subs[i].active) {
            size_t copy_len
                = component_name.len < sizeof(subs[i].name) - 1
                ? component_name.len
                : sizeof(subs[i].name) - 1;
            memcpy(subs[i].name, component_name.data, copy_len);
            subs[i].name[copy_len] = '\0';
            subs[i].handle = handle;
            subs[i].active = true;
            pthread_mutex_unlock(&subs_mtx);
            return GG_ERR_OK;
        }
    }
    pthread_mutex_unlock(&subs_mtx);
    GG_LOGW("Subscription table full.");
    return GG_ERR_NOMEM;
}
