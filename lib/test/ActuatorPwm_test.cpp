/*
 * Copyright 2018 BrewPi/Elco Jacobs.
 *
 * This file is part of BrewBlox.
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

#include <stdlib.h> /* srand, rand */

#include "ActuatorAnalogConstrained.h"
#include "ActuatorDigital.h"
#include "ActuatorDigitalConstrained.h"
#include "ActuatorPwm.h"
#include "Balancer.h"
#include "MockIoArray.h"
#include <cmath> // for sin
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

#define PRINT_TOGGLE_TIMES 0

using value_t = ActuatorAnalog::value_t;
using State = ActuatorDigitalBase::State;

const auto output = decltype(&std::cout)(nullptr);
// auto output = &std::cout; // uncomment for stdout output

double
randomIntervalTest(const int& numPeriods,
                   ActuatorPwm& pwm,
                   ActuatorDigital& target,
                   const value_t& duty,
                   const duration_millis_t& delayMax,
                   ticks_millis_t& now)
{
    pwm.setting(duty);
    ticks_millis_t lowToHighTime = now;
    ticks_millis_t highToLowTime = now;
    ticks_millis_t totalHighTime = 0;
    ticks_millis_t totalLowTime = 0;
    ticks_millis_t nextUpdate = now;
    if (output) {
        *output << std::endl
                << std::endl
                << "*** Results running 100 periods and random 1-"
                << delayMax << " ms update intervals,"
                << " with duty cycle " << duty
                << " and period " << pwm.period()
                << " ***" << std::endl;

#if PRINT_TOGGLE_TIMES
        *output << std::endl
                << std::endl
                << "l->h time        h->l time       high time       low time    value   period"
                << std::endl;
#endif
    }

    for (int i = 0; i < numPeriods + 4; i++) {
        do {
            now += 1 + std::rand() % delayMax;
            if (now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
        } while (target.state() == State::Active);
        highToLowTime = now;
        ticks_millis_t highTime = highToLowTime - lowToHighTime;
        if (i >= 4) {
            totalHighTime += highTime;
        }
#if PRINT_TOGGLE_TIMES
        if (output) {
            *output << std::setw(10) << lowToHighTime
                    << "\t"
                    << std::setw(10) << highToLowTime
                    << "\t"
                    << std::setw(10) << highTime;
        }
#endif
        do {
            now += 1 + std::rand() % delayMax;
            if (now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
        } while (target.state() == State::Inactive);
        lowToHighTime = now;
        ticks_millis_t lowTime = lowToHighTime - highToLowTime;
        if (i >= 4) {
            totalLowTime += lowTime;
        }
#if PRINT_TOGGLE_TIMES
        if (output) {
            *output << "\t"
                    << std::setw(10) << lowTime
                    << std::setw(10) << pwm.value()
                    << std::setw(10) << lowTime + highTime
                    << std::endl;
        }
#endif
    }
    double totalTime = totalHighTime + totalLowTime;
    double avgDuty = double(totalHighTime) / (totalHighTime + totalLowTime) * double(100);
    if (output) {
        *output << "total high time: " << totalHighTime << "\n"
                << "total low time: " << totalLowTime << "\n"
                << "avg duty: " << avgDuty << "/100\n"
                << "avg period: " << totalTime / numPeriods << "\n"
                << std::endl;
    }
    return avgDuty;
}
SCENARIO("ActuatorPWM driving mock actuator", "[pwm]")
{
    auto now = ticks_millis_t(0);

    auto mockIo = std::make_shared<MockIoArray>();
    ActuatorDigital mock([mockIo]() { return mockIo; }, 1);

    auto constrained = std::make_shared<ActuatorDigitalConstrained>(mock);
    ActuatorPwm pwm([constrained]() { return constrained; }, 4000);

    WHEN("Actuator setting is written, setting is contrained between  0-100")
    {
        CHECK(pwm.setting() == value_t(0)); // PWM value is initialized to 0

        // Test that PWM can be set and read
        pwm.setting(50);
        CHECK(pwm.setting() == value_t(50));

        pwm.setting(100);
        CHECK(pwm.setting() == value_t(100));

        pwm.setting(0);
        CHECK(pwm.setting() == value_t(0));

        pwm.setting(110);
        CHECK(pwm.setting() == value_t(100)); // max is 100

        pwm.setting(-50.0);
        CHECK(pwm.setting() == value_t(0)); // min is 0
    }

    WHEN("update is called without delay, low and high times are correct")
    {
        auto duty = value_t(50);
        pwm.setting(duty);
        ticks_millis_t lowToHighTime1 = now;
        ticks_millis_t highToLowTime1 = now;
        ticks_millis_t lowToHighTime2 = now;
        auto nextUpdate = now;

        do {
            if (++now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
        } while (mock.state() == State::Inactive);
        lowToHighTime1 = now;
        do {
            if (++now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
        } while (mock.state() == State::Active);
        highToLowTime1 = now;
        do {
            if (++now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
        } while (mock.state() == State::Inactive);
        lowToHighTime2 = now;

        ticks_millis_t timeHigh = highToLowTime1 - lowToHighTime1;
        ticks_millis_t timeLow = lowToHighTime2 - highToLowTime1;
        double actualDuty = (timeHigh * 100.0) / (timeHigh + timeLow);
        if (output) {
            *output << "*** Timestamps testing one period with duty cycle " << duty << " and period " << pwm.period() << "***\n";
            *output << "lowToHigh1: " << lowToHighTime1 << "\t"
                    << "highToLow1: " << highToLowTime1 << " \t lowToHigh2: " << lowToHighTime2 << "\n"
                    << "time high: " << timeHigh << "\t"
                    << "time low: " << timeLow << "\t"
                    << "actual duty cycle: " << actualDuty;
        }
        CHECK(actualDuty == Approx(50.0).epsilon(0.01));
    }

    WHEN("update is called without delays, the average duty cycle is correct")
    {
        CHECK(randomIntervalTest(100, pwm, mock, 49.0, 1, now) == Approx(49.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, mock, 50.0, 1, now) == Approx(50.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, mock, 51.0, 1, now) == Approx(51.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, mock, 2.0, 1, now) == Approx(2.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, mock, 98.0, 1, now) == Approx(98.0).margin(0.2));
    }

    WHEN("update interval is random, the average duty cycle is still correct")
    {
        CHECK(randomIntervalTest(100, pwm, mock, 50.0, 500, now) == Approx(50.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 20.0, 500, now) == Approx(20.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 80.0, 500, now) == Approx(80.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 2.0, 500, now) == Approx(2.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 98.0, 500, now) == Approx(98.0).margin(1));
    }

    WHEN("Average_duty_cycle_is_correct_with_very long_period")
    {
        pwm.period(3600000);
        CHECK(randomIntervalTest(10, pwm, mock, 40.0, 500, now) == Approx(40.0).margin(0.1));
    }

    WHEN("the PWM actuator is set to zero, the output stays low")
    {
        pwm.setting(0);
        // wait target to go low
        while (mock.state() != State::Inactive) {
            pwm.update(now++);
        }
        // INFO(now);
        for (; now < 10 * pwm.period(); now += 100) {
            pwm.update(now);
            // INFO(now);
            REQUIRE(mock.state() == State::Inactive);
        }
    }

    WHEN("the PWM actuator is set to 100, the output stays high")
    {
        pwm.setting(100);
        // wait target to go low
        while (mock.state() != State::Active) {
            pwm.update(now++);
        }
        // INFO(now);
        for (; now < 10 * pwm.period(); now += 100) {
            pwm.update(now);
            // INFO(now);
            REQUIRE(mock.state() == State::Active);
        }
    }

    WHEN("the PWM actuator is set to 50, and infrequently but regularly updated, "
         "the achieved value is correctly calculated at all moments in the period")
    {
        pwm.setting(50);
        auto nextUpdateTime = now;
        for (; now < 5 * pwm.period(); now += 211) {
            if (now > nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
        }
        for (; now < 50 * pwm.period(); now += 211) {
            if (now > nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
            // INFO(now);
            CHECK(pwm.value() == Approx(50).margin(0.1));
        }
    }

    WHEN("the period is changed, the duty cycle is updated")
    {
        pwm.setting(50);
        pwm.period(30000);
        auto nextUpdateTime = now;
        for (; now < 2 * pwm.period(); now += 1) {
            if (now > nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
        }
        auto durations = constrained->activeDurations(now);
        CHECK(durations.previousActive == 2000);
    }

    WHEN("the PWM actuator is set to 50, and infrequently and irregularly updated, "
         "the achieved value is correctly calculated at all moments in the period")
    {
        pwm.setting(50);
        auto nextUpdateTime = now;
        for (; now < 5 * pwm.period(); now += std::rand() % 250) {
            if (now > nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
        }
        for (; now < 50 * pwm.period(); now += std::rand() % 250) {
            if (now > nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
            // INFO(now);
            CHECK(pwm.value() == Approx(50).margin(3));
            // INFO(double(pwm.value()));
            auto durations = constrained->activeDurations(now);
            CHECK(durations.previousActive >= 2000);
            CHECK(durations.previousActive <= 2250);
            CHECK(durations.previousPeriod >= 3500);
            CHECK(durations.previousPeriod <= 5000);
        }
    }

    WHEN("the PWM actuator is set to 90% right after initialization, it doesn't stay low longer than the normal low period")
    {
        pwm.setting(90);
        // wait target to go low
        while (mock.state() != State::Inactive) {
            now = pwm.update(now);
        }
        CHECK(now <= 400);
    }

    WHEN("the PWM actuator is set to 60% after being 99% for a long time, the low time has the normal duration")
    {
        pwm.setting(99);
        while (now < 10000 || mock.state() == State::Active) {
            now = pwm.update(now);
        }

        pwm.setting(60);
        pwm.update(now);

        CHECK(mock.state() == State::Inactive);
        auto lowStartTime = now;
        while (mock.state() == State::Inactive) {
            now = pwm.update(now);
        }
        auto lowTime = now - lowStartTime;
        CHECK(lowTime == pwm.period() * 0.4);
    }

    WHEN("the PWM actuator is set to 40% after being 99% for a long time, the low time is stretched to compensate for the history")
    {
        pwm.setting(99);
        while (now < 100000 || mock.state() == State::Active) {
            now = pwm.update(now);
        }

        pwm.setting(40);
        pwm.update(now);

        auto lowStartTime = now;
        while (mock.state() == State::Inactive) {
            now = pwm.update(now);
        }
        auto lowTime = now - lowStartTime;
        CHECK(lowTime == pwm.period() * 0.6 * 1.5);
    }

    WHEN("the PWM actuator is set to 40% after being 1% for a long time, the high time has the normal duration")
    {
        pwm.setting(1);
        while (now < 100000 || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(40);
        pwm.update(now);

        auto highStartTime = now;
        while (mock.state() == State::Active) {
            now = pwm.update(now);
        }
        auto highTime = now - highStartTime;
        CHECK(highTime == pwm.period() * 0.4);
    }

    WHEN("the PWM actuator is set to 60% after being 1% for a long time, the high time is strechted to compensate for the history")
    {
        pwm.setting(1);
        while (now < 100000 || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(60);
        pwm.update(now);

        auto highStartTime = now;
        while (mock.state() == State::Active) {
            now = pwm.update(now);
        }
        auto highTime = now - highStartTime;
        CHECK(highTime == pwm.period() * 0.6 * 1.5);
    }

    WHEN("the PWM actuator is set to 60% after being 1% for a long time, the high time is strechted to compensate for the history")
    {
        pwm.setting(1);
        while (now < 100000 || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(60);
        pwm.update(now);

        auto highStartTime = now;
        while (mock.state() == State::Active) {
            now = pwm.update(now);
        }
        auto highTime = now - highStartTime;
        CHECK(highTime == pwm.period() * 0.6 * 1.5);
    }

    WHEN("the PWM actuator is set to 60% after being 1% for a long time, the high time is strechted to compensate for the history")
    {
        pwm.setting(1);
        while (now < 100000 || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(60);
        pwm.update(now);

        auto highStartTime = now;
        while (mock.state() == State::Active) {
            now = pwm.update(now);
        }
        auto highTime = now - highStartTime;
        CHECK(highTime == pwm.period() * 0.6 * 1.5);
    }

    WHEN("the PWM actuator is set to 60% after being 1% for a long time, "
         "with a  minimum ON time constraint active, the high time is not strechted beyond 1.5x normal duration")
    {
        constrained->addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(1000)); // 1 second
        pwm.setting(1);
        while (now < 100000 || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(60);
        pwm.update(now);

        auto highStartTime = now;
        while (mock.state() == State::Active) {
            now = pwm.update(now);
        }
        auto highTime = now - highStartTime;
        CHECK(highTime == pwm.period() * 0.6 * 1.5);
    }

    WHEN("the PWM actuator is set to 40% after being 99% for a long time, "
         "with a  minimum OFF time constraint active, the low time is not strechted beyond 1.5x normal duration")
    {
        constrained->addConstraint(std::make_unique<ADConstraints::MinOffTime<1>>(1000)); // 1 second
        pwm.setting(99);
        while (now < 100000 || mock.state() == State::Active) {
            now = pwm.update(now);
        }

        pwm.setting(40);
        pwm.update(now);

        auto lowStartTime = now;
        while (mock.state() == State::Inactive) {
            now = pwm.update(now);
        }
        auto lowTime = now - lowStartTime;
        CHECK(lowTime == pwm.period() * 0.6 * 1.5);
    }

    WHEN("the PWM actuator is set from 0 to 50, it goes high immediately")
    {
        pwm.setting(0);
        for (; now < 5 * pwm.period(); now += 1000) {
            pwm.update(now);
        }
        pwm.setting(50);
        pwm.update(now + 1);
        CHECK(mock.state() == State::Active);
    }

    WHEN("the PWM actuator is set from 100 to 50, it goes low immediately")
    {
        pwm.setting(100);
        for (; now < 5 * pwm.period(); now += 1000) {
            pwm.update(now);
        }
        pwm.setting(50);
        pwm.update(now + 1);
        CHECK(mock.state() == State::Inactive);
    }

    WHEN("PWM actuator target is constrained with a minimal ON time and minimum OFF time, average is still correct")
    {
        // values typical for a fridge compressor
        pwm.period(2400000);                                                                // 40 minutes
        constrained->addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(300000));  // 5 minutes
        constrained->addConstraint(std::make_unique<ADConstraints::MinOffTime<1>>(600000)); // 10 minutes

        // use max delay of 5000 here to speed up tests
        CHECK(randomIntervalTest(10, pwm, mock, 50.0, 5000, now) == Approx(50.0).margin(0.5));
        CHECK(randomIntervalTest(10, pwm, mock, 20.0, 5000, now) == Approx(20.0).margin(0.5));
        CHECK(randomIntervalTest(10, pwm, mock, 80.0, 5000, now) == Approx(80.0).margin(0.5));

        // we don't use 2% and 98% here, because with the maximum history taken into account it is not achievable under the constraints
        CHECK(randomIntervalTest(10, pwm, mock, 4.0, 5000, now) == Approx(4.0).margin(0.5));
        CHECK(randomIntervalTest(10, pwm, mock, 96.0, 5000, now) == Approx(96.0).margin(0.5));
    }

    WHEN("The actuator has been set to 30% duty and switches to 20% with minimum ON time at 40% duty")
    {
        pwm.period(10000);                                                               // 10s
        constrained->addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(4000)); // 4 s
        pwm.setting(30);                                                                 // will result in 4s on, 13.3s period

        auto nextUpdate = pwm.update(now);

        while (now < 100000) {
            nextUpdate = pwm.update(now);
            now = nextUpdate;
        }

        auto durations = constrained->activeDurations(now);
        CHECK(durations.previousPeriod == Approx(13333).margin(10));

        pwm.setting(20);
        nextUpdate = pwm.update(now);

        THEN("The output is turned off when the minimum ON time is elapsed")
        {
            while (mock.state() == State::Active) {
                nextUpdate = pwm.update(now);
                now = nextUpdate;
            }
            auto durations = constrained->activeDurations(now);
            CHECK(durations.previousActive == 4000);

            AND_THEN("The next low time is stretched to 1.5x the previous one")
            {
                while (mock.state() == State::Inactive) {
                    nextUpdate = pwm.update(now);
                    now = nextUpdate;
                }
                while (mock.state() == State::Active) {
                    nextUpdate = pwm.update(now);
                    now = nextUpdate;
                }
                durations = constrained->activeDurations(now);
                CHECK(durations.previousPeriod == Approx(18000).margin(10)); // 9333 * 1.5 + 4000
            }
        }
        THEN("It pwm will settle on the desired duty")
        {
            while (now < 200000) {
                now = pwm.update(now);
            }

            CHECK(pwm.value() == Approx(20.0).margin(0.001));
            auto durations = constrained->activeDurations(now);
            CHECK(double(durations.previousActive) / double(durations.previousPeriod) == Approx(0.2).margin(0.0001));
            AND_THEN("The stretched periods will have expected length")
            {
                // finish period
                while (mock.state() == State::Inactive) {
                    now += 100;
                    pwm.update(now);
                }
                durations = constrained->activeDurations(now);
                CHECK(durations.previousPeriod == Approx(20000).margin(2));

                // finish another period

                while (mock.state() == State::Active) {
                    now += 100;
                    pwm.update(now);
                }

                while (mock.state() == State::Inactive) {
                    now += 100;
                    pwm.update(now);
                }

                durations = constrained->activeDurations(now);

                CHECK(durations.previousPeriod == Approx(20000).margin(2));
            }
        }
    }

    WHEN("PWM actuator target is constrained with a minimal ON time")
    {
        pwm.period(10000);                                                               // 10s
        constrained->addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(2000)); // 2 s
        pwm.setting(10);
        auto nextUpdate = pwm.update(now);
        THEN("the achieved value is always correct once adjusted")
        {
            for (; now < 100000; now += 100) {
                if (now >= nextUpdate) {
                    nextUpdate = pwm.update(now);
                    if (now > 60000) {
                        CHECK(pwm.value() == Approx(10).margin(2));
                    }
                }
            }
        }
    }

    WHEN("PWM actuator is set to invalid, the output pin is set low")
    {
        pwm.setting(100);
        pwm.update(now);

        CHECK(mock.state() == State::Active);

        pwm.settingValid(false);

        CHECK(mock.state() == State::Inactive);
    }

    WHEN("The target actuator returns an unknown state, the value is marked invalid")
    {
        pwm.setting(10);
        mockIo->setChannelError(1, true);
        constrained->update(now);
        pwm.update(now);

        CHECK(pwm.valueValid() == false);
    }
}

SCENARIO("Two PWM actuators driving mutually exclusive digital actuators")
{
    auto now = ticks_millis_t(0);
    auto period = duration_millis_t(4000);

    auto mockIo = std::make_shared<MockIoArray>();
    ActuatorDigital mock1([mockIo]() { return mockIo; }, 1);

    auto constrainedMock1 = std::make_shared<ActuatorDigitalConstrained>(mock1);
    ActuatorPwm pwm1([constrainedMock1]() { return constrainedMock1; }, 4000);
    ActuatorAnalogConstrained constrainedPwm1(pwm1);
    ActuatorDigital mock2([mockIo]() { return mockIo; }, 2);
    auto constrainedMock2 = std::make_shared<ActuatorDigitalConstrained>(mock2);
    ActuatorPwm pwm2([constrainedMock2]() { return constrainedMock2; }, 4000);
    ActuatorAnalogConstrained constrainedPwm2(pwm2);

    auto mut = std::make_shared<MutexTarget>();
    auto balancer = std::make_shared<Balancer<2>>();

    constrainedMock1->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
        [mut]() {
            return mut;
        },
        0,
        true));
    constrainedMock2->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
        [mut]() {
            return mut;
        },
        0,
        true));

    auto checkDuties = [&](value_t duty1, value_t duty2, double expected1, double expected2) {
        auto timeHigh1 = duration_millis_t(0);
        auto timeHigh2 = duration_millis_t(0);
        auto timeIdle = duration_millis_t(0);

        auto nextUpdate1 = ticks_millis_t(now);
        auto nextUpdate2 = ticks_millis_t(now);

        constrainedPwm1.setting(duty1);
        constrainedPwm2.setting(duty2);

        auto start = now;
        while (++now - start <= 100 * period) {
            if (now >= nextUpdate1) {
                nextUpdate1 = pwm1.update(now);
                REQUIRE(!(mock1.state() == State::Active && mock2.state() == State::Active)); // not active at the same time
            }
            if (now >= nextUpdate2) {
                nextUpdate2 = pwm2.update(now);
                REQUIRE(!(mock1.state() == State::Active && mock2.state() == State::Active)); // not active at the same time
            }
            if (now % 1000 == 0) {
                // keep setting the value to the constrained PWM, this is when the balancer gets its values. TODO: change that this is necessary ?
                balancer->update();
                constrainedPwm1.setting(duty1);
                constrainedPwm2.setting(duty2);
            }
            if (mock1.state() == State::Active) {
                timeHigh1++;
            } else if (mock2.state() == State::Active) {
                timeHigh2++;
            } else {
                timeIdle++;
            }
        }
        auto timeTotal = timeHigh1 + timeHigh2 + timeIdle;
        // INFO(std::to_string(timeHigh1) + ", " + std::to_string(timeHigh2) + ", " + std::to_string(timeIdle));
        auto avgDuty1 = double(timeHigh1) * 100 / timeTotal;
        auto avgDuty2 = double(timeHigh2) * 100 / timeTotal;
        CHECK(avgDuty1 == Approx(double(expected1)).margin(0.5));
        CHECK(avgDuty2 == Approx(double(expected2)).margin(0.5));
        bool duty2_correct = avgDuty1 == Approx(double(expected1)).margin(0.5);
        bool duty1_correct = avgDuty2 == Approx(double(expected2)).margin(0.5);
        return duty1_correct && duty2_correct; // also return result to find which call triggered the fail
    };

    WHEN("The sum of duty cycles is under 100, they can both reach their target by alternating")
    {
        CHECK(checkDuties(40, 50, 40, 50));
        CHECK(checkDuties(50, 50, 50, 50));
        CHECK(checkDuties(30, 60, 30, 60));
    }

    WHEN("A balancing constraint is added")
    {
        constrainedPwm1.addConstraint(std::make_unique<AAConstraints::Balanced<2>>([balancer]() { return balancer; }));
        constrainedPwm2.addConstraint(std::make_unique<AAConstraints::Balanced<2>>([balancer]() { return balancer; }));

        THEN("Achieved duty cycle matches the setting if the total is under 100")
        {
            CHECK(checkDuties(40, 50, 40, 50));
            CHECK(checkDuties(50, 50, 50, 50));
            CHECK(checkDuties(30, 60, 30, 60));
        }

        THEN("Achieved duty cycle is scaled proportionally if total is over 100")
        {
            CHECK(checkDuties(100, 100, 50, 50));
            CHECK(checkDuties(75, 50, 60, 40));
            CHECK(checkDuties(80, 30, 80.0 / 1.1, 30.0 / 1.1));
            CHECK(checkDuties(90, 20, 90.0 / 1.1, 20.0 / 1.1));
            CHECK(checkDuties(95, 10, 95.0 / 1.05, 10.0 / 1.05));
            CHECK(checkDuties(85, 25, 85.0 / 1.1, 25.0 / 1.1));
        }

        AND_WHEN("A PWM is set to invalid, its requested value is zero in the balancer")
        {
            constrainedPwm1.setting(50);
            constrainedPwm1.settingValid(false);

            constrainedPwm1.update();
            constrainedPwm2.update();
            balancer->update();

            CHECK(balancer->clients()[0].requested == 0);
        }
    }

    WHEN("A PWM actuator output goes active after being held back by the Mutex for a long time")
    {
        constrainedPwm1.setting(100);
        constrainedPwm2.setting(0);

        constrainedPwm1.update();
        constrainedPwm2.update();
        pwm1.update(now);
        pwm2.update(now);

        constrainedMock1->removeAllConstraints();
        constrainedMock1->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [mut]() {
                return mut;
            },
            5 * period,
            true));
        constrainedMock2->removeAllConstraints();
        constrainedMock2->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [mut]() {
                return mut;
            },
            5 * period,
            true));

        auto start = now;
        auto turnOffPwm1 = now + 50 * period;
        auto end = now + 100 * period;

        for (; now - start <= turnOffPwm1; now += 100) {
            constrainedPwm1.update();
            constrainedPwm2.update();
            pwm1.update(now);
            pwm2.update(now);
        }
        constrainedPwm1.setting(0);
        constrainedPwm2.setting(5);

        for (; now - start <= end; now += 100) {
            constrainedPwm1.update();
            constrainedPwm2.update();
            pwm1.update(now);
            pwm2.update(now);
            REQUIRE(pwm2.value() <= 5);
        }
    }

    WHEN("A PWM actuator that with a blocked target is set to 100%, followed by 0%, the desired state of the target goes from high to low")
    {
        constrainedMock1->removeAllConstraints();
        constrainedMock1->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [mut]() {
                return mut;
            },
            5 * period,
            true));
        constrainedMock2->removeAllConstraints();
        constrainedMock2->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
            [mut]() {
                return mut;
            },
            5 * period,
            true));

        constrainedPwm1.setting(100);
        constrainedPwm2.setting(100);
        constrainedPwm1.update();
        constrainedPwm2.update();
        pwm1.update(now++);
        pwm2.update(now++);

        // actuator 1 now holds the mutex
        CHECK(constrainedMock1->desiredState() == State::Active);
        CHECK(constrainedMock2->desiredState() == State::Active);
        CHECK(constrainedMock1->state() == State::Active);
        CHECK(constrainedMock2->state() == State::Inactive);

        constrainedPwm1.setting(0);
        constrainedPwm2.setting(100);

        constrainedPwm1.update();
        constrainedPwm2.update();
        pwm1.update(now++);
        pwm2.update(now++);

        // actuator 1 still holds the mutex, due to the wait time

        CHECK(constrainedMock1->desiredState() == State::Inactive);
        CHECK(constrainedMock2->desiredState() == State::Active);
        CHECK(constrainedMock1->state() == State::Inactive);
        CHECK(constrainedMock2->state() == State::Inactive);

        constrainedPwm1.setting(0);
        constrainedPwm2.setting(0);

        constrainedPwm1.update();
        constrainedPwm2.update();
        pwm1.update(now++);
        pwm2.update(now++);

        // actuator 1 still holds the mutex, but actuator 2 should have an updated desired state
        CHECK(constrainedMock1->desiredState() == State::Inactive);
        CHECK(constrainedMock2->desiredState() == State::Inactive);
        CHECK(constrainedMock1->state() == State::Inactive);
        CHECK(constrainedMock2->state() == State::Inactive);
    }
}

