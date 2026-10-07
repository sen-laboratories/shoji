/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Application.h>

class ShojiApp : public BApplication
{
    public:
                            ShojiApp();
        virtual			    ~ShojiApp();
        virtual void        RefsReceived(BMessage* message);
        virtual void        ArgvReceived(int32 argc, char **argv);
    private:
        status_t            GenerateTemplates();
};
