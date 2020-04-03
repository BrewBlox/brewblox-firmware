/*
 * Copyright 2020 BrewPi B.V.
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
#include "blox/ActuatorLogicBlock.h"
#include "blox/DigitalActuatorBlock.h"
#include "blox/SetpointSensorPairBlock.h"
#include "blox/TempSensorMockBlock.h"
#include "cbox/DataStreamIo.h"
#include "proto/test/cpp/ActuatorLogic_test.pb.h"
#include "proto/test/cpp/DigitalActuator_test.pb.h"
#include "proto/test/cpp/SetpointSensorPair_test.pb.h"
#include "proto/test/cpp/TempSensorMock_test.pb.h"
#include <sstream>

SCENARIO("Test", "[maklogicblock]")
{
    CHECK(true);
    BrewBloxTestBox testBox;
    using commands = cbox::Box::CommandID;

    testBox.reset();

    // acuators 101-105
    // sensors 111-115
    // setpoints 121-125
    // logic 130

    auto setAct = [&testBox](cbox::obj_id_t id, blox::DigitalState state, bool firstCreate = false) {
        // configure digital actuator by writing to the object
        auto sparkPinsId = cbox::obj_id_t(19); // system object 19 is Spark IO pins

        testBox.put(uint16_t(0)); // msg id
        testBox.put(firstCreate ? commands::CREATE_OBJECT : commands::WRITE_OBJECT);
        testBox.put(id);
        testBox.put(uint8_t(0xFF));
        testBox.put(DigitalActuatorBlock::staticTypeId());

        auto message = blox::DigitalActuator();
        message.set_hwdevice(sparkPinsId);
        message.set_channel(uint8_t(id - 100));
        message.set_desiredstate(state);

        testBox.put(message);

        testBox.processInput();
        REQUIRE(testBox.lastReplyHasStatusOk());
    };

    auto setSensor = [&testBox](cbox::obj_id_t id, temp_t setting, bool firstCreate = false) {
        // create mock sensor or write to existing
        testBox.put(uint16_t(0)); // msg id
        testBox.put(firstCreate ? commands::CREATE_OBJECT : commands::WRITE_OBJECT);
        testBox.put(id);
        testBox.put(uint8_t(0xFF));
        testBox.put(TempSensorMockBlock::staticTypeId());

        auto newSensor = blox::TempSensorMock();
        newSensor.set_setting(cnl::unwrap(setting));
        newSensor.set_connected(true);
        testBox.put(newSensor);

        testBox.processInput();
        REQUIRE(testBox.lastReplyHasStatusOk());
    };

    auto setSetpoint = [&testBox](cbox::obj_id_t id, temp_t setting, bool firstCreate = false) {
        // create setpoint or write to existing
        testBox.put(uint16_t(0)); // msg id
        testBox.put(firstCreate ? commands::CREATE_OBJECT : commands::WRITE_OBJECT);
        testBox.put(id);
        testBox.put(uint8_t(0xFF));
        testBox.put(SetpointSensorPairBlock::staticTypeId());

        blox::SetpointSensorPair newPair;
        newPair.set_sensorid(id - 10);
        newPair.set_settingenabled(true);
        newPair.set_storedsetting(cnl::unwrap(setting));
        newPair.set_filter(blox::SetpointSensorPair::FilterChoice::SetpointSensorPair_FilterChoice_FILT_NONE);
        newPair.set_filterthreshold(cnl::unwrap(temp_t(0.5)));
        testBox.put(newPair);
        testBox.processInput();
        REQUIRE(testBox.lastReplyHasStatusOk());
    };

    // create 5 digital actuators
    for (cbox::obj_id_t i = 101; i <= 105; i++) {
        setAct(i, blox::DigitalState::Inactive, true);
    }

    // create 5 mock sensors
    for (cbox::obj_id_t i = 111; i <= 115; i++) {
        setSensor(i, temp_t{20}, true);
    }

    // create 5 setpoints
    for (cbox::obj_id_t i = 121; i <= 125; i++) {
        setSetpoint(i, temp_t{21}, true);
    }

    auto setLogic = [&testBox](blox::ActuatorLogic& message, bool firstCreate = false) {
        testBox.put(uint16_t(0)); // msg id
        testBox.put(firstCreate ? commands::CREATE_OBJECT : commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t{130});
        testBox.put(uint8_t(0xFF));
        testBox.put(ActuatorLogicBlock::staticTypeId());

        message.set_targetid(105);
        message.set_enabled(true);

        testBox.put(message);

        auto decoded = blox::ActuatorLogic();
        testBox.processInputToProto(decoded);
        REQUIRE(testBox.lastReplyHasStatusOk());
        return decoded;
    };

    // create logic block with emty logic
    auto message = blox::ActuatorLogic();
    auto result = setLogic(message, true);
    CHECK(result.ShortDebugString() == "targetId: 105 drivenTargetId: 105 enabled: true result: EMPTY");

    WHEN("4 digital actuators are combined with various expressions")
    {
        auto message = blox::ActuatorLogic();
        {
            auto d = message.add_digital();
            d->set_id(101);
            d->set_rhs(blox::DigitalState::Active);
            d->set_op(blox::ActuatorLogic_DigitalCompareOp_DESIRED_IS);
        }
        {
            auto d = message.add_digital();
            d->set_id(102);
            d->set_rhs(blox::DigitalState::Active);
            d->set_op(blox::ActuatorLogic_DigitalCompareOp_DESIRED_IS);
        }
        {
            auto d = message.add_digital();
            d->set_id(103);
            d->set_rhs(blox::DigitalState::Active);
            d->set_op(blox::ActuatorLogic_DigitalCompareOp_DESIRED_IS);
        }
        {
            auto d = message.add_digital();
            d->set_id(104);
            d->set_rhs(blox::DigitalState::Active);
            d->set_op(blox::ActuatorLogic_DigitalCompareOp_DESIRED_IS);
        }

        THEN("The target is active when the expression is true")
        {
            // or
            message.set_expression("a|b|c");

            auto result = setLogic(message);
            CHECK(result.ShortDebugString() == "targetId: 105 drivenTargetId: 105 enabled: true expression: \"a|b|c\" digital { op: DESIRED_IS id: 101 rhs: Active } digital { op: DESIRED_IS id: 102 rhs: Active } digital { op: DESIRED_IS id: 103 rhs: Active } digital { op: DESIRED_IS id: 104 rhs: Active }");

            setAct(101, blox::DigitalState::Inactive);
            setAct(102, blox::DigitalState::Active);
            setAct(103, blox::DigitalState::Inactive);

            testBox.update(1000);

            {
                testBox.put(uint16_t(0));
                testBox.put(commands::READ_OBJECT);
                testBox.put(cbox::obj_id_t{130});
                auto decoded = blox::ActuatorLogic();
                testBox.processInputToProto(decoded);
                CHECK(testBox.lastReplyHasStatusOk());
                CHECK(decoded.ShortDebugString() == "targetId: 105 drivenTargetId: 105 enabled: true result: TRUE expression: \"a|b|c\" digital { op: DESIRED_IS id: 101 rhs: Active } digital { op: DESIRED_IS result: TRUE id: 102 rhs: Active } digital { op: DESIRED_IS id: 103 rhs: Active } digital { op: DESIRED_IS id: 104 rhs: Active }");
            }

            setAct(101, blox::DigitalState::Inactive);
            setAct(102, blox::DigitalState::Inactive);
            setAct(103, blox::DigitalState::Active);

            {
                testBox.update(2000);

                testBox.put(uint16_t(0));
                testBox.put(commands::READ_OBJECT);
                testBox.put(cbox::obj_id_t{130});

                auto decoded = blox::ActuatorLogic();
                testBox.processInputToProto(decoded);
                CHECK(testBox.lastReplyHasStatusOk());
                CHECK(decoded.ShortDebugString() == "targetId: 105 drivenTargetId: 105 enabled: true result: TRUE expression: \"a|b|c\" digital { op: DESIRED_IS id: 101 rhs: Active } digital { op: DESIRED_IS id: 102 rhs: Active } digital { op: DESIRED_IS result: TRUE id: 103 rhs: Active } digital { op: DESIRED_IS id: 104 rhs: Active }");
            }

            // brackets
            message.set_expression("a|(b&c)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_FALSE);
            CHECK(result.errorpos() == 0);

            // invert
            message.set_expression("a|!(b&c)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_TRUE);
            CHECK(result.errorpos() == 0);

            setAct(101, blox::DigitalState::Inactive);
            setAct(102, blox::DigitalState::Active);
            setAct(103, blox::DigitalState::Active);

            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_FALSE);
            CHECK(result.errorpos() == 0);

            // xor
            message.set_expression("a^b^c");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_FALSE);
            CHECK(result.errorpos() == 0);

            setAct(101, blox::DigitalState::Inactive);
            setAct(102, blox::DigitalState::Active);
            setAct(103, blox::DigitalState::Inactive);

            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_TRUE);
            CHECK(result.errorpos() == 0);

            // nested brackets and all operators
            message.set_expression("a^!((b|c)&(c|d))");
            setAct(101, blox::DigitalState::Inactive);
            setAct(102, blox::DigitalState::Active);
            setAct(103, blox::DigitalState::Inactive);
            setAct(104, blox::DigitalState::Active);

            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_FALSE);
            CHECK(result.errorpos() == 0);

            setAct(101, blox::DigitalState::Inactive);
            setAct(102, blox::DigitalState::Active);
            setAct(103, blox::DigitalState::Inactive);
            setAct(104, blox::DigitalState::Inactive);

            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_TRUE);
            CHECK(result.errorpos() == 0);

            setAct(101, blox::DigitalState::Active);
            setAct(102, blox::DigitalState::Active);
            setAct(103, blox::DigitalState::Inactive);
            setAct(104, blox::DigitalState::Inactive);

            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_FALSE);
            CHECK(result.errorpos() == 0);
        }

        THEN("The target is inactive when the expression has an error and the correct error code and pos is returned")
        {
            message.set_expression("e&c");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_INVALID_DIG_COMPARE_IDX);
            CHECK(result.errorpos() == 0);

            message.set_expression("E&c");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_INVALID_ANA_COMPARE_IDX);
            CHECK(result.errorpos() == 0);

            message.set_expression("a(|b&c)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPENING_BRACKET);
            CHECK(result.errorpos() == 1);

            message.set_expression("a|(b&c");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_MISSING_CLOSING_BRACKET);
            CHECK(result.errorpos() == 5);

            message.set_expression("a|(b&c))");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_CLOSING_BRACKET);
            CHECK(result.errorpos() == 7);

            message.set_expression("a|(b&.)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_CHARACTER);
            CHECK(result.errorpos() == 5);

            message.set_expression("a|(b&)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_EMPTY_SUBSTRING);
            CHECK(result.errorpos() == 5);

            message.set_expression("a|&b");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPERATOR);
            CHECK(result.errorpos() == 2);

            message.set_expression("a||b");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPERATOR);
            CHECK(result.errorpos() == 2);

            message.set_expression("a&|b");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPERATOR);
            CHECK(result.errorpos() == 2);

            message.set_expression("a^|b");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPERATOR);
            CHECK(result.errorpos() == 2);

            message.set_expression("a!&b");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPERATOR);
            CHECK(result.errorpos() == 2);

            message.set_expression("ab");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_COMPARISON);
            CHECK(result.errorpos() == 1);

            message.set_expression("a(");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_UNEXPECTED_OPENING_BRACKET);
            CHECK(result.errorpos() == 1);

            testBox.put(uint16_t(0)); // msg id
            testBox.put(commands::DELETE_OBJECT);
            testBox.put(cbox::obj_id_t(102));
            testBox.processInput();
            CHECK(testBox.lastReplyHasStatusOk());

            message.set_expression("b");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_BLOCK_NOT_FOUND);
            CHECK(result.errorpos() == 0);
        }

        AND_WHEN("Analog comparisons are used")
        {
            // sensors are 20, setpoints are 21
            message.clear_digital();
            {
                auto d = message.add_analog();
                d->set_id(121);
                d->set_rhs(cnl::unwrap(temp_t{21}));
                d->set_op(blox::ActuatorLogic_AnalogCompareOp_VALUE_GE);
                // false: 20 >= 21
            }
            {
                auto d = message.add_analog();
                d->set_id(122);
                d->set_rhs(cnl::unwrap(temp_t{21}));
                d->set_op(blox::ActuatorLogic_AnalogCompareOp_SETTING_GE);
                // true: 21 >= 21
            }
            {
                auto d = message.add_analog();
                d->set_id(123);
                d->set_rhs(cnl::unwrap(temp_t{21}));
                d->set_op(blox::ActuatorLogic_AnalogCompareOp_VALUE_LE);
                // true: 20 <= 21
            }
            {
                auto d = message.add_analog();
                d->set_id(124);
                d->set_rhs(cnl::unwrap(temp_t{20.5}));
                d->set_op(blox::ActuatorLogic_AnalogCompareOp_SETTING_LE);
                // false: 21 <= 20.5
            }

            message.set_expression("A|B|C|D");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_TRUE);
            CHECK(result.ShortDebugString() == "targetId: 105 drivenTargetId: 105 enabled: true result: TRUE expression: \"A|B|C|D\" analog { op: VALUE_GE id: 121 rhs: 86016 } analog { op: SETTING_GE result: TRUE id: 122 rhs: 86016 } analog { result: TRUE id: 123 rhs: 86016 } analog { op: SETTING_LE id: 124 rhs: 83968 }");

            message.set_expression("(A|B)&(C|D)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_TRUE);

            message.set_expression("(A&B)|(C&D)");
            result = setLogic(message);
            CHECK(result.result() == blox::ActuatorLogic_Result_FALSE);
        }
    }
}