SCENARIO("A PWM actuator driving a target with delayed ON and OFF time", "[pwm]")
{
    auto now = ticks_millis_t(0);
    auto period = duration_millis_t(4000);

    auto mockIo = std::make_shared<MockIoArray>();
    ActuatorDigital mock1([mockIo]() { return mockIo; }, 1);

    auto constrainedMock1 = std::make_shared<ActuatorDigitalConstrained>(mock1);
    ActuatorPwm pwm1([constrainedMock1]() { return constrainedMock1; }, 4000);
    ActuatorAnalogConstrained constrainedPwm1(pwm1);

    auto mut = std::make_shared<MutexTarget>();
    auto balancer = std::make_shared<Balancer<2>>();

    constrainedMock1->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
        [mut]() {
            return mut;
        },
        0,
        true));
    constrainedMock1->addConstraint(std::make_unique<ADConstraints::Mutex<3>>(
        [mut]() {
            return mut;
        },
        0,
        true));

    WHEN("A PWM actuator is held back by constraints that delay toggling ON and off, the period doesn't stretched more 3x this delay")
    {
        // explanation:
        // Delayed OFF streteched the high period by 1000
        // The low period is stretched by 1000 as well to compensate
        // The delayed OFF accounts for another delay by 1000
        // Next cycle:
        // High period is still +1000
        // Low period is not stretched, because previous period was too low duty
        // Low period switch has delay of 1000

        constrainedPwm1.setting(50);

        constrainedPwm1.update();
        pwm1.update(now);

        constrainedMock1->removeAllConstraints();
        constrainedMock1->addConstraint(std::make_unique<ADConstraints::DelayedOn<4>>(
            1000));
        constrainedMock1->addConstraint(std::make_unique<ADConstraints::DelayedOff<5>>(
            1000));

        auto start = now;
        auto end = now + 100 * period;

        for (; now - start <= end; now += 100) {
            constrainedPwm1.update();
            pwm1.update(now);
            auto durations = constrainedMock1->activeDurations(now);
            CHECK(durations.previousPeriod <= period + 3100); // +100 due to update frequency
        }
    }
}

