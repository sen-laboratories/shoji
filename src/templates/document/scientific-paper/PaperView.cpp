/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PaperView.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PaperView"


PaperView::PaperView() : FormView("Paper View") {
    AddField("dc:title", B_TRANSLATE("Title"));
    AddField("dc:creator", B_TRANSLATE("Authors"));
    AddField("dcterms:isPartOf", B_TRANSLATE("Published in"));
    AddField("dc:date", B_TRANSLATE("Date"));
    AddField("dc:identifier", B_TRANSLATE("DOI"));
    AddField("dc:subject", B_TRANSLATE("Keywords"));
    AddField("dc:description", B_TRANSLATE("Abstract"));
    AddField("dc:type", B_TRANSLATE("Kind of work"));
    AddField("schema:url", B_TRANSLATE("Page"));
    AddField("schema:contentUrl", B_TRANSLATE("PDF"));
    AddField("SEN:oaStatus", B_TRANSLATE("Open access"));
    AddField("SEN:citationCount", B_TRANSLATE("Citations"));
    Finish();
}

PaperView::~PaperView()
{
}
