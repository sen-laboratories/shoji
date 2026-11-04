/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDurationControl.h"

#include <string.h>

#include "ShojiDuration.h"

ShojiDurationControl::ShojiDurationControl(const char* name, const char* label, BMessage* message)
    :
    BTextControl(name, label, "", message)
{
    SetToolTip("Hours, minutes and seconds: 01:55:00");
}


void ShojiDurationControl::SetDuration(const char* iso)
{
    std::string clock;
    const char* shown = shoji::DurationToClock(iso != NULL ? iso : "", &clock) ? clock.c_str() : (iso != NULL ? iso : "");
    if (strcmp(Text(), shown) != 0)
        SetText(shown);
}


BString ShojiDurationControl::Duration() const
{
    std::string iso;
    if (shoji::ClockToDuration(Text(), &iso))
        return BString(iso.c_str());
    return BString(Text());
}
