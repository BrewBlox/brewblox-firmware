/*
 * Copyright 2014-2015 Matthew McGowan.
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
#include "CompositeDataStream.h"
#include "DataStream.h"
#include "DataStreamConverters.h"
#include <functional>
#include <memory>
#include <vector>

namespace cbox {
/**
 * Represents a connection to an endpoint. The details of the endpoint are not provided here.
 * A connection has these components:
 *
 * - a stream for input data (DataIn)
 * - a stream for output data (DatOut)
 * - a connected flag: indicates if this connection can read/write data to the resource
 *
 */

class Connection {
public:
    Connection() = default;
    virtual ~Connection() = default;

    virtual DataOut& getDataOut() = 0;
    virtual DataIn& getDataIn() = 0;
    virtual bool isConnected() = 0;
    virtual void stop() = 0;
};

class ConnectionSource {
public:
    ConnectionSource() = default;
    virtual ~ConnectionSource() = default;

    virtual std::unique_ptr<Connection> newConnection() = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
};

template <class S>
StreamType
getStreamType();

/**
 * Adapts a Stream instance to DataIn.
 */
template <class S>
class StreamDataIn : public DataIn {
protected:
    S& stream;

public:
    StreamDataIn(S& _stream)
        : stream(_stream)
    {
    }

    virtual bool hasNext() override
    {
        return stream.available() > 0;
    }

    virtual uint8_t next() override
    {
        return uint8_t(stream.read());
    }

    virtual uint8_t peek() override
    {
        return uint8_t(stream.peek());
    }

    virtual stream_size_t available() override
    {
        return stream_size_t(stream.available());
    }

    static StreamType streamTypeImpl();

    virtual StreamType streamType() const override final
    {
        return streamTypeImpl();
    }
};

/**
 * Wraps a stream to provide the DataOut interface.
 */
template <typename T>
class StreamDataOut final : public DataOut {
protected:
    /**
     * The stream type that is adapted to a DataOut instance.
     * non-NULL.
     */
    T& stream;

public:
    StreamDataOut(T& _stream)
        : stream(_stream)
    {
    }

    bool write(uint8_t data) override
    {
        return stream.write(data) != 0;
    }

    virtual bool writeBuffer(const void* data, stream_size_t length) override final
    {
        return stream.write((const uint8_t*)data, length) == length;
    }
};

template <typename T>
class StreamRefConnection : public Connection {
private:
    T& stream;
    StreamDataIn<T> in;
    StreamDataOut<T> out;

public:
    StreamRefConnection(T& _stream)
        : stream(_stream)
        , in(stream)
        , out(stream)
    {
    }
    virtual ~StreamRefConnection() = default;

    virtual DataOut& getDataOut() override
    {
        return out;
    }

    virtual DataIn& getDataIn() override
    {
        return in;
    }

    virtual bool isConnected() override
    {
        return stream.isConnected();
    }

    T& get()
    {
        return stream;
    }

    StreamRefConnection(const StreamRefConnection& other) = delete; // not copyable
};

template <typename T>
class StreamConnection : public Connection {
private:
    T stream;
    StreamDataIn<T> in;
    StreamDataOut<T> out;

public:
    explicit StreamConnection(T&& _stream)
        : stream(std::move(_stream))
        , in(stream)
        , out(stream)
    {
    }
    virtual ~StreamConnection() = default;

    virtual DataOut& getDataOut() override
    {
        return out;
    }

    virtual DataIn& getDataIn() override
    {
        return in;
    }

    virtual bool isConnected() override
    {
        return stream.connected();
    }

    T& get()
    {
        return stream;
    }

    StreamConnection(const StreamConnection& other) = delete; // not copyable
};

extern void
connectionStarted(DataOut& out);

class ConnectionPool {
private:
    std::vector<std::reference_wrapper<ConnectionSource>> connectionSources;
    std::vector<std::unique_ptr<Connection>> connections;

    CompositeDataOut<decltype(connections)> allConnectionsDataOut;
    DataOut& currentDataOut;

public:
    ConnectionPool(std::initializer_list<std::reference_wrapper<ConnectionSource>> list)
        : connectionSources(list)
        , allConnectionsDataOut(connections, [](const decltype(connections)::value_type& conn) -> DataOut& { return conn->getDataOut(); })
        , currentDataOut(allConnectionsDataOut)
    {
    }

    void updateConnections()
    {
        connections.erase(
            std::remove_if(connections.begin(), connections.end(), [](std::unique_ptr<Connection>& conn) {
                return !conn->isConnected(); // remove disconnected connections from pool
            }),
            connections.end());

        for (auto& source : connectionSources) {
            std::unique_ptr<Connection> newConnection = source.get().newConnection();
            if (newConnection) {
                connectionStarted(newConnection->getDataOut());
                connections.push_back(std::move(newConnection));
            }
        }
    }

    size_t size()
    {
        return connections.size();
    }

    void process(std::function<void(DataIn& in, DataOut& out)> handler)
    {
        updateConnections();
        for (auto& conn : connections) {
            DataIn& in = conn->getDataIn();
            DataOut& out = conn->getDataOut();
            currentDataOut = out;
            handler(in, out);
        }
        currentDataOut = allConnectionsDataOut;
    }

    DataOut& logDataOut() const
    {
        return currentDataOut;
    }

    void stopAll()
    {
        for (auto& c : connections) {
            // don't delete connections here.
            // stop them, and let updateConnections destroy them when they are disconnected.
            c->stop();
        }
        for (auto& source : connectionSources) {
            source.get().stop();
        }
    }

    void startAll()
    {
        for (auto& source : connectionSources) {
            source.get().start();
        }
    }
};

} // end namespace cbox
