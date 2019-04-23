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

#include "FixedPoint.h"
#include "FpFilterChain.h"
#include "ProcessValue.h"
#include <cstring>
#include <functional>

class Pid {
public:
    using in_t = fp12_t;
    using out_t = fp12_t;
    using integral_t = saturated_elastic_fixed_point<19, 12, int32_t>;
    using derivative_t = saturated_elastic_fixed_point<1, 23, int32_t>;

private:
    const std::function<std::shared_ptr<ProcessValue<in_t>>()> m_inputPtr;
    const std::function<std::shared_ptr<ProcessValue<out_t>>()> m_outputPtr;

    FpFilterChain<in_t> m_filter;

    // state
    in_t m_error = in_t{0};
    in_t m_p = in_t{0};
    in_t m_i = in_t{0};
    in_t m_d = in_t{0};
    integral_t m_integral = integral_t{0};
    derivative_t m_derivative = derivative_t{0};

    uint8_t m_inputFailureCount = 255; // force a reset on init

    // settings
    in_t m_kp = in_t{0};        // proportional gain
    uint16_t m_ti = 0;          // integral time constant
    uint16_t m_td = 0;          // derivative time constant
    uint8_t m_filterChoice = 0; // input filter index
    bool m_enabled = false;     // persisted setting to manually disable the pid
    bool m_active = false;      // automatically set when input is invalid

public:
    explicit Pid(
        std::function<std::shared_ptr<ProcessValue<in_t>>()>&& input,
        std::function<std::shared_ptr<ProcessValue<out_t>>()>&& output)
        : m_inputPtr(input)
        , m_outputPtr(output)
        , m_filter(0)
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

    void kp(const in_t& arg)
    {
        m_kp = arg;
    }

    auto ti() const
    {
        return m_ti;
    }

    void ti(const uint16_t& arg)
    {
        if (m_ti != 0) {
            // scale integral history so integral action doesn't change
            m_integral = m_integral * integral_t(cnl::fraction<>(arg, m_ti));
        }
        m_ti = arg;
    }

    auto td() const
    {
        return m_td;
    }

    void td(const uint16_t& arg)
    {
        m_td = arg;
    }

    auto filterChoice() const
    {
        return m_filterChoice;
    }

    auto filterThreshold() const
    {
        return m_filter.getStepThreshold();
    }

    void configureFilter(const uint8_t& choice, const in_t& threshold)
    {
        if (m_filterChoice != choice) {
            m_filterChoice = choice;
            m_filter.setParams(choice, threshold);
        } else {
            m_filter.setStepThreshold(threshold);
        }
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
