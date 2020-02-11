#pragma once

#ifdef __arm__
#include <string>

// forward declare std::to_string. Arm gcc 5.3 compiler cannot find it in headers and cnl has references to it in headers
namespace std {
template <typename T>
string to_string(T);
}
#define CNL_RELEASE true
#endif

#define CNL_USE_INT128 false

#include "../cnl/include/cnl/elastic_integer.h"
#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/overflow_integer.h"
#include "../cnl/include/cnl/scaled_integer.h"
#include "../cnl/include/cnl/wide_integer.h"
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
