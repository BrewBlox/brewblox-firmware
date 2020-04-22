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
#include "blox/ActuatorPwmBlock.h"
#include "blox/BalancerBlock.h"
#include "blox/DigitalActuatorBlock.h"
#include "blox/MutexBlock.h"
#include "proto/test/cpp/ActuatorPwm_test.pb.h"
#include "proto/test/cpp/Balancer_test.pb.h"
#include "proto/test/cpp/DigitalActuator_test.pb.h"
#include "proto/test/cpp/Mutex_test.pb.h"

SCENARIO("Two pin actuators are constrained by a mutex", "[balancer, mutex]")
{
    BrewBloxTestBox testBox;
    using commands = cbox::Box::CommandID;

    auto mutexId = cbox::obj_id_t(101);
    auto pin1Id = cbox::obj_id_t(102);
    auto pin2Id = cbox::obj_id_t(103);

    testBox.reset();

    // create mutex
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(mutexId);
    testBox.put(uint8_t(0xFF));
    testBox.put(MutexBlock::staticTypeId());
    {
        auto newMutex = blox::Mutex();
        newMutex.set_differentactuatorwait(100);
        testBox.put(newMutex);
    }
    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create/set digital actuator helper

    auto setPin = [&testBox, mutexId, pin1Id, pin2Id](uint8_t chan, blox::DigitalState state, bool firstCreate = false) {
        // configure digital actuator by writing to the object
        testBox.put(uint16_t(0)); // msg id
        testBox.put(firstCreate ? commands::CREATE_OBJECT : commands::WRITE_OBJECT);
        testBox.put(chan == 1 ? pin1Id : pin2Id);
        testBox.put(uint8_t(0xFF));
        testBox.put(DigitalActuatorBlock::staticTypeId());

        auto pinMsg = blox::DigitalActuator();
        pinMsg.set_desiredstate(state);
        pinMsg.set_invert(false);
        pinMsg.set_hwdevice(19);
        pinMsg.set_channel(chan);
        auto constraintPtr = pinMsg.mutable_constrainedby()->add_constraints();
        constraintPtr->set_mutex(mutexId);
        testBox.put(pinMsg);

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());
    };

    // configure pin actuator 1
    setPin(1, blox::DigitalState::Active, true);
    // configure pin actuator 2
    setPin(2, blox::DigitalState::Active, true);

    THEN("The objects read back as expected")
    {
        // read mutex
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put(mutexId);

        {
            auto decoded = blox::Mutex();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "differentActuatorWait: 100 waitRemaining: 100");
        }

        // read pin actuator 1, which is active
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put(pin1Id);

        {
            auto decoded = blox::DigitalActuator();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "hwDevice: 19 channel: 1 state: Active constrainedBy { constraints { mutexed { mutexId: 101 hasLock: true } } } desiredState: Active");
        }

        // read pin actuator 2, which is constrained and inactive
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put(pin2Id);

        {
            auto decoded = blox::DigitalActuator();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "hwDevice: 19 channel: 2 constrainedBy { constraints { mutexed { mutexId: 101 } remaining: 101 } } desiredState: Active");
        }
    }

    WHEN("actuator 2 is toggled, it remains constrained while actuator 1 is active")
    {
        auto readPin = [&testBox](cbox::obj_id_t id) {
            testBox.put(uint16_t(0)); // msg id
            testBox.put(commands::READ_OBJECT);
            testBox.put(id);

            auto decoded = blox::DigitalActuator();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());

            return decoded.state();
        };
        setPin(1, blox::DigitalState::Active);
        setPin(2, blox::DigitalState::Inactive);
        testBox.update(1);

        setPin(2, blox::DigitalState::Active);
        CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);
        testBox.update(2);

        setPin(2, blox::DigitalState::Active);
        CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

        setPin(2, blox::DigitalState::Inactive);
        CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);
        testBox.update(3);

        setPin(2, blox::DigitalState::Active);
        CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);
        testBox.update(4);

        AND_WHEN("Actuator 1 is turned OFF")
        {
            setPin(1, blox::DigitalState::Inactive);
            CHECK(readPin(pin1Id) == blox::DigitalState::Inactive);
            testBox.update(5);

            THEN("Actuator 2 can only turn on after the minimum wait time has passed")
            {
                testBox.update(6);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(10);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(103);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(104);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Active);
            }

            THEN("Activating actuator 1 resets the wait time")
            {
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(10);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(50);
                setPin(1, blox::DigitalState::Active);
                CHECK(readPin(pin1Id) == blox::DigitalState::Active);

                testBox.update(60);
                setPin(1, blox::DigitalState::Inactive);
                CHECK(readPin(pin1Id) == blox::DigitalState::Inactive);

                testBox.update(61);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(120);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(155);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Inactive);

                testBox.update(160);
                setPin(2, blox::DigitalState::Active);
                CHECK(readPin(pin2Id) == blox::DigitalState::Active);
            }
        }
    }

    WHEN("The pins are driven by 2 balanced PWM actuators")
    {
        auto balancerId = cbox::obj_id_t(200);
        auto pwm1Id = cbox::obj_id_t(201);
        auto pwm2Id = cbox::obj_id_t(202);

        // create balancer
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::CREATE_OBJECT);
        testBox.put(balancerId);
        testBox.put(uint8_t(0xFF));
        testBox.put(BalancerBlock::staticTypeId());
        {
            auto newBalancer = blox::Balancer();
            testBox.put(newBalancer);
        }
        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        // create pwm actuator 1
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::CREATE_OBJECT);
        testBox.put(pwm1Id);
        testBox.put(uint8_t(0xFF));
        testBox.put(ActuatorPwmBlock::staticTypeId());

        {
            auto newPwm = blox::ActuatorPwm();
            newPwm.set_actuatorid(pin1Id);
            newPwm.set_desiredsetting(cnl::unwrap(ActuatorAnalog::value_t(80)));
            newPwm.set_period(4000);
            newPwm.set_enabled(true);

            auto c = newPwm.mutable_constrainedby()->add_constraints();
            auto balanced = new blox::AnalogConstraint_Balanced();
            balanced->set_balancerid(balancerId);
            c->set_allocated_balanced(balanced);

            testBox.put(newPwm);
        }
        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        // create pwm actuator 2
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::CREATE_OBJECT);
        testBox.put(pwm2Id);
        testBox.put(uint8_t(0xFF));
        testBox.put(ActuatorPwmBlock::staticTypeId());

        {
            auto newPwm = blox::ActuatorPwm();
            newPwm.set_actuatorid(pin2Id);
            newPwm.set_desiredsetting(cnl::unwrap(ActuatorAnalog::value_t(80)));
            newPwm.set_period(4000);
            newPwm.set_enabled(true);

            auto c = newPwm.mutable_constrainedby()->add_constraints();
            auto balanced = new blox::AnalogConstraint_Balanced();
            balanced->set_balancerid(balancerId);
            c->set_allocated_balanced(balanced);

            testBox.put(newPwm);
        }

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        testBox.update(0);
        testBox.update(1000);

        // read balancer
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put(balancerId);

        {
            auto decoded = blox::Balancer();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "clients { id: 1 requested: 327680 granted: 204800 } "  // 80*4096, 50*4096
                                                "clients { id: 2 requested: 327680 granted: 204800 }"); // 80*4096, 50*4096
        }

        // read a pwm actuator 1
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put(pwm1Id);

        {
            auto decoded = blox::ActuatorPwm();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "actuatorId: 102 "
                                                "period: 4000 setting: 204800 "
                                                "constrainedBy { "
                                                "constraints { "
                                                "balanced { balancerId: 200 granted: 204800 id: 1 } "
                                                "limiting: true } } "
                                                "drivenActuatorId: 102 "
                                                "enabled: true "
                                                "desiredSetting: 327680");
        }

        // read a pwm actuator 2
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put(pwm2Id);

        {
            auto decoded = blox::ActuatorPwm();
            testBox.processInputToProto(decoded);
            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "actuatorId: 103 "
                                                "period: 4000 setting: 204800 "
                                                "constrainedBy { "
                                                "constraints { "
                                                "balanced { balancerId: 200 granted: 204800 id: 2 } "
                                                "limiting: true } } "
                                                "drivenActuatorId: 103 "
                                                "enabled: true "
                                                "desiredSetting: 327680");
        }

        // run for a while
        for (ticks_millis_t now = 1001; now < 50000; ++now) {
            testBox.update(now);
        }

        // read a pwm actuator 1
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put((pwm1Id));

        {
            auto decoded = blox::ActuatorPwm();
            testBox.processInputToProto(decoded);
            CHECK(decoded.value() == Approx(cnl::unwrap(ActuatorAnalog::value_t(50))).epsilon(0.05));
        }

        // read a pwm actuator 2
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::READ_OBJECT);
        testBox.put((pwm2Id));

        {
            auto decoded = blox::ActuatorPwm();
            testBox.processInputToProto(decoded);
            CHECK(decoded.value() == Approx(cnl::unwrap(ActuatorAnalog::value_t(50))).epsilon(0.05));
        }
    }
}
