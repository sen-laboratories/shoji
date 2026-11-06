/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiMessageInspector.h"

#include <Clipboard.h>
#include <Entry.h>
#include <Font.h>
#include <GroupLayout.h>
#include <ListItem.h>
#include <Menu.h>
#include <MenuItem.h>
#include <OutlineListView.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>
#include <Window.h>

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <sen/SenMessagePath.h>
#include <sen/SenSettingsForm.h>

using namespace sen::settings;
namespace form = sen::settings::form;

namespace shoji {

namespace {

// where the columns start (the header and the rows use the same): the name takes what is left of the first part, then the type, then the value
float TypeLeft(float width) { return std::max(170.0f, width * 0.36f); }
float ValueLeft(float width) { return TypeLeft(width) + 90.0f; }

const float kMaxDisplayed = 400;


BString
TypeName(type_code type)
{
    switch (type) {
        case B_BOOL_TYPE:           return "bool";
        case B_INT8_TYPE:           return "int8";
        case B_INT16_TYPE:          return "int16";
        case B_INT32_TYPE:          return "int32";
        case B_INT64_TYPE:          return "int64";
        case B_UINT8_TYPE:          return "uint8";
        case B_UINT16_TYPE:         return "uint16";
        case B_UINT32_TYPE:         return "uint32";
        case B_UINT64_TYPE:         return "uint64";
        case B_FLOAT_TYPE:          return "float";
        case B_DOUBLE_TYPE:         return "double";
        case B_STRING_TYPE:         return "string";
        case B_TIME_TYPE:           return "time";
        case B_POINT_TYPE:          return "point";
        case B_RECT_TYPE:           return "rect";
        case B_RGB_32_BIT_TYPE:
        case B_RGB_COLOR_TYPE:      return "color";
        case B_MESSAGE_TYPE:        return "message";
        case B_RAW_TYPE:            return "raw";
        case B_REF_TYPE:            return "ref";
        case B_MESSENGER_TYPE:      return "messenger";
        case B_POINTER_TYPE:        return "pointer";
        case B_SIZE_TYPE:           return "size";
        case B_OFF_T_TYPE:          return "off_t";
        case B_MIME_TYPE:           return "mime";
    }
    // (the four characters of the type code, as a programmer reads them)
    char text[16];
    snprintf(text, sizeof(text), "'%c%c%c%c'", (char)(type >> 24), (char)(type >> 16), (char)(type >> 8), (char)type);
    return text;
}


/** One value of the message, as the rows show it. */
struct Row {
    Leaf        leaf;
    BString     name;           // `tags[1]` for the second value of an array
    BString     type;
    BString     value;          // as shown: no line breaks, not too long
    std::string text;           // the text that edits it (what GetText gives), for the types of text
    bool        textual = false;    // a text stands for it (SenMessagePath.h)
    bool        editable = false;   // and it can be edited in the row (a string with a line break cannot: the field has one line)
    bool        isBool = false;
    bool        isColor = false;
    rgb_color   color = {0, 0, 0, 255};
};


BString
HexPreview(const void* data, ssize_t size)
{
    BString text;
    text << (int)size << " bytes";
    const uint8* bytes = static_cast<const uint8*>(data);
    for (ssize_t i = 0; i < std::min<ssize_t>(size, 16); i++) {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", bytes[i]);
        text << (i == 0 ? ": " : " ") << hex;
    }
    if (size > 16)
        text << " " B_UTF8_ELLIPSIS;
    return text;
}


/** What the row shows of a value that no text stands for. */
BString
DescribeValue(const BMessage& root, const Leaf& leaf)
{
    detail::Located located;
    if (detail::Locate(root, leaf.path, &located) != B_OK)
        return "?";
    const char* name = located.step.name.c_str();
    const int32 index = (int32)located.step.index;
    BString text;
    switch (leaf.type) {
        case B_MESSAGE_TYPE:
        {
            BMessage child;
            located.container.FindMessage(name, index, &child);
            const int32 fields = child.CountNames(B_ANY_TYPE);
            text << "{" << (int)fields << (fields == 1 ? " field}" : " fields}");
            return text;
        }
        case B_REF_TYPE:
        {
            entry_ref ref;
            if (located.container.FindRef(name, index, &ref) != B_OK)
                return "?";
            BPath path(&ref);
            return path.InitCheck() == B_OK ? BString(path.Path()) : BString(ref.name != NULL ? ref.name : "(no name)");
        }
        case B_MESSENGER_TYPE:
        {
            BMessenger messenger;
            located.container.FindMessenger(name, index, &messenger);
            text << "team " << (int)messenger.Team() << (messenger.IsValid() ? "" : " (gone)");
            return text;
        }
        case B_POINTER_TYPE:
        {
            void* pointer = NULL;
            located.container.FindPointer(name, index, &pointer);
            char hex[32];
            snprintf(hex, sizeof(hex), "%p", pointer);
            return hex;
        }
        default:
        {
            const void* data = NULL;
            ssize_t size = 0;
            if (located.container.FindData(name, leaf.type, index, &data, &size) != B_OK)
                return "?";
            return HexPreview(data, size);
        }
    }
}


Row
MakeRow(const BMessage& root, const Leaf& leaf)
{
    Row row;
    row.leaf = leaf;
    row.name = leaf.name.c_str();
    if (leaf.count > 1)
        row.name << "[" << (int)leaf.index << "]";
    row.type = TypeName(leaf.type);
    row.textual = detail::IsText(leaf.type);
    row.isBool = leaf.type == B_BOOL_TYPE;
    row.isColor = leaf.type == B_RGB_32_BIT_TYPE || leaf.type == B_RGB_COLOR_TYPE;

    if (!leaf.addressable) {
        row.value = "(its name cannot be used as a path)";
        return row;
    }
    if (row.textual && GetText(root, leaf.path, &row.text) == B_OK) {
        row.editable = row.text.find_first_of("\n\r") == std::string::npos;
        std::string shown = row.text;
        for (char& c : shown) {
            if (c == '\n' || c == '\r')
                c = ' ';
        }
        if (shown.size() > (size_t)kMaxDisplayed)
            shown = shown.substr(0, (size_t)kMaxDisplayed) + B_UTF8_ELLIPSIS;
        row.value = shown.c_str();
        if (row.isColor) {
            detail::Located located;
            const void* data = NULL;
            ssize_t size = 0;
            if (detail::Locate(root, leaf.path, &located) == B_OK
                    && located.container.FindData(located.step.name.c_str(), leaf.type, (int32)located.step.index, &data, &size) == B_OK
                    && size == (ssize_t)sizeof(row.color))
                memcpy((void*)&row.color, data, sizeof(row.color));
        }
        return row;
    }
    row.textual = false;
    row.value = DescribeValue(root, leaf);
    return row;
}


// ---- the rows

class RowItem : public BListItem {
public:
    RowItem(const Row& row)
        :
        BListItem((uint32)row.leaf.depth, true),
        fRow(row)
    {
    }

