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

class OneWireDevice {
public:
    OneWireDevice(OneWire& oneWire_, const OneWireAddress& address_);

protected:
    ~OneWireDevice() = default;

public:
    OneWireAddress getDeviceAddress() const;
    void setDeviceAddress(const OneWireAddress& addr);
    bool validAddress() const;

    bool connected() const
    {
        return m_connected;
    }

    void selectROM() const
    {
        oneWire.reset();
        oneWire.select(address.asUint8ptr());
    }

protected:
    OneWire& oneWire;
    OneWireAddress address;

    mutable bool m_connected = false;
};
