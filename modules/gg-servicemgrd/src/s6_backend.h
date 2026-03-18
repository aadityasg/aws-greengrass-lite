// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef S6_BACKEND_H
#define S6_BACKEND_H

#include <gg/buffer.h>
#include <gg/error.h>

#define S6_SCAN_DIR "/run/s6-services"
#define S6_SERVICE_PREFIX "ggl."

GgError s6_start_component(
    GgBuffer component_name, GgBuffer version, GgBuffer phase
);

GgError s6_stop_component(GgBuffer component_name);

#endif
