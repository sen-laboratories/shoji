/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "EventView.h"

#include <TextControl.h>
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "EventView"


static const char* kTimeHelp = "A time (2026-11-14 10:00), a day (2026-11-14), or a time that is not exact: 1994-05~ (about), 1994? (uncertain), 202X (some year of the 2020s)";

EventView::EventView() : FormView("Event View") {
    AddField("dc:title", B_TRANSLATE("Title"));
    // the kind of the event: text; the common kinds (the ontology has them, with the flag whether other text is allowed) are offered by the window as the user types
    AddField("SEN:eventType", B_TRANSLATE("Event Type"));
    AddField("dc:description", B_TRANSLATE("Description"));
    AddField("schema:startDate", B_TRANSLATE("Start"))->SetToolTip(kTimeHelp);
    AddField("schema:endDate", B_TRANSLATE("End"))->SetToolTip(kTimeHelp);
    // the clock time of the duration is shown by the control for durations, which takes the place of this field when the file is shown
    AddField("schema:duration", B_TRANSLATE("Duration"));
    // how it repeats, in words (the rule itself is in SEN:recurrence); the other occurrences are the relation Series
    AddField("schema:repeatFrequency", B_TRANSLATE("Repeats"));
    AddField("schema:location", B_TRANSLATE("Location"))->SetToolTip(B_TRANSLATE("Where the event takes place. Click the label to look the place up and relate the event to it."));
    AddField("schema:organizer", B_TRANSLATE("Organizer"));
    // the people of the event, one entry each with the role and the answer in brackets (the people themselves are the relation Attendee); the file is the source of it
    AddField("schema:attendee", B_TRANSLATE("Attendees"));
    AddField("schema:eventStatus", B_TRANSLATE("Status"));
    AddField("schema:eventAttendanceMode", B_TRANSLATE("Attendance"));
    // links: the page of the event and the online meeting (click the label)
    AddField("schema:url", B_TRANSLATE("URL"));
    AddField("SEN:meetingUrl", B_TRANSLATE("Meeting URL"));
    AddField("SEN:meetingType", B_TRANSLATE("Meeting Type"));
    Finish();
}

EventView::~EventView()
{
}
