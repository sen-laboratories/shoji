/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <TextControl.h>
#include <String.h>

/**
 * A text field for a duration: it shows the clock time `HH:mm:ss` of an ISO 8601 duration (`PT2H` is `02:00:00`, see ShojiDuration.h) and gives the ISO text back, which is what the
 * attribute keeps. A duration that has no clock form (years, months, fractions of seconds) is shown as it is. What the user typed that is no clock time is given back as typed,
 * so that storing it can say that it is not valid.
 */
class ShojiDurationControl : public BTextControl {
public:
                        ShojiDurationControl(const char* name, const char* label, BMessage* message = NULL);

    /** shows the duration (ISO text); the message of the control is not sent */
    void                SetDuration(const char* iso);
    /** the ISO text of what is shown (what was typed, if that is no clock time) */
    BString             Duration() const;
};
