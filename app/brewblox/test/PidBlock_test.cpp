/*
 * Copyright 2018 BrewPi B.V.
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

#include "BrewBloxTestBox.h"
#include "Temperature.h"
#include "blox/ActuatorAnalogMockBlock.h"
#include "blox/PidBlock.h"
#include "blox/SetpointSensorPairBlock.h"
#include "blox/TempSensorMockBlock.h"
#include "proto/test/cpp/ActuatorAnalogMock_test.pb.h"
#include "proto/test/cpp/Pid_test.pb.h"
#include "proto/test/cpp/SetpointSensorPair_test.pb.h"
#include "proto/test/cpp/TempSensorMock_test.pb.h"

extern cbox::Box&
makeBrewBloxBox();

SCENARIO("A Blox Pid object with mock analog actuator")
{
    BrewBloxTestBox testBox;
    using commands = cbox::Box::CommandID;

    testBox.reset();
    auto sensorId = cbox::obj_id_t(100);
    auto setpointId = cbox::obj_id_t(101);
    auto actuatorId = cbox::obj_id_t(102);
    auto pidId = cbox::obj_id_t(103);

    // create mock sensor
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(sensorId);
    testBox.put(uint8_t(0xFF));
    testBox.put(TempSensorMockBlock::staticTypeId());

    auto newSensor = blox::TempSensorMock();
    newSensor.set_value(cnl::unwrap(temp_t(20.0)));
    newSensor.set_connected(true);
    testBox.put(newSensor);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create pair
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(setpointId));
    testBox.put(uint8_t(0xFF));
    testBox.put(SetpointSensorPairBlock::staticTypeId());

    blox::SetpointSensorPair newPair;
    newPair.set_sensorid(sensorId);
    newPair.set_storedsetting(cnl::unwrap(temp_t(21)));
    newPair.set_settingenabled(true);
    newPair.set_filter(blox::SetpointSensorPair_FilterChoice::SetpointSensorPair_FilterChoice_FILT_15s);
    newPair.set_filterthreshold(cnl::unwrap(temp_t(1)));
    testBox.put(newPair);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create actuator
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(actuatorId));
    testBox.put(uint8_t(0xFF));
    testBox.put(ActuatorAnalogMockBlock::staticTypeId());

    blox::ActuatorAnalogMock newActuator;
    newActuator.set_setting(cnl::unwrap(ActuatorAnalog::value_t(0)));
    newActuator.set_minsetting(cnl::unwrap(ActuatorAnalog::value_t(0)));
    newActuator.set_maxsetting(cnl::unwrap(ActuatorAnalog::value_t(100)));
    newActuator.set_minvalue(cnl::unwrap(ActuatorAnalog::value_t(0)));
    newActuator.set_maxvalue(cnl::unwrap(ActuatorAnalog::value_t(100)));
    testBox.put(newActuator);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create Pid
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(pidId));
    testBox.put(uint8_t(0xFF));
    testBox.put(PidBlock::staticTypeId());

    blox::Pid newPid;
    newPid.set_inputid(setpointId);
    newPid.set_outputid(actuatorId);
    newPid.set_enabled(true);
    newPid.set_kp(cnl::unwrap(Pid::in_t(10)));
    newPid.set_ti(2000);
    newPid.set_td(200);
    newPid.set_boilpointadjust(cnl::unwrap(Pid::in_t(-0.5)));
    newPid.set_boilminoutput(cnl::unwrap(Pid::in_t(25)));

    testBox.put(newPid);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // update 999 seconds (PID updates every second, now is in ms)
    // one extra update will be triggered on proto receive
    uint32_t now = 0;
    for (; now < 999'000; now += 100) {
        testBox.update(now);
    }

    // read PID
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::READ_OBJECT);
    testBox.put(cbox::obj_id_t(pidId));

    auto decoded = blox::Pid();
    testBox.processInputToProto(decoded);
    CHECK(testBox.lastReplyHasStatusOk());

    CHECK(cnl::wrap<Pid::out_t>(decoded.p()) == Approx(10.0).epsilon(0.01));
    CHECK(cnl::wrap<Pid::out_t>(decoded.i()) == Approx(10.0 * 1.0 * 1000 / 2000).epsilon(0.01));
    CHECK(cnl::wrap<Pid::out_t>(decoded.d()) == 0);
    CHECK(cnl::wrap<Pid::out_t>(decoded.outputvalue()) == Approx(15.0).epsilon(0.01));

    THEN("The decoded proto message is correct")
    {
        // only nonzero values are shown in the debug string
        CHECK(decoded.ShortDebugString() == "inputId: 101 outputId: 102 "
                                            "inputValue: 81920 inputSetting: 86016 "
                                            "outputValue: 61439 outputSetting: 61439 "
                                            "enabled: true active: true "
                                            "kp: 40960 ti: 2000 td: 200 "
                                            "p: 40960 i: 20479 "
                                            "error: 4096 integral: 4096000 "
                                            "drivenOutputId: 102 "
                                            "boilPointAdjust: -2048 "
                                            "boilMinOutput: 102400");
    }

    THEN("The integral value can be written externally to reset it trough the integralReset field")
    {

        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t(pidId));
        testBox.put(uint8_t(0xFF));
        testBox.put(PidBlock::staticTypeId());

        newPid.set_integralreset(cnl::unwrap(Pid::out_t(20)));
        testBox.put(newPid);

        auto decoded = blox::Pid();
        testBox.processInputToProto(decoded);

        CHECK(testBox.lastReplyHasStatusOk());
        CHECK(decoded.ShortDebugString() == "inputId: 101 outputId: 102 "
                                            "inputValue: 81920 inputSetting: 86016 "
                                            "outputValue: 122900 outputSetting: 122900 "
                                            "enabled: true active: true "
                                            "kp: 40960 ti: 2000 td: 200 "
                                            "p: 40960 i: 81940 "
                                            "error: 4096 integral: 16388096 "
                                            "drivenOutputId: 102 "
                                            "boilPointAdjust: -2048 "
                                            "boilMinOutput: 102400");
    }

    AND_WHEN("The setpoint is disabled")
    {
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t(setpointId));
        testBox.put(uint8_t(0xFF));
        testBox.put(SetpointSensorPairBlock::staticTypeId());
        newPair.set_settingenabled(false);
        testBox.put(newPair);

        auto decoded = blox::SetpointSensorPair();
        testBox.processInputToProto(decoded);

        CHECK(testBox.lastReplyHasStatusOk());

        brewbloxBox().update(now + 2000);
        auto pidLookup = brewbloxBox().makeCboxPtr<PidBlock>(pidId);
        auto actuatorLookup = brewbloxBox().makeCboxPtr<ActuatorAnalogMockBlock>(actuatorId);
        THEN("The PID becomes inactive")
        {
            auto pid = pidLookup.lock();
            CHECK(pid->get().active() == false);
        }
        THEN("The Actuator is set to zero and setting invalid")
        {
            auto act = actuatorLookup.lock();
            CHECK(act->get().setting() == 0);
            CHECK(act->get().settingValid() == false);
        }
    }

    AND_WHEN("The setpoint is set to 99.5, with boil adjust at -0.5, it activates boil mode")
    {
        // change mock sensor
        testBox.put(uint16_t(1)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(sensorId);
        testBox.put(uint8_t(0xFF));
        testBox.put(TempSensorMockBlock::staticTypeId());

        auto newSensor = blox::TempSensorMock();
        newSensor.set_value(cnl::unwrap(temp_t(99.5)));
        newSensor.set_connected(true);
        testBox.put(newSensor);

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        // change setpoint
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t(setpointId));
        testBox.put(uint8_t(0xFF));
        testBox.put(SetpointSensorPairBlock::staticTypeId());
        newPair.set_settingenabled(true);
        newPair.set_storedsetting(cnl::unwrap(Pid::in_t(99.5)));
        newPair.set_filter(blox::SetpointSensorPair_FilterChoice(0)); // no filtering
        testBox.put(newPair);

        auto decoded = blox::SetpointSensorPair();
        testBox.processInputToProto(decoded);

        CHECK(testBox.lastReplyHasStatusOk());

        brewbloxBox().update(now + 2000);
        THEN("Boil mode becomes active and the PID output is the minimum output")
        {
            testBox.put(uint16_t(0)); // msg id
            testBox.put(commands::READ_OBJECT);
            testBox.put(cbox::obj_id_t(pidId));
            testBox.put(uint8_t(0xFF));
            testBox.put(PidBlock::staticTypeId());

            auto decoded = blox::Pid();
            testBox.processInputToProto(decoded);

            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "inputId: 101 outputId: 102 "
                                                "inputValue: 407552 inputSetting: 407552 "
                                                "outputValue: 102400 outputSetting: 102400 "
                                                "enabled: true active: true "
                                                "kp: 40960 ti: 2000 td: 200 "
                                                "drivenOutputId: 102 "
                                                "boilPointAdjust: -2048 "
                                                "boilMinOutput: 102400 "
                                                "boilModeActive: true");
        }
    }
}
