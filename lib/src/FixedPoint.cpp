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
pow10(int e)
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
    static constexpr fp12_t rounders[5] = {cnl::fraction<>(1, 2),
                                           cnl::fraction<>(1, 20),
                                           cnl::fraction<>(1, 200),
                                           cnl::fraction<>(1, 2000),
                                           cnl::fraction<>(1, 20000)};

    if (decimals > 4) {
        decimals = 4;
    }

    auto rounder = (t >= 0) ? rounders[decimals] : -rounders[decimals];

    std::string result = cnl::to_chars(t + rounder).data();

    // trim digits after requested precision
    result.erase(result.find('.') + decimals + 1, std::string::npos);

    return result;
}