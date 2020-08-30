/*
 * Copyright 2018 BrewPi B.V./Elco Jacobs.
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

#include "ActuatorAnalogConstrained.h"
#include "ActuatorAnalogMock.h"
#include "ActuatorDigital.h"
#include "ActuatorDigitalConstrained.h"
#include "ActuatorOffset.h"
#include "ActuatorPwm.h"
#include "MockIoArray.h"
#include "Pid.h"
#include "SetpointSensorPair.h"
#include "TempSensorMock.h"
#include <iostream>
#include <math.h>

SCENARIO("PID Test with mock actuator", "[pid]")
{
    auto sensor = std::make_shared<TempSensorMock>(20.0);

    auto input = std::make_shared<SetpointSensorPair>([&sensor]() { return sensor; });
    input->setting(20);
    input->settingValid(true);

    auto actuator = std::make_shared<ActuatorAnalogMock>();

    Pid pid(
        [&input]() { return input; },
        [&actuator]() { return actuator; });

    pid.enabled(true);

    WHEN("Only proportional gain is active, the output value is correct")
    {
        pid.kp(10);
        pid.ti(0);
        pid.td(0);

        for (int32_t i = 0; i < 1000; ++i) {
            input->setting(21);
            sensor->setting(20);
        }

        input->update();
        pid.update();

        THEN("With Td zero, the derivative filter nr is 1")
        {
            CHECK(pid.derivativeFilterNr() == 1);
        }

        CHECK(actuator->setting() == Approx(10).margin(0.01));
    }

    WHEN("Only Kp is zero, the output is zero.")
    {
        pid.kp(0);
        pid.ti(2000);
        pid.td(100);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();

        for (int32_t i = 0; i < 1000; ++i) {
            input->update();
            pid.update();
        }

        CHECK(pid.p() == 0);
        CHECK(pid.i() == 0);
        CHECK(pid.d() == 0);
        CHECK(actuator->setting() == 0);
    }

    WHEN("Proportional gain and integrator are enabled, the output value is correct")
    {
        pid.kp(10);
        pid.ti(2000);
        pid.td(0);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();

        for (int32_t i = 0; i < 1000; ++i) {
            input->update();
            pid.update();
        }

        CHECK(pid.p() == Approx(10).epsilon(0.001));
        CHECK(pid.i() == Approx(5).epsilon(0.001));
        CHECK(pid.d() == 0);
        CHECK(actuator->setting() == Approx(10.0 * (1.0 + 1000 * 1.0 / 2000)).epsilon(0.001));

        for (int32_t i = 0; i < 1000; ++i) {
            pid.update();
        }

        CHECK(pid.p() == Approx(10).epsilon(0.001));
        CHECK(pid.i() == Approx(10).epsilon(0.001));
        CHECK(pid.integral() == Approx(2000).epsilon(0.001));
        CHECK(pid.d() == 0);
        CHECK(actuator->setting() == Approx(10.0 * (1.0 + 2000 * 1.0 / 2000)).epsilon(0.001));
    }

    WHEN("Proportional, Integral and Derivative are enabled, the output value is correct with positive Kp")
    {
        pid.kp(10);
        pid.ti(2000);
        CHECK(input->filterLength() == 1);
        pid.td(200);

        THEN("The PID will ensure the filter of the input is long enough for td")
        {
            CHECK(input->filterLength() == 4);
        }

        input->setting(30);
        sensor->setting(20);
        input->resetFilter();

        fp12_t mockVal;
        double integralValue = 0;
        for (int32_t i = 0; i <= 900; ++i) {
            mockVal = fp12_t(20.0 + 9.0 * i / 900);
            sensor->setting(mockVal);
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        CHECK(mockVal == 29);
        CHECK(pid.error() == Approx(1.1).epsilon(0.05)); // the filter introduces some delay, which is why this is not exactly 1.0
        CHECK(pid.p() == Approx(11).epsilon(0.05));
        CHECK(pid.i() == Approx(integralValue * pid.kp() / pid.ti()).epsilon(0.001));
        CHECK(pid.integral() == Approx(integralValue).epsilon(0.001));
        CHECK(actuator->setting() == pid.p() + pid.i() + pid.d());
    }

    WHEN("Proportional, Integral and Derivative are enabled, the output value is correct with negative Kp")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(200);

        input->setting(20);
        sensor->setting(30);
        input->resetFilter();

        fp12_t mockVal;
        double integralValue = 0;
        for (int32_t i = 0; i <= 900; ++i) {
            mockVal = fp12_t(30.0 - (9.0 * i) / 900);
            sensor->setting(mockVal);
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        CHECK(mockVal == 21);
        CHECK(pid.error() == Approx(-1.1).epsilon(0.05)); // the filter introduces some delay, which is why this is not 1.0
        CHECK(pid.p() == Approx(11).epsilon(0.05));
        CHECK(pid.i() == Approx(integralValue * pid.kp() / pid.ti()).epsilon(0.001));
        CHECK(pid.integral() == Approx(integralValue).epsilon(0.001));

        CHECK(actuator->setting() == pid.p() + pid.i() + pid.d());
    }

    WHEN("A sensor quantized like a OneWire sensor is used")
    {
        pid.kp(50);
        pid.ti(2000);

        std::vector<uint16_t> tdValues{10, 30, 60, 90, 120, 180, 240, 300, 450, 600, 900, 1200, 1800, 3600};

        THEN("The effect of bit flips is max 10% of Kp for all possible values of Td")
        {
            for (auto td : tdValues) {
                pid.td(td);

                input->setting(21);
                sensor->setting(20);
                input->resetFilter();

                double minD = 0.0;
                // will be negative
                // use a very slow rising temperature so only bit flips and not the actual derivative have an effect
                for (uint32_t i = 0; i <= 60 * 60 * 1000ul; i += 1000) {
                    fp12_t mockVal = fp12_t(20.0 + i / (12 * 60 * 60 * 1000.0));
                    mockVal = mockVal - mockVal % fp12_t{0.0625};
                    sensor->setting(mockVal);
                    input->update();
                    pid.update();
                    if (pid.d() < minD) {
                        minD = double(pid.d());
                    }
                }
                CAPTURE(td);
                auto selectedFilter = pid.derivativeFilterNr();
                CAPTURE(selectedFilter);
                CHECK(minD > -12.5); // max 25% of Kp
            }
        }
    }

    WHEN("The actuator setting is clipped, the integrator is limited by anti-windup (positive kp)")
    {
        pid.kp(10);
        pid.ti(2000);
        pid.td(60);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();
        actuator->minSetting(0);
        actuator->maxSetting(20);

        double integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        double integratorValueWithoutAntiWindup = integralValue * (10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(10).epsilon(0.01));
        CHECK(pid.i() == Approx(10).epsilon(0.01)); // anti windup limits this to 10
        CHECK(pid.integral() == Approx(2000).epsilon(0.001));
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(20).epsilon(0.01));

        input->setting(19);
        integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        integratorValueWithoutAntiWindup = integralValue * (10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(-50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(-10).epsilon(0.01));
        CHECK(pid.i() == Approx(0).margin(0.01)); // anti windup limits this to 0
        CHECK(pid.integral() == Approx(0).margin(0.01));
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(0).margin(0.01));
    }

    WHEN("The actuator setting is clipped, the integrator is limited by anti-windup (negative kp)")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(60);

        input->setting(19);
        sensor->setting(20);
        input->resetFilter();
        actuator->minSetting(0);
        actuator->maxSetting(20);

        double integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        double integratorValueWithoutAntiWindup = integralValue * (-10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(10).epsilon(0.01));
        CHECK(pid.i() == Approx(10).epsilon(0.01)); // anti windup limits this to 10
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(20).epsilon(0.01));

        input->setting(21);
        integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        integratorValueWithoutAntiWindup = integralValue * (-10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(-50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(-10).epsilon(0.01));
        CHECK(pid.i() == Approx(0).margin(0.01)); // anti windup limits this to 0
        CHECK(pid.integral() == Approx(0).margin(0.01));
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(0).margin(0.01));
    }

    WHEN("The actuator value is not reaching setting, the integrator is limited by anti-windup (positive kp)")
    {
        pid.kp(10);
        pid.ti(2000);
        pid.td(60);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();
        actuator->minValue(5);
        actuator->maxValue(20);

        double integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        double integratorValueWithoutAntiWindup = integralValue * (10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(10).epsilon(0.01));
        CHECK(pid.i() == Approx(13.33).epsilon(0.01)); // anti windup limits this to 13.33 (clipped output + error / 3)
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(23.33).epsilon(0.01));

        input->setting(19);
        integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        integratorValueWithoutAntiWindup = integralValue * (10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(-50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(-10).epsilon(0.01));
        CHECK(pid.i() == Approx(0).margin(0.01)); // anti windup limits this to 0
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(-10).margin(0.01));
    }

    WHEN("The actuator value is not reaching setting, the integrator is limited by anti-windup (negative kp)")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(60);

        input->setting(19);
        sensor->setting(20);
        input->resetFilter();
        actuator->minValue(5);
        actuator->maxValue(20);

        double integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        double integratorValueWithoutAntiWindup = integralValue * (-10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(10).epsilon(0.01));
        CHECK(pid.i() == Approx(13.33).epsilon(0.01)); // anti windup limits this to 13.33 (clipped output + proportional part / 3)
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(23.33).epsilon(0.01));

        input->setting(21);
        integralValue = 0;
        for (int32_t i = 0; i <= 10000; ++i) {
            input->update();
            pid.update();
            integralValue += double(pid.p() + pid.d()) / double(pid.kp());
        }

        integratorValueWithoutAntiWindup = integralValue * (-10.0 / 2000);
        CHECK(integratorValueWithoutAntiWindup == Approx(-50.0).epsilon(0.01));
        CHECK(pid.p() == Approx(-10).epsilon(0.01));
        CHECK(pid.i() == Approx(0).margin(0.01)); // anti windup limits this to 0
        CHECK(pid.d() == Approx(0).margin(0.01));

        CHECK(actuator->setting() == Approx(-10).margin(0.01));
    }

    WHEN("The sensor of the PID input becomes invalid")
    {
        pid.kp(10);
        pid.ti(2000);
        pid.td(60);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();

        int32_t i = 0;
        for (; i <= 10000; ++i) {
            if (i == 2000) {
                sensor->connected(false);
            }
            input->update();
            pid.update();
            if (!pid.active()) {
                break;
            }
        }
        THEN("The input becomes invalid after 5 seconds")
        {
            CHECK(i == 2010);
        }
        AND_THEN("PID is inactive")
        {
            CHECK(actuator->settingValid() == false);
        }

        AND_WHEN("The input becomes valid again, the pid and actuator become active again")
        {
            sensor->connected(true);
            input->update();
            pid.update();

            CHECK(pid.active() == true);
            CHECK(actuator->settingValid() == true);
        }
    }
}

SCENARIO("PID Test with offset actuator", "[pid]")
{
    auto targetSensor = std::make_shared<TempSensorMock>(65.0);

    auto referenceSensor = std::make_shared<TempSensorMock>(65.0);

    auto target = std::make_shared<SetpointSensorPair>(
        [targetSensor]() { return targetSensor; });
    target->setting(65);
    target->settingValid(true);

    auto reference = std::make_shared<SetpointSensorPair>(
        [referenceSensor]() { return referenceSensor; });
    reference->setting(67);
    reference->settingValid(true);

    auto actuator = std::make_shared<ActuatorOffset>(
        [target]() { return target; },
        [reference]() { return reference; });

    Pid pid(
        [&reference]() { return reference; },
        [&actuator]() { return actuator; });

    pid.enabled(true);

    pid.kp(2);
    pid.ti(0);
    pid.td(0);

    for (int32_t i = 0; i < 100; ++i) {
        reference->update();
        pid.update(); // update 100 times to settle input filter
    }

    WHEN("The PID has updated, the target setpoint is set correctly")
    {
        CHECK(actuator->setting() == Approx(4.0).margin(0.01));
        CHECK(target->setting() == Approx(71.0).margin(0.01));
        CHECK(actuator->settingValid() == true);
    }

    WHEN("The PID input sensor becomes invalid")
    {
        referenceSensor->connected(false);
        reference->update();
        pid.update();

        THEN("The input value becomes invalid and the target setpoint is set to invalid after 10 failed sensor reads")
        {
            for (uint8_t i = 0; i < 10; ++i) {
                CHECK(pid.active() == true);
                CHECK(target->settingValid() == true);
                reference->update();
                pid.update();
            }

            CHECK(reference->valueValid() == false);
            CHECK(pid.active() == false);
            CHECK(target->settingValid() == false);
        }

        AND_WHEN("The sensor comes back alive, the pid and setpoint are active/valid again")
        {
            referenceSensor->connected(true);
            reference->update();
            pid.update();

            CHECK(reference->valueValid() == true);
            CHECK(pid.active() == true);
            CHECK(target->settingValid() == true);
        }
    }

    WHEN("The PID is disabled")
    {
        pid.enabled(false);
        reference->update();
        pid.update();

        THEN("The target setpoint is set to invalid")
        {
            CHECK(target->settingValid() == false);
        }

        WHEN("Something else sets the target setpoint later")
        {
            CHECK(target->settingValid() == false);
            target->setting(20.0);
            target->settingValid(true);
            CHECK(target->settingValid() == true);
            reference->update();
            pid.update();

            THEN("The already disabled PID doesn't affect it")
            {
                CHECK(target->settingValid() == true);
                CHECK(target->setting() == 20.0);
            }
        }
    }
}

SCENARIO("PID Test with PWM actuator", "[pid]")
{
    auto now = ticks_millis_t(0);
    auto sensor = std::make_shared<TempSensorMock>(20.0);

    auto input = std::make_shared<SetpointSensorPair>(
        [&sensor]() { return sensor; });
    input->settingValid(true);
    input->setting(20);
    input->filterChoice(1);
    input->filterThreshold(5);

    auto mockIo = std::make_shared<MockIoArray>();
    ActuatorDigital mock([mockIo]() { return mockIo; }, 1);
    auto constrainedDigital = std::make_shared<ActuatorDigitalConstrained>(mock);

    ActuatorPwm pwm(
        [constrainedDigital]() { return constrainedDigital; },
        4000);

    auto actuator = std::make_shared<ActuatorAnalogConstrained>(pwm);

    Pid pid(
        [&input]() { return input; },
        [&actuator]() { return actuator; });

    pid.enabled(true);

    auto nextPwmUpdate = now;
    auto nextPidUpdate = now;

    auto run1000seconds = [&]() {
        auto start = now;
        while (++now < start + 1000'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                input->update();
                pid.update();
                actuator->update();
                nextPidUpdate = now + 1000;
            }
        }
    };

    WHEN("Only proportional gain is active, the output value is correct")
    {
        pid.kp(10);
        pid.ti(0);
        pid.td(0);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();

        run1000seconds();

        CHECK(actuator->setting() == Approx(10).margin(0.01));
    }

    WHEN("Proportional gain and integrator are enabled, the output value is correct")
    {
        pid.kp(10);
        pid.ti(2000);
        pid.td(0);

        input->setting(21);
        sensor->setting(20);
        input->resetFilter();

        run1000seconds();

        CHECK(pid.p() == Approx(10).epsilon(0.001));
        CHECK(pid.i() == Approx(5).epsilon(0.02));
        CHECK(pid.d() == 0);
        CHECK(actuator->setting() == Approx(10.0 * (1.0 + 1000 * 1.0 / 2000)).epsilon(0.02));

        run1000seconds();

        CHECK(pid.p() == Approx(10).epsilon(0.001));
        CHECK(pid.i() == Approx(10).epsilon(0.02)); // more margin for anti-windup due to PWM lag
        CHECK(pid.d() == 0);
        CHECK(actuator->setting() == Approx(10.0 * (1.0 + 2000 * 1.0 / 2000)).epsilon(0.02));
    }

    WHEN("Proportional, Integral and Derivative are enabled, the output value is correct with positive Kp")
    {
        pid.kp(10);
        pid.ti(2000);
        pid.td(60);

        input->setting(30);
        sensor->setting(20);
        input->resetFilter();

        fp12_t mockVal;
        double integralValue = 0;

        auto start = now;
        while (now <= start + 900'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                mockVal = fp12_t(20.0 + 9.0 * (now - start) / 900'000);
                sensor->setting(mockVal);
                input->update();
                pid.update();
                actuator->update();
                integralValue += double(pid.p() + pid.d()) / double(pid.kp());
                nextPidUpdate = now + 1000;
            }
            ++now;
        }

        CHECK(mockVal == 29);
        CHECK(pid.error() == Approx(1).epsilon(0.1)); // the filter introduces some delay, which is why this is not 1.0
        CHECK(pid.p() == Approx(10).epsilon(0.1));
        CHECK(pid.i() == Approx(integralValue * pid.kp() / pid.ti()).epsilon(0.03)); // some integral anti-windup will occur due to filtering at the start
        CHECK(pid.d() == Approx(-10 * 9.0 / 900 * 60).epsilon(0.01));

        CHECK(actuator->setting() == pid.p() + pid.i() + pid.d());
    }

    WHEN("Proportional, Integral and Derivative are enabled, the output value is correct with negative Kp")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(60);

        input->setting(20);
        sensor->setting(30);
        input->resetFilter();

        fp12_t mockVal;
        double integralValue = 0;

        auto start = now;
        while (now <= start + 900'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                mockVal = fp12_t(30.0 - 9.0 * (now - start) / 900'000);
                sensor->setting(mockVal);
                input->update();
                pid.update();
                actuator->update();
                integralValue += double(pid.p() + pid.d()) / double(pid.kp());
                nextPidUpdate = now + 1000;
            }
            ++now;
        }

        CHECK(mockVal == 21);
        CHECK(pid.error() == Approx(-1).epsilon(0.1)); // the filter introduces some delay, which is why this is not 1.0
        CHECK(pid.p() == Approx(10).epsilon(0.1));
        CHECK(pid.i() == Approx(integralValue * pid.kp() / pid.ti()).epsilon(0.03)); // some integral anti-windup will occur due to filtering at the start
        CHECK(pid.d() == Approx(-10 * 9.0 / 900 * 60).epsilon(0.02));

        CHECK(actuator->setting() == pid.p() + pid.i() + pid.d());
    }

    WHEN("When Ti is changed")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(60);

        // sensor is left at 20
        input->setting(19);

        auto start = now;
        while (now <= start + 1000'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                input->update();
                pid.update();
                actuator->update();
                nextPidUpdate = now + 1000;
            }
            ++now;
        }

        CHECK(pid.error() == Approx(-1).epsilon(0.01));
        CHECK(pid.p() == Approx(10).epsilon(0.01));
        CHECK(pid.i() == Approx(10.0 * 1000 / 2000).epsilon(0.02)); // more margin for anti-windup due to PWM lag
        CHECK(pid.d() == Approx(0.0).margin(0.01));

        pid.ti(1000);
        input->update();
        pid.update();

        THEN("The integral action is unchanged")
        {
            CHECK(pid.i() == Approx(10.0 * 1000 / 2000).epsilon(0.2));
        }
        THEN("The integral is scaled with the inverse factor of the change")
        {
            CHECK(pid.integral() == Approx(-500).epsilon(0.02));
        }
    }

    WHEN("When Kp is changed")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(60);

        // sensor is left at 20
        input->setting(19);

        auto start = now;
        while (now <= start + 1000'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                input->update();
                pid.update();
                actuator->update();
                nextPidUpdate = now + 1000;
            }
            ++now;
        }

        CHECK(pid.error() == Approx(-1).epsilon(0.01));
        CHECK(pid.p() == Approx(10).epsilon(0.01));
        CHECK(pid.i() == Approx(10.0 * 1000 / 2000).epsilon(0.02)); // more margin for anti-windup due to PWM lag
        CHECK(pid.d() == Approx(0.0).margin(0.01));

        pid.kp(-20);
        input->update();
        pid.update();

        THEN("The integral action is unchanged")
        {
            CHECK(pid.i() == Approx(10.0 * 1000 / 2000).epsilon(0.02));
        }
        THEN("The integral is scaled with the inverse factor of the change")
        {
            CHECK(pid.integral() == Approx(-500).epsilon(0.02));
        }
    }

    WHEN("When a maximum constraint is added to the actuator that limits to a value under the proportional part")
    {
        pid.kp(-10);
        pid.ti(2000);
        pid.td(60);

        // sensor is left at 20
        input->setting(15);

        auto start = now;
        while (now <= start + 1000'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                input->update();
                pid.update();
                actuator->update();
                nextPidUpdate = now + 1000;
            }
            ++now;
        }

        CHECK(pid.p() == Approx(50).epsilon(0.01));
        CHECK(pid.i() == Approx(50.0 * 1000 / 2000).epsilon(0.02)); // more margin for anti-windup due to PWM lag
        CHECK(pid.d() == Approx(0.0).margin(0.01));

        CHECK(pid.p() + pid.i() + pid.d() == actuator->setting());

        THEN("The integral will be reduced back to zero by the anti-windup after adding the constraint")
        {
            actuator->addConstraint(std::make_unique<AAConstraints::Maximum<1>>(40));

            start = now;
            while (now <= start + 1000'000) {
                if (now >= nextPwmUpdate) {
                    nextPwmUpdate = pwm.update(now);
                }
                if (now >= nextPidUpdate) {
                    input->update();
                    pid.update();
                    actuator->update();
                    nextPidUpdate = now + 1000;
                }
                ++now;
            }

            input->update();
            pid.update();

            CHECK(pid.p() == Approx(50).epsilon(0.01));
            CHECK(pid.i() == Approx(0.0).margin(0.05));
            CHECK(pid.d() == Approx(0.0).margin(0.01));

            CHECK(pid.p() + pid.i() + pid.d() != actuator->setting());
        }

        THEN("The integral will be reduced back to zero if increased Kp makes the proportional part over 100")
        {
            pid.kp(-25);

            start = now;
            while (now <= start + 1000'000) {
                if (now >= nextPwmUpdate) {
                    nextPwmUpdate = pwm.update(now);
                }
                if (now >= nextPidUpdate) {
                    input->update();
                    pid.update();
                    actuator->update();
                    nextPidUpdate = now + 1000;
                }
                ++now;
            }

            input->update();
            pid.update();

            CHECK(pid.p() == Approx(125).epsilon(0.01));
            CHECK(pid.i() == Approx(0.0).margin(0.1));
            CHECK(pid.d() == Approx(0.0).margin(0.01));

            CHECK(pid.p() + pid.i() + pid.d() != actuator->setting());
        }
    }

    WHEN("The PID is disabled")
    {
        pid.enabled(false);
        input->update();
        pid.update();

        THEN("The pwm setting is set to invalid")
        {
            CHECK(actuator->settingValid() == false);
        }

        AND_WHEN("It is set manually later")
        {
            actuator->setting(50);
            actuator->settingValid(true);
            CHECK(actuator->settingValid() == true);
            CHECK(actuator->setting() == 50);
            input->update();
            pid.update();

            THEN("The disabled PID doesn't affect it")
            {
                CHECK(actuator->settingValid() == true);
                CHECK(actuator->setting() == 50.0);
            }
        }
    }

    WHEN("The integral part is set externally")
    {
        pid.kp(10);
        pid.ti(1000);
        pid.td(0);
        pid.setIntegral(20);
        pid.update();

        THEN("The new output matches what whas set")
        {
            CHECK(pid.i() == Approx(20).epsilon(0.001));
        }
    }

    WHEN("A step response is applied to the input for various Td")
    {
        pid.kp(10);
        pid.ti(0);
        input->filterChoice(1);
        input->filterThreshold(99);
        auto testStep = [&](uint16_t td) {
            pid.td(td);
            pid.update();
            auto start = now;
            auto changeInputAt = now + 10'000;
            auto dMax = Pid::derivative_t(0);
            auto dMaxTime = now;
            sensor->setting(20);
            input->resetFilter();
            nextPwmUpdate = now;
            nextPidUpdate = now;

            while (now <= start + 7200'000) {
                if (now >= nextPwmUpdate) {
                    nextPwmUpdate = pwm.update(now);
                }
                if (now == changeInputAt) {
                    sensor->setting(21);
                }
                if (now >= nextPidUpdate) {
                    input->update();
                    pid.update();
                    if (pid.derivative() > dMax) {
                        dMax = pid.derivative();
                        dMaxTime = now;
                    }
                    if (pid.derivative() < dMax && dMax != 0) {
                        break; // passed the maximum
                    }
                    nextPidUpdate = now + 1000;
                }
                ++now;
            }
            auto lag = (dMaxTime - changeInputAt) / 1000; // return lag in seconds
            return lag;
        };

        THEN("A derivative filter is selected so that the lag between value max derivative is less than Td*1.5")
        {
            // for the derivative to compensate overshoot, the derivative should not lag the sensor too much
            // Td is the estimated overshoot time, so the lag in process response after changing the input
            // The derivative should not lag much more than Td, or it will be too late.
            // With filter lag at Td, the max derivative of an input step will be reached after Td seconds have elapsed
            // This seems a good middle ground between noise suppression and derivative response
            std::vector<uint16_t> tdValues{10, 30, 60, 90, 120, 180, 240, 300, 450, 600, 900, 1200, 1800, 3600};
            for (auto td : tdValues) {
                CAPTURE(td);
                pid.td(td);
                pid.update();
                auto selectedFilter = pid.derivativeFilterNr();
                CAPTURE(selectedFilter);
                auto lag = testStep(td);
                CHECK(lag < td * 1.5);
            }
        }
    }

    WHEN("The boil min output is set to 40")
    {
        input->filterChoice(0); // no filtering
        pid.boilMinOutput(40);
        pid.update();
        pid.kp(10);
        AND_WHEN("The setpoint is 99 and the actual temp is 98")
        {
            input->setting(99);
            sensor->setting(98);
            input->update();
            pid.update();

            THEN("The output of the PID is 10 (normal)")
            {
                CHECK(!pid.boilModeActive());
                CHECK(actuator->settingValid() == true);
                CHECK(actuator->setting() == 10.0);
            }
        }

        AND_WHEN("The setpoint is 100 and the actual temp is 99")
        {
            input->setting(100);
            sensor->setting(99);
            input->update();
            pid.update();

            THEN("The output of the PID is 40 and the integral zero (boil mode active)")
            {
                CHECK(pid.boilModeActive());
                CHECK(pid.i() == 0);
                CHECK(actuator->settingValid() == true);
                CHECK(actuator->setting() == 40.0);
            }
        }

        AND_WHEN("The setpoint is 100 and the actual temp is 100")
        {
            input->setting(100);
            sensor->setting(100);
            input->resetFilter();
            input->update();
            pid.update();

            THEN("The output of the PID is 40")
            {
                CHECK(pid.boilModeActive());
                CHECK(actuator->settingValid() == true);
                CHECK(actuator->setting() == 40.0);
            }

            AND_WHEN("The the boil point is adjusted by +1")
            {
                pid.boilPointAdjust(1);
                pid.update();
                THEN("The boil mode doesn't trigger and the output of the PID is 0")
                {
                    CHECK(!pid.boilModeActive());
                    CHECK(actuator->settingValid() == true);
                    CHECK(actuator->setting() == 0.0);
                }
            }
        }
    }

    WHEN("The PID heats a kettle until setpoint is reached")
    {
        input->filterChoice(0); // no filtering
        pid.kp(100);
        pid.td(120);
        pid.ti(1200);
        pid.update();

        input->setting(60);
        sensor->setting(20);
        duration_millis_t closeDuration = 0;
        duration_millis_t reachedDuration = 0;

        auto start = now;
        while (now < start + 100000'000) {
            if (now >= nextPwmUpdate) {
                nextPwmUpdate = pwm.update(now);
            }
            if (now >= nextPidUpdate) {
                auto newTemp = sensor->value() + actuator->value() / 2400; // very simple model for heating
                sensor->setting(newTemp);
                input->update();
                pid.update();
                actuator->update();
                nextPidUpdate = now + 1000;
            }
            if (!closeDuration && sensor->value() + 1 >= input->setting()) {
                closeDuration = now - start;
            }
            if (!reachedDuration && sensor->value() + 0.1 >= input->setting()) {
                reachedDuration = now - start;
                break;
            }
            now = std::min(nextPwmUpdate, nextPidUpdate);
        }
        CHECK(closeDuration == Approx(1020'000).margin(10000));   // takes 17.0 minutes to get to setpoint - 1.0
        CHECK(reachedDuration == Approx(1242'000).margin(10000)); // takes 20.7 minutes to get to setpoint - 0.1
        AND_THEN("The overshoot is minimal and the integral is close to zero")
        {

            CHECK(input->value() - input->setting() < 0.1); // overshoot is small
            CHECK(pid.i() < 1.0);
        }
    }
}