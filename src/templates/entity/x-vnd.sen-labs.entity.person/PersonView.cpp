/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PersonView.h"

PersonView::PersonView() : FormView("Person View") {
    AddField("foaf:name", "Name");
    AddField("foaf:givenName", "Given name");
    AddField("foaf:familyName", "Family name");
    AddField("foaf:nick", "Nickname");
    AddField("schema:jobTitle", "Job title");
    AddField("foaf:mbox", "E-Mail");
    AddField("foaf:phone", "Phone");
    AddField("foaf:homepage", "Homepage");
    AddField("schema:birthDate", "Birthday");
    // Groups (Family, Friends...): a list attribute, so the text field is replaced by the control for lists when the file is shown
    AddField("vcard:category", "Groups");
    AddField("dc:description", "Description");
    Finish();
}

PersonView::~PersonView()
{
}
