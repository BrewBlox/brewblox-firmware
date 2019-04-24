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

#include "SetpointSensorPair.h"
#include "Temperature.h"
#include "TicksTypes.h"
#include <algorithm>
#include <vector>

class SetpointProfile {
public:
    struct Point {
        ticks_seconds_t time;
        temp_t temp;
    };

private:
    const std::function<std::shared_ptr<SetpointSensorPair>()> m_target;
    const ticks_seconds_t& m_deviceStartTime;
    ticks_seconds_t m_profileStartTime = 0;
    bool m_enabled = true;

    std::vector<Point> m_points;

public:
    explicit SetpointProfile(
        std::function<std::shared_ptr<SetpointSensorPair>()>&& target, // process value to manipulate setpoint of
        const ticks_seconds_t& deviceStartTimeRef)
        : m_target(target)
        , m_deviceStartTime(deviceStartTimeRef)
    {
    }

    void addPoint(Point&& p)
    {
        m_points.push_back(std::move(p));
    }

    void removeAllPoints()
    {
        m_points.clear();
    }

    bool isDriving() const
    {
        return (m_enabled && !m_points.empty() && m_deviceStartTime != 0);
    }

    void update(const ticks_millis_t& now)
    {
        struct TimeStampLessEqual {
            bool operator()(const Point& p, const ticks_seconds_t& time) const { return p.time <= time; }
            bool operator()(const ticks_seconds_t& time, const Point& p) const { return time <= p.time; }
        };

        if (!isDriving()) {
            return;
        }

        auto newTemp = temp_t(0);

        if (!m_points.empty() && m_deviceStartTime != 0) {

            auto nowSeconds = ticks_seconds_t(now / 1000) + m_deviceStartTime;
            auto elapsed = nowSeconds - m_profileStartTime;
            auto upper = std::lower_bound(m_points.cbegin(), m_points.cend(), elapsed, TimeStampLessEqual{});
            if (upper == m_points.cend()) { // every point is in the past, use the last point
                newTemp = m_points.back().temp;
            } else if (upper != m_points.cbegin()) { // first point is not in the future
                auto lower = upper - 1;
                auto segmentElapsed = elapsed - lower->time;
                auto segmentDuration = upper->time - lower->time;

                temp_t fraction = ((upper->temp - lower->temp) * segmentElapsed) / segmentDuration;

                auto interpolated = lower->temp + fraction;
                newTemp = interpolated;
            } else {
                return;
            }
            if (auto targetPtr = m_target()) {
                targetPtr->setting(newTemp);
                targetPtr->settingValid(true);
            }
        }
    }

    bool enabled() const
    {
        return m_enabled;
    }

    void enabled(bool v)
    {
        m_enabled = v;
    }

    const std::vector<Point>& points() const
    {
        return m_points;
    }

    void points(std::vector<Point>&& newPoints)
    {
        m_points = newPoints;
    }

    ticks_seconds_t startTime() const
    {
        return m_profileStartTime;
    }

    void startTime(ticks_seconds_t v)
    {
        m_profileStartTime = v;
    }
};
