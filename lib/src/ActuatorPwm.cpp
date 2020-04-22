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
    if (auto actPtr = m_target()) {
        if (actPtr->state() != State::Active) {
            if (m_fastPwmElapsed < m_dutyTime) {
                actPtr->setStateUnlogged(State::Active);
            }
            if (m_fastPwmElapsed == 1) {
                m_dutyAchieved = value_t{0}; // never active in previous cycle
            }
        } else {
            if (m_fastPwmElapsed >= m_dutyTime) {
                actPtr->setStateUnlogged(State::Inactive);
                m_dutyAchieved = m_fastPwmElapsed;
            } else {
                if (m_fastPwmElapsed == 99) {
                    m_dutyAchieved = maxDuty(); // never inactive in this cycle
                }
            }
        }
    }
    m_fastPwmElapsed = (m_fastPwmElapsed + 1) % 100;
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
        auto twoPeriodElapsed = previousPeriod + currentPeriod;
        auto twoPeriodHighTime = previousHighTime + currentHighTime;

        auto wait = duration_millis_t(0);

        if (lastHistoricState == State::Active) {
            if (m_dutySetting == maxDuty()) {
                m_dutyAchieved = maxDuty();
                actPtr->desiredState(State::Active, now); // ensure desired state is correct
                return now + 1000;
            }

            if (m_dutySetting <= (maxDuty() >> 1)) {
                // high period is fixed, low period adapts
                if (currentHighTime < m_dutyTime) {
                    wait = m_dutyTime - currentHighTime;
                }
            } else {
                // for checking the currently achieved value, for this cycle so far and the previous

                // high period can adapt between boundaries
                // maximum high time is the highest value among:
                // - 1.5x the previous hight time
                // - 1.5x the normal high time

                auto maxHighTime = std::max(m_dutyTime, previousHighTime);
                if (previousPeriod >= m_period) {
                    maxHighTime = maxHighTime * 3 / 2; // stretching allowed if previous period was not shortened
                }

                if (currentHighTime < maxHighTime) {
                    // for checking the currently achieved value, look back max 2 periods (toggles)
                    auto twoPeriodTargetHighTime = duration_millis_t(twoPeriodElapsed * dutyFraction());

                    // if previous high time is twice the unadjusted high time
                    // use at least normal high time, because this is a due to a big duty setting decrease
                    // not a slight adjustment for contraints or jitter
                    if (previousHighTime > invDutyTime) {
                        twoPeriodTargetHighTime = std::max(twoPeriodTargetHighTime, previousHighTime + invDutyTime);
                    }

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
        } else if (lastHistoricState == State::Inactive) {
            if (m_dutySetting == value_t{0}) {
                m_dutyAchieved = value_t{0};
                actPtr->desiredState(State::Inactive, now); // ensure desired state is correct
                return now + 1000;
            }
            auto currentLowTime = currentPeriod - currentHighTime;

            if (m_dutySetting > (maxDuty() >> 1)) {
                // low period is fixed, high period adapts
                if (currentLowTime < invDutyTime) {
                    wait = invDutyTime - currentLowTime;
                }
            } else {
                // low period can adapt between boundaries
                // maximum low time is the highest value among:
                // - 1.5x the previous low time
                // - 1.5x the normal low time

                auto previousLowTime = previousPeriod - previousHighTime;
                auto maxLowTime = std::max(invDutyTime, previousLowTime);
                if (previousPeriod >= m_period) {
                    maxLowTime = maxLowTime * 3 / 2; // stretching allowed if previous period was not shortened
                }

                if (currentLowTime < maxLowTime) {
                    // for checking the currently achieved value, look back max 2 periods (toggles)
                    auto twoPeriodTargetLowTime = twoPeriodElapsed - duration_millis_t(dutyFraction() * twoPeriodElapsed);

                    // if previous low time is twice the unadjusted low time
                    // use at least normal low time, because this is a due to a big duty setting increase
                    // not a slight adjustment for contraints or jitter
                    if (previousLowTime > invDutyTime * 2) {
                        twoPeriodTargetLowTime = std::max(twoPeriodTargetLowTime, previousLowTime + invDutyTime);
                    }
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

        // take into account period until next update in calculating achieved
        auto twoPeriodTotal = twoPeriodElapsed + wait;

        if (twoPeriodTotal < m_period * 2) {
            // always calculate duty from at least 2 normal periods
            // this prevents shortened cycle lengths to cause a higher duty to be reported than the setting
            twoPeriodTotal = m_period * 2;
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

        // Toggle actuator if necessary
        if (m_enabled && m_settingValid && (wait == 0)) {
            if (lastHistoricState == State::Inactive) {
                actPtr->desiredState(State::Active, now);
            } else {
                actPtr->desiredState(State::Inactive, now);
            }
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
