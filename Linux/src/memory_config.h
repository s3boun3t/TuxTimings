#ifndef MEMORY_CONFIG_H
#define MEMORY_CONFIG_H
#include "dram.h"

/* Pure policy: SMBIOS observation, decoder route and PM clock stay distinct. */
void memory_config_resolve(memory_config_t *out, const memory_module_t *modules,
                           int count, bool inventory_complete,
                           const dram_timings_t *dram, float mclk_mhz);
const char *memory_type_label(mem_type_t type);
const char *memory_type_reason(memory_type_state_t state);
const char *memory_frequency_source(value_source_t source);
#endif
