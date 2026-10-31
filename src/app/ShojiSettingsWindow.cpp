/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiSettingsWindow.h"

#include <Application.h>
#include <Button.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <RadioButton.h>
#include <StringView.h>
#include <Cursor.h>

#include <stdio.h>

#include "ShojiAssociations.h"

extern const char* kApplicationSignature;

static const uint32 kMsgOk = 'SSok';
static const uint32 kMsgDefaults = 'SSdf';
static const uint32 kMsgChoice = 'SSch';

ShojiSettingsWindow::ShojiSettingsWindow()
    :
    BWindow(BRect(200, 200, 560, 400), "Shoji Settings", B_TITLED_WINDOW,
        B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE)
{
    fSettings.Load();

    BStringView* heading = new BStringView("heading", "Associate Shoji with:");
    heading->SetFont(be_bold_font);

    fNothing = new BRadioButton("nothing", "No file types", new BMessage(kMsgChoice));
    fUnassociated = new BRadioButton("unassociated", "All unassociated file types", new BMessage(kMsgChoice));
    fTemplated = new BRadioButton("templated", "All file types with a template, too", new BMessage(kMsgChoice));

    BStringView* nothingNote = new BStringView("nothingnote", "Shoji opens what it is asked to open (the SEN menus, Open With).");
    BStringView* unassociatedNote = new BStringView("unassociatednote", "A type that has no template gets a form made from its attributes.");
    BStringView* templatedNote = new BStringView("templatednote", "Replaces the application of those types, e.g. People for the contacts.");
    for (BStringView* note : {nothingNote, unassociatedNote, templatedNote}) {
        note->SetFont(be_plain_font);
        note->SetHighUIColor(B_PANEL_TEXT_COLOR, B_LIGHTEN_1_TINT);
    }

    fStatus = new BStringView("status", "");
    fStatus->SetHighUIColor(B_PANEL_TEXT_COLOR, B_LIGHTEN_1_TINT);

    fDefaults = new BButton("defaults", "Revert to defaults", new BMessage(kMsgDefaults));
    BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOk));
    ok->MakeDefault(true);

    const float indent = be_control_look->DefaultItemSpacing() * 2;
    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_WINDOW_INSETS)
        .Add(heading)
        .AddGroup(B_VERTICAL, 0)
            .SetInsets(indent, 0, 0, 0)
            .Add(fNothing)
            .AddGroup(B_HORIZONTAL, 0).SetInsets(indent, 0, 0, B_USE_HALF_ITEM_SPACING).Add(nothingNote).End()
            .Add(fUnassociated)
            .AddGroup(B_HORIZONTAL, 0).SetInsets(indent, 0, 0, B_USE_HALF_ITEM_SPACING).Add(unassociatedNote).End()
            .Add(fTemplated)
            .AddGroup(B_HORIZONTAL, 0).SetInsets(indent, 0, 0, 0).Add(templatedNote).End()
        .End()
        .Add(fStatus)
        .AddGlue()
        .AddGroup(B_HORIZONTAL)
            .AddGlue()
            .Add(fDefaults)
            .Add(ok)
        .End()
    .End();

    ShowMode(fSettings.Mode());
    UpdateStatus();
    CenterOnScreen();
}

shoji::AssociationMode ShojiSettingsWindow::ChosenMode() const
{
    if (fTemplated->Value() == B_CONTROL_ON)
        return shoji::kAssociateTemplated;
    if (fUnassociated->Value() == B_CONTROL_ON)
        return shoji::kAssociateUnassociated;
    return shoji::kAssociateNothing;
}

void ShojiSettingsWindow::ShowMode(shoji::AssociationMode mode)
{
    fNothing->SetValue(mode == shoji::kAssociateNothing ? B_CONTROL_ON : B_CONTROL_OFF);
    fUnassociated->SetValue(mode == shoji::kAssociateUnassociated ? B_CONTROL_ON : B_CONTROL_OFF);
    fTemplated->SetValue(mode == shoji::kAssociateTemplated ? B_CONTROL_ON : B_CONTROL_OFF);
    fDefaults->SetEnabled(mode != shoji::Settings::Default());
}

void ShojiSettingsWindow::UpdateStatus()
{
    BString text;
    const int32 count = fSettings.CountAssociated();
    if (count == 0)
        text = "Shoji is not the default application of any file type.";
    else
        text << "Shoji is the default application of " << count << (count == 1 ? " file type." : " file types.");
    fStatus->SetText(text.String());
}

// Puts the choice into effect: what was associated before is put back, then the types for the mode are taken. It can take a moment (a call to the
// file type database for each type), so the pointer says so.
void ShojiSettingsWindow::Apply()
{
    BCursor busy(B_CURSOR_ID_PROGRESS);
    be_app->SetCursor(&busy);

    const shoji::AssociationMode mode = ChosenMode();
    fSettings.SetMode(mode);
    shoji::Apply(mode, kApplicationSignature, &fSettings.Previous());
    fSettings.Save();

    be_app->SetCursor(B_CURSOR_SYSTEM_DEFAULT);
    UpdateStatus();
}

void ShojiSettingsWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kMsgChoice:
            fDefaults->SetEnabled(ChosenMode() != shoji::Settings::Default());
            break;
        case kMsgDefaults:
            ShowMode(shoji::Settings::Default());
            break;
        case kMsgOk:
            Apply();
            PostMessage(B_QUIT_REQUESTED);
            break;
        default:
            BWindow::MessageReceived(message);
    }
}