#if 0
WHEN("Actuator PWM value is alternated between zero and a low value, the average is correct")
{
    // test with minimum ON of 2 seconds, minimum off of 5 seconds and period 5 seconds
    ActuatorBool vAct ();
    ActuatorTimeLimited onOffAct (vAct, 20, 50);
    ActuatorPwm act (onOffAct, 100);

    utc_seconds_t timeHigh = 0;
    utc_seconds_t timeLow = 0;

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1#value, 2a#pin" << endl;

    for (int cycles = 0; cycles < 100; cycles++) {
        if (cycles % 2 == 0) {
            pwm.set(4.0); // under minimum ON time of 20, doesn't trigger skipping ahead
        } else {
            pwm.set(0.0);
        }
        for (int i = 0; i < 180; i++) { // 180 seconds, not full periods on purpose
            delay(1000);
            pwm.update();
            if (vAct.getState() == State::Active) {
                timeHigh++;
            } else {
                timeLow++;
            }
            csv << pwm.setting() << "," // setpoint
                << vAct.getState()      // actual cooler pin state
                << endl;
        }
    }

    csv.close();

    double avgDuty = double(timeHigh) * 100.0 / (timeHigh + timeLow);
    BOOST_CHECK_CLOSE(avgDuty, 2, 25); // value is between 1.5 and 2.5
}

