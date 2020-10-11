#include "ActuatorPwm.h"
#include "future_std.h"
#include <cstdint>

#if PLATFORM_ID != PLATFORM_GCC
#include "TimerInterrupts.h"
#endif

ActuatorPwm::ActuatorPwm(
    std::function<std::shared_ptr<ActuatorDigitalConstrained>()>&& target_,
    duration_millis_t period_)
    : m_target(target_)
{
    period(period_);
}

void
ActuatorPwm::setting(const value_t& val)
{
    if (val <= value_t{0}) {
        m_dutySetting = value_t{0};
        m_dutyTime = 0;
    } else if (val >= maxDuty()) {
        m_dutySetting = maxDuty();
        m_dutyTime = m_period;
    } else {
        m_dutySetting = val;
        auto unScaledTime = m_dutySetting * m_period;
        m_dutyTime = uint64_t(unScaledTime) / 100;
    }

    settingValid(true);
}

// returns the actual achieved PWM value, not the set value
ActuatorPwm::value_t
ActuatorPwm::value() const
{
    return m_dutyAchieved;
}

safe_elastic_fixed_point<2, 28>
ActuatorPwm::dutyFraction() const
{
    constexpr auto rounder = (cnl::numeric_limits<value_t>::min() >> 1);
    return safe_elastic_fixed_point<2, 28>{cnl::quotient(m_dutySetting + rounder, maxDuty())};
}

#if PLATFORM_ID != PLATFORM_GCC
void
ActuatorPwm::manageTimerTask()
{
    if (m_period < 1000 && m_enabled) {
        m_period = 100;
        if (!timerFuncId) {
            timerFuncId = TimerInterrupts::add([this]() { timerTask(); });
        }
    } else {
        if (timerFuncId) {
            TimerInterrupts::remove(timerFuncId);
            timerFuncId = 0;
            m_dutyAchieved = value_t{0};
        }
    }
}
#endif

void
ActuatorPwm::period(const duration_millis_t& p)
{
    m_period = p;
    if (auto actPtr = m_target()) {
        if (p < 1000 && !actPtr->supportsFastIo()) {
            m_period = 1000;
        }
    }
#if PLATFORM_ID != PLATFORM_GCC
    manageTimerTask();
#endif
}

duration_millis_t
ActuatorPwm::period() const
{
#if PLATFORM_ID != PLATFORM_GCC
    if (m_period < 1000) {
        return 10; // internally 100 is used for timer based pwm, but return 10ms, the actual period
    }
#endif
    return m_period;
}

#if PLATFORM_ID != PLATFORM_GCC
void
ActuatorPwm::timerTask()
{

    // timer clock is 10 kHz, 100 steps at 100Hz
    if (m_fastPwmElapsed == 0) {
        auto actPtr = m_target();
        if (m_dutyTime != 0 && actPtr) {
            if (actPtr->state() == State::Active) {
                m_dutyAchieved = maxDuty(); // was never low
            } else {
                actPtr->setStateUnlogged(State::Active);
            }
        } else {
            m_dutyAchieved = value_t{0};
        }
        m_fastPwmElapsed = 0;
    } else {
        if (m_fastPwmElapsed == m_dutyTime) {
            if (auto actPtr = m_target()) {
                actPtr->setStateUnlogged(State::Inactive);
                m_dutyAchieved = m_dutySetting;
            }
        }
    }
    m_fastPwmElapsed = m_fastPwmElapsed == 99 ? 0 : m_fastPwmElapsed + 1;
}

ActuatorPwm::update_t
ActuatorPwm::update(const update_t& now)
{
    if (timerFuncId) {
        return now + 1000;
    }
    return slowPwmUpdate(now);
}
#else
ActuatorPwm::update_t
ActuatorPwm::update(const update_t& now)
{
    return slowPwmUpdate(now);
}
#endif

