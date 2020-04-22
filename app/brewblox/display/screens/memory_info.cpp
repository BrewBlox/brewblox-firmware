#include "d4d.hpp"

#include "../fonts/fonts.h"

#include "SmallColorScheme.h"
#include "core_hal.h"
#include "memory_info.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

char scr_mem_icon_str[2] = "\x2c";
char scr_mem_val_str[10] = "";

D4D_DECLARE_LABEL(scr_mem_icon, scr_mem_icon_str, 256, 0, 20, 20, D4D_LBL_F_DEFAULT, AS_D4D_COLOR_SCHEME(&TOP_BAR_SCHEME), FONT_ICON, nullptr, nullptr);
D4D_DECLARE_LABEL(scr_mem_text, scr_mem_val_str, 270, 0, 50, 20, D4D_LBL_F_DEFAULT, AS_D4D_COLOR_SCHEME(&TOP_BAR_SCHEME), FONT_REGULAR, nullptr, nullptr);

bool
printHeapUse(char* dest, uint8_t maxLen)
{
    static uint8_t lastValue = 0;
    runtime_info_t info;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);
    HAL_Core_Runtime_Info(&info, NULL);

    uint8_t freePct = info.total_heap ? (100 * info.freeheap) / info.total_heap : 0;
    if (freePct == lastValue) {
        return false;
    }
    lastValue = freePct;

    uint8_t usedPct = 100 - freePct;
    uint8_t maxPct = info.total_heap ? (100 * info.max_used_heap) / info.total_heap : 0;
    snprintf(dest, maxLen, "%2d%% %2d%%", usedPct, maxPct);
    return true;
}

void
updateRamDisplay()
{
    if (printHeapUse(scr_mem_val_str, 10)) {
        D4D_InvalidateObject(&scr_mem_text, D4D_FALSE);
    }
}