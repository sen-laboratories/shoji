/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include "../../FormView.h"

/**
 * The template for people (entity/x-vnd.sen-labs.entity.person): a form with the attributes of a person (FOAF, schema.org and vCard). The people of the
 * People application of Haiku (application/x-person, legacy) have the template of ContactView.
 */
class PersonView : public FormView {
    public:
        PersonView();
        virtual ~PersonView();
};
