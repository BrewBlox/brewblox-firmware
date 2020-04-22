/*
 * Copyright 2016 BrewPi/Matthew McGowan.
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

#include "spark_wiring_spi.h"
#include <functional>
#include <mutex>

const uint16_t SS_PIN_NONE = UINT16_MAX - 1;
const uint16_t SS_PIN_UNINITIALIZED = UINT16_MAX;

class SPIConfiguration {
public:
    SPIConfiguration()
        : mode_(SPI_MODE0)
        , bitOrder_(MSBFIRST)
        , clockDivider_(SPI_CLOCK_DIV128)
        , ss_pin_(SS_PIN_UNINITIALIZED)
    {
    }
    ~SPIConfiguration() = default;

protected:
    uint8_t mode_;
    uint8_t bitOrder_;
    uint8_t clockDivider_;
    uint16_t ss_pin_;

public:
    inline uint8_t getMode() const { return mode_; }
    inline uint8_t getBitOrder() const { return bitOrder_; }
    inline uint8_t getClockDivider() const { return clockDivider_; }
    inline uint16_t getSSPin() const { return ss_pin_; }
};

class SPIArbiter : private SPIConfiguration {
    SPIConfiguration* current_;

    std::mutex mutex;

    void unapply();
    void apply(SPIConfiguration& client);

public:
    SPIArbiter()
        : current_(nullptr)
    {
    }

    ~SPIArbiter()
    {
    }

    inline bool try_begin(SPIConfiguration& client)
    {
        if (isClient(client) || mutex.try_lock()) {
            current_ = &client;
            apply(client);
        }
        return isClient(client);
    }

    inline void begin(SPIConfiguration& client)
    {
        mutex.lock();
        current_ = &client;
        apply(client);
    }

    inline void end(SPIConfiguration& client)
    {
        if (isClient(client)) {
            current_ = nullptr;
            unapply();
            mutex.unlock();
        }
    }

    /*
     * transfer assumes the SPI has already been acquired by calling begin()
     */
    inline uint8_t transfer(SPIConfiguration& client, uint8_t data)
    {
        if (isClient(client)) {

            return HAL_SPI_Send_Receive_Data(HAL_SPI_INTERFACE1, data);
        }
        return 0;
    }

    inline void transfer(SPIConfiguration& client, void* tx_buffer, void* rx_buffer, size_t length, wiring_spi_dma_transfercomplete_callback_t user_callback)
    {
        if (isClient(client)) {
            HAL_SPI_DMA_Transfer(HAL_SPI_INTERFACE1, tx_buffer, rx_buffer, length, user_callback);
        }
        // todo - should we independently track that DMA is in progress?
    }

    inline void transferCancel(SPIConfiguration& client)
    {
        if (isClient(client)) {
            HAL_SPI_DMA_Transfer_Cancel(HAL_SPI_INTERFACE1);
        }
    }

    inline bool isClient(SPIConfiguration& client)
    {
        return &client == current_;
    }
};

/**
 *
 */
class SPIUser : public SPIConfiguration {
    SPIArbiter& spi_;

public:
    SPIUser(SPIArbiter& spi)
        : spi_(spi)
    {
    }

    inline void begin()
    {
        spi_.begin(*this);
    }

    inline void begin(uint16_t ss_pin)
    {
        ss_pin_ = ss_pin;
        spi_.begin(*this);
    }

    inline void end()
    {
        spi_.end(*this);
    }

    inline void setBitOrder(uint8_t bitOrder)
    {
        bitOrder_ = bitOrder;
    }

    inline void setDataMode(uint8_t dataMode)
    {
        mode_ = dataMode;
    }

    /**
     * Sets the clock speed as a divider relative to the clock divider reference.
     * @param divider SPI_CLOCK_DIVx where x is a power of 2 from 2 to 256.
     */
    inline void setClockDivider(uint8_t divider)
    {
        clockDivider_ = divider;
    }

    inline byte transfer(byte _data)
    {
        return spi_.transfer(*this, _data);
    }

    inline void transfer(void* tx_buffer, void* rx_buffer, size_t length, wiring_spi_dma_transfercomplete_callback_t user_callback)
    {
        spi_.transfer(*this, tx_buffer, rx_buffer, length, user_callback);
    }

    inline void transferCancel()
    {
        spi_.transferCancel(*this);
    }
};

extern SPIArbiter GlobalSPIArbiter;
