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

#include "ActuatorDigitalChangeLogged.h"
#include "TicksTypes.h"
#include <algorithm>
#include <array>
#include <cstdint>

using State = ActuatorDigitalBase::State;

void
ActuatorDigitalChangeLogged::state(const State& val, const ticks_millis_t& now)
{
    actuator.state(val);
    update(now);
}

void
ActuatorDigitalChangeLogged::state(const State& val)
{
    state(val, lastUpdateTime);
}

void
ActuatorDigitalChangeLogged::setStateUnlogged(const State& val)
{
    actuator.state(val);
}

State
ActuatorDigitalChangeLogged::state() const
{
    return actuator.state();
}

void
ActuatorDigitalChangeLogged::update(const ticks_millis_t& now)
{
    if (state() != history.front().newState) {
        std::rotate(history.rbegin(), history.rbegin() + 1, history.rend());
        history[0] = {state(), now};
    }
    lastUpdateTime = now;
}

ActuatorDigitalChangeLogged::StartEnd
ActuatorDigitalChangeLogged::getLastStartEndTime(const State& state, const ticks_millis_t& now) const
{
    StartEnd result;

    result.start = now + 1;
    result.end = (actuator.state() == state) ? now : now + 1;

    ticks_millis_t end = result.end;
    for (const auto& h : history) {
        if (h.newState == state) {
            result.start = h.startTime;
            result.end = end;
            break;
        }
        end = h.startTime;
    }
    return result;
}

ActuatorDigitalChangeLogged::Durations
ActuatorDigitalChangeLogged::activeDurations(const ticks_millis_t& now)
{
    Durations result;

    result.currentActive = 0;
    result.currentPeriod = 0;
    result.previousActive = 0;
    result.previousPeriod = 0;
    result.lastState = history.front().newState;
    auto end = now;
    auto start = ticks_millis_t(0);
    uint8_t activePeriods = 0;

    auto h = history.cbegin();
    for (; h < history.cend(); ++h) {
        start = h->startTime;
        auto duration = end - start;
        end = start;
        if (h->newState == State::Active) {
            ++activePeriods;
            if (activePeriods == 1) {
                result.currentActive = duration;
                result.currentPeriod += duration;
            } else if (activePeriods == 2) {
                result.previousActive = duration;
                result.previousPeriod += duration;
            } else {
                break;
            }
        } else {
            if (history.front().newState == State::Inactive) {
                if (activePeriods == 0) {
                    result.currentPeriod += duration;
                } else if (activePeriods <= 1) {
                    result.previousPeriod += duration;
                } else {
                    break;
                }
            } else {
                if (activePeriods <= 1) {
                    result.currentPeriod += duration;
                } else {
                    result.previousPeriod += duration;
                }
            }
        }
    }
    return result;
}

void
ActuatorDigitalChangeLogged::resetHistory()
{
    history.fill(StateChange{State::Unknown, ticks_millis_t(-1)});
    history[0] = {actuator.state(), 0};
    lastUpdateTime = 0;
}

bool
ActuatorDigitalChangeLogged::supportsFastIo() const
{
    return actuator.supportsFastIo();
}
