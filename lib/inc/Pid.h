/*
 * Copyright 2015 BrewPi / Elco Jacobs
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

#include "ProcessValue.h"
#include "SetpointSensorPair.h"
#include <cstring>
#include <functional>

class Pid {
public:
    using in_t = fp12_t;
    using out_t = fp12_t;
    using integral_t = safe_elastic_fixed_point<18, 12>;
    using derivative_t = SetpointSensorPair::derivative_t;

private:
    const std::function<std::shared_ptr<SetpointSensorPair>()> m_inputPtr;
    const std::function<std::shared_ptr<ProcessValue<out_t>>()> m_outputPtr;

    // state
    in_t m_error = in_t{0};
    out_t m_p = out_t{0};
    out_t m_i = out_t{0};
    out_t m_d = out_t{0};
    integral_t m_integral = integral_t{0};
    derivative_t m_derivative = derivative_t{0};

    // settings
    in_t m_kp = in_t{0};    // proportional gain
    uint16_t m_ti = 0;      // integral time constant
    uint16_t m_td = 0;      // derivative time constant
    bool m_enabled = false; // persisted setting to manually disable the pid
    bool m_active = false;  // automatically set when input is invalid

    in_t m_boilPointAdjust = in_t{0}; // offset from 100C for lower limit to activate boil mode

    out_t m_boilMinOutput = out_t{0}; // minimum output when boiling (to control boil intensity)

    bool m_boilModeActive = false;

public:
    explicit Pid(
        std::function<std::shared_ptr<SetpointSensorPair>()>&& input,
        std::function<std::shared_ptr<ProcessValue<out_t>>()>&& output)
        : m_inputPtr(input)
        , m_outputPtr(output)
    {
    }

    ~Pid() = default;

    void init();

    void update();

    // state
    auto error() const
    {
        return m_error;
    }

    integral_t integral() const
    {
        return m_integral;
    }

    auto derivative() const
    {
        return m_derivative;
    }

    auto p() const
    {
        return m_p;
    }

    auto i() const
    {
        return m_i;
    }

    auto d() const
    {
        return m_d;
    }

    // settings
    auto kp() const
    {
        return m_kp;
    }

    void kp(const in_t& arg);

    auto ti() const
    {
        return m_ti;
    }

    void ti(const uint16_t& arg);

    auto td() const
    {
        return m_td;
    }

    void td(const uint16_t& arg)
    {
        m_td = arg;
    }

    void enabled(bool state)
    {
        active(state);
        m_enabled = state;
    }

    auto enabled() const
    {
        return m_enabled;
    }

    auto active() const
    {
        return m_active;
    }
    void setIntegral(const out_t& newIntegratorPart);

    bool boilModeActive() const
    {
        return m_boilModeActive;
    }

    in_t boilPointAdjust() const
    {
        return m_boilPointAdjust;
    }

    void boilPointAdjust(const in_t& v)
    {
        m_boilPointAdjust = v;
    }

    out_t boilMinOutput() const
    {
        return m_boilMinOutput;
    }

    void boilMinOutput(const out_t& v)
    {
        m_boilMinOutput = v;
    }

private:
    void active(bool state)
    {
        if (m_enabled && m_active && !state) {
            if (auto ptr = m_outputPtr()) {
                ptr->setting(0);
                ptr->settingValid(false);
            }
        }
        m_active = state;
    }
};
