/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include <ControlLook.h>
#include <Font.h>
#include <GridLayout.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>
#include <Window.h>

#include <algorithm>
#include <string>
#include <vector>

#include <sen/SenList.h>

#include "ShojiListControl.h"

// ---- the editor: a text field that lies over the value that is edited

/**
 * The text field in the list. Enter keeps what was typed and Escape takes it back (both tell the control), and so does leaving the field: the value is kept (that is
 * what a field of a form does, too).
 */
class ShojiValueEditor : public BTextView {
public:
    ShojiValueEditor()
        :
        BTextView("value editor", B_WILL_DRAW),
        fDone(false)
    {
        SetWordWrap(false);
        SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        SetStylable(false);
        SetInsets(7, 1, 3, 1);
    }

    /** where Enter, Escape and leaving the field are told: the control, once it is in a window (a messenger to a handler that has no looper is none) */
    void SetTarget(const BMessenger& target) { fTarget = target; }
    bool HasTarget() const { return fTarget.IsValid(); }

    /** the field is shown for a value: not done yet */
    void Reset() { fDone = false; }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (numBytes == 1 && (bytes[0] == B_RETURN || bytes[0] == B_ESCAPE)) {
            Finish(bytes[0] == B_RETURN ? ShojiListControl::kEditorKept : ShojiListControl::kEditorCancelled);
            return;
        }
        BTextView::KeyDown(bytes, numBytes);
    }

    virtual void MakeFocus(bool focus = true)
    {
        BTextView::MakeFocus(focus);
        if (!focus && !IsHidden())
            Finish(ShojiListControl::kEditorKept);
    }

    virtual void Draw(BRect updateRect)
    {
        BTextView::Draw(updateRect);
        // the edge: this is the one field of the list, it has to stand out from the values
        SetHighUIColor(B_CONTROL_BORDER_COLOR);
        StrokeRect(Bounds());
    }

private:
    void Finish(uint32 what)
    {
        if (fDone)
            return;
        fDone = true;
        fTarget.SendMessage(what);
    }

    BMessenger  fTarget;
    bool        fDone;
};


// ---- the list: its mouse and keys are what it does that BListView does not

class ShojiValueList : public BListView {
public:
    ShojiValueList()
        :
        BListView("values", B_SINGLE_SELECTION_LIST),
        fEditable(true)
    {
    }

    void SetEditable(bool editable) { fEditable = editable; }
    /** where what the mouse and the keys do is sent: the control, once it is in a window */
    void SetControl(const BMessenger& control) { fControl = control; }
    bool HasControl() const { return fControl.IsValid(); }

    virtual void MouseDown(BPoint where)
    {
        BMessage* message = Window() != NULL ? Window()->CurrentMessage() : NULL;
        int32 buttons = message != NULL ? message->GetInt32("buttons", 0) : 0;
        int32 clicks = message != NULL ? message->GetInt32("clicks", 1) : 1;
        MakeFocus(true);

        const int32 index = IndexOf(where);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            if (index >= 0)
                Select(index);
            else
                DeselectAll();
            ShowMenu(ConvertToScreen(where));
            return;
        }

        // (a double click on a value is the invocation of the list: it changes the value, see ShojiListControl)
        if (clicks >= 2 && index < 0 && fEditable) {
            DeselectAll();
            fControl.SendMessage(ShojiListControl::kAddValue);
            return;
        }
        BListView::MouseDown(where);
    }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (fEditable && numBytes == 1) {
            switch (bytes[0]) {
                case B_DELETE:
                case B_BACKSPACE:
                    if (CurrentSelection() >= 0)
                        fControl.SendMessage(ShojiListControl::kDeleteValue);
                    return;
                case B_INSERT:
                case '+':       // (also the plus of the number keys: what is typed, on any layout)
                    fControl.SendMessage(ShojiListControl::kAddValue);
                    return;
                case B_ENTER:
                    if (CurrentSelection() >= 0)
                        fControl.SendMessage(ShojiListControl::kEditValue);
                    return;
                default:
                    break;
            }
        }
        BListView::KeyDown(bytes, numBytes);
    }

