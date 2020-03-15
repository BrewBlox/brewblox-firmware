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

#include "IoArray.h"

class MockIoArray : public IoArray {
public:
    uint8_t pinStates = 0;  // 1 = high, 0 is low
    uint8_t pinModes = 0;   // 1 = ouput, 0 is input
    uint8_t errorState = 0; // error for specific channel
    bool isConnected = true;

    MockIoArray()
        : IoArray(8)
    {
    }

    ~MockIoArray() = default;

    virtual bool senseChannelImpl(uint8_t channel, State& result) const override final
    {
        // TODO
        if (isConnected && validChannel(channel) && ((getMask(channel) & errorState) == 0)) {
            result = (pinStates & getMask(channel)) != 0 ? State::Active : State::Inactive;
            return true;
        }
        result = State::Unknown;
        return false;
    }

    virtual bool writeChannelImpl(uint8_t channel, const ChannelConfig& config) override final
    {
        if (isConnected && validChannel(channel)) {
            uint8_t mask = getMask(channel);
            switch (config) {
            case ChannelConfig::ACTIVE_HIGH:
                pinStates |= mask;
                pinModes |= mask;
                return true;
            case ChannelConfig::ACTIVE_LOW:
                pinStates &= ~mask;
                pinModes |= mask;
                return true;
            case ChannelConfig::INPUT:
                pinModes &= mask;
                return true;
            case ChannelConfig::UNUSED:
            case ChannelConfig::UNKNOWN:
                return false;
            }
        }
        return false;
    }

    void connected(bool v)
    {
        isConnected = v;
    }

    void setChannelError(uint8_t chan, bool isError)
    {
        uint8_t mask = getMask(chan);
        if (isError) {
            errorState |= mask;
        } else {
            errorState &= mask;
        }
    }

    virtual bool supportsFastIo() const override final
    {
        return true;
    }

private:
    uint8_t static getMask(uint8_t channel)
    {
        return 0x1 << (channel - 1);
    }
};
