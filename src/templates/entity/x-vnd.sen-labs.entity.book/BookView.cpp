/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include <ControlLook.h>
#include <Font.h>
#include <GroupLayout.h>
#include <SpaceLayoutItem.h>
#include <TextControl.h>

#include "BookView.h"

namespace {

// the width of the labels: the same in every row, so that the fields line up
float
LabelWidth()
{
    return be_plain_font->StringWidth("Publisher") + be_control_look->DefaultLabelSpacing() * 2;
}

// a field with its label; a short one (pages, a date) has the room that its value needs and leaves the rest to the one next to it
BTextControl*
Field(const char* attribute, const char* label, float valueWidth = 0)
{
    BTextControl* field = new BTextControl(attribute, label, "", NULL);
    if (valueWidth <= 0) {
        field->SetDivider(LabelWidth());
    } else {
        float divider = be_plain_font->StringWidth(label) + be_control_look->DefaultLabelSpacing() * 2;
        field->SetDivider(divider);
        field->SetExplicitMaxSize(BSize(divider + valueWidth, B_SIZE_UNSET));
    }
    return field;
}

BGroupView*
Row(BTextControl* first, BTextControl* second = NULL)
{
    BGroupView* row = new BGroupView(B_HORIZONTAL, be_control_look->DefaultItemSpacing());
    row->GroupLayout()->AddView(first);
    if (second != NULL)
        row->GroupLayout()->AddView(second);
    return row;
}

}   // namespace


BookView::BookView() : BGroupView(B_VERTICAL, be_control_look->DefaultLabelSpacing()) {
    SetName("Book View");
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    SetFont(be_plain_font);

    GroupLayout()->AddView(Row(Field("dc:title", "Title")));
    GroupLayout()->AddView(Row(Field("dc:creator", "Authors")));
    GroupLayout()->AddView(Row(Field("schema:isbn", "ISBN"), Field("schema:numberOfPages", "Pages", be_plain_font->StringWidth("00000") + 16)));
    GroupLayout()->AddView(Row(Field("dc:publisher", "Publisher"), Field("dc:date", "Date", be_plain_font->StringWidth("0000-00-00 00:00:00") + 16)));
    GroupLayout()->AddView(Row(Field("dc:language", "Language")));
    GroupLayout()->AddView(Row(Field("dc:subject", "Subjects")));
    GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
}

BookView::~BookView()
{
}
