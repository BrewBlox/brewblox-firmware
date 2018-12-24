#pragma once

#include "DS2413.h"
#include "blox/Block.h"
#include "proto/cpp/DS2413.pb.h"

OneWire&
theOneWire();

class DS2413Block : public Block<BrewbloxOptions_BlockType_DS2413> {
private:
    DS2413 device;

public:
    DS2413Block()
        : device(theOneWire())
    {
    }

    virtual cbox::CboxError streamFrom(cbox::DataIn& in) override final
    {
        blox_DS2413 newData = blox_DS2413_init_zero;
        cbox::CboxError res = streamProtoFrom(in, &newData, blox_DS2413_fields, blox_DS2413_size);
        /* if no errors occur, write new settings to wrapped object */
        if (res == cbox::CboxError::OK) {
            device.setDeviceAddress(OneWireAddress(newData.address));
        }
        return res;
    }

    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_DS2413 message = blox_DS2413_init_zero;

        message.address = device.getDeviceAddress();
        message.state = device.cachedState();

        return streamProtoTo(out, &message, blox_DS2413_fields, blox_DS2413_size);
    }

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut& out) const override final
    {
        return streamTo(out);
    }

    virtual cbox::update_t update(const cbox::update_t& now) override final
    {
        device.update();
        return update_1s(now);
    }

    virtual void* implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == BrewbloxOptions_BlockType_DS2413) {
            return this; // me!
        }
        if (iface == cbox::interfaceId<DS2413>()) {
            // return the member that implements the interface in this case
            DS2413* ptr = &device;
            return ptr;
        }
        if (iface == cbox::interfaceId<OneWireDevice>()) {
            // return the member that implements the interface in this case
            DS2413* dsPtr = &device;
            OneWireDevice* devicePtr = dsPtr;
            return devicePtr;
        }
        return nullptr;
    }

    DS2413& get()
    {
        return device;
    }
};
