// Sensor simulator for GenAI edge-to-cloud demo
// Publishes simulated telemetry to IoT Core. Reads "mode" from config
// to switch between normal (temp ramps up) and low-power (temp drops).

#include <gg/error.h>
#include <gg/ipc/client.h>
#include <gg/object.h>
#include <gg/sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

static volatile int running = 1;
static int low_power = 0;
static double temp = 25.0;

static void sighandler(int sig) {
    (void) sig;
    running = 0;
}

static void on_config_update(
    void *ctx,
    GgBuffer component_name,
    GgList key_path,
    GgIpcSubscriptionHandle handle
) {
    (void) ctx;
    (void) component_name;
    (void) key_path;
    (void) handle;

    uint8_t mem[256];
    GgObject value;
    GgError err = ggipc_get_config(
        GG_BUF_LIST(GG_STR("mode")), NULL, GG_BUF(mem), &value
    );
    if (err == GG_ERR_OK && gg_obj_type(value) == GG_TYPE_BUF) {
        GgBuffer mode = gg_obj_into_buf(value);
        low_power = (mode.len == 9 && memcmp(mode.data, "low-power", 9) == 0);
        fprintf(stderr, "Config update: mode=%.*s low_power=%d\n",
            (int) mode.len, mode.data, low_power);
    }
}

int main(void) {
    gg_sdk_init();

    GgError err = ggipc_connect();
    if (err != GG_ERR_OK) {
        fprintf(stderr, "Failed to connect to IPC.\n");
        return 1;
    }

    // Read initial mode config
    uint8_t mem[256];
    GgObject value;
    err = ggipc_get_config(
        GG_BUF_LIST(GG_STR("mode")), NULL, GG_BUF(mem), &value
    );
    if (err == GG_ERR_OK && gg_obj_type(value) == GG_TYPE_BUF) {
        GgBuffer mode = gg_obj_into_buf(value);
        low_power = (mode.len == 9 && memcmp(mode.data, "low-power", 9) == 0);
    }

    // Subscribe to config updates
    GgIpcSubscriptionHandle handle;
    (void) ggipc_subscribe_to_configuration_update(
        NULL, GG_BUF_LIST(GG_STR("mode")),
        on_config_update, NULL, &handle
    );

    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);

    fprintf(stderr, "Sensor sim started. mode=%s\n",
        low_power ? "low-power" : "normal");

    while (running) {
        if (low_power) {
            if (temp > 30.0) temp -= 2.0;
        } else {
            if (temp < 45.0) temp += 0.5;
        }

        double humidity = 40.0 + (temp - 25.0) * 0.5;
        time_t now = time(NULL);

        char payload[256];
        snprintf(payload, sizeof(payload),
            "{\"temp\":%.1f,\"humidity\":%.1f,\"mode\":\"%s\","
            "\"timestamp\":%ld,\"device_id\":\"cam-3\"}",
            temp, humidity, low_power ? "low-power" : "normal", (long) now);

        GgBuffer topic = GG_STR("ggl/demo/cam-3/telemetry");
        GgBuffer msg = { .data = (uint8_t *) payload, .len = strlen(payload) };

        err = ggipc_publish_to_iot_core(topic, msg, 0);
        if (err == GG_ERR_OK) {
            fprintf(stderr, "Published: %s\n", payload);
        } else {
            fprintf(stderr, "Publish failed: %d\n", err);
        }

        sleep(2);
    }
}
