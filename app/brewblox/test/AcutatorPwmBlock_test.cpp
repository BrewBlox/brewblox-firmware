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
#include "blox/DigitalActuatorBlock.h"
#include "proto/test/cpp/ActuatorPwm_test.pb.h"
#include "proto/test/cpp/DigitalActuator_test.pb.h"

SCENARIO("A Blox ActuatorPwm object can be created from streamed protobuf data")
{
    BrewBloxTestBox testBox;
    using commands = cbox::Box::CommandID;

    testBox.reset();

    auto actId = cbox::obj_id_t(100);
    auto pwmId = cbox::obj_id_t(101);
    auto sparkPinsId = cbox::obj_id_t(19); // system object 19 is Spark IO pins

    // create digital actuator with Spark pin as target
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(actId));
    testBox.put(uint8_t(0xFF));
    testBox.put(DigitalActuatorBlock::staticTypeId());

    auto message = blox::DigitalActuator();
    message.set_hwdevice(sparkPinsId);
    message.set_channel(1);
    message.set_state(blox::DigitalState::Inactive);

    testBox.put(message);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create pwm actuator
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(pwmId));
    testBox.put(uint8_t(0xFF));
    testBox.put(ActuatorPwmBlock::staticTypeId());

    blox::ActuatorPwm newPwm;
    newPwm.set_actuatorid(actId); // predefined system object for pin actuator
    newPwm.set_desiredsetting(cnl::unwrap(ActuatorAnalog::value_t(20)));
    newPwm.set_period(4000);
    newPwm.set_enabled(true);
    auto c = newPwm.mutable_constrainedby()->add_constraints();
    c->set_min(cnl::unwrap(ActuatorAnalog::value_t(10)));

    testBox.put(newPwm);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // read pwm
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::READ_OBJECT);
    testBox.put(cbox::obj_id_t(pwmId));

    auto decoded = blox::ActuatorPwm();
    testBox.processInputToProto(decoded);

    CHECK(testBox.lastReplyHasStatusOk());
    CHECK(decoded.ShortDebugString() == "actuatorId: 100 "
                                        "period: 4000 setting: 81920 "
                                        "constrainedBy { constraints { min: 40960 } } "
                                        "drivenActuatorId: 100 "
                                        "enabled: true "
                                        "desiredSetting: 81920");
}
