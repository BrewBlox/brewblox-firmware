/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of Controlbox
 *
 * Controlbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Controlbox.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SysInfoBlock.h"
#include "deviceid_hal.h"
#include "stringify.h"
#include <cstring>

#ifndef GIT_VERSION
#error GIT_VERSION not set
#endif

cbox::CboxError
SysInfoBlock::streamTo(cbox::DataOut& out) const
{
    blox_SysInfo message = blox_SysInfo_init_zero;

    HAL_device_ID(static_cast<uint8_t*>(&message.deviceId[0]), 12);

    strncpy(message.version, stringify(GIT_VERSION), 12);
    strncpy(message.protocolVersion, stringify(PROTO_VERSION), 12);
    strncpy(message.releaseDate, stringify(GIT_DATE), 12);
    strncpy(message.protocolDate, stringify(PROTO_DATE), 12);

    message.platform = blox_SysInfo_Platform(PLATFORM_ID);

    return streamProtoTo(out, &message, blox_SysInfo_fields, blox_SysInfo_size);
}

cbox::CboxError
SysInfoBlock::streamFrom(cbox::DataIn&)
{
    return cbox::CboxError::OK;
}

cbox::CboxError
SysInfoBlock::streamPersistedTo(cbox::DataOut&) const
{
    return cbox::CboxError::OK;
}