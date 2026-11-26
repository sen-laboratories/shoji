/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include "../../FormView.h"

/**
 * The template for institutions (entity/x-vnd.sen-labs.entity.institution): a university, institute or any other organization of research and education, as the Research
 * Organization Registry (ROR) and OpenAlex describe it. The labels of the homepage, the e-mail address and the city are links, as in the form of a person, and so is the
 * semantic type.
 */
class InstitutionView : public FormView {
    public:
        InstitutionView();
        virtual ~InstitutionView();
};