    Row&        GetRow() { return fRow; }

    virtual void Update(BView* owner, const BFont* font)
    {
        BListItem::Update(owner, font);
        font_height height;
        font->GetHeight(&height);
        SetHeight(ceilf(height.ascent + height.descent + height.leading) + 4);
    }

    virtual void DrawItem(BView* owner, BRect frame, bool complete)
    {
        const float width = owner->Bounds().Width();
        const float typeLeft = TypeLeft(width);
        const float valueLeft = ValueLeft(width);

        const bool selected = IsSelected();
        const rgb_color low = owner->LowColor();
        const rgb_color background = selected ? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : ui_color(B_LIST_BACKGROUND_COLOR);
        if (selected || complete) {
            owner->SetHighColor(background);
            owner->FillRect(BRect(frame.left, frame.top, std::max(frame.right, width), frame.bottom));
        }
        owner->SetLowColor(background);

        // the lines between the columns
        owner->SetHighColor(tint_color(background, B_DARKEN_1_TINT));
        owner->StrokeLine(BPoint(typeLeft - 4, frame.top), BPoint(typeLeft - 4, frame.bottom));
        owner->StrokeLine(BPoint(valueLeft - 4, frame.top), BPoint(valueLeft - 4, frame.bottom));

        const rgb_color text = selected ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR) : ui_color(B_LIST_ITEM_TEXT_COLOR);
        font_height height;
        owner->GetFontHeight(&height);
        const float baseline = frame.top + (frame.Height() - (height.ascent + height.descent)) / 2 + height.ascent;
        BFont font;
        owner->GetFont(&font);

