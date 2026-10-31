/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Window.h>

#include "ShojiSettings.h"

class BButton;
class BRadioButton;
class BStringView;

/**
 * The settings of Shoji: "Associate Shoji with:" no file types, all that have no application yet, or those and all that Shoji has a template for. OK stores the
 * choice and makes the associations (the file type database of the user); "Revert to defaults" goes back to no file types, which OK then puts into effect (and puts
 * the applications of the types back as they were).
 */
class ShojiSettingsWindow : public BWindow {
public:
                            ShojiSettingsWindow();

    virtual void            MessageReceived(BMessage* message);

private:
    shoji::AssociationMode  ChosenMode() const;
    void                    ShowMode(shoji::AssociationMode mode);
    void                    UpdateStatus();
    void                    Apply();

    shoji::Settings         fSettings;
    BRadioButton*           fNothing;
    BRadioButton*           fUnassociated;
    BRadioButton*           fTemplated;
    BStringView*            fStatus;
    BButton*                fDefaults;
};
