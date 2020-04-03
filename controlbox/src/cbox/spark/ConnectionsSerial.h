/*
 * Copyright 2018 BrewBlox / Elco Jacobs
 *
 * This file is part of Controlbox.
 *
 * Controlbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Controlbox.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "../Connections.h"
#include "spark_wiring_usbserial.h"

namespace cbox {

static bool SerialInUse = false;
class SerialConnection : public StreamRefConnection<USBSerial> {
public:
    SerialConnection()
        : StreamRefConnection(Serial)
    {
        SerialInUse = true;
    }
    virtual ~SerialConnection()
    {
        stop();
    };

    virtual void stop() override final
    {
        SerialInUse = false;
    }
};

class SerialConnectionSource : public ConnectionSource {
public:
    SerialConnectionSource()
    {
        Serial.begin(115200);
    }

    std::unique_ptr<Connection> newConnection() override final
    {
        if (Serial.isConnected() && !SerialInUse) {
            return std::make_unique<SerialConnection>();
        }
        return nullptr;
    }

    virtual void stop() override final
    {
        Serial.flush(); // only flush, leave port open
    }
};

} // end namespace cbox
