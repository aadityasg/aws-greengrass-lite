// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <gg/buffer.h>
#include <gg/error.h>
#include <stdint.h>

#define MAX_SUBSCRIPTIONS 10

GgError subscriptions_add(GgBuffer component_name, uint32_t handle);
void subscriptions_init(void);

#endif
