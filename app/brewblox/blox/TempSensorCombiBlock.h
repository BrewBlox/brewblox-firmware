#pragma once

#include "TempSensorCombi.h"

#include "TempSensor.h"
#include "blox/Block.h"
#include "blox/FieldTags.h"
#include "cbox/CboxPtr.h"
#include "proto/cpp/TempSensorCombi.pb.h"

class TempSensorCombiBlock : public Block<BrewBloxTypes_BlockType_TempSensorCombi> {
private:
    TempSensorCombi sensor;
    cbox::ObjectContainer& objectsRef; // remember object container reference to create CboxPtrs
    std::vector<cbox::CboxPtr<TempSensor>> inputs;

public:
    TempSensorCombiBlock(cbox::ObjectContainer& objects)
        : objectsRef(objects)
    {
    }

    virtual ~TempSensorCombiBlock() = default;

    virtual cbox::CboxError
    streamFrom(cbox::DataIn& in) override final
    {
        blox_TempSensorCombi newData = blox_TempSensorCombi_init_zero;
        cbox::CboxError result = streamProtoFrom(in, &newData, blox_TempSensorCombi_fields, blox_TempSensorCombi_size);
        if (result == cbox::CboxError::OK) {
            sensor.func = TempSensorCombi::CombineFunc(newData.combinefunc);
            inputs.clear();
            sensor.inputs.clear();
            inputs.reserve(newData.sensors_count);
            sensor.inputs.reserve(newData.sensors_count);
            for (uint8_t i = 0; i < newData.sensors_count && i < 8; i++) {
                inputs.push_back(cbox::CboxPtr<TempSensor>(objectsRef, newData.sensors[i]));
            }
            for (auto& i : inputs) {
                sensor.inputs.push_back(i.lockFunctor());
            }
        }
        return result;
    }

    void
    writeMessage(blox_TempSensorCombi& message, bool includeReadOnly) const
    {
        FieldTags stripped;

        message.sensors_count = sensor.inputs.size();
        message.combinefunc = _blox_SensorCombiFunc(sensor.func);
        for (uint8_t i = 0; i < message.sensors_count && i < 8; i++) {
            message.sensors[i] = inputs[i].getId();
        }

        if (includeReadOnly) {
            if (sensor.valid()) {
                message.value = cnl::unwrap((sensor.value()));
            } else {
                stripped.add(blox_TempSensorCombi_value_tag);
            }
        }
        stripped.copyToMessage(message.strippedFields, message.strippedFields_count, 1);
    }

    virtual cbox::CboxError
    streamTo(cbox::DataOut& out) const override final
    {
        blox_TempSensorCombi message = blox_TempSensorCombi_init_zero;
        writeMessage(message, true);

        return streamProtoTo(out, &message, blox_TempSensorCombi_fields, blox_TempSensorCombi_size);
    }

    virtual cbox::CboxError
    streamPersistedTo(cbox::DataOut& out) const override final
    {
        blox_TempSensorCombi message = blox_TempSensorCombi_init_zero;
        writeMessage(message, false);
        return streamProtoTo(out, &message, blox_TempSensorCombi_fields, blox_TempSensorCombi_size);
    }

    virtual cbox::update_t
    update(const cbox::update_t& now) override final
    {
        sensor.update();
        return update_1s(now);
    }

    virtual void*
    implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == BrewBloxTypes_BlockType_TempSensorCombi) {
            return this; // me!
        }
        if (iface == cbox::interfaceId<TempSensor>()) {
            // return the member that implements the interface in this case
            TempSensor* ptr = &sensor;
            return ptr;
        }
        return nullptr;
    }

    TempSensorCombi&
    get()
    {
        return sensor;
    }
};
