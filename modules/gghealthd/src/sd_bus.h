// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef GGHEALTHD_SD_BUS_H
#define GGHEALTHD_SD_BUS_H

#include <gg/buffer.h>
#include <gg/error.h>

// PoC: These functions now call gg_supervisor via coreBus instead of sd_bus.

GgError get_lifecycle_state(GgBuffer component_name, GgBuffer *state);
GgError restart_component(GgBuffer component_name);
GgError reset_restart_counters(GgBuffer component_name);

#endif
