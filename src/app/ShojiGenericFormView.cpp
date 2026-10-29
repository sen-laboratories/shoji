/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <CheckBox.h>
#include <ControlLook.h>
#include <Entry.h>
#include <Errors.h>
#include <GridLayout.h>
#include <StringView.h>
#include <LayoutItem.h>
#include <SpaceLayoutItem.h>
#include <TextControl.h>

#include <cstring>

#include "ShojiGenericFormView.h"
#include "ShojiListControl.h"

/*
 * generic dynamic view to use when no MIME specific view is available for the source file.
 * Builds a form with a control for each attribute of the MIME type, according to the type of the attribute:
 * a check box for booleans, a text field for everything else. The controls are named after their attribute.
 */
ShojiGenericFormView::ShojiGenericFormView() : ShojiView()
{
    BGridView* form = new BGridView();
    form->SetName("Shoji Dynamic View");
    form->SetFlags(form->Flags() | B_NAVIGABLE);
    form->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    fView = form;
    fType = NULL;
}

ShojiGenericFormView::~ShojiGenericFormView()
{
}

status_t ShojiGenericFormView::Populate(const ShojiAttributeList& attrs, const BMessage* values)
{
    BGridLayout* layout = reinterpret_cast<BGridView*>(fView)->GridLayout();
    int32 row = 0;

    for (const ShojiAttribute& attr : attrs) {
        BString text;
        values->FindString(attr.name.String(), &text);

        BControl* control = CreateDataControl(attr, text.String());
        if (ShojiListControl* list = dynamic_cast<ShojiListControl*>(control)) {
            // the label on top of the left column, the list across the right one
            BStringView* label = new BStringView("label", attr.label.String());
            label->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
            layout->AddView(label, 0, row);
            layout->AddView(list, 1, row);
        } else if (BTextControl* textControl = dynamic_cast<BTextControl*>(control)) {
            // label and field in two columns, aligned over all rows; a field is wide enough to edit in
            textControl->TextView()->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 20, B_SIZE_UNSET));
            textControl->TextView()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
            layout->AddItem(textControl->CreateLabelLayoutItem(), 0, row);
            layout->AddItem(textControl->CreateTextViewLayoutItem(), 1, row);
        } else {
            layout->AddView(control, 0, row, 2, 1);
        }
        row++;
    }
    layout->SetColumnWeight(1, 100.0f);
    // keep the rows together at the top
    layout->AddItem(BSpaceLayoutItem::CreateGlue(), 0, row, 2, 1);
    layout->SetRowWeight(row, 100.0f);

    return B_OK;
}

BControl* ShojiGenericFormView::CreateDataControl(const ShojiAttribute& attr, const char* text)
{
    BControl* control = NULL;

    if (attr.list.isList && attr.type == B_STRING_TYPE) {
        ShojiListControl* list = new ShojiListControl(attr.name.String(), attr.list.separator);
        list->SetListText(text);
        control = list;
    } else if (attr.type == B_BOOL_TYPE) {
        BCheckBox* checkBox = new BCheckBox(attr.name.String(), attr.label.String(), NULL);
        checkBox->SetValue(strcmp(text, "1") == 0 ? B_CONTROL_ON : B_CONTROL_OFF);
        control = checkBox;
    } else if (shoji::IsSupportedType(attr.type)) {
        control = new BTextControl(attr.name.String(), attr.label.String(), text, NULL);
    } else {
        BString note("(type of attribute '");
        note << attr.name << "' is not supported)";
        control = new BTextControl(attr.name.String(), attr.label.String(), note.String(), NULL);
    }
    control->SetEnabled(attr.editable);

    return control;
}
