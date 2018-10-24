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
#include "blox/TempSensorOneWireBlock.h"
#include "cbox/CboxPtr.h"
#include "cbox/DataStreamIo.h"
#include "proto/test/cpp/TempSensorOneWire.test.pb.h"
#include <sstream>

SCENARIO("A TempSensorOneWireBlock")
{
    WHEN("a TempSensorOneWire object is created")
    {
        BrewBloxTestBox testBox;
        using commands = cbox::Box::CommandID;

        testBox.reset();

        testBox.put(commands::CREATE_OBJECT);
        testBox.put(cbox::obj_id_t(100));
        testBox.put(uint8_t(0xFF));
        testBox.put(TempSensorOneWireBlock::staticTypeId());

        auto message = blox::TempSensorOneWire();
        message.set_address(12345678);
        message.set_offset(100);
        message.set_valid(true);
        message.set_value(123);

        testBox.put(message);

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        testBox.put(commands::READ_OBJECT);
        testBox.put(cbox::obj_id_t(100));

        auto decoded = blox::TempSensorOneWire();
        testBox.processInputToProto(decoded);

        THEN("The returned protobuf data is as expected")
        {
            CHECK(testBox.lastReplyHasStatusOk());

            CHECK(decoded.ShortDebugString() == "offset: 100 "
                                                "address: 12345678");
        }

        THEN("The writable settings match what was sent")
        {
            auto lookup = brewbloxBox().makeCboxPtr<TempSensorOneWireBlock>(100);
            auto sensorPtr = lookup.lock();
            REQUIRE(sensorPtr);
            CHECK(sensorPtr->get().getAddress() == 12345678);
            CHECK(sensorPtr->get().getCalibration() == cnl::wrap<temp_t>(100));

            AND_THEN("The values that are not writable are unchanged")
            {
                CHECK(sensorPtr->get().value() == temp_t(0));
                CHECK(sensorPtr->get().valid() == false);
            }
        }

        WHEN("An object discovery command is received")
        {
            testBox.put(commands::DISCOVER_NEW_OBJECTS);
            auto reply = testBox.processInput();
            THEN("3 new objects are discovered")
            {
                CHECK(reply == cbox::addCrc("0C") + "|0000" + "," + cbox::addCrc("6500") + "," + cbox::addCrc("6600") + "," + cbox::addCrc("6700") + "\n");
                AND_THEN("These objects can be used as temp sensor")
                {
                    auto s1 = brewbloxBox().makeCboxPtr<TempSensor>(101);
                    auto s2 = brewbloxBox().makeCboxPtr<TempSensor>(102);
                    auto s3 = brewbloxBox().makeCboxPtr<TempSensor>(103);

                    CHECK(s1.lock());
                    CHECK(s2.lock());
                    CHECK(s3.lock());
                }
            }

            AND_WHEN("The command is given for the second time")
            {
                testBox.put(commands::DISCOVER_NEW_OBJECTS);
                auto reply = testBox.processInput();
                THEN("No new objects are discovered")
                {
                    CHECK(reply == cbox::addCrc("0C") + "|0000\n");
                }
            }

            AND_WHEN("One of the sensors is removed")
            {
                testBox.put(commands::DELETE_OBJECT);
                testBox.put(cbox::obj_id_t(101));
                testBox.processInput();
                CHECK(testBox.lastReplyHasStatusOk());

                THEN("It will be discovered again")
                {
                    testBox.put(commands::DISCOVER_NEW_OBJECTS);
                    auto reply = testBox.processInput();
                    CHECK(reply == cbox::addCrc("0C") + "|0000" + "," + cbox::addCrc("6800") + "\n");
                }
            }
        }
    }
}