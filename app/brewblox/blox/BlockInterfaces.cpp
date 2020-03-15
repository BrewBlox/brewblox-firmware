/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of BrewBlox
 *
 * BrewBlox is free software: you can redistribute it and/or modify
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
 * along with BrewBlox.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ActuatorAnalogConstrained.h"
#include "ActuatorDigitalConstrained.h"
#include "AppTicks.h"
#include "Balancer.h"
#include "DS2408.h"
#include "DigitalConstraints.pb.h"
#include "FixedPoint.h"
#include "IoArray.h"
#include "OneWireDevice.h"
#include "ProcessValue.h"
#include "SetpointSensorPair.h"
#include "TempSensor.h"
#include "brewblox.pb.h"
#include "cbox/ObjectBase.h"

namespace cbox {

template <>
const obj_type_t
interfaceIdImpl<ProcessValue<fp12_t>>()
{
    return BrewBloxTypes_BlockType_ProcessValueInterface;
}

template <>
const obj_type_t
interfaceIdImpl<TempSensor>()
{
    return BrewBloxTypes_BlockType_TempSensorInterface;
}

template <>
const obj_type_t
interfaceIdImpl<SetpointSensorPair>()
{
    return BrewBloxTypes_BlockType_SetpointSensorPairInterface;
}

template <>
const obj_type_t
interfaceIdImpl<ActuatorAnalogConstrained>()
{
    return BrewBloxTypes_BlockType_ActuatorAnalogInterface;
}

template <>
const obj_type_t
interfaceIdImpl<ActuatorDigitalConstrained>()
{
    return BrewBloxTypes_BlockType_ActuatorDigitalInterface;
}

template <>
const obj_type_t
interfaceIdImpl<MutexTarget>()
{
    return BrewBloxTypes_BlockType_MutexInterface;
}

template <>
const obj_type_t
interfaceIdImpl<Balancer<blox_DigitalConstraint_mutex_tag>>()
{
    return BrewBloxTypes_BlockType_BalancerInterface;
}

template <>
const obj_type_t
interfaceIdImpl<OneWireDevice>()
{
    return BrewBloxTypes_BlockType_OneWireDeviceInterface;
}

template <>
const obj_type_t
interfaceIdImpl<IoArray>()
{
    return BrewBloxTypes_BlockType_IoArrayInterface;
}

template <>
const obj_type_t
interfaceIdImpl<DS2408>()
{
    return BrewBloxTypes_BlockType_DS2408Interface;
}

} // end namespace cbox
