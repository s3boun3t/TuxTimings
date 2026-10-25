/* SMBIOS parser integration: injected output only; no commands or hardware. */
#ifdef NDEBUG
#error "This test suite requires assertions; remove -DNDEBUG"
#endif
#include "../src/backend.c"
#include <assert.h>

#define MODULE "Memory Device\n\tSize: 8 GB\n\tType: DDR4\n" \
               "\tSerial Number: 12345678\n\tPart Number: SAME-PART\n"
static const char *dmidecode_fixture;
static bool command_success;
static unsigned checked_calls, unchecked_calls;

static char *fixture_output(const char *cmd)
{
    assert(strstr(cmd, "dmidecode") && strstr(cmd, "-t memory"));
    char *copy = strdup(dmidecode_fixture);
    assert(copy);
    return copy;
}

char *run_command(const char *cmd)
{
    unchecked_calls++;
    return fixture_output(cmd);
}

char *run_command_checked(const char *cmd)
{
    checked_calls++;
    assert(strstr(cmd, "dmidecode") && strstr(cmd, "-t memory"));
    return command_success ? fixture_output(cmd) : NULL;
}

static void parse_fixture(const char *text, bool success)
{
    dmidecode_fixture = text;
    command_success = success;
    checked_calls = unchecked_calls = 0;
    s_module_count = 0;
    s_module_inventory_complete = true;
    memset(s_modules, 0, sizeof(s_modules));
    parse_dmidecode_memory();
}

static void test_types(void)
{
    parse_fixture("Memory Device\n\tSize: 8 GB\n\tType: DDR4\n"
                  "\tType Detail: Synchronous Unbuffered\n\tLocator: DIMM_A2\n"
                  "\tSerial Number: 00000000\n\tPart Number: SAME-PART\n", true);
    assert(s_module_count == 1 && s_modules[0].type == MEM_DDR4);
    assert(!s_modules[0].serial_number[0] && s_module_inventory_complete);
    assert(s_modules[0].capacity_bytes == 8ULL * 1024 * 1024 * 1024);
    assert(!strcmp(s_modules[0].capacity_display, "8.0 GiB"));
    parse_fixture("Memory Device\n\tSize: 16 GB\n\tType: DDR5\n"
                  "\tSerial Number: 12345678\n\tPart Number: DDR5-PART\n", true);
    assert(s_module_count == 1 && s_modules[0].type == MEM_DDR5);
    assert(!strcmp(s_modules[0].serial_number, "12345678"));
    parse_fixture("Memory Device\n\tSize: 8 GB\n\tType: LPDDR4\n", true);
    assert(s_module_count == 1 && s_modules[0].type == MEM_LPDDR4);
    parse_fixture("Memory Device\n\tSize: 8 GB\n\tType: Unknown\n", true);
    assert(s_module_count == 1 && s_modules[0].type == MEM_UNKNOWN);
}

static void test_sizes(void)
{
    const char *uncertain[] = {
        "\tSize: Unknown\n", "", "\tSize: invalid\n", "\tSize: 0 MB\n",
        "\tSize: Unknown No Module Installed\n", "\tSize: Not Installed?\n"
    };
    char text[2048], record[512];
    for (size_t i = 0; i < sizeof(uncertain) / sizeof(uncertain[0]); i++) {
        snprintf(record, sizeof(record), "Memory Device\n%s\tType: DDR4\n"
                 "\tSerial Number: 12345678\n\tPart Number: SAME-PART\n", uncertain[i]);
        /* The uncertain duplicate must invalidate first, middle and last records. */
        for (int position = 0; position < 3; position++) {
            snprintf(text, sizeof(text), "%s%s%s",
                     position == 0 ? record : MODULE,
                     position == 1 ? record : MODULE,
                     position == 2 ? record : MODULE);
            parse_fixture(text, true);
            assert(s_module_count == 2 && !s_module_inventory_complete);
        }
        parse_fixture(record, true);
        assert(!s_module_count && !s_module_inventory_complete);
    }
    const char *empty[] = { "No Module Installed", "Not Installed", " \tNo Module Installed \t\r" };
    for (size_t i = 0; i < sizeof(empty) / sizeof(empty[0]); i++) {
        snprintf(record, sizeof(record), "Memory Device\n\tSize: %s\n", empty[i]);
        snprintf(text, sizeof(text), "%s%s", record, MODULE);
        parse_fixture(text, true);
        assert(s_module_count == 1 && s_module_inventory_complete);
        parse_fixture(record, true);
        assert(!s_module_count && s_module_inventory_complete);
    }
}

static void test_commands(void)
{
    /* A valid-looking prefix from a failed command cannot establish uniqueness. */
    parse_fixture(MODULE, false);
    assert(!s_module_count && !s_module_inventory_complete);
    assert(checked_calls == 3 && !unchecked_calls);
    parse_fixture(MODULE, true);
    assert(s_module_count == 1 && s_module_inventory_complete);
    assert(checked_calls == 1 && !unchecked_calls);
}

static void test_overflow(void)
{
    char text[4096] = {0};
    for (int i = 0; i <= MAX_MODULES; i++) strcat(text, MODULE);
    parse_fixture(text, true);
    assert(s_module_count == MAX_MODULES && !s_module_inventory_complete);
}

int main(int argc, char **argv)
{
    if (argc == 1 || !strcmp(argv[1], "types")) test_types();
    if (argc == 1 || !strcmp(argv[1], "sizes")) test_sizes();
    if (argc == 1 || !strcmp(argv[1], "commands")) test_commands();
    if (argc == 1 || !strcmp(argv[1], "overflow")) test_overflow();
    puts("SMBIOS memory tests passed; no commands or hardware accessed");
    return 0;
}