private:
    void ShowMenu(BPoint screenWhere)
    {
        BPopUpMenu* menu = new BPopUpMenu("values", false, false);
        BMenuItem* add = new BMenuItem("Add" B_UTF8_ELLIPSIS, new BMessage(ShojiListControl::kAddValue));
        BMenuItem* remove = new BMenuItem("Delete", new BMessage(ShojiListControl::kDeleteValue));
        add->SetEnabled(fEditable);
        remove->SetEnabled(fEditable && CurrentSelection() >= 0);
        menu->AddItem(add);
        menu->AddItem(remove);
        menu->SetTargetForItems(fControl);
        // (asynchronous: the menu is deleted by the looper when it is closed)
        menu->Go(screenWhere, true, true, true);
    }

    BMessenger  fControl;
    bool        fEditable;
};


// ---- the control

ShojiListControl::ShojiListControl(const char* name, char separator, const char* label, float divider, alignment labelAlignment)
    :
    BControl(name, NULL, NULL, B_WILL_DRAW),
    fEditing(-1),
    fEditingNew(false),
    fSeparator(separator),
    fEdited(false),
    fModificationMessage(NULL)
{
    const float spacing = be_control_look->DefaultItemSpacing();

    fList = new ShojiValueList();
    fList->SetInvocationMessage(new BMessage(kEditValue));
    fList->SetToolTip("Double-click a value to change it, the empty part (or + ) to add one.\nDelete removes the selected value.");
    BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true, B_FANCY_BORDER);
    fScroll = scroll;
    _SizeForValues();

    fEditor = new ShojiValueEditor();
    fEditor->Hide();
    fList->AddChild(fEditor);

    // the label (if there is one) on the left, in the middle of the height of the list: it names all of the values
    BGridLayout* root = new BGridLayout(spacing, be_control_look->DefaultLabelSpacing());
    SetLayout(root);

    int32 column = 0;
    if (label != NULL) {
        BStringView* labelView = new BStringView("label", label);
        labelView->SetAlignment(labelAlignment);
        labelView->SetExplicitAlignment(BAlignment(labelAlignment, B_ALIGN_VERTICAL_CENTER));
        if (divider > 0) {
            labelView->SetExplicitMinSize(BSize(divider, B_SIZE_UNSET));
            labelView->SetExplicitMaxSize(BSize(divider, B_SIZE_UNSET));
        }
        root->AddView(labelView, 0, 0);
        column = 1;
    }
    root->AddView(scroll, column, 0);
    root->SetColumnWeight(column, 1);
}

// The list is as high as its values need, with an empty row below them to double-click (one for a language, five for authors at the most): the form does not
// take more height than the values are worth, and a list with more values scrolls.
void ShojiListControl::_SizeForValues()
{
    font_height height;
    be_plain_font->GetHeight(&height);
    const float row = height.ascent + height.descent + height.leading + 4;
    const int32 rows = std::max<int32>(2, std::min<int32>(5, fList->CountItems() + 1));
    fScroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, row * rows));
    fScroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, row * rows));
    fScroll->InvalidateLayout();
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
    // the invocation message of the list (a double click on a value) comes here, not to the window; and the list and the field send what the mouse and the keys do
    // here. (A messenger made in the constructor would be none: the control has no looper before it is in a window.)
    fList->SetTarget(this);
    static_cast<ShojiValueList*>(fList)->SetControl(BMessenger(this));
    fEditor->SetTarget(BMessenger(this));
}

bool ShojiListControl::IsConnected() const
{
    return static_cast<ShojiValueList*>(fList)->HasControl() && fEditor->HasTarget();
}

void ShojiListControl::SetModificationMessage(BMessage* message)
{
    delete fModificationMessage;
    fModificationMessage = message;
}

void ShojiListControl::SetEnabled(bool enabled)
{
    BControl::SetEnabled(enabled);
    if (!enabled && fEditing >= 0)
        _EndEdit(false);
    static_cast<ShojiValueList*>(fList)->SetEditable(enabled);
}

void ShojiListControl::SetListText(const char* text)
{
    if (fEditing >= 0)
        _EndEdit(false);

    fOriginal = text != NULL ? text : "";
    fEdited = false;

    while (BListItem* item = fList->RemoveItem((int32) 0))
        delete item;
    for (const std::string& value : sen::list::Split(fOriginal.String(), fSeparator))
        fList->AddItem(new BStringItem(value.c_str()));
    _SizeForValues();
}

