/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDateControl.h"

#include <ControlLook.h>
#include <DateTime.h>
#include <Font.h>

#include <stdio.h>

#include <algorithm>

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
