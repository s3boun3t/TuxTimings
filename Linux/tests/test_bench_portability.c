/* Exercise instruction selection only; no flush or benchmark is executed. */
#include "../src/bench.c"
#include <assert.h>
int main(void)
{
    assert(select_flush_implementation(0) == flush_buffer_legacy);
    assert(select_flush_implementation(1) == flush_buffer_optimized);
    assert(select_flush_implementation(128) == flush_buffer_optimized);
    puts("Portable benchmark dispatch passed; no benchmark or flush executed");
    return 0;
}
