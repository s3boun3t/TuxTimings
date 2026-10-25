# SPDX-License-Identifier: GPL-2.0
obj-m := tuxtimings_aod_probe.o
KDIR ?= /lib/modules/$(shell uname -r)/build
HOSTCC ?= cc
HOSTCFLAGS ?= -O2 -g -Wall -Wextra -Werror

.PHONY: all check clean
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

check:
	@set -eu; probe_test=$$(mktemp /tmp/tuxtimings-aod-probe-test.XXXXXX); \
	trap 'rm -f "$$probe_test"' EXIT; \
	$(HOSTCC) $(HOSTCFLAGS) test_protocol.c -o "$$probe_test"; \
	"$$probe_test"

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
