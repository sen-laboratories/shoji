/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

/**
 * @file ShojiDuration.h
 * @brief Durations (`schema:duration`: an ISO 8601 duration like `PT2H` or `PT1H55M`) as the clock time that people read and type, `HH:mm:ss`. Plain C++, no Haiku.
 *
 * The attribute keeps the ISO 8601 text (what schema.org and the plugins write); Shoji shows and edits `02:00:00` and stores the ISO form again. A duration with years or months
 * (which have no length of their own), or with fractions of seconds, cannot be shown as a clock: it is shown as it is and kept as it is until the user types a clock time.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

namespace shoji {

/** Whether an attribute holds a duration, by its name (`schema:duration`, `dc:duration`...). */
inline bool
IsDurationAttribute(const std::string& name)
{
	const std::string ending = ":duration";
	return name.size() >= ending.size() && name.compare(name.size() - ending.size(), ending.size(), ending) == 0;
}


/** The seconds of an ISO 8601 duration that has a length of its own: `P[n]W[n]D[T[n]H[n]M[n]S]`. */
inline bool
IsoDurationSeconds(const std::string& text, long long* seconds)
{
	if (text.size() < 3 || text[0] != 'P')
		return false;
	size_t i = 1;
	bool timePart = false, any = false;
	long long total = 0;
	while (i < text.size()) {
		if (text[i] == 'T') {
			if (timePart)
				return false;
			timePart = true;
			i++;
			continue;
		}
		const size_t start = i;
		while (i < text.size() && isdigit((unsigned char)text[i]))
			i++;
		if (i == start || i >= text.size() || i - start > 9)
			return false;
		const long long n = atoll(text.substr(start, i - start).c_str());
		const char unit = text[i++];
		if (!timePart) {
			if (unit == 'W')
				total += n * 7 * 86400;
			else if (unit == 'D')
				total += n * 86400;
			else
				return false;
		} else {
			if (unit == 'H')
				total += n * 3600;
			else if (unit == 'M')
				total += n * 60;
			else if (unit == 'S')
				total += n;
			else
				return false;
		}
		any = true;
	}
	if (!any)
		return false;
	*seconds = total;
	return true;
}


/** Whether the text is an ISO 8601 duration at all (years, months and fractions of seconds included): what may be stored. */
inline bool
IsIsoDuration(const std::string& text)
{
	if (text.size() < 3 || text[0] != 'P')
		return false;
	// P[nY][nM][nW][nD][T[nH][nM][n[.n]S]] in this order, at least one number
	static const char dateUnits[] = "YMWD";
	static const char timeUnits[] = "HMS";
	size_t i = 1;
	bool inTime = false, any = false;
	const char* units = dateUnits;
	int last = -1;
	while (i < text.size()) {
		if (text[i] == 'T') {
			if (inTime)
				return false;
			inTime = true;
			units = timeUnits;
			last = -1;
			i++;
			continue;
		}
		const size_t start = i;
		while (i < text.size() && (isdigit((unsigned char)text[i]) || (text[i] == '.' && inTime)))
			i++;
		if (i == start || i >= text.size())
			return false;
		const char unit = text[i++];
		const char* found = NULL;
		for (const char* u = units; *u != '\0'; u++) {
			if (*u == unit)
				found = u;
		}
		if (found == NULL || (int)(found - units) <= last)
			return false;
		const std::string number = text.substr(start, i - 1 - start);
		if (number.find('.') != std::string::npos && unit != 'S')
			return false;
		last = (int)(found - units);
		any = true;
	}
	return any;
}


/** `PT2H` -> `02:00:00`. False if the duration has no clock form (the text is not changed then). */
inline bool
DurationToClock(const std::string& iso, std::string* clock)
{
	long long seconds = 0;
	if (!IsoDurationSeconds(iso, &seconds))
		return false;
	char text[64];
	snprintf(text, sizeof(text), "%02lld:%02lld:%02lld", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
	*clock = text;
	return true;
}


/** `1:55:00` or `01:55:00` -> `PT1H55M`. Minutes and seconds are below 60. False if the text is no clock time. */
inline bool
ClockToDuration(const std::string& clock, std::string* iso)
{
	long long parts[3] = {0, 0, 0};
	int count = 0;
	size_t i = 0;
	while (i < clock.size() && isspace((unsigned char)clock[i]))
		i++;
	while (count < 3) {
		const size_t start = i;
		while (i < clock.size() && isdigit((unsigned char)clock[i]))
			i++;
		if (i == start || i - start > 6)
			return false;
		parts[count++] = atoll(clock.substr(start, i - start).c_str());
		if (count < 3) {
			if (i >= clock.size() || clock[i] != ':')
				return false;
			i++;
		}
	}
	while (i < clock.size() && isspace((unsigned char)clock[i]))
		i++;
	if (i != clock.size() || parts[1] > 59 || parts[2] > 59)
		return false;

	std::string text = "PT";
	if (parts[0] > 0)
		text += std::to_string(parts[0]) + "H";
	if (parts[1] > 0)
		text += std::to_string(parts[1]) + "M";
	if (parts[2] > 0)
		text += std::to_string(parts[2]) + "S";
	if (text == "PT")
		text = "PT0S";
	*iso = text;
	return true;
}

}	// namespace shoji
