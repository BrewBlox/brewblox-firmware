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
#include <ctime>

template <typename Impl>
class Ticks {
    Impl impl;

public:
    Ticks()
        : impl()
    {
    }

    inline void delayMillis(const duration_millis_t& val) const
    {
        return impl.delayMillis(val);
    }

    inline ticks_millis_t millis() const
    {
        return impl.millis();
    }

    inline ticks_micros_t micros() const
    {
        return impl.micros();
    }

    /*inline utc_seconds_t seconds()
    {
        return impl.seconds();
    }*/

    inline utc_seconds_t utc() const
    {
        return impl.utc();
    }

    void setUtc(const utc_seconds_t& utcNow)
    {
        impl.setUtc(utcNow);
    }

    inline utc_seconds_t secondsSince(utc_seconds_t timeStamp) const
    {
        return utc() - timeStamp;
    }

    inline ticks_millis_t millisSince(ticks_millis_t timeStamp) const
    {
        return millis() - timeStamp;
    }

    struct tm calendarTime() const
    {
        struct tm* calendar_time;
        calendar_time = utc();
        calendar_time->tm_year += 1900;
        return *calendar_time;
    }

    Impl& ticksImpl()
    {
        return impl;
    }

    enum class TaskId : uint8_t {
        Communication = 0,
        BlocksUpdate = 1,
        DisplayUpdate = 2,
        System = 3,
        NumTasks = 4,
    };

    void switchTaskTimer(TaskId startedTask)
    {
        auto now = millis();
        auto elapsed = now - lastTimerTick;
        uint8_t timerIdx = uint8_t(runningTask);
        timers[timerIdx] = timers[timerIdx] - (timers[timerIdx] >> 5) + elapsed;
        runningTask = startedTask;
        lastTimerTick = now;
    }

    ticks_millis_t taskTime(uint8_t id) const
    {
        return timers[id];
    }

private:
    ticks_millis_t timers[uint8_t(TaskId::NumTasks)]; // EMA of durations of each task
    ticks_millis_t lastTimerTick = 0;
    TaskId runningTask = TaskId::System;
};
