/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDateControl.h"

#include <ControlLook.h>
#include <DateTime.h>
#include <DateFormat.h>
#include <Font.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

ShojiDateControl::ShojiDateControl(const char* name, BMessage* message)
    :
    BPrivate::DateEdit(name, 3, message),
    fHasDate(false),
    fYear(0),
    fMonth(0),
    fDay(0),
    fTypedAt(0),
    fSkipSeparator(false),
    fUnsettable(false),
    fUnset(false),
    fPending(false)
{
    // the short date format of the locale: the order and the separator as configured; the year with all its digits
    BString pattern;
    if (fFormat.GetDateFormat(B_SHORT_DATE_FORMAT, pattern) == B_OK && pattern.FindFirst("yy") >= 0 && pattern.FindFirst("yyy") < 0) {
        pattern.ReplaceAll("yy", "yyyy");
        fFormat.SetDateFormat(B_SHORT_DATE_FORMAT, pattern);
    }
}


ShojiDateControl::~ShojiDateControl()
{
}


void ShojiDateControl::AttachedToWindow()
{
    BPrivate::DateEdit::AttachedToWindow();
    if (fHasDate)
        SetDate(fYear, fMonth, fDay);
    // as wide as the date needs: it is no field to stretch. The sections are equally wide, and the year has four digits (the arrows sit at the right).
    BSize size = PreferredSize();
    const float wanted = 3 * (StringWidth("0000") + 6) + 2 * SeparatorWidth() + 24;
    if (size.width < wanted)
        size.width = wanted;
    SetExplicitMinSize(size);
    SetExplicitMaxSize(size);
}


void ShojiDateControl::DrawBorder(const BRect& updateRect)
{
    // the border, the field and the areas that the mouse hits (the base class makes them), then the arrows over its thin ones
    BPrivate::DateEdit::DrawBorder(updateRect);

    // The arrows are those that the list view of Tracker and of the settings window has (BOutlineListView::DrawLatch()): the filled triangles of BControlLook::DrawArrowShape(),
    // the colour of the panel as the base and a dark tint (a light one on a dark panel). Each is in its half of the area at the right, inside the border, and not larger than that.
    const BRect inner = Bounds().InsetByCopy(2, 2);
    const BRect area(inner.right - 13, inner.top, inner.right, inner.bottom);
    const float half = floorf(area.Height() / 2);
    const float side = std::max(4.0f, std::min(std::min(half - 2, area.Width() - 2), floorf(be_plain_font->Size() * 0.7f)));

    // (DrawArrowShape() sets the colour of the pen to that of the arrow, and the sections are drawn with the pen of the view: the state is put back)
    PushState();
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    FillRect(area, B_SOLID_LOW);

    const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    float tint = IsEnabled() ? B_DARKEN_4_TINT : B_DARKEN_2_TINT;
    if (base.red + base.green + base.blue <= 128 * 3)
        tint = IsEnabled() ? B_LIGHTEN_2_TINT : B_DARKEN_1_TINT;

    const float x = floorf(area.left + (area.Width() + 1 - side) / 2);
    BRect up(x, floorf(area.top + (half - side) / 2), x + side - 1, 0);
    up.bottom = up.top + side - 1;
    BRect down(x, floorf(area.top + half + (area.Height() + 1 - half - side) / 2), x + side - 1, 0);
    down.bottom = down.top + side - 1;
    be_control_look->DrawArrowShape(this, up, updateRect, base, BControlLook::B_UP_ARROW, 0, tint);
    be_control_look->DrawArrowShape(this, down, updateRect, base, BControlLook::B_DOWN_ARROW, 0, tint);
    PopState();
}


bool ShojiDateControl::IsDateText(const char* text)
{
    int year, month, day, used = 0;
    if (text == NULL || sscanf(text, "%d-%d-%d%n", &year, &month, &day, &used) < 3 || text[used] != '\0')
        return false;
    return BDate(year, month, day).IsValid();
}


bool ShojiDateControl::IsDayAttribute(const char* name)
{
    return name != NULL && (strcmp(name, "schema:birthDate") == 0 || strcmp(name, "schema:deathDate") == 0);
}


void ShojiDateControl::SetUnset()
{
    fUnset = true;
    fPending = false;
    if (Window() != NULL)
        Invalidate();
}


void ShojiDateControl::_Begin()
{
    if (!fUnset)
        return;
    fUnset = false;
    fPending = true;
    const BDate today = BDate::CurrentDate(B_LOCAL_TIME);
    SetDate(today.Year(), today.Month(), today.Day());
    BPrivate::DateEdit::SectionFocus(0);		// (the first field of the locale: typing starts there)
    fTyped = "";
    Invalidate();
}


