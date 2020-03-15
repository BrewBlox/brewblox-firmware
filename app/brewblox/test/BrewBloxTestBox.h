/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of BrewBlox.
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

#include "AppTicks.h"
#include "BrewBlox.h"
#include "blox/TicksBlock.h"
#include "cbox/Box.h"
#include "cbox/DataStream.h"
#include "cbox/DataStreamIo.h"
#include "testHelpers.h"
#include <memory>
#include <sstream>

class BrewBloxTestBox {
public:
    std::shared_ptr<std::stringstream> in;
    std::shared_ptr<std::stringstream> out;
    cbox::OStreamDataOut inOs;
    cbox::EncodedDataOut toHex;
    cbox::EncodedDataOut inEncoder;
    ProtoDataOut inProto;
    bool lastReplyOk = false;
    TicksClass& ticks;

    BrewBloxTestBox();
    ~BrewBloxTestBox() = default;

    void clearStreams();

    void reset();

    bool lastReplyHasStatusOk();

    std::string processInput();

    void processInputToProto(::google::protobuf::Message& message);

    template <typename T>
    void put(const T& t, typename std::enable_if_t<!std::is_base_of<::google::protobuf::Message, T>::value>* = 0)
    {
        inEncoder.put(t);
    }

    void put(const ::google::protobuf::Message& message);

    void endInput();

    void update(const cbox::update_t& now);
};
