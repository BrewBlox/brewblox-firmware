/*
 * Copyright 2014-2015 Matthew McGowan.
 * Copyright 2018 BrewBlox / Elco Jacobs
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

#include "CboxError.h"
#include "CboxPtr.h"
#include "Connections.h"
#include "DataStream.h"
#include "DataStreamConverters.h"
#include "EepromObjectStorage.h"
#include "Object.h"
#include "ObjectContainer.h"
#include "ObjectFactory.h"
#include "ScanningFactory.h"
#include <memory>

namespace cbox {

class Box {
private:
    // A single container is used for both system and user objects.
    // The application can add the system objects first, then set the start ID to a higher value.
    // The objects with an ID lower than the start ID cannot be deleted.
    const ObjectFactory& factory;
    ObjectContainer& objects;
    ObjectStorage& storage;
    // Box receives commands from connections in the connection pool and streams back the answer to the same connection
    ConnectionPool& connections;
    std::vector<std::unique_ptr<ScanningFactory>> scanners;
    uint8_t activeGroups = 0x81; // system group and first user group
    update_t lastUpdateTime = 0;

    // command handlers
    void noop(DataIn& in, EncodedDataOut& out);
    void invalidCommand(DataIn& in, EncodedDataOut& out);
    void readObject(DataIn& in, EncodedDataOut& out);
    void writeObject(DataIn& in, EncodedDataOut& out);
    void createObject(DataIn& in, EncodedDataOut& out);
    void deleteObject(DataIn& in, EncodedDataOut& out);
    void listActiveObjects(DataIn& in, EncodedDataOut& out);
    void readStoredObject(DataIn& in, EncodedDataOut& out);
    void listStoredObjects(DataIn& in, EncodedDataOut& out);
    void clearObjects(DataIn& in, EncodedDataOut& out);
    void reboot(DataIn& in, EncodedDataOut& out);
    void factoryReset(DataIn& in, EncodedDataOut& out);
    void listCompatibleObjects(DataIn& in, EncodedDataOut& out);
    void discoverNewObjects(DataIn& in, EncodedDataOut& out);

    std::tuple<CboxError, std::shared_ptr<Object>, uint8_t> createObjectFromStream(DataIn& in);
    CboxError loadSingleObjectFromStorage(const storage_id_t& id, RegionDataIn& objInStorage);

public:
    Box(const ObjectFactory& _factory,
        ObjectContainer& _objects,
        ObjectStorage& _storage,
        ConnectionPool& _connections,
        std::vector<std::unique_ptr<ScanningFactory>>&& _scanners = std::vector<std::unique_ptr<ScanningFactory>>());

    Box(const Box&) = delete;
    Box& operator=(const Box&) = delete;
    Box(Box&&) = default;

    ~Box() = default;

    void handleCommand(DataIn& data, DataOut& out);

    // process all incoming messages assuming they are hex encoded
    void hexCommunicate();

    auto getObject(const obj_id_t& id)
    {
        return objects.fetch(id);
    }

    void setActiveGroupsAndUpdateObjects(uint8_t newGroups);

    uint8_t getActiveGroups() const
    {
        return activeGroups;
    }

    void update(const update_t& now)
    {
        lastUpdateTime = now;
        objects.update(now);
    }

    void forcedUpdate(const update_t& now)
    {
        lastUpdateTime = now;
        objects.forcedUpdate(now);
    }

    void loadObjectsFromStorage();

    inline const obj_id_t userStartId() const
    {
        return obj_id_t(100);
    }

    template <typename T>
    CboxPtr<T> makeCboxPtr(const obj_id_t& id = 0)
    {
        return CboxPtr<T>(objects, id);
    }

    obj_id_t
    discoverNewObject(std::function<std::shared_ptr<Object>()>& discoverObject, std::function<bool(Object&, Object&)> isSame);

    CboxError storeUpdatedObject(const obj_id_t& id) const;
    CboxError reloadStoredObject(const obj_id_t& id);

    enum CommandID : uint8_t {
        NONE = 0,                     // no-op
        READ_OBJECT = 1,              // stream an object to the data out
        WRITE_OBJECT = 2,             // stream new data into an object from the data in
        CREATE_OBJECT = 3,            // add a new object
        DELETE_OBJECT = 4,            // delete an object by id
        LIST_ACTIVE_OBJECTS = 5,      // list objects saved to persistent storage
        READ_STORED_OBJECT = 6,       // list objects saved to persistent storage
        LIST_STORED_OBJECTS = 7,      // list objects saved to persistent storage
        CLEAR_OBJECTS = 8,            // remove all user objects
        REBOOT = 9,                   // reboot the system
        FACTORY_RESET = 10,           // erase all settings and reboot
        LIST_COMPATIBLE_OBJECTS = 11, // list object IDs implementing the requested interface
        DISCOVER_NEW_OBJECTS = 12,    // discover newly connected objects that support auto discovery
    };
    // application can add additional commands, starting at 100.

    void startConnections()
    {
        connections.startAll();
    }

    void stopConnections()
    {
        connections.stopAll();
    }

    void unloadAllObjects()
    {
        objects.clearAll();
    }
};

bool
applicationCommand(uint8_t cmdId, DataIn& in, EncodedDataOut& out); // command handler specified by application to add additional commands

} // end namespace cbox
