/*
 * Copyright 2018 BrewPi
 *
 * This file is part of BrewPi.
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

#include "catch.hpp"

#include "../inc/Temperature.h"
#include "../inc/future_std.h"
#include "FpFilterChain.h"
#include "TestMatchers.hpp"
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

SCENARIO("Fixed point filterchain using temp_t", "[fpfilterchain]")
{

    std::vector<std::vector<uint8_t>> chainsSpecs = {{0},
                                                     {0, 1},
                                                     {0, 1, 1}};

    for (uint8_t filter = 0; filter <= 6; filter++) {
        GIVEN("A filter chain tapped at stage " + std::to_string(filter))
        {
            FpFilterChain<temp_t> chain(filter);

            temp_t step = 10;
            WHEN("A step input of std::to_string(step) is applied")
            {
                uint32_t count = 0;
                while (count++ < 3000) {
                    chain.add(step);
                }
                THEN("The steady state output matches the step amplitude")
                {
                    // INFO(filter);
                    CHECK(chain.read() == Approx(temp_t(10)).epsilon(0.001));
                }
            }
        }
    }

    WHEN("Some filter chains are excited with a sine wave")
    {
        auto sine = [](const uint32_t& t, const uint32_t& period, const temp_t& ampl) {
            auto result = ampl * temp_t(sin(2.0 * M_PI * t / period));
            return temp_t(result);
        };

        auto findGainAtPeriod = [&sine](FpFilterChain<temp_t>& c, const uint32_t& period, bool checkMaxDerivative = true) {
            using derivative_t = safe_elastic_fixed_point<1, 23>;
            const temp_t amplIn = 10;
            temp_t max = 0;
            derivative_t maxDerivative = 0;
            c.reset(0);
            for (uint32_t t = 0; t < period * 6; t++) {
                auto wave = sine(t, period, amplIn);
                c.add(wave);
                if (t > 3 * period) { // ignore start
                    auto filterOutput = c.read();
                    auto derivative = c.readDerivative<derivative_t>();
                    if (filterOutput > max) {
                        max = filterOutput;
                    }
                    if (derivative > maxDerivative) {
                        maxDerivative = derivative;
                    }
                }
            }
            auto gain = double(max) / amplIn;
            if (checkMaxDerivative) {
                // derivative should be filtered as much as the amplitude
                auto maxDerivativeIn = double(amplIn) * 2 * M_PI / period;
                auto maxDerivativeDouble = double(maxDerivative);
                auto derivativeGain = maxDerivativeDouble / maxDerivativeIn;
                CHECK(derivativeGain == Approx(gain).epsilon(0.15));
            }
            return gain;
        };

        auto findHalfAmplitudePeriod = [&findGainAtPeriod](FpFilterChain<temp_t>& c) {
            uint32_t period = 10;
            while (true) {
                auto gain = findGainAtPeriod(c, period, false);
                if (gain >= temp_t(0.5)) {
                    // std::cout << "\n"
                    //          << "Period of sine wave that is decreased by 0.5: "
                    //          << period << "\n";
                    return period;
                }
                auto periodIncreaseFactor = 1.0 + (0.5 - gain); // fast approach until close
                periodIncreaseFactor = std::clamp(periodIncreaseFactor, 1.0, 1.1);

                period = std::max(period * periodIncreaseFactor, period + 1.0); // add at least 1
                // std::cout << "[" << gain << ", " << period << "]\t";
            };
        };

        THEN("They block higher frequencies and pass lower frequencies")
        {
            {
                FpFilterChain<temp_t> chain(1);
                CHECK(findHalfAmplitudePeriod(chain) == 13);
                CHECK(findGainAtPeriod(chain, 7) < 0.1);
                CHECK(findGainAtPeriod(chain, 26) > 0.8);
            }
            {
                FpFilterChain<temp_t> chain(2);
                CHECK(findHalfAmplitudePeriod(chain) == 43);
                CHECK(findGainAtPeriod(chain, 22) < 0.1);
                CHECK(findGainAtPeriod(chain, 86) > 0.8);
            }
            {
                FpFilterChain<temp_t> chain(3);
                CHECK(findHalfAmplitudePeriod(chain) == 91);
                CHECK(findGainAtPeriod(chain, 46) < 0.1);
                CHECK(findGainAtPeriod(chain, 182) > 0.8);
            }
            {
                FpFilterChain<temp_t> chain(4);
                CHECK(findHalfAmplitudePeriod(chain) == 184);
                CHECK(findGainAtPeriod(chain, 92) < 0.1);
                CHECK(findGainAtPeriod(chain, 368) > 0.8);
            }
            {
                FpFilterChain<temp_t> chain(5);
                CHECK(findHalfAmplitudePeriod(chain) == 519);
                CHECK(findGainAtPeriod(chain, 259) < 0.1);
                CHECK(findGainAtPeriod(chain, 1038) > 0.8);
            }
            {
                FpFilterChain<temp_t> chain(6);
                CHECK(findHalfAmplitudePeriod(chain) == 1546);
                CHECK(findGainAtPeriod(chain, 773) < 0.1);
                CHECK(findGainAtPeriod(chain, 3096) > 0.8);
            }
        }

        AND_WHEN("The step threshold is set, a slow filter will respond much quicker to a step input")
        {
            auto findStepResponseDelay = [](FpFilterChain<temp_t>& c, const temp_t& stepAmpl) {
                c.reset(0);
                uint32_t t = 0;
                uint32_t stagesFinished = 0;
                for (t = 0; t < 10000 && stagesFinished < c.length(); ++t) {
                    c.add(stepAmpl);
                    for (uint8_t i = 0; i < c.length(); ++i) {
                        auto filterOutput = c.read(uint8_t(i + 1));
                        if (filterOutput >= stepAmpl / 2 && i >= stagesFinished) { // ignore start
                            // std::cout << "stage " << +i << " at 50\% at t=" << t << ", derivative:" << c.readDerivative<temp_t>(i) << "\n";
                            ++stagesFinished;
                        }
                    }
                }
                // std::cout << "\n";
                return t;
            };
            FpFilterChain<temp_t> chain(6);
            CHECK(findStepResponseDelay(chain, temp_t(100)) == 1400);
            CHECK(findStepResponseDelay(chain, temp_t(10)) == 1400);
            CHECK(findStepResponseDelay(chain, temp_t(0.9)) == 1400);
            chain.setStepThreshold(1);
            CHECK(findStepResponseDelay(chain, 100) < 400);
            CHECK(findStepResponseDelay(chain, 10) < 1400 / 2);
            CHECK(findStepResponseDelay(chain, 0.9) == 1400);
        }

        AND_WHEN("The derivative can be read from a different filter stage, selected by the max interval")
        {
            using derivative_t = safe_elastic_fixed_point<1, 23>;
            uint32_t period = 200;
            const temp_t amplIn = period / (2.0 * M_PI); // derivative max 1

            FpFilterChain<temp_t> c(6);

            derivative_t maxDerivative12 = 0;
            derivative_t maxDerivative25 = 0;
            derivative_t maxDerivative100 = 0;
            for (uint32_t t = 0; t < period * 10; ++t) {
                auto wave = sine(t, period, amplIn);
                c.add(wave);

                auto derivative = c.readDerivative<derivative_t>(c.intervalToFilterIdx(12));
                if (derivative > maxDerivative12) {
                    maxDerivative12 = derivative;
                }
                derivative = c.readDerivative<derivative_t>(c.intervalToFilterIdx(25));
                if (derivative > maxDerivative25) {
                    maxDerivative25 = derivative;
                }

                derivative = c.readDerivative<derivative_t>(c.intervalToFilterIdx(100));
                if (derivative > maxDerivative100) {
                    maxDerivative100 = derivative;
                }
            }

            CHECK(maxDerivative12 > 0.8);
            CHECK(maxDerivative25 > 0.5);  // sample rate of 25 should have most of the sine wave left
            CHECK(maxDerivative100 < 0.1); // sample rate of 100 should filted out the sine with period 200
        }
    }
}