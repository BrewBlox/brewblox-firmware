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

#include <algorithm>
#include <stdlib.h> /* srand, rand */

#include "ActuatorAnalogConstrained.h"
#include "ActuatorDigital.h"
#include "ActuatorDigitalConstrained.h"
#include "ActuatorPwm.h"
#include "Balancer.h"
#include "DS2408.h"
#include "DS2408Mock.h"
#include "DS2413.h"
#include "DS2413Mock.h"
#include "MockIoArray.h"
#include "MotorValve.h"
#include "OneWire.h"
#include "OneWireMockDriver.h"
#include "TestLogger.h"
#include <cmath> // for sin
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

bool printToggleTimes = false;
bool printSummary = false;
auto output = &std::cout;

using value_t = ActuatorAnalog::value_t;
using State = ActuatorDigitalBase::State;

double
randomIntervalTest(
    const int& numPeriods,
    ActuatorPwm& pwm,
    ActuatorDigitalBase& target,
    const value_t& duty,
    const duration_millis_t& delayMax,
    ticks_millis_t& now,
    std::function<void()> everySecond = []() {})
{
    pwm.setting(duty);
    ticks_millis_t lowToHighTime = now;
    ticks_millis_t highToLowTime = now;
    ticks_millis_t totalHighTime = 0;
    ticks_millis_t totalLowTime = 0;
    ticks_millis_t nextUpdate = now;
    ticks_millis_t lastOneSecondUpdate = now;
    if (printSummary) {
        *output << std::endl
                << std::endl
                << "*** Results running 100 periods and random 1-"
                << delayMax << " ms update intervals,"
                << " with duty cycle " << duty
                << " and period " << pwm.period()
                << " ***" << std::endl;
    }
    if (printToggleTimes) {
        *output << std::endl
                << std::endl
                << "\t"
                << " l->h time\t h->l time\t high time\t  low time\t  value\t    period"
                << std::endl;
    }

    for (int i = 0; i < numPeriods + 4; i++) {
        do {
            now += 1 + std::rand() % delayMax;
            if (now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
            if (now >= lastOneSecondUpdate) {
                lastOneSecondUpdate = now;
                everySecond();
            }
        } while (target.state() == State::Active);
        highToLowTime = now;
        ticks_millis_t highTime = highToLowTime - lowToHighTime;
        if (i >= 4) {
            totalHighTime += highTime;
        }
        do {
            now += 1 + std::rand() % delayMax;
            if (now >= nextUpdate) {
                nextUpdate = pwm.update(now);
            }
            if (now >= lastOneSecondUpdate) {
                lastOneSecondUpdate = now;
                everySecond();
            }
        } while (target.state() == State::Inactive);
        lowToHighTime = now;
        ticks_millis_t lowTime = lowToHighTime - highToLowTime;
        if (i >= 4) {
            totalLowTime += lowTime;
        }
        if (printToggleTimes) {

            *output << "\t"
                    << std::setw(10) << lowToHighTime
                    << "\t"
                    << std::setw(10) << highToLowTime
                    << "\t"
                    << std::setw(10) << highTime
                    << "\t"
                    << std::setw(10) << lowTime
                    << "\t"
                    << std::setprecision(5) << double(pwm.value())
                    << "\t"
                    << std::setw(10) << lowTime + highTime
                    << std::endl;
        }
    }
    double totalTime = totalHighTime + totalLowTime;
    double avgDuty = double(totalHighTime) / (totalHighTime + totalLowTime) * double(100);
    if (printSummary) {
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
        } while (mock.state() != State::Inactive);

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
        if (printToggleTimes) {
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
        CHECK(randomIntervalTest(100, pwm, mock, 50.0, 300, now) == Approx(50.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 20.0, 300, now) == Approx(20.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 80.0, 300, now) == Approx(80.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 2.0, 300, now) == Approx(2.0).margin(1));
        CHECK(randomIntervalTest(100, pwm, mock, 98.0, 300, now) == Approx(98.0).margin(1));
    }

    WHEN("Average_duty_cycle_is_correct_with_very long_period")
    {
        pwm.period(3600000);
        CHECK(randomIntervalTest(10, pwm, mock, 40.0, 300, now) == Approx(40.0).margin(0.1));
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

    WHEN(
        "the PWM actuator is set to 50, and infrequently but regularly updated, "
        "the achieved value is correctly calculated at all moments in the period")
    {
        pwm.setting(50);
        auto nextUpdateTime = now;
        for (; now < 5 * pwm.period(); now += 211) {
            if (now >= nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
        }
        for (; now < 50 * pwm.period(); now += 211) {
            if (now >= nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
            // INFO(now);
            CHECK(pwm.value() == Approx(50).margin(0.5));
        }
    }

    WHEN("the period is changed, the duty cycle is updated")
    {
        pwm.setting(50);
        pwm.period(30000);
        auto nextUpdateTime = now;
        for (; now < 2 * pwm.period(); now += 1) {
            if (now >= nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
        }
        auto durations = constrained->activeDurations(now);
        CHECK(durations.previousActive == 2000);
    }

    WHEN(
        "the PWM actuator is set to 50, and infrequently and irregularly updated, "
        "the achieved value is correctly calculated at all moments in the period")
    {
        pwm.setting(50);
        auto nextUpdateTime = now;
        for (; now < 5 * pwm.period(); now += std::rand() % 250) {
            if (now >= nextUpdateTime) {
                nextUpdateTime = pwm.update(now);
            }
        }
        for (; now < 50 * pwm.period(); now += std::rand() % 250) {
            if (now >= nextUpdateTime) {
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

    WHEN("the PWM actuator is set to 40% after being 99% for a long time, the low time is not stretched")
    {
        // stretching is only allowed when the previous cycle had normal length
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
        CHECK(lowTime == pwm.period() * 0.6);
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

    WHEN(
        "the PWM actuator is set to 60% after being 1% for a long time, "
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

    WHEN(
        "the PWM actuator is set to 40% after being 99% for a long time, "
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

    WHEN("the PWM runs at 0% for an hour, then 100% for an hour, then 0% for an hour and then goes to 99%")
    {
        pwm.setting(0);
        while (now < 1000 * 3600) {
            now = pwm.update(now);
        }

        pwm.setting(100);
        while (now < 2000 * 3600) {
            now = pwm.update(now);
        }

        pwm.setting(0);
        while (now < 3000 * 3600) {
            now = pwm.update(now);
        }

        THEN("The reported achieved duty is 99% within 1 period")
        {
            pwm.setting(99);
            auto start = now;
            while (now < start + 10000) {
                now = pwm.update(now);
                if (pwm.value() == Approx(99).margin(1)) {
                    break;
                }
            }
            CHECK(now - start < pwm.period());
        }
    }

    WHEN("the PWM runs at 100% for an hour, then 0% for an hour, then 100% for an hour and then goes to 1%")
    {
        pwm.setting(100);
        while (now < 1000 * 3600) {
            now = pwm.update(now);
        }

        pwm.setting(0);
        while (now < 2000 * 3600) {
            now = pwm.update(now);
        }

        pwm.setting(100);
        while (now < 3000 * 3600) {
            now = pwm.update(now);
        }

        THEN("The reported achieved duty is 1% within 1 period")
        {
            pwm.setting(1);
            auto start = now;
            while (now < start + 10000) {
                now = pwm.update(now);
                if (pwm.value() == Approx(1).margin(1)) {
                    break;
                }
            }
            CHECK(now - start < pwm.period());
        }
    }

    WHEN("the PWM actuator is set from 0 to 50, it goes high immediately")
    {
        pwm.setting(0);
        while (now < 5.33 * pwm.period()) {
            now = pwm.update(now);
        }
        pwm.setting(50);
        pwm.update(now + 1);
        CHECK(mock.state() == State::Active);
    }

    WHEN("the PWM actuator is set from 100 to 50, it goes low immediately")
    {
        pwm.setting(100);
        while (now < 5.33 * pwm.period()) {
            now = pwm.update(now);
        }
        pwm.setting(50);
        pwm.update(now + 1);
        CHECK(mock.state() == State::Inactive);
    }
    WHEN("the PWM actuator is set to 40% at startup")
    {
        pwm.setting(40);
        auto previous = pwm.value();
        while (now < 10 * pwm.period()) {
            now = pwm.update(now);
            auto current = pwm.value();
            if (current < 35) {
                REQUIRE(current >= previous); // strictly rising
            } else {
                REQUIRE(current < 45);
            }
            previous = current;
        }
        REQUIRE(previous == Approx(40.0).margin(0.1));
    }

    WHEN("the PWM actuator is set to 40% after being 10%")
    {
        pwm.setting(10.0);
        auto changeSettingAt = 5 * pwm.period();
        while (now < changeSettingAt) {
            now = pwm.update(now);
        }

        pwm.setting(40.0);
        auto previous = pwm.value();
        while (now < 10 * pwm.period()) {
            now = pwm.update(now);
            auto current = pwm.value();
            if (current != Approx(40.0).margin(0.1)) {
                REQUIRE(current >= previous); // strictly rising
            }
            previous = current;
        }
        REQUIRE(previous == Approx(40.0).margin(0.1));
    }

    WHEN("the PWM actuator is set to 100% after being 10%")
    {
        pwm.setting(10.0);
        auto changeSettingAt = 5 * pwm.period();
        while (now < changeSettingAt || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(100.0);
        auto previous = pwm.value();
        while (now < 10 * pwm.period()) {
            now = pwm.update(now);
            auto current = pwm.value();
            if (current != 100) {
                CHECK(current >= previous); // strictly rising
            }
            previous = current;
        }
        REQUIRE(previous == 100);
    }

    WHEN("the PWM actuator is set to 90% after being 10%")
    {
        pwm.setting(10.0);
        auto changeSettingAt = 5 * pwm.period();
        while (now < changeSettingAt || mock.state() == State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(90.0);
        auto previous = pwm.value();
        while (now < 20 * pwm.period()) {
            now = pwm.update(now);
            auto current = pwm.value();
            if (current < 85) {
                CHECK(current >= previous); // strictly rising until close
            }
            previous = current;
        }
        REQUIRE(previous == Approx(90).margin(0.1));
    }

    WHEN("the PWM actuator is set to 0% after being 100%")
    {
        // run at 50% for a while first to have some cycle history, end on a high output
        pwm.setting(50.0);
        auto changeSettingAt = 5 * pwm.period();
        while (now < changeSettingAt || mock.state() != State::Active) {
            now = pwm.update(now);
        }

        pwm.setting(100);
        changeSettingAt = 10 * pwm.period();
        while (now < changeSettingAt) {
            now = pwm.update(now);
        }

        pwm.setting(0.0);
        auto changedSettingAt = now;
        THEN("The pin goes low immediately")
        {
            while (mock.state() != State::Inactive) {
                now = pwm.update(now);
            }
            CHECK(now - changedSettingAt <= 1);

            AND_THEN("It starts at a value of 50% (due to lookahead)")
            {
                now = pwm.update(now);
                CHECK(pwm.value() == Approx(50).margin(0.01));
            }

            AND_THEN("It reports a value of 0% in under 2 periods and only decreases")
            {
                auto previous = pwm.value();
                while (pwm.value() > 0 && now - changeSettingAt < 100000) {
                    now = pwm.update(now);
                    auto value = pwm.value();
                    CHECK(value <= previous);
                    previous = value;
                }
                CHECK(now - changedSettingAt < 2 * pwm.period());
            }
        }
    }

    WHEN("the PWM actuator is set to 50% after being 0% and 100% for a while")
    {
        // run at 50% for a while first to have some cycle history, end on a high output
        pwm.setting(50.0);
        auto changeSettingAt = 5 * pwm.period();
        while (now < changeSettingAt || mock.state() != State::Active) {
            now = pwm.update(now);
        }

        pwm.setting(0);
        changeSettingAt = 10 * pwm.period();
        while (now < changeSettingAt) {
            now = pwm.update(now);
        }

        pwm.setting(100);
        changeSettingAt = 15 * pwm.period();
        while (now < changeSettingAt) {
            now = pwm.update(now);
        }

        auto changedSettingAt = now;
        pwm.setting(50.0);
        THEN("The pin goes low immediately")
        {
            while (mock.state() != State::Inactive) {
                now = pwm.update(now);
            }
            CHECK(now - changedSettingAt <= 1);

            AND_THEN("The first low times are longer, to compensate for the long high time")
            {
                std::vector<ticks_millis_t> stateDurations;
                auto previousState = mock.state();
                while (stateDurations.size() < 10) {
                    auto stateStart = now;
                    while (mock.state() == previousState) {
                        now = pwm.update(now);
                    }
                    stateDurations.push_back(now - stateStart);
                    previousState = mock.state();
                }
                // first cycles have longer low times
                std::vector<ticks_millis_t> expected = {3132, 2000, 2868, 2000, 1500, 2000, 2000, 2000, 2000, 2000};
                CHECK(stateDurations == expected);
            }
        }
    }

    WHEN("the PWM actuator is set to 100% after being 0%, with a period of 4s")
    {
        // run at 50% for a while first to have some cycle history, end on a low output
        pwm.setting(50.0);
        auto changeSettingAt = 5 * pwm.period();
        while (now < changeSettingAt | mock.state() != State::Inactive) {
            now = pwm.update(now);
        }

        pwm.setting(0);
        changeSettingAt = 10 * pwm.period();
        while (now < changeSettingAt) {
            now = pwm.update(now);
        }
        auto changedSettingAt = now;
        pwm.setting(100.0);
        THEN("The pin goes high immediately")
        {
            while (mock.state() != State::Inactive) {
                now = pwm.update(now);
            }
            CHECK(now - changedSettingAt <= 1);

            AND_THEN("It starts at a value of 50% (due to lookahead)")
            {
                now = pwm.update(now);
                CHECK(pwm.value() == Approx(50).margin(0.01));
            }

            AND_THEN("It reports a value of 100 in under 2 periods and only increases")
            {
                auto previous = pwm.value();
                while (pwm.value() < 100 && now - changeSettingAt < 100000) {
                    now = pwm.update(now);
                    auto value = pwm.value();
                    CHECK(value >= previous);
                    previous = value;
                }
                CHECK(now - changedSettingAt < 2 * pwm.period());
            }
        }
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
        CHECK(randomIntervalTest(10, pwm, mock, 92.0, 5000, now) == Approx(92.0).margin(0.5));
    }

    WHEN("The actuator has been set to 30% duty and switches to 20% with minimum ON time at 40% duty")
    {
        pwm.period(10000);                                                               // 10s
        constrained->addConstraint(std::make_unique<ADConstraints::MinOnTime<2>>(4000)); // 4 s
        pwm.setting(30);                                                                 // will result in 4s on, 13.3s period

        auto nextUpdate = pwm.update(now);

        while (now < 100000) {
            now = pwm.update(now);
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
                        REQUIRE(pwm.value() == Approx(10).margin(2));
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

    WHEN("A PWM actuator is held back by constraints that delay toggling ON and off, the period doesn't stretch more than 3x this delay")
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

        auto end = now + 100 * period;

        while (now <= end) {
            now = pwm1.update(now);
            auto durations = constrainedMock1->activeDurations(now);
            REQUIRE(durations.previousPeriod <= period + 3000);
        }
    }
}

SCENARIO("ActuatorPWM driving mock DS2413 actuator", "[pwm]")
{
    auto now = ticks_millis_t(0);
    OneWireMockDriver mockOw;
    OneWire ow(mockOw);
    auto addr = OneWireAddress(0x0644'4444'4444'443A);
    auto ds2413mock = std::make_shared<DS2413Mock>(addr);
    mockOw.attach(ds2413mock); // DS2413
    auto ds = std::make_shared<DS2413>(ow, addr);
    ActuatorDigital act([ds]() { return ds; }, 1);
    ds->update(); // connected update happens here
    auto constrained = std::make_shared<ActuatorDigitalConstrained>(act);
    ActuatorPwm pwm([constrained]() { return constrained; }, 4000);

    WHEN("update is called without delays, the average duty cycle is correct")
    {
        CHECK(randomIntervalTest(100, pwm, act, 49.0, 1, now) == Approx(49.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 50.0, 1, now) == Approx(50.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 51.0, 1, now) == Approx(51.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 2.0, 1, now) == Approx(2.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 98.0, 1, now) == Approx(98.0).margin(0.2));
    }

    WHEN("Communication errors occur on the bus, the PWM values are still correct")
    {
        auto nextWriteFlip = []() {
            static uint32_t v = 113;
            v += 5133;
            return v;
        };
        std::vector<uint32_t> writeBitFlips(100);
        std::generate(writeBitFlips.begin(), writeBitFlips.end(), nextWriteFlip);

        auto nextReadFlip = []() {
            static uint32_t v = 1742;
            v += 6457;
            return v;
        };
        std::vector<uint32_t> readBitFlips(100);
        std::generate(readBitFlips.begin(), readBitFlips.end(), nextReadFlip);

        auto everySecond = [&ds]() {
            ds->update(); // reconnects happen in this object, so need to include the 1s update
        };

        TestLogger::clear();
        ds2413mock->flipWrittenBits(writeBitFlips);
        ds2413mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 49.0, 1, now, everySecond) == Approx(49.0).margin(0.2));
        ds2413mock->flipWrittenBits(writeBitFlips);
        ds2413mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 50.0, 1, now, everySecond) == Approx(50.0).margin(0.2));
        ds2413mock->flipWrittenBits(writeBitFlips);
        ds2413mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 51.0, 1, now, everySecond) == Approx(51.0).margin(0.2));
        ds2413mock->flipWrittenBits(writeBitFlips);
        ds2413mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 2.0, 1, now, everySecond) == Approx(2.0).margin(0.2));
        ds2413mock->flipWrittenBits(writeBitFlips);
        ds2413mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 98.0, 1, now, everySecond) == Approx(98.0).margin(0.2));

        AND_THEN("The log contains disconnect and reconnect logs")
        {
            CHECK(TestLogger::count("LOG(INFO): DS2413 connected: " + addr.toString()) > 10);
            CHECK(TestLogger::count("LOG(WARN): DS2413 disconnected: " + addr.toString()) > 10);
        }
        TestLogger::clear();
    }
}

SCENARIO("ActuatorPWM driving mock DS2408 motor valve", "[pwm]")
{
    auto now = ticks_millis_t(0);
    OneWireMockDriver mockOw;
    OneWire ow(mockOw);
    auto addr = OneWireAddress(0xDA55'5555'5555'5529);
    auto ds2408mock = std::make_shared<DS2408Mock>(addr);
    mockOw.attach(ds2408mock);
    auto ds = std::make_shared<DS2408>(ow, addr);
    MotorValve act([ds]() { return ds; }, 1);

    auto constrained = std::make_shared<ActuatorDigitalConstrained>(act);
    ActuatorPwm pwm([constrained]() { return constrained; }, 4000);

    WHEN("update is called without delays, the average duty cycle is correct")
    {
        CHECK(randomIntervalTest(100, pwm, act, 49.0, 1, now) == Approx(49.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 50.0, 1, now) == Approx(50.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 51.0, 1, now) == Approx(51.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 2.0, 1, now) == Approx(2.0).margin(0.2));
        CHECK(randomIntervalTest(100, pwm, act, 98.0, 1, now) == Approx(98.0).margin(0.2));
    }

    WHEN("Communication errors occur on the bus, the PWM values are still correct")
    {
        auto nextWriteFlip = []() {
            static uint32_t v = 113;
            v += 5133;
            return v;
        };
        std::vector<uint32_t> writeBitFlips(100);
        std::generate(writeBitFlips.begin(), writeBitFlips.end(), nextWriteFlip);

        auto nextReadFlip = []() {
            static uint32_t v = 1742;
            v += 6457;
            return v;
        };
        std::vector<uint32_t> readBitFlips(100);
        std::generate(readBitFlips.begin(), readBitFlips.end(), nextReadFlip);

        auto everySecond = [&ds]() {
            ds->update(); // reconnects happen in this object, so need to include the 1s update
        };

        TestLogger::clear();
        ds2408mock->flipWrittenBits(writeBitFlips);
        ds2408mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 49.0, 1, now, everySecond) == Approx(49.0).margin(0.2));
        ds2408mock->flipWrittenBits(writeBitFlips);
        ds2408mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 50.0, 1, now, everySecond) == Approx(50.0).margin(0.2));
        ds2408mock->flipWrittenBits(writeBitFlips);
        ds2408mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 51.0, 1, now, everySecond) == Approx(51.0).margin(0.2));
        ds2408mock->flipWrittenBits(writeBitFlips);
        ds2408mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 2.0, 1, now, everySecond) == Approx(2.0).margin(0.2));
        ds2408mock->flipWrittenBits(writeBitFlips);
        ds2408mock->flipReadBits(readBitFlips);
        CHECK(randomIntervalTest(100, pwm, act, 98.0, 1, now, everySecond) == Approx(98.0).margin(0.2));

        AND_THEN("The log contains disconnect and reconnect logs")
        {
            CHECK(TestLogger::count("LOG(INFO): DS2408 connected: " + addr.toString()) > 10);
            CHECK(TestLogger::count("LOG(WARN): DS2408 disconnected: " + addr.toString()) > 10);
        }
        TestLogger::clear();
    }
}
