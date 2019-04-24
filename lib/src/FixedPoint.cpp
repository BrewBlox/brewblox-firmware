/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of the BrewBlox Control Library.
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

#include "FixedPoint.h"
#include "Logger.h"
#include <algorithm>
#include <cstring>
#include <stdlib.h>

constexpr uint32_t
pow10(uint8_t e)
{
    uint32_t r = 10;
    while (e--) {
        r *= 10;
    }

    return r;
}

std::string
to_string_dec(const fp12_t& t, uint8_t decimals)
{

    // shift to right number of digits
    auto shifted = t * pow10(decimals);

    // ensure correct rounding
    auto rounder = fp12_t(0.5);
    shifted = t >= fp12_t(0) ? shifted + rounder : shifted - rounder;

    // convert to int
    auto intValue = int32_t(shifted);

    // convert to string
    auto s = std::string();
    s = s << intValue;

    // prefix zeros for values smaller than 1
    uint8_t insertPos = (intValue < 0) ? 1 : 0; // handle minus sign
    uint8_t minChars = decimals + insertPos + 1;
    if (minChars > s.size()) {
        s.insert(insertPos, minChars - s.size(), '0');
    }

    // insert decimal point
    s.insert(s.end() - decimals, '.');
    return s;
}