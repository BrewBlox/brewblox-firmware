#define CATCH_CONFIG_MAIN
#include <catch.hpp> // ensure that this is the single_include version

#define CNL_USE_INT128 false

#include "../cnl/include/cnl/elastic_integer.h"
#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/overflow_integer.h"
#include "../cnl/include/cnl/rounding_integer.h"
#include "../cnl/include/cnl/scaled_integer.h"
#include <cstdint>
#include <limits>

template <
    int IntegerDigits,
    int FractionalDigits,
    class Narrowest = int32_t>
using safe_elastic_fixed_point = cnl::scaled_integer<
    cnl::elastic_integer<
        IntegerDigits + FractionalDigits,
        cnl::rounding_integer<
            cnl::overflow_integer<
                Narrowest,
                cnl::saturated_overflow_tag>,
            cnl::nearest_rounding_tag>>,
    cnl::power<-FractionalDigits>>;

using fp12_t = safe_elastic_fixed_point<11, 12>;

template <class T>
constexpr const T&
clamp(const T& v, const T& lo, const T& hi)
{
    assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

const int max = 1000; // set to 100 when running with callgrind

TEST_CASE("Fixedpoint calculations", "[cnltest]")
{

    double aMax = max;
    double bMax = max;
    int loop = 0;
    for (double a = 0; a < aMax; a += 1) {
        for (double b = 0; b < bMax; b += 1) {
            loop++;
            auto a_f = fp12_t(a);
            auto b_f = fp12_t(b);

            auto c = a_f * b_f;
            auto d = fp12_t(c);

            // INFO(c); // this makes the tests slow!

            REQUIRE(c < decltype(c)(max * max));
            REQUIRE(d >= 0);
            if (c >= decltype(c)(2048)) {
                REQUIRE(cnl::unwrap(d) == (int64_t(1) << 23) - 1);
            };
        }
    }
}

TEST_CASE("Equivalent integer calculations", "[cnltest]")
{
    int32_t aMax = max << 12;
    int32_t bMax = max << 12;
    int loop = 0;
    for (int32_t a = 0; a < aMax; a += (1 << 12)) {
        for (int32_t b = 0; b < bMax; b += (1 << 12)) {
            loop++;
            auto c = (int64_t(a) * int64_t(b)) >> 12;
            int32_t d = clamp(c, -(int64_t(1) << 23), (int64_t(1) << 23) - 1);

            // INFO(c);

            REQUIRE(c < int64_t(max * max) << 12);
            REQUIRE(d >= 0);
            if (c >= (int64_t(1) << 23) - 1) {
                REQUIRE(d == (int64_t(1) << 23) - 1);
            };
        }
    }
}
