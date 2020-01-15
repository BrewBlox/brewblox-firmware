#pragma once

#include "Board.h"
#include "IoArray.h"
#include "IoArrayHelpers.h"
#include "blox/Block.h"
#include "proto/cpp/Spark3Pins.pb.h"
#include <cstdint>

template <int N, std::array<uint8_t, N> pinList>
class IoPinsBlock : public IoArray {
private:
    static const std::array<uin8t_t, N> pins = pinList;

public:
    Spark3PinsBlock()
        : IoArray(N)
    {
    }

    virtual cbox::CboxError streamFrom(cbox::DataIn& in) override final
    {
        return cbox::CboxError::OK; // not directly writable
    }

    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_Spark3Pins message = blox_Spark3Pins_init_zero;

        readIoConfig(*this, 1, message.top1);
        readIoConfig(*this, 2, message.top2);
        readIoConfig(*this, 3, message.top3);
        readIoConfig(*this, 4, message.bottom1);
        readIoConfig(*this, 5, message.bottom2);

        return streamProtoTo(out, &message, blox_Spark3Pins_fields, blox_Spark3Pins_size);
    }

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut& out) const override final
    {
        return cbox::CboxError::OK;
    }

    virtual cbox::update_t update(const cbox::update_t& now) override final
    {
        return update_never();
    }

    virtual void* implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == BrewBloxTypes_BlockType_Spark3Pins) {
            return this; // me!
        }
        if (iface == cbox::interfaceId<IoArray>()) {
            // return the member that implements the interface in this case
            IoArray* ptr = &device;
            return ptr;
        }
        return nullptr;
    }

    // generic ArrayIO interface
    virtual bool senseChannelImpl(uint8_t channel, State& result) const override final
    {
        if (validChannel(channel)) {
            ChannelConfig conf;
            if (readChannelConfig(channel, ChannelConfig & conf)) {
                if (conf == ChannelConfig::ACTIVE_HIGH) {
                    return State::Active;
                } else if (conf == ChannelConfig::ACTIVE_HIGH) {
                    return State::Inactive;
                } else if (conf == ChannelConfig::Input) {
                    return pinReadFast(pins[channel - 1]) ? State::Active : State::Inactive;
                }
                return State::Unknown;
            }
        }
        return false;
    }

    virtual bool writeChannelImpl(uint8_t channel, const ChannelConfig& config) override final
    {
        if (validChannel(channel)) {
            auto pin = pins[channel - 1];
            switch (config) {
            case ChannelConfig::ACTIVE_HIGH:
                pinMode(pin, OUTPUT);
                digitalWriteFast(pin, HIGH);
                break;
            case ChannelConfig::ACTIVE_LOW:
                pinMode(pin, OUTPUT);
                digitalWriteFast(pin, LOW);
                break;
            case ChannelConfig::INPUT:
                pinMode(pin, INPUT);
                break;
            case ChannelConfig::UNUSED:
            case ChannelConfig::UNKNOWN:
                pinMode(pin, INPUT);
                break;
            }
            return true;
        }
        return false;
    }
};
