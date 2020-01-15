/*
 * Copyright 2013 BrewPi/Elco Jacobs.
 * Copyright 2013 Matthew McGowan
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

#include "ActuatorDigitalBase.h"
#include "IoArray.h"
#include <functional>
#include <memory>

/*
 * A digital actuator that toggles a channel of an ArrayIo object.
 *
 */
class ActuatorDigital : public ActuatorDigitalBase {
private:
    const std::function<std::shared_ptr<IoArray>()> m_target;
    bool m_invert = false;
    uint8_t m_channel = 0;
    uint8_t m_desiredChannel = 0;

public:
    explicit ActuatorDigital(std::function<std::shared_ptr<IoArray>()>&& target, uint8_t chan)
        : m_target(target)
    {
        channel(chan);
    }
    ~ActuatorDigital()
    {
        channel(0); // release channel before destruction
    }

    virtual void state(const State& v) override final
    {
        if (channelReady()) {
            if (auto devPtr = m_target()) {
                auto newState = v;
                if (m_invert) {
                    newState = invertState(v);
                }
                IoArray::ChannelConfig config = newState == State::Active ? IoArray::ChannelConfig::ACTIVE_HIGH : IoArray::ChannelConfig::ACTIVE_LOW;
                devPtr->writeChannelConfig(m_channel, config);
            }
        }
    }

    virtual State state() const override final
    {
        if (channelReady()) {
            if (auto devPtr = m_target()) {
                State result;
                if (devPtr->senseChannel(m_channel, result)) {
                    if (m_invert) {
                        result = invertState(result);
                    }
                    return result;
                }
            }
        }

        return State::Unknown;
    }

    bool invert() const
    {
        return m_invert;
    }

    void invert(bool inv)
    {
        auto active = state();
        m_invert = inv;
        state(active);
    }

    uint8_t channel() const
    {
        return m_desiredChannel;
    }

    void claimChannel()
    {
        if (auto devPtr = m_target()) {
            if (m_channel != 0) {
                if (!devPtr->releaseChannel(m_channel)) {
                    return;
                }
            }

            if (m_desiredChannel == 0) {
                m_channel = 0;
                return;
            }
            if (devPtr->claimChannel(m_desiredChannel, IoArray::ChannelConfig::ACTIVE_LOW)) {
                m_channel = m_desiredChannel;
            }
        }
    }

    bool channelReady() const
    {
        return m_desiredChannel == m_channel;
    }

    void update()
    {
        if (!channelReady()) {
            // Periodic retry to claim channel in case target didn't exist
            // at earlier tries
            claimChannel();
        }
    }

    void channel(uint8_t newChannel)
    {
        m_desiredChannel = newChannel;
        update();
    }

    virtual bool supportsFastIo() const override final
    {
        if (auto devPtr = m_target()) {
            return devPtr->supportsFastIo();
        }
        return false;
    }
};
