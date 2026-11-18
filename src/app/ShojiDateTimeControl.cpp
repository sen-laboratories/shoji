/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDateTimeControl.h"

#include <Box.h>
#include <Button.h>
#include <CalendarView.h>
#include <CheckBox.h>
#include <DateTime.h>
#include <DateTimeEdit.h>
#include <GridLayout.h>
#include <GroupLayout.h>
#include <Messenger.h>
#include <Screen.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <TimeFormat.h>
#include <Window.h>

#include <ControlLook.h>
#include <math.h>
#include <stdlib.h>

#include <algorithm>

#include <stdio.h>
#include <string.h>

#include "ShojiDateControl.h"

static const uint32 kDateChanged = 'sdtD';
static const uint32 kTimeChanged = 'sdtT';
static const uint32 kAllDayChanged = 'sdtA';
static const uint32 kOpenCalendar = 'sdtO';
static const uint32 kDayPicked = 'sdtP';

// --- the time of day: the field of the system does not look or act disabled (the sections take the mouse and the keys and look the same), so a time that does not count (all day) is dimmed and left alone

class ShojiTimeEdit : public BPrivate::TimeEdit {
public:
    ShojiTimeEdit(const char* name, uint32 sections, BMessage* message)
        :
        BPrivate::TimeEdit(name, sections, message)
    {
    }

    virtual void Draw(BRect updateRect)
    {
        BPrivate::TimeEdit::Draw(updateRect);
        if (!IsEnabled()) {
            PushState();
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
            panel.alpha = 150;
            SetHighColor(panel);
            FillRect(Bounds());
            PopState();
        }
    }

    virtual void MouseDown(BPoint where)
    {
        if (IsEnabled())
            BPrivate::TimeEdit::MouseDown(where);
    }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (IsEnabled())
            BPrivate::TimeEdit::KeyDown(bytes, numBytes);
    }

    virtual void MakeFocus(bool focused = true)
    {
        BPrivate::TimeEdit::MakeFocus(focused && IsEnabled());
    }
};

// --- the button that opens the calendar: a small button with the filled arrow that the date field has (BControlLook::DrawArrowShape(), as in the lists of Tracker)

class PickButton : public BButton {
public:
    PickButton(const char* name, BMessage* message)
        :
        BButton(name, "", message)
    {
        // as wide as the arrow needs, and as high as a text field: not a tall button next to it
        const float height = ceilf(be_plain_font->Size() * 1.4f) + 8;
        SetExplicitMinSize(BSize(ceilf(be_plain_font->Size() * 1.6f) + 6, height));
        SetExplicitMaxSize(BSize(ceilf(be_plain_font->Size() * 1.6f) + 6, height));
        SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_MIDDLE));
    }

    virtual void Draw(BRect updateRect)
    {
        BButton::Draw(updateRect);
        const BRect bounds = Bounds();
        const float side = std::max(4.0f, floorf(be_plain_font->Size() * 0.7f));
        const float x = floorf(bounds.left + (bounds.Width() + 1 - side) / 2);
        const float y = floorf(bounds.top + (bounds.Height() + 1 - side) / 2);
        const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
        float tint = IsEnabled() ? B_DARKEN_4_TINT : B_DARKEN_2_TINT;
        if (base.red + base.green + base.blue <= 128 * 3)
            tint = IsEnabled() ? B_LIGHTEN_2_TINT : B_DARKEN_1_TINT;
        PushState();
        BRect arrow(x, y, x + side - 1, y + side - 1);
        be_control_look->DrawArrowShape(this, arrow, updateRect, base, BControlLook::B_DOWN_ARROW, 0, tint);
        PopState();
    }
};

// --- the calendar that drops down from the date, as the one of the Deskbar (the clock) has: a month with its days, to be picked from, and the buttons for the month and the year