BOOST_AUTO_TEST_CASE(on_big_positive_changes_shortened_cycle_has_correct_value)
{
    ActuatorBool vAct ();
    ActuatorTimeLimited limited (vAct, 0, 0);
    ActuatorPwm act (limited, 100); // period is 100 seconds

    pwm.set(short(30));
    ticks_millis_t start = ticks.millis();
    ticks_millis_t periodStart;
    while (ticks.millis() - start < 250000) { // 250 seconds
        State oldState = vAct.getState();
        pwm.update();
        State newState = vAct.getState();
        if (oldState == State::Inactive && newState == State::Active) { // low to high transition
            periodStart = ticks.millis();
        }
        delay(1000);
    }

    BOOST_CHECK(vAct.getState() == State::Inactive); // actuator is inactive, ~50 seconds into 3rd cycle
    pwm.set(50.0);
    while (vAct.getState() == State::Inactive) {
        delay(1000);
        pwm.update();
    }
    // cycle should be shortened to 60 seconds, 30 high + 30 low
    BOOST_CHECK_CLOSE(double(ticks.millis() - periodStart), 60000, 3);
    periodStart = ticks.millis();

    while (vAct.getState() == State::Active) {
        delay(1000);
        pwm.update();
    }
    // next high time should be normal
    BOOST_CHECK_CLOSE(double(ticks.millis() - periodStart), 50000, 1); // actuator turned on for 50 seconds

    while (vAct.getState() == State::Inactive) {
        delay(1000);
        pwm.update();
    }
    // next low time should be normal
    BOOST_CHECK_CLOSE(double(ticks.millis() - periodStart), 100000, 1); // actuator turned on for 50 seconds
}

