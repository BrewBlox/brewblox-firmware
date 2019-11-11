/*
 * Copyright 2018 Elco Jacobs / BrewBlox
 *
 * This file is part of ControlBox
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

#include "DataStream.h"
#include "InactiveObject.h"
#include "Object.h"
#include <limits>
#include <memory>

namespace cbox {

/**
 * A wrapper around an object that stores which type it is and in which groups it is active
 */
class ContainedObject {
public:
    explicit ContainedObject(obj_id_t id, uint8_t groups, std::shared_ptr<Object>&& obj)
        : _id(std::move(id))
        , _groups(std::move(groups))
        , _obj(std::move(obj))
        , _nextUpdateTime(0){};

private:
    obj_id_t _id;                 // unique id of object
    uint8_t _groups;              // active in these groups
    std::shared_ptr<Object> _obj; // pointer to runtime object
    update_t _nextUpdateTime;     // next time update should be called on _obj

public:
    const obj_id_t& id() const
    {
        return _id;
    }

    const uint8_t& groups() const
    {
        return _groups;
    }

    const std::shared_ptr<Object>& object() const
    {
        return _obj;
    }

    void deactivate()
    {
        obj_type_t oldType = _obj->typeId();
        _obj = std::make_shared<InactiveObject>(oldType);
    }

    void update(const update_t& now)
    {
        const update_t overflowGuard = std::numeric_limits<update_t>::max() / 2;
        if (overflowGuard - now + _nextUpdateTime <= overflowGuard) {
            _nextUpdateTime = _obj->update(now);
        }
    }

    void forcedUpdate(const uint32_t& now)
    {
        _nextUpdateTime = _obj->update(now);
    }

    CboxError streamTo(DataOut& out) const
    {
        if (!out.put(_id)) {
            return CboxError::OUTPUT_STREAM_WRITE_ERROR; // LCOV_EXCL_LINE
        }
        if (!out.put(_groups)) {
            return CboxError::OUTPUT_STREAM_WRITE_ERROR; // LCOV_EXCL_LINE
        }
        if (!out.put(_obj->typeId())) {
            return CboxError::OUTPUT_STREAM_WRITE_ERROR; // LCOV_EXCL_LINE
        }
        return _obj->streamTo(out);
    }

    CboxError streamFrom(DataIn& in)
    {
        // id is not streamed in. It is immutable and assumed to be already read to find this entry

        uint8_t newGroups;
        obj_type_t expectedType;
        if (!in.get(newGroups)) {
            return CboxError::INPUT_STREAM_READ_ERROR; // LCOV_EXCL_LINE
        }
        if (!in.get(expectedType)) {
            return CboxError::INPUT_STREAM_READ_ERROR; // LCOV_EXCL_LINE
        }

        if (expectedType == _obj->typeId()) {
            if (_groups & 0x80) {
                // system object, always keep system group flag
                _groups = newGroups | 0x80;
            } else {
                // user object, don't allow system group flag
                _groups = newGroups & 0x7F;
            }

            return _obj->streamFrom(in);
        }
        return CboxError::INVALID_OBJECT_TYPE;
    }

    CboxError streamPersistedTo(DataOut& out) const
    {
        // id is not streamed out. It is passed to storage separately
        if (_obj->typeId() == InactiveObject::staticTypeId()) {
            // inactive objects are not persisted, but no error is returned
            // never happens, because for a write an inactive object is temporarily replaced with an active object to process the write
            return CboxError::OK; // LCOV_EXCL_LINE
        }
        if (!out.put(_groups)) {
            return CboxError::PERSISTED_STORAGE_WRITE_ERROR; // LCOV_EXCL_LINE
        }
        if (!out.put(_obj->typeId())) {
            return CboxError::PERSISTED_STORAGE_WRITE_ERROR; // LCOV_EXCL_LINE
        }
        return _obj->streamPersistedTo(out);
    }
};

} // end namespace cbox
