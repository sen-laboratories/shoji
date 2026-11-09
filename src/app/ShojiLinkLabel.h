/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Entry.h>
#include <String.h>
#include <View.h>

namespace shoji {

/** What the label of a field does when it is clicked. */
enum LinkKind {
    kNoLink,
    kWebLink,       ///< the text is an address: it opens in the web browser of the user
    kMailLink,      ///< the text is an e-mail address: a new message to it is started in the mail program of the user
    kPlaceLink      ///< the text is the name of a place: the enricher of places looks it up, makes the place and relates the file to it
};

/** The attributes that have a link label (a later version can read this from the ontology). */
LinkKind LinkKindOf(const char* attribute);

/** What a click on the label of @p attribute does for the text of its field; @p file is the file that the form shows. */
status_t OpenLink(LinkKind kind, const char* text, const entry_ref& file);

}   // namespace shoji


/**
 * The label of a field that links somewhere: shown as a link (the color and the underline that Haiku has for links) with the cursor of a link while the field has a text to follow;
 * without a text it is a label as any other. A click posts SHOJI_MSG_LINK_CLICKED with the name of the attribute to the window, which opens the link (shoji::OpenLink).
 */
class ShojiLinkLabel : public BView {
    public:
        enum { kClicked = 'SJlk' };

                            ShojiLinkLabel(const char* attribute, const char* text);

        virtual void        AttachedToWindow();
        virtual void        Draw(BRect updateRect);
        virtual void        MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage);
        virtual void        MouseDown(BPoint where);
        virtual void        Pulse();
        virtual BSize       MinSize();
        virtual BSize       MaxSize();
        virtual BSize       PreferredSize();

    private:
        /** whether the field has a text that can be followed */
        bool                _Active();

        BString             fAttribute;
        BString             fText;
        bool                fHover;
        bool                fWasActive;
};
