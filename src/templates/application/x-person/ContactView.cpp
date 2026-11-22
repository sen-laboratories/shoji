/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
/**
 * simple demo view for People files: the attributes of the People application of Haiku, as a form
 */
#include "ContactView.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ContactView"


ContactView::ContactView() : FormView("Contact View") {
    // todo: add picture to the left (the thumbnail or the icon of the file is on the stripe of the window)
    AddField("META:name", B_TRANSLATE("Name"));
    AddField("META:company", B_TRANSLATE("Company"));
    AddField("META:email", B_TRANSLATE("E-Mail"));
    AddField("META:mphone", B_TRANSLATE("Mobile"));
    AddField("META:address", B_TRANSLATE("Address"));
    AddField("META:zip", B_TRANSLATE("ZIP code"));
    AddField("META:city", B_TRANSLATE("City"));
    AddField("META:country", B_TRANSLATE("Country"));
    // Groups (Family, Friends...): a list attribute, so the text field is replaced by the control for lists when the file is shown
    AddField("META:group", B_TRANSLATE("Groups"));
    Finish();
}

ContactView::~ContactView()
{
}
