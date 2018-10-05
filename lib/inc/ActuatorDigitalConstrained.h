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

#include "ActuatorDigital.h"
#include "ActuatorDigitalChangeLogged.h"
#include "TicksTypes.h"
#include <functional>
#include <mutex>
#include <vector>

/*
 * An ActuatorAnalog has a range output
 */
class ActuatorDigitalConstrained : public ActuatorDigitalChangeLogged {
public:
    using ConstrainFunc = std::function<bool(const State&, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act)>;

private:
    std::vector<ConstrainFunc> constraints;

public:
    ActuatorDigitalConstrained(ActuatorDigital& act, const ticks_millis_t& now)
        : ActuatorDigitalChangeLogged(act, now){};
    virtual ~ActuatorDigitalConstrained() = default;

    void addConstraint(ConstrainFunc&& newConstraint)
    {
        constraints.push_back(std::move(newConstraint));
    }

    void removeAllConstraints()
    {
        constraints.clear();
    }

    bool checkConstraints(const State& val, const ticks_millis_t& now)
    {
        bool allowed = true;
        for (auto& constrainFunc : constraints) {
            allowed &= constrainFunc(val, now, *this);
        }
        return allowed;
    }

    virtual void state(const State& val, const ticks_millis_t& now) override final
    {
        if (!checkConstraints(val, now)) {
            // before returning, check constraints again with current state
            // to reset any state keeping contstraints like mutex
            checkConstraints(ActuatorDigitalChangeLogged::state(), now);
            return;
        }
        ActuatorDigitalChangeLogged::state(val, now);
    }

    State state() const
    {
        return ActuatorDigitalChangeLogged::state();
    }
};

class TimedMutex {
private:
    std::mutex m_mutex;
    duration_millis_t m_minSwitchTime = 0;
    ticks_millis_t lastActive = 0;
    const ActuatorDigitalChangeLogged* lastActuator = nullptr;

public:
    TimedMutex() = default;
    ~TimedMutex() = default;

    bool try_lock(const duration_millis_t& now, const ActuatorDigitalChangeLogged& act)
    {
        if (lastActuator == nullptr || lastActuator == &act || (now - lastActive) >= m_minSwitchTime) {
            return m_mutex.try_lock();
        }
        return false;
    }

    void unlock(const duration_millis_t& now, const ActuatorDigitalChangeLogged& act)
    {
        if (act.state() == ActuatorDigital::State::Active) {
            lastActive = now;
            lastActuator = &act;
        }
        m_mutex.unlock();
    }

    duration_millis_t minSwitchTime()
    {
        return m_minSwitchTime;
    }

    void minSwitchTime(const duration_millis_t& v)
    {
        m_minSwitchTime = v;
    }
};

namespace ADConstraints {
using State = ActuatorDigital::State;
class MinOnTime {
private:
    duration_millis_t minTime;

public:
    MinOnTime(const duration_millis_t& min)
        : minTime(min)
    {
    }

    bool operator()(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act)
    {
        auto times = act.getLastStartEndTime(State::Active, now);
        return newState == State::Active || times.end - times.start >= minTime;
    }
};

class MinOffTime {
private:
    duration_millis_t minTime;

public:
    MinOffTime(const duration_millis_t& min)
        : minTime(min)
    {
    }

    bool operator()(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act)
    {
        auto times = act.getLastStartEndTime(State::Inactive, now);
        return newState == State::Inactive || times.end - times.start >= minTime;
    }
};

class Mutex {
private:
    const std::function<std::shared_ptr<TimedMutex>()> m_mutex;
    bool hasLock = false;

public:
    explicit Mutex(
        std::function<std::shared_ptr<TimedMutex>()>&& mut)
        : m_mutex(mut)
    {
    }

    bool operator()(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act)
    {
        if (newState == State::Inactive) {
            if (hasLock) {
                if (auto mutPtr = m_mutex()) {
                    mutPtr->unlock(now, act);
                    hasLock = false;
                }
            }
            return true;
        }

        if (auto mutPtr = m_mutex()) {
            if (act.state() != State::Active && newState == State::Active) {
                hasLock = mutPtr->try_lock(now, act);
                return hasLock;
            }
        }
        return false;
    }
};

} // end namespace ADConstraints
