/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of the BrewBlox Control Library.
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

#pragma once

#include "TicksTypes.h"
#include <cstdint>

/**
 * A Ticks implementation that increments the millis count each time it is called.
 * This is used for testing.
 */
class MockTicks {
public:
    MockTicks(duration_millis_t autoIncrement = 0)
        : _increment(autoIncrement)
        , _ticks(0)
    {
    }

    ticks_millis_t millis() const
    {
        return _ticks += _increment;
    }
    ticks_micros_t micros() const
    {
        return 1000 * _ticks++;
    }
    utc_seconds_t utc() const
    {
        if (_utc_boot_time) {
            return _utc_boot_time + millis() / 1000;
        }
        return 0;
    }
    void setUtc(const utc_seconds_t& t)
    {
        _utc_boot_time = t - millis() / 1000;
    }
    utc_seconds_t timeSinceSeconds(utc_seconds_t timeStamp) const
    {
        return utc() - timeStamp;
    }
    ticks_millis_t timeSinceMillis(ticks_millis_t timeStamp) const
    {
        return millis() - timeStamp;
    }
    void reset(ticks_millis_t v = 0) { _ticks = v; }
    void delayMillis(const duration_millis_t& duration) const
    {
        _ticks += duration;
    }

private:
    ticks_millis_t _increment;
    mutable ticks_millis_t _ticks;
    utc_seconds_t _utc_boot_time;
};
