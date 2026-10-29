/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Bitmap.h>
#include <Mime.h>
#include <Window.h>

#include <map>
#include <set>
#include <string>

class BButton;

#include "ShojiAttributes.h"
#include "ShojiView.h"

class ShojiWindow : public BWindow
{
    public:
                            ShojiWindow(const entry_ref *ref);
        virtual			    ~ShojiWindow();

        virtual void        MessageReceived(BMessage* message);
        virtual bool        QuitRequested();
        virtual void        WindowActivated(bool active);

    protected:
        ShojiView*          GetViewTemplateForType(const char* mimeType);
        void                ShowUserError(const char* title, const char* message, status_t errorCode);
        status_t            GetMimeTypeForRef(const entry_ref* ref, char* mimeType);

        BBitmap*            LoadPicture(float height);
        bool                IsFileWritable() const;
        status_t            LoadAttributes(const BMessage* mimeAttrInfo, const BMessage* superTypeAttrInfo);
        void                ConnectControls();
        BView*              FindControl(const char* name);
        const ShojiAttribute* FindAttribute(const char* name) const;
        bool                HasChanged(const char* name);

        // write back to the attributes of the file
        status_t            Commit(const char* name);
        status_t            CommitAll();
        void                Revert();
        bool                HasChangesSinceOpen();
        void                UpdateRevertButton();

        // changes of the attributes by others (the file is watched)
        void                ExternalChange(const char* name);
        void                AdoptFileValue(const char* name);
        void                AskAboutExternalChange(const ShojiAttribute& attr, const char* external, const char* mine);
        void                ShowUserEditError(const ShojiAttribute& attr, status_t errorCode);
        void                UpdateTitle();

    private:
        entry_ref           fRef;
        bool                fWritable;
        ShojiView*          fEntityView;
        ShojiAttributeList  fAttrs;
        BMessage            fValues;    // text of the attributes as stored in the file, field per name
        std::set<std::string> fDirty;   // attributes changed in the view, not stored yet
        std::map<std::string, ShojiAttributeSnapshot> fOriginal;    // the editable attributes as the file had them when it was opened
        node_ref            fNodeRef;
        std::set<std::string> fAsking;  // attributes that the user is asked about
        BButton*            fRevertButton;
};