        // the name (a message that has rows below it is in bold), the type (dim), the value
        BString name(fRow.name);
        font.TruncateString(&name, B_TRUNCATE_END, typeLeft - 8 - frame.left);
        owner->SetHighColor(text);
        owner->MovePenTo(frame.left + 2, baseline);
        owner->DrawString(name.String());

        BString type(fRow.type);
        owner->SetHighColor(tint_color(text, selected ? B_NO_TINT : B_LIGHTEN_1_TINT));
        owner->MovePenTo(typeLeft, baseline);
        owner->DrawString(type.String());

        float x = valueLeft;
        if (fRow.isColor) {
            // the colour itself, before its text
            const float side = frame.Height() - 4;
            const BRect swatch(x, frame.top + 2, x + side, frame.top + 2 + side);
            owner->SetHighColor(fRow.color);
            owner->FillRect(swatch);
            owner->SetHighColor(tint_color(background, B_DARKEN_3_TINT));
            owner->StrokeRect(swatch);
            x += side + 6;
        }
        BString value(fRow.value);
        font.TruncateString(&value, B_TRUNCATE_END, std::max(10.0f, width - x - 4));
        // (what cannot be edited is a little dimmer)
        owner->SetHighColor(fRow.editable || !fRow.textual ? text : tint_color(text, B_LIGHTEN_1_TINT));
        owner->MovePenTo(x, baseline);
        owner->DrawString(value.String());
        owner->SetLowColor(low);
    }

private:
    Row         fRow;
};


// ---- the field that lies over a value that is edited (the pattern of the list control)

}   // namespace

class ValueEditor : public BTextView {
public:
    ValueEditor()
        :
        BTextView("inspector editor", B_WILL_DRAW),
        fDone(false)
    {
        SetWordWrap(false);
        SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        SetStylable(false);
        SetInsets(3, 1, 3, 1);
    }

    void SetTarget(const BMessenger& target) { fTarget = target; }
    void Reset() { fDone = false; }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (numBytes == 1 && (bytes[0] == B_RETURN || bytes[0] == B_ESCAPE)) {
            Finish(bytes[0] == B_RETURN ? ShojiMessageInspector::kEditorKept : ShojiMessageInspector::kEditorCancelled);
            return;
        }
        BTextView::KeyDown(bytes, numBytes);
    }

    // leaving the field keeps the value, as a field of a form does
    virtual void MakeFocus(bool focus = true)
    {
        BTextView::MakeFocus(focus);
        if (!focus && !IsHidden())
            Finish(ShojiMessageInspector::kEditorKept);
    }

    virtual void Draw(BRect updateRect)
    {
        BTextView::Draw(updateRect);
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


// ---- the list: the mouse and the keys

class InspectorList : public BOutlineListView {
public:
    InspectorList()
        :
        BOutlineListView("inspector rows", B_SINGLE_SELECTION_LIST),
        fEditable(true),
        fHeader(NULL)
    {
    }

    void SetEditable(bool editable) { fEditable = editable; }
    void SetOwner(const BMessenger& owner) { fOwner = owner; }
    void SetHeader(BView* header) { fHeader = header; }

    virtual void FrameResized(float width, float height)
    {
        BOutlineListView::FrameResized(width, height);
        Invalidate();
        if (fHeader != NULL)
            fHeader->Invalidate();
    }

    virtual void MouseDown(BPoint where)
    {
        BMessage* message = Window() != NULL ? Window()->CurrentMessage() : NULL;
        const int32 buttons = message != NULL ? message->GetInt32("buttons", 0) : 0;
        const int32 clicks = message != NULL ? message->GetInt32("clicks", 1) : 1;
        MakeFocus(true);

        const int32 index = IndexOf(where);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            if (index >= 0)
                Select(index);
            ShowMenu(ConvertToScreen(where));
            return;
        }
        BOutlineListView::MouseDown(where);
        // (a double click on a value that can be edited: edit it; the latch of a message is its own click)
        if (clicks >= 2 && index >= 0 && fOwner.IsValid() && where.x > TypeLeft(Bounds().Width()))
            fOwner.SendMessage(ShojiMessageInspector::kEdit);
    }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (numBytes == 1 && fOwner.IsValid() && CurrentSelection() >= 0) {
            switch (bytes[0]) {
                case B_ENTER:
                    fOwner.SendMessage(ShojiMessageInspector::kEdit);
                    return;
                case B_SPACE:
                {
                    RowItem* item = dynamic_cast<RowItem*>(ItemAt(CurrentSelection()));
                    if (item != NULL && item->GetRow().isBool) {
                        fOwner.SendMessage(ShojiMessageInspector::kEdit);
                        return;
                    }
                    break;
                }
                case B_DELETE:
                case B_BACKSPACE:
                    if (fEditable)
                        fOwner.SendMessage(ShojiMessageInspector::kRemove);
                    return;
                default:
                    break;
            }
        }
        BOutlineListView::KeyDown(bytes, numBytes);
    }

