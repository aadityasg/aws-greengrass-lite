// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "s6_backend.h"
#include <servicemgrd.h>
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/server.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static GgError start_component(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgObject *version_obj;
    GgObject *phase_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("component_name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj },
            { GG_STR("version"), GG_REQUIRED, GG_TYPE_BUF, &version_obj },
            { GG_STR("phase"), GG_REQUIRED, GG_TYPE_BUF, &phase_obj },
        )
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE("start_component received invalid arguments.");
        return GG_ERR_INVALID;
    }

    GgBuffer name = gg_obj_into_buf(*name_obj);
    GgBuffer version = gg_obj_into_buf(*version_obj);
    GgBuffer phase = gg_obj_into_buf(*phase_obj);

    ret = s6_start_component(name, version, phase);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    ggl_respond(handle, GG_OBJ_NULL);
    return GG_ERR_OK;
}

static GgError stop_component(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("component_name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj },
        )
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE("stop_component received invalid arguments.");
        return GG_ERR_INVALID;
    }

    GgBuffer name = gg_obj_into_buf(*name_obj);

    ret = s6_stop_component(name);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    ggl_respond(handle, GG_OBJ_NULL);
    return GG_ERR_OK;
}

static GgError get_status(void *ctx, GgMap params, uint32_t handle) {
    (void) ctx;
    GgObject *name_obj;
    GgError ret = gg_map_validate(
        params,
        GG_MAP_SCHEMA(
            { GG_STR("component_name"), GG_REQUIRED, GG_TYPE_BUF, &name_obj },
        )
    );
    if (ret != GG_ERR_OK) {
        GG_LOGE("get_status received invalid arguments.");
        return GG_ERR_INVALID;
    }

    GgBuffer name = gg_obj_into_buf(*name_obj);
    GgBuffer state = { 0 };
    ret = s6_get_status(name, &state);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    ggl_respond(
        handle,
        gg_obj_map(GG_MAP(
            gg_kv(GG_STR("lifecycle_state"), gg_obj_buf(state))
        ))
    );
    return GG_ERR_OK;
}

static GgError subscribe_to_lifecycle(
    void *ctx, GgMap params, uint32_t handle
) {
    (void) ctx;
    (void) params;
    (void) handle;
    GG_LOGW("subscribe_to_lifecycle not yet implemented.");
    return GG_ERR_UNSUPPORTED;
}

GgError run_servicemgrd(void) {
    static GglRpcMethodDesc handlers[]
        = { { GG_STR("start_component"), false, start_component, NULL },
            { GG_STR("stop_component"), false, stop_component, NULL },
            { GG_STR("get_status"), false, get_status, NULL },
            { GG_STR("subscribe_to_lifecycle"),
              true,
              subscribe_to_lifecycle,
              NULL } };
    static const size_t HANDLERS_LEN = sizeof(handlers) / sizeof(handlers[0]);

    GG_LOGI("gg-servicemgrd starting coreBus listener on gg_supervisor.");
    GgError ret = ggl_listen(GG_STR("gg_supervisor"), handlers, HANDLERS_LEN);
    GG_LOGE("Exiting with error %u.", (unsigned) ret);

    return GG_ERR_FAILURE;
}
