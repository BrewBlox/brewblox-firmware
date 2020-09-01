/*
 * Copyright 2020 BrewPi B.V.
 *
 * This file is part of the BrewBlox Control Library.
 *
 * Brewblox is free software: you can redistribute it and/or modify
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
 * along with Brewblox.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "TempSensor.h"
#include <functional>
#include <vector>

/*
 * A process value has a setting and an current value
 */
class TempSensorCombi : public TempSensor {
public:
    enum class CombineFunc : uint8_t {

        AVG = 0,
        MIN = 1,
        MAX = 2,
    };

    std::vector<std::function<std::shared_ptr<TempSensor>()>> inputs;
    CombineFunc func = CombineFunc::AVG;

private:
    temp_t m_value = temp_t{0};
    bool m_valid = false;

public:
    TempSensorCombi() = default;
    virtual ~TempSensorCombi() = default;

    virtual bool valid() const override final
    {
        return m_valid;
    }

    virtual temp_t value() const override final
    {
        return m_value;
    }

    void update()
    {
        m_value = 0;
        m_valid = false;
        switch (func) {
        case CombineFunc::AVG: {
            auto sum = safe_elastic_fixed_point<18, 12>{0};
            uint16_t count = 0;
            for (auto sensorLookup : inputs) {
                if (auto sens = sensorLookup()) {
                    if (sens->valid()) {
                        ++count;
                        sum += sens->value();
                    }
                }
            }
            if (count > 0) {
                m_valid = true;
                m_value = sum / count;
            } else {
                m_valid = false;
            }
            return;
        }
        case CombineFunc::MIN: {
            for (auto sensorLookup : inputs) {
                if (auto sens = sensorLookup()) {
                    if (sens->valid()) {
                        if (m_valid) {
                            auto v = sens->value();
                            if (v < m_value) {
                                m_value = v;
                            }
                        } else {
                            m_value = sens->value();
                        }
                        m_valid = true;
                    }
                }
            }
            return;
        }
        case CombineFunc::MAX: {
            for (auto sensorLookup : inputs) {
                if (auto sens = sensorLookup()) {
                    if (sens->valid()) {
                        if (m_valid) {
                            auto v = sens->value();
                            if (v > m_value) {
                                m_value = v;
                            }
                        } else {
                            m_value = sens->value();
                        }
                        m_valid = true;
                    }
                }
            }
            return;
        }
        }
    }
};