class CalendarWindow : public BWindow {
public:
    CalendarWindow(BPoint where, const BDate& date, const BMessenger& target)
        :
        // (no window frame: it is a part of the form that drops down; the line around it is drawn by the content)
        BWindow(BRect(0.0, 0.0, 100.0, 130.0), "", B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
            B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS | B_CLOSE_ON_ESCAPE | B_NOT_MINIMIZABLE | B_NOT_MOVABLE | B_NOT_RESIZABLE | B_NOT_ZOOMABLE),
        fTarget(target)
    {

        fMonth = new BStringView("month", "");
        fYear = new BStringView("year", "");
        fView = new BPrivate::BCalendarView(Bounds(), "calendar", B_FOLLOW_ALL);
        fView->SetSelectionMessage(new BMessage(kDayPicked));
        fView->SetInvocationMessage(new BMessage(kDayPicked));

        BGroupLayout* outer = new BGroupLayout(B_VERTICAL, 0.0);
        outer->SetInsets(0, 0, 0, 0);
        SetLayout(outer);
        BBox* frame = new BBox("frame", B_WILL_DRAW, B_PLAIN_BORDER);
        BGroupLayout* layout = new BGroupLayout(B_VERTICAL, 5.0);
        layout->SetInsets(5.0, 5.0, 5.0, 5.0);
        frame->SetLayout(layout);
        outer->AddView(frame);

        BGroupLayout* bar = new BGroupLayout(B_HORIZONTAL, 5.0);
        layout->AddItem(bar);
        bar->AddView(_Button("-", kMonthDown));
        bar->AddView(fMonth);
        bar->AddView(_Button("+", kMonthUp));
        bar->AddItem(BSpaceLayoutItem::CreateGlue());
        bar->AddView(_Button("-", kYearDown));
        bar->AddView(fYear);
        bar->AddView(_Button("+", kYearUp));
        layout->AddView(fView);

        MoveTo(where);
        _Show(date);
    }

    virtual void Show()
    {
        // (it opens where it is asked to, and does not go off the screen)
        BRect screen(BScreen().Frame());
        BRect frame = Frame();
        const BSize size = GetLayout()->PreferredSize();
        BPoint where = frame.LeftTop();
        if (where.x + size.width > screen.right)
            where.x = screen.right - size.width;
        if (where.y + size.height > screen.bottom)
            where.y = frame.top - size.height - 30;
        MoveTo(where);
        fView->MakeFocus(true);
        BWindow::Show();
    }

    virtual void WindowActivated(bool active)
    {
        if (!active)
            PostMessage(B_QUIT_REQUESTED);
    }

    virtual void MessageReceived(BMessage* message)
    {
        switch (message->what) {
            case kDayPicked:
            {
                int32 day, month, year;
                if (message->FindInt32("day", &day) != B_OK || message->FindInt32("month", &month) != B_OK || message->FindInt32("year", &year) != B_OK)
                    break;
                BMessage picked(kDayPicked);
                picked.AddInt32("day", day);
                picked.AddInt32("month", month);
                picked.AddInt32("year", year);
                fTarget.SendMessage(&picked);
                PostMessage(B_QUIT_REQUESTED);
                break;
            }
            case kMonthDown:
            case kMonthUp:
            {
                BDate date = fView->Date();
                date.AddMonths(message->what == kMonthDown ? -1 : 1);
                _Show(date);
                break;
            }
            case kYearDown:
            case kYearUp:
            {
                BDate date = fView->Date();
                date.AddYears(message->what == kYearDown ? -1 : 1);
                _Show(date);
                break;
            }
            default:
                BWindow::MessageReceived(message);
        }
    }

private:
    enum { kMonthDown = 'cwmd', kMonthUp = 'cwmu', kYearDown = 'cwyd', kYearUp = 'cwyu' };

    BButton* _Button(const char* label, uint32 what)
    {
        BButton* button = new BButton(label, label, new BMessage(what));
        float width, height;
        fMonth->GetPreferredSize(&width, &height);
        button->SetExplicitMinSize(BSize(height + 6, height + 2));
        button->SetExplicitMaxSize(BSize(height + 6, height + 2));
        return button;
    }

