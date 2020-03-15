/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of BrewBlox
 *
 * BrewBlox is free software: you can redistribute it and/or modify
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
 * along with BrewBlox.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Ticks.h"
#include "blox/Block.h"
#include "cbox/DataStream.h"
#include "proto/cpp/Ticks.pb.h"

// provides a protobuf interface to the ticks object
template <typename T>
class TicksBlock : public Block<BrewBloxTypes_BlockType_Ticks> {
    T& ticks;

public:
    TicksBlock(T& _ticks)
        : ticks(_ticks)
    {
    }
    virtual ~TicksBlock() = default;

    virtual cbox::CboxError streamFrom(cbox::DataIn& dataIn) override final
    {
        blox_Ticks newData = blox_Ticks_init_zero;
        cbox::CboxError result = streamProtoFrom(dataIn, &newData, blox_Ticks_fields, blox_Ticks_size);
        if (result == cbox::CboxError::OK) {
            ticks.setUtc(newData.secondsSinceEpoch);
        }
        return result;
    }

    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_Ticks message = blox_Ticks_init_zero;
        message.secondsSinceEpoch = ticks.utc();
        message.millisSinceBoot = ticks.millis();

        message.avgCommunicationTask = ticks.taskTime(0);
        message.avgBlocksUpdateTask = ticks.taskTime(1);
        message.avgDisplayTask = ticks.taskTime(2);
        message.avgSystemTask = ticks.taskTime(3);

        return streamProtoTo(out, &message, blox_Ticks_fields, blox_Ticks_size);
    }

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut&) const override final
    {
        return cbox::CboxError::OK;
    }

    virtual cbox::update_t update(const cbox::update_t& now) override final
    {
        return cbox::Object::update_never(now);
    }

    T& get()
    {
        return ticks;
    }

    const T& const_get() const
    {
        return ticks;
    }
};
