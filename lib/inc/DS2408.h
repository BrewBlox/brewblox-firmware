/*
 * Copyright 2013 Matthew McGowan 
 * Copyright 2013 BrewPi/Elco Jacobs.
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

#include <cstdint>

#include "IoArray.h"
#include "OneWireDevice.h"

#define DS2408_FAMILY_ID 0x29

/**
 * Provides access to a OneWire-addressable 8-channel I/O device.
 * Each output has a pull-down transistor, which can be enabled by writing 0 to the pio output latch.
 * This pulls the output pin to GND.
 *
 * When the output latch is disabled, the pio can be read as digital input (sense).
 * This is the power on-default if a reset signal is pulled low. Without reset, the state is random.
 */
class DS2408 : public OneWireDevice, public IoArray {

private:
    // cache of all of the DS2408 status registers
    mutable struct {              // bass address 0x0088 on chip
        uint8_t pio;              // 0 = pio logic state
        uint8_t latch;            // 1 = output latch state
        uint8_t activity;         // 2 = activity latch state
        uint8_t cond_search_mask; // 3 = conditional search channel selection mask
        uint8_t cond_search_pol;  // 4 = conditional search channel polarity selection
        uint8_t status;           // 5 = control/status register

    } m_regCache;

    static const uint8_t READ_PIO_REG = 0xF0;
    static const uint8_t ACCESS_READ = 0xF5;
    static const uint8_t ACCESS_WRITE = 0x5A;
    static const uint8_t ACK_SUCCESS = 0xAA;
    static const uint8_t ACK_ERROR = 0xFF;

    // all addresses have upper bits 0x00
    static const uint8_t ADDRESS_UPPER = 0x00;
    static const uint8_t ADDRESS_PIO_STATE_LOWER = 0x88;
    static const uint8_t ADDRESS_LATCH_STATE_LOWER = 0x89;

public:
    /**
     * Constructor initializes both caches to 0xFF.
     * This means the output latches are disabled and all pins are sensed high
     */
    DS2408(OneWire& oneWire, OneWireAddress address = 0)
        : OneWireDevice(oneWire, address)
        , IoArray(8)
    {
        m_regCache.pio = 0xFF;
        m_regCache.latch = 0xFF;
        updateLatches();
    }

    /**
     * Destructor is default.
     */
    ~DS2408() = default;

private:
    /**
     * extracts a single bit from a byte
     *
     * @param target input byte to extract a bit from
     * @param pos position of the byte with the rightmost bit being A.
     * @returns extracted bit as bool
     */
    inline bool getBit(const uint8_t& target, const uint8_t& pos) const
    {
        return ((0b1 << pos) & target) != 0x0;
    }

    /**
     * sets a single bit in a byte
     * @param input byte to change a bit in
     * @param pos position of bit in the byte
     * @param state new state for the bit, 1 or 0
     * @returns new byte with the bit at position pos changed to state
     */
    inline uint8_t setBit(const uint8_t& input, const uint8_t& pos, bool state) const
    {
        uint8_t mask = (0b1 << pos);
        if (state) {
            return input | mask;
        } else {
            return input & ~mask;
        }
    }

    /**
     * Set all output latches to correct state based on channel config
     * @return true on success
     */
    bool updateLatches()
    {
        uint8_t newLatches = 0xFF; // all latches disabled
        for (uint8_t i = 0; i < 8; ++i) {
            newLatches = setBit(newLatches, i, channels[i].config != ChannelConfig::ACTIVE_HIGH);
        }

        bool success = accessWrite(newLatches);
        if (success) {
            m_regCache.latch = newLatches;
        }
        return success;
    }

    /**
     * Reads the pio state of all pins and returns them as a single byte.
     * Note that the state that is returned is the actual pin state, not the state of the latch register.
     * If the pull down latch is disabled (written as 0), this can be used to read an input switch
     * @return bit field with all 8 pio pin states
     */
    uint8_t accessRead();

    /**
     * Writes the state of all PIOs in one operation.
     * @param latches pio data - a bit field with new values for the output latch
     * @param maxTries the maximum number of attempts before giving up.
     * @return true on success
     */
    bool accessWrite(uint8_t latches, uint8_t maxTries = 1);

    /**
     * Updates all cache registers by reading them from the device.
     * Performs CRC checking on communication and sets the connect state to false on CRC error or to true on success.
     */

public:
    void update() const;

    // generic ArrayIo interface
    virtual bool senseChannelImpl(uint8_t channel, State& result) const override final
    {
        if (connected() && validChannel(channel)) {
            bool pioState = getBit(m_regCache.pio, channel - 1);
            if (pioState == false) {
                result = State::Active;
            } else {
                result = State::Inactive;
            }
            return true; // valid channel
        }
        result = State::Unknown;
        return false;
    }

    virtual bool writeChannelImpl(uint8_t channel, const ChannelConfig&) override final
    {
        // second argument is not used, already set by caller and used in updateLatches
        if (connected() && validChannel(channel)) {
            updateLatches();
            return true;
        }
        return false;
    }

    virtual bool supportsFastIo() const override final
    {
        return false;
    }
};
