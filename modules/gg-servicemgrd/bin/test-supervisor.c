// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include <gg/error.h>
#include <gg/log.h>
#include <gg/map.h>
#include <gg/object.h>
#include <gg/types.h>
#include <ggl/core_bus/client.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(
            stderr,
            "Usage: %s <method> <component_name> [version] [phase]\n",
            argv[0]
        );
        return 1;
    }
    const char *method = argv[1];
    const char *comp = argv[2];

    uint8_t alloc_mem[4096];
    GgArena alloc = gg_arena_init((GgBuffer) { .data = alloc_mem, .len = sizeof(alloc_mem) });
    GgObject result = GG_OBJ_NULL;
    GgError error = GG_ERR_OK;
    GgError ret;

    GgBuffer comp_buf
        = { .data = (uint8_t *) comp, .len = strlen(comp) };

    if (strcmp(method, "get_status") == 0) {
        ret = ggl_call(
            GG_STR("gg_supervisor"),
            GG_STR("get_status"),
            GG_MAP(
                gg_kv(GG_STR("component_name"), gg_obj_buf(comp_buf))
            ),
            &error,
            &alloc,
            &result
        );
        if (ret == GG_ERR_OK) {
            GgObject *state_obj;
            GgError vret = gg_map_validate(
                gg_obj_into_map(result),
                GG_MAP_SCHEMA(
                    { GG_STR("lifecycle_state"),
                      GG_REQUIRED,
                      GG_TYPE_BUF,
                      &state_obj },
                )
            );
            if (vret == GG_ERR_OK) {
                GgBuffer state = gg_obj_into_buf(*state_obj);
                printf("%.*s\n", (int) state.len, state.data);
            }
        }
    } else if (strcmp(method, "start_component") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Need: name version phase\n");
            return 1;
        }
        GgBuffer ver
            = { .data = (uint8_t *) argv[3], .len = strlen(argv[3]) };
        GgBuffer phase
            = { .data = (uint8_t *) argv[4], .len = strlen(argv[4]) };
        ret = ggl_call(
            GG_STR("gg_supervisor"),
            GG_STR("start_component"),
            GG_MAP(
                gg_kv(GG_STR("component_name"), gg_obj_buf(comp_buf)),
                gg_kv(GG_STR("version"), gg_obj_buf(ver)),
                gg_kv(GG_STR("phase"), gg_obj_buf(phase))
            ),
            &error,
            &alloc,
            &result
        );
        if (ret == GG_ERR_OK) {
            printf("OK\n");
        }
    } else if (strcmp(method, "stop_component") == 0) {
        ret = ggl_call(
            GG_STR("gg_supervisor"),
            GG_STR("stop_component"),
            GG_MAP(
                gg_kv(GG_STR("component_name"), gg_obj_buf(comp_buf))
            ),
            &error,
            &alloc,
            &result
        );
        if (ret == GG_ERR_OK) {
            printf("OK\n");
        }
    } else {
        fprintf(stderr, "Unknown method: %s\n", method);
        return 1;
    }

    if (ret != GG_ERR_OK) {
        fprintf(stderr, "FAILED: ret=%d error=%d\n", ret, error);
        return 1;
    }
    return 0;
}
