#pragma once

#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/static_number.h"
#include <cstdint>

template <
    int Digits,
    int Exponent,
    typename Narrowest = signed>
using safe_elastic_fixed_point = cnl::static_number<Digits, Exponent, cnl::native_rounding_tag, cnl::saturated_overflow_tag, Narrowest>;

using fp12_t = safe_elastic_fixed_point<23, -12>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);