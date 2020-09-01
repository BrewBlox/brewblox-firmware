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
#include <cstdio>
#include <iomanip>
#include <iostream>

#include "../BrewBlox.h"
#include "BrewBloxTestBox.h"
#include "Temperature.h"
#include "blox/TempSensorCombiBlock.h"
#include "blox/TempSensorMockBlock.h"
#include "cbox/Box.h"
#include "cbox/DataStream.h"
#include "cbox/DataStreamIo.h"
#include "cbox/Object.h"
#include "proto/test/cpp/TempSensorCombi_test.pb.h"
#include "proto/test/cpp/TempSensorMock_test.pb.h"
#include "testHelpers.h"

SCENARIO("A TempSensorCombi block")
{
    BrewBloxTestBox testBox;
    using commands = cbox::Box::CommandID;

    testBox.reset();

    // create mock sensor 1
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(101));
    testBox.put(uint8_t(0xFF));
    testBox.put(TempSensorMockBlock::staticTypeId());

    auto mockSensor1 = blox::TempSensorMock();
    mockSensor1.set_setting(cnl::unwrap(temp_t(21.0)));
    mockSensor1.set_connected(true);

    testBox.put(mockSensor1);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create mock sensor 2
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(102));
    testBox.put(uint8_t(0xFF));
    testBox.put(TempSensorMockBlock::staticTypeId());

    auto mockSensor2 = blox::TempSensorMock();
    mockSensor2.set_setting(cnl::unwrap(temp_t(22.0)));
    mockSensor2.set_connected(true);

    testBox.put(mockSensor2);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create mock sensor 3
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(103));
    testBox.put(uint8_t(0xFF));
    testBox.put(TempSensorMockBlock::staticTypeId());

    auto mockSensor3 = blox::TempSensorMock();
    mockSensor3.set_setting(cnl::unwrap(temp_t(23.0)));
    mockSensor3.set_connected(true);

    testBox.put(mockSensor3);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    // create combi sensor
    testBox.put(uint16_t(0)); // msg id
    testBox.put(commands::CREATE_OBJECT);
    testBox.put(cbox::obj_id_t(100));
    testBox.put(uint8_t(0xFF));
    testBox.put(TempSensorCombiBlock::staticTypeId());

    auto TempSensorCombi = blox::TempSensorCombi();
    TempSensorCombi.add_sensors(101);
    TempSensorCombi.add_sensors(102);
    TempSensorCombi.add_sensors(103);

    testBox.put(TempSensorCombi);

    testBox.processInput();
    CHECK(testBox.lastReplyHasStatusOk());

    testBox.update(0);

    WHEN("func is set to AVG")
    {
        {
            // AVG is default
            testBox.update(1000);

            THEN("The value of the sensor is the average of the 3")
            {
                // read pair
                testBox.put(uint16_t(0)); // msg id
                testBox.put(commands::READ_OBJECT);
                testBox.put(cbox::obj_id_t(100));

                auto decoded = blox::TempSensorCombi();
                testBox.processInputToProto(decoded);

                CHECK(testBox.lastReplyHasStatusOk());
                CHECK(decoded.ShortDebugString() == "value: 90112 sensors: 101 sensors: 102 sensors: 103");
            }
        }
    }

    WHEN("func is set to MAX")
    {
        {
            testBox.put(uint16_t(0)); // msg id
            testBox.put(commands::WRITE_OBJECT);
            testBox.put(cbox::obj_id_t(100));
            testBox.put(uint8_t(0xFF));
            testBox.put(TempSensorCombiBlock::staticTypeId());

            TempSensorCombi.set_combinefunc(blox::SensorCombiFunc::SENSOR_COMBI_FUNC_MAX);
            testBox.put(TempSensorCombi);

            testBox.processInput();
            CHECK(testBox.lastReplyHasStatusOk());

            testBox.update(1000);

            THEN("The value of the sensor is the highest of the 3")
            {
                // read pair
                testBox.put(uint16_t(0)); // msg id
                testBox.put(commands::READ_OBJECT);
                testBox.put(cbox::obj_id_t(100));

                auto decoded = blox::TempSensorCombi();
                testBox.processInputToProto(decoded);

                CHECK(testBox.lastReplyHasStatusOk());
                CHECK(decoded.ShortDebugString() == "value: 94208 combinefunc: SENSOR_COMBI_FUNC_MAX sensors: 101 sensors: 102 sensors: 103");
            }
        }
    }

    WHEN("func is set to MIN")
    {
        {
            testBox.put(uint16_t(0)); // msg id
            testBox.put(commands::WRITE_OBJECT);
            testBox.put(cbox::obj_id_t(100));
            testBox.put(uint8_t(0xFF));
            testBox.put(TempSensorCombiBlock::staticTypeId());

            TempSensorCombi.set_combinefunc(blox::SensorCombiFunc::SENSOR_COMBI_FUNC_MIN);
            testBox.put(TempSensorCombi);

            testBox.processInput();
            CHECK(testBox.lastReplyHasStatusOk());

            testBox.update(1000);

            THEN("The value of the sensor is the lowest of the 3")
            {
                // read pair
                testBox.put(uint16_t(0)); // msg id
                testBox.put(commands::READ_OBJECT);
                testBox.put(cbox::obj_id_t(100));

                auto decoded = blox::TempSensorCombi();
                testBox.processInputToProto(decoded);

                CHECK(testBox.lastReplyHasStatusOk());
                CHECK(decoded.ShortDebugString() == "value: 86016 combinefunc: SENSOR_COMBI_FUNC_MIN sensors: 101 sensors: 102 sensors: 103");
            }
        }
    }

    WHEN("All 3 sensors are disconnected, the value is a stripped field ")
    {

        // create mock sensor 1
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t(101));
        testBox.put(uint8_t(0xFF));
        testBox.put(TempSensorMockBlock::staticTypeId());

        auto mockSensor1 = blox::TempSensorMock();
        mockSensor1.set_setting(cnl::unwrap(temp_t(21.0)));
        mockSensor1.set_connected(false);

        testBox.put(mockSensor1);

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        // create mock sensor 2
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t(102));
        testBox.put(uint8_t(0xFF));
        testBox.put(TempSensorMockBlock::staticTypeId());

        auto mockSensor2 = blox::TempSensorMock();
        mockSensor2.set_setting(cnl::unwrap(temp_t(22.0)));
        mockSensor2.set_connected(false);

        testBox.put(mockSensor2);

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        // create mock sensor 3
        testBox.put(uint16_t(0)); // msg id
        testBox.put(commands::WRITE_OBJECT);
        testBox.put(cbox::obj_id_t(103));
        testBox.put(uint8_t(0xFF));
        testBox.put(TempSensorMockBlock::staticTypeId());

        auto mockSensor3 = blox::TempSensorMock();
        mockSensor3.set_setting(cnl::unwrap(temp_t(23.0)));
        mockSensor3.set_connected(false);

        testBox.put(mockSensor3);

        testBox.processInput();
        CHECK(testBox.lastReplyHasStatusOk());

        testBox.update(1000);

        THEN("The value of the sensor is the average of the 3")
        {
            // read pair
            testBox.put(uint16_t(0)); // msg id
            testBox.put(commands::READ_OBJECT);
            testBox.put(cbox::obj_id_t(100));

            auto decoded = blox::TempSensorCombi();
            testBox.processInputToProto(decoded);

            CHECK(testBox.lastReplyHasStatusOk());
            CHECK(decoded.ShortDebugString() == "sensors: 101 sensors: 102 sensors: 103 strippedFields: 1");
        }
    }
}