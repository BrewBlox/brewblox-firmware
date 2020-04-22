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

#include "./reset.h"
#include "AppTicks.h"
#include "Board.h"
#include "Logger.h"
#include "OneWireScanningFactory.h"
#include "blox/ActuatorAnalogMockBlock.h"
#include "blox/ActuatorLogicBlock.h"
#include "blox/ActuatorOffsetBlock.h"
#include "blox/ActuatorPwmBlock.h"
#include "blox/BalancerBlock.h"
#include "blox/DS2408Block.h"
#include "blox/DS2413Block.h"
#include "blox/DigitalActuatorBlock.h"
#include "blox/DisplaySettingsBlock.h"
#include "blox/MockPinsBlock.h"
#include "blox/MotorValveBlock.h"
#include "blox/MutexBlock.h"
#include "blox/OneWireBusBlock.h"
#include "blox/PidBlock.h"
#include "blox/SetpointProfileBlock.h"
#include "blox/SetpointSensorPairBlock.h"
#include "blox/SysInfoBlock.h"
#include "blox/TempSensorMockBlock.h"
#include "blox/TempSensorOneWireBlock.h"
#include "blox/TouchSettingsBlock.h"
#include "blox/WiFiSettingsBlock.h"
#include "blox/stringify.h"
#include "cbox/Box.h"
#include "cbox/Connections.h"
#include "cbox/EepromObjectStorage.h"
#include "cbox/ObjectContainer.h"
#include "cbox/ObjectFactory.h"
#include "cbox/spark/SparkEepromAccess.h"
#include "deviceid_hal.h"
#include "platforms.h"
#include <memory>

using EepromAccessImpl = cbox::SparkEepromAccess;

#if defined(SPARK)
#include "rgbled.h"
void
changeLedColor()
{
    LED_SetRGBColor(RGB_COLOR_MAGENTA);
}
extern void
updateFirmwareFromStream(cbox::StreamType streamType);
#else
void
changeLedColor()
{
}
void updateFirmwareFromStream(cbox::StreamType)
{
}
#endif

// Include OneWire implementation depending on platform
#if !defined(PLATFORM_ID) || PLATFORM_ID == 3
#include "OneWireNull.h"
#include "test/MockOneWireScanningFactory.h"
using OneWireDriver = OneWireNull;
#define ONEWIRE_ARG
#else
#include "DS248x.h"
#include "OneWireScanningFactory.h"
using OneWireDriver = DS248x;
#define ONEWIRE_ARG 0x00
#endif

// Include serial connection for platform
#if defined(SPARK)
#if PLATFORM_ID != 3 || defined(STDIN_SERIAL)
#include "cbox/spark/ConnectionsSerial.h"
#endif
#include "cbox/spark/ConnectionsTcp.h"
#else
#include "cbox/ConnectionsStringStream.h"

cbox::StringStreamConnectionSource&
testConnectionSource()
{
    static cbox::StringStreamConnectionSource connSource;
    return connSource;
}
#endif

#if PLATFORM_ID == 6
#include "blox/Spark2PinsBlock.h"
using PinsBlock = Spark2PinsBlock;
#else
#include "blox/Spark3PinsBlock.h"
using PinsBlock = Spark3PinsBlock;
#endif

cbox::ConnectionPool&
theConnectionPool()
{
#if defined(SPARK)
    static cbox::TcpConnectionSource tcpSource(8332);
#if PLATFORM_ID != 3 || defined(STDIN_SERIAL)
    static auto& boxSerial = _fetch_usbserial();
    static cbox::SerialConnectionSource serialSource(boxSerial);
    static cbox::ConnectionPool connections = {tcpSource, serialSource};
#else
    static cbox::ConnectionPool connections = {tcpSource};
#endif
#else
    static cbox::ConnectionPool connections = {testConnectionSource()};
#endif

    return connections;
}

cbox::Box&
makeBrewBloxBox()
{
    static cbox::ObjectContainer objects({
        // groups will be at position 1
        cbox::ContainedObject(2, 0x80, std::make_shared<SysInfoBlock>()),
            cbox::ContainedObject(3, 0x80, std::make_shared<TicksBlock<TicksClass>>(ticks)),
            cbox::ContainedObject(4, 0x80, std::make_shared<OneWireBusBlock>(theOneWire())),
#if defined(SPARK)
            cbox::ContainedObject(5, 0x80, std::make_shared<WiFiSettingsBlock>()),
            cbox::ContainedObject(6, 0x80, std::make_shared<TouchSettingsBlock>()),
#endif
            cbox::ContainedObject(7, 0x80, std::make_shared<DisplaySettingsBlock>()),
            cbox::ContainedObject(19, 0x80, std::make_shared<PinsBlock>()),
    });

    static const cbox::ObjectFactory objectFactory{
        {TempSensorOneWireBlock::staticTypeId(), std::make_shared<TempSensorOneWireBlock>},
        {SetpointSensorPairBlock::staticTypeId(), []() { return std::make_shared<SetpointSensorPairBlock>(objects); }},
        {TempSensorMockBlock::staticTypeId(), std::make_shared<TempSensorMockBlock>},
        {ActuatorAnalogMockBlock::staticTypeId(), []() { return std::make_shared<ActuatorAnalogMockBlock>(objects); }},
        {PidBlock::staticTypeId(), []() { return std::make_shared<PidBlock>(objects); }},
        {ActuatorPwmBlock::staticTypeId(), []() { return std::make_shared<ActuatorPwmBlock>(objects); }},
        {ActuatorOffsetBlock::staticTypeId(), []() { return std::make_shared<ActuatorOffsetBlock>(objects); }},
        {BalancerBlock::staticTypeId(), std::make_shared<BalancerBlock>},
        {MutexBlock::staticTypeId(), std::make_shared<MutexBlock>},
        {SetpointProfileBlock::staticTypeId(), []() { return std::make_shared<SetpointProfileBlock>(objects); }},
        {DS2413Block::staticTypeId(), std::make_shared<DS2413Block>},
        {DigitalActuatorBlock::staticTypeId(), []() { return std::make_shared<DigitalActuatorBlock>(objects); }},
        {DS2408Block::staticTypeId(), std::make_shared<DS2408Block>},
        {MotorValveBlock::staticTypeId(), []() { return std::make_shared<MotorValveBlock>(objects); }},
        {ActuatorLogicBlock::staticTypeId(), []() { return std::make_shared<ActuatorLogicBlock>(objects); }},
        {MockPinsBlock::staticTypeId(), []() { return std::make_shared<MockPinsBlock>(); }},
    };

    static EepromAccessImpl eeprom;
    static cbox::EepromObjectStorage objectStore(eeprom);
    static cbox::ConnectionPool& connections = theConnectionPool();

    std::vector<std::unique_ptr<cbox::ScanningFactory>> scanningFactories;
    scanningFactories.reserve(1);
#if PLATFORM_ID == 3
    scanningFactories.push_back(std::make_unique<MockOneWireScanningFactory>(objects, theOneWire()));
#else
    scanningFactories.push_back(std::make_unique<OneWireScanningFactory>(objects, theOneWire()));
#endif

    static cbox::Box box(objectFactory, objects, objectStore, connections, std::move(scanningFactories));

    return box;
}