private:
    void ShowMenu(BPoint screenWhere)
    {
        RowItem* item = dynamic_cast<RowItem*>(ItemAt(CurrentSelection()));
        const bool selected = item != NULL;
        BPopUpMenu* menu = new BPopUpMenu("row", false, false);
        BMenuItem* edit = new BMenuItem("Edit", new BMessage(ShojiMessageInspector::kEdit));
        edit->SetEnabled(fEditable && selected && item->GetRow().editable);
        BMenuItem* copyPath = new BMenuItem("Copy path key", new BMessage(ShojiMessageInspector::kCopyPath));
        copyPath->SetEnabled(selected && item->GetRow().leaf.addressable);
        BMenuItem* copyValue = new BMenuItem("Copy value", new BMessage(ShojiMessageInspector::kCopyValue));
        copyValue->SetEnabled(selected);
        BMenuItem* remove = new BMenuItem("Delete", new BMessage(ShojiMessageInspector::kRemove));
        remove->SetEnabled(fEditable && selected && item->GetRow().leaf.addressable);
        menu->AddItem(edit);
        menu->AddSeparatorItem();
        menu->AddItem(copyPath);
        menu->AddItem(copyValue);
        menu->AddSeparatorItem();
        menu->AddItem(remove);
        menu->SetTargetForItems(fOwner);
        // (asynchronous: the looper deletes the menu when it is closed)
        menu->Go(screenWhere, true, true, true);
    }

    BMessenger  fOwner;
    bool        fEditable;
    BView*      fHeader;
};


// ---- the names of the columns, over the list

class InspectorHeader : public BView {
public:
    InspectorHeader(BView* list)
        :
        BView("inspector header", B_WILL_DRAW),
        fList(list)
    {
        SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
    }

    virtual BSize MinSize()
    {
        font_height height;
        GetFontHeight(&height);
        return BSize(50, ceilf(height.ascent + height.descent + height.leading) + 6);
    }

    virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, MinSize().height); }
    virtual BSize PreferredSize() { return MinSize(); }

    virtual void Draw(BRect)
    {
        // (the list is as wide as its rows are: the columns of both start in the same places, the header has the space of the scroll bar more)
        const float width = fList->Bounds().Width();
        font_height height;
        GetFontHeight(&height);
        const float baseline = (Bounds().Height() - (height.ascent + height.descent)) / 2 + height.ascent;
        const float offset = 3;     // (the frame of the scroll view)
        SetHighUIColor(B_PANEL_TEXT_COLOR);
        MovePenTo(offset + 16, baseline);
        DrawString("Name");
        MovePenTo(offset + TypeLeft(width), baseline);
        DrawString("Type");
        MovePenTo(offset + ValueLeft(width), baseline);
        DrawString("Value");
        SetHighUIColor(B_PANEL_BACKGROUND_COLOR, B_DARKEN_2_TINT);
        StrokeLine(BPoint(0, Bounds().bottom), BPoint(Bounds().right, Bounds().bottom));
    }

private:
    BView*  fList;
};

}   // namespace shoji


using namespace shoji;

