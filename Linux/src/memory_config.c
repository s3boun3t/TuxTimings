#include "memory_config.h"
#include <math.h>
#include <string.h>

const char *memory_type_label(mem_type_t type)
{
    switch (type) {
    case MEM_DDR4: return "DDR4";
    case MEM_DDR5: return "DDR5";
    case MEM_LPDDR4: return "LPDDR4";
    case MEM_LPDDR5: return "LPDDR5";
    default: return "—";
    }
}

const char *memory_type_reason(memory_type_state_t state)
{
    switch (state) {
    case MEMORY_TYPE_CONSISTENT: return "Memory type reported consistently by SMBIOS.";
    case MEMORY_TYPE_INCOMPLETE: return "Memory type unavailable: incomplete SMBIOS inventory.";
    case MEMORY_TYPE_CONFLICT: return "Memory type unavailable: conflicting SMBIOS module types.";
    default: return "Memory type unavailable: no known SMBIOS module type.";
    }
}

const char *memory_frequency_source(value_source_t source)
{
    switch (source) {
    case VALUE_SOURCE_SMN: return "UMC0 ratio (SMN userspace I/O; freshness unverified)";
    case VALUE_SOURCE_PM: return "PM MCLK × 2 (derived DDR rate)";
    case VALUE_SOURCE_SMBIOS: return "SMBIOS configured speed (not a live measurement)";
    default: return "Frequency unavailable";
    }
}

static void set_rate(memory_config_t *out, float rate, value_source_t source)
{
    if (!isfinite(rate) || rate <= 0.0f) return;
    out->frequency = rate;
    out->frequency_info = (value_info_t){true, source, VALUE_UNIT_MTS};
}

void memory_config_resolve(memory_config_t *out, const memory_module_t *modules,
                           int count, bool inventory_complete,
                           const dram_timings_t *dram, float mclk_mhz)
{
    memset(out, 0, sizeof(*out));
    bool complete = inventory_complete;
    if (count < 0 || count > MAX_MODULES || (count && !modules)) {
        complete = false;
        count = 0;
    }
    mem_type_t known = MEM_UNKNOWN;
    bool conflict = false;
    for (int i = 0; i < count; i++) {
        mem_type_t type = modules[i].type;
        if (type < MEM_DDR4 || type > MEM_LPDDR5) { complete = false; continue; }
        if (known != MEM_UNKNOWN && known != type) conflict = true;
        known = type;
    }
    if (conflict) out->type_state = MEMORY_TYPE_CONFLICT;
    else if (!complete) out->type_state = MEMORY_TYPE_INCOMPLETE;
    else if (known == MEM_UNKNOWN) out->type_state = MEMORY_TYPE_UNKNOWN;
    else out->type_state = MEMORY_TYPE_CONSISTENT;
    if (out->type_state == MEMORY_TYPE_CONSISTENT) out->type = known;
    if (conflict) return;

    /* A route supplies a decoded rate, never a claim about the physical type.
     * LPDDR and a known type conflicting with the route cannot use its ratio. */
    bool route_matches = known == MEM_UNKNOWN ||
        (known == MEM_DDR4 && dram->layout == DRAM_LAYOUT_DDR4) ||
        (known == MEM_DDR5 && dram->layout == DRAM_LAYOUT_DDR5);
    if (dram->layout != DRAM_LAYOUT_NONE && route_matches &&
        dram->data_rate_info.available && dram->data_rate_info.unit == VALUE_UNIT_MTS) {
        set_rate(out, dram->data_rate_mts, VALUE_SOURCE_SMN);
        if (out->frequency_info.available) return;
    }

    /* Require a complete, concordant inventory rather than picking the maximum
     * of unrelated module speeds. Legacy dmidecode MHz labels are ambiguous. */
    if (inventory_complete && count > 0) {
        uint32_t rate = modules[0].configured_speed;
        bool consistent = rate > 0;
        for (int i = 0; i < count; i++)
            if (modules[i].configured_speed_unit != VALUE_UNIT_MTS ||
                modules[i].configured_speed != rate) consistent = false;
        if (consistent) {
            set_rate(out, (float)rate, VALUE_SOURCE_SMBIOS);
            return;
        }
    }
    if (out->type == MEM_DDR4 || out->type == MEM_DDR5)
        set_rate(out, mclk_mhz * 2.0f, VALUE_SOURCE_PM);
}
