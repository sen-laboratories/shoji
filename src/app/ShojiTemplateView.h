/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Path.h>
#include <String.h>
#include <View.h>
#include "ShojiView.h"

#define SHOJI_TEMPLATE_VIEW_TYPE "application/x.vnd-sen-labs.Shoji.templateView"
#define SHOJI_TEMPLATE_PATH "shoji/templates"
#define SHOJI_TEMPLATE_NAME "template.view"

class ShojiTemplateView : public ShojiView
{
    public:
                    ShojiTemplateView(const char* mimeType);
        virtual	   ~ShojiTemplateView();
        const char* GetType();
        status_t    Initialize();
        status_t    Populate(const ShojiAttributeList& attrs, const BMessage* values);

    protected:
        // the template of a MIME type: the first of the folders (see FindTemplateFile()) that has one, unarchived
        status_t    LookupView(const char* mimeType);
        // <folder>/shoji/templates/<MIME type>/template.view in the user's settings, the user's data, the system's data (what packages bring), ...
        static status_t FindTemplateFile(const char* mimeType, BPath* file);
        BString     fMimeType;
};
