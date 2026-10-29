/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "BookView.h"

BookView::BookView() : FormView("Book View") {
    AddField("dc:title", "Title");
    AddField("dc:creator", "Authors");
    AddField("schema:isbn", "ISBN");
    AddField("schema:numberOfPages", "Pages");
    AddField("dc:publisher", "Publisher");
    AddField("dc:date", "Date");
    AddField("dc:language", "Language");
    AddField("dc:subject", "Subjects");
    Finish();
}

BookView::~BookView()
{
}
