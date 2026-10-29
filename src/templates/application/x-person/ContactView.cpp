/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
/**
 * simple demo view for People files: the attributes of the People application of Haiku, as a form
 */
#include "ContactView.h"

ContactView::ContactView() : FormView("Contact View") {
    // todo: add picture to the left (the thumbnail or the icon of the file is on the stripe of the window)
    AddField("META:name", "Name");
    AddField("META:company", "Company");
    AddField("META:email", "E-Mail");
    AddField("META:mphone", "Mobile");
    AddField("META:address", "Address");
    AddField("META:zip", "ZIP code");
    AddField("META:city", "City");
    AddField("META:country", "Country");
    // Groups (Family, Friends...): a list attribute, so the text field is replaced by the control for lists when the file is shown
    AddField("META:group", "Groups");
    Finish();
}

ContactView::~ContactView()
{
}
