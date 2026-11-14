/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include "../../FormView.h"

/**
 * The template for events (entity/x-vnd.sen-labs.entity.event): the title, the time of the event (start and end, or start and duration) and a description. A start and an end take
 * a time (`2026-11-14 10:00`), a day (`2026-11-14`) or a time that is not exact in the Extended Date/Time Format (`1994-05~`, `1994?`, `202X`); see sen/SenEdtf.h. The duration is
 * the clock time (`02:00:00`) and is kept as ISO 8601 (`PT2H`).
 */
class EventView : public FormView {
    public:
        EventView();
        virtual ~EventView();
};
