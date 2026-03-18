// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "component_table.h"
#include <gg/buffer.h>
#include <gg/log.h>
#include <string.h>
#include <time.h>

static struct component_entry table[MAX_COMPONENTS];

void component_table_add(GgBuffer name) {
    // Check if already exists
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (table[i].active
            && strncmp(table[i].name, (const char *) name.data, name.len) == 0
            && table[i].name[name.len] == '\0') {
            table[i].restart_count = 0;
            table[i].restart_window_start = time(NULL);
            table[i].was_up = false;
            return;
        }
    }
    // Find empty slot
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (!table[i].active) {
            size_t copy_len
                = name.len < sizeof(table[i].name) - 1
                ? name.len
                : sizeof(table[i].name) - 1;
            memcpy(table[i].name, name.data, copy_len);
            table[i].name[copy_len] = '\0';
            table[i].active = true;
            table[i].restart_count = 0;
            table[i].restart_window_start = time(NULL);
            table[i].was_up = false;
            return;
        }
    }
    GG_LOGW("Component table full, cannot track %.*s.", (int) name.len, name.data);
}

void component_table_remove(GgBuffer name) {
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (table[i].active
            && strncmp(table[i].name, (const char *) name.data, name.len) == 0
            && table[i].name[name.len] == '\0') {
            table[i].active = false;
            return;
        }
    }
}

struct component_entry *component_table_get(GgBuffer name) {
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (table[i].active
            && strncmp(table[i].name, (const char *) name.data, name.len) == 0
            && table[i].name[name.len] == '\0') {
            return &table[i];
        }
    }
    return NULL;
}

void component_table_record_exit(GgBuffer name) {
    struct component_entry *entry = component_table_get(name);
    if (entry == NULL) {
        return;
    }
    time_t now = time(NULL);
    if (now - entry->restart_window_start > RESTART_WINDOW_SEC) {
        entry->restart_count = 0;
        entry->restart_window_start = now;
    }
    entry->restart_count++;
    GG_LOGD(
        "Component %s exit recorded, restart_count=%d.",
        entry->name,
        entry->restart_count
    );
}

void component_table_reset_counters(GgBuffer name) {
    struct component_entry *entry = component_table_get(name);
    if (entry != NULL) {
        entry->restart_count = 0;
        entry->restart_window_start = time(NULL);
    }
}
