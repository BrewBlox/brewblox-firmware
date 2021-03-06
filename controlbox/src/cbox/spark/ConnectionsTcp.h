/*
 * Copyright 2018 BrewBlox / Elco Jacobs
 *
 * This file is part of Controlbox.
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

#pragma once

#include "../Connections.h"
#include "spark_wiring_tcpclient.h"
#include "spark_wiring_tcpserver.h"
#include "spark_wiring_wifi.h"

namespace cbox {

class TcpConnection : public StreamConnection<TCPClient> {
public:
    explicit TcpConnection(TCPClient&& _client)
        : StreamConnection<TCPClient>(std::move(_client))
    {
    }
    virtual ~TcpConnection() = default;

    virtual void stop() override final
    {
        get().stop();
    }
};

class TcpConnectionSource : public ConnectionSource {
private:
    TCPServer server;
    bool server_started = false;
    bool server_enabled = false;

public:
    TcpConnectionSource(uint16_t port)
        : server(port)
    {
    }
    virtual ~TcpConnectionSource() = default;

    std::unique_ptr<Connection> newConnection() override final
    {
        if (spark::WiFi.ready()) {
            if (server_enabled && !server_started) {
                server_started = server.begin();
            }

            TCPClient newClient = server.available();
            if (newClient) {
                newClient.setTimeout(100);
                return std::make_unique<TcpConnection>(std::move(newClient));
            }
        } else {
            stop();
        }
        return std::unique_ptr<Connection>();
    }

    virtual void stop() override final
    {
        server.stop();
        server_enabled = false;
    }

    virtual void start() override final
    {
        server_enabled = true;
        server.begin();
    }
};

} // end namespace cbox
