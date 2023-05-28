#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "toml.h"

#define POWERSAVE "powersave"
#define PERFORMANCE "performance"

enum tuna_cpu_mode tuna_cpu_mode_from_str(const char *cpu_mode) {
    if (strcmp(cpu_mode, POWERSAVE) == 0) {
        return TUNA_CPU_MODE_POWERSAVE;
    } else if (strcmp(cpu_mode, PERFORMANCE) == 0) {
        return TUNA_CPU_MODE_PERFORMANCE;
    }
    return TUNA_CPU_MODE_DEFAULT;
}

const char *tuna_cpu_mode_to_str(enum tuna_cpu_mode cpu_mode) {
    switch (cpu_mode) {
    case TUNA_CPU_MODE_POWERSAVE:
        return POWERSAVE;
    case TUNA_CPU_MODE_PERFORMANCE:
        return PERFORMANCE;
    }
    return NULL;
}

struct tuna_profile *parse_profile(toml_table_t *table, bool is_default) {
    struct tuna_profile *profile;

    profile = malloc(sizeof(struct tuna_profile));
    if (is_default) {
        profile->name = NULL;
    } else {
        toml_datum_t toml_name = toml_string_in(table, "name");
        if (!toml_name.ok) {
            free(profile);
            return NULL;
        }
        profile->name = toml_name.u.s;
    }

    toml_datum_t toml_default_cpu_mode = toml_string_in(table, "cpu_mode");
    if (!toml_default_cpu_mode.ok) {
        free((void *)profile->name);
        free(profile);
        return NULL;
    }
    profile->cpu_mode = tuna_cpu_mode_from_str(toml_default_cpu_mode.u.s);
    free(toml_default_cpu_mode.u.s);

    return profile;
}

void tuna_config_from_file(struct tuna_config *config, const char *path) {
    toml_table_t *toml;
    {
        char errbuf[256] = {0};

        FILE *fp = fopen(path, "r");
        toml = toml_parse_file(fp, errbuf, sizeof(errbuf));
        fclose(fp);

        if (!toml) {
            fprintf(stderr, "Error: %s\n", errbuf);
            exit(EXIT_FAILURE);
        }
    }

    toml_table_t *toml_default_profile = toml_table_in(toml, "default");
    config->default_profile = parse_profile(toml_default_profile, true);
    if (!config->default_profile) {
        fprintf(stderr, "Error: no default profile found\n");
        exit(EXIT_FAILURE);
    }

    config->profiles_len = 0;
    config->profiles = NULL;

    toml_array_t *apps = toml_array_in(toml, "apps");
    if (!apps) {
        printf("Warn: no apps found\n");
        return;
    }

    while (toml_table_at(apps, config->profiles_len)) {
        config->profiles_len += 1;
    }

    config->profiles =
        malloc(sizeof(struct tuna_profile) * config->profiles_len);

    for (size_t i = 0; i < config->profiles_len; i++) {
        toml_table_t *toml_profile = toml_table_at(apps, i);
        config->profiles[i] = parse_profile(toml_profile, false);
        assert(config->profiles[i]);
    }

    toml_free(toml);
}

void tuna_config_destroy(struct tuna_config *config) {
    free(config->default_profile);

    for (size_t i = 0; i < config->profiles_len; i++) {
        struct tuna_profile *profile =
            (struct tuna_profile *)config->profiles[i];

        free((char *)profile->name);
        free(profile);
    }
    free(config->profiles);
}

void tuna_config_print(struct tuna_config *config) {
    printf("=====[Default]=====\n");
    printf("CPU mode: %d\n", config->default_profile->cpu_mode);

    for (size_t i = 0; i < config->profiles_len; i++) {
        struct tuna_profile *profile = config->profiles[i];
        printf("\n=====[%s]=====\n", profile->name);
        printf("CPU mode: %d\n", profile->cpu_mode);
    }
}
