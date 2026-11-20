/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <CheckBox.h>
#include <Control.h>
#include <String.h>

#include <functional>

class BButton;
class BGridLayout;
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

    /** whether the value is a day with no time of day (the box All day of the form, ShojiAllDayBox, sets it: the control has no box of its own) */
    bool                IsAllDay() const;
    /** whether a day is set */
    bool                HasDate() const;
    /** sets All day (the time of day is left out); the message of the control is sent if @p notify, a day is set and the value changed */
    void                SetAllDay(bool allDay, bool notify);
    /** called whenever what the control shows changes (a value was set, the user changed something): the box of the form follows */
    void                SetStateCallback(const std::function<void()>& callback) { fStateCallback = callback; }

private:
    void                _UpdateEnabled();
    void                _Changed();
    void                _Edited();
    bool                _HasFocus();

    ShojiDateControl*   fDate;
    BButton*            fPick;
    BPrivate::TimeEdit* fTime;
    bool                fAllDay;        ///< the value is a day with no time of day (the time field is not used)
    int32               fHour;          ///< the time that the field shows (it forgets it when it is attached)
    int32               fMinute;
    int32               fSeconds;       ///< the seconds of the value (the time field shows hours and minutes: what is stored is kept)
    bool                fSetting;       ///< a value is being shown: nothing is sent
    bool                fEnabled;
    BMessage*           fModification;
    bool                fEdited;        ///< something was typed that the message of the control has not told yet
    std::function<void()> fStateCallback;
};


/**
 * The box All day of the start and the end of something (the controls have none): it sits to the right of the two controls, in the middle between them. It shows what both say (checked if both are a day,
 * empty if both have a time of day, a dash if they differ) and sets both. A start with no end has it beside it. Made by ShojiCombineAllDay() and ShojiAddAllDay().
 */
class ShojiAllDayBox : public BCheckBox {
public:
                        ShojiAllDayBox(const char* label, ShojiDateTimeControl* start, ShojiDateTimeControl* end = NULL);

    virtual void        AttachedToWindow();
    virtual void        MessageReceived(BMessage* message);

    /** shows what the controls say */
    void                Refresh();

private:
    ShojiDateTimeControl*   fStart;
    ShojiDateTimeControl*   fEnd;
    bool                    fMixed;     ///< the two differ (a click makes both a day)
};


/**
 * Puts the start and the end that stand in two rows one under the other in the column of the fields of a form (grid, label in the first column, controls in the second) in one cell that spans both rows,
 * with the one box All day to the right of them, in the middle. The controls lose their own box. The rows are those that the caller knows (the cells of a grid are not looked at: a grid keeps stale cells
 * when a field was replaced). The labels of the two rows (if the caller has them) get the height of a control, so that a grid that shares the height of the one cell between the two rows gives each the same.
 * @return false if they are not in two following rows (nothing is changed)
 */
bool ShojiCombineAllDay(BGridLayout* grid, ShojiDateTimeControl* start, int32 startRow, BView* startLabel, ShojiDateTimeControl* end, int32 endRow, BView* endLabel);

/** A start or an end that has no partner in the form (a file type that has only one of them): the box All day stands beside the control, in its cell. The control is at the row @p row of the column of the fields. */
bool ShojiAddAllDay(BGridLayout* grid, ShojiDateTimeControl* control, int32 row);
