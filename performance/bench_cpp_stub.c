// Stands in for bench_cpp.cpp when no C++ compiler is available or the C++
// headers were not fetched, so main.c can call the registration hook
// unconditionally.
#include "bench.h"

void bx_bench_register_cpp(void)
{
}
