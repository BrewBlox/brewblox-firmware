/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of BrewBlox.
 * 
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "startup_screen.h"
#include "../fonts/fonts.h"
#include "../logo/brewblox_logo.h"
#include "BrewBlox.h"
#include "blox/stringify.h"
#include "d4d.hpp"
#include "memory_info.h"
#include "spark_wiring_ticks.h"
#include "spark_wiring_timer.h"
#include "stdio.h"

#if PLATFORM_ID != 3
#include "BrewPiTouch.h"
extern BrewPiTouch touch;
#endif

#ifndef GIT_VERSION
#error GIT_VERSION not set
#endif
#ifndef GIT_DATE
#error GIT_DATE not set
#endif
char stepTxt[32] = "Init board";
char versionString[] = "version: " stringify(GIT_VERSION) " (" stringify(GIT_DATE) ")";

D4D_DECLARE_STD_PICTURE(scrStartup_logo, 70, 100, 180, 34, &bmp_brewblox_logo)
D4D_DECLARE_STD_LABEL(scrStartup_step, stepTxt, 0, 193, 320, 15, FONT_REGULAR)
D4D_DECLARE_STD_PROGRESS_BAR(scrStartup_progress, 0, 212, 320, 12, 0)
D4D_DECLARE_STD_LABEL(scrStartup_version, versionString, 0, 224, 320, 15, FONT_REGULAR)

D4D_DECLARE_SCREEN_BEGIN(screen_startup, ScrStartup_, 0, 0, (D4D_COOR)(D4D_SCREEN_SIZE_LONGER_SIDE), (D4D_COOR)(D4D_SCREEN_SIZE_SHORTER_SIDE), nullptr, 0, nullptr, (D4D_SCR_F_DEFAULT | D4D_SCR_F_TOUCHENABLE), nullptr)
&scrStartup_logo,
    &scrStartup_version,
    &scrStartup_progress,
    &scr_mem_icon,
    &scr_mem_text,
    &scrStartup_step,
    D4D_DECLARE_SCREEN_END();

void
StartupScreen::activate()
{
    D4D_EnableObject(&scrStartup_version, D4D_FALSE); // disable for darker color
    D4D_ActivateScreen(&screen_startup, D4D_TRUE);
}

void
StartupScreen::calibrateTouchIfNeeded()
{
    auto calib = D4D_GetTouchScreenCalibration();
    if (calib.ScreenCalibrated != 1) {
        calibrateTouch();
    }
}

void
StartupScreen::setProgress(uint8_t v)
{
    D4D_PrgrsBarSetValue(&scrStartup_progress, v);
    D4D_Poll();
}

void
StartupScreen::setStep(std::string&& txt)
{
    auto s = txt;
    D4D_LabelSetText(&scrStartup_step, txt.c_str());
    D4D_Poll();
}

void
StartupScreen::calibrateTouch()
{
#if PLATFORM_ID != 3
    static auto timeoutTimer = Timer(20000, D4D_InterruptCalibrationScreen, true);
#endif
    D4D_CalibrateTouchScreen();
#if PLATFORM_ID != 3
#endif
    if (D4D_TCH_GetCalibrationStatus()) {
        brewbloxBox().storeUpdatedObject(2); // save system object
    }
}

void
ScrStartup_OnInit()
{
    D4D_PrgrsBarSetValue(&scrStartup_progress, 0);
}

void
ScrStartup_OnMain()
{
    updateRamDisplay();
}

void
ScrStartup_OnActivate()
{
}

void
ScrStartup_OnDeactivate()
{
}

uint8_t
ScrStartup_OnObjectMsg(D4D_MESSAGE* pMsg)
{
    if (pMsg->nMsgId == D4D_MSG_TOUCHED) {
        StartupScreen::calibrateTouch();
    }
    return D4D_FALSE; // don't block further processing
}
