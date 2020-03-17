/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of the BrewBlox Control Library.
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

#pragma once

#include "ActuatorAnalog.h"
#include "ActuatorAnalogConstrained.h"
#include <algorithm>
#include <functional>
#include <vector>

namespace AAConstraints {
template <uint8_t ID>
class Balanced;
}

template <uint8_t ID>
class Balancer {
private:
    using value_t = ActuatorAnalog::value_t;
    using Balanced = AAConstraints::Balanced<ID>;

    struct Request {
        uint8_t id;
        value_t requested;
        value_t granted;
    };

    const value_t available = 100;

    std::vector<Request> requesters;

public:
    Balancer() = default;
    virtual ~Balancer() = default;

    uint8_t registerEntry()
    {
        for (uint8_t id = 1; id < 255; id++) {
            // find free id
            auto match = find_if(requesters.begin(), requesters.end(), [&id](const Request& r) {
                return r.id == id;
            });

            if (match == requesters.end()) {
                requesters.push_back(Request{id, available, 0});
                return id;
            }
        };
        return 0;
    }

    void unregisterEntry(const uint8_t& requester_id)
    {
        requesters.erase(std::remove_if(requesters.begin(), requesters.end(), [&requester_id](const Request& r) { return r.id == requester_id; }),
                         requesters.end());
    }

    value_t constrain(uint8_t& requester_id, const value_t& val)
    {
        auto match = find_if(requesters.begin(), requesters.end(), [&requester_id](const Request& r) {
            return r.id == requester_id;
        });

        if (match != requesters.end()) {
            match->requested = val;
            return std::min(val, match->granted);
        };

        // not found. Could happen is actuator was created before the balancer.
        // assign new requester id
        requester_id = registerEntry();
        return 0;
    }

    value_t granted(const uint8_t& requester_id) const
    {
        auto match = find_if(requesters.begin(), requesters.end(), [&requester_id](const Request& r) {
            return r.id == requester_id;
        });

        if (match == requesters.end()) {
            return 0;
        }

        return match->granted;
    }

    void update()
    {
        int8_t numActuators = requesters.size(); // signed, because value_t is signed too
        if (numActuators == 0) {
            return;
        }

        auto requestedTotal = value_t(0);
        for (const auto& a : requesters) {
            requestedTotal += a.requested;
        }
        auto budgetLeft = value_t(0);
        if (available > requestedTotal) {
            budgetLeft = available - requestedTotal;
            requestedTotal = available;
        }
        auto budgetLeftPerActuator = value_t(cnl::quotient(budgetLeft, numActuators));

        safe_elastic_fixed_point<10, 21> scale = cnl::quotient(available, requestedTotal);

        for (auto& a : requesters) {
            a.granted = a.requested * scale;
            a.granted += budgetLeftPerActuator;
        }
    }

    const std::vector<Request>& clients() const
    {
        return requesters;
    }
};

namespace AAConstraints {

template <uint8_t ID>
class Balanced : public Base {
private:
    const std::function<std::shared_ptr<Balancer<ID>>()> m_balancer;
    mutable uint8_t m_req_id; // can be updated by balancer in request

public:
    explicit Balanced(
        std::function<std::shared_ptr<Balancer<ID>>()>&& balancer)
        : m_balancer(balancer)
    {
        if (auto balancerPtr = m_balancer()) {
            m_req_id = balancerPtr->registerEntry();
        }
    }

    Balanced(const Balanced&) = delete;
    Balanced& operator=(const Balanced&) = delete;
    Balanced(Balanced&&) = default;
    Balanced& operator=(Balanced&&) = default;

    ~Balanced()
    {
        if (auto balancerPtr = m_balancer()) {
            balancerPtr->unregisterEntry(m_req_id);
        }
    }

    virtual value_t constrain(const value_t& val) const override final
    {
        if (auto balancerPtr = m_balancer()) {
            return balancerPtr->constrain(m_req_id, val);
        }
        return val;
    }

    virtual uint8_t id() const override final
    {
        return ID;
    }

    uint8_t requesterId() const
    {
        return m_req_id;
    }

    value_t granted() const
    {
        if (auto balancerPtr = m_balancer()) {
            return balancerPtr->granted(m_req_id);
        }
        return 0;
    }
    virtual uint8_t order() const override final
    {
        return 2;
    }
};
}