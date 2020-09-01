/*
 * Copyright 2020 BrewPi B.V.
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

#include "../inc/TempSensorCombi.h"
#include "../inc/TempSensorMock.h"

SCENARIO("TempSensorCombiTest", "[TempSensorCombi]")
{
    WHEN("No sensors are added to the combi sensor")
    {
        TempSensorCombi combined;

        THEN("It reads as invalid and zero")
        {
            combined.update();
            combined.func = TempSensorCombi::CombineFunc::AVG;
            CHECK(combined.valid() == false);
            CHECK(combined.value() == temp_t(0));

            combined.update();
            combined.func = TempSensorCombi::CombineFunc::MIN;
            CHECK(combined.valid() == false);
            CHECK(combined.value() == temp_t(0));

            combined.update();
            combined.func = TempSensorCombi::CombineFunc::MAX;
            CHECK(combined.valid() == false);
            CHECK(combined.value() == temp_t(0));
        }
    }

    WHEN("4 sensors are added to the combi sensor")
    {
        TempSensorCombi combined;
        auto mock1 = std::make_shared<TempSensorMock>(20);
        auto mock2 = std::make_shared<TempSensorMock>(18);
        auto mock3 = std::make_shared<TempSensorMock>(26);
        auto mock4 = std::make_shared<TempSensorMock>(22);
        combined.inputs = {
            [mock1]() { return mock1; },
            [mock2]() { return mock2; },
            [mock3]() { return mock3; },
            [mock4]() { return mock4; },

        };

        THEN("The average is returned when AVG is selected")
        {
            combined.func = TempSensorCombi::CombineFunc::AVG;
            combined.update();
            CHECK(combined.valid() == true);
            CHECK(combined.value() == temp_t(21.5));
        }

        THEN("The lowest value is returned when MIN selected")
        {
            combined.func = TempSensorCombi::CombineFunc::MIN;
            combined.update();
            CHECK(combined.valid() == true);
            CHECK(combined.value() == temp_t(18));
        }

        THEN("The highest value is returned when MAX is selected")
        {
            combined.func = TempSensorCombi::CombineFunc::MAX;
            combined.update();
            CHECK(combined.valid() == true);
            CHECK(combined.value() == temp_t(26));
        }

        AND_WHEN("2 of the 4 sensors are not valid, they are ignored")
        {
            mock2->connected(false);
            mock3->connected(false);

            THEN("The average is returned when AVG is selected")
            {
                combined.func = TempSensorCombi::CombineFunc::AVG;
                combined.update();
                CHECK(combined.valid() == true);
                CHECK(combined.value() == temp_t(21));
            }

            THEN("The lowest value is returned when MIN selected")
            {
                combined.func = TempSensorCombi::CombineFunc::MIN;
                combined.update();
                CHECK(combined.valid() == true);
                CHECK(combined.value() == temp_t(20));
            }

            THEN("The highest value is returned when MAX is selected")
            {
                combined.func = TempSensorCombi::CombineFunc::MAX;
                combined.update();
                CHECK(combined.valid() == true);
                CHECK(combined.value() == temp_t(22));
            }
        }
    }
}