BString ShojiListControl::ListText() const
{
    if (!fEdited)
        return fOriginal;

    std::vector<std::string> values;
    for (int32 i = 0; i < fList->CountItems(); i++) {
        // (a value that is being made is no value yet)
        if (i == fEditing && fEditingNew)
            continue;
        BStringItem* item = dynamic_cast<BStringItem*>(fList->ItemAt(i));
        if (item != NULL)
            values.push_back(item->Text());
    }
    return BString(sen::list::Join(values, fSeparator).c_str());
}

// the list was changed by the user: the window is told (and learns the new text when it asks), and a change of a list is complete at once
void ShojiListControl::_Changed()
{
    fEdited = true;
    InvokeNotify(fModificationMessage, B_CONTROL_MODIFIED);
    Invoke();
}

// A value is edited where it stands: the field lies over it, with its text selected.
void ShojiListControl::_BeginEdit(int32 index, bool isNew)
{
    if (!IsEnabled() || index < 0 || index >= fList->CountItems())
        return;
    if (fEditing >= 0)
        _EndEdit(true);
    // (keeping the value that was edited before can have removed a value: the index is looked up again by the item)
    BStringItem* item = dynamic_cast<BStringItem*>(fList->ItemAt(std::min(index, fList->CountItems() - 1)));
    if (item == NULL)
        return;
    index = fList->IndexOf(item);

    fEditing = index;
    fEditingNew = isNew;
    fList->Select(index);
    fList->ScrollTo(BPoint(0, std::max(0.0f, fList->ItemFrame(index).bottom - fList->Bounds().Height())));

    BRect frame = fList->ItemFrame(index);
    frame.right = std::max(frame.right, fList->Bounds().right);
    fEditor->Reset();
    fEditor->SetText(item->Text());
    fEditor->MoveTo(frame.LeftTop());
    fEditor->ResizeTo(frame.Width(), frame.Height());
    fEditor->Show();
    fEditor->MakeFocus(true);
    fEditor->SelectAll();
}

// The field goes. Kept: the text is the value (an empty one is no value: it is removed, a new one is not made); not kept: the value is as it was.
void ShojiListControl::_EndEdit(bool keep)
{
    if (fEditing < 0)
        return;

    const int32 index = fEditing;
    const bool isNew = fEditingNew;
    fEditing = -1;
    fEditingNew = false;

    BString text(fEditor->Text());
    text.Trim();
    if (!fEditor->IsHidden())
        fEditor->Hide();

    BStringItem* item = dynamic_cast<BStringItem*>(fList->ItemAt(index));
    if (item == NULL)
        return;

    bool changed = false;
    if (keep && !text.IsEmpty()) {
        if (isNew || text != item->Text()) {
            item->SetText(text.String());
            fList->InvalidateItem(index);
            changed = true;
        }
    } else if (isNew || (keep && text.IsEmpty())) {
        // nothing to keep of a new value, and a value that was emptied is taken away
        delete fList->RemoveItem(index);
        changed = !isNew;
    }

    fList->MakeFocus(true);
    if (changed)
        _Changed();
}

void ShojiListControl::_AddValue()
{
    if (!IsEnabled())
        return;
    if (fEditing >= 0)
        _EndEdit(true);
    // an empty value at the end: it is the one that is edited, and it is no value until something is kept
    BStringItem* item = new BStringItem("");
    fList->AddItem(item);
    _BeginEdit(fList->IndexOf(item), true);
}

void ShojiListControl::_DeleteSelected()
{
    if (!IsEnabled())
        return;
    if (fEditing >= 0)
        _EndEdit(false);
    int32 selected = fList->CurrentSelection();
    if (selected < 0)
        return;
    delete fList->RemoveItem(selected);
    if (fList->CountItems() > 0)
        fList->Select(std::min(selected, fList->CountItems() - 1));
    _Changed();
}

void ShojiListControl::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kEditValue:
            // (Enter on a value, and the double click that the list invokes)
            _BeginEdit(fList->CurrentSelection(), false);
            break;
        case kAddValue:
            _AddValue();
            break;
        case kDeleteValue:
            _DeleteSelected();
            break;
        case kEditorKept:
            _EndEdit(true);
            break;
        case kEditorCancelled:
            _EndEdit(false);
            break;
        default:
            BControl::MessageReceived(message);
    }
}
