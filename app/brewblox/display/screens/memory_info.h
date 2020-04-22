#pragma once

#include "d4d.hpp"
#include <cstdint>

bool
printHeapUse(char* dest, uint8_t maxLen);

void
updateRamDisplay();

extern const D4D_OBJECT scr_mem_icon;
extern const D4D_OBJECT scr_mem_text;