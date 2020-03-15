#pragma once

#include "cnl/_impl/fixed_point/constants.h"
#include "cnl/_impl/fixed_point/convert.h"
#include "cnl/_impl/fixed_point/declaration.h"
#include "cnl/_impl/fixed_point/extras.h"
// #include "cnl/_impl/fixed_point/fraction.h"
#include "cnl/_impl/fixed_point/from_rep.h"
#include "cnl/_impl/fixed_point/is_fixed_point.h"
#include "cnl/_impl/fixed_point/named.h"
#include "cnl/_impl/fixed_point/operators.h"
#include "cnl/_impl/fixed_point/to_chars.h"
// #include "cnl/_impl/fixed_point/to_string.h"
#include "cnl/_impl/fixed_point/type.h"

#include "cnl/elastic_integer.h"
#include "cnl/num_traits.h"
#include "cnl/overflow_integer.h"
#include "cnl/rounding_integer.h"

template <uint8_t I, uint8_t F, class Narrowest>
using elastic_fixed_point = cnl::fixed_point<
    cnl::elastic_integer<
        I + F,
        Narrowest>,
    -F>;

template <uint8_t I, uint8_t F, class Narrowest>
using safe_elastic_fixed_point = cnl::fixed_point<
    cnl::overflow_integer<
        cnl::elastic_integer<
            I + F,
            Narrowest>,
        cnl::saturated_overflow_tag>,
    -F>;

using fp12_t = safe_elastic_fixed_point<11, 12, int32_t>;

struct fp12_v_t {
    fp12_t value;
    bool valid;
};

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);