/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDateControl.h"

#include <ControlLook.h>
#include <DateTime.h>
#include <Font.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

ShojiDateControl::ShojiDateControl(const char* name, BMessage* message)
    :
    BPrivate::DateEdit(name, 3, message),
    fHasDate(false),
    fYear(0),
    fMonth(0),
    fDay(0),
    fUnsettable(false),
    fUnset(false),
    fPending(false)
{
}


ShojiDateControl::~ShojiDateControl()
{
}


void ShojiDateControl::AttachedToWindow()
{
    BPrivate::DateEdit::AttachedToWindow();
    if (fHasDate)
        SetDate(fYear, fMonth, fDay);
    // as wide as the date needs: it is no field to stretch. The year has four digits (the control makes room for what it shows at first, and the arrows sit over the last field).
    BSize size = PreferredSize();
    const float wanted = StringWidth("00.00.0000") + 2 * be_control_look->DefaultLabelSpacing() + be_plain_font->Size() * 2;
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
    if (fUnset && IsEnabled() && numBytes >= 1 && (isdigit((unsigned char)bytes[0]) || bytes[0] == B_UP_ARROW || bytes[0] == B_DOWN_ARROW)) {
        _Begin();
        if (!isdigit((unsigned char)bytes[0]))
            return;
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


void ShojiDateControl::DrawSection(uint32 index, BRect bounds, bool hasFocus)
{
    if (!fUnset) {
        BPrivate::DateEdit::DrawSection(index, bounds, hasFocus);
        return;
    }
    // no date: dashes where the day, the month and the year are
    PushState();
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    FillRect(bounds, B_SOLID_LOW);
    SetHighUIColor(B_DOCUMENT_TEXT_COLOR, B_LIGHTEN_1_TINT);
    font_height height;
    GetFontHeight(&height);
    const char* text = "--";
    DrawString(text, BPoint(bounds.left + floorf((bounds.Width() - StringWidth(text)) / 2), bounds.top + floorf((bounds.Height() + height.ascent - height.descent) / 2) + 1));
    PopState();
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
