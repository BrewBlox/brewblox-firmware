/*
 * Copyright 2018 BrewPi B.V.
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

#include "listening_screen.h"

#include "../fonts/fonts.h"

#include "memory_info.h"

char listeningString1[] = "Listening mode active";
char listeningString2[] = "On the BrewBlox server, run:";
char listeningString3[] = "brewblox-ctl wifi";
char listeningString4[] = "to change WiFi settings over USB";

D4D_DECLARE_STD_LABEL(scrListening_txt1, listeningString1, 0, 85, 320, 15, FONT_REGULAR)
D4D_DECLARE_STD_LABEL(scrListening_txt2, listeningString2, 0, 112, 320, 15, FONT_REGULAR)
D4D_DECLARE_STD_LABEL(scrListening_txt3, listeningString3, 0, 131, 320, 15, FONT_REGULAR)
D4D_DECLARE_STD_LABEL(scrListening_txt4, listeningString4, 0, 151, 320, 15, FONT_REGULAR)

D4D_DECLARE_SCREEN_BEGIN(screen_listening, ScrListening_, 0, 0, (D4D_COOR)(D4D_SCREEN_SIZE_LONGER_SIDE), (D4D_COOR)(D4D_SCREEN_SIZE_SHORTER_SIDE), nullptr, 0, nullptr, (D4D_SCR_F_DEFAULT | D4D_SCR_F_TOUCHENABLE), nullptr)
&scr_mem_icon,
    &scr_mem_text,
    &scrListening_txt1,
    &scrListening_txt2,
    &scrListening_txt3,
    &scrListening_txt4,
    D4D_DECLARE_SCREEN_END();

void
ListeningScreen::activate()
{
    D4D_ActivateScreen(&screen_listening, D4D_TRUE);
}

void
ScrListening_OnInit()
{
}

void
ScrListening_OnMain()
{
    updateRamDisplay();
}

void
ScrListening_OnActivate()
{
}

void
ScrListening_OnDeactivate()
{
}

uint8_t
ScrListening_OnObjectMsg(D4D_MESSAGE* pMsg)
{
    return D4D_FALSE; // don't block further processing
}
