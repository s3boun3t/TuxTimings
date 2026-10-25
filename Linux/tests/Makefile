CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror
WRAPS = fopen fwrite fread __fread_chk fflush fseek rewind fclose
LDFLAGS += $(foreach f,$(WRAPS),-Wl,--wrap=$(f))
.PHONY: check clean
check: test_dram_refresh test_pm_vermeer test_dimm_sensors test_backend_memory test_run_command test_memory_config test_aod_aml test_bench_portability
	./test_dram_refresh
	./test_pm_vermeer
	./test_dimm_sensors
	./test_backend_memory
	./test_run_command
	./test_memory_config
	./test_aod_aml
	./test_bench_portability
	python3 test_delivery.py
	$(MAKE) -C ../tools/aod-wmi-probe check
	python3 ../tools/ddr4-refresh/test_capture_refresh.py
test_dram_refresh: Makefile test_dram_refresh.c ../src/dram.c ../src/util.c ../src/types.h ../src/dram.h ../src/util.h
	$(CC) $(CFLAGS) -I../src test_dram_refresh.c ../src/dram.c ../src/util.c $(LDFLAGS) -lm -o $@
test_pm_vermeer: test_pm_vermeer.c ../src/pm_table.c ../src/pm_table.h ../src/types.h
	$(CC) $(CFLAGS) -I../src test_pm_vermeer.c ../src/pm_table.c -lm -o $@
test_dimm_sensors: test_dimm_sensors.c ../src/dimm_sensors.c ../src/dimm_sensors.h ../src/types.h
	$(CC) $(CFLAGS) -I../src test_dimm_sensors.c ../src/dimm_sensors.c -Wl,--wrap=readdir -Wl,--wrap=closedir -o $@
test_backend_memory: test_backend_memory.c ../src/backend.c ../src/backend.h ../src/dimm_sensors.h ../src/types.h ../src/util.h ../src/memory_config.c ../src/memory_config.h
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -I../src test_backend_memory.c ../src/memory_config.c -Wl,--gc-sections -lm -o $@
test_run_command: test_run_command.c ../src/util.c ../src/util.h
	$(CC) $(CFLAGS) -I../src test_run_command.c -o $@
test_memory_config: test_memory_config.c ../src/memory_config.c ../src/memory_config.h ../src/dram.c ../src/dram.h ../src/types.h ../src/util.c
	$(CC) $(CFLAGS) -I../src test_memory_config.c ../src/memory_config.c ../src/dram.c ../src/util.c -lm -o $@
test_aod_aml: test_aod_aml.c ../src/aod-voltages/aod_aml.h
	$(CC) $(CFLAGS) -I../src test_aod_aml.c -o $@
test_bench_portability: test_bench_portability.c ../src/bench.c ../src/bench.h ../src/tuxbench/tuxbench.h
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -I../src test_bench_portability.c -Wl,--gc-sections -lpthread -lm -o $@
clean:
	rm -f test_dram_refresh test_pm_vermeer test_dimm_sensors test_backend_memory test_run_command test_memory_config test_aod_aml test_bench_portability

# Optional: requires a working GTK display; all backend data is synthetic.
.PHONY: check-ui
check-ui:
	@set -eu; ui_test=$$(mktemp /tmp/tuxtimings-ui-test.XXXXXX); \
	trap 'rm -f "$$ui_test"' EXIT; \
	$(CC) $(CFLAGS) -I../src $$(pkg-config --cflags gtk4) test_ui.c ../src/dram.c ../src/util.c ../src/dimm_sensors.c ../src/memory_config.c $$(pkg-config --libs gtk4) -lm -o "$$ui_test"; \
	bash ./run-ui.sh "$$ui_test"
