/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include <Button.h>
#include <math.h>
#include <ControlLook.h>
#include <Font.h>
#include <GridLayout.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <StringView.h>
#include <TextControl.h>

#include "FormView.h"
#include "../app/ShojiAttributes.h"

FormView::FormView(const char* name) : BGridView(be_control_look->DefaultItemSpacing(), be_control_look->DefaultLabelSpacing()), fRows(0)
{
    SetName(name);
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    SetFont(be_plain_font);
    GridLayout()->SetInsets(0, 0, 0, 0);
}

FormView::FormView(BMessage* archive) : BGridView(archive), fRows(0)
{
}

BArchivable* FormView::Instantiate(BMessage* archive)
{
    if (!validate_instantiation(archive, "FormView"))
        return NULL;
    return new FormView(archive);
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

BTextControl* FormView::AddChoiceField(const char* attribute, const char* label, const char* const* choices, unsigned count)
{
    BStringView* labelView = new BStringView(NULL, label);
    labelView->SetAlignment(B_ALIGN_RIGHT);
    labelView->SetExplicitAlignment(BAlignment(B_ALIGN_RIGHT, B_ALIGN_VERTICAL_CENTER));
    BTextControl* field = new BTextControl(attribute, NULL, "", NULL);

    // the button asks the window (the target is not set) to show the menu: the values are in its message
    BMessage* choose = new BMessage(SHOJI_MSG_CHOOSE);
    choose->AddString(SHOJI_FIELD_ATTR, attribute);
    for (unsigned i = 0; i < count; i++)
        choose->AddString("choice", choices[i]);
    BString buttonName(attribute);
    buttonName << SHOJI_CHOOSE_BUTTON_SUFFIX;
    BButton* button = new BButton(buttonName.String(), "\xE2\x96\xBE", choose);	// (a small triangle pointing down)
    button->SetToolTip("Choose from the common values (any other text can be typed)");
    button->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
    // (as wide as it is high, about: a button does not take a third of the line)
    const float width = ceilf(be_plain_font->Size() * 2.4f);
    button->SetExplicitMinSize(BSize(width, B_SIZE_UNSET));
    button->SetExplicitMaxSize(BSize(width, B_SIZE_UNSET));
    button->SetExplicitPreferredSize(BSize(width, B_SIZE_UNSET));

    BGroupView* row = new BGroupView(B_HORIZONTAL, be_control_look->DefaultLabelSpacing());
    row->GroupLayout()->SetInsets(0, 0, 0, 0);
    row->GroupLayout()->AddView(field, 1.0f);
    row->GroupLayout()->AddView(button, 0.0f);
    GridLayout()->AddView(labelView, 0, fRows);
    GridLayout()->AddView(row, 1, fRows);
    fRows++;
    return field;
}

void FormView::Finish()
{
    GridLayout()->SetColumnWeight(0, 0);
    GridLayout()->SetColumnWeight(1, 1);
}
