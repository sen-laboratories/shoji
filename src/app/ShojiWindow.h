/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Mime.h>
#include <Window.h>
#include "ShojiView.h"

class ShojiWindow : public BWindow
{
    public:
                            ShojiWindow(entry_ref *ref);
        virtual			    ~ShojiWindow();

    protected:
        status_t            MapAttributesToMessage(const entry_ref *ref, const BMessage *mimeAttrInfo, BMessage* outAttrMsg);
        ShojiView*          GetViewTemplateForType(const char* mimeType);
        void                ShowUserError(const char* title, const char* message, status_t errorCode);
        status_t            GetMimeTypeForRef(const entry_ref* ref, char* mimeType);
};
