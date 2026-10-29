/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <GridView.h>

#include "ShojiView.h"

class ShojiGenericFormView : public ShojiView
{
    public:
                    ShojiGenericFormView();
        virtual	   ~ShojiGenericFormView();
        status_t    Populate(const ShojiAttributeList& attrs, const BMessage* values);

    protected:
        BControl*   CreateDataControl(const ShojiAttribute& attr, const char* text);
};
