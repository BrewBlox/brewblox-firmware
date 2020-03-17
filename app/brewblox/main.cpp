/*
 * Copyright 2017 BrewPi
 *
 * This file is part of BrewPi.
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

#include "AppTicks.h"
#include "Board.h"
#include "BrewBlox.h"
#include "Buzzer.h"
#include "TimerInterrupts.h"
#include "blox/stringify.h"
#include "cbox/Box.h"
#include "cbox/Object.h"
#include "connectivity.h"
#include "d4d.hpp"
#include "delay_hal.h"
#include "display/screens/WidgetsScreen.h"
#include "display/screens/startup_screen.h"
#include "eeprom_hal.h"
#include "reset.h"
#include "spark_wiring_startup.h"
#include "spark_wiring_system.h"
#include "spark_wiring_timer.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
STARTUP(System.enableFeature(FEATURE_RESET_INFO));
STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

#if PLATFORM_ID == PLATFORM_GCC
#include <csignal>
void
signal_handler(int signal)
{

    exit(signal);
}
#endif

void
watchdogReset()
{
    System.reset(RESET_USER_REASON::WATCHDOG);
}

#if PLATFORM_THREADING
#include "spark_wiring_watchdog.h"
ApplicationWatchdog appWatchdog(60000, watchdogReset);
inline void
watchdogCheckin()
{
    appWatchdog.checkin();
}
#else
// define dummy watchdog checkin for when the watchdog is not available
inline void
watchdogCheckin()
{
}
#endif

void
displayTick()
{
    static ticks_millis_t lastTick = -40;
    auto now = ticks.millis();
    if (now > lastTick + 40) {
        lastTick = now;
        D4D_Poll();
        D4D_CheckTouchScreen();
        D4D_TimeTickPut();
        D4D_FlushOutput();
    }
}

void
onSetupModeBegin()
{
    logEvent("SETUP_MODE");
    HAL_Delay_Milliseconds(100);
}

#if PLATFORM_ID != PLATFORM_GCC
STARTUP(
    boardInit();
    Buzzer.beep(2, 50););
#endif

void
setup()
{
// Install a signal handler
#if PLATFORM_ID == PLATFORM_GCC
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    // pin map is not initialized properly in gcc build before setup runs
    boardInit();
    Buzzer.beep(2, 50);
#endif

    System.on(setup_update, watchdogCheckin);
    System.on(setup_begin, onSetupModeBegin);
    HAL_Delay_Milliseconds(1);

#if PLATFORM_ID == PLATFORM_GCC
    manageConnections(0); // init network early to websocket display emulation works during setup()
#endif

    // init display
    D4D_Init(nullptr);
    D4D_TOUCHSCREEN_CALIB defaultCalib = {1, 0, 0, 64, 64};
    D4D_TCH_SetCalibration(defaultCalib);

    StartupScreen::activate();
    HAL_Delay_Milliseconds(1);

    StartupScreen::setProgress(10);
    StartupScreen::setStep("Power cycling peripherals");

    do {
        displayTick();
    } while (ticks.millis() < 2000);

    enablePheripheral5V(true);
    HAL_Delay_Milliseconds(1);

    StartupScreen::setProgress(30);
    StartupScreen::setStep("Init OneWire");
    theOneWire();

    HAL_Delay_Milliseconds(1);
    StartupScreen::setProgress(40);
    StartupScreen::setStep("Init BrewBlox framework");
    brewbloxBox();

    HAL_Delay_Milliseconds(1);
    StartupScreen::setProgress(80);
    StartupScreen::setStep("Loading objects");
    brewbloxBox().loadObjectsFromStorage(); // init box and load stored objects
    HAL_Delay_Milliseconds(1);
    StartupScreen::setProgress(100);

    // perform pending EEPROM erase while we're waiting. Can take up to 500ms and stalls all code execution
    // This avoids having to do it later when writing to EEPROM
    HAL_EEPROM_Perform_Pending_Erase();

    StartupScreen::setStep("Ready!");

    while (ticks.millis() < 5000) {
        displayTick();
        HAL_Delay_Milliseconds(1);
    };

    WidgetsScreen::activate();
#if PLATFORM_ID != PLATFORM_GCC
    TimerInterrupts::init();
#endif

    wifiInit();
    HAL_Delay_Milliseconds(1);
}

void
loop()
{
    ticks.switchTaskTimer(TicksClass::TaskId::Communication);
    if (!listeningModeEnabled()) {
        manageConnections(ticks.millis());
        brewbloxBox().hexCommunicate();
    }

    ticks.switchTaskTimer(TicksClass::TaskId::BlocksUpdate);
    updateBrewbloxBox();

    ticks.switchTaskTimer(TicksClass::TaskId::DisplayUpdate);
    displayTick();

    ticks.switchTaskTimer(TicksClass::TaskId::System);
    watchdogCheckin();
    HAL_Delay_Milliseconds(1);
}

void
handleReset(bool exitFlag, uint8_t reason)
{
    if (exitFlag) {
#if PLATFORM_ID == PLATFORM_GCC
        exit(0);
#else
        System.reset(reason);
#endif
    }
}