    void _Show(const BDate& date)
    {
        if (!date.IsValid())
            return;
        fView->SetDate(date);
        BString year;
        year << date.Year();
        fYear->SetText(year.String());
        fMonth->SetText(date.LongMonthName(date.Month()).String());
    }

    BMessenger                  fTarget;
    BStringView*                fMonth;
    BStringView*                fYear;
    BPrivate::BCalendarView*    fView;
};

// a day, with a time of day: the parts of the text (the time is -1 if there is none)
static bool ParseDateTime(const char* text, BString* day, int* hour, int* minute, int* second)
{
    int year, month, date, used = 0;
    *hour = *minute = *second = -1;
    if (text == NULL || sscanf(text, "%d-%d-%d%n", &year, &month, &date, &used) < 3 || !BDate(year, month, date).IsValid())
        return false;
    day->SetToFormat("%04d-%02d-%02d", year, month, date);
    if (text[used] == '\0')
        return true;
    if (text[used] != ' ')
        return false;
    int h = 0, m = 0, s = 0, usedTime = 0;
    const char* clock = text + used + 1;
    int parts = sscanf(clock, "%d:%d:%d%n", &h, &m, &s, &usedTime);
    if (parts == 2)
        sscanf(clock, "%d:%d%n", &h, &m, &usedTime);
    else if (parts != 3)
        return false;
    if (clock[usedTime] != '\0' || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
        return false;
    *hour = h;
    *minute = m;
    *second = parts == 3 ? s : 0;
    return true;
}


ShojiDateTimeControl::ShojiDateTimeControl(const char* name, BMessage* message)
    :
    BControl(name, "", message, B_WILL_DRAW | B_NAVIGABLE_JUMP),
    fHour(9),
    fMinute(0),
    fSeconds(0),
    fSetting(false),
    fEnabled(true)
{
    // (the control draws nothing itself: its background is the panel's, or what was under it (the calendar that dropped down, a window that was over the form) stays)
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    BGroupLayout* layout = new BGroupLayout(B_HORIZONTAL, B_USE_DEFAULT_SPACING);
    layout->SetInsets(0, 0, 0, 0);
    SetLayout(layout);

    // the date control starts with no date (the value of the attribute is then none); picking a day makes the value a day
    fDate = new ShojiDateControl("date", new BMessage(kDateChanged));
    fDate->SetUnsettable(true);
    fDate->SetUnset();

    // hours and minutes (and the marker of the half day if the locale has one)
    bool halfDay = false;
    BTimeFormat format;
    BDateElement* fields = NULL;
    int fieldCount = 0;
    if (format.GetTimeFields(fields, fieldCount, B_SHORT_TIME_FORMAT) == B_OK) {
        for (int i = 0; i < fieldCount; i++)
            halfDay = halfDay || fields[i] == B_DATE_ELEMENT_AM_PM;
    }
    free(fields);
    fTime = new ShojiTimeEdit("time", halfDay ? 3 : 2, new BMessage(kTimeChanged));
    fTime->SetTime(9, 0, 0);

    fAllDay = new BCheckBox("allday", "All day", new BMessage(kAllDayChanged));
    fAllDay->SetValue(B_CONTROL_ON);

    // the calendar that drops down from the date (a day is picked there as in the clock of the Deskbar)
    fPick = new PickButton("pick", new BMessage(kOpenCalendar));

    layout->AddView(fDate);
    layout->AddView(fPick);
    layout->AddView(fTime);
    layout->AddView(fAllDay);
    _UpdateEnabled();
}


ShojiDateTimeControl::~ShojiDateTimeControl()
{
}


void ShojiDateTimeControl::AttachedToWindow()
{
    BControl::AttachedToWindow();
    // (the parts tell this control what the user changed)
    fDate->SetTarget(this);
    fTime->SetTarget(this);
    fAllDay->SetTarget(this);
    fPick->SetTarget(this);
}


void ShojiDateTimeControl::AllAttached()
{
    BControl::AllAttached();
    // (the time field starts with the time of now when it is attached: what the value says is shown again)
    fSetting = true;
    fTime->SetTime(fHour, fMinute, 0);
    fSetting = false;
}


void ShojiDateTimeControl::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kTimeChanged:
        {
            const BTime time = fTime->GetTime();
            fHour = time.Hour();
            fMinute = time.Minute();
            _UpdateEnabled();
            _Changed();
            break;
        }
        case kDateChanged:
        case kAllDayChanged:
            _UpdateEnabled();
            _Changed();
            break;
        case kOpenCalendar:
        {
            BDate date = fDate->IsUnset() ? BDate::CurrentDate(B_LOCAL_TIME) : BDate();
            if (!fDate->IsUnset()) {
                int year, month, day;
                if (sscanf(fDate->DateText().String(), "%d-%d-%d", &year, &month, &day) == 3)
                    date.SetDate(year, month, day);
            }
            BPoint where = fDate->ConvertToScreen(BPoint(0, fDate->Bounds().bottom + 2));
            CalendarWindow* calendar = new CalendarWindow(where, date, BMessenger(this));
            calendar->Show();
            break;
        }
        case kDayPicked:
        {
            int32 day, month, year;
            if (message->FindInt32("day", &day) != B_OK || message->FindInt32("month", &month) != B_OK || message->FindInt32("year", &year) != B_OK)
                break;
            BString text;
            text.SetToFormat("%04d-%02d-%02d", (int)year, (int)month, (int)day);
            fSetting = true;
            fDate->SetDateText(text.String());
            fSetting = false;
            _UpdateEnabled();
            _Changed();
            break;
        }
        default:
            BControl::MessageReceived(message);
    }
}


