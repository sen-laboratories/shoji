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

#include <sen/SenTimeAttributes.h>

#include "ShojiDateControl.h"
#include "ShojiDateTimeControl.h"
#include "ShojiDuration.h"
#include "ShojiDurationControl.h"
#include "ShojiGenericFormView.h"
#include "ShojiLinkLabel.h"
#include "ShojiListControl.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ShojiGenericFormView"


/*
 * generic dynamic view to use when no MIME specific view is available for the source file.
 * Builds a form with a control for each attribute of the MIME type, according to the type of the attribute:
 * a check box for booleans, the date control for a date, a text field for everything else. The controls are named after their attribute.
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
    int32 startRow = -1, endRow = -1;
    BView* startLabel = NULL;
    BView* endLabel = NULL;

    for (const ShojiAttribute& attr : attrs) {
        if (!attr.viewable)
            continue;

        BString text;
        values->FindString(attr.name.String(), &text);

        BControl* control = CreateDataControl(attr, text.String());
        if (ShojiListControl* list = dynamic_cast<ShojiListControl*>(control)) {
            // the label on top of the left column, the list across the right one
            BStringView* label = new BStringView("label", attr.label.String());
            label->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
            layout->AddView(label, 0, row);
            layout->AddView(list, 1, row);
        } else if (ShojiDateTimeControl* dateTime = dynamic_cast<ShojiDateTimeControl*>(control)) {
            dateTime->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
            BStringView* dateLabel = new BStringView("label", attr.label.String());
            if (attr.name == "schema:startDate") {
                startRow = row;
                startLabel = dateLabel;
            } else if (attr.name == "schema:endDate") {
                endRow = row;
                endLabel = dateLabel;
            }
            layout->AddView(dateLabel, 0, row);
            layout->AddView(dateTime, 1, row);
        } else if (ShojiDateControl* date = dynamic_cast<ShojiDateControl*>(control)) {
            // the date control has no label of its own: the label on the left, the control as wide as it needs
            date->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
            layout->AddView(new BStringView("label", attr.label.String()), 0, row);
            layout->AddView(date, 1, row);
        } else if (BTextControl* textControl = dynamic_cast<BTextControl*>(control)) {
            // label and field in two columns, aligned over all rows; a field is wide enough to edit in
            textControl->TextView()->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 20, B_SIZE_UNSET));
            textControl->TextView()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
            if (shoji::LinkKindOf(attr.name.String()) != shoji::kNoLink)
                layout->AddView(new ShojiLinkLabel(attr.name.String(), attr.label.String(), false), 0, row);		// (a link while the field has a text: the type opens in FileTypes)
            else
                layout->AddItem(textControl->CreateLabelLayoutItem(), 0, row);
            layout->AddItem(textControl->CreateTextViewLayoutItem(), 1, row);
        } else {
            layout->AddView(control, 0, row, 2, 1);
        }
        row++;
    }
    // the start and the end of something: one box All day for both, in the middle between them (a form that has only one of them has the box beside it)
    ShojiDateTimeControl* start = startRow >= 0 ? dynamic_cast<ShojiDateTimeControl*>(fView->FindView("schema:startDate")) : NULL;
    ShojiDateTimeControl* end = endRow >= 0 ? dynamic_cast<ShojiDateTimeControl*>(fView->FindView("schema:endDate")) : NULL;
    if (!(start != NULL && end != NULL && ShojiCombineAllDay(layout, start, startRow, startLabel, end, endRow, endLabel))) {
        if (start != NULL)
            ShojiAddAllDay(layout, start, startRow);
        if (end != NULL)
            ShojiAddAllDay(layout, end, endRow);
    }
    if (row == 0) {
        // nothing to show, and the window should say so, not look broken
        BStringView* note = new BStringView("note", B_TRANSLATE("This file has no attributes to show."));
        note->SetHighUIColor(B_PANEL_TEXT_COLOR, B_DARKEN_1_TINT);
        layout->AddView(note, 0, row++, 2, 1);
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
    } else if (attr.type == B_TIME_TYPE && sen::timeattr::Find(attr.name.String()) != NULL
            && (text[0] == '\0' || ShojiDateTimeControl::IsDateTimeText(text))) {
        // the start and the end of something: a day, with a time of day if there is one
        ShojiDateTimeControl* dateTime = new ShojiDateTimeControl(attr.name.String());
        dateTime->SetDateTimeText(text);
        control = dateTime;
    } else if (attr.type == B_TIME_TYPE && (ShojiDateControl::IsDateText(text) || (text[0] == '\0' && ShojiDateControl::IsDayAttribute(attr.name.String())))
            && sen::timeattr::Find(attr.name.String()) == NULL) {
        // a date (midnight UTC), as in the form of a message: the date control of the Time preferences; a time of day, or no value, is typed (except a day that may be missing, a birthday: the
        // control shows no date then)
        ShojiDateControl* date = new ShojiDateControl(attr.name.String());
        date->SetUnsettable(ShojiDateControl::IsDayAttribute(attr.name.String()));
        if (text[0] == '\0')
            date->SetUnset();
        else
            date->SetDateText(text);
        control = date;
    } else if (attr.type == B_STRING_TYPE && shoji::IsDurationAttribute(attr.name.String())) {
        // an ISO 8601 duration (PT2H) is shown and typed as a clock time (02:00:00)
        ShojiDurationControl* duration = new ShojiDurationControl(attr.name.String(), attr.label.String());
        duration->SetDuration(text);
        control = duration;
    } else if (attr.type == B_BOOL_TYPE) {
        BCheckBox* checkBox = new BCheckBox(attr.name.String(), attr.label.String(), NULL);
        checkBox->SetValue(strcmp(text, "1") == 0 ? B_CONTROL_ON : B_CONTROL_OFF);
        control = checkBox;
    } else if (shoji::IsSupportedType(attr.type)) {
        control = new BTextControl(attr.name.String(), attr.label.String(), text, NULL);
    } else {
        BString note(B_TRANSLATE("(type of attribute '%name%' is not supported)"));
        note.ReplaceAll("%name%", attr.name.String());
        control = new BTextControl(attr.name.String(), attr.label.String(), note.String(), NULL);
    }
    shoji::SetControlEditable(control, attr.editable);

    return control;
}
