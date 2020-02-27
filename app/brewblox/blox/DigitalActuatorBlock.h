#pragma once

#include "ActuatorDigital.h"
#include "ActuatorDigitalConstrained.h"
#include "ActuatorDigitalConstraintsProto.h"
#include "FieldTags.h"
#include "blox/Block.h"
#include "cbox/CboxPtr.h"
#include "proto/cpp/DigitalActuator.pb.h"
#include <cstdint>

class DigitalActuatorBlock : public Block<BrewBloxTypes_BlockType_DigitalActuator> {
private:
    cbox::ObjectContainer& objectsRef; // remember object container reference to create constraints
    cbox::CboxPtr<IoArray> hwDevice;
    ActuatorDigital actuator;
    ActuatorDigitalConstrained constrained;

public:
    DigitalActuatorBlock(cbox::ObjectContainer& objects)
        : objectsRef(objects)
        , hwDevice(objects)
        , actuator(hwDevice.lockFunctor(), 0)
        , constrained(actuator)
    {
    }
    virtual ~DigitalActuatorBlock() = default;

    virtual cbox::CboxError streamFrom(cbox::DataIn& dataIn) override final
    {
        blox_DigitalActuator message = blox_DigitalActuator_init_zero;
        cbox::CboxError result = streamProtoFrom(dataIn, &message, blox_DigitalActuator_fields, blox_DigitalActuator_size);

        if (result == cbox::CboxError::OK) {
            if (hwDevice.getId() != message.hwDevice) {
                actuator.channel(0); // unregister at old hwDevice
                hwDevice.setId(message.hwDevice);
            }
            actuator.channel(message.channel);
            actuator.invert(message.invert);
            setDigitalConstraints(message.constrainedBy, constrained, objectsRef);
            constrained.desiredState(ActuatorDigitalBase::State(message.desiredState));
        }

        return result;
    }

    void writePersistedStateToMessage(blox_DigitalActuator& message) const
    {
        message.hwDevice = hwDevice.getId();
        message.channel = actuator.channel();
        message.invert = actuator.invert();
        message.desiredState = blox_DigitalState(constrained.desiredState());

        getDigitalConstraints(message.constrainedBy, constrained);
    }

    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_DigitalActuator message = blox_DigitalActuator_init_zero;
        FieldTags stripped;

        writePersistedStateToMessage(message);
        auto state = actuator.state();
        if (state == ActuatorDigitalBase::State::Unknown) {
            stripped.add(blox_DigitalActuator_state_tag);
        } else {
            message.state = blox_DigitalState(actuator.state());
        }

        stripped.copyToMessage(message.strippedFields, message.strippedFields_count, 1);
        return streamProtoTo(out, &message, blox_DigitalActuator_fields, blox_DigitalActuator_size);
    }

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut& out) const override final
    {
        blox_DigitalActuator message = blox_DigitalActuator_init_zero;
        writePersistedStateToMessage(message);
        return streamProtoTo(out, &message, blox_DigitalActuator_fields, blox_DigitalActuator_size);
    }

    virtual cbox::update_t update(const cbox::update_t& now) override final
    {
        actuator.update();
        return constrained.update(now);
    }

    virtual void* implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == BrewBloxTypes_BlockType_DigitalActuator) {
            return this; // me!
        }
        if (iface == cbox::interfaceId<ActuatorDigitalConstrained>()) {
            // return the member that implements the interface in this case
            ActuatorDigitalConstrained* ptr = &constrained;
            return ptr;
        }
        return nullptr;
    }

    ActuatorDigitalConstrained& getConstrained()
    {
        return constrained;
    }
};
