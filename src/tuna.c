#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* used for checking if the config was changed*/
#include <sys/stat.h>

#include "config.h"

struct tuna_sys_info {
    enum tuna_cpu_mode cpu_mode;
    uint16_t core_count;
};

struct tuna_state {
    const char *config_file;
    struct tuna_config *config;
    struct tuna_sys_info *sys_info;
};

static int should_exit = 0;

static void exit_handler(int sig);
static void setup(struct tuna_state *state);
static void run(struct tuna_state *state);
static void cleanup(struct tuna_state *state);
static void update_check(struct tuna_state *state);

void exit_handler(int sig) {
    should_exit = 1;
    printf("\nExiting...\n");
    (void)sig;
}

void setup(struct tuna_state *state) {
    signal(SIGINT, exit_handler);

    struct tuna_config *config = malloc(sizeof(struct tuna_config));
    {
        printf("Using config file: %s\n", state->config_file);
        tuna_config_from_file(config, state->config_file);
    }
    state->config = config;

    struct tuna_sys_info *sys_info = malloc(sizeof(struct tuna_sys_info));
    {
        // TODO: this is awful. please fix
        char buf[64] = {0};
        FILE *cpu_present = fopen("/sys/devices/system/cpu/present", "r");
        fread_unlocked(buf, sizeof(buf), sizeof(buf) - 1, cpu_present);
        fclose(cpu_present);

        int len = strlen(buf);
        buf[len - 1] = '\0';
        char *cores = buf;
        cores += 2;

        sys_info->core_count = strtol(cores, (char **)NULL, 10) + 1;
        sys_info->cpu_mode = config->default_profile->cpu_mode;
    }
    state->sys_info = sys_info;
}

void run(struct tuna_state *state) {
    struct stat file_stat;
    struct timespec last_change;

    /* this is done because the config is already parsed from setup()*/
    stat(state->config_file, &file_stat);
    last_change = file_stat.st_mtim;

    while (!should_exit) {
        if (stat(state->config_file, &file_stat) > 0) {
            sleep(1);
            continue;
        }

        if (last_change.tv_nsec != file_stat.st_mtim.tv_nsec) {
            last_change = file_stat.st_mtim;
            tuna_config_destroy(state->config);
            tuna_config_from_file(state->config, state->config_file);
            printf("Config file updated\n");
        }

        time_t start = clock();
        update_check(state);
        printf("Time taken: %f\n", (double)(clock() - start) / CLOCKS_PER_SEC);
        sleep(2);
    }
}

void cleanup(struct tuna_state *state) {
    tuna_config_destroy(state->config);
    free(state->config);
    free(state->sys_info);
    free(state);
}

int is_process_running(const char *process_name) {
    char command[512] = {0};
    snprintf(command, sizeof(command), "pgrep %s > /dev/null", process_name);

    FILE *output = popen(command, "r");
    if (!output) {
        perror("popen");
        return 0;
    }

    int status = pclose(output);
    if (status == -1) {
        perror("pclose");
        return 0;
    }

    return status == 0;
}

// TODO: set performance governor only for each core the process is running on
void update_check(struct tuna_state *state) {
    enum tuna_cpu_mode cpu_mode = state->config->default_profile->cpu_mode;

    for (size_t i = 0; i < state->config->profiles_len; i++) {
        struct tuna_profile *profile = state->config->profiles[i];
        if (profile->cpu_mode > cpu_mode && is_process_running(profile->name)) {
            cpu_mode = profile->cpu_mode;
        }
    }

    const char *cpu_mode_str = tuna_cpu_mode_to_str(cpu_mode);
    int cpu_mode_str_len = strlen(cpu_mode_str);

    if (cpu_mode != state->sys_info->cpu_mode) {
        const char *from = tuna_cpu_mode_to_str(state->sys_info->cpu_mode);
        printf("cpu mode: %s -> %s\n", from, cpu_mode_str);
        for (uint16_t core = 0; core < state->sys_info->core_count; core++) {
            char path[256] = {0};
            snprintf(path, sizeof(path),
                     "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
                     core);
            FILE *cpu_file = fopen(path, "w");
            assert(cpu_file);
            fwrite(cpu_mode_str, sizeof(char), cpu_mode_str_len, cpu_file);
            fclose(cpu_file);
        }

        state->sys_info->cpu_mode = cpu_mode;
    }
}

int main(int argc, char **argv) {
    // TODO: use config file at /etc/tuna.toml instead
    if (argc != 2) {
        printf("Usage: %s <config>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *config_file = argv[1];
    if (access(config_file, R_OK) != 0) {
        printf("File does not exist or couldn't be read\n");
        return EXIT_FAILURE;
    }

    struct tuna_state *state = malloc(sizeof(struct tuna_state));
    state->config_file = config_file;

    setup(state);
    run(state);
    cleanup(state);

    return EXIT_SUCCESS;
}
