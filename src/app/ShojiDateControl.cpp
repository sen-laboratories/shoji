/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDateControl.h"

#include <ControlLook.h>
#include <DateTime.h>
#include <Font.h>

#include <stdio.h>

ShojiDateControl::ShojiDateControl(const char* name, BMessage* message)
    :
    BPrivate::DateEdit(name, 3, message),
    fHasDate(false),
    fYear(0),
    fMonth(0),
    fDay(0)
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

    BRect bounds(Bounds());
    bounds.left = bounds.right - 16;		// (the width of the area of the arrows)
    bounds.right = Bounds().right - 2;
    BRect up(bounds.left + 3, bounds.top + 2, bounds.right, bounds.bottom / 2.0);
    BRect down = up.OffsetByCopy(0, up.Height() + 2);

    // (DrawArrowShape() sets the colour of the pen to that of the arrow, and the sections are drawn with the pen of the view: the state is put back)
    PushState();
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    FillRect(up.InsetByCopy(-1, -1) | down.InsetByCopy(-1, -1), B_SOLID_LOW);

    // (as the latch of a list in Tracker: the colour of the panel as the base, a dark tint of it)
    const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    const float tint = IsEnabled() ? B_DARKEN_4_TINT : B_DARKEN_2_TINT;
    up.InsetBy(-1, -1);
    down.InsetBy(-1, -1);
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


bool ShojiDateControl::SetDateText(const char* text)
{
    int year, month, day, used = 0;
    if (text == NULL || sscanf(text, "%d-%d-%d%n", &year, &month, &day, &used) < 3 || text[used] != '\0'
            || !BDate(year, month, day).IsValid())
        return false;
    fHasDate = true;
    fYear = year;
    fMonth = month;
    fDay = day;
    SetDate(year, month, day);
    return true;
}


BString ShojiDateControl::DateText()
{
    const BDate date = GetDate();
    BString text;
    text.SetToFormat("%04d-%02d-%02d", (int)date.Year(), (int)date.Month(), (int)date.Day());
    return text;
}
