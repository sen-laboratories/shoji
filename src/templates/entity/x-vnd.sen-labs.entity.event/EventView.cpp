/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "EventView.h"

#include <TextControl.h>

#include <sen/SenOntoTemporal.h>

static const char* kTimeHelp = "A time (2026-11-14 10:00), a day (2026-11-14), or a time that is not exact: 1994-05~ (about), 1994? (uncertain), 202X (some year of the 2020s)";

EventView::EventView() : FormView("Event View") {
    AddField("dc:title", "Title");
    // the kind of the event: text, with the common kinds to choose from (the list is that of the ontology; any other text can be typed)
    {
        namespace values = sen::onto::temporal::values;
        const char* kinds[values::kEventTypeCount];
        for (unsigned i = 0; i < values::kEventTypeCount; i++)
            kinds[i] = values::kEventType[i].text;
        AddChoiceField(sen::onto::temporal::attr::kEventType, "Type", kinds, values::kEventTypeCount);
    }
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
