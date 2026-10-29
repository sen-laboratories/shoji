/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <GridView.h>

class BTextControl;

/**
 * The base of the templates that are a form: one field below the other, each with its label, in a grid of two columns (the labels, the fields): the labels are as
 * wide as the widest so that the fields start at the same place and line up. The fields are named after the attributes that they show (Shoji finds them by that name). Add the fields, then call Finish().
 */
class FormView : public BGridView {
    public:
                        FormView(const char* name);
        virtual        ~FormView();

        BTextControl*   AddField(const char* attribute, const char* label);
        /** nothing to do now that the grid sets the widths, kept so that a template says where it ends */
        void            Finish();

    private:
        int32           fRows;
};
