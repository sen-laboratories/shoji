/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Bitmap.h>
#include <Mime.h>
#include <String.h>
#include <Window.h>

#include <map>
#include <set>
#include <string>

class BButton;
class BMenuBar;

#include "ShojiAttributes.h"
#include "ShojiView.h"

class BGroupView;

namespace sen {
class StripeView;
}

class ShojiWindow : public BWindow
{
    public:
                            ShojiWindow(const entry_ref *ref);
        virtual			    ~ShojiWindow();

        virtual void        MessageReceived(BMessage* message);
        virtual bool        QuitRequested();
        virtual void        WindowActivated(bool active);
        virtual void        FrameResized(float width, float height);

    protected:
        ShojiView*          GetViewTemplateForType(const char* mimeType);
        void                ShowUserError(const char* title, const char* message, status_t errorCode);
        status_t            GetMimeTypeForRef(const entry_ref* ref, char* mimeType);

        BBitmap*            LoadPicture(float height, BBitmap** full = NULL);
        void                RefreshPicture();
        bool                IsFileWritable() const;
        status_t            LoadAttributes(const BMessage* mimeAttrInfo, const BMessage* superTypeAttrInfo);
        void                ConnectControls();
        /** the text fields of attributes that cannot be changed: not editable, greyed, their labels as they are. Only when the controls are attached: a text control makes itself editable then. */
        void                ApplyReadOnly();
        BView*              FindControl(const char* name);
        const ShojiAttribute* FindAttribute(const char* name) const;
        bool                HasChanged(const char* name);

        // write back to the attributes of the file
        status_t            Commit(const char* name);
        status_t            CommitAll();
        void                Revert();
        bool                HasChangesSinceOpen();
        void                UpdateRevertButton();
        // the form is centred on the line through the middle of the picture (a third of the height), as the stripe has it
        void                AlignForm();
        BMenuBar*           BuildMenuBar();
        float               MenuBarHeight() const;
        float               BlockHeight() const;

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
        BMenuBar*           fMenuBar;
        BGroupView*         fPanel;
        float               fPictureHeight;
        sen::StripeView*    fStripe;            // the picture on the left, which is replaced when the thumbnail of the file is written (the place enricher does that a moment after the window was opened)
        BString             fPendingPlace;  // the text of a Location that the place enricher is looking up: its place is opened when it is linked
};