BOOST_AUTO_TEST_CASE(on_big_negative_changes_go_low_immediately)
{
    ActuatorBool vAct ();
    ActuatorTimeLimited limited (vAct, 0, 0);
    ActuatorPwm act (limited, 100); // period is 100 seconds

    ticks_millis_t lastLowTimeBeforeChange = ticks.millis();
    pwm.set(60.0);
    for (uint32_t i = 0; i < 250; i++) { // 250 seconds
        delay(1000);
        pwm.update();
        if (vAct.getState() == State::Inactive) {
            lastLowTimeBeforeChange = ticks.millis();
        }
    }

    BOOST_CHECK(vAct.getState() == State::Active); // actuator is active
    pwm.set(30.0);
    pwm.update();
    BOOST_CHECK(vAct.getState() == State::Inactive); // actuator turns off immediately

    ticks_millis_t highTolowTime = ticks.millis();
    ticks_millis_t highPeriod = highTolowTime - lastLowTimeBeforeChange;

    while (vAct.getState() == State::Inactive) {
        delay(100);
        pwm.update();
    }
    ticks_millis_t lowToHighTime = ticks.millis();
    ticks_millis_t lowPeriod = lowToHighTime - highTolowTime;
    // check that this cycle has normal duration
    BOOST_CHECK_CLOSE(double(highPeriod + lowPeriod), 100000, 2);

    // but overshooting the high value is compensated in high period of next cycle
    while (vAct.getState() == State::Active) {
        delay(100);
        pwm.update();
    }
    ticks_millis_t reducedHighPeriod = ticks.millis() - lowToHighTime;

    BOOST_CHECK_CLOSE(double(highPeriod + reducedHighPeriod), 0.3 * 2 * 100000, 2);
}

