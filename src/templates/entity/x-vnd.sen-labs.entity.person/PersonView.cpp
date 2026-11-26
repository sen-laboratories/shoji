/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PersonView.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PersonView"


PersonView::PersonView() : FormView("Person View") {
    AddField("foaf:name", B_TRANSLATE("Full name"));
    AddField("foaf:givenName", B_TRANSLATE("Given name"));
    AddField("foaf:familyName", B_TRANSLATE("Family name"));
    // the nickname is the handle that Robin searches the services for (with or without the @; a Fediverse handle is user@server, one of Bluesky a domain): called Nickname as in People and the ontology
    AddField("foaf:nick", B_TRANSLATE("Nickname"));
    AddField("schema:jobTitle", B_TRANSLATE("Job title"));
    AddField("schema:worksFor", B_TRANSLATE("Company"));
    AddField("schema:homeLocation", B_TRANSLATE("Location"));
    AddField("foaf:mbox", B_TRANSLATE("E-Mail"));
    AddField("foaf:phone", B_TRANSLATE("Phone"));
    AddField("foaf:homepage", B_TRANSLATE("Homepage"));
    AddField("schema:birthDate", B_TRANSLATE("Birthday"));
    // Groups (Family, Friends...): a list attribute, so the text field is replaced by the control for lists when the file is shown
    AddField("vcard:category", B_TRANSLATE("Groups"));
    AddField("dc:description", B_TRANSLATE("Description"));
    AddField("vivo:orcidId", B_TRANSLATE("ORCID"));
    // where else the person is known (ORCID, Wikidata...), a list of IRIs; the photo's credit and license as the source asks to say them
    AddField("schema:sameAs", B_TRANSLATE("Same as"));
    AddField("schema:creditText", B_TRANSLATE("Photo credit"));
    AddField("schema:license", B_TRANSLATE("Photo license"));
    AddField("META:TYPE", B_TRANSLATE("Semantic Type"));
    Finish();
}

PersonView::~PersonView()
{
}
