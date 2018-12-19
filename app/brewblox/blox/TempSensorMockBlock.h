#pragma once

#include "TempSensorMock.h"

#include "blox/Block.h"
#include "proto/cpp/TempSensorMock.pb.h"

class TempSensorMockBlock : public Block<blox_TempSensorMock_msgid> {
private:
    TempSensorMock sensor;

public:
    TempSensorMockBlock() = default;
    ~TempSensorMockBlock() = default;

    virtual cbox::CboxError streamFrom(cbox::DataIn& in) override final
    {
        blox_TempSensorMock newData = blox_TempSensorMock_init_zero;
        cbox::CboxError res = streamProtoFrom(in, &newData, blox_TempSensorMock_fields, blox_TempSensorMock_size);
        /* if no errors occur, write new settings to wrapped object */
        if (res == cbox::CboxError::OK) {
            sensor.value(cnl::wrap<temp_t>(newData.value));
            sensor.connected(newData.connected);
        }
        return res;
    }

    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_TempSensorMock message = blox_TempSensorMock_init_zero;
        message.value = cnl::unwrap(sensor.value());
        message.valid = sensor.valid();
        message.connected = sensor.connected();
        return streamProtoTo(out, &message, blox_TempSensorMock_fields, blox_TempSensorMock_size);
    }

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut& out) const override final
    {
        blox_TempSensorMock message = blox_TempSensorMock_init_zero;
        message.value = cnl::unwrap(sensor.value());
        message.connected = sensor.connected();
        return streamProtoTo(out, &message, blox_TempSensorMock_fields, blox_TempSensorMock_size);
    }

    virtual cbox::update_t update(const cbox::update_t& now) override final
    {
        // No updates for now. Alternatively, a periodic bus scan for new devices?
        return update_never(now);
    }

    virtual void* implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == blox_TempSensorMock_msgid) {
            return this; // me!
        }
        if (iface == cbox::interfaceId<TempSensor>()) {
            // return the member that implements the interface in this case
            TempSensor* ptr = &sensor;
            return ptr;
        }
        return nullptr;
    }

    TempSensorMock& get()
    {
        return sensor;
    }
};