BOOST_AUTO_TEST_CASE(ActuatorPWM_with_min_max_time_limited_OnOffActuator_as_driver)
{
    // test with minimum ON of 2 seconds, minimum off of 5 seconds and period 10 seconds

    srand(time(NULL));
    ActuatorBool vAct ();
    ActuatorTimeLimited onOffAct (vAct, 2, 5);
    ActuatorPwm act (onOffAct, 10);

    // Test that average duty cycle is correct, even with minimum times enforced in the actuator
    BOOST_CHECK_CLOSE(randomIntervalTest(pwm, vAct, 50.0, 500), 50.0, 1);
    BOOST_CHECK_CLOSE(randomIntervalTest(pwm, vAct, 3.0, 500), 3.0, 16.7);
    BOOST_CHECK_CLOSE(randomIntervalTest(pwm, vAct, 1.0, 500), 1.0, 50);
    BOOST_CHECK_CLOSE(randomIntervalTest(pwm, vAct, 99.0, 500), 99.0, 0.5);
}

BOOST_AUTO_TEST_CASE(ramping_PWM_up_faster_than_period_gives_correct_average)
{
    ActuatorBool vAct ();
    ActuatorPwm act (vAct, 20);
    utc_seconds_t timeHigh = 0;
    utc_seconds_t timeLow = 0;

    for (int ramps = 0; ramps < 100; ramps++) { // enough ramps to not be affected by time window
        for (value_t v = value_t(40.0); v <= value_t(60.0); v = v + value_t(0.25)) {
            pwm.set(v);
            for (int j = 0; j < 100; j++) { // 10 seconds total
                delay(100);
                pwm.update();
                if (vAct.getState() == State::Active) {
                    timeHigh++;
                } else {
                    timeLow++;
                }
            }
        }
    }

    double avgDuty = double(timeHigh) * 100.0 / (timeHigh + timeLow);
    BOOST_CHECK_CLOSE(avgDuty, 50.0, 2);
}

BOOST_AUTO_TEST_CASE(ramping_PWM_down_faster_than_period_gives_correct_average)
{
    ActuatorBool vAct ();
    ActuatorPwm act (vAct, 20);
    utc_seconds_t timeHigh = 0;
    utc_seconds_t timeLow = 0;

    for (int ramps = 0; ramps < 100; ramps++) { // enough ramps to not be affected by time window
        for (value_t v = value_t(60.0); v >= value_t(40.0); v = v - value_t(0.25)) {
            pwm.set(v);
            for (int j = 0; j < 100; j++) { // 10 seconds total
                delay(100);
                pwm.update();
                if (vAct.getState() == State::Active) {
                    timeHigh++;
                } else {
                    timeLow++;
                }
            }
        }
    }

    double avgDuty = double(timeHigh) * 100.0 / (timeHigh + timeLow);
    BOOST_CHECK_CLOSE(avgDuty, 50.0, 2);
}

