#include "ActuatorPwm.h"
#include "future_std.h"
#include <cstdint>

#if PLATFORM_ID != PLATFORM_GCC
#include "TimerInterrupts.h"
#endif

ActuatorPwm::ActuatorPwm(
    std::function<std::shared_ptr<ActuatorDigitalChangeLogged>()>&& target_,
    duration_millis_t period_)
    : m_target(target_)
{
    period(period_);
}

void
ActuatorPwm::setting(value_t const& val)
{
    m_dutySetting = std::clamp(val, value_t(0), value_t(100));
    m_dutyTime = duration_millis_t((m_dutySetting * m_period) / value_t(100));
    settingValid(true);
}

// returns the actual achieved PWM value, not the set value
ActuatorPwm::value_t
ActuatorPwm::value() const
{
    return m_dutyAchieved;
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
            m_dutyAchieved = 0;
        }
    }
}
#endif

void
ActuatorPwm::period(const duration_millis_t& p)
{
    m_period = p;
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
                m_dutyAchieved = 0; // never active in previous cycle
            }
        } else {
            if (m_fastPwmElapsed >= m_dutyTime) {
                actPtr->setStateUnlogged(State::Inactive);
                m_dutyAchieved = m_fastPwmElapsed;
            } else {
                if (m_fastPwmElapsed == 99) {
                    m_dutyAchieved = 100; // never inactive in this cycle
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
        auto twoPeriodTotalTime = durations.previousPeriod + durations.currentPeriod;
        auto twoPeriodHighTime = durations.previousActive + durations.currentActive;
        auto twoPeriodLowTime = twoPeriodTotalTime - twoPeriodHighTime;
        auto currentHighTime = durations.currentActive;
        auto currentPeriod = durations.currentPeriod;
        auto wait = duration_millis_t(0);
        auto currentState = actPtr->state();

        if (currentState == State::Active) {
            if (m_dutySetting == value_t(100)) {
                m_dutyAchieved = 100;
                return now + 1000;
            }

            if (m_dutySetting <= value_t(50)) {
                // high period is fixed, low period adapts
                if (currentHighTime < m_dutyTime) {
                    wait = m_dutyTime - currentHighTime;
                }
            } else {
                // high period can adapt between boundaries

                // for checking the currently achieved value, for this cycle so far and the previous
                auto twoPeriodTargetHighTime = duration_millis_t(twoPeriodTotalTime * (m_dutySetting / 100));
                // maximum high time is 1.5x the previous high time or 1.5 the normal duty time, whichever is higher
                auto maxHighTime = std::max(m_dutyTime, durations.previousActive) * 3 / 2;

                // make sure that periods following each other do not alternate in high time
                // if the current period is already longer than the duty, diminish it by 25% of the extra time
                // This prevents alternating between 1500 and 2500 when the total of 2 periods should be 4000.
                if (currentHighTime > m_dutyTime && twoPeriodHighTime < 2 * m_dutyTime) {
                    twoPeriodTargetHighTime -= (currentHighTime - m_dutyTime) / 4;
                }

                if (currentHighTime < maxHighTime) {
                    if (twoPeriodHighTime < twoPeriodTargetHighTime) {
                        wait = std::min(twoPeriodTargetHighTime - twoPeriodHighTime, maxHighTime - currentHighTime);
                    }
                }
            }
            m_valueValid = true;
        } else if (currentState == State::Inactive) {
            if (m_dutySetting == value_t(0)) {
                m_dutyAchieved = 0;
                return now + 1000;
            }
            // for checking the currently achieved value, look back max 2 periods (toggles)
            auto invDutyTime = m_period - m_dutyTime;
            auto twoPeriodTargetLowTime = duration_millis_t(twoPeriodTotalTime * ((value_t(100) - m_dutySetting) / 100));

            auto thisPeriodLowTime = currentPeriod - currentHighTime;

            if (m_dutySetting > value_t(50)) {
                // low period is fixed, high period adapts
                if (thisPeriodLowTime < invDutyTime) {
                    wait = invDutyTime - thisPeriodLowTime;
                }
            } else {
                // low period can adapt between boundaries
                auto previousLowTime = twoPeriodLowTime - thisPeriodLowTime;
                // maximum low time is 1.5 the previous low time, or 1.5 the normal time whichever is higher
                auto maxLowTime = std::max(invDutyTime, previousLowTime) * 3 / 2;

                // make sure that periods following each other do not alternate in low time
                // if the current period is already longer than the invDuty, diminish it by 33% of the extra time
                // This prevents alternating between 1500 and 2500 when the total of 2 periods should be 4000.
                if (thisPeriodLowTime > invDutyTime && twoPeriodLowTime < 2 * invDutyTime) {
                    twoPeriodTargetLowTime -= (thisPeriodLowTime - invDutyTime) / 3;
                }

                if (thisPeriodLowTime < maxLowTime) {
                    if (twoPeriodLowTime < twoPeriodTargetLowTime) {
                        wait = std::min(twoPeriodTargetLowTime - twoPeriodLowTime, maxLowTime - thisPeriodLowTime);
                    }
                }
            }
            m_valueValid = true;
        } else {
            m_valueValid = false;
        }

        // calculate achieved duty cycle
        twoPeriodTotalTime += wait;
        if (twoPeriodTotalTime == 0) {
            m_dutyAchieved = 0;
        } else {
            auto dutyAchieved = (value_t(100) * value_t::rep(twoPeriodHighTime)) / value_t::rep(twoPeriodTotalTime);
            if (wait == 0) {
                // end of high or low time
                m_dutyAchieved = dutyAchieved;
            } else if ((currentState == State::Inactive && dutyAchieved < m_dutyAchieved)
                       || (currentState == State::Active && dutyAchieved > m_dutyAchieved)) {
                // current period is long enough to start using the current achieved value including this period
                m_dutyAchieved = dutyAchieved;
            }
        }

        // Toggle actuator if necessary
        if (m_enabled && m_settingValid && wait == 0) {
            if (currentState == State::Inactive) {
                actPtr->state(State::Active, now);
            } else {
                actPtr->state(State::Inactive, now);
            }
        }

        return now + std::min(update_t(1000), wait >> 1);
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
            actPtr->state(State::Inactive);
        }
    }
    m_settingValid = v;
}
