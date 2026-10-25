/* Command reader tests: stdio is replaced; no process is ever started. */
#ifdef NDEBUG
#error "This test suite requires assertions; remove -DNDEBUG"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *fixture_popen(const char *, const char *);
static size_t fixture_fread(void *, size_t, size_t, FILE *);
static int fixture_ferror(FILE *);
static int fixture_pclose(FILE *);
#define popen fixture_popen
#define fread fixture_fread
#define ferror fixture_ferror
#define pclose fixture_pclose
#include "../src/util.c"
#undef popen
#undef fread
#undef ferror
#undef pclose

static unsigned char stream_token;
#define STREAM ((FILE *)&stream_token)
static const char *output;
static size_t offset, read_chunk;
static int read_error, close_status, open_error;
static unsigned closes, error_checks;

static FILE *fixture_popen(const char *cmd, const char *mode)
{
    assert(strcmp(cmd, "fixture-command") == 0 && strcmp(mode, "r") == 0);
    return open_error ? NULL : STREAM;
}

static size_t fixture_fread(void *ptr, size_t size, size_t count, FILE *fp)
{
    assert(fp == STREAM && size == 1 && count > 0);
    size_t available = strlen(output) - offset;
    size_t n = available < count ? available : count;
    if (n > read_chunk) n = read_chunk;
    memcpy(ptr, output + offset, n);
    offset += n;
    return n;
}

static int fixture_ferror(FILE *fp)
{
    assert(fp == STREAM);
    error_checks++;
    return read_error;
}

static int fixture_pclose(FILE *fp)
{
    assert(fp == STREAM && closes == 0);
    closes++;
    return close_status;
}

static void reset(const char *text)
{
    output = text;
    offset = 0;
    read_chunk = 17;
    read_error = close_status = open_error = 0;
    closes = error_checks = 0;
}

static void test_success(void)
{
    reset("");
    char *out = run_command_checked("fixture-command");
    assert(out && !out[0] && closes == 1 && error_checks == 1);
    free(out);

    char large[12001];
    memset(large, 'x', sizeof(large) - 1);
    large[sizeof(large) - 1] = '\0';
    reset(large);
    out = run_command_checked("fixture-command");
    assert(out && strcmp(out, large) == 0 && closes == 1 && error_checks == 1);
    free(out);
}

static void test_status(void)
{
    const int failures[] = { 1 << 8, 15, -1 };
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        reset("partial SMBIOS inventory\n");
        close_status = failures[i];
        char *out = run_command_checked("fixture-command");
        assert(!out && closes == 1 && error_checks == 1);
    }
}

static void test_read_error(void)
{
    reset("partial SMBIOS inventory\n");
    read_error = 1;
    char *out = run_command_checked("fixture-command");
    assert(!out && closes == 1 && error_checks == 1);
}

static void test_unchecked(void)
{
    reset("best-effort output\n");
    read_error = 1;
    close_status = 1 << 8;
    char *out = run_command("fixture-command");
    assert(out && strcmp(out, output) == 0 && closes == 1 && !error_checks);
    free(out);
}

static void test_open_error(void)
{
    reset("");
    open_error = 1;
    assert(!run_command_checked("fixture-command") && !closes && !error_checks);
}

int main(int argc, char **argv)
{
    if (argc == 1 || !strcmp(argv[1], "success")) test_success();
    if (argc == 1 || !strcmp(argv[1], "status")) test_status();
    if (argc == 1 || !strcmp(argv[1], "read-error")) test_read_error();
    if (argc == 1 || !strcmp(argv[1], "unchecked")) test_unchecked();
    if (argc == 1 || !strcmp(argv[1], "open-error")) test_open_error();
    puts("Command reader tests passed; no processes started");
    return 0;
}
