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

#include "ActuatorDigital.h"
#include "ActuatorDigitalConstrained.h"
#include "MockIoArray.h"

using State = ActuatorDigital::State;

SCENARIO("ActuatorDigitalConstrained", "[constraints]")
{
    auto now = ticks_millis_t(0);

    auto mockIo = std::make_shared<MockIoArray>();
    auto mock = ActuatorDigital([mockIo]() { return mockIo; }, 1);
    auto constrained = ActuatorDigitalConstrained(mock);

    WHEN("A minimum ON time constrained is added, the actuator cannot turn off before it has passed")
    {
        constrained.addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(1500));
        constrained.desiredState(State::Active, now);
        CHECK(constrained.state() == State::Active);
        CHECK(mock.state() == State::Active);

        now += 1499;
        constrained.desiredState(State::Inactive, now);
        CHECK(constrained.state() == State::Active);
        CHECK(mock.state() == State::Active);

        now += 1;
        constrained.desiredState(State::Inactive, now);
        CHECK(constrained.state() == State::Inactive);
        CHECK(mock.state() == State::Inactive);
    }

    WHEN("A minimum OFF time constrained is added, the actuator cannot turn on before it has passed")
    {
        constrained.addConstraint(std::make_unique<ADConstraints::MinOffTime<1>>(1500));
        constrained.desiredState(State::Inactive, now);
        CHECK(constrained.state() == State::Inactive);
        CHECK(mock.state() == State::Inactive);

        now += 1499;
        constrained.desiredState(State::Active, now);
        CHECK(constrained.state() == State::Inactive);
        CHECK(mock.state() == State::Inactive);

        now += 1;
        constrained.desiredState(State::Active, now);
        CHECK(constrained.state() == State::Active);
        CHECK(mock.state() == State::Active);
    }

    WHEN("A minimum ON and a minimum OFF time are added, both are honored")
    {
        constrained.desiredState(State::Inactive, now);
        constrained.addConstraint(std::make_unique<ADConstraints::MinOffTime<1>>(1000));
        constrained.addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(2000));

        while (constrained.state() == State::Inactive) {
            constrained.desiredState(State::Active, ++now);
        }
        auto timesOff = constrained.getLastStartEndTime(State::Inactive, now);
        CHECK(timesOff.end - timesOff.start == 1000);

        while (constrained.state() == State::Active) {
            constrained.desiredState(State::Inactive, ++now);
        }

        auto timesOn = constrained.getLastStartEndTime(State::Active, now);
        CHECK(timesOn.end - timesOn.start == 2000);
    }
}

