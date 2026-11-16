/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <DateFormat.h>
#include <DateTimeEdit.h>
#include <String.h>

/**
 * The control for a date: the date field of the Time preferences (BPrivate::DateEdit, from libshared), with the date as the text `YYYY-MM-DD` that the forms of
 * Shoji use for a value of type `B_TIME_TYPE` (midnight UTC). The message of the control is sent when the user changes the date; SetDateText() does not send it.
 *
 * The fields are those of the short date format of the Locale preferences: their ORDER and the SEPARATOR as configured (17.05.1980, 05/17/1980, 1980-05-17); the year is always shown
 * with all its digits (the short format of many locales has two, which is no way to enter a birthday). Digits are typed as they are written: a field takes two digits (four for the year)
 * and moves on to the next, the separator that the locale has (and a dot, a slash, a dash, a blank) moves on at once, so that 17.5.1980 can be typed as it is said.
 *
 * A day that may be missing (a birthday that is not known yet) is shown by the control as `-- . -- . --` while the file has none: the control is made UNSETTABLE (SetUnsettable()) and unset
 * (SetUnset()). The first click, arrow or digit starts it at today's date (not stored: the control goes back to unset if it loses the focus before a change), the first change is sent as
 * usual; Delete or Backspace empties it again (the text is then empty, which removes the attribute).
 */
class ShojiDateControl : public BPrivate::DateEdit {
public:
                        ShojiDateControl(const char* name, BMessage* message = NULL);
    virtual            ~ShojiDateControl();

    virtual void        AttachedToWindow();
    virtual void        MouseDown(BPoint where);
    virtual void        KeyDown(const char* bytes, int32 numBytes);
    virtual void        MakeFocus(bool focused = true);
    virtual status_t    Invoke(BMessage* message = NULL);

    virtual void        DrawSection(uint32 index, BRect bounds, bool hasFocus);
    virtual void        DrawSeparator(uint32 index, BRect bounds);
    virtual float       MinSectionWidth();
    virtual void        DoUpPress();
    virtual void        DoDownPress();

    /** the border and the arrows: the arrows are the filled triangles that the other controls of the system have (the latch of a list, the buttons of a spinner), not the thin lines of the Time preferences */
    virtual void        DrawBorder(const BRect& updateRect);

public:
    /** @return false if the text is not a date (`YYYY-MM-DD`), the control is not changed */
    bool                SetDateText(const char* text);
    BString             DateText();

    /** the control can show no date (a birthday that is not known): see above */
    void                SetUnsettable(bool unsettable) { fUnsettable = unsettable; }
    bool                Unsettable() const { return fUnsettable; }
    /** shows no date (the text of the control is empty); does not send the message */
    void                SetUnset();
    bool                IsUnset() const { return fUnset; }

    /** the attributes that hold a day and are shown with this control even when they have none (the birthday of a person) */
    static bool         IsDayAttribute(const char* name);

    /** whether a text is a date with no time of day: what this control can show (anything else needs a text field) */
    static bool         IsDateText(const char* text);

private:
    bool                fHasDate;       ///< the control makes its fields when it is attached, and starts with today: the date is set again then
    int32               fYear;
    int32               fMonth;
    int32               fDay;
    bool                fUnsettable;    ///< it can show no date
    bool                fUnset;         ///< and it does now
    BDateFormat         fFormat;        ///< the short format of the locale with a year of four digits
    BString             fTyped;         ///< the digits typed into the field that has the focus
    bigtime_t           fTypedAt;
    bool                fSkipSeparator; ///< the field moved on by itself: the next separator is the one that was typed after it
    bool                fPending;       ///< it was started at today's date and nothing was changed yet: it goes back to unset if it loses the focus
    void                _Begin();       ///< leaves the unset state, at today's date
    /** the text of the date in the format, with where the fields are (positions of start and end for each field and each separator, malloc'ed) and what they are; false if it cannot be made */
    bool                _Layout(BString* text, int** positions, int* positionCount, BDateElement** kinds, int* kindCount);
    void                _Type(int digit);
    void                _Advance(bool automatic);
    bool                _IsSeparator(char c);
};