void ShojiDateControl::MouseDown(BPoint where)
{
    if (fUnset && IsEnabled()) {
        // the first click only shows a date to start from (the arrows would otherwise move it by a day at once)
        MakeFocus(true);
        _Begin();
        return;
    }
    BPrivate::DateEdit::MouseDown(where);
}


void ShojiDateControl::KeyDown(const char* bytes, int32 numBytes)
{
    if (IsEnabled() && fUnsettable && numBytes == 1 && (bytes[0] == B_DELETE || bytes[0] == B_BACKSPACE)) {
        if (!fUnset) {
            SetUnset();
            BControl::Invoke();		// (the text is empty now: the attribute is removed)
        }
        return;
    }
    if (IsEnabled() && numBytes == 1 && (isdigit((unsigned char)bytes[0]) || bytes[0] == B_UP_ARROW || bytes[0] == B_DOWN_ARROW || _IsSeparator(bytes[0]))) {
        if (fUnset) {
            _Begin();
            if (bytes[0] == B_UP_ARROW || bytes[0] == B_DOWN_ARROW)
                return;
        }
        if (isdigit((unsigned char)bytes[0])) {
            fSkipSeparator = false;
            _Type(bytes[0] - '0');
            return;
        }
        if (bytes[0] != B_UP_ARROW && bytes[0] != B_DOWN_ARROW) {
            if (fSkipSeparator)
                fSkipSeparator = false;		// (the field moved on when it was full: this is its separator)
            else
                _Advance(false);			// (the separator: on to the next field)
            return;
        }
    }
    BPrivate::DateEdit::KeyDown(bytes, numBytes);
}


void ShojiDateControl::DoUpPress()
{
    if (fUnset) {
        _Begin();
        return;
    }
    BPrivate::DateEdit::DoUpPress();
}


void ShojiDateControl::DoDownPress()
{
    if (fUnset) {
        _Begin();
        return;
    }
    BPrivate::DateEdit::DoDownPress();
}


void ShojiDateControl::MakeFocus(bool focused)
{
    BPrivate::DateEdit::MakeFocus(focused);
    // started at today's date but nothing was changed: it is no date that the user chose
    if (!focused && fPending && fUnsettable)
        SetUnset();
}


status_t ShojiDateControl::Invoke(BMessage* message)
{
    fPending = false;
    return BPrivate::DateEdit::Invoke(message);
}


bool ShojiDateControl::_Layout(BString* text, int** positions, int* positionCount, BDateElement** kinds, int* kindCount)
{
    *positions = NULL;
    *kinds = NULL;
    *positionCount = *kindCount = 0;
    // (with no date the layout of today's: the dashes take the place of its digits)
    const time_t time = BDateTime(fUnset ? BDate::CurrentDate(B_LOCAL_TIME) : GetDate(), BTime()).Time_t();
    if (fFormat.Format(*text, *positions, *positionCount, time, B_SHORT_DATE_FORMAT) != B_OK || fFormat.GetFields(*kinds, *kindCount, B_SHORT_DATE_FORMAT) != B_OK) {
        free(*positions);
        free(*kinds);
        *positions = NULL;
        *kinds = NULL;
        return false;
    }
    return true;
}


void ShojiDateControl::DrawSection(uint32 index, BRect bounds, bool hasFocus)
{
    BString text;
    int* positions;
    int positionCount, kindCount;
    BDateElement* kinds;
    if (!_Layout(&text, &positions, &positionCount, &kinds, &kindCount))
        return;
    if ((int)(index * 2 + 1) < positionCount) {
        BString field;
        text.CopyCharsInto(field, positions[index * 2], positions[index * 2 + 1] - positions[index * 2]);
        if (fUnset) {
            // no date: dashes where the digits would be
            BString dashes;
            for (int32 i = 0; i < field.CountChars(); i++)
                dashes << "-";
            field = dashes;
        }
        PushState();
        if (hasFocus)
            SetLowColor(mix_color(ui_color(B_CONTROL_HIGHLIGHT_COLOR), ui_color(B_DOCUMENT_BACKGROUND_COLOR), 192));
        else
            SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        FillRect(bounds, B_SOLID_LOW);
        SetHighUIColor(B_DOCUMENT_TEXT_COLOR, fUnset ? B_LIGHTEN_1_TINT : B_NO_TINT);
        BPoint point(bounds.LeftBottom());
        point.y -= bounds.Height() / 2.0 - 6.0;
        point.x += (bounds.Width() - StringWidth(field)) / 2;
        DrawString(field, point);
        PopState();
    }
    free(positions);
    free(kinds);
}


