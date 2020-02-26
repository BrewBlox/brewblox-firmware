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
#include "spark_wiring_system.h"
#include "spark_wiring_tcpclient.h"
#include "spark_wiring_tcpserver.h"
#include "spark_wiring_usbserial.h"
#include "spark_wiring_wifi.h"
#include <cstdio>
volatile uint32_t localIp = 0;
volatile bool wifiIsConnected = false;

auto mdns = MDNS();
volatile bool mdns_started = false;
volatile bool http_started = false;
#if PLATFORM_ID == PLATFORM_GCC
auto httpserver = TCPServer(8380); // listen on 8380 to serve a simple page with instructions
#else
auto httpserver = TCPServer(80); // listen on 80 to serve a simple page with instructions
#endif

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
    return _fetch_usbserial().isConnected();
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

void
manageConnections(uint32_t now)
{
    static uint32_t lastConnect = 0;
    if (wifiIsConnected) {
        if (!mdns_started) {
            mdns_started = mdns.begin(true);
        }
        if (!http_started) {
            http_started = httpserver.begin();
        }

        if (mdns_started) {
            mdns.processQueries();
        }
        if (http_started) {
            TCPClient client = httpserver.available();
            if (client) {
                while (client.read() != -1) {
                }

                client.write("HTTP/1.1 200 Ok\n\n<html><body>"
                             "<p>Your BrewBlox Spark is online but it does not run its own web server. "
                             "Please install a BrewBlox server to connect to it using the BrewBlox protocol.</p>"
                             "<p>Device ID = ");
                client.write(System.deviceID());
                client.write("</p></body></html>\n\n");
                client.flush();
                HAL_Delay_Milliseconds(5);
                client.stop();
            }
            return;
        }
    } else {
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
    bool success = mdns.setHostname(System.deviceID());
    success = success && mdns.addService("tcp", "http", 80, System.deviceID());
    success = success && mdns.addService("tcp", "brewblox", 8332, System.deviceID());
    if (success) {
        auto hw = String("Spark ");
        switch (getSparkVersion()) {
        case SparkVersion::V1:
            hw += "1";
            break;
        case SparkVersion::V2:
            hw += "2";
            break;
        case SparkVersion::V3:
            hw += "3";
            break;
        }
        mdns.addTXTEntry("VERSION", stringify(GIT_VERSION));
        mdns.addTXTEntry("ID", System.deviceID());
        mdns.addTXTEntry("PLATFORM", stringify(PLATFORM_ID));
        mdns.addTXTEntry("HW", hw);
    }
}

void
handleNetworkEvent(system_event_t event, int param)
{
    switch (param) {
    case network_status_connected: {
        IPAddress ip = spark::WiFi.localIP();
        localIp = ip.raw().ipv4;
        wifiIsConnected = true;
#if PLATFORM_ID != PLATFORM_GCC
        // Particle.connect();

#endif
    } break;
    default:
        localIp = uint32_t(0);
        wifiIsConnected = false;
        // Particle.disconnect();
        break;
    }
}

void
wifiInit()
{
    System.disable(SYSTEM_FLAG_RESET_NETWORK_ON_CLOUD_ERRORS);
    spark::WiFi.setListenTimeout(30);
    spark::WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);
    System.on(network_status, handleNetworkEvent);
    initMdns();
}

void
updateFirmwareStreamHandler(Stream& stream)
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
        int recv = stream.read();
        switch (recv) {
        case 'F':
            command = DCMD::FlashFirmware;
            break;
        case '\n':
            if (command == DCMD::Ack) {
                stream.write("<!FIRMWARE_UPDATER,");
                stream.write(versionCsv());
                stream.write(">\n");
                stream.flush();
            }
            if (command == DCMD::FlashFirmware) {
                stream.write("<!READY_FOR_FIRMWARE>\n");
                stream.flush();
                system_firmwareUpdate(&stream);
                break;
            } else {
                stream.write("<Invalid command received>\n");
                stream.flush();
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
        WITH_LOCK(ser);
        if (ser.baud() == 0) {
            ser.begin(115200);
        }
        updateFirmwareStreamHandler(ser);
    } else {
        TCPServer server(8332); // re-open TCP server

        TCPClient client;
        while (true) {
            client = server.available();
            if (client) {
                updateFirmwareStreamHandler(client);
                client.stop();
            }
        }
    }
}