/*
 * Copyright 2013 BrewPi/Elco Jacobs.
 * Copyright 2013 Matthew McGowan
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

#pragma once

#include <functional>
#include <memory>

/*
 * A digital actuator that toggles a channel of an ArrayIo object.
 *
 */
class ActuatorDigitalBase {
public:
    enum State : uint8_t {
        Inactive = 0,
        Active = 1,
        Unknown = 2,
    };

    static State invertState(State s)
    {
        switch (s) {
        case State::Active:
            return State::Inactive;
        case State::Inactive:
            return State::Active;
        case State::Unknown:
            return State::Unknown;
        default:
            return State::Unknown;
        }
    }

public:
    ActuatorDigitalBase() = default;

protected:
    ~ActuatorDigitalBase() = default;

public:
    virtual void state(const State& v) = 0;

    virtual State state() const = 0;

    virtual bool supportsFastIo() const = 0;
};
