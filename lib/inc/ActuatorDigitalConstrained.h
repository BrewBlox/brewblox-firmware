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

#include "ActuatorDigitalChangeLogged.h"
#include "TicksTypes.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class ActuatorDigitalConstrained;
namespace ADConstraints {
using State = ActuatorDigitalBase::State;

class Base {
public:
    Base() = default;
    virtual ~Base() = default;

    virtual bool allowed(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) = 0;

    virtual uint8_t id() const = 0;

    virtual uint8_t order() const = 0;
};
} // end namespace ADConstraints

class ActuatorDigitalConstrained : private ActuatorDigitalChangeLogged {
public:
    using Constraint = ADConstraints::Base;

private:
    std::vector<std::unique_ptr<Constraint>> constraints;
    uint8_t m_limiting = 0x00;
    State m_desiredState = State::Inactive;

public:
    ActuatorDigitalConstrained(ActuatorDigitalBase& act)
        : ActuatorDigitalChangeLogged(act){};

    ActuatorDigitalConstrained(const ActuatorDigitalConstrained&) = delete;
    ActuatorDigitalConstrained& operator=(const ActuatorDigitalConstrained&) = delete;
    ActuatorDigitalConstrained& operator=(ActuatorDigitalConstrained&&) = delete;
    ActuatorDigitalConstrained(ActuatorDigitalConstrained&&) = default;

    virtual ~ActuatorDigitalConstrained() = default;

    // ActuatorDigitalChangeLogged is inherited privately to prevent bypassing constraints.
    // explicitly make functions available that should be in public interface here.
    using ActuatorDigitalChangeLogged::activeDurations;
    using ActuatorDigitalChangeLogged::getLastStartEndTime;
    using ActuatorDigitalChangeLogged::setStateUnlogged;
    using ActuatorDigitalChangeLogged::supportsFastIo;

    void addConstraint(std::unique_ptr<Constraint>&& newConstraint)
    {
        if (constraints.size() < 8) {
            constraints.push_back(std::move(newConstraint));
        }
        std::sort(constraints.begin(), constraints.end(),
                  [](const std::unique_ptr<Constraint>& a, const std::unique_ptr<Constraint>& b) { return a->order() < b->order(); });
    }

    void removeAllConstraints()
    {
        constraints.clear();
    }

    void resetHistory()
    {
        ActuatorDigitalChangeLogged::resetHistory();
    }

    uint8_t checkConstraints(const State& val, const ticks_millis_t& now)
    {
        uint8_t limiting = 0x00;
        uint8_t bit = 0x01;
        for (auto& c : constraints) {
            if (!c->allowed(val, now, *this)) {
                limiting = limiting | bit;
                break;
            }
            bit = bit << 1;
        }
        return limiting;
    }

    uint8_t limiting() const
    {
        return m_limiting;
    }

    void desiredState(const State& val, const ticks_millis_t& now)
    {
        lastUpdateTime = now; // always update fallback time for state setter without time
        m_desiredState = val;
        m_limiting = checkConstraints(val, now);
        if (m_limiting == 0) {
            ActuatorDigitalChangeLogged::state(val, now);
        }
    }

    void desiredState(const State& val)
    {
        desiredState(val, lastUpdateTime);
    }

    State state() const
    {
        return ActuatorDigitalChangeLogged::state();
    }

    void update(const ticks_millis_t& now)
    {
        desiredState(m_desiredState, now); // re-apply constraints for new update time
    }

    State desiredState() const
    {
        return m_desiredState;
    }

    const std::vector<std::unique_ptr<Constraint>>& constraintsList() const
    {
        return constraints;
    };
};

class MutexTarget {
public:
    MutexTarget() = default;
    ~MutexTarget() = default;
    std::mutex mut;
};

namespace ADConstraints {
template <uint8_t ID>
class MinOnTime : public Base {
private:
    duration_millis_t m_limit;

public:
    MinOnTime(const duration_millis_t& min)
        : m_limit(min)
    {
    }

    bool allowed(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) override final
    {
        if (act.state() != State::Active) {
            return true;
        }
        auto times = act.getLastStartEndTime(State::Active, now);
        return newState == State::Active || times.end - times.start >= m_limit;
    }

    virtual uint8_t id() const override final
    {
        return ID;
    }

    duration_millis_t limit()
    {
        return m_limit;
    }

    virtual uint8_t order() const override final
    {
        return 1;
    }
};

template <uint8_t ID>
class MinOffTime : public Base {
private:
    duration_millis_t m_limit;

public:
    MinOffTime(const duration_millis_t& min)
        : m_limit(min)
    {
    }

    virtual bool allowed(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) override final
    {
        if (act.state() != State::Inactive) {
            return true;
        }
        auto times = act.getLastStartEndTime(State::Inactive, now);
        auto elapsedOff = times.end - times.start;
        return newState == State::Inactive || elapsedOff >= m_limit;
    }

    virtual uint8_t id() const override final
    {
        return ID;
    }

    duration_millis_t limit()
    {
        return m_limit;
    }

    virtual uint8_t order() const override final
    {
        return 0;
    }
};

template <uint8_t ID>
class Mutex : public Base {
private:
    const std::function<std::shared_ptr<MutexTarget>()> m_mutexTarget;
    uint16_t holdAfterTurnOff = 0;
    std::shared_ptr<MutexTarget> m_lockedMutex; // keep shared pointer to mutex, so it cannot be destroyed while locked
    std::unique_lock<std::mutex> m_lock;

public:
    explicit Mutex(
        std::function<std::shared_ptr<MutexTarget>()>&& mut, uint16_t hold)
        : m_mutexTarget(mut)
        , holdAfterTurnOff(hold)
    {
    }
    ~Mutex() = default;

    virtual bool allowed(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) override final
    {
        if (m_lock) {
            // already owner of lock.
            if (newState == State::Inactive) {
                // Release lock if actuator has been off for minimal time
                auto times = act.getLastStartEndTime(State::Inactive, now);
                auto elapsedOff = times.end - times.start;
                if (elapsedOff >= holdAfterTurnOff) {
                    m_lock.unlock();
                    m_lockedMutex.reset();
                }
            }
            return true;
        }
        if (newState == State::Inactive) {
            // no locked, but no lock needed
            return true;
        }
        if (newState == State::Active) {
            m_lockedMutex = m_mutexTarget(); // store shared pointer to target so it can't be deleted while locked
            if (m_lockedMutex) {
                m_lock = std::unique_lock<std::mutex>(m_lockedMutex->mut, std::try_to_lock);
                if (m_lock) {
                    // successfully aquired lock of target
                    return true;
                }
            }
        }
        return false;
    }

    virtual uint8_t
    id() const override final
    {
        return ID;
    }

    virtual uint8_t
    order() const override final
    {
        return 2;
    }
};

} // end namespace ADConstraints
