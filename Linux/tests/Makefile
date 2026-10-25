CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror
WRAPS = fopen fwrite fread __fread_chk fflush fseek rewind fclose
LDFLAGS += $(foreach f,$(WRAPS),-Wl,--wrap=$(f))
.PHONY: check clean
check: test_dram_refresh test_pm_vermeer test_dimm_sensors test_backend_memory test_run_command
	./test_dram_refresh
	./test_pm_vermeer
	./test_dimm_sensors
	./test_backend_memory
	./test_run_command
test_dram_refresh: Makefile test_dram_refresh.c ../src/dram.c ../src/util.c ../src/types.h ../src/dram.h ../src/util.h
	$(CC) $(CFLAGS) -I../src test_dram_refresh.c ../src/dram.c ../src/util.c $(LDFLAGS) -lm -o $@
test_pm_vermeer: test_pm_vermeer.c ../src/pm_table.c ../src/pm_table.h ../src/types.h
	$(CC) $(CFLAGS) -I../src test_pm_vermeer.c ../src/pm_table.c -lm -o $@
test_dimm_sensors: test_dimm_sensors.c ../src/dimm_sensors.c ../src/dimm_sensors.h ../src/types.h
	$(CC) $(CFLAGS) -I../src test_dimm_sensors.c ../src/dimm_sensors.c -Wl,--wrap=readdir -Wl,--wrap=closedir -o $@
test_backend_memory: test_backend_memory.c ../src/backend.c ../src/backend.h ../src/dimm_sensors.h ../src/types.h ../src/util.h
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -I../src test_backend_memory.c -Wl,--gc-sections -lm -o $@
test_run_command: test_run_command.c ../src/util.c ../src/util.h
	$(CC) $(CFLAGS) -I../src test_run_command.c -o $@
clean:
	rm -f test_dram_refresh test_pm_vermeer test_dimm_sensors test_backend_memory test_run_command

# Optional: requires a working GTK display; all backend data is synthetic.
.PHONY: check-ui
check-ui:
	@set -eu; ui_test=$$(mktemp /tmp/tuxtimings-ui-test.XXXXXX); \
	ui_captures=$$(mktemp -d /tmp/tuxtimings-ui-captures.XXXXXX); \
	trap 'rm -f "$$ui_test"; rm -rf "$$ui_captures"' EXIT; \
	$(CC) $(CFLAGS) $$(pkg-config --cflags gtk4) test_ui.c ../src/dram.c ../src/util.c ../src/dimm_sensors.c $$(pkg-config --libs gtk4) -lm -o "$$ui_test"; \
	for scenario in ddr4 ddr5 unknown unavailable ratio fgr5 associated temp-zero temp-error temp-missing temp-ambiguous; do \
	    GSK_RENDERER=cairo "$$ui_test" "$$scenario" "$$ui_captures/$$scenario.png"; \
	done
