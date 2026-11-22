/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The date control: the fields of the locale, typing as it is written, the separator, a day that is not set. Build and run on Haiku (the locale of the user decides the order of the fields:
// the checks are for day.month.year as in de_AT, and are skipped for another order):
//   g++ -std=c++20 -Isrc/app -I/boot/home/config/non-packaged/develop/headers -I/boot/system/develop/headers/private/shared tests/DateControlTest.cpp src/app/ShojiDateControl.cpp -o /tmp/datecontrol-test -lbe -lshared && /tmp/datecontrol-test

#include "ShojiDateControl.h"

#include <Application.h>
#include <Window.h>

#include <stdio.h>
#include <string.h>

static int sFailures = 0, sChecks = 0;

#define CHECK_TEXT(got, expected) \
    do { \
        sChecks++; \
        if (BString(got) != BString(expected)) { \
            sFailures++; \
            printf("FAIL %s:%d: %s\n   got: '%s'\n   expected: '%s'\n", __FILE__, __LINE__, #got, BString(got).String(), expected); \
        } \
    } while (0)

static void Type(ShojiDateControl* control, const char* keys)
{
    for (const char* key = keys; *key != '\0'; key++) {
        char one[2] = { *key, '\0' };
        control->KeyDown(one, 1);
    }
}

int main()
{
    BApplication app("application/x-vnd.sen-labs-datecontrol-test");
    BWindow* window = new BWindow(BRect(100, 100, 400, 200), "test", B_TITLED_WINDOW, 0);
    window->Lock();
    ShojiDateControl* date = new ShojiDateControl("date");
    window->AddChild(date);
    date->SetUnsettable(true);

    // typed as it is written, from a date that is there
    date->SetDateText("2026-11-16");
    date->BPrivate::DateEdit::SectionFocus(0);
    Type(date, "17.05.1980");
    CHECK_TEXT(date->DateText(), "1980-05-17");

    // a single digit that cannot take another moves on (5 is no start of a day), and the separator that follows is its own
    date->SetDateText("2026-11-16");
    date->BPrivate::DateEdit::SectionFocus(0);
    Type(date, "5.5.1980");
    CHECK_TEXT(date->DateText(), "1980-05-05");

    // a digit that can take another waits for it, the separator ends the field: 1.2.1980
    date->SetDateText("2026-11-16");
    date->BPrivate::DateEdit::SectionFocus(0);
    Type(date, "1.2.1980");
    CHECK_TEXT(date->DateText(), "1980-02-01");

    // a value that the field cannot have starts the field again (35 -> 5), February has no 31st
    date->SetDateText("2026-11-16");
    date->BPrivate::DateEdit::SectionFocus(0);
    Type(date, "35");
    CHECK_TEXT(date->DateText(), "2026-11-05");
    date->SetDateText("2024-01-31");
    date->BPrivate::DateEdit::SectionFocus(1);
    Type(date, "02");
    CHECK_TEXT(date->DateText(), "2024-02-29");

    // no date: the text is empty, a digit starts it at today, Delete takes the date away again
    date->SetUnset();
    CHECK_TEXT(date->DateText(), "");
    CHECK_TEXT(date->IsUnset() ? "unset" : "set", "unset");
    Type(date, "1");
    CHECK_TEXT(date->IsUnset() ? "unset" : "set", "set");
    Type(date, "7.05.1980");
    CHECK_TEXT(date->DateText(), "1980-05-17");
    char deleteKey = B_DELETE;
    date->KeyDown(&deleteKey, 1);
    CHECK_TEXT(date->DateText(), "");

    window->Unlock();
    printf("%d checks, %d failures\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
