/*
 * Copyright 2019 BrewPi B.V.
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

#include <catch.hpp>

#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/static_number.h"

#include <cstdint>

template <
    int IntegerDigits,
    int FractionalDigits,
    class Narrowest>
using saturated_elastic_fixed_point = cnl::static_number<
    IntegerDigits + FractionalDigits, -FractionalDigits, cnl::native_rounding_tag, cnl::saturated_overflow_tag, Narrowest>;

using temp_t = saturated_elastic_fixed_point<7, 8, std::int16_t>;
using temp_precise_t = saturated_elastic_fixed_point<7, 23, std::int32_t>;
using temp_wide_t = saturated_elastic_fixed_point<23, 8, std::int32_t>;

SCENARIO("CNL bloated execution time", "[fp_bloat]")
{
    WHEN("A unaray + operator is used, it runs very slowly")
    {
        temp_wide_t v = 0;
        while (v < 1000) {
            v += temp_wide_t(0.10);
            CHECK(v > 0);
        }
    }

    WHEN("The bit below doesn't affect run time at all")
    {
        uint32_t v = 0;
        while (v < 100000) {
            v += uint32_t(1);
        }
    }
}
