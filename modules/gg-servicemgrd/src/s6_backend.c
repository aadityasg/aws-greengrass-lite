// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "s6_backend.h"
#include "component_table.h"
#include <gg/buffer.h>
#include <gg/error.h>
#include <gg/log.h>
#include <ggl/process.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static GgError build_service_dir(
    GgBuffer component_name, char *out, size_t out_len
) {
    int ret = snprintf(
        out,
        out_len,
        "%s/%s%.*s",
        S6_SCAN_DIR,
        S6_SERVICE_PREFIX,
        (int) component_name.len,
        component_name.data
    );
    if (ret < 0 || (size_t) ret >= out_len) {
        return GG_ERR_RANGE;
    }
    return GG_ERR_OK;
}

static GgError write_run_script(
    const char *service_dir,
    GgBuffer component_name,
    GgBuffer version,
    GgBuffer phase
) {
    char run_path[PATH_MAX];
    snprintf(run_path, sizeof(run_path), "%s/run", service_dir);

    FILE *f = fopen(run_path, "w");
    if (f == NULL) {
        GG_LOGE("Failed to create run script at %s.", run_path);
        return GG_ERR_FAILURE;
    }
    fprintf(
        f,
        "#!/bin/sh\n"
        "export AWS_GG_NUCLEUS_DOMAIN_SOCKET_FILEPATH_FOR_COMPONENT="
        "/var/lib/greengrass/gg-ipc.socket\n"
        "exec recipe-runner -n %.*s -v %.*s -p %.*s\n",
        (int) component_name.len,
        component_name.data,
        (int) version.len,
        version.data,
        (int) phase.len,
        phase.data
    );
    fclose(f);
    chmod(run_path, 0755);

    // Write finish script: stop restarting on clean exit (exit code 0)
    char finish_path[PATH_MAX];
    snprintf(finish_path, sizeof(finish_path), "%s/finish", service_dir);
    FILE *ff = fopen(finish_path, "w");
    if (ff != NULL) {
        fprintf(
            ff,
            "#!/bin/sh\n"
            "# If component exited cleanly (exit 0), don't restart\n"
            "if [ \"$1\" = \"0\" ]; then\n"
            "  s6-svc -O %s\n"
            "fi\n",
            service_dir
        );
        fclose(ff);
        chmod(finish_path, 0755);
    }
    return GG_ERR_OK;
}

static GgError s6_rescan(void) {
    const char *argv[] = { "s6-svscanctl", "-a", S6_SCAN_DIR, NULL };
    return ggl_process_call(argv, NULL);
}

static GgError wait_for_supervise(const char *service_dir) {
    char status_path[PATH_MAX];
    snprintf(status_path, sizeof(status_path), "%s/supervise/status", service_dir);
    for (int i = 0; i < 20; i++) {
        if (access(status_path, F_OK) == 0) {
            return GG_ERR_OK;
        }
        usleep(100000); // 100ms
    }
    GG_LOGE("Timed out waiting for supervise dir in %s.", service_dir);
    return GG_ERR_FAILURE;
}