void ShojiDateControl::DrawSeparator(uint32 index, BRect bounds)
{
    if (index >= 2)
        return;
    BString text;
    int* positions;
    int positionCount, kindCount;
    BDateElement* kinds;
    if (!_Layout(&text, &positions, &positionCount, &kinds, &kindCount))
        return;
    if ((int)(index * 2 + 2) < positionCount) {
        BString field;
        text.CopyCharsInto(field, positions[index * 2 + 1], positions[index * 2 + 2] - positions[index * 2 + 1]);
        PushState();
        SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
        BPoint point(bounds.LeftBottom());
        point.y -= bounds.Height() / 2.0 - 6.0;
        point.x += (bounds.Width() - StringWidth(field)) / 2;
        DrawString(field, point);
        PopState();
    }
    free(positions);
    free(kinds);
}


float ShojiDateControl::MinSectionWidth()
{
    return StringWidth("0000") + 6;
}


bool ShojiDateControl::_IsSeparator(char c)
{
    if (c == '.' || c == ',' || c == '/' || c == '-' || c == ' ')
        return true;
    // the separator of the locale, whatever it is (the text between the first two fields)
    BString text;
    int* positions;
    int positionCount, kindCount;
    BDateElement* kinds;
    bool same = false;
    if (_Layout(&text, &positions, &positionCount, &kinds, &kindCount)) {
        if (positionCount >= 3 && positions[2] - positions[1] == 1)
            same = text.ByteAt(positions[1]) == c;
        free(positions);
        free(kinds);
    }
    return same;
}


void ShojiDateControl::_Advance(bool automatic)
{
    fTyped = "";
    // (a field that moved on by itself leaves the separator that is typed next to nothing to do: 17.05.1980 is typed as it is written)
    fSkipSeparator = automatic;
    const int32 index = FocusIndex();
    if (index < 0)
        BPrivate::DateEdit::SectionFocus(0);
    else if (index < 2)
        BPrivate::DateEdit::SectionFocus(index + 1);
    Invalidate();
}


// A digit is typed into the field that has the focus: day and month take two digits, the year four, and the field moves on when it cannot take more (a 5 for the day: there is no 50).
void ShojiDateControl::_Type(int digit)
{
    BString text;
    int* positions;
    int positionCount, kindCount;
    BDateElement* kinds;
    if (!_Layout(&text, &positions, &positionCount, &kinds, &kindCount))
        return;
    free(positions);
    int32 index = FocusIndex();
    if (index < 0) {
        BPrivate::DateEdit::SectionFocus(0);
        index = 0;
    }
    const BDateElement kind = index < kindCount ? kinds[index] : B_DATE_ELEMENT_INVALID;
    free(kinds);
    if (kind != B_DATE_ELEMENT_DAY && kind != B_DATE_ELEMENT_MONTH && kind != B_DATE_ELEMENT_YEAR)
        return;

    const bigtime_t now = system_time();
    if (now - fTypedAt > 2000000)
        fTyped = "";
    fTypedAt = now;

    BDate date = GetDate();
    int32 year = date.Year(), month = date.Month(), day = date.Day();
    const int32 maxDigits = kind == B_DATE_ELEMENT_YEAR ? 4 : 2;
    BString candidate(fTyped);
    candidate << digit;
    if (candidate.Length() > maxDigits) {
        candidate.SetTo("");
        candidate << digit;
    }
    int32 value = atoi(candidate.String());
    // (a value that the field cannot have - 45 for a day - starts again with the digit)
    const int32 limit = kind == B_DATE_ELEMENT_DAY ? BDate(year, month, 1).DaysInMonth() : kind == B_DATE_ELEMENT_MONTH ? 12 : 9999;
    if (value > limit) {
        candidate.SetTo("");
        candidate << digit;
        value = digit;
    }
    fTyped = candidate;
    if (value < 1)
        return;		// (a 0 is the start of 05)

    switch (kind) {
        case B_DATE_ELEMENT_DAY:
            day = value;
            break;
        case B_DATE_ELEMENT_MONTH:
            month = value;
            break;
        default:
            year = value;
            break;
    }
    const int32 days = BDate(year, month, 1).DaysInMonth();
    if (day > days)
        day = days;
    SetDate(year, month, day);
    Invalidate();
    Invoke();

    const bool full = candidate.Length() >= maxDigits || (kind != B_DATE_ELEMENT_YEAR && value * 10 > limit);
    if (full)
        _Advance(true);
}


bool ShojiDateControl::SetDateText(const char* text)
{
    int year, month, day, used = 0;
    if (text == NULL || sscanf(text, "%d-%d-%d%n", &year, &month, &day, &used) < 3 || text[used] != '\0'
            || !BDate(year, month, day).IsValid())
        return false;
    fHasDate = true;
    fUnset = false;
    fPending = false;
    fYear = year;
    fMonth = month;
    fDay = day;
    SetDate(year, month, day);
    return true;
}


BString ShojiDateControl::DateText()
{
    if (fUnset)
        return BString();
    const BDate date = GetDate();
    BString text;
    text.SetToFormat("%04d-%02d-%02d", (int)date.Year(), (int)date.Month(), (int)date.Day());
    return text;
}
