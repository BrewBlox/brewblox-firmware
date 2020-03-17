#pragma once

#ifdef __arm__
#define CNL_RELEASE true
#endif

#define CNL_USE_INT128 false

#include "cnl/elastic_integer.h"
#include "cnl/num_traits.h"
#include "cnl/overflow_integer.h"
#include "cnl/scaled_integer.h"
#include "cnl/wide_integer.h"
#include <cstdint>

template <
    int IntegerDigits,
    int FractionalDigits,
    class Narrowest = int32_t>
using safe_elastic_fixed_point = cnl::scaled_integer<
    cnl::overflow_integer<
        cnl::elastic_integer<
            IntegerDigits + FractionalDigits,
            cnl::wide_integer<
                cnl::digits_v<Narrowest>,
                Narrowest>>,
        cnl::saturated_overflow_tag>,
    cnl::power<-FractionalDigits>>;

using fp12_t = safe_elastic_fixed_point<11, 12>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);
