/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include "../../FormView.h"

/**
 * The template for scientific papers (document/scientific-paper, the semantic type that Paperboy gives to a PDF or EPUB): a form with the attributes of a paper
 * (Dublin Core and schema.org): the title, the authors, where and when it was published, the DOI, the keywords and the abstract, and the places to read it.
 * The open access status and the number of citations are what a service said (read-only, as the type declares them).
 */
class PaperView : public FormView {
    public:
        PaperView();
        virtual ~PaperView();
};
