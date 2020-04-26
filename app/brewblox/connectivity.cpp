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

#include "connectivity.h"
#include "Board.h"
#include "BrewBlox.h"
#include "MDNS.h"
#include "deviceid_hal.h"
#include "spark_wiring_tcpclient.h"
#include "spark_wiring_tcpserver.h"
#include "spark_wiring_usbserial.h"
#include "spark_wiring_wifi.h"
#include <cstdio>
volatile uint32_t localIp = 0;
volatile bool wifiIsConnected = false;

volatile bool mdns_started = false;
volatile bool http_started = false;

constexpr uint16_t webPort = PLATFORM_ID == PLATFORM_GCC ? 8380 : 80;
static TCPServer httpserver(webPort); // Serve a simple page with instructions

void
printWiFiIp(char dest[16])
{
    IPAddress ip = localIp;
    snprintf(dest, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

int8_t
wifiSignal()
{
    if (!wifiIsConnected) {
        return 2;
    }

    wlan_connected_info_t info = {0};
    info.size = sizeof(info);
    int r = wlan_connected_info(nullptr, &info, nullptr);
    if (r == 0) {
        return info.rssi != std::numeric_limits<int32_t>::min() ? info.rssi / 100 : 2;
    }
    return 2;
}

bool
serialConnected()
{
    return HAL_USB_USART_Is_Connected(HAL_USB_USART_SERIAL);
}

bool
setWifiCredentials(const char* ssid, const char* password, uint8_t security, uint8_t cipher)
{
    return spark::WiFi.setCredentials(ssid, password, security, cipher);
};

void
printWifiSSID(char* dest, const uint8_t& maxLen)
{
    if (wifiIsConnected) {
        strncpy(dest, spark::WiFi.SSID(), maxLen);
    } else {
        dest[0] = 0;
    }
}

bool
wifiConnected()
{
    return wifiIsConnected;
}

bool
listeningModeEnabled()
{
    return spark::WiFi.listening();
}

inline uint8_t
d2h(uint8_t bin)
{
    return uint8_t(bin + (bin > 9 ? 'A' - 10 : '0'));
}

std::string
deviceIdStringInit()
{
    std::string hex;
    hex.reserve(25);
    uint8_t id[12];
    HAL_device_ID(id, 12);
    for (uint8_t i = 0; i < 12; i++) {
        hex.push_back(d2h(uint8_t(id[i] & 0xF0) >> 4));
        hex.push_back(d2h(uint8_t(id[i] & 0xF)));
    }
    return hex;
}

const std::string&
deviceIdString()
{
    static auto hexId = deviceIdStringInit();
    return hexId;
}

MDNS&
theMdns()
{
    static MDNS* theStaticMDNS = new MDNS(deviceIdString());
    return *theStaticMDNS;
}

void
manageConnections(uint32_t now)
{
    static uint32_t lastConnect = 0;
    static uint32_t lastAnnounce = 0;
    if (spark::WiFi.ready()) {
        if ((!mdns_started) || ((now - lastAnnounce) > 300000)) {
            // explicit announce every 5 minutes
            mdns_started = theMdns().begin(true);
            lastAnnounce = now;
        }
        if (!http_started) {
            http_started = httpserver.begin();
        }

        if (mdns_started) {
            theMdns().processQueries();
        }
        if (http_started) {
            TCPClient client = httpserver.available();
            if (client) {
                client.setTimeout(100);
                while (client.read() != -1) {
                }
                const uint8_t start[] = "HTTP/1.1 200 Ok\n\n<html><body>"
                                        "<p>Your BrewBlox Spark is online but it does not run its own web server. "
                                        "Please install a BrewBlox server to connect to it using the BrewBlox protocol.</p>"
                                        "<p>Device ID = ";
                const uint8_t end[] = "</p></body></html>\n\n";

                client.write(start, sizeof(start), 0);
                if (!client.getWriteError()) {
                    client.write(reinterpret_cast<const uint8_t*>(deviceIdString().data()), 24, 0);
                }
                if (!client.getWriteError()) {
                    client.write(end, sizeof(end), 0);
                }

                if (!client.getWriteError()) {
                    client.flush();
                }
                HAL_Delay_Milliseconds(5);
                client.stop();
            }
            return;
        }
    } else {
        httpserver.stop();
        // mdns.stop();
        mdns_started = false;
        http_started = false;
    }
    if (now - lastConnect > 60000) {
        // after 60 seconds without WiFi, trigger reconnect
        // wifi is expected to reconnect automatically. This is a failsafe in case it does not
        if (!spark::WiFi.connecting()) {
            spark::WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);
        }
        lastConnect = now;
    }
}

void
initMdns()
{
    MDNS& mdns = theMdns();
    mdns.addService(MDNS::Protocol::TCP, "_http", deviceIdString(), 80);

    std::string hwEntry("HW=Spark ");
    switch (getSparkVersion()) {
    case SparkVersion::V1:
        hwEntry += "1";
        break;
    case SparkVersion::V2:
        hwEntry += "2";
        break;
    case SparkVersion::V3:
        hwEntry += "3";
        break;
    }

    mdns.addService(MDNS::Protocol::TCP, "_brewblox", deviceIdString(), 8332,
                    {"VERSION=" stringify(GIT_VERSION),
                     std::string("ID=") + deviceIdString(),
                     "PLATFORM=" stringify(PLATFORM_ID),
                     hwEntry});
}

void
handleNetworkEvent(system_event_t event, int param)
{
    switch (param) {
    case network_status_connected: {
        IPAddress ip = spark::WiFi.localIP();
        localIp = ip.raw().ipv4;
        wifiIsConnected = true;
    } break;
    default:
        localIp = uint32_t(0);
        wifiIsConnected = false;
        break;
    }
}

void
wifiInit()
{
    System.disable(SYSTEM_FLAG_RESET_NETWORK_ON_CLOUD_ERRORS);
    spark::WiFi.setListenTimeout(45);
    spark::WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);
    System.on(network_status, handleNetworkEvent);
    initMdns();
}

