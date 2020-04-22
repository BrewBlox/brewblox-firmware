/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of BrewPi.
 * 
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "Temperature.h"
#include "WidgetWrapper.h"

struct WidgetSettings {
    TempUnit tempUnit;
};

class WidgetBase {
protected:
    WidgetWrapper& wrapper;

public:
    WidgetBase(WidgetWrapper& myWrapper)
        : wrapper(myWrapper)
    {
    }
    virtual ~WidgetBase()
    {
        wrapper.resetClickHandler();
        wrapper.resetChildren();
        wrapper.resetName();
    }

    void setClickHandler(void* obj, void (*func)(void*))
    {
        wrapper.setClickHandler(obj, func);
    }

    void enableBackground(bool enabled)
    {
        wrapper.setEnabled(enabled);
    }

    static void setAndEnable(D4D_OBJECT* obj, std::string&& txt)
    {
        static const char errTxt[] = "--.-";
        auto s = std::move(txt);

        if (!s.empty()) {
            D4D_SetText(obj, s.c_str());
            D4D_EnableObject(obj, true);
            return;
        }
        D4D_SetText(obj, errTxt);
        D4D_EnableObject(obj, false);
    }

    virtual void update(const WidgetSettings& settings) = 0;
};