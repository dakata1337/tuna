#ifndef _TUNA_CONFIG_H_
#define _TUNA_CONFIG_H_

#include <stddef.h>

#define TUNA_CPU_MODE_DEFAULT (enum tuna_cpu_mode)0

enum tuna_cpu_mode {
    TUNA_CPU_MODE_POWERSAVE,
    TUNA_CPU_MODE_PERFORMANCE,
};

struct tuna_profile {
    const char *name;
    enum tuna_cpu_mode cpu_mode;
};

struct tuna_config {
    struct tuna_profile *default_profile;
    struct tuna_profile **profiles;
    size_t profiles_len;
};

enum tuna_cpu_mode tuna_cpu_mode_from_str(const char *cpu_mode);
const char *tuna_cpu_mode_to_str(enum tuna_cpu_mode cpu_mode);
void tuna_config_from_file(struct tuna_config *config, const char *path);
void tuna_config_destroy(struct tuna_config *config);
void tuna_config_print(struct tuna_config *config);

#endif /* _TUNA_CONFIG_H_ */
