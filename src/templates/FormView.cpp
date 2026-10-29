/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include <ControlLook.h>
#include <Font.h>
#include <GridLayout.h>
#include <StringView.h>
#include <TextControl.h>

#include "FormView.h"

FormView::FormView(const char* name) : BGridView(be_control_look->DefaultItemSpacing(), be_control_look->DefaultLabelSpacing()), fRows(0)
{
    SetName(name);
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    SetFont(be_plain_font);
    GridLayout()->SetInsets(0, 0, 0, 0);
}

FormView::~FormView()
{
}

BTextControl* FormView::AddField(const char* attribute, const char* label)
{
    // the label is a view of its own in the first column (a text control has no say in how wide its label is when it stands in a layout), the field has none
    BStringView* labelView = new BStringView(NULL, label);
    labelView->SetAlignment(B_ALIGN_RIGHT);
    labelView->SetExplicitAlignment(BAlignment(B_ALIGN_RIGHT, B_ALIGN_VERTICAL_CENTER));
    BTextControl* field = new BTextControl(attribute, NULL, "", NULL);

    GridLayout()->AddView(labelView, 0, fRows);
    GridLayout()->AddView(field, 1, fRows);
    fRows++;
    return field;
}

void FormView::Finish()
{
    GridLayout()->SetColumnWeight(0, 0);
    GridLayout()->SetColumnWeight(1, 1);
}
