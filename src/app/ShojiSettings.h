/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Message.h>
#include <SupportDefs.h>

#include "ShojiAssociations.h"

namespace shoji {

/**
 * The settings of Shoji, a message in `~/config/settings/shoji/settings`: which file types Shoji is the default application of (an AssociationMode), and what the
 * applications of the types were before, to put them back.
 */
class Settings {
public:
    Settings();

    status_t            Load();
    status_t            Save() const;

    AssociationMode     Mode() const { return fMode; }
    void                SetMode(AssociationMode mode) { fMode = mode; }
    BMessage&           Previous() { return fPrevious; }
    const BMessage&     Previous() const { return fPrevious; }
    /** how many file types are Shoji's by the settings */
    int32               CountAssociated() const;

    /** the mode that nothing is changed for */
    static AssociationMode  Default() { return kAssociateNothing; }

private:
    AssociationMode     fMode;
    BMessage            fPrevious;
};

}   // namespace shoji
