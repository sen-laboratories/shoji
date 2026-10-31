/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Application.h>
#include <OS.h>

// asks the application for its settings window (from the menu of a window)
static const uint32 kMsgShowSettings = 'SAse';

class ShojiApp : public BApplication
{
    public:
                            ShojiApp();
        virtual			    ~ShojiApp();
        virtual void        RefsReceived(BMessage* message);
        virtual void        ArgvReceived(int32 argc, char **argv);
        virtual void        AboutRequested();
        virtual void        MessageReceived(BMessage* message);
        virtual void        ReadyToRun();
        virtual void        Pulse();
    private:
        status_t            GenerateTemplates();
        void                ShowSettings();

        bool                fOpenedFile;    // a file was given: no settings window of its own
        bool                fWindowSeen;    // a window was open: the application ends when the last one is closed
        bigtime_t           fStarted;
};
