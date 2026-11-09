/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "ShojiLinkLabel.h"

#include <Alert.h>
#include <Cursor.h>
#include <Font.h>
#include <LayoutUtils.h>
#include <Roster.h>
#include <TextControl.h>
#include <Url.h>
#include <Window.h>

#include <string.h>

static const char* const kPlaceEnricherSignature = "application/x-vnd.sen-plugin.sen-labs.place-enricher";


namespace shoji {

LinkKind
LinkKindOf(const char* attribute)
{
    if (strcmp(attribute, "foaf:homepage") == 0)
        return kWebLink;
    if (strcmp(attribute, "foaf:mbox") == 0)
        return kMailLink;
    if (strcmp(attribute, "schema:homeLocation") == 0)
        return kPlaceLink;
    return kNoLink;
}


status_t
OpenLink(LinkKind kind, const char* text, const entry_ref& file)
{
    BString value(text);
    value.Trim();
    if (value.IsEmpty())
        return B_BAD_VALUE;

    if (kind == kWebLink) {
        // (an address without a scheme, as people write it: www.sen-labs.org)
        if (value.FindFirst("://") < 0)
            value.Prepend("https://");
        BUrl url(value, true);
        if (!url.IsValid()) {
            (new BAlert("link", "This is not an address that a web browser can open.", "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go(NULL);
            return B_BAD_VALUE;
        }
        return url.OpenWithPreferredApplication();
    }

    if (kind == kMailLink) {
        // (the address as it is stored: `mailto:` in front of it or not, and a name in front of it as in "Ann <ann@example.org>")
        const int32 open = value.FindFirst('<');
        const int32 close = value.FindLast('>');
        if (open >= 0 && close > open)
            value.Truncate(close).Remove(0, open + 1);
        if (value.IFindFirst("mailto:") == 0)
            value.Remove(0, 7);
        value.Trim();
        if (value.FindFirst('@') <= 0) {
            (new BAlert("link", "This is not an e-mail address.", "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go(NULL);
            return B_BAD_VALUE;
        }
        value.Prepend("mailto:");
        BUrl url(value, true);
        return url.OpenWithPreferredApplication();
    }

    if (kind == kPlaceLink) {
        // the enricher of places is asked for the place of that name (it chooses if there are several, as it does for any file), makes it a file of its own and relates this one to it
        entry_ref plugin;
        if (be_roster->FindApp(kPlaceEnricherSignature, &plugin) != B_OK) {
            (new BAlert("link", "The place enricher (SEN Spaces) is not installed: it looks the place up. Install the package sen_plugin_place_enricher.", "OK", NULL, NULL,
                B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go(NULL);
            return B_NAME_NOT_FOUND;
        }
        BMessage lookup(B_REFS_RECEIVED);
        lookup.AddRef("refs", &file);
        lookup.AddString("query", value.String());
        lookup.AddBool("wipe", false);
        status_t result = be_roster->Launch(&plugin, &lookup);
        return result == B_ALREADY_RUNNING ? B_OK : result;
    }
    return B_BAD_VALUE;
}

}   // namespace shoji


ShojiLinkLabel::ShojiLinkLabel(const char* attribute, const char* text)
    :
    BView(NULL, B_WILL_DRAW | B_PULSE_NEEDED),
    fAttribute(attribute),
    fText(text),
    fHover(false),
    fWasActive(false)
{
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    SetExplicitAlignment(BAlignment(B_ALIGN_RIGHT, B_ALIGN_VERTICAL_CENTER));
}


void
ShojiLinkLabel::AttachedToWindow()
{
    BView::AttachedToWindow();
    fWasActive = _Active();
}


bool
ShojiLinkLabel::_Active()
{
    if (Window() == NULL)
        return false;
    BTextControl* field = dynamic_cast<BTextControl*>(Window()->FindView(fAttribute.String()));
    if (field == NULL)
        return false;
    BString text(field->Text());
    text.Trim();
    return !text.IsEmpty();
}


void
ShojiLinkLabel::Draw(BRect)
{
    const bool active = _Active();
    BFont font;
    GetFont(&font);
    font.SetFace(active ? B_UNDERSCORE_FACE : B_REGULAR_FACE);
    SetFont(&font);
    SetHighUIColor(active ? (fHover ? B_LINK_HOVER_COLOR : B_LINK_TEXT_COLOR) : B_PANEL_TEXT_COLOR);

    font_height height;
    font.GetHeight(&height);
    const float width = font.StringWidth(fText.String());
    DrawString(fText.String(), BPoint(Bounds().right - width,
        floorf((Bounds().Height() - (height.ascent + height.descent)) / 2 + height.ascent)));
}


void
ShojiLinkLabel::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
{
    const bool active = _Active();
    const bool inside = transit != B_EXITED_VIEW && transit != B_OUTSIDE_VIEW;
    // the cursor of a link while there is something to follow, else the one of the system
    BCursor cursor(inside && active ? B_CURSOR_ID_FOLLOW_LINK : B_CURSOR_ID_SYSTEM_DEFAULT);
    SetViewCursor(&cursor);
    if (inside != fHover) {
        fHover = inside;
        Invalidate();
    }
    BView::MouseMoved(where, transit, dragMessage);
}


void
ShojiLinkLabel::MouseDown(BPoint where)
{
    BMessage* current = Window() != NULL ? Window()->CurrentMessage() : NULL;
    int32 buttons = 0;
    if (current != NULL)
        current->FindInt32("buttons", &buttons);
    if (_Active() && (buttons & B_PRIMARY_MOUSE_BUTTON) != 0) {
        BMessage clicked(kClicked);
        clicked.AddString("attribute", fAttribute.String());
        Window()->PostMessage(&clicked);
        return;
    }
    BView::MouseDown(where);
}


void
ShojiLinkLabel::Pulse()
{
    // the field was edited: the label is a link now (or no longer)
    const bool active = _Active();
    if (active != fWasActive) {
        fWasActive = active;
        Invalidate();
    }
}


BSize
ShojiLinkLabel::MinSize()
{
    return PreferredSize();
}


BSize
ShojiLinkLabel::MaxSize()
{
    return BLayoutUtils::ComposeSize(ExplicitMaxSize(), BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
}


BSize
ShojiLinkLabel::PreferredSize()
{
    BFont font;
    GetFont(&font);
    font_height height;
    font.GetHeight(&height);
    return BSize(ceilf(font.StringWidth(fText.String())) + 1, ceilf(height.ascent + height.descent + height.leading));
}
