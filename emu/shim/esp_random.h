#pragma once
#include <stdint.h>
#include <stdlib.h>
inline uint32_t esp_random() { return (uint32_t)rand() ^ ((uint32_t)rand() << 16); }
#include <stdlib.h>