ShojiMessageInspector::ShojiMessageInspector(const char* name)
    :
    BGroupView(name, B_VERTICAL, 0),
    fEditable(true),
    fList(NULL),
    fHeader(NULL),
    fScroll(NULL),
    fStatus(NULL),
    fEditor(NULL),
    fEditing(-1)
{
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    fList = new InspectorList();
    fList->SetSelectionMessage(new BMessage(kSelected));
    fHeader = new InspectorHeader(fList);
    fList->SetHeader(fHeader);
    fScroll = new BScrollView("inspector scroll", fList, 0, false, true, B_FANCY_BORDER);
    fStatus = new BStringView("inspector status", "");
    fStatus->SetHighUIColor(B_PANEL_TEXT_COLOR, B_DARKEN_1_TINT);
    fStatus->SetExplicitMinSize(BSize(50, B_SIZE_UNSET));

    fEditor = new ValueEditor();
    fList->AddChild(fEditor);
    fEditor->Hide();

    // (a list has no size of its own to ask for: room for the three columns and a screenful of rows)
    const float em = be_plain_font->StringWidth("M");
    SetExplicitMinSize(BSize(em * 32, em * 12));
    SetExplicitPreferredSize(BSize(em * 56, em * 32));

    GroupLayout()->AddView(fHeader);
    GroupLayout()->AddView(fScroll);
    GroupLayout()->AddView(fStatus);
}


ShojiMessageInspector::~ShojiMessageInspector()
{
    // the items are ours, the views are the window's
    std::vector<BListItem*> items;
    for (int32 i = 0; i < fList->FullListCountItems(); i++)
        items.push_back(fList->FullListItemAt(i));
    fList->MakeEmpty();
    for (BListItem* item : items)
        delete item;
}


void ShojiMessageInspector::AttachedToWindow()
{
    BGroupView::AttachedToWindow();
    fList->SetTarget(this);
    fList->SetOwner(BMessenger(this));
}


status_t ShojiMessageInspector::SetData(const BMessage& message)
{
    fWorking = message;
    _Rebuild();
    return B_OK;
}


void ShojiMessageInspector::SetEditable(bool editable)
{
    fEditable = editable;
    fList->SetEditable(editable);
    if (!editable)
        _EndEdit(false);
    _ShowStatus();
}


void ShojiMessageInspector::CommitPending()
{
    _EndEdit(true);
}


BTextView* ShojiMessageInspector::Editor() const
{
    return fEditing >= 0 ? fEditor : NULL;
}


int32 ShojiMessageInspector::CountRows() const { return fList->CountItems(); }
int32 ShojiMessageInspector::CountAllRows() const { return fList->FullListCountItems(); }


int32 ShojiMessageInspector::RowOf(const std::string& path) const
{
    for (int32 i = 0; i < fList->FullListCountItems(); i++) {
        RowItem* item = dynamic_cast<RowItem*>(fList->FullListItemAt(i));
        if (item != NULL && item->GetRow().leaf.addressable && item->GetRow().leaf.path == path)
            return i;
    }
    return -1;
}


static RowItem* ItemAtRow(BOutlineListView* list, int32 row)
{
    return dynamic_cast<RowItem*>(list->FullListItemAt(row));
}

BString ShojiMessageInspector::RowName(int32 row) const { RowItem* item = ItemAtRow(fList, row); return item != NULL ? item->GetRow().name : BString(); }
BString ShojiMessageInspector::RowType(int32 row) const { RowItem* item = ItemAtRow(fList, row); return item != NULL ? item->GetRow().type : BString(); }
BString ShojiMessageInspector::RowValue(int32 row) const { RowItem* item = ItemAtRow(fList, row); return item != NULL ? item->GetRow().value : BString(); }
std::string ShojiMessageInspector::RowPath(int32 row) const { RowItem* item = ItemAtRow(fList, row); return item != NULL ? item->GetRow().leaf.path : std::string(); }
bool ShojiMessageInspector::RowEditable(int32 row) const { RowItem* item = ItemAtRow(fList, row); return item != NULL && item->GetRow().editable && fEditable; }


bool ShojiMessageInspector::SelectPath(const std::string& path)
{
    const int32 row = RowOf(path);
    if (row < 0)
        return false;
    // (a row below a collapsed one is shown first)
    RowItem* item = ItemAtRow(fList, row);
    BListItem* parent = fList->Superitem(item);
    while (parent != NULL) {
        if (!parent->IsExpanded())
            fList->Expand(parent);
        parent = fList->Superitem(parent);
    }
    fList->Select(fList->IndexOf(item));
    fList->ScrollToSelection();
    return true;
}


