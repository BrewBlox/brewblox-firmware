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
#include "display/screens/listening_screen.h"
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
static ApplicationWatchdog appWatchdog(60000, watchdogReset, 256);
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
    ListeningScreen::activate();
    manageConnections(ticks.millis()); // stop http server
    brewbloxBox().stopConnections();
    brewbloxBox().unloadAllObjects();
    HAL_Delay_Milliseconds(100);
}

void
onSetupModeEnd()
{
    handleReset(true, RESET_USER_REASON::LISTENING_MODE_EXIT);
}

void
onOutOfMemory(system_event_t event, int param)
{
    HAL_Delay_Milliseconds(1000);
}

#if PLATFORM_ID != PLATFORM_GCC
STARTUP(
    boardInit(););
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
    manageConnections(0); // init network early to websocket display emulation works during setup()
#else
    Buzzer.beep(2, 50);
    HAL_Delay_Milliseconds(1);
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
        StartupScreen::setProgress(ticks.millis() / 40); // up to 50
        displayTick();
    } while (ticks.millis() < ((PLATFORM_ID != PLATFORM_GCC) ? 2000 : 0));

    enablePheripheral5V(true);
    HAL_Delay_Milliseconds(1);

    StartupScreen::setStep("Init OneWire");
    theOneWire();

    HAL_Delay_Milliseconds(1);
    StartupScreen::setProgress(60);
    StartupScreen::setStep("Init BrewBlox framework");
    brewbloxBox();

    HAL_Delay_Milliseconds(1);

    StartupScreen::setProgress(70);
    StartupScreen::setStep("Loading blocks");
    brewbloxBox().loadObjectsFromStorage(); // init box and load stored objects
    HAL_Delay_Milliseconds(1);

    StartupScreen::setProgress(80);
    StartupScreen::setStep("Enabling WiFi and mDNS");
    wifiInit();
    HAL_Delay_Milliseconds(1);

    StartupScreen::setProgress(100);
    StartupScreen::setStep("Ready!");

    // perform pending EEPROM erase while we're waiting. Can take up to 500ms and stalls all code execution
    // This avoids having to do it later when writing to EEPROM
    HAL_EEPROM_Perform_Pending_Erase();

#if PLATFORM_ID != PLATFORM_GCC
    TimerInterrupts::init();
    System.on(setup_begin, onSetupModeBegin);
    System.on(setup_end, onSetupModeEnd);
    // System.on(out_of_memory, onOutOfMemory); // uncomment when debugging memory leaks
#endif

    brewbloxBox().startConnections();
    WidgetsScreen::activate();
}

void
loop()
{
    ticks.switchTaskTimer(TicksClass::TaskId::DisplayUpdate);
    displayTick();
    if (!listeningModeEnabled()) {

        ticks.switchTaskTimer(TicksClass::TaskId::Communication);
        manageConnections(ticks.millis());
        brewbloxBox().hexCommunicate();

        ticks.switchTaskTimer(TicksClass::TaskId::BlocksUpdate);
        updateBrewbloxBox();

        watchdogCheckin(); // not done while listening, so 60s timeout for stuck listening mode
    }
    ticks.switchTaskTimer(TicksClass::TaskId::System);
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
