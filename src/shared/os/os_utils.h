#pragma once

#include "shared.h"

uint64_t time_millis();

uint8_t * allocate_executable(size_t size);

void sleep_ms(uint64_t ms);
