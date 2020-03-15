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

#include "cbox/DataStream.h"
#include "cbox/DataStreamConverters.h"
#include "cbox/DataStreamIo.h"
#include <sstream>

namespace google {
namespace protobuf {
    class Message;
}
}

class ProtoDataOut {
public:
    cbox::EncodedDataOut& out;
    ProtoDataOut(cbox::EncodedDataOut& target)
        : out(target)
    {
    }

    void put(const ::google::protobuf::Message& message);
};

void
decodeProtoFromReply(std::stringstream& ss, ::google::protobuf::Message& message);
