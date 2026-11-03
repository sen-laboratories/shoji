/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiDateControl.h"

#include <DateTime.h>

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
    // as wide as the date needs: it is no field to stretch
    const BSize size = PreferredSize();
    SetExplicitMinSize(size);
    SetExplicitMaxSize(size);
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
