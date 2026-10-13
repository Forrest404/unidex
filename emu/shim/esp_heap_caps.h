#pragma once
#include <stddef.h>
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_SPIRAM 1
inline size_t heap_caps_get_free_size(int) { return 200000; }
inline size_t heap_caps_get_largest_free_block(int) { return 100000; }