GgError s6_start_component(
    GgBuffer component_name, GgBuffer version, GgBuffer phase
) {
    char service_dir[PATH_MAX];
    GgError ret = build_service_dir(component_name, service_dir, sizeof(service_dir));
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Create service directory with down file (don't auto-start yet)
    mkdir(service_dir, 0755);

    char down_path[PATH_MAX];
    snprintf(down_path, sizeof(down_path), "%s/down", service_dir);
    FILE *df = fopen(down_path, "w");
    if (df != NULL) {
        fclose(df);
    }

    ret = write_run_script(service_dir, component_name, version, phase);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Tell s6-svscan to pick up the new service directory
    ret = s6_rescan();
    if (ret != GG_ERR_OK) {
        GG_LOGE("s6-svscanctl rescan failed.");
        return ret;
    }

    // Wait for s6-supervise to create the supervise/ directory
    ret = wait_for_supervise(service_dir);
    if (ret != GG_ERR_OK) {
        return ret;
    }

    // Remove down file and bring service up
    unlink(down_path);
    const char *up_argv[] = { "s6-svc", "-u", service_dir, NULL };
    ret = ggl_process_call(up_argv, NULL);
    if (ret != GG_ERR_OK) {
        GG_LOGE("s6-svc -u failed for %s.", service_dir);
        return ret;
    }

    GG_LOGI(
        "Started component %.*s via s6 at %s.",
        (int) component_name.len,
        component_name.data,
        service_dir
    );
    component_table_add(component_name);
    return GG_ERR_OK;
}

GgError s6_stop_component(GgBuffer component_name) {
    char service_dir[PATH_MAX];
    GgError ret = build_service_dir(component_name, service_dir, sizeof(service_dir));
    if (ret != GG_ERR_OK) {
        return ret;
    }

    if (access(service_dir, F_OK) != 0) {
        GG_LOGD(
            "Service dir %s does not exist, nothing to stop.", service_dir
        );
        return GG_ERR_OK;
    }

    // Bring service down
    const char *down_argv[] = { "s6-svc", "-d", service_dir, NULL };
    (void) ggl_process_call(down_argv, NULL);

    // Exit the supervisor
    const char *exit_argv[] = { "s6-svc", "-x", service_dir, NULL };
    (void) ggl_process_call(exit_argv, NULL);

    // Brief wait for supervisor to exit
    usleep(500000);

    // Remove service directory
    const char *rm_argv[] = { "rm", "-rf", service_dir, NULL };
    ret = ggl_process_call(rm_argv, NULL);
    if (ret != GG_ERR_OK) {
        GG_LOGE("Failed to remove service dir %s.", service_dir);
        return ret;
    }

    // Rescan
    ret = s6_rescan();

    GG_LOGI(
        "Stopped component %.*s, removed %s.",
        (int) component_name.len,
        component_name.data,
        service_dir
    );
    component_table_remove(component_name);
    return GG_ERR_OK;
}

GgError s6_get_status(GgBuffer component_name, GgBuffer *lifecycle_state) {
    char service_dir[PATH_MAX];
    GgError ret
        = build_service_dir(component_name, service_dir, sizeof(service_dir));
    if (ret != GG_ERR_OK) {
        return ret;
    }

    if (access(service_dir, F_OK) != 0) {
        *lifecycle_state = GG_STR("INSTALLED");
        return GG_ERR_OK;
    }

    // Use s6-svstat parseable output: "up pid exitcode signal wantedup"
    // We capture output by reading from a pipe
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return GG_ERR_FAILURE;
    }

    pid_t child = fork();
    if (child == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp(
            "s6-svstat",
            "s6-svstat",
            "-o",
            "up,pid,exitcode,signal,wantedup",
            service_dir,
            NULL
        );
        _exit(1);
    }
    close(pipefd[1]);

    char buf[256] = { 0 };
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);
    waitpid(child, NULL, 0);

    if (n <= 0) {
        *lifecycle_state = GG_STR("INSTALLED");
        return GG_ERR_OK;
    }
    buf[n] = '\0';

    // Parse: "true 175 -1 NA true\n" or "false -1 0 NA false\n"
    char up_str[8] = { 0 };
    int pid_val = -1;
    int exitcode = -1;
    char signal_str[16] = { 0 };
    char wantedup_str[8] = { 0 };
    sscanf(buf, "%7s %d %d %15s %7s", up_str, &pid_val, &exitcode, signal_str, wantedup_str);

    bool is_up = (strcmp(up_str, "true") == 0);
    bool wanted_up = (strcmp(wantedup_str, "true") == 0);

    struct component_entry *entry = component_table_get(component_name);

    if (is_up && pid_val > 0) {
        // Detect restart by PID change
        if (entry != NULL) {
            if (entry->last_pid > 0 && entry->last_pid != pid_val) {
                component_table_record_exit(component_name);
                if (entry->restart_count >= RESTART_LIMIT) {
                    *lifecycle_state = GG_STR("BROKEN");
                    return GG_ERR_OK;
                }
            }
            entry->last_pid = pid_val;
            entry->was_up = true;
        }
        *lifecycle_state = GG_STR("RUNNING");
        return GG_ERR_OK;
    }

    // Service is down
    if (entry != NULL) {
        if (entry->was_up) {
            // Transitioned from up to down — record exit
            component_table_record_exit(component_name);
            entry->was_up = false;
        }
        if (entry->restart_count >= RESTART_LIMIT) {
            *lifecycle_state = GG_STR("BROKEN");
            return GG_ERR_OK;
        }
    }

    if (wanted_up) {
        // s6 will restart it — it's in an error/restart cycle
        *lifecycle_state = GG_STR("ERRORED");
        return GG_ERR_OK;
    }

    // Stopped intentionally
    if (exitcode == 0) {
        *lifecycle_state = GG_STR("FINISHED");
    } else {
        *lifecycle_state = GG_STR("ERRORED");
    }
    return GG_ERR_OK;
}
