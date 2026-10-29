/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <View.h>

#include "ShojiAttributes.h"

class ShojiView
{
    public:
        virtual BView*      GetView() { return fView; }
        virtual const char* GetType() { return fType; }
        virtual bool        IsValid() { return fView != NULL; }
        virtual status_t    Initialize() { return B_OK; }
        // shows the attributes of the MIME type; values holds the text of those that the file has (field per name)
        virtual status_t    Populate(const ShojiAttributeList& attrs, const BMessage* values) { return B_ERROR; }

    protected:
        BView*      fView;
        const char* fType;
};
