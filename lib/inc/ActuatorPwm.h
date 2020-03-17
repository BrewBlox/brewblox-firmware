/*
 * Copyright 2018 BrewPi B.V.
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

#pragma once

#include "ActuatorAnalog.h"
#include "ActuatorDigitalConstrained.h"
#include "FixedPoint.h"
#include <cstdint>
#include <functional>
#include <memory>

#ifndef PLATFORM_GCC
#define PLATFORM_GCC 3
#endif

/**
	ActuatorPWM drives a (change logged) digital actuator and makes it available as range actuator, by quickly turning it on and off repeatedly.
 */
class ActuatorPwm final : public ActuatorAnalog {
public:
    using value_t = ActuatorAnalog::value_t;
    using State = ActuatorDigitalBase::State;
    using update_t = ticks_millis_t;

private:
    const std::function<std::shared_ptr<ActuatorDigitalConstrained>()> m_target;
    duration_millis_t m_period;
    duration_millis_t m_dutyTime = 0;
    value_t m_dutySetting = 0;
    value_t m_dutyAchieved = 0;
    bool m_settingValid = true;
    bool m_valueValid = true;

    static constexpr const auto maxDuty()
    {
        return value_t{100};
    }

    auto dutyFraction() const
    {
        return safe_elastic_fixed_point<2, 29>(cnl::quotient(m_dutySetting + (cnl::numeric_limits<value_t>::min() >> 1), maxDuty()));
    }

    // separate flag for manually disabling the pwm actuator
    bool m_enabled = true;

#if PLATFORM_ID != PLATFORM_GCC
    uint8_t timerFuncId = 0;
    duration_millis_t m_fastPwmElapsed = 0;
#endif

public:
    /** Constructor.
     *  @param _m_
     target Digital actuator to be toggled with PWM
     *  @param _period PWM period in seconds
     *  @sa getPeriod(), setPeriod(), getTarget(), setTarget()
     */
    explicit ActuatorPwm(
        std::function<std::shared_ptr<ActuatorDigitalConstrained>()>&& target,
        duration_millis_t period = 4000);

    ~ActuatorPwm()
    {
        // ensure that interrupts are removed before destruction.
        enabled(false);
    }

    /** ActuatorPWM keeps track of the last high and low transition.
     *  This function returns the actually achieved value. This can differ from
     *  the set value, because the m_
     target actuator is not toggling.
     *
     * @return achieved duty cycle in fixed point.
     */
    virtual value_t value() const override final;

    /** Returns the set duty cycle
     * @return duty cycle setting in fixed point
     */
    virtual value_t setting() const override final
    {
        return m_dutySetting;
    }

    /** Sets a new duty cycle
     * @param val new duty cycle in fixed point
     */
    virtual void setting(const value_t& val) override final;

    update_t update(const update_t& now);

    //** Calculates whether the m_target should toggle and tries to toggle it if necessary
    /** Each update, the PWM actuator checks whether it should toggle to achieve the set duty cycle.
     * It checks wether the output pin toggled and updates it's internal counters to keep track of
     * the achieved duty cycle. When it toggles late, it tries to compensate for this in the next cycle.
     * To maintain the correct duty cycle average, it can make the next high time shorter or longer.
     * If needed, it can even skip going high or low. This will happen, for example, when the m_
     target is
     * a time limited actuator with a minimum on and/or off time.
     */
    update_t slowPwmUpdate(const update_t& now);

#if PLATFORM_ID != PLATFORM_GCC
    update_t fastUpdate(const update_t& now);

    /**
    When the period is less than 1000ms, switch to timer interrupt based tasks
    */
    void timerTask();

    void manageTimerTask();
#endif

    /** returns the PWM period
     * @return PWM period in seconds
     */
    duration_millis_t period() const;

    /** sets the PWM period
     * @param sec new period in seconds
     */
    void period(const duration_millis_t& p);

    virtual bool valueValid() const override final;

    virtual bool settingValid() const override final;

    virtual void settingValid(bool v) override final;

    bool enabled() const
    {
        return m_enabled;
    }

    void enabled(bool v)
    {
        if (!v && m_enabled) {
            settingValid(false);
        }
        m_enabled = v;
#if PLATFORM_ID != PLATFORM_GCC
        manageTimerTask();
#endif
    }
};
