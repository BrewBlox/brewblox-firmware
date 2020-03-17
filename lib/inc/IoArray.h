/*
 * Copyright 2019 BrewPi/Elco Jacobs.
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

#pragma once

#include "ActuatorDigitalBase.h"
#include <inttypes.h>
#include <vector>

/*
 * Abstract interface to an array of digital inputs and/or outputs
 */
class IoArray {
public:
    using State = ActuatorDigitalBase::State;
    IoArray(uint8_t size)
        : channels(size, {ChannelConfig::UNUSED, State::Unknown})
    {
    }

    virtual ~IoArray() = default;

    enum class ChannelConfig {
        UNUSED = 0,
        ACTIVE_LOW = 1,
        ACTIVE_HIGH = 2,
        INPUT = 10,
        UNKNOWN = 255,
    };

    bool validChannel(uint8_t channel) const
    {
        return channel > 0 && channel <= size();
    }

    // returns the chached value for the pin state. The
    bool senseChannel(uint8_t channel, ActuatorDigitalBase::State& result) const
    {
        // first channel is 1, because 0 is used as 'unconfigured'
        if (validChannel(channel)) {
            if (senseChannelImpl(channel, channels[channel - 1].state)) {
                result = channels[channel - 1].state;
                return true;
            }
        }
        return false;
    }

    // returns cached value for channel config, which is assumed to be equal to the last write
    bool readChannelConfig(uint8_t channel, ChannelConfig& result) const
    {
        // first channel on external interface is 1, because 0 is unconfigured
        if (validChannel(channel)) {
            result = channels[channel - 1].config;
            return true;
        }
        return false;
    }
    bool writeChannelConfig(uint8_t channel, const ChannelConfig& config)
    {
        // first channel on external interface is 1, because 0 is unconfigured
        if (validChannel(channel)) {
            channels[channel - 1].config = config;
            writeChannelImpl(channel, config);
            return true;
        }
        return false;
    }

    bool claimChannel(uint8_t channel, const ChannelConfig& config)
    {
        ChannelConfig existingConfig;
        if (readChannelConfig(channel, existingConfig)) {
            if (existingConfig == ChannelConfig::UNUSED) {
                writeChannelConfig(channel, config);
                return true;
            }
        }
        return false;
    }

    bool releaseChannel(uint8_t channel)
    {
        if (!validChannel(channel) || writeChannelConfig(channel, ChannelConfig::UNUSED)) {
            return true;
        }

        return false;
    }

    const auto& readChannels() const
    {
        return channels;
    }

    uint8_t size() const
    {
        return channels.size();
    }

    // virtual functions to be implemented by super class that perform actual hardware IO.
    // most data and/or caching is stored in this class.
    // the super class can choose to apply/read immediately or to implement no-op functions and sync in an update function
    virtual bool supportsFastIo() const = 0;

protected:
    virtual bool senseChannelImpl(uint8_t channel, State& result) const = 0;
    virtual bool writeChannelImpl(uint8_t channel, const ChannelConfig& config) = 0;

    struct Channel {
        ChannelConfig config;
        State state;
    };

    mutable std::vector<Channel> channels;
};