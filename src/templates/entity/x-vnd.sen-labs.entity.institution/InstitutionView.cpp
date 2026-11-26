/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "InstitutionView.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "InstitutionView"


InstitutionView::InstitutionView() : FormView("Institution View") {
    // who it is: the name, the other names (the acronym, names in other languages: a list), the ROR id (bare, the key by which a paper finds its institution) and the kind (a list: a university that funds
    // research is Education and Funder; the values are offered)
    AddField("foaf:name", B_TRANSLATE("Full name"));
    AddField("schema:alternateName", B_TRANSLATE("Other names"));
    AddField("dc:identifier", B_TRANSLATE("ROR"));
    AddField("SEN:institutionType", B_TRANSLATE("Institution Type"));
    // how to reach it, and where it is (the labels of the homepage, the e-mail address and the city are links; the city is looked up as a place)
    AddField("foaf:homepage", B_TRANSLATE("Homepage"));
    AddField("foaf:mbox", B_TRANSLATE("E-Mail"));
    AddField("foaf:phone", B_TRANSLATE("Phone"));
    AddField("schema:addressLocality", B_TRANSLATE("City"));
    AddField("schema:addressCountry", B_TRANSLATE("Country"));
    // what it works on: a description, the topics it works on most (OpenAlex, a list) and how much it published and how often that was cited, when it was enriched
    AddField("dc:description", B_TRANSLATE("Description"));
    AddField("dc:subject", B_TRANSLATE("Topics"));
    AddField("SEN:worksCount", B_TRANSLATE("Works"));
    AddField("SEN:citationCount", B_TRANSLATE("Citations"));
    // where else it is known (ROR, Wikidata, Wikipedia): a list of IRIs
    AddField("schema:sameAs", B_TRANSLATE("Same as"));
    AddField("META:TYPE", B_TRANSLATE("Semantic Type"));
    Finish();
}

InstitutionView::~InstitutionView()
{
}
