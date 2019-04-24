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

#include <catch.hpp>

#include "../inc/SetpointProfile.h"
#include "../inc/SetpointSensorPair.h"
#include "../inc/TempSensorMock.h"
#include "../inc/Temperature.h"
#include <cstdint>

SCENARIO("SetpointProfile test", "[SetpointProfile]")
{
    auto deviceStartTime = ticks_seconds_t(10);
    auto sensor = std::make_shared<TempSensorMock>(20.0);
    auto sspair = std::make_shared<SetpointSensorPair>([sensor]() { return sensor; });
    sspair->setting(99);
    sspair->settingValid(true);
    auto profile = SetpointProfile([sspair]() { return sspair; }, deviceStartTime);

    WHEN("the profile has no values, it does not change the setpoint")
    {
        CHECK(sspair->setting() == temp_t(99));
        CHECK(profile.isDriving() == false);

        profile.update(0);

        CHECK(sspair->setting() == temp_t(99));
        CHECK(profile.isDriving() == false);
    }

    WHEN("the profile contains a single temp")
    {
        profile.startTime(10);
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(1), temp_t(10)});

        AND_WHEN("the timestamp is in the future")
        {

            THEN("The profile doesn't change the setpoint, but does indicate it as driving")
            {
                profile.update(0);
                CHECK(sspair->setting() == temp_t(99));
                CHECK(profile.isDriving() == true);
                profile.update(999);
                CHECK(sspair->setting() == temp_t(99));
                CHECK(profile.isDriving() == true);
            }
        }
        AND_WHEN("the timestamp is in the past")
        {
            profile.update(2000);
            THEN("The profile sets the setpoint to this value")
            {
                CHECK(sspair->setting() == temp_t(10));
                CHECK(profile.isDriving() == true);
            }
        }
    }

    WHEN("the profile contains two temperatures")
    {
        profile.startTime(10);
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(1), temp_t(10)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(11), temp_t(20)});

        AND_WHEN("The time is before the first point, it doesn't change the setpoint, but it is shown as driving")
        {
            profile.update(500);
            CHECK(sspair->setting() == temp_t(99));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time is after the last point, the last temp is used")
        {
            profile.update(12000);
            CHECK(sspair->setting() == temp_t(20));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time between the 2 points, it is correctly interpolated")
        {
            profile.update(2000);
            CHECK(sspair->setting() == temp_t(11));
            CHECK(profile.isDriving() == true);

            profile.update(6000);
            CHECK(sspair->setting() == temp_t(15));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time is exactly at a point, that temp is used")
        {
            profile.update(1000);
            CHECK(sspair->setting() == temp_t(10));
            CHECK(profile.isDriving() == true);

            profile.update(11000);
            CHECK(sspair->setting() == temp_t(20));
            CHECK(profile.isDriving() == true);
        }
    }

    WHEN("the profile contains multiple temperatures")
    {
        profile.startTime(10);
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(1), temp_t(10)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(11), temp_t(20)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(21), temp_t(40)});

        AND_WHEN("The time is before the first point, it doesn't change the setpoint but is indicated as driving")
        {
            profile.update(500);
            CHECK(sspair->setting() == temp_t(99));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time is after the last point, the last temp is used")
        {
            profile.update(22000);
            CHECK(sspair->setting() == temp_t(40));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time between the 2 points, it is correctly interpolated")
        {
            profile.update(2000);
            CHECK(sspair->setting() == temp_t(11));
            CHECK(profile.isDriving() == true);

            profile.update(6000);
            CHECK(sspair->setting() == temp_t(15));
            CHECK(profile.isDriving() == true);

            profile.update(12000);
            CHECK(sspair->setting() == temp_t(22));
            CHECK(profile.isDriving() == true);

            profile.update(20000);
            CHECK(sspair->setting() == temp_t(38));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time is exactly at a point, that temp is used")
        {
            profile.update(1000);
            CHECK(sspair->setting() == temp_t(10));
            CHECK(profile.isDriving() == true);

            profile.update(11000);
            CHECK(sspair->setting() == temp_t(20));
            CHECK(profile.isDriving() == true);

            profile.update(21000);
            CHECK(sspair->setting() == temp_t(40));
            CHECK(profile.isDriving() == true);
        }
    }

    WHEN("the profile contains two temperatures with the same time")
    {
        profile.startTime(10);
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(1), temp_t(10)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(11), temp_t(20)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(11), temp_t(30)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(21), temp_t(40)});

        AND_WHEN("The time is before the step, it is correctly interpolated with the first point of the step")
        {
            profile.update(2000);
            CHECK(sspair->setting() == temp_t(11));
            CHECK(profile.isDriving() == true);

            profile.update(6000);
            CHECK(sspair->setting() == temp_t(15));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time is at the step (rounded down to whole seconds), it is equal to the last point of the step")
        {
            profile.update(11000);
            CHECK(sspair->setting() == temp_t(30));
            CHECK(profile.isDriving() == true);
        }

        AND_WHEN("The time is after the step, it is correctly interpolated with the second point of the step")
        {

            profile.update(12000);
            CHECK(sspair->setting() == temp_t(31));
            CHECK(profile.isDriving() == true);

            profile.update(21000);
            CHECK(sspair->setting() == temp_t(40));
            CHECK(profile.isDriving() == true);
        }
    }

    WHEN("The device start time is still at 0, the setpoint is unchanged")
    {
        deviceStartTime = 0;
        profile.startTime(10);
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(1), temp_t(10)});
        profile.addPoint(SetpointProfile::Point{ticks_seconds_t(11), temp_t(20)});

        profile.update(1000);
        CHECK(sspair->setting() == temp_t(99));
        CHECK(profile.isDriving() == false);

        profile.update(10000);
        CHECK(sspair->setting() == temp_t(99));
        CHECK(profile.isDriving() == false);
    }
}
