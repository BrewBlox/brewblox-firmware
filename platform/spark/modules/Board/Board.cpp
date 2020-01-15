/*
 * Copyright 2015 BrewPi/Elco Jacobs/Matthew McGowan.
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

#include "Board.h"
#include "delay_hal.h"
#include "pwm_hal.h"

bool
readAlarmPin()
{
    HAL_Pin_Mode(PIN_ALARM, INPUT);
#if PLATFORM_ID != 3
    HAL_Delay_Milliseconds(1); // give time to change
#endif
    bool result = HAL_GPIO_Read(PIN_ALARM);
    HAL_Pin_Mode(PIN_ALARM, OUTPUT);
    return result;
}

SparkVersion
getSparkVersion()
{

#if PLATFORM_ID == 8 || PLATFORM_ID == 3 // P1 or simulation
    return SparkVersion::V3;
#else
    // V2 has a pull down resistor, V1 has a pull up resistor on the alarm pin
    // If the pin is low, it is V2
    static SparkVersion version = readAlarmPin() ? SparkVersion::V1 : SparkVersion::V2;
    return version;
#endif
}

void
boardInit()
{
#if PLATFORM_ID == 8 || PLATFORM_ID == 3 // P1 or simulation
    HAL_Pin_Mode(PIN_V3_BOTTOM1, OUTPUT);
    digitalWriteFast(PIN_V3_BOTTOM1, LOW);

    HAL_Pin_Mode(PIN_V3_BOTTOM2, OUTPUT);
    digitalWriteFast(PIN_V3_BOTTOM2, LOW);

#ifdef PIN_V3_TOP1
    HAL_Pin_Mode(PIN_V3_TOP1, INPUT_PULLDOWN);
#endif

#ifdef PIN_V3_TOP1_DIR
    HAL_Pin_Mode(PIN_V3_TOP1_DIR, OUTPUT);
    digitalWriteFast(PIN_V3_TOP1_DIR, LOW); // configure as input
#endif

    HAL_Pin_Mode(PIN_V3_TOP2, INPUT_PULLDOWN);

#ifdef PIN_V3_TOP2_DIR
    HAL_Pin_Mode(PIN_V3_TOP2_DIR, OUTPUT);
    digitalWriteFast(PIN_V3_TOP2_DIR, LOW); // configure as input
#endif

    HAL_Pin_Mode(PIN_V3_TOP3, OUTPUT);
    digitalWriteFast(PIN_V3_TOP3, LOW);

#ifdef PIN_12V_ENABLE
    HAL_Pin_Mode(PIN_12V_ENABLE, OUTPUT);
    digitalWriteFast(PIN_12V_ENABLE, HIGH); // 12V disabled by default
#endif

#ifdef PIN_5V_ENABLE
    HAL_Pin_Mode(PIN_5V_ENABLE, OUTPUT);
    // 5V on RJ12 enabled by default, 12V disabled to prevent damaging wrongly connected peripherals
    digitalWriteFast(PIN_5V_ENABLE, LOW);
#endif

    digitalWriteFast(PIN_ALARM, LOW);

#ifdef PIN_LCD_BACKLIGHT
    HAL_Pin_Mode(PIN_LCD_BACKLIGHT, OUTPUT);
    displayBrightness(255);
#endif

#elif PLATFORM_ID == 6
    HAL_Pin_Mode(PIN_ACTUATOR1, OUTPUT);
    HAL_Pin_Mode(PIN_ACTUATOR2, OUTPUT);
    HAL_Pin_Mode(PIN_ACTUATOR3, OUTPUT);
    digitalWriteFast(PIN_ACTUATOR1, LOW);
    digitalWriteFast(PIN_ACTUATOR2, LOW);
    digitalWriteFast(PIN_ACTUATOR3, LOW);

    if (getSparkVersion() == SparkVersion::V1) {
        digitalWriteFast(PIN_ALARM, HIGH); // alarm is inverted on V1
    } else {
        HAL_Pin_Mode(PIN_ACTUATOR0, OUTPUT); // actuator 0 is not available on V1, but is on V2
        digitalWriteFast(PIN_ACTUATOR0, LOW);
    }
#endif

    HAL_Pin_Mode(PIN_ALARM, OUTPUT);

#if PLATFORM_ID != 3
    HAL_Pin_Mode(PIN_RS485_TX, OUTPUT);
    HAL_Pin_Mode(PIN_RS485_RX, INPUT);
    HAL_Pin_Mode(PIN_RS485_TX_EN, OUTPUT);

    digitalWriteFast(PIN_TOUCH_CS, HIGH);
    HAL_Pin_Mode(PIN_TOUCH_CS, OUTPUT);
    digitalWriteFast(PIN_LCD_CS, HIGH);
    HAL_Pin_Mode(PIN_LCD_CS, OUTPUT);
    digitalWriteFast(PIN_SD_CS, HIGH);
    HAL_Pin_Mode(PIN_SD_CS, OUTPUT);
    HAL_Pin_Mode(PIN_LCD_DC, OUTPUT);
    HAL_Pin_Mode(PIN_TOUCH_IRQ, INPUT);
#endif
}

void
enablePheripheral5V(bool enabled)
{
#if defined(PIN_5V_ENABLE)
    digitalWriteFast(PIN_5V_ENABLE, enabled);
#endif
}

void
enablePheripheral12V(bool enabled)
{
#if defined(PIN_12V_ENABLE)
    digitalWriteFast(PIN_12V_ENABLE, enabled);
#endif
}

void
displayBrightness(uint8_t v)
{
#if defined(PIN_LCD_BACKLIGHT) && defined(SPARK)
    HAL_PWM_Write_With_Frequency(PIN_LCD_BACKLIGHT, v, 100);
#endif
}
