/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Control.h>
#include <String.h>

class BButton;
class BCheckBox;
class ShojiDateControl;

namespace BPrivate {
class TimeEdit;
}

/**
 * The control for the start and the end of something (an event): the date, as the control for a date has it, the time of day (the time field of the Time preferences) and the box "All day"
 * that leaves the time out. Its text is that of the form: `YYYY-MM-DD hh:mm:ss`, or `YYYY-MM-DD` for a day, empty while no date is set (the attribute is removed then). The message of the
 * control is sent when the user changes any part; SetDateTimeText() does not send it. A value that is no date (a fuzzy one: 1994-05~, 202X) is typed into a text field: this control
 * only holds what it can show.
 */
class ShojiDateTimeControl : public BControl {
public:
                        ShojiDateTimeControl(const char* name, BMessage* message = NULL);
    virtual            ~ShojiDateTimeControl();

    virtual void        AttachedToWindow();
    virtual void        AllAttached();
    virtual void        MessageReceived(BMessage* message);
    virtual void        SetEnabled(bool enabled);

    /** @return the text of the value (see above); empty if there is no date */
    BString             DateTimeText();
    /** @return false if the text is neither a day nor a day with a time of day (`YYYY-MM-DD hh:mm[:ss]`), the control is not changed; empty text shows no date */
    bool                SetDateTimeText(const char* text);
    /** shows no date (the text of the control is empty); does not send the message */
    void                SetUnset();

    /** whether a text is a day, or a day with a time of day: what this control can show */
    static bool         IsDateTimeText(const char* text);

    /** sent (a copy of it) on every change that is not complete yet: typing in the day or the time. The message of the control is sent when the change is complete: Enter, the focus leaving the control, a choice from the calendar, the box All day. */
    void                SetModificationMessage(BMessage* message);
    /** the change is complete now (Enter): the message of the control is sent if there is a change that was not sent */
    void                CommitEdit();
    /** whether the day or the time was changed and not sent yet */
    bool                IsEdited() const { return fEdited; }

private:
    void                _UpdateEnabled();
    void                _Changed();
    void                _Edited();
    bool                _HasFocus();

    ShojiDateControl*   fDate;
    BButton*            fPick;
    BPrivate::TimeEdit* fTime;
    BCheckBox*          fAllDay;
    int32               fHour;          ///< the time that the field shows (it forgets it when it is attached)
    int32               fMinute;
    int32               fSeconds;       ///< the seconds of the value (the time field shows hours and minutes: what is stored is kept)
    bool                fSetting;       ///< a value is being shown: nothing is sent
    bool                fEnabled;
    BMessage*           fModification;
    bool                fEdited;        ///< something was typed that the message of the control has not told yet
};
