/*
 * Copyright 2018 BrewPi B.V.
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

#pragma once

#include "OneWire.h"
#include "OneWireAddress.h"
#include "OneWireDevice.h"
#include "blox/DS2408Block.h"
#include "blox/DS2413Block.h"
#include "blox/TempSensorOneWireBlock.h"
#include "cbox/Object.h"
#include "cbox/ObjectContainer.h"
#include "cbox/ScanningFactory.h"
#include <memory>

/**
 * Simple mock factory that emulates object discovery
 * Normally, a scanning factory will scan some type of communication bus
 * This factory just has a list of candidates. If a LongIntObject with that value doesn't exist, it creates it.
 */
class OneWireScanningFactory : public cbox::ScanningFactory {
private:
    OneWire& bus;

public:
    OneWireScanningFactory(cbox::ObjectContainer& objects, OneWire& ow)
        : cbox::ScanningFactory(objects)
        , bus(ow)
    {
        reset();
    }

    virtual ~OneWireScanningFactory() = default;

    virtual void reset() override
    {
        bus.reset_search();
    };

    virtual OneWireAddress next()
    {
        auto newAddr = OneWireAddress();
        if (bus.search(newAddr.asUint8ptr())) {
            return newAddr;
        }
        return 0;
    }

    virtual std::shared_ptr<cbox::Object> scan() override final
    {
        while (true) {
            if (auto newAddr = next()) {
                bool found = false;
                for (auto existing = objectsRef.cbegin(); existing != objectsRef.cend(); ++existing) {
                    OneWireDevice* ptrIfCorrectType = reinterpret_cast<OneWireDevice*>(existing->object()->implements(cbox::interfaceId<OneWireDevice>()));
                    if (ptrIfCorrectType == nullptr) {
                        continue; // not the right type, no match
                    }
                    if (ptrIfCorrectType->getDeviceAddress() == newAddr) {
                        found = true; // object with value already exists
                        break;
                    }
                }
                if (!found) {
                    // create new object
                    uint8_t familyCode = newAddr.asUint8ptr()[0];
                    switch (familyCode) {
                    case DS18B20MODEL: {
                        auto newSensor = std::make_shared<TempSensorOneWireBlock>();
                        newSensor->get().setDeviceAddress(newAddr);
                        return std::move(newSensor);
                    }
                    case DS2413_FAMILY_ID: {
                        auto newDevice = std::make_shared<DS2413Block>();
                        newDevice->get().setDeviceAddress(newAddr);
                        return std::move(newDevice);
                    }
                    case DS2408_FAMILY_ID: {
                        auto newDevice = std::make_shared<DS2408Block>();
                        newDevice->get().setDeviceAddress(newAddr);
                        return std::move(newDevice);
                    }
                    default:
                        break;
                    }
                }
            } else {
                break;
            }
        };
        return nullptr;
    };
};
