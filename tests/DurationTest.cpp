/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The durations of Shoji (src/app/ShojiDuration.h): plain C++, runs everywhere.
//   c++ -std=c++17 -Wall -I src/app tests/DurationTest.cpp -o /tmp/duration-test && /tmp/duration-test

#include "ShojiDuration.h"

#include <stdio.h>

static int sChecks = 0, sFailures = 0;
#define CHECK(x) do { sChecks++; if (!(x)) { sFailures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static std::string Clock(const char* iso) { std::string c; return shoji::DurationToClock(iso, &c) ? c : "-"; }
static std::string Iso(const char* clock) { std::string i; return shoji::ClockToDuration(clock, &i) ? i : "-"; }

int main()
{
	CHECK(Clock("PT2H") == "02:00:00");
	CHECK(Clock("PT1H55M") == "01:55:00");
	CHECK(Clock("PT90M") == "01:30:00");
	CHECK(Clock("PT45S") == "00:00:45");
	CHECK(Clock("PT1H2M3S") == "01:02:03");
	CHECK(Clock("P1DT1H") == "25:00:00");
	CHECK(Clock("P1W") == "168:00:00");
	CHECK(Clock("P1Y") == "-" && Clock("P1M") == "-" && Clock("PT1.5S") == "-");		// (no clock form)
	CHECK(Clock("PT") == "-" && Clock("P") == "-" && Clock("2h") == "-" && Clock("") == "-" && Clock("PT1X") == "-");

	CHECK(Iso("02:00:00") == "PT2H");
	CHECK(Iso("1:55:00") == "PT1H55M");
	CHECK(Iso("00:00:45") == "PT45S");
	CHECK(Iso("01:02:03") == "PT1H2M3S");
	CHECK(Iso("00:00:00") == "PT0S");
	CHECK(Iso(" 2:00:00 ") == "PT2H");
	CHECK(Iso("120:00:00") == "PT120H");
	CHECK(Iso("2:00") == "-" && Iso("2") == "-" && Iso("0:60:00") == "-" && Iso("0:00:60") == "-" && Iso("a:b:c") == "-" && Iso("1:2:3:4") == "-" && Iso("") == "-");

	// what is shown and typed comes back the same
	for (const char* iso : {"PT2H", "PT1H55M", "PT45S", "PT1H2M3S"})
		CHECK(Iso(Clock(iso).c_str()) == iso);
	CHECK(Iso(Clock("PT90M").c_str()) == "PT1H30M");		// (the normal form)

	CHECK(shoji::IsIsoDuration("PT2H") && shoji::IsIsoDuration("P1Y2M3DT4H5M6S") && shoji::IsIsoDuration("PT1.5S") && shoji::IsIsoDuration("P1W"));
	CHECK(!shoji::IsIsoDuration("") && !shoji::IsIsoDuration("P") && !shoji::IsIsoDuration("PT") && !shoji::IsIsoDuration("2:00:00") && !shoji::IsIsoDuration("PT2M1H")
		&& !shoji::IsIsoDuration("abc") && !shoji::IsIsoDuration("P1.5D"));

	CHECK(shoji::IsDurationAttribute("schema:duration") && shoji::IsDurationAttribute("dc:duration") && !shoji::IsDurationAttribute("schema:durationX") && !shoji::IsDurationAttribute("duration"));

	printf("%d checks, %d failed\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
