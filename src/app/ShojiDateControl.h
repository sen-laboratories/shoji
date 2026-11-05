/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <DateTimeEdit.h>
#include <String.h>

/**
 * The control for a date: the date field of the Time preferences (BPrivate::DateEdit, from libshared), with the date as the text `YYYY-MM-DD` that the forms of
 * Shoji use for a value of type `B_TIME_TYPE` (midnight UTC). The message of the control is sent when the user changes the date; SetDateText() does not send it.
 */
class ShojiDateControl : public BPrivate::DateEdit {
public:
                        ShojiDateControl(const char* name, BMessage* message = NULL);
    virtual            ~ShojiDateControl();

    virtual void        AttachedToWindow();

protected:
    /** the border and the arrows: the arrows are the filled triangles that the other controls of the system have (the latch of a list, the buttons of a spinner), not the thin lines of the Time preferences */
    virtual void        DrawBorder(const BRect& updateRect);

public:
    /** @return false if the text is not a date (`YYYY-MM-DD`), the control is not changed */
    bool                SetDateText(const char* text);
    BString             DateText();

    /** whether a text is a date with no time of day: what this control can show (anything else needs a text field) */
    static bool         IsDateText(const char* text);

private:
    bool                fHasDate;       ///< the control makes its fields when it is attached, and starts with today: the date is set again then
    int32               fYear;
    int32               fMonth;
    int32               fDay;
};