cbox::Box&
brewbloxBox()
{
    static cbox::Box& box = makeBrewBloxBox();
    return box;
}

OneWire&
theOneWire()
{
    static auto owDriver = OneWireDriver(ONEWIRE_ARG);
    static auto ow = OneWire(owDriver);
    return ow;
}

Logger&
logger()
{
    static Logger logger([](Logger::LogLevel level, const std::string& log) {
        cbox::DataOut& out = theConnectionPool().logDataOut();
        out.write('<');
        const char debug[] = "DEBUG";
        const char info[] = "INFO";
        const char warn[] = "WARNING";
        const char err[] = "ERROR";

        switch (level) {
        case Logger::LogLevel::DEBUG:
            out.writeBuffer(debug, strlen(debug));
            break;
        case Logger::LogLevel::INFO:
            out.writeBuffer(info, strlen(info));
            break;
        case Logger::LogLevel::WARN:
            out.writeBuffer(warn, strlen(warn));
            break;
        case Logger::LogLevel::ERROR:
            out.writeBuffer(err, strlen(err));
            break;
        }
        out.write(':');
        for (const auto& c : log) {
            out.write(c);
        }
        out.write('>');
    });
    return logger;
}

void
logEvent(const std::string& event)
{
    cbox::DataOut& out = theConnectionPool().logDataOut();
    out.write('<');
    out.write('!');
    for (const auto& c : event) {
        out.write(c);
    }
    out.write('>');
}

void
updateBrewbloxBox()
{
    brewbloxBox().update(ticks.millis());
#if PLATFORM_ID == 3
    ticks.delayMillis(10); // prevent 100% cpu usage
#endif
}

const char*
versionCsv()
{
#if PLATFORM_ID == 3
#define PLATFORM_STRING "gcc"
#elif PLATFORM_ID == 6
#define PLATFORM_STRING "photon"
#elif PLATFORM_ID == 8
#define PLATFORM_STRING "p1"
#else
#define PLATFORM_STRING "unkown"
#endif

    static const char version[] = stringify(GIT_VERSION) "," stringify(PROTO_VERSION) "," stringify(GIT_DATE) "," stringify(PROTO_DATE) "," stringify(SYSTEM_VERSION_STRING) "," PLATFORM_STRING;
    return version;
}

namespace cbox {
void
connectionStarted(DataOut& out)
{
    char header[] = "<!BREWBLOX,";

    out.writeBuffer(header, strlen(header));
    out.writeBuffer(versionCsv(), strlen(versionCsv()));
    out.write(',');
    cbox::EncodedDataOut hexOut(out);

#if PLATFORM_ID == 3
    int resetReason = 0;
#else
    auto resetReason = System.resetReason();
#endif
    hexOut.write(resetReason);
    out.write(',');
#if PLATFORM_ID == 3
    int resetData = 0;
#else
    auto resetData = System.resetReasonData();
#endif
    hexOut.write(resetData);
    out.write(',');
    uint8_t deviceId[12];
    HAL_device_ID(deviceId, 12);
    hexOut.writeBuffer(deviceId, 12);
    out.write('>');
}

bool
applicationCommand(uint8_t cmdId, cbox::DataIn& in, cbox::EncodedDataOut& out)
{
    switch (cmdId) {
    case 100: // firmware update
    {
        CboxError status = CboxError::OK;
        in.spool();
        if (out.crc()) {
            status = CboxError::CRC_ERROR_IN_COMMAND;
        }
        out.writeResponseSeparator();
        out.write(asUint8(status));
        out.endMessage();
        if (status == CboxError::OK) {
            changeLedColor();
            brewbloxBox().stopConnections();
            updateFirmwareFromStream(in.streamType());
            uint8_t reason = uint8_t(RESET_USER_REASON::FIRMWARE_UPDATE_FAILED);
            handleReset(true, reason); // reset in case the firmware update failed
        }
        return true;
    }
    default:
        return false;
    }
}

} // end namespace cbox