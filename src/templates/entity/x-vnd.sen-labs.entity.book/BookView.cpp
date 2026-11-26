/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "BookView.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BookView"


BookView::BookView() : FormView("Book View") {
    AddField("dc:title", B_TRANSLATE("Title"));
    AddField("dc:creator", B_TRANSLATE("Authors"));
    AddField("schema:isbn", B_TRANSLATE("ISBN"));
    AddField("schema:numberOfPages", B_TRANSLATE("Pages"));
    AddField("dc:publisher", B_TRANSLATE("Publisher"));
    AddField("dc:date", B_TRANSLATE("Date"));
    AddField("dc:language", B_TRANSLATE("Language"));
    AddField("dc:subject", B_TRANSLATE("Subjects"));
    AddField("META:TYPE", B_TRANSLATE("Semantic Type"));
    Finish();
}

BookView::~BookView()
{
}