BOOST_AUTO_TEST_CASE(two_mutex_PWM_actuators_can_overlap_with_equal_duty)
{
    ActuatorMutexGroup mutex ();
    ActuatorBool boolAct1 ();
    ActuatorMutexDriver mutexAct1 (boolAct1, mutex);
    ActuatorPwm act1 (mutexAct1, 10);

    ActuatorBool boolAct2 ();
    ActuatorMutexDriver mutexAct2 (boolAct2, mutex);
    ActuatorPwm act2 (mutexAct2, 10);

    mutex.setDeadTime(0);

    utc_seconds_t timeLow1 = 0;
    utc_seconds_t timeHigh1 = 0;
    utc_seconds_t timeHigh2 = 0;
    utc_seconds_t timeLow2 = 0;

    act1.set(20.0);
    act2.set(20.0);

    act1.update();
    act2.update();
    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1a#pin1, 1a#pin2" << endl;

    while (ticks.millis() - start <= 100000) { // run for 100 seconds
        act1.update();
        act2.update();
        mutex.update();
        if (boolAct1.getState() == State::Active) {
            timeHigh1++;
        } else {
            timeLow1++;
        }
        if (boolAct2.getState() == State::Active) {
            timeHigh2++;
        } else {
            timeLow2++;
        }
        BOOST_REQUIRE(!(boolAct1.getState() == State::Active && boolAct2.getState() == State::Active)); // not active at the same time
        csv << boolAct1.getState() << ","
            << boolAct2.getState()
            << endl;
        delay(100);
    }

    double avgDuty1 = double(timeHigh1) * 100.0 / (timeHigh1 + timeLow1);
    BOOST_CHECK_CLOSE(avgDuty1, 20.0, 2); // small error possible due to test window influence

    double avgDuty2 = double(timeHigh2) * 100.0 / (timeHigh2 + timeLow2);
    BOOST_CHECK_CLOSE(avgDuty2, 20.0, 2); // small error possible due to test window influenceutc_seconds_t timeLow1 = 0;
    utc_seconds_t timeHigh1 = 0;
    utc_seconds_t timeHigh2 = 0;
    utc_seconds_t timeLow2 = 0;

    act1.set(20.0);
    act2.set(20.0);

    act1.update();
    act2.update();
    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1a#pin1, 1a#pin2" << endl;

    while (ticks.millis() - start <= 100000) { // run for 100 seconds
        act1.update();
        act2.update();
        mutex.update();
        if (boolAct1.getState() == State::Active) {
            timeHigh1++;
        } else {
            timeLow1++;
        }
        if (boolAct2.getState() == State::Active) {
            timeHigh2++;
        } else {
            timeLow2++;
        }
        BOOST_REQUIRE(!(boolAct1.getState() == State::Active && boolAct2.getState() == State::Active)); // not active at the same time
        csv << boolAct1.getState() << ","
            << boolAct2.getState()
            << endl;
        delay(100);
    }

    double avgDuty1 = double(timeHigh1) * 100.0 / (timeHigh1 + timeLow1);
    BOOST_CHECK_CLOSE(avgDuty1, 20.0, 2); // small error possible due to test window influence

    double avgDuty2 = double(timeHigh2) * 100.0 / (timeHigh2 + timeLow2);
    BOOST_CHECK_CLOSE(avgDuty2, 20.0, 2); // small error possible due to test window influence
}

BOOST_AUTO_TEST_CASE(two_mutex_PWM_actuators_can_overlap_with_different_duty)
{
    ActuatorMutexGroup mutex ();
    ActuatorBool boolAct1 ();
    ActuatorMutexDriver mutexAct1 (boolAct1, mutex);
    ActuatorPwm act1 (mutexAct1, 10);

    ActuatorBool boolAct2 ();
    ActuatorMutexDriver mutexAct2 (boolAct2, mutex);
    ActuatorPwm act2 (mutexAct2, 10);

    mutex.setDeadTime(0);

    utc_seconds_t timeHigh1 = 0;
    utc_seconds_t timeLow1 = 0;
    utc_seconds_t timeHigh2 = 0;
    utc_seconds_t timeLow2 = 0;

    act1.set(60.0);
    act2.set(20.0);

    act1.update();
    act2.update();
    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1a#pin1, 1a#pin2" << endl;

    while (ticks.millis() - start <= 100000) { // run for 100 seconds
        act1.update();
        act2.update();
        mutex.update();
        if (boolAct1.getState() == State::Active) {
            timeHigh1++;
        } else {
            timeLow1++;
        }
        if (boolAct2.getState() == State::Active) {
            timeHigh2++;
        } else {
            timeLow2++;
        }
        BOOST_REQUIRE(!(boolAct1.getState() == State::Active && boolAct2.getState() == State::Active)); // not active at the same time
        csv << boolAct1.getState() << ","
            << boolAct2.getState()
            << endl;
        delay(100);
    }

    double avgDuty1 = double(timeHigh1) * 100.0 / (timeHigh1 + timeLow1);
    BOOST_CHECK_CLOSE(avgDuty1, 60.0, 2); // small error possible due to test window influence

    double avgDuty2 = double(timeHigh2) * 100.0 / (timeHigh2 + timeLow2);
    BOOST_CHECK_CLOSE(avgDuty2, 20.0, 2); // small error possible due to test window influence
}

BOOST_AUTO_TEST_CASE(mutex_actuator_which_cannot_go_active_cannot_block_other_actuator)
{
    ActuatorMutexGroup mutex ();
    ActuatorBool boolAct1 ();
    ActuatorMutexDriver mutexAct1 (boolAct1, mutex);
    ActuatorPwm act1 (mutexAct1, 10);

    ActuatorNop boolAct2 (); // actuator which can never go active
    ActuatorMutexDriver mutexAct2 (boolAct2, mutex);
    ActuatorPwm act2 (mutexAct2, 10);

    mutex.setDeadTime(0);

    utc_seconds_t timeHigh1 = 0;
    utc_seconds_t timeLow1 = 0;
    utc_seconds_t timeHigh2 = 0;
    utc_seconds_t timeLow2 = 0;

    act1.set(20.0);
    act2.set(40.0); // <-- act2 will have higher priority due to higher duty cycle

    act1.update();
    act2.update();
    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1a#pin1, 1a#pin2" << endl;

    while (ticks.millis() - start <= 100000) { // run for 100 seconds
        act1.update();
        act2.update();
        mutex.update();
        if (boolAct1.getState() == State::Active) {
            timeHigh1++;
        } else {
            timeLow1++;
        }
        if (boolAct2.getState() == State::Active) {
            timeHigh2++;
        } else {
            timeLow2++;
        }
        BOOST_REQUIRE(!(boolAct1.getState() == State::Active && boolAct2.getState() == State::Active)); // not active at the same time
        csv << boolAct1.getState() << ","
            << boolAct2.getState()
            << endl;
        delay(100);
    }

    double avgDuty1 = double(timeHigh1) * 100.0 / (timeHigh1 + timeLow1);
    BOOST_CHECK_CLOSE(avgDuty1, 20.0, 2); // small error possible due to test window influence

    double avgDuty2 = double(timeHigh2) * 100.0 / (timeHigh2 + timeLow2);
    BOOST_CHECK_CLOSE(avgDuty2, 0.0, 2); // Nop actuator cannot go active
}

BOOST_AUTO_TEST_CASE(actual_value_returned_by_ActuatorPwm_readValue_is_correct)
{
    ActuatorBool boolAct ();
    ActuatorPwm pwmAct (boolAct, 20);

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1#set value, 1#read value, 2a#pin" << endl;

    pwmAct.set(20.0);
    ticks_millis_t start = ticks.millis();
    while (ticks.millis() - start < 200000) { // run for 200 seconds to dial in cycle time
        pwmAct.update();
        delay(100);
    }

    start = ticks.millis();

    int count = 0;
    double sum = 0.0;
    while (ticks.millis() - start < 500000) { // run for 500 seconds
        pwmAct.update();
        csv << pwmAct.setting() << ","
            << pwmAct.value() << ","
            << boolAct.getState()
            << endl;
        count++;
        sum += double(pwmAct.value());
        delay(100);
    }
    double average = sum / count;
    BOOST_CHECK_CLOSE(average, 20.0, 1);
}

