#define CATCH_CONFIG_MAIN
#include <catch.hpp> // ensure that this is the single_include version

#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/static_number.h"
#include <catch.hpp>
#include <cstdint>
#include <limits>

template <
    int Digits,
    int Exponent,
    typename Narrowest = signed>
using safe_elastic_fixed_point = cnl::static_number<Digits, Exponent, cnl::native_rounding_tag, cnl::saturated_overflow_tag, Narrowest>;

using fp12_t = safe_elastic_fixed_point<23, -12>;

template <class T>
constexpr const T&
clamp(const T& v, const T& lo, const T& hi)
{
    assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

const int max = 1000; // set to 100 when running with callgrind
const int onlyTestEveryXLoops = 111;

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
            if (loop % onlyTestEveryXLoops == 0) {
                if (c >= decltype(c)(4096)) {
                    REQUIRE(cnl::unwrap(d) == (int64_t(1) << 23) - 1);
                };
            }
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
            if (loop % onlyTestEveryXLoops == 0) {
                REQUIRE(c < int64_t(1000000) << 12);
                REQUIRE(d >= 0);
                if (c >= (int64_t(1) << 23) - 1) {
                    REQUIRE(d == (int64_t(1) << 23) - 1);
                };
            }
        }
    }
}
