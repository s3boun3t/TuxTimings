#ifndef DRAM_H
#define DRAM_H

#include "types.h"

/* Read timings based on the existing codename routes. layout identifies
 * the decoder used; DDR4 refresh availability reports userspace I/O only. */
void dram_read_timings(int codename_index, dram_timings_t *out);

/* Known decoder route, independent of physical SMBIOS memory type. */
dram_layout_t dram_layout_for_codename(int codename_index);
static inline bool dram_has(const dram_timings_t *d, uint32_t dependencies)
{
    return d->layout != DRAM_LAYOUT_NONE &&
           (d->available & dependencies) == dependencies;
}

/* Configured DDR4 FGR capability, never the instantaneous refresh mode. */
const char *dram_fgr_label(uint32_t code);

#endif
