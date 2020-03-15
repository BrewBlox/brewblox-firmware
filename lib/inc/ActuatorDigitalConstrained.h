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

    virtual duration_millis_t allowedImpl(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) = 0;

    virtual uint8_t id() const = 0;

    virtual uint8_t order() const = 0;

    void timeRemaining(duration_millis_t v)
    {
        m_timeRemaining = v;
    }

    duration_millis_t timeRemaining() const
    {
        return m_timeRemaining;
    }

    duration_millis_t allowed(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act)
    {
        m_timeRemaining = allowedImpl(newState, now, act);
        return m_timeRemaining;
    }

private:
    duration_millis_t m_timeRemaining = 0;
};
} // end namespace ADConstraints

class ActuatorDigitalConstrained : private ActuatorDigitalChangeLogged {
public:
    using Constraint = ADConstraints::Base;

private:
    std::vector<std::unique_ptr<Constraint>> constraints;
    State m_desiredState = State::Inactive;

public:
    ActuatorDigitalConstrained(ActuatorDigitalBase& act)
        : ActuatorDigitalChangeLogged(act)
    {
    }

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

    // remove all constraints and return vector of removed constraints
    auto removeAllConstraints()
    {
        auto oldConstraints = std::move(constraints);
        constraints = std::vector<std::unique_ptr<Constraint>>();
        return oldConstraints;
    }

    void resetHistory()
    {
        ActuatorDigitalChangeLogged::resetHistory();
    }

    duration_millis_t checkConstraints(const State& val, const ticks_millis_t& now)
    {
        for (auto& c : constraints) {
            auto remaining = c->allowed(val, now, *this);
            if (remaining > 0) {
                return remaining;
            }
        }
        return 0;
    }

    duration_millis_t desiredState(const State& val, const ticks_millis_t& now)
    {
        lastUpdateTime = now; // always update fallback time for state setter without time
        m_desiredState = val;
        auto timeRemaining = checkConstraints(val, now);
        if (timeRemaining == 0) {
            ActuatorDigitalChangeLogged::state(val, now);
        }
        return timeRemaining;
    }

    duration_millis_t desiredState(const State& val)
    {
        return desiredState(val, lastUpdateTime);
    }

    State state() const
    {
        return ActuatorDigitalChangeLogged::state();
    }

    duration_millis_t update(const ticks_millis_t& now)
    {
        // re-apply constraints for new update time
        auto remaining = desiredState(m_desiredState, now);
        // update at least once per second
        return (remaining < 1000) ? remaining : 1000;
    }

    State
    desiredState() const
    {
        return m_desiredState;
    }

    const std::vector<std::unique_ptr<Constraint>>&
    constraintsList() const
    {
        return constraints;
    }
};

class MutexTarget {
public:
    MutexTarget() = default;
    ~MutexTarget() = default;
    std::mutex mut;

    duration_millis_t holdAfterTurnOff() const
    {
        return m_holdAfterTurnOff;
    }

    void holdAfterTurnOff(duration_millis_t v)
    {
        m_holdAfterTurnOff = v;
    }

    void timeRemaining(duration_millis_t v)
    {
        m_timeRemaining = v;
    }

    duration_millis_t timeRemaining() const
    {
        return m_timeRemaining;
    }

private:
    duration_millis_t m_timeRemaining = 0;
    duration_millis_t m_holdAfterTurnOff = 0;
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

    duration_millis_t allowedImpl(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) override final
    {
        if (act.state() != State::Active) {
            return 0;
        }
        if (newState == State::Active) {
            return 0;
        }
        auto times = act.getLastStartEndTime(State::Active, now);
        auto elapsedOn = times.end - times.start;

        if (elapsedOn >= m_limit) {
            return 0;
        }

        return m_limit - elapsedOn;
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

    virtual duration_millis_t allowedImpl(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) override final
    {
        if (act.state() != State::Inactive) {
            return 0;
        }

        if (newState == State::Inactive) {
            return 0;
        }

        auto times = act.getLastStartEndTime(State::Inactive, now);
        auto elapsedOff = times.end - times.start;

        if (elapsedOff >= m_limit) {
            return 0;
        }

        return m_limit - elapsedOff;
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
    duration_millis_t m_holdAfterTurnOff;
    bool m_useCustomHoldDuration;
    // keep shared pointer to mutex, so it cannot be destroyed while locked
    std::shared_ptr<MutexTarget> m_lockedMutex;
    std::unique_lock<std::mutex> m_lock;

public:
    explicit Mutex(
        std::function<std::shared_ptr<MutexTarget>()>&& mut, duration_millis_t hold, bool useCustomHold)
        : m_mutexTarget(mut)
        , m_holdAfterTurnOff(hold)
        , m_useCustomHoldDuration(useCustomHold)
    {
    }
    ~Mutex() = default;

    virtual duration_millis_t allowedImpl(const State& newState, const ticks_millis_t& now, const ActuatorDigitalChangeLogged& act) override final
    {
        if (m_lock) {
            // already owner of lock.
            auto elapsedMinimal = m_useCustomHoldDuration ? m_holdAfterTurnOff : m_lockedMutex->holdAfterTurnOff();
            if (newState == State::Inactive) {
                // Release lock if actuator has been off for minimal time
                duration_millis_t elapsedOff = 0;
                if (act.state() == State::Inactive) {
                    auto times = act.getLastStartEndTime(State::Inactive, now);
                    elapsedOff = times.end - times.start;
                }

                if (elapsedOff >= elapsedMinimal) {
                    m_lockedMutex->timeRemaining(0);
                    m_lock.unlock();
                    m_lockedMutex.reset();
                } else {
                    m_lockedMutex->timeRemaining(elapsedMinimal - elapsedOff);
                }
            } else {
                m_lockedMutex->timeRemaining(elapsedMinimal);
            }
            return 0;
        }
        if (newState == State::Inactive) {
            // not locked, but no lock needed
            return 0;
        }
        if (newState == State::Active) {
            m_lockedMutex = m_mutexTarget(); // store shared pointer to target so it can't be deleted while locked
            if (m_lockedMutex) {
                m_lock = std::unique_lock<std::mutex>(m_lockedMutex->mut, std::try_to_lock);
                if (m_lock) {
                    // successfully aquired lock of target
                    return 0;
                }
                return m_lockedMutex->timeRemaining() + 1;
            }
        }
        return 1000;
    }

    auto holdAfterTurnOff() const
    {
        return m_holdAfterTurnOff;
    }

    void holdAfterTurnOff(duration_millis_t v)
    {
        m_holdAfterTurnOff = v;
    }

    bool useCustomHoldDuration() const
    {
        return m_useCustomHoldDuration;
    }

    void useCustomHoldDuration(bool v)
    {
        m_useCustomHoldDuration = v;
    }

    bool hasLock() const
    {
        return bool(m_lock);
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
