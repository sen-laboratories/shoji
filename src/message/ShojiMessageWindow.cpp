/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiMessageWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <ControlLook.h>
#include <DataIO.h>
#include <File.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <String.h>
#include <Volume.h>

#include <unistd.h>

#include <sen/SenSettingsForm.h>

#include "../app/ShojiApp.h"
#include "ShojiMessageView.h"

namespace form = sen::settings::form;

static const uint32 kMsgRevert = 'SMrv';
static const uint32 kMsgDefaults = 'SMdf';
static const off_t kMaxMessageFile = 8 * 1024 * 1024;

ShojiMessageWindow::ShojiMessageWindow(const entry_ref& ref, const BMessage& data)
    :
    BWindow(BRect(BPoint(320.0, 128.0), BSize(560.0, 400.0)), ref.name, B_DOCUMENT_WINDOW,
        B_WILL_ACCEPT_FIRST_CLICK | B_AUTO_UPDATE_SIZE_LIMITS),
    fRef(ref),
    fWritable(false),
    fView(NULL),
    fMenuBar(NULL),
    fRevertButton(NULL),
    fDefaultsButton(NULL)
{
    // (a file that cannot be written is shown, not edited)
    BVolume volume(fRef.device);
    BPath path(&fRef);
    fWritable = volume.InitCheck() == B_OK && !volume.IsReadOnly() && path.InitCheck() == B_OK && access(path.Path(), W_OK) == 0;

    fMenuBar = new BMenuBar("menubar");
    BMenu* menu = new BMenu("File");
    BMenuItem* about = new BMenuItem("About Shoji", new BMessage(B_ABOUT_REQUESTED));
    about->SetTarget(be_app);
    menu->AddItem(about);
    BMenuItem* settings = new BMenuItem("Settings" B_UTF8_ELLIPSIS, new BMessage(kMsgShowSettings), ',');
    settings->SetTarget(be_app);
    menu->AddItem(settings);
    menu->AddSeparatorItem();
    BMenuItem* quit = new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q');
    quit->SetTarget(be_app);
    menu->AddItem(quit);
    fMenuBar->AddItem(menu);

    fView = new ShojiMessageView();
    fView->SetRootLabel(ref.name);
    fView->SetTarget(BMessenger(this));
    fView->SetEditable(fWritable);
    fView->SetData(data);

    fRevertButton = new BButton("revert", "Revert", new BMessage(kMsgRevert));
    fDefaultsButton = new BButton("defaults", "Defaults", new BMessage(kMsgDefaults));

    const float spacing = be_control_look->DefaultItemSpacing();
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(fMenuBar)
        .Add(fView)
        .AddGroup(B_HORIZONTAL, spacing)
            .SetInsets(spacing)
            .AddGlue()
            .Add(fDefaultsButton)
            .Add(fRevertButton)
        .End();

    if (!fView->HasDefaults())
        fDefaultsButton->Hide();
    _UpdateButtons();
    _UpdateTitle();
}


ShojiMessageWindow::~ShojiMessageWindow()
{
}


bool ShojiMessageWindow::LoadMessageFile(const entry_ref& ref, BMessage* data)
{
    BFile file(&ref, B_READ_ONLY);
    off_t size = 0;
    if (file.InitCheck() != B_OK || file.GetSize(&size) != B_OK || size < 16 || size > kMaxMessageFile)
        return false;

    // the first four bytes of a flattened message: '1FMH' (Haiku), in the order of the machine that wrote it
    uint32 magic = 0;
    if (file.ReadAt(0, &magic, sizeof(magic)) != (ssize_t)sizeof(magic) || (magic != '1FMH' && magic != 'HMF1'))
        return false;

    file.Seek(0, SEEK_SET);
    return data->Unflatten(&file) == B_OK;
}


void ShojiMessageWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case form::kChanged:
            _Save();
            _UpdateButtons();
            _UpdateTitle();
            break;
        case form::kRefused:
        {
            // not blocking: the field has gone back to its value
            BString text("The value is not valid for '");
            text << message->GetString(form::kFieldPath, "") << "': " << message->GetString(form::kFieldDetail, "") << "\nThe previous value has been restored.";
            BAlert* alert = new BAlert("Value refused", text.String(), "Oh no.");
            alert->SetFlags(alert->Flags() | B_WARNING_ALERT | B_CLOSE_ON_ESCAPE);
            alert->Go(NULL);
            break;
        }
        case kMsgRevert:
            fView->Revert();
            break;
        case kMsgDefaults:
            fView->Defaults();
            break;
        default:
            BWindow::MessageReceived(message);
            break;
    }
}


// A field that has the focus when the window is left or closed keeps what was typed.
bool ShojiMessageWindow::QuitRequested()
{
    fView->CommitPending();
    return true;
}


void ShojiMessageWindow::WindowActivated(bool active)
{
    BWindow::WindowActivated(active);
    if (!active)
        fView->CommitPending();
}


// The file is written in place (a replacement would lose the attributes, the type among them). The message is flattened first, so that a failure there does not
// leave a file that was cut off.
status_t ShojiMessageWindow::_Save()
{
    if (!fWritable)
        return B_NOT_ALLOWED;

    BMallocIO buffer;
    status_t result = fView->Settings().Flatten(&buffer);
    if (result != B_OK)
        return result;

    BFile file(&fRef, B_WRITE_ONLY | B_ERASE_FILE);
    result = file.InitCheck();
    if (result != B_OK)
        return result;
    const ssize_t written = file.Write(buffer.Buffer(), buffer.BufferLength());
    if (written != (ssize_t)buffer.BufferLength()) {
        BAlert* alert = new BAlert("Error storing", "The file could not be written.", "Oh no.");
        alert->SetFlags(alert->Flags() | B_WARNING_ALERT | B_CLOSE_ON_ESCAPE);
        alert->Go(NULL);
        return written < 0 ? (status_t)written : B_IO_ERROR;
    }
    return B_OK;
}


void ShojiMessageWindow::_UpdateButtons()
{
    fRevertButton->SetEnabled(fWritable && fView->IsModified());
    fDefaultsButton->SetEnabled(fWritable);
}


void ShojiMessageWindow::_UpdateTitle()
{
    BString title(fRef.name);
    title << " - Shoji";
    if (!fWritable)
        title << " (read-only)";
    SetTitle(title.String());
}
