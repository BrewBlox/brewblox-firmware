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

#include "../inc/Pid.h"
#include "../inc/future_std.h"

void
Pid::update()
{
    auto input = m_inputPtr();
    if (input && input->settingValid() && input->valueValid()) {
        if (m_enabled) {
            active(true);
        }
        auto inputError = input->setting() - input->value();
        if (m_inputFailureCount >= 10) {
            m_filter.reset(inputError);
        }
        m_filter.add(inputError);
        m_inputFailureCount = 0;
    } else {
        if (m_inputFailureCount < 10) {
            ++m_inputFailureCount;
        } else {
            if (active()) {
                active(false);
            }
            return;
        }
    }
    if (!active()) {
        return;
    }

    // calculate PID parts.
    m_error = m_filter.read();
    m_p = m_kp * m_error;

    if (m_ti != 0) {
        m_integral += m_error;
        m_i = (m_integral * m_kp) / integral_t::rep(m_ti);
    } else {
        m_integral = 0;
        m_ti = 0;
    }

    m_derivative = m_filter.readDerivative<decltype(m_derivative)>();
    m_d = m_kp * (m_derivative * m_td);

    auto pidResult = m_p + m_i + m_d;

    out_t outputValue = pidResult;

    // try to set the output to the desired setting
    if (m_enabled) {

        if (auto output = m_outputPtr()) {
            output->settingValid(true);
            output->setting(outputValue);

            // get the clipped setting from the actuator
            if (output->settingValid()) {
                auto outputSetting = output->setting();

                if (m_ti != 0) { // 0 has been chosen to indicate that the integrator is disabled. This also prevents divide by zero.
                                 // update integral with anti-windup back calculation
                                 // pidResult - output is zero when actuator is not saturated

                    auto antiWindup = out_t(0);
                    if (m_kp != 0) { // prevent divide by zero
                        if (pidResult != outputSetting) {
                            // clipped to actuator min or max set in target actuator
                            // calculate anti-windup from setting instead of actual value, so it doesn't dip under the maximum
                            // make sure anti-windup is at least m_error when clipping to prevent further windup, with extra anti-windup to scale back integral
                            antiWindup = m_error + fp12_t(3 * (pidResult - outputSetting)) / m_kp; // anti windup gain is 3

                        } else {
                            // Actuator could be not reaching set value due to physics or limits in its target actuator
                            // Get the actual achieved value in actuator. This could differ due to slowness time/mutex limits
                            if (output->valueValid()) {
                                auto achievedValue = output->value();

                                // Anti windup gain is 3
                                antiWindup = fp12_t(3 * (pidResult - achievedValue)) / m_kp;

                                // Disable anti-windup if integral part dominates. But only if it counteracts p.
                                if (m_i < 0 && m_p < 0 && m_i < 3 * m_p) {
                                    antiWindup = 0;
                                }
                                if (m_i > 0 && m_p > 0 && m_i > 3 * m_p) {
                                    antiWindup = 0;
                                }
                            }
                        }
                    }

                    // make sure integral does not cross zero and does not increase by anti-windup
                    integral_t newIntegral = m_integral - antiWindup;
                    if (m_integral >= 0) {
                        m_integral = std::clamp(newIntegral, integral_t(0), m_integral);
                    } else {
                        m_integral = std::clamp(newIntegral, m_integral, integral_t(0));
                    }
                }
            }
        }
    }
}
