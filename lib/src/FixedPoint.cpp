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

std::string
to_string_dec(const fp12_t& t, uint8_t decimals)
{
    // shift to right number of digits
    fp12_t shifted = t;
    for (uint8_t i = decimals; i > 0; --i) {
        shifted = shifted * int8_t(10);
    }

    // convert to int
    auto intValue = int32_t(shifted); // will round correctly due to nearest_roundig_tag

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

// implementation below using CNL functions is much slower
std::string
to_string_dec2(const fp12_t& t, uint8_t decimals)
{
    using calc_t = cnl::set_rounding_t<safe_elastic_fixed_point<14, 16>, cnl::native_rounding_tag>;

    int rounderScale = 10;
    for (uint8_t i = decimals; i > 0; --i) {
        rounderScale *= 10;
    }
    calc_t rounder = (t >= 0) ? calc_t{5} : calc_t{-5};
    calc_t val = calc_t{t} * rounderScale + rounder;

    auto rounded = val / rounderScale;
    auto s = cnl::to_string(rounded);
    auto dot = s.find_first_of('.');
    auto end = s.length();
    auto originalDecimals = end - dot - 1;
    if (dot == std::string::npos) {
        // no dot found, append zeros
        s.push_back('.');
        while (decimals-- > 0) {
            s.push_back('0');
        }
    } else if (originalDecimals < decimals) {
        while (decimals-- >= originalDecimals) {
            s.push_back('0');
        }
    } else if (originalDecimals > decimals) {
        while (originalDecimals-- > decimals) {
            s.pop_back();
        }
    }

    return s;
}