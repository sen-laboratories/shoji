/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <GroupView.h>

#include "ShojiView.h"

class ShojiGenericFormView : public ShojiView
{
    public:
                    ShojiGenericFormView();
        virtual	   ~ShojiGenericFormView();
        status_t    Populate(const BMessage *mimeAttrInfo, const BMessage* attrs);

    protected:
        BView*      CreateDataView(const char* name, type_code typeCode, bool editable, const void* data);
};
