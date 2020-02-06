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

#include "../inc/SetpointProfile.h"

void
SetpointProfile::update(const utc_seconds_t& time)
{
    struct TimeStampLessEqual {
        bool operator()(const Point& p, const utc_seconds_t& time) const { return p.time <= time; }
        bool operator()(const utc_seconds_t& time, const Point& p) const { return time <= p.time; }
    };

    if (!isDriving()) {
        return;
    }

    auto newTemp = temp_t(0);

    if (!m_points.empty() && time != 0) {

        if (m_profileStartTime > time) {
            return;
        }
        auto elapsed = time - m_profileStartTime;
        auto upper = std::lower_bound(m_points.cbegin(), m_points.cend(), elapsed, TimeStampLessEqual{});
        if (upper == m_points.cend()) { // every point is in the past, use the last point
            newTemp = m_points.back().temp;
        } else if (upper != m_points.cbegin()) { // first point is not in the future
            auto lower = upper - 1;
            auto segmentElapsed = elapsed - lower->time;
            auto segmentDuration = upper->time - lower->time;
            auto fraction = safe_elastic_fixed_point<1, 30>(cnl::quotient(segmentElapsed, segmentDuration));
            auto interpolated = lower->temp + temp_t((upper->temp - lower->temp) * fraction);
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