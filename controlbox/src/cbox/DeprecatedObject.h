/*
 * Copyright 2018 Elco Jacobs / BrewBlox, based on earlier work of Matthew McGowan
 *
 * This file is part of ControlBox.
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
#include "ObjectBase.h"
#include "ObjectIds.h"
#include <limits>

namespace cbox {

/**
 * An object that does nothing. When read, it returns the type it becomes when it is activated.
 */
class DeprecatedObject : public ObjectBase<std::numeric_limits<uint16_t>::max() - 2> {
private:
    obj_id_t originalId;

public:
    DeprecatedObject(const obj_id_t& oid)
        : originalId(oid)
    {
    }
    virtual ~DeprecatedObject() = default;

    virtual CboxError streamTo(DataOut& out) const override final
    {
        out.put(originalId);
        return CboxError::OK;
    }

    virtual CboxError streamFrom(DataIn&) override final
    {
        return CboxError::OBJECT_NOT_WRITABLE;
    }

    virtual CboxError streamPersistedTo(DataOut&) const override final
    {
        return CboxError::OBJECT_NOT_WRITABLE;
    }

    virtual update_t update(const update_t& now) override final
    {
        return update_never(now);
    }

    obj_id_t storageId()
    {
        return originalId;
    }
};

} // end namespace cbox