void ShojiDateTimeControl::SetEnabled(bool enabled)
{
    fEnabled = enabled;
    BControl::SetEnabled(enabled);
    _UpdateEnabled();
}


void ShojiDateTimeControl::_UpdateEnabled()
{
    const bool hasDate = !fDate->IsUnset();
    fDate->SetEnabled(fEnabled);
    fPick->SetEnabled(fEnabled);
    fAllDay->SetEnabled(fEnabled && hasDate);
    fTime->SetEnabled(fEnabled && hasDate && fAllDay->Value() != B_CONTROL_ON);
}


void ShojiDateTimeControl::_Changed()
{
    if (!fSetting)
        Invoke();
}


bool ShojiDateTimeControl::IsDateTimeText(const char* text)
{
    BString day;
    int hour, minute, second;
    return ParseDateTime(text, &day, &hour, &minute, &second);
}


BString ShojiDateTimeControl::DateTimeText()
{
    if (fDate->IsUnset())
        return BString();
    BString text = fDate->DateText();
    if (fAllDay->Value() != B_CONTROL_ON) {
        const BTime time = fTime->GetTime();
        BString clock;
        clock.SetToFormat(" %02d:%02d:%02d", (int)time.Hour(), (int)time.Minute(), (int)fSeconds);
        text << clock;
    }
    return text;
}


bool ShojiDateTimeControl::SetDateTimeText(const char* text)
{
    if (text == NULL)
        return false;
    if (text[0] == '\0') {
        SetUnset();
        return true;
    }
    BString day;
    int hour, minute, second;
    if (!ParseDateTime(text, &day, &hour, &minute, &second))
        return false;
    // (what is shown already is left alone: the user may be in the middle of changing it)
    if (DateTimeText() == text)
        return true;
    fSetting = true;
    fDate->SetDateText(day.String());
    if (hour >= 0) {
        fSeconds = second;
        fHour = hour;
        fMinute = minute;
        fTime->SetTime(hour, minute, 0);
        fAllDay->SetValue(B_CONTROL_OFF);
    } else {
        fAllDay->SetValue(B_CONTROL_ON);
    }
    fSetting = false;
    _UpdateEnabled();
    return true;
}


void ShojiDateTimeControl::SetUnset()
{
    fSetting = true;
    fDate->SetUnset();
    fAllDay->SetValue(B_CONTROL_ON);
    fSeconds = 0;
    fSetting = false;
    _UpdateEnabled();
}
