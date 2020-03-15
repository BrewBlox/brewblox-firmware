#pragma once

#include "SetpointProfile.h"
#include "SetpointSensorPair.h"
#include "TicksBlock.h"
#include "blox/Block.h"
#include "blox/FieldTags.h"
#include "cbox/CboxPtr.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "proto/cpp/SetpointProfile.pb.h"

class SetpointProfileBlock : public Block<BrewBloxTypes_BlockType_SetpointProfile> {
private:
    cbox::CboxPtr<TicksBlock<TicksClass>> ticksPtr;
    cbox::CboxPtr<SetpointSensorPair> target;
    SetpointProfile profile;
    using Point = SetpointProfile::Point;

protected:
    static bool streamPointsOut(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
    {
        const std::vector<Point>* points = reinterpret_cast<std::vector<Point>*>(*arg);
        for (const auto& p : *points) {
            auto submsg = blox_SetpointProfile_Point();
            submsg.time = p.time;
            submsg.temperature_oneof.temperature = cnl::unwrap(p.temp);
            submsg.which_temperature_oneof = blox_SetpointProfile_Point_temperature_tag;
            if (!pb_encode_tag_for_field(stream, field)) {
                return false;
            }
            if (!pb_encode_submessage(stream, blox_SetpointProfile_Point_fields, &submsg)) {
                return false;
            }
        }
        return true;
    }

    static bool streamPointsIn(pb_istream_t* stream, const pb_field_t*, void** arg)
    {
        std::vector<Point>* newPoints = reinterpret_cast<std::vector<Point>*>(*arg);

        if (stream->bytes_left) {
            blox_SetpointProfile_Point submsg = blox_SetpointProfile_Point_init_zero;
            if (!pb_decode(stream, blox_SetpointProfile_Point_fields, &submsg)) {
                return false;
            }
            newPoints->push_back(Point{submsg.time, cnl::wrap<decltype(Point::temp)>(submsg.temperature_oneof.temperature)});
        }
        return true;
    }

public:
    SetpointProfileBlock(cbox::ObjectContainer& objects)
        : ticksPtr(objects, 3)
        , target(objects)
        , profile(target.lockFunctor())
    {
    }

    virtual ~SetpointProfileBlock() = default;

    virtual cbox::CboxError streamFrom(cbox::DataIn& in) override final
    {
        blox_SetpointProfile newData = blox_SetpointProfile_init_zero;
        std::vector<Point> newPoints;
        newData.points.funcs.decode = &streamPointsIn;
        newData.points.arg = &newPoints;
        cbox::CboxError result = streamProtoFrom(in, &newData, blox_SetpointProfile_fields, std::numeric_limits<size_t>::max() - 1);
        if (result == cbox::CboxError::OK) {
            profile.points(std::move(newPoints));
            profile.enabled(newData.enabled);
            profile.startTime(newData.start);
            target.setId(newData.targetId);
        }
        return result;
    }

    virtual cbox::CboxError streamTo(cbox::DataOut& out) const override final
    {
        blox_SetpointProfile message = blox_SetpointProfile_init_zero;
        FieldTags stripped;
        message.points.funcs.encode = &streamPointsOut;
        message.points.arg = const_cast<std::vector<Point>*>(&profile.points());
        message.enabled = profile.enabled();
        message.start = profile.startTime();
        message.targetId = target.getId();
        if (profile.isDriving()) {
            message.drivenTargetId = target.getId();
        }

        cbox::CboxError result = streamProtoTo(out, &message, blox_SetpointProfile_fields, std::numeric_limits<size_t>::max() - 1);
        return result;
    }

    virtual cbox::CboxError streamPersistedTo(cbox::DataOut& out) const override final
    {
        return streamTo(out);
    }

    virtual cbox::update_t update(const cbox::update_t& now) override final
    {
        if (auto pTicks = ticksPtr.const_lock()) {
            auto time = pTicks->const_get().utc();
            profile.update(time);
        }
        return update_1s(now);
    }

    virtual void* implements(const cbox::obj_type_t& iface) override final
    {
        if (iface == BrewBloxTypes_BlockType_SetpointProfile) {
            return this; // me!
        }

        return nullptr;
    }

    SetpointProfile& get()
    {
        return profile;
    }
};
