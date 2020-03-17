/*
 * Copyright 2019 BrewPi B.V.
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
#include "DS2408.h"
#include <functional>
#include <memory>

/*
 * A digital actuator that toggles a channel of an ArrayIo object.
 *
 */
class MotorValve : public ActuatorDigitalBase {

public:
    enum class ValveState : uint8_t {
        Unknown = 0,
        Open = 1,
        Closed = 2,
        Opening = 3,
        Closing = 4,
        HalfOpenIdle = 5,
        InitIdle = 6,
    };

    static const uint8_t chanIsClosed = 0;
    static const uint8_t chanIsOpen = 1;
    static const uint8_t chanOpeningHigh = 2;
    static const uint8_t chanClosingHigh = 3;

private:
    const std::function<std::shared_ptr<DS2408>()> m_target;
    uint8_t m_startChannel = 0;
    uint8_t m_desiredChannel = 0;

    ValveState m_desiredValveState = ValveState::InitIdle;
    ValveState m_actualValveState = ValveState::InitIdle;

public:
    explicit MotorValve(std::function<std::shared_ptr<DS2408>()>&& target, uint8_t startChan)
        : m_target(target)
    {
        startChannel(startChan);
    }
    ~MotorValve()
    {
        startChannel(0); // release channels before destruction
    }

    virtual void state(const State& v) override final
    {
        auto oldState = m_desiredValveState;
        if (v == State::Active) {
            m_desiredValveState = ValveState::Opening;
        } else {
            m_desiredValveState = ValveState::Closing;
        }
        if (oldState != m_desiredValveState) {
            update();
        }
    }

    virtual State state() const override final
    {
        if (m_actualValveState == ValveState::Opening || m_actualValveState == ValveState::Open) {
            return State::Active;
        }
        if (m_actualValveState == ValveState::Closing || m_actualValveState == ValveState::Closed) {
            return State::Inactive;
        }

        return State::Unknown;
    }

    ValveState valveState() const
    {
        return m_actualValveState;
    }

    void applyValveState(ValveState v, std::shared_ptr<DS2408>& devPtr)
    {
        if (m_startChannel == 0) {
            return;
        }
        // ACTIVE HIGH means latch pull down enabled, so the input to the H-bridge is inverted.
        // We keep the motor enabled just in case. The valve itself has an internal shutoff.
        if (v == ValveState::Opening || v == ValveState::Open) {
            devPtr->writeChannelConfig(m_startChannel + chanOpeningHigh, DS2408::ChannelConfig::ACTIVE_LOW);
            devPtr->writeChannelConfig(m_startChannel + chanClosingHigh, DS2408::ChannelConfig::ACTIVE_HIGH);
        } else if (v == ValveState::Closing || v == ValveState::Closed) {
            devPtr->writeChannelConfig(m_startChannel + chanOpeningHigh, DS2408::ChannelConfig::ACTIVE_HIGH);
            devPtr->writeChannelConfig(m_startChannel + chanClosingHigh, DS2408::ChannelConfig::ACTIVE_LOW);
        } else {
            devPtr->writeChannelConfig(m_startChannel + chanOpeningHigh, DS2408::ChannelConfig::ACTIVE_HIGH);
            devPtr->writeChannelConfig(m_startChannel + chanClosingHigh, DS2408::ChannelConfig::ACTIVE_HIGH);
        }
    }

    ValveState getValveState(const std::shared_ptr<DS2408>& devPtr) const
    {
        if (m_startChannel == 0) {
            return ValveState::Unknown;
        }

        State isClosedPin = State::Unknown;
        devPtr->senseChannel(m_startChannel + chanIsClosed, isClosedPin);
        State isOpenPin = State::Unknown;
        devPtr->senseChannel(m_startChannel + chanIsOpen, isOpenPin);

        using Config = DS2408::ChannelConfig;
        Config openChan = Config::UNKNOWN;
        devPtr->readChannelConfig(m_startChannel + chanOpeningHigh, openChan);
        Config closeChan = Config::UNKNOWN;
        devPtr->readChannelConfig(m_startChannel + chanClosingHigh, closeChan);

        ValveState vs = ValveState::Unknown;

        // Note: signal to H-bridge is inverted because active high enables a pull down latch
        if (openChan == Config::ACTIVE_LOW && closeChan == Config::ACTIVE_HIGH) {
            vs = ValveState::Opening;
        } else if (openChan == Config::ACTIVE_HIGH && closeChan == Config::ACTIVE_LOW) {
            vs = ValveState::Closing;
        } else if (openChan == Config::ACTIVE_HIGH && closeChan == Config::ACTIVE_HIGH) {
            vs = ValveState::HalfOpenIdle;
        } else {
            return ValveState::InitIdle; // return immediately to get out of init state
        }

        if (isOpenPin == State::Active) {
            if (vs != ValveState::Closing) {
                vs = ValveState::Open;
            }
        }

        if (isClosedPin == State::Active) {
            if (vs != ValveState::Opening) {
                vs = ValveState::Closed;
            }
        }

        return vs;
    }

    void update()
    {
        if (!channelReady()) {
            // Periodic retry to claim channel in case target didn't exist
            // at earlier tries
            claimChannel();
            return;
        }
        if (auto devPtr = m_target()) {
            m_actualValveState = getValveState(devPtr);

            if (m_desiredValveState == ValveState::Closing && m_actualValveState == ValveState::Closed) {
                return; // leave motor driven
            }
            if (m_desiredValveState == ValveState::Opening && m_actualValveState == ValveState::Open) {
                return; // leave motor driven
            }

            if (m_actualValveState != m_desiredValveState) {
                applyValveState(m_desiredValveState, devPtr);
                m_actualValveState = getValveState(devPtr);
            }
        }
    }

    uint8_t startChannel() const
    {
        return m_desiredChannel;
    }

    bool channelReady() const
    {
        return m_desiredChannel == m_startChannel;
    }

    void startChannel(uint8_t newChannel)
    {
        m_desiredChannel = newChannel;
        update();
    }

    void claimChannel()
    {
        if (auto devPtr = m_target()) {
            if (m_startChannel != 0) {
                for (uint8_t i = 0; i < 4; ++i) {
                    devPtr->releaseChannel(m_startChannel + i);
                }
                m_startChannel = 0;
            }
            if (m_desiredChannel == 1 || m_desiredChannel == 5) { // only 2 valid options
                bool success = devPtr->claimChannel(m_desiredChannel + chanIsClosed, IoArray::ChannelConfig::INPUT);
                success = devPtr->claimChannel(m_desiredChannel + chanIsOpen, IoArray::ChannelConfig::INPUT) && success;
                success = devPtr->claimChannel(m_desiredChannel + chanOpeningHigh, IoArray::ChannelConfig::ACTIVE_HIGH) && success;
                success = devPtr->claimChannel(m_desiredChannel + chanClosingHigh, IoArray::ChannelConfig::ACTIVE_HIGH) && success;
                if (success) {
                    m_startChannel = m_desiredChannel;
                } else {
                    for (uint8_t i = 0; i < 4; ++i) {
                        devPtr->releaseChannel(m_desiredChannel + i); // cancel all channels again
                    }
                }
            } else {
                m_desiredChannel = 0;
            }
        }
    }

    virtual bool
    supportsFastIo() const override final
    {
        return false;
    }
};
