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

std::string
to_string_dec(const fp12_t& t, uint8_t decimals)
{
    using calc_t = cnl::set_rounding_t<safe_elastic_fixed_point<14, 16>, cnl::native_rounding_tag>;

    int scale = 10;
    for (uint8_t i = decimals; i > 1; --i) {
        scale *= 10;
    }

    auto rounder = (t >= 0) ? calc_t{0.5} : calc_t{-0.5};
    auto asInt = static_cast<int32_t>(scale * calc_t{t} + rounder);
    auto s = std::to_string(asInt);

    int missingZeros = int(decimals) + 1 - s.length() + (asInt < 0);
    auto insertAt = s.begin() + (asInt < 0);
    if (missingZeros > 0) {
        insertAt = s.insert(insertAt, missingZeros, '0'); // leading zeros
    }
    auto periodPos = s.end() - decimals;
    s.insert(periodPos, '.');
    return s;
}