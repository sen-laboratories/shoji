/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include <Button.h>
#include <ControlLook.h>
#include <Font.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <ListItem.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>

#include <string>
#include <vector>

#include <sen/SenList.h>

#include "ShojiListControl.h"

ShojiListControl::ShojiListControl(const char* name, char separator, const char* label, float divider)
    :
    BControl(name, NULL, NULL, B_WILL_DRAW),
    fSeparator(separator),
    fEdited(false),
    fModificationMessage(NULL)
{
    const float spacing = be_control_look->DefaultItemSpacing();

    fList = new BListView("values", B_SINGLE_SELECTION_LIST);
    fList->SetSelectionMessage(new BMessage(kSelected));
    BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true, B_FANCY_BORDER);
    font_height height;
    be_plain_font->GetHeight(&height);
    scroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, (height.ascent + height.descent + height.leading + 4) * 3));

    fItem = new BTextControl("item", NULL, "", new BMessage(kItemDone));
    fItem->SetModificationMessage(new BMessage(kItemEdited));
    fItem->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 12, B_SIZE_UNSET));
    fAdd = new BButton("add", "Add", new BMessage(kAdd));
    fRemove = new BButton("remove", "Remove", new BMessage(kRemove));

    // the label (if there is one) on the left, at the top of the list: it names all of the values; then the list, and below it the field and the buttons
    BGroupLayout* root = new BGroupLayout(B_HORIZONTAL, spacing);
    SetLayout(root);

    if (label != NULL) {
        BStringView* labelView = new BStringView("label", label);
        labelView->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
        if (divider > 0) {
            labelView->SetExplicitMinSize(BSize(divider, B_SIZE_UNSET));
            labelView->SetExplicitMaxSize(BSize(divider, B_SIZE_UNSET));
        }
        root->AddView(labelView);
    }

    BGroupView* body = new BGroupView(B_VERTICAL, be_control_look->DefaultLabelSpacing());
    body->GroupLayout()->AddView(scroll);
    BGroupView* editRow = new BGroupView(B_HORIZONTAL, be_control_look->DefaultLabelSpacing());
    editRow->GroupLayout()->AddView(fItem);
    editRow->GroupLayout()->AddView(fAdd);
    editRow->GroupLayout()->AddView(fRemove);
    body->GroupLayout()->AddView(editRow);
    root->AddView(body);

    _UpdateButtons();
}

ShojiListControl::~ShojiListControl()
{
    // the items are ours
    while (BListItem* item = fList->RemoveItem((int32) 0))
        delete item;
    delete fModificationMessage;
}

void ShojiListControl::AttachedToWindow()
{
    BControl::AttachedToWindow();

    // the messages of the parts come here, not to the window
    fList->SetTarget(this);
    fItem->SetTarget(this);
    fAdd->SetTarget(this);
    fRemove->SetTarget(this);
}

void ShojiListControl::SetModificationMessage(BMessage* message)
{
    delete fModificationMessage;
    fModificationMessage = message;
}

void ShojiListControl::SetEnabled(bool enabled)
{
    BControl::SetEnabled(enabled);
    fItem->SetEnabled(enabled);
    _UpdateButtons();
}

void ShojiListControl::SetListText(const char* text)
{
    fOriginal = text != NULL ? text : "";
    fEdited = false;

    while (BListItem* item = fList->RemoveItem((int32) 0))
        delete item;
    for (const std::string& value : sen::list::Split(fOriginal.String(), fSeparator))
        fList->AddItem(new BStringItem(value.c_str()));

    fItem->SetText("");
    _UpdateButtons();
}

BString ShojiListControl::ListText() const
{
    if (!fEdited)
        return fOriginal;

    std::vector<std::string> values;
    for (int32 i = 0; i < fList->CountItems(); i++) {
        BStringItem* item = dynamic_cast<BStringItem*>(fList->ItemAt(i));
        if (item != NULL)
            values.push_back(item->Text());
    }
    return BString(sen::list::Join(values, fSeparator).c_str());
}

void ShojiListControl::_UpdateButtons()
{
    fAdd->SetEnabled(IsEnabled());
    fRemove->SetEnabled(IsEnabled() && fList->CurrentSelection() >= 0);
}

// the list was changed by the user: the window is told (and learns the new text when it asks)
void ShojiListControl::_Changed()
{
    fEdited = true;
    InvokeNotify(fModificationMessage, B_CONTROL_MODIFIED);
}

void ShojiListControl::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kSelected:
        {
            // the selected value is edited in the field
            BStringItem* item = dynamic_cast<BStringItem*>(fList->ItemAt(fList->CurrentSelection()));
            fItem->SetText(item != NULL ? item->Text() : "");
            _UpdateButtons();
            break;
        }
        case kItemEdited:
        {
            // (setting the text of the field for another selection sends this too: only a different text is a change)
            int32 selected = fList->CurrentSelection();
            BStringItem* item = dynamic_cast<BStringItem*>(fList->ItemAt(selected));
            if (item == NULL || strcmp(item->Text(), fItem->Text()) == 0)
                break;
            item->SetText(fItem->Text());
            fList->InvalidateItem(selected);
            _Changed();
            break;
        }
        case kItemDone:
            // Enter, or the field was left with another text than it got the focus with
            if (fEdited)
                Invoke();
            break;
        case kAdd:
        {
            fList->AddItem(new BStringItem(""));
            fList->Select(fList->CountItems() - 1);
            fItem->MakeFocus(true);
            break;
        }
        case kRemove:
        {
            int32 selected = fList->CurrentSelection();
            if (selected < 0)
                break;
            delete fList->RemoveItem(selected);
            if (fList->CountItems() > 0)
                fList->Select(std::min(selected, fList->CountItems() - 1));
            else
                fItem->SetText("");
            _UpdateButtons();
            _Changed();
            Invoke();   // (taking a value away is complete at once)
            break;
        }
        default:
            BControl::MessageReceived(message);
    }
}