ActuatorPwm::update_t
ActuatorPwm::slowPwmUpdate(const update_t& now)
{
    if (auto actPtr = m_target()) {
        auto durations = actPtr->activeDurations(now);
        auto currentHighTime = durations.currentActive;
        auto previousHighTime = durations.previousActive;
        auto previousPeriod = durations.previousPeriod;
        auto currentPeriod = durations.currentPeriod;
        auto lastHistoricState = durations.lastState;
        auto invDutyTime = m_period - m_dutyTime;

        auto wait = duration_millis_t(0);

        // limit history length taken into account
        // special case for 0% and 100%, use fixed window of 2*m_period
        auto twoPeriods = 2 * m_period;
        if (m_dutyTime == 0) {
            auto previousLowTime = previousPeriod - previousHighTime;
            auto currentLowTime = currentPeriod - currentHighTime;
            if (currentPeriod < twoPeriods && currentPeriod + previousLowTime > twoPeriods) {
                currentLowTime = std::min(currentLowTime + previousLowTime, twoPeriods - currentHighTime);
            } else {
                currentLowTime = currentLowTime + previousLowTime;
            }
            currentPeriod = twoPeriods;
            currentHighTime = twoPeriods - std::min(currentLowTime, twoPeriods);
            previousPeriod = 0;
            previousHighTime = 0;
        } else if (m_dutyTime == m_period) {
            if (currentPeriod < twoPeriods && currentPeriod + previousHighTime > twoPeriods) {
                auto currentLowTime = currentPeriod - currentHighTime;
                currentHighTime = std::min(currentHighTime + previousHighTime, twoPeriods - currentLowTime);
            } else {
                currentHighTime = currentHighTime + previousHighTime;
            }
            currentHighTime = std::min(currentHighTime, twoPeriods);
            currentPeriod = twoPeriods;
            previousPeriod = 0;
            previousHighTime = 0;
        } else {
            // The shortest part of the period is fixed (high over 50%, low under 50%)
            // For the previous period, cap the fixed part at m_period if the period was over 2 * m_periods long
            // discard excess
            if (previousPeriod > (twoPeriods)) {
                auto limit = m_period;
                if ((2 * m_dutyTime <= m_period)) {
                    // high period is fixed, low period adapts
                    if (previousHighTime > limit) {
                        auto excess = previousHighTime - limit;

                        previousHighTime = limit;
                        previousPeriod -= excess;
                    }
                } else {
                    // low period is fixed, high period adapts
                    auto previousLowTime = previousPeriod - previousHighTime;
                    if (previousLowTime > limit) {
                        auto excess = previousLowTime - limit;
                        previousPeriod -= excess;
                    }
                }
            }
            // for the current period, do the same, but shift the discarded bit to the previous period
            if (currentPeriod > twoPeriods) {
                auto limit = m_period;
                if ((2 * m_dutyTime <= m_period)) {
                    // high period is fixed, low period adapts
                    if (currentHighTime > limit) {
                        auto excess = currentHighTime - limit;

                        previousHighTime += excess;
                        previousPeriod += excess;
                        currentHighTime = limit;
                        currentPeriod -= excess;
                    }
                } else {
                    // low period is fixed, high period adapts
                    auto currentLowTime = currentPeriod - currentHighTime;
                    if (currentLowTime > limit) {
                        auto excess = currentLowTime - limit;

                        previousPeriod += excess;
                        currentPeriod -= excess;
                    }
                }
            }

            // compress the previous period, limit length to 3*m_period, keep duty % equal
            auto maxPeriod = 3 * m_period;
            if (previousPeriod > maxPeriod) {
                previousHighTime = uint64_t(previousHighTime) * uint64_t(maxPeriod) / previousPeriod;
                previousPeriod = maxPeriod;
            } else if (previousPeriod < m_period) {
                // if previous period was shortened, lengthen it again with the state that would result it bringing duty closer to desired duty
                auto shortenedBy = m_period - previousPeriod;
                previousPeriod = m_period;
                if (previousHighTime < m_dutyTime) {
                    previousHighTime = std::min(previousHighTime + shortenedBy, m_dutyTime);
                }
            }
        }

        auto twoPeriodElapsed = previousPeriod + currentPeriod;
        auto twoPeriodHighTime = previousHighTime + currentHighTime;

        if (lastHistoricState == State::Active) {
            if (m_dutyTime == m_period) { // 100%
                // ensure desired state is correct and get time from possibly blocked actuator
                auto actWait = actPtr->desiredState(State::Active, now);
                if (currentPeriod + 1000 <= m_period) {
                    wait = m_period - currentPeriod;
                } else {
                    wait = 1000;
                }
                wait = std::max(actWait, wait);
            } else if (2 * m_dutyTime <= m_period) {
                // high period is fixed, low period adapts
                if (currentHighTime < m_dutyTime) {
                    wait = m_dutyTime - currentHighTime;
                }
            } else {
                // for checking the currently achieved value, for this cycle so far and the previous

                // high period can adapt between boundaries
                // maximum high time is the highest value among:
                // - 1.5x the normal high time
                // - 1.5x the previous high time, but not more than 3x the normal high time
                // minimum high time is 75% of normal high time

                // use unadjusted time to calculate max time
                auto minHighTime = m_dutyTime - (m_dutyTime >> 2);
                if (currentHighTime < minHighTime) {
                    wait = minHighTime - currentHighTime;
                } else {
                    auto maxHighTime = std::max(std::max(m_dutyTime, durations.previousActive), (3 * m_dutyTime) >> 2);
                    if (durations.previousPeriod >= m_period) {
                        maxHighTime += maxHighTime / 2; // stretching allowed if previous period was not shortened
                    }

                    if (currentHighTime < maxHighTime) {
                        // for checking the currently achieved value, look back max 2 periods (toggles)
                        auto twoPeriodTargetHighTime = duration_millis_t(twoPeriodElapsed * dutyFraction());

                        // make sure that periods following each other do not continuously alternate in shortend/stretched cycle
                        // by converging to the mean or unadjusted, whichever is higher
                        auto mean = std::max(m_dutyTime, twoPeriodTargetHighTime / 2);
                        if (currentHighTime > mean && previousHighTime < mean) {
                            twoPeriodTargetHighTime -= (currentHighTime - previousHighTime) / 4;
                        }

                        if (twoPeriodHighTime < twoPeriodTargetHighTime) {
                            wait = std::min(twoPeriodTargetHighTime - twoPeriodHighTime, maxHighTime - currentHighTime);
                        }
                    }
                }
            }
        } else if (lastHistoricState == State::Inactive) {
            auto currentLowTime = currentPeriod - currentHighTime;
            if (m_dutyTime == 0) {
                // ensure desired state is correct and get time from possibly blocked actuator
                auto actWait = actPtr->desiredState(State::Inactive, now);
                if (currentPeriod + 1000 <= m_period) {
                    wait = m_period - currentPeriod;
                } else {
                    wait = 1000;
                }
                wait = std::max(actWait, wait);
            } else if (2 * m_dutyTime > m_period) {
                // low period is fixed, high period adapts
                if (currentLowTime < invDutyTime) {
                    wait = invDutyTime - currentLowTime;
                }
            } else {
                // low period can adapt between boundaries
                // maximum low time is the highest value among:
                // - 1.5x the normal low time
                // - 1.5x the previous low time, but not more than 3x the normal low time
                // minimum low time is 75% of normal low time

                auto minLowTime = invDutyTime - (invDutyTime >> 2);
                if (currentLowTime < minLowTime) {
                    wait = minLowTime - currentLowTime;
                } else {
                    auto maxLowTime = std::max(std::max(invDutyTime, durations.previousPeriod - durations.previousActive), (3 * invDutyTime) >> 2);

                    if (durations.previousPeriod >= m_period) {
                        maxLowTime += maxLowTime / 2; // stretching only allowed if previous period was not shortened
                    }

                    if (currentLowTime < maxLowTime) {
                        // for checking the currently achieved value, look back max 2 periods (toggles)
                        auto twoPeriodTargetLowTime = twoPeriodElapsed - duration_millis_t(dutyFraction() * twoPeriodElapsed);
                        auto previousLowTime = previousPeriod - previousHighTime;

                        // make sure that periods following each other do not continuously alternate in shortend/stretched cycle
                        // by converging to the mean or the unadjusted time, whichever is higher
                        auto mean = std::max(invDutyTime, twoPeriodTargetLowTime / 2);
                        if (currentLowTime > mean && previousLowTime < mean) {
                            twoPeriodTargetLowTime -= (currentLowTime - previousLowTime) / 4;
                        }
                        auto twoPeriodLowTime = twoPeriodElapsed - twoPeriodHighTime;
                        if (twoPeriodLowTime < twoPeriodTargetLowTime) {
                            wait = std::min(twoPeriodTargetLowTime - twoPeriodLowTime, maxLowTime - currentLowTime);
                        }
                    }
                }
            }
        }

        // Toggle actuator if necessary
        if (m_enabled && m_settingValid && (wait == 0)) {
            // if actuator is blocked, update wait time to prevent useless pwm updates
            if (lastHistoricState == State::Inactive) {
                wait = actPtr->desiredState(State::Active, now);
            } else {
                wait = actPtr->desiredState(State::Inactive, now);
            }
            // update state in case we toggled
            lastHistoricState = actPtr->state();
        }

        // take into account period until next update in calculating achieved
        auto twoPeriodTotal = twoPeriodElapsed + wait;
        if (lastHistoricState == State::Active) {
            twoPeriodHighTime += wait;
        }

        auto dutyAchieved = value_t{cnl::quotient(100 * twoPeriodHighTime, twoPeriodTotal)};
        // calculate achieved duty cycle
        // only update achieved if current cycle is long enough
        // to let current state move achieved in the right direction
        m_valueValid = true;

        if (lastHistoricState == State::Active) {
            if (dutyAchieved >= m_dutyAchieved) {
                m_dutyAchieved = dutyAchieved;
            }
        } else if (lastHistoricState == State::Inactive) {
            if (dutyAchieved <= m_dutyAchieved) {
                m_dutyAchieved = dutyAchieved;
            }
        } else {
            m_valueValid = false;
            m_dutyAchieved = m_dutySetting;
        }

        return now + std::min(update_t(1000), (wait >> 1) + 1);
    }
    return now + 1000;
}

bool
ActuatorPwm::valueValid() const
{
    return m_valueValid;
}

bool
ActuatorPwm::settingValid() const
{
    return m_settingValid;
}

void
ActuatorPwm::settingValid(bool v)
{
    if (!v && m_enabled) {
        if (auto actPtr = m_target()) {
            actPtr->desiredState(State::Inactive);
        }
    }
    m_settingValid = v;
}
