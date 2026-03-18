// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef COMPONENT_TABLE_H
#define COMPONENT_TABLE_H

#include <gg/buffer.h>
#include <stdbool.h>
#include <time.h>

#define MAX_COMPONENTS 64
#define RESTART_WINDOW_SEC 3600
#define RESTART_LIMIT 3

struct component_entry {
    char name[128];
    bool active;
    int restart_count;
    time_t restart_window_start;
    bool was_up;
};

void component_table_add(GgBuffer name);
void component_table_remove(GgBuffer name);
struct component_entry *component_table_get(GgBuffer name);
void component_table_record_exit(GgBuffer name);
void component_table_reset_counters(GgBuffer name);

#endif
