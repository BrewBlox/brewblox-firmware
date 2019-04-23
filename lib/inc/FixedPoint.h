#pragma once

// include fixed_point sources from _impl because std::to_string is not available on arm
#include "../cnl/include/cnl/_impl/fixed_point/constants.h"
#include "../cnl/include/cnl/_impl/fixed_point/declaration.h"
#include "../cnl/include/cnl/_impl/fixed_point/extras.h"
#include "../cnl/include/cnl/_impl/fixed_point/fraction.h"
#include "../cnl/include/cnl/_impl/fixed_point/from_rep.h"
#include "../cnl/include/cnl/_impl/fixed_point/is_fixed_point.h"
#include "../cnl/include/cnl/_impl/fixed_point/named.h"
#include "../cnl/include/cnl/_impl/fixed_point/operators.h"
#include "../cnl/include/cnl/_impl/fixed_point/to_chars.h"
#include "../cnl/include/cnl/_impl/fixed_point/type.h"
#include "../cnl/include/cnl/_impl/num_traits/wrap.h"

#include "../cnl/include/cnl/elastic_integer.h"
#include "../cnl/include/cnl/overflow_integer.h"
#include "../cnl/include/cnl/rounding_integer.h"

#include <cstdint>
#include <type_traits>

template <
    int IntegerDigits,
    int FractionalDigits,
    class Narrowest>
using saturated_elastic_fixed_point = cnl::fixed_point<cnl::overflow_integer<
                                                           cnl::elastic_integer<
                                                               IntegerDigits + FractionalDigits,
                                                               Narrowest>,
                                                           cnl::saturated_overflow_tag>,
                                                       -FractionalDigits>;

using fp12_t = saturated_elastic_fixed_point<11, 12, std::int32_t>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);