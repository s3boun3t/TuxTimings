#ifndef DIMM_SENSORS_H
#define DIMM_SENSORS_H

#include "types.h"
#include <stddef.h>

/* Minimal identity only: no timings, voltages, writes or SPD programming. */
typedef struct {
    mem_type_t type;
    char serial[9];
    char part[31];
    bool serial_valid, part_valid;
} spd_identity_t;

typedef struct dimm_sensors_cache dimm_sensors_cache_t;

bool dimm_spd_identity(const uint8_t *bytes, size_t length, spd_identity_t *out);
dimm_sensors_cache_t *dimm_sensors_create(void);
void dimm_sensors_destroy(dimm_sensors_cache_t *cache);
/* sysfs_root is supplied explicitly for synthetic tests; production uses /sys.
 * modules_complete must be false if SMBIOS was truncated by MAX_MODULES. */
void dimm_sensors_update(dimm_sensors_cache_t *cache, const char *sysfs_root,
                         memory_module_t *modules, int module_count,
                         bool modules_complete, smu_metrics_t *metrics);
const char *dimm_temperature_reason(dimm_temp_state_t state);

#endif
