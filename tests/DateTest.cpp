/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// Dates and times of a B_TIME_TYPE attribute: a value at midnight UTC (what the plugins store for a publication or birth date) is shown as the day, a typed day is stored the
// same way, and a time of day stays a time of day in local time. Build and run on Haiku:
//   g++ -std=c++20 -Isrc/app -I/boot/home/config/non-packaged/develop/headers tests/DateTest.cpp src/app/ShojiAttributes.cpp src/app/ShojiListControl.cpp -o /tmp/date-test -lbe && /tmp/date-test

#include "ShojiAttributes.h"

#include <File.h>
#include <fs_attr.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int sFailures = 0;
static int sChecks = 0;

#define CHECK(condition) \
    do { \
        sChecks++; \
        if (!(condition)) { \
            sFailures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while (0)

#define CHECK_TEXT(got, expected) \
    do { \
        sChecks++; \
        if (BString(got) != BString(expected)) { \
            sFailures++; \
            printf("FAIL %s:%d: %s\n   got: '%s'\n   expected: '%s'\n", __FILE__, __LINE__, #got, BString(got).String(), expected); \
        } \
    } while (0)

static ShojiAttribute TimeAttribute()
{
    ShojiAttribute attr;
    attr.name = "test:date";
    attr.label = "Date";
    attr.type = B_TIME_TYPE;
    attr.viewable = true;
    attr.editable = true;
    return attr;
}

static BString Shown(BNode& node, ShojiAttribute* attr)
{
    BString text;
    CHECK(shoji::LoadAttribute(node, attr, &text) == B_OK);
    return text;
}

static void WriteSeconds(BNode& node, int64 seconds)
{
    time_t time = (time_t)seconds;
    node.WriteAttr("test:date", B_TIME_TYPE, 0, &time, sizeof(time));
}

// what the local time of a moment looks like, in the time zone of the machine that runs the test
static BString Local(int64 seconds, const char* format)
{
    time_t time = (time_t)seconds;
    struct tm parts;
    localtime_r(&time, &parts);
    char buffer[64];
    strftime(buffer, sizeof(buffer), format, &parts);
    return buffer;
}

static int64 ReadSeconds(BNode& node)
{
    time_t time = 0;
    node.ReadAttr("test:date", B_TIME_TYPE, 0, &time, sizeof(time));
    return (int64)time;
}

int main()
{
    const char* path = "/tmp/shoji-date-test";
    BFile file(path, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    CHECK(file.InitCheck() == B_OK);
    ShojiAttribute attr = TimeAttribute();

    // days that a plugin stored (midnight UTC), in the time zones where local midnight is another day
    WriteSeconds(file, -473385600);     // 1955-01-01 (a day before the epoch is negative too)
    CHECK_TEXT(Shown(file, &attr), "1955-01-01");
    WriteSeconds(file, 1114041600);     // 2005-04-21
    CHECK_TEXT(Shown(file, &attr), "2005-04-21");
    WriteSeconds(file, 988675200);      // 2001-05-01, which is the day before or the same day in local time, whatever the zone
    CHECK_TEXT(Shown(file, &attr), "2001-05-01");
    WriteSeconds(file, 0);
    CHECK_TEXT(Shown(file, &attr), "1970-01-01");
    WriteSeconds(file, -3 * 86400 * 365);   // before the epoch, a whole day
    CHECK_TEXT(Shown(file, &attr), "1967-01-02");

    // a time of day is shown in local time, with the time
    WriteSeconds(file, 988675200 + 3600 * 10);      // 10:00 UTC
    CHECK_TEXT(Shown(file, &attr), Local(988675200 + 3600 * 10, "%Y-%m-%d %H:%M:%S"));

    // a typed day is stored as midnight UTC, and shown as typed
    CHECK(shoji::StoreAttribute(file, attr, "2001-05-01") == B_OK);
    CHECK(ReadSeconds(file) == 988675200);
    CHECK_TEXT(Shown(file, &attr), "2001-05-01");
    CHECK(shoji::StoreAttribute(file, attr, "1854-10-16") == B_OK);
    CHECK(ReadSeconds(file) % 86400 == 0);
    CHECK_TEXT(Shown(file, &attr), "1854-10-16");

    // a typed time stays a time (local), and comes back as typed
    CHECK(shoji::StoreAttribute(file, attr, "2001-05-01 12:00:00") == B_OK);
    CHECK_TEXT(Local(ReadSeconds(file), "%Y-%m-%d %H:%M:%S"), "2001-05-01 12:00:00");
    CHECK_TEXT(Shown(file, &attr), "2001-05-01 12:00:00");
    CHECK(shoji::StoreAttribute(file, attr, "2001-05-01 12:30") == B_OK);
    CHECK_TEXT(Shown(file, &attr), "2001-05-01 12:30:00");

    // nonsense is refused
    CHECK(shoji::StoreAttribute(file, attr, "yesterday") != B_OK);

    remove(path);
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
