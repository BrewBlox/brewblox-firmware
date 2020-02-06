/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
 * Copyright 2013 Matthew McGowan.
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

#include "../inc/Logger.h"

#include "../inc/DallasTemperature.h"
#include "../inc/OneWire.h"
#include "../inc/OneWireAddress.h"
#include "../inc/TempSensorOneWire.h"
#include "../inc/Temperature.h"

/**
 * Initializes the temperature sensor.
 * This method should be called when the sensor is first created and also any time the sensor reports it has been reset.
 * This re-intializes the reset detection.
 */
void
TempSensorOneWire::init()
{
    if (m_sensor.initConnection(getDeviceAddress().asUint8ptr())) {
        requestConversion();
    }
}

void
TempSensorOneWire::requestConversion()
{
    m_sensor.requestTemperaturesByAddress(getDeviceAddress().asUint8ptr());
}

void
TempSensorOneWire::connected(bool _connected)
{
    if (m_connected == _connected) {
        return; // state stays the same
    }
    m_connected = _connected;
    if (m_connected) {
        // CL_LOG_WARN("OneWire temp sensor connected: ") << getDeviceAddress().toString();
    } else {
        // CL_LOG_WARN("OneWire temp sensor disconnected: ") << getDeviceAddress().toString();
    }
}

temp_t
TempSensorOneWire::value() const
{
    if (!m_connected) {
        return 0;
    }

    return m_cachedValue;
}

void
TempSensorOneWire::update()
{
    m_cachedValue = readAndConstrainTemp();
    requestConversion();
}

temp_t
TempSensorOneWire::readAndConstrainTemp()
{
    int16_t tempRaw;
    bool success;

    tempRaw = m_sensor.getTempRaw(getDeviceAddress().asUint8ptr());
    success = tempRaw > RESET_DETECTED_RAW;

    if (tempRaw == RESET_DETECTED_RAW) {
        // retry re-init if the sensor is present, but needs a reset
        init();
    }

    connected(success);

    if (!success) {
        return 0;
    }

    // difference in precision between DS18B20 format and temperature format
    constexpr auto shift = cnl::_impl::fractional_digits<temp_t>{} - ONEWIRE_TEMP_SENSOR_PRECISION;
    temp_t temp = cnl::wrap<temp_t>(tempRaw << shift);
    return temp + m_calibrationOffset;
}