BOOST_AUTO_TEST_CASE(actual_value_returned_by_ActuatorPwm_readValue_is_correct_with_time_limited_actuator)
{
    ActuatorBool boolAct ();
    ActuatorTimeLimited timeLimitedAct (boolAct, 2, 5);
    ActuatorPwm pwmAct (timeLimitedAct, 20);

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1#set value, 1#read value, 2a#pin" << endl;

    pwmAct.set(5.0); // set to a value with duty cycle lower than time limit

    ticks_millis_t start = ticks.millis();
    while (ticks.millis() - start < 100000) { // run for 100 seconds to dial in cycle time
        pwmAct.update();
        delay(100);
    }

    start = ticks.millis();

    int count = 0;
    double sum = 0.0;
    while (ticks.millis() - start < 1000000) { // run for 1000 seconds
        pwmAct.update();
        csv << pwmAct.setting() << ","
            << pwmAct.value() << ","
            << boolAct.getState()
            << endl;
        count++;
        sum += double(pwmAct.value());
        delay(100);
    }
    double average = sum / count;
    BOOST_CHECK_CLOSE(average, 5.0, 10);
}

BOOST_AUTO_TEST_CASE(slowly_changing_pwm_value_reads_back_as_correct_value)
{
    ActuatorBool boolAct ();
    ActuatorPwm pwmAct (boolAct, 20);

    pwmAct.set(0.0);
    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1#set value, 1#read value, 2a#pin" << endl;

    double pwmValue = 50;

    while (ticks.millis() - start < 3000000) { // run for 3000 seconds
        // fluctuate with a period of 1000 seconds around 50 with amplitude 60, so some clipping will occur
        pwmValue = 50.0 - 60.0 * cos(3.14159265 * 2 * (ticks.millis() - start) / 1000000); // starts at zero
        pwmAct.set(pwmValue);

        if (pwmValue < 0.1) {
            pwmAct.set(pwmValue);
        }

        pwmAct.update();
        delay(100);
        csv << pwmAct.setting() << ","
            << pwmAct.value() << ","
            << boolAct.getState()
            << endl;
        // maximum from one cylce to the next is maximum derivative * pwm period = 60*2*pi/1000 * 20 = 7.5398
        BOOST_REQUIRE_LE(abs(double(pwmAct.setting() - pwmAct.value())), 7.5398); // read back value stays within 5% of set value
    }
}

BOOST_AUTO_TEST_CASE(fluctuating_pwm_value_gives_correct_average_with_time_limited_actuator)
{
    ActuatorBool boolAct ();
    ActuatorTimeLimited timeLimitedAct (boolAct, 2, 5);
    ActuatorPwm pwmAct (timeLimitedAct, 20);

    pwmAct.set(5.0); // set to a value with duty cycle lower than time limit
    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1#set value, 1#read value, 2a#pin" << endl;

    double pwmValue = 50;

    int count = 0;
    double sum = 0.0;
    double timeHigh = 0.0;
    double timeLow = 0.0;
    ticks_millis_t loopTime = ticks.millis();
    while (ticks.millis() - start < 1000000) { // run for 1000 seconds
        // fluctuate with a period of 200 seconds around 50 with amplitude 50
        pwmValue = 50.0 - 50.0 * cos(3.14159265 * 2 * (ticks.millis() - start) / 200000);
        pwmAct.set(pwmValue);

        pwmAct.update();
        delay(100);
        csv << pwmAct.setting() << ","
            << pwmAct.value() << ","
            << boolAct.getState()
            << endl;
        count++;
        sum += double(pwmAct.value());
        ticks_millis_t prevLoopTime = loopTime;
        loopTime = delay(200);
        if (boolAct.getState() == State::Active) {
            timeHigh += loopTime - prevLoopTime;
        } else {
            timeLow += loopTime - prevLoopTime;
        }
    }
    double readAverage = sum / count;
    double actualDuty = (timeHigh * 100.0) / (timeHigh + timeLow); // rounded result
    BOOST_CHECK_CLOSE(actualDuty, 50.0, 10);                       // setpoint is fluctuating very fast given time limits. Error of just 5 is good enough.
    BOOST_CHECK_CLOSE(readAverage, actualDuty, 5);
}

BOOST_AUTO_TEST_CASE(decreasing_pwm_value_after_long_high_time_and_mutex_wait)
{
    ActuatorMutexGroup mutex ();
    mutex.setDeadTime(100000);

    // actuator that prevents other actuator from going high
    ActuatorBool blocker ();
    ActuatorMutexDriver blockerMutex (blocker, mutex);

    ActuatorBool boolAct ();
    ActuatorMutexDriver mutexAct (boolAct, mutex);
    ActuatorPwm pwmAct (mutexAct, 20);

    ticks_millis_t start = ticks.millis();

    ofstream csv("./test_results/" + boost_test_name() + ".csv");
    csv << "1#set value, 1#read value, 2a#pin" << endl;

    // trigger dead time of mutex
    blockerMutex.setState(State::Active);
    mutex.update();
    BOOST_CHECK(blocker.getState() == State::Active);
    blockerMutex.setState(State::Inactive);
    BOOST_CHECK_EQUAL(mutex.getWaitTime(), 100000u);

    double pwmValue = 100;

    while (ticks.millis() - start < 1500000) { // run for 1500 seconds
        if (ticks.millis() - start < 100000) {
            BOOST_REQUIRE(boolAct.getState() == State::Inactive); // mutex group dead time keeps actuator low
        }

        pwmAct.set(pwmValue);
        mutex.update();
        pwmAct.update();

        if (ticks.millis() - start > 200000) { // start decreasing after 200 s
            pwmValue -= 0.01;                  // decrease slowly, with 0.1 degree per second
            // maximum difference between history based value and setpoint is 4
            BOOST_REQUIRE_LE(abs(double(pwmAct.setting() - pwmAct.value())), 4);
        }

        delay(100);
        csv << pwmAct.setting() << ","
            << pwmAct.value() << ","
            << boolAct.getState()
            << endl;
    }
}

BOOST_AUTO_TEST_SUITE_END()
#endif