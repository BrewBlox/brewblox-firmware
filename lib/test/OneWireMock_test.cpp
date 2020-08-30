/*
 * Copyright 2020 BrewPi B.V.
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

#include "../inc/OneWireMockDevice.h"
#include "../inc/OneWireMockDriver.h"
#include "DS18B20.h"
#include "DS18B20Mock.h"
#include "DS2408.h"
#include "DS2408Mock.h"
#include "DS2413.h"
#include "DS2413Mock.h"
#include "MotorValve.h"

namespace Catch {
template <>
struct StringMaker<OneWireAddress> {
    static std::string convert(OneWireAddress const& value)
    {
        return value.toString();
    }
};
}

OneWireAddress
makeValidAddress(OneWireAddress addr)
{
    addr[7] = OneWireCrc8(&addr[0], 7);
    return addr;
}

SCENARIO("A mocked OneWire bus and mocked slaves", "[onewire]")
{
    OneWireMockDriver owMock;
    OneWire ow(owMock);

    WHEN("No devices are on the bus, reset returns false")
    {
        CHECK(ow.reset() == false);
    }

    WHEN("No devices are on the bus, search returns false")
    {
        OneWireAddress addr(0);
        ow.reset_search();
        CHECK(ow.search(addr) == false);
    }

    WHEN("A mock DS18B20 is attached")
    {
        auto addr1 = makeValidAddress(0x0011223344556628);
        auto mockSensor = std::make_shared<DS18B20Mock>(addr1);
        owMock.attach(mockSensor);
        THEN("Reset returns a presence")
        {
            CHECK(ow.reset() == true);
        }

        THEN("It can be found with a bus search")
        {
            OneWireAddress addr(0);
            ow.reset();
            bool found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addr1);
        }

        THEN("A read without issueing a command returns 0xFF")
        {
            ow.reset();
            uint8_t v = 0;
            ow.read(v);
            CHECK(v == 0xFF);
        }

        THEN("An invalid command is ignored")
        {
            ow.reset();
            ow.write(0xFE);
            uint8_t v = 0;
            ow.read(v);
            CHECK(v == 0xFF);
        }

        THEN("With a single device on the bus, a read ROM command can return its address")
        {
            OneWireAddress addr(0);
            ow.reset();
            ow.write(0x33);
            ow.read_bytes(&addr[0], 8);
            CHECK(addr == addr1);
        }

        THEN("With a single device on the bus, a skip ROM command can select it without knowing the address")
        {
            OneWireAddress addr(0);
            ow.reset();
            ow.skip();      // skip selecting by address
            ow.write(0xB4); // read power supply
            uint8_t v = 0;
            ow.read(v);
            CHECK(v == 0x80);
        }

        THEN("A OneWire sensor can use it on the fake bus")
        {
            DS18B20 sensor(ow, addr1);
            sensor.update();
            CHECK(sensor.valid() == false); // a reset will be detected, triggering a re-init
            sensor.update();
            CHECK(sensor.valid() == true);
            CHECK(sensor.value() == 20.0);

            mockSensor->setTemperature(temp_t{21.0});
            CHECK(mockSensor->getTemperature() == 21.0);
            sensor.update();
            CHECK(sensor.value() == 21.0);

            mockSensor->setTemperature(temp_t{-10.0});
            CHECK(mockSensor->getTemperature() == -10.0);
            sensor.update();
            CHECK(sensor.value() == -10.0);
        }

        WHEN("The sensor is disconnected")
        {
            DS18B20 sensor(ow, addr1);
            mockSensor->setConnected(false);
            sensor.update();
            sensor.update();

            THEN("It reads as invalid")
            {
                CHECK(sensor.valid() == false);
            }

            THEN("When it comes back, the first value is invalid and the second is valid (reset detection)")
            {
                mockSensor->setConnected(true);
                sensor.update();
                CHECK(sensor.valid() == false);
                sensor.update();
                CHECK(sensor.valid() == true);
            }
        }

        WHEN("Communication bitflips when reading the sensor occur")
        {
            DS18B20 sensor(ow, addr1);
            mockSensor->setTemperature(temp_t{21.0});
            sensor.update();
            sensor.update();

            THEN("A single bitflip will not give an error due to a retry")
            {
                // 9 scratchpad bytes are read, 81 bits
                mockSensor->flipReadBits({13});
                sensor.update();
                CHECK(sensor.valid() == true);
                CHECK(sensor.value() == 21.0);
            }

            THEN("A bitflip in 2 scratchpads will give an error")
            {
                mockSensor->flipReadBits({13, 81 + 13});
                sensor.update();
                CHECK(sensor.valid() == false);
                CHECK(sensor.value() == 0.0);
            }
        }

        AND_WHEN("Another sensor is connected")
        {

            auto addr2 = makeValidAddress(0x0099223344556628);
            auto mockSensor2 = std::make_shared<DS18B20Mock>(addr2);
            owMock.attach(mockSensor2);

            DS18B20 sensor1(ow, addr1);
            DS18B20 sensor2(ow, addr2);

            THEN("A bus search finds two sensors")
            {
                OneWireAddress addr(0);
                ow.reset();
                ow.reset_search();
                bool found = ow.search(addr);
                CHECK(found == true);
                CHECK(addr == OneWireAddress(addr1));

                found = ow.search(addr);
                CHECK(found == true);
                CHECK(addr == OneWireAddress(addr2));

                found = ow.search(addr);
                CHECK(found == false);
            }

            sensor1.update();
            sensor2.update();
            sensor1.update();
            sensor2.update();
            CHECK(sensor1.valid() == true);
            CHECK(sensor2.valid() == true);
            CHECK(sensor1.value() == 20.0);
            CHECK(sensor2.value() == 20.0);

            mockSensor->setTemperature(temp_t{21.0});
            mockSensor2->setTemperature(temp_t{22.0});
            CHECK(mockSensor->getTemperature() == 21.0);
            CHECK(mockSensor2->getTemperature() == 22.0);
            sensor1.update();
            sensor2.update();
            CHECK(sensor1.value() == 21.0);
            CHECK(sensor2.value() == 22.0);
        }
    }

    WHEN("A mock DS2413 is attached")
    {
        auto addr3 = makeValidAddress(0x002222334455663A);
        auto ds1mock = std::make_shared<DS2413Mock>(addr3);
        owMock.attach(ds1mock);
        THEN("Reset returns a presence")
        {
            CHECK(ow.reset() == true);
        }

        THEN("It can be found with a bus search")
        {
            OneWireAddress addr(0);
            ow.reset();
            bool found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addr3);
        }

        THEN("A DS2413 class can use it as output")
        {
            DS2413 ds1(ow, addr3);

            ActuatorDigitalBase::State result;
            ds1.update();
            CHECK(ds1.connected() == true);
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);
            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            // note that for IoArray ACTIVE_HIGH means OUTPUT and ACTIVE
            // It is actually an open drain pull down on in the DS2413
            // Should we rename this?
            CHECK(ds1.writeChannelConfig(1, IoArray::ChannelConfig::ACTIVE_HIGH));
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Active);

            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            CHECK(ds1.writeChannelConfig(1, IoArray::ChannelConfig::ACTIVE_LOW));
            CHECK(ds1.writeChannelConfig(2, IoArray::ChannelConfig::ACTIVE_HIGH));
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Active);
        }

        THEN("A DS2413 class can use it as input")
        {
            DS2413 ds1(ow, addr3);
            ds1.update();

            ActuatorDigitalBase::State result;
            CHECK(ds1.writeChannelConfig(1, IoArray::ChannelConfig::INPUT));
            CHECK(ds1.writeChannelConfig(2, IoArray::ChannelConfig::INPUT));
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            ds1mock->setExternalPullDownA(true);

            ds1.update();
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Active);

            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            ds1mock->setExternalPullDownA(false);
            ds1mock->setExternalPullDownB(true);

            ds1.update();
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Active);
        }
    }

    WHEN("A mock DS2408 is attached")
    {
        auto addr4 = makeValidAddress(0x0022223344556629);
        auto ds2408mock = std::make_shared<DS2408Mock>(addr4);
        owMock.attach(ds2408mock);
        THEN("Reset returns a presence")
        {
            CHECK(ow.reset() == true);
        }

        THEN("It can be found with a bus search")
        {
            OneWireAddress addr(0);
            ow.reset();
            bool found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addr4);
        }

        THEN("A DS2408 class can use it as output driver")
        {
            DS2408 ds1(ow, addr4);

            ActuatorDigitalBase::State result;
            ds1.update();
            CHECK(ds1.connected() == true);
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);
            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);

            for (uint8_t chan = 1; chan <= 8; chan++) {
                // set one channel as active, others as inactive
                for (uint8_t i = 1; i <= 8; i++) {
                    auto config = i == chan ? IoArray::ChannelConfig::ACTIVE_HIGH : IoArray::ChannelConfig::ACTIVE_LOW;
                    CHECK(ds1.writeChannelConfig(i, config));
                }
                ds1.update();

                for (uint8_t i = 1; i <= 8; i++) {
                    CHECK(ds1.senseChannel(i, result));
                    if (i == chan) {
                        CHECK(result == ActuatorDigitalBase::State::Active);
                    } else {
                        CHECK(result == ActuatorDigitalBase::State::Inactive);
                    }
                }
            }
        }

        THEN("A DS2408 class can use it as input on some pins and output on others")
        {
            DS2408 ds1(ow, addr4);

            ActuatorDigitalBase::State result;
            ds1.update();
            CHECK(ds1.connected() == true);
            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);
            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive);
            // channels start at 1
            CHECK(ds1.writeChannelConfig(1, IoArray::ChannelConfig::INPUT));
            CHECK(ds1.writeChannelConfig(2, IoArray::ChannelConfig::ACTIVE_HIGH));
            CHECK(ds1.writeChannelConfig(3, IoArray::ChannelConfig::ACTIVE_LOW));
            CHECK(ds1.writeChannelConfig(4, IoArray::ChannelConfig::INPUT));
            CHECK(ds1.writeChannelConfig(5, IoArray::ChannelConfig::INPUT));
            CHECK(ds1.writeChannelConfig(6, IoArray::ChannelConfig::ACTIVE_LOW));
            CHECK(ds1.writeChannelConfig(7, IoArray::ChannelConfig::ACTIVE_HIGH));
            CHECK(ds1.writeChannelConfig(8, IoArray::ChannelConfig::INPUT));

            // bit index starts at 0
            ds2408mock->setExternalPullDown(0, false);
            ds2408mock->setExternalPullDown(3, true);

            ds2408mock->setExternalPullDown(4, true);
            ds2408mock->setExternalPullDown(7, false);

            ds1.update();

            CHECK(ds1.senseChannel(1, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive); // no external pulldown
            CHECK(ds1.senseChannel(2, result));
            CHECK(result == ActuatorDigitalBase::State::Active); // internal latch enabled
            CHECK(ds1.senseChannel(3, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive); // internal latch disabled
            CHECK(ds1.senseChannel(4, result));
            CHECK(result == ActuatorDigitalBase::State::Active); // external pulldown
            CHECK(ds1.senseChannel(5, result));
            CHECK(result == ActuatorDigitalBase::State::Active); // external pulldown
            CHECK(ds1.senseChannel(6, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive); // internal latch disabled
            CHECK(ds1.senseChannel(7, result));
            CHECK(result == ActuatorDigitalBase::State::Active); // internal latch enabled
            CHECK(ds1.senseChannel(8, result));
            CHECK(result == ActuatorDigitalBase::State::Inactive); // no external pulldown
        }
    }

    WHEN("3x DS18B20, DS2413, and DS2408 are connected")
    {
        auto addrSensor1 = OneWireAddress(0x7E11'1111'1111'1128);
        auto addrSensor2 = OneWireAddress(0xDE22'2222'2222'2228);
        auto addrSensor3 = OneWireAddress(0xBE33'3333'3333'3328);
        auto addrDS2413 = OneWireAddress(0x0644'4444'4444'443A);
        auto addrDS2408 = OneWireAddress(0xDA55'5555'5555'5529);
        owMock.attach(std::make_shared<DS18B20Mock>(addrSensor1)); // DS18B20
        owMock.attach(std::make_shared<DS18B20Mock>(addrSensor2)); // DS18B20
        owMock.attach(std::make_shared<DS18B20Mock>(addrSensor3)); // DS18B20
        owMock.attach(std::make_shared<DS2413Mock>(addrDS2413));   // DS2413
        owMock.attach(std::make_shared<DS2408Mock>(addrDS2408));   // DS2408

        THEN("A bus search finds 5 devices")
        {
            ow.reset_search();

            OneWireAddress addr(0);
            bool found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrSensor2);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrSensor1);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrSensor3);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrDS2413);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrDS2408);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == false);
            CHECK(addr == OneWireAddress(0));
        }

        THEN("Then target search can select only one family code to be found")
        {
            ow.target_search(DS18B20Mock::family_code);

            OneWireAddress addr(0);
            bool found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrSensor2);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrSensor1);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrSensor3);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == false);
            CHECK(addr == OneWireAddress(0));

            ow.target_search(DS2413Mock::family_code);
            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrDS2413);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == false);
            CHECK(addr == OneWireAddress(0));

            ow.target_search(DS2408Mock::family_code);
            addr = 0;
            found = ow.search(addr);
            CHECK(found == true);
            CHECK(addr == addrDS2408);

            addr = 0;
            found = ow.search(addr);
            CHECK(found == false);
            CHECK(addr == OneWireAddress(0));
        }
    }
}
