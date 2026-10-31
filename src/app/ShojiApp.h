/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Application.h>
#include <OS.h>

class ShojiApp : public BApplication
{
    public:
                            ShojiApp();
        virtual			    ~ShojiApp();
        virtual void        RefsReceived(BMessage* message);
        virtual void        ArgvReceived(int32 argc, char **argv);
        virtual void        AboutRequested();
        virtual void        ReadyToRun();
        virtual void        Pulse();
    private:
        status_t            GenerateTemplates();

        bool                fWindowSeen;    // a window was open: the application ends when the last one is closed
        bigtime_t           fStarted;
};
