/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <GroupView.h>

/**
 * The template for books (entity/x-vnd.sen-labs.entity.book): the title and the authors across, the rest in pairs. The
 * controls are named after the attributes of the book (Dublin Core and schema.org), the cover of the book is the
 * thumbnail on the stripe of the window.
 */
class BookView : public BGroupView {
    public:
        BookView();
        virtual ~BookView();
};
