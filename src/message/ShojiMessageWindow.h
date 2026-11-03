/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Entry.h>
#include <Message.h>
#include <Window.h>

class BButton;
class BMenuBar;
class ShojiMessageView;

/**
 * The window for a file that is a flattened BMessage (a settings file, for instance `~/config/settings/shoji/settings`): the form of the message in a
 * ShojiMessageView, and the file written as the values are changed. Revert puts the file back as it was when it was opened.
 */
class ShojiMessageWindow : public BWindow {
public:
                        ShojiMessageWindow(const entry_ref& ref, const BMessage& data);
    virtual            ~ShojiMessageWindow();

    /** Whether the file is a flattened message (the first bytes say so), and its content. */
    static bool         LoadMessageFile(const entry_ref& ref, BMessage* data);

    virtual void        MessageReceived(BMessage* message);
    virtual bool        QuitRequested();
    virtual void        WindowActivated(bool active);

    ShojiMessageView*   View() const { return fView; }

private:
    status_t            _Save();
    void                _UpdateButtons();
    void                _UpdateTitle();

    entry_ref           fRef;
    bool                fWritable;
    ShojiMessageView*   fView;
    BMenuBar*           fMenuBar;
    BButton*            fRevertButton;
    BButton*            fDefaultsButton;
};