int32 ShojiMessageInspector::_SelectedRow() const
{
    BListItem* item = fList->ItemAt(fList->CurrentSelection());
    return item != NULL ? fList->FullListIndexOf(item) : -1;
}


// Rows for all values, a message with the rows of its fields below it. What was collapsed stays collapsed, what was selected stays selected if it is still there.
void ShojiMessageInspector::_Rebuild()
{
    _EndEdit(false);

    std::string selected;
    for (int32 i = 0; i < fList->FullListCountItems(); i++) {
        RowItem* item = dynamic_cast<RowItem*>(fList->FullListItemAt(i));
        if (item == NULL || !item->GetRow().leaf.addressable)
            continue;
        BListItem* next = fList->FullListItemAt(i + 1);
        if (item->GetRow().leaf.type == B_MESSAGE_TYPE && next != NULL && next->OutlineLevel() > item->OutlineLevel()) {
            if (item->IsExpanded())
                fCollapsed.erase(item->GetRow().leaf.path);
            else
                fCollapsed.insert(item->GetRow().leaf.path);
        }
    }
    if (_SelectedRow() >= 0)
        selected = RowPath(_SelectedRow());

    std::vector<BListItem*> old;
    for (int32 i = 0; i < fList->FullListCountItems(); i++)
        old.push_back(fList->FullListItemAt(i));
    fList->MakeEmpty();
    for (BListItem* item : old)
        delete item;

    std::vector<Leaf> leaves;
    Enumerate(fWorking, &leaves);
    for (const Leaf& leaf : leaves)
        fList->AddItem(new RowItem(MakeRow(fWorking, leaf)));

    for (int32 i = fList->FullListCountItems() - 1; i >= 0; i--) {
        RowItem* item = ItemAtRow(fList, i);
        if (item != NULL && item->GetRow().leaf.type == B_MESSAGE_TYPE && item->GetRow().leaf.addressable
                && fCollapsed.count(item->GetRow().leaf.path) > 0)
            fList->Collapse(item);
    }

    if (!selected.empty())
        SelectPath(selected);
    _ShowStatus();
}


void ShojiMessageInspector::_ShowStatus()
{
    BString text;
    const int32 row = _SelectedRow();
    RowItem* item = row >= 0 ? ItemAtRow(fList, row) : NULL;
    if (item == NULL) {
        text << (int)fList->FullListCountItems() << " values";
        if (!fEditable)
            text << " (read-only)";
    } else {
        const Row& r = item->GetRow();
        text << (r.leaf.addressable ? r.leaf.path.c_str() : "(no path)") << "  " B_UTF8_BULLET "  " << r.type;
        if (r.leaf.count > 1)
            text << "[" << (int)r.leaf.count << "]";
        if (!fEditable)
            text << "  (read-only)";
    }
    fStatus->SetText(text.String());
}


// Enter or a double click: a bool is toggled, any other value that a text stands for gets the field over it.
void ShojiMessageInspector::_EditSelected()
{
    const int32 row = _SelectedRow();
    RowItem* item = row >= 0 ? ItemAtRow(fList, row) : NULL;
    if (item == NULL || !fEditable || !item->GetRow().editable)
        return;

    const Row& r = item->GetRow();
    if (r.isBool) {
        SetValue(r.leaf.path, r.text == "true" ? "false" : "true");
        return;
    }

    _EndEdit(true);
    // (the rows can have changed when the value before was kept)
    const int32 visible = fList->IndexOf(ItemAtRow(fList, row));
    if (visible < 0)
        return;
    fEditing = row;
    const BRect frame = fList->ItemFrame(visible);
    const float width = fList->Bounds().Width();
    fEditor->MoveTo(ValueLeft(width) - 2, frame.top);
    fEditor->ResizeTo(std::max(30.0f, width - ValueLeft(width) + 1), frame.Height() - 1);
    fEditor->SetText(r.text.c_str());
    fEditor->SetTarget(BMessenger(this));
    fEditor->Reset();
    fEditor->Show();
    fEditor->MakeFocus(true);
    fEditor->SelectAll();
}


