#ifndef DRAM_H
#define DRAM_H

#include "types.h"

/* Read timings based on the existing codename routes. layout identifies
 * the decoder used; DDR4 refresh availability reports userspace I/O only. */
void dram_read_timings(int codename_index, dram_timings_t *out);

/* Configured DDR4 FGR capability, never the instantaneous refresh mode. */
const char *dram_fgr_label(uint32_t code);

#endif
