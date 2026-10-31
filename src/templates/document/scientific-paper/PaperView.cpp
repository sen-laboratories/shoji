/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PaperView.h"

PaperView::PaperView() : FormView("Paper View") {
    AddField("dc:title", "Title");
    AddField("dc:creator", "Authors");
    AddField("dcterms:isPartOf", "Published in");
    AddField("dc:date", "Date");
    AddField("dc:identifier", "DOI");
    AddField("dc:subject", "Keywords");
    AddField("dc:description", "Abstract");
    AddField("dc:type", "Kind of work");
    AddField("schema:url", "Page");
    AddField("schema:contentUrl", "PDF");
    AddField("SEN:oaStatus", "Open access");
    AddField("SEN:citationCount", "Citations");
    Finish();
}

PaperView::~PaperView()
{
}
