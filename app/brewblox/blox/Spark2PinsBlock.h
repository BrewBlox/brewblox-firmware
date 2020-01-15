/*
 * Copyright 2019 BrewPi B.V.
 *
 * This file is part of BrewPi.
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

#include "blox/Block.h"
#include "blox/SparkIoBase.h"
#include "proto/cpp/Spark2Pins.pb.h"

class Spark2PinsBlock : public SparkIoBase, public Block<BrewBloxTypes_BlockType_Spark2Pins> {
private:
    static const uint8_t numPins = 4;
    virtual pin_t channelToPin(uint8_t channel) const override final
    {
        auto pins = std::array<pin_t, numPins>{
            PIN_ACTUATOR1,
            PIN_ACTUATOR2,
            PIN_ACTUATOR3,
            PIN_ACTUATOR0,
        };

        if (validChannel(channel)) {
            return pins[channel - 1];
        }
        return -1;
    }

public:
    Spark2PinsBlock()
        : SparkIoBase(getSparkVersion() == SparkVersion::V2 ? numPins : numPins - 1)
    {
    }

    virtual cbox::CboxError streamFrom(cbox::DataIn& in) override final
    {
        blox_Spark2Pins message = blox_Spark2Pins_init_zero;
        cbox::CboxError result = streamProtoFrom(in, &message, blox_Spark2Pins_fields, blox_Spark2Pins_size);

        if (result == cbox::CboxError::OK) {
            // io pins are not writable through this block. They are configured by creating Digital Actuators
            digitalWriteFast(PIN_ALARM, message.soundAlarm);
        }
        return result;
    }

    virtual cbox::CboxError
    streamTo(cbox::DataOut& out) const override final
    {
        blox_Spark2Pins message = blox_Spark2Pins_init_zero;

        message.pins[0].which_Pin = blox_Spark2Pins_IoPin_bottom1_tag;
        readIo(*this, 1, message.pins[0].Pin.bottom1);
        message.pins[1].which_Pin = blox_Spark2Pins_IoPin_bottom2_tag;
        readIo(*this, 2, message.pins[1].Pin.bottom2);
        message.pins[2].which_Pin = blox_Spark2Pins_IoPin_bottom3_tag;
        readIo(*this, 3, message.pins[2].Pin.bottom3);

        if (getSparkVersion() != SparkVersion::V1) {
            message.pins[3].which_Pin = blox_Spark2Pins_IoPin_bottom0_tag;
            readIoConfig(*this, 4, message.pins[3].Pin.bottom0.config);
            message.pins_count = numPins;
        } else {
            message.pins_count = numPins - 1;
        }

        message.soundAlarm = HAL_GPIO_Read(PIN_ALARM);

        auto hw = blox_Spark2Pins_Hardware::blox_Spark2Pins_Hardware_unknown_hw;
        switch (getSparkVersion()) {
        case SparkVersion::V1:
            hw = blox_Spark2Pins_Hardware_Spark1;
            break;
        case SparkVersion::V2:
            hw = blox_Spark2Pins_Hardware_Spark2;
            break;
        }
        message.hardware = hw;

        return streamProtoTo(out, &message, blox_Spark2Pins_fields, blox_Spark2Pins_size);
    }

    virtual cbox::CboxError
    streamPersistedTo(cbox::DataOut& out) const override final
    {
        blox_Spark2Pins message = blox_Spark2Pins_init_zero;

        message.soundAlarm = HAL_GPIO_Read(PIN_ALARM);

        return streamProtoTo(out, &message, blox_Spark2Pins_fields, blox_Spark2Pins_size);
    }

    virtual void*
    implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == BrewBloxTypes_BlockType_Spark2Pins) {
            return this; // me!
        }
        if (iface == cbox::interfaceId<IoArray>()) {
            // return the member that implements the interface in this case
            IoArray* ptr = this;
            return ptr;
        }
        return nullptr;
    }

    virtual cbox::update_t
    update(const cbox::update_t& now) override final
    {
        return update_never(now);
    }
};
