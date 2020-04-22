/*
 * Copyright 2020 BrewPi B.V.
 *
 * This file is part of BrewBlox.
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

#include "SmallColorScheme.h"

SmallColorScheme TOP_BAR_SCHEME = {
    D4D_COLOR_RGB(0, 0, 0),      ///< The object background color in standard state
    D4D_COLOR_RGB(0, 0, 0),      ///< The object background color in disabled state
    D4D_COLOR_RGB(0, 0, 0),      ///< The object background color in focused state
    D4D_COLOR_RGB(0, 0, 0),      ///< The object background color in captured state
    D4D_COLOR_RGB(80, 128, 150), ///< The object fore color in standard state
    D4D_COLOR_RGB(64, 64, 64),   ///< The object fore color in disabled state
    D4D_COLOR_RGB(64, 100, 200), ///< The object fore color in focused state
    D4D_COLOR_RGB(64, 100, 255), ///< The object fore color in captured state
};