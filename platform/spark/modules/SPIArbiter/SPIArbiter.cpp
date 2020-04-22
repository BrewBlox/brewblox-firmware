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

#include "SPIArbiter.h"

void
SPIArbiter::apply(SPIConfiguration& client)
{
    if (ss_pin_ == SS_PIN_UNINITIALIZED) {
        // first time using the SPI, initialize it.
        // this applies Particle defaults, so do it before configuring the mode, clock and bit order.
        // only do it when the SPI is first used.
        HAL_SPI_Begin(HAL_SPI_INTERFACE1, client.getSSPin());
    }

    if (ss_pin_ != client.getSSPin()) {
        ss_pin_ = client.getSSPin();
        digitalWrite(ss_pin_, LOW); // select new client
    }
    if (mode_ != client.getMode()) {
        mode_ = client.getMode();
        HAL_SPI_Set_Data_Mode(HAL_SPI_INTERFACE1, mode_);
    }

    if (bitOrder_ != client.getBitOrder()) {
        bitOrder_ = client.getBitOrder();
        HAL_SPI_Set_Bit_Order(HAL_SPI_INTERFACE1, bitOrder_);
    }

    if (clockDivider_ != client.getClockDivider()) {
        clockDivider_ = client.getClockDivider();
        HAL_SPI_Set_Clock_Divider(HAL_SPI_INTERFACE1, clockDivider_);
    }
}

void
SPIArbiter::unapply()
{
    digitalWrite(ss_pin_, HIGH); // unselect pin
    ss_pin_ = SS_PIN_NONE;
    // spi_.end(); do not end global SPI, leave SPI Active.
}

SPIArbiter GlobalSPIArbiter;
