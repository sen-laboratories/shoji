/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "EventView.h"

#include <TextControl.h>

static const char* kTimeHelp = "A time (2026-11-14 10:00), a day (2026-11-14), or a time that is not exact: 1994-05~ (about), 1994? (uncertain), 202X (some year of the 2020s)";

EventView::EventView() : FormView("Event View") {
    AddField("dc:title", "Title");
    // the kind of the event: text; the common kinds (the ontology has them, with the flag whether other text is allowed) are offered by the window as the user types
    AddField("SEN:eventType", "Event Type");
    AddField("dc:description", "Description");
    AddField("schema:startDate", "Start")->SetToolTip(kTimeHelp);
    AddField("schema:endDate", "End")->SetToolTip(kTimeHelp);
    // the clock time of the duration is shown by the control for durations, which takes the place of this field when the file is shown
    AddField("schema:duration", "Duration");
    Finish();
}

EventView::~EventView()
{
}
