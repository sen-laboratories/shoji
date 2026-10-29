/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include "../../FormView.h"

/**
 * The template for books (entity/x-vnd.sen-labs.entity.book): a form with the attributes of a book (Dublin Core and schema.org). The cover of the
 * book is the thumbnail on the stripe of the window.
 */
class BookView : public FormView {
    public:
        BookView();
        virtual ~BookView();
};