void
updateFirmwareStreamHandler(Stream* stream)
{
    enum class DCMD : uint8_t {
        None,
        Ack,
        FlashFirmware,
    };

    auto command = DCMD::Ack;
    uint8_t invalidCommands = 0;

    while (true) {
        HAL_Delay_Milliseconds(1);
        int recv = stream->read();
        switch (recv) {
        case 'F':
            command = DCMD::FlashFirmware;
            break;
        case '\n':
            if (command == DCMD::Ack) {
                stream->write("<!FIRMWARE_UPDATER,");
                stream->write(versionCsv());
                stream->write(">\n");
                stream->flush();
                HAL_Delay_Milliseconds(10);
            } else if (command == DCMD::FlashFirmware) {
                stream->write("<!READY_FOR_FIRMWARE>\n");
                stream->flush();
                HAL_Delay_Milliseconds(10);
#if PLATFORM_ID == PLATFORM_GCC
                // just exit for sim
                HAL_Core_System_Reset_Ex(RESET_REASON_UPDATE, 0, nullptr);
#else
                system_firmwareUpdate(stream);
#endif

                break;
            } else {
                stream->write("<Invalid command received>\n");
                stream->flush();
                HAL_Delay_Milliseconds(10);
                if (++invalidCommands > 2) {
                    return;
                }
            }
            command = DCMD::Ack;
            break;
        case -1:
            continue; // empty
        default:
            command = DCMD::None;
            break;
        }
    }
}

void
updateFirmwareFromStream(cbox::StreamType streamType)
{
    if (streamType == cbox::StreamType::Usb) {
        auto ser = Serial;
        if (ser.baud() == 0) {
            ser.begin(115200);
        }
        updateFirmwareStreamHandler(&ser);
    } else {
        TCPServer server(8332); // re-open TCP server

        while (true) {
            HAL_Delay_Milliseconds(10); // allow thread switch so system thread can set up client
            {
                TCPClient client = server.available();
                if (client) {
                    updateFirmwareStreamHandler(&client);
                }
            }
        }
    }
}