SCENARIO("Mutex contraint", "[constraints]")
{
    auto now = ticks_millis_t(0);
    auto mockIo = std::make_shared<MockIoArray>();
    auto mock1 = ActuatorDigital([mockIo]() { return mockIo; }, 1);
    auto constrained1 = ActuatorDigitalConstrained(mock1);
    auto mock2 = ActuatorDigital([mockIo]() { return mockIo; }, 2);
    auto constrained2 = ActuatorDigitalConstrained(mock2);
    auto mut = std::make_shared<MutexTarget>();

    constrained1.addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
        [&mut]() {
            return mut;
        },
        0,
        true));
    constrained2.addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
        [&mut]() {
            return mut;
        },
        0,
        true));

    WHEN("Two actuators share a mutex, they cannot be active at the same time")
    {
        constrained1.desiredState(State::Active, ++now);
        CHECK(constrained1.state() == State::Active);
        constrained2.desiredState(State::Active, ++now);
        CHECK(constrained2.state() == State::Inactive);

        constrained1.desiredState(State::Inactive, ++now);
        constrained2.desiredState(State::Active, ++now);
        CHECK(constrained2.state() == State::Active);

        constrained1.desiredState(State::Active, ++now);
        CHECK(constrained1.state() == State::Inactive);
    }

    WHEN("A minimum OFF time constraint holds an actuator low, it doesn't lock the mutex")
    {
        constrained1.addConstraint(std::make_unique<ADConstraints::MinOffTime<1>>(1000));
        constrained1.desiredState(State::Active, ++now);
        CHECK(constrained1.state() == State::Inactive);
        constrained2.desiredState(State::Active, ++now);
        CHECK(constrained2.state() == State::Active);
    }

    WHEN("An actuator doesn't have the mutex, it won't unlock it when set to Inactive")
    {
        constrained1.desiredState(State::Active, ++now);
        CHECK(constrained1.state() == State::Active);
        constrained2.desiredState(State::Inactive, ++now);
        constrained2.desiredState(State::Active, ++now);
        CHECK(constrained2.state() == State::Inactive);
    }

    WHEN("An extra hold time of 1000 is set on actuator 1, and it was active earlier")
    {
        constrained1.removeAllConstraints();
        constrained2.removeAllConstraints();
        constrained1.addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [&mut]() {
                return mut;
            },
            1000, true));
        constrained2.addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [&mut]() {
                return mut;
            },
            0, true));

        constrained1.desiredState(State::Active, ++now);
        CHECK(constrained1.state() == State::Active);

        constrained1.desiredState(State::Inactive, ++now);
        CHECK(constrained1.state() == State::Inactive);

        THEN("Actuator 1 can go active again immediately")
        {
            constrained1.desiredState(State::Active, ++now);
            CHECK(constrained1.state() == State::Active);
        }

        THEN("Actuator 2 has to wait until no actuator has been active for 1000ms")
        {
            constrained2.desiredState(State::Active, ++now);
            CHECK(constrained2.state() == State::Inactive);
            CHECK(mut->timeRemaining() == 1000);

            while (constrained2.state() != State::Active && now < 2000) {
                ++now;
                constrained1.update(now);
                constrained2.update(now);
            }
            CHECK(now == 1002);

            AND_THEN("Actuar 2 does not hold the mutex longer after turn off")
            {
                constrained2.desiredState(State::Inactive, ++now);
                constrained1.desiredState(State::Active, ++now);
                CHECK(constrained1.state() == State::Active);
                CHECK(constrained2.state() == State::Inactive);
            }
        }

        THEN("Toggling actuator 1 again resets the wait time")
        {
            constrained2.desiredState(State::Active, ++now);
            CHECK(constrained2.state() == State::Inactive);
            CHECK(mut->timeRemaining() == 1000);

            while (constrained2.state() != State::Active && now < 500) {
                ++now;
                constrained1.update(now);
                constrained2.update(now);
            }
            CHECK(mut->timeRemaining() == 502);

            constrained1.desiredState(State::Active, ++now);
            constrained1.desiredState(State::Inactive, ++now);
            CHECK(mut->timeRemaining() == 1000);

            while (constrained2.state() != State::Active && now < 2000) {
                ++now;
                constrained1.update(now);
                constrained2.update(now);
            }

            CHECK(now == 1502);
        }

        THEN("When the Mutex target is unavailable the actuators cannot go active, but already active actuators will reference their old mutex until it unlocks")
        {
            mut.reset();
            constrained1.desiredState(State::Active, ++now);
            constrained2.desiredState(State::Active, ++now);
            CHECK(constrained1.state() == State::Active);
            CHECK(constrained2.state() == State::Inactive);

            constrained1.desiredState(State::Inactive, ++now);
            CHECK(constrained1.state() == State::Inactive);
            constrained1.desiredState(State::Active, ++now);
            CHECK(constrained1.state() == State::Active);
            constrained1.desiredState(State::Inactive, ++now);
            now += 1000;
            constrained1.desiredState(State::Inactive, ++now);
            constrained1.desiredState(State::Active, ++now);
            CHECK(constrained1.state() == State::Inactive);
        }
    }

    WHEN("A default extra hold time of 1000ms is set on the Mutex and only act 2 has a custom hold time of 100ms")
    {
        mut->holdAfterTurnOff(1000);

        constrained1.removeAllConstraints();
        constrained2.removeAllConstraints();
        constrained1.addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [&mut]() {
                return mut;
            },
            0, false));
        constrained2.addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [&mut]() {
                return mut;
            },
            100, true));

        constrained1.desiredState(State::Active, ++now);
        CHECK(constrained1.state() == State::Active);

        constrained1.desiredState(State::Inactive, ++now);
        CHECK(constrained1.state() == State::Inactive);

        THEN("Actuator 2 has to wait until actuator 1 has been inactive for 1000ms")
        {
            constrained2.desiredState(State::Active, ++now);
            CHECK(constrained2.state() == State::Inactive);

            while (constrained2.state() != State::Active && now < 2000) {
                ++now;
                constrained1.update(now);
                constrained2.update(now);
            }
            CHECK(now == 1002);
            CHECK(constrained1.state() == State::Inactive);
            CHECK(constrained2.state() == State::Active);

            WHEN("Actuator 2 turns off, it holds the mutex for 100ms")
            {
                constrained2.desiredState(State::Inactive, ++now);
                constrained1.desiredState(State::Active, ++now);
                CHECK(constrained1.state() == State::Inactive);
                CHECK(constrained2.state() == State::Inactive);

                while (constrained1.state() != State::Active && now < 2000) {
                    ++now;
                    constrained1.update(now);
                    constrained2.update(now);
                }
                CHECK(now == 1104);
            }
        }
    }

    WHEN("The state is changed without providing the current time, it is applied using the last update time")
    {
        constrained1.desiredState(State::Active, 2000);
        CHECK(constrained1.state() == State::Active);

        constrained1.update(6000);
        constrained1.desiredState(State::Inactive);

        auto activeTimes = constrained1.getLastStartEndTime(State::Active, 8000);
        auto inactiveTimes = constrained1.getLastStartEndTime(State::Inactive, 8000);

        CHECK(activeTimes.start == 2000);
        CHECK(activeTimes.end == 6000);

        CHECK(inactiveTimes.start == 6000);
        CHECK(inactiveTimes.end == 8000);
    }

    WHEN("Try to create deadlock with errouneous actuator")
    {
        WHEN("Target IO module cannot be reached")
        {
            mockIo->connected(false); // emulate disconnect

            THEN("Desired state is still set correctly")
            {
                CHECK(constrained1.desiredState() == State::Inactive);
                CHECK(constrained2.desiredState() == State::Inactive);
                CHECK(constrained1.state() == State::Unknown);
                CHECK(constrained2.state() == State::Unknown);

                constrained1.desiredState(State::Active, 2004);
                constrained2.desiredState(State::Active, 2005);
                constrained1.update(2006);
                constrained2.update(2006);
                CHECK(constrained1.desiredState() == State::Active);
                CHECK(constrained2.desiredState() == State::Active);
                CHECK(constrained1.state() == State::Unknown);
                CHECK(constrained2.state() == State::Unknown);

                AND_WHEN("The actuators are both turned off while disconnected")
                {
                    constrained1.desiredState(State::Inactive, 2007);
                    constrained2.desiredState(State::Inactive, 2008);
                    AND_WHEN("The target is connected again")
                    {
                        mockIo->connected(true);
                        THEN("Turning the actuators is still handled correctly by the mutex")
                        {
                            constrained1.desiredState(State::Active, 2009);
                            constrained2.desiredState(State::Active, 2010);
                            CHECK(constrained1.desiredState() == State::Active);
                            CHECK(constrained2.desiredState() == State::Active);
                            CHECK(constrained1.state() == State::Active);
                            CHECK(constrained2.state() == State::Inactive);
                        }
                    }
                }
            }
        }
    }
}
