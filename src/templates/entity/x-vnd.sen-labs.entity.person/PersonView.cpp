/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PersonView.h"

PersonView::PersonView() : FormView("Person View") {
    AddField("foaf:name", "Full name");
    AddField("foaf:givenName", "Given name");
    AddField("foaf:familyName", "Family name");
    // the handle that Robin searches the services for (with or without the @; a Fediverse handle is user@server, one of Bluesky a domain)
    AddField("foaf:nick", "Handle");
    AddField("schema:jobTitle", "Job title");
    AddField("schema:worksFor", "Company");
    AddField("schema:homeLocation", "Location");
    AddField("foaf:mbox", "E-Mail");
    AddField("foaf:phone", "Phone");
    AddField("foaf:homepage", "Homepage");
    AddField("schema:birthDate", "Birthday");
    // Groups (Family, Friends...): a list attribute, so the text field is replaced by the control for lists when the file is shown
    AddField("vcard:category", "Groups");
    AddField("dc:description", "Description");
    AddField("vivo:orcidId", "ORCID");
    // where else the person is known (ORCID, Wikidata...), a list of IRIs; the photo's credit and license as the source asks to say them
    AddField("schema:sameAs", "Same as");
    AddField("schema:creditText", "Photo credit");
    AddField("schema:license", "Photo license");
    Finish();
}

PersonView::~PersonView()
{
}
