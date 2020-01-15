/*
 * Copyright 2014 BrewPi/Elco Jacobs.
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

#include "BrewPiTouch.h"
#include "Board.h"

#include <algorithm>
#include <cstdint>
#include <limits.h>
#include <vector>

BrewPiTouch::BrewPiTouch(SPIArbiter& spia, const uint8_t cs, const uint8_t irq)
    : _spi(spia)
    , pinCS(cs)
    , pinIRQ(irq)
    , filterX(0)
    , filterY(0)
{
    config = BrewPiTouch::START;
}

BrewPiTouch::~BrewPiTouch()
{
}

void
BrewPiTouch::init()
{
    // default is:
    // 12bit (mode=0)
    // power down between conversions, penIRQ enabled (PD1,PD0 = 00)
    // Differential reference (SER/DFR = 0)
    pinMode(pinCS, OUTPUT);
    pinMode(pinIRQ, INPUT_PULLUP);
    // clock base is 120/2 = 60Mhz on the Photon and 72 Mhz on the core.
    // Minimum clock cycle is 200 + 200 = 400 ns
    // Fmax = 2.5 Mhz. 60 Mhz / 2.5 MHz = 24. Clock div of 32 is within margins
    _spi.setClockDivider(SPI_CLOCK_DIV32);
    _spi.setDataMode(SPI_MODE0);
    _spi.begin(pinCS);
    _spi.transfer(config);
    _spi.end();

    update();
}

void
BrewPiTouch::set8bit()
{
    config = config | MODE;
}

void
BrewPiTouch::set12bit()
{
    config = config & ~MODE;
}

bool
BrewPiTouch::is8bit() const
{
    return (config & MODE) ? 1 : 0;
}

bool
BrewPiTouch::is12bit() const
{
    return (config & MODE) ? 0 : 1;
}

uint16_t
BrewPiTouch::readChannel(uint8_t channel, bool singleEnded) const
{
    _spi.begin(); // will drive CS pin low, needed for conversion timing
    _spi.transfer((config & CHMASK)
                  | channel                                                            // select channel
                  | (singleEnded ? controlBits::SER | controlBits::PD1 : uint8_t(0))); // enable internal reference and apply single ended conversion
    delayMicroseconds(2);                                                              // make sure aquisition is complete, without checking busy pin
    uint16_t data = _spi.transfer(0);
    data = data << 8;
    data += _spi.transfer(0);
    uint8_t shift = is12bit() ? 4 : 8;
    if (singleEnded) {
        shift = shift - 1; // single ended conversions are delayed by 1 cycle
    }
    data = data >> shift;
    _spi.end(); // will drive CS pin high again
    return data;
}

uint16_t
BrewPiTouch::read5V() const
{
    return readChannel(CHBAT, true);
}

uint16_t
BrewPiTouch::read12V() const
{
    return readChannel(CHAUX, true);
}

bool
BrewPiTouch::isTouched() const
{
    return (pinReadFast(pinIRQ) == HIGH ? 0 : 1);
}

int16_t
BrewPiTouch::getX() const
{
    return filterX.read();
}

int16_t
BrewPiTouch::getY() const
{
    return filterY.read();
}

/*
 *  update() updates the x and y coordinates of the touch screen
 *  It reads numSamples values and takes the median
 *  The result is fed to the low pass filters
 */
bool
BrewPiTouch::update()
{
    if (!isTouched()) {
        return false; // exit immediately when not touched to prevent claiming SPI
    }
    // read enough samples for settling time of low pass filter
    for (uint16_t i = 0; i < 20; i++) {
        if (!isTouched()) {
            return false;
        }
        filterX.add(readChannel(CHX, false));
        filterY.add(readChannel(CHX, false));
    }
    return true;
}
