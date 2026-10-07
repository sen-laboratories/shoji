/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <View.h>

class ShojiView
{
    public:
        virtual BView*      GetView() { return fView; }
        virtual const char* GetType() { return fType; }
        virtual bool        IsValid() { return fView != NULL; }
        virtual status_t    Initialize() { return B_OK; }
        virtual status_t    Populate(const BMessage *mimeAttrInfo, const BMessage* attrs) { return B_ERROR; }

    protected:
        BView*      fView;
        const char* fType;
};