void ShojiMessageInspector::_EndEdit(bool keep)
{
    if (fEditing < 0)
        return;
    const int32 row = fEditing;
    fEditing = -1;
    const std::string text = fEditor->Text();
    fEditor->Hide();
    fList->MakeFocus(true);
    if (keep && row < fList->FullListCountItems()) {
        const std::string path = RowPath(row);
        if (!path.empty())
            SetValue(path, text);
    }
}


status_t ShojiMessageInspector::SetValue(const std::string& path, const std::string& text)
{
    if (!fEditable)
        return B_NOT_ALLOWED;

    std::string before;
    status_t result = GetText(fWorking, path, &before);
    BMessage changed(fWorking);
    if (result == B_OK)
        result = SetText(&changed, path, text);
    if (result != B_OK) {
        BString note("Refused: ");
        note << path.c_str() << " (" << (result == B_BAD_VALUE ? "the value does not fit its type" : strerror(result)) << ")";
        fStatus->SetText(note.String());
        if (fTarget.IsValid()) {
            BMessage refused(form::kRefused);
            refused.AddString(form::kFieldPath, path.c_str());
            refused.AddString(form::kFieldValue, text.c_str());
            refused.AddInt32(form::kFieldStatus, result);
            refused.AddString(form::kFieldDetail, result == B_BAD_VALUE ? "The value does not fit." : strerror(result));
            refused.AddInt32(form::kFieldProtocol, form::kProtocol);
            fTarget.SendMessage(&refused);
        }
        return result;
    }
    fWorking = changed;

    std::string after;
    GetText(fWorking, path, &after);
    const int32 row = RowOf(path);
    RowItem* item = row >= 0 ? ItemAtRow(fList, row) : NULL;
    if (item != NULL) {
        Leaf leaf = item->GetRow().leaf;
        item->GetRow() = MakeRow(fWorking, leaf);
        const int32 visible = fList->IndexOf(item);
        if (visible >= 0)
            fList->InvalidateItem(visible);
    }
    _ShowStatus();
    if (after != before)
        _Changed(path, after);
    return B_OK;
}


status_t ShojiMessageInspector::RemoveValue(const std::string& path)
{
    if (!fEditable)
        return B_NOT_ALLOWED;
    BMessage changed(fWorking);
    status_t result = Remove(&changed, path);
    if (result != B_OK)
        return result;
    fWorking = changed;
    _Rebuild();
    _Changed(std::string(), std::string());
    return B_OK;
}


// told to the target: a value changed (path and its text), or the whole message (no path: a value was removed)
void ShojiMessageInspector::_Changed(const std::string& path, const std::string& value)
{
    if (!fTarget.IsValid())
        return;
    BMessage changed(form::kChanged);
    if (!path.empty()) {
        changed.AddString(form::kFieldPath, path.c_str());
        changed.AddString(form::kFieldValue, value.c_str());
    }
    changed.AddMessage(form::kFieldSettings, &fWorking);
    changed.AddInt32(form::kFieldProtocol, form::kProtocol);
    fTarget.SendMessage(&changed);
}


void ShojiMessageInspector::_Copy(const std::string& text)
{
    if (!be_clipboard->Lock())
        return;
    be_clipboard->Clear();
    BMessage* data = be_clipboard->Data();
    if (data != NULL)
        data->AddData("text/plain", B_MIME_TYPE, text.data(), (ssize_t)text.size());
    be_clipboard->Commit();
    be_clipboard->Unlock();
}


void ShojiMessageInspector::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kEdit:
            _EditSelected();
            break;
        case kEditorKept:
            _EndEdit(true);
            break;
        case kEditorCancelled:
            _EndEdit(false);
            break;
        case kRemove:
        {
            const int32 row = _SelectedRow();
            if (row >= 0 && !RowPath(row).empty())
                RemoveValue(RowPath(row));
            break;
        }
        case kCopyPath:
        {
            const int32 row = _SelectedRow();
            if (row >= 0)
                _Copy(RowPath(row));
            break;
        }
        case kCopyValue:
        {
            RowItem* item = ItemAtRow(fList, _SelectedRow());
            if (item != NULL)
                _Copy(item->GetRow().textual ? item->GetRow().text : std::string(item->GetRow().value.String()));
            break;
        }
        case kSelected:
            _ShowStatus();
            break;
        default:
            BGroupView::MessageReceived(message);
            break;
    }
}
