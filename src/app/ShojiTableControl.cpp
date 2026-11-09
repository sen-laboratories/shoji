/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "ShojiTableControl.h"

#include <ControlLook.h>
#include <Cursor.h>
#include <Handler.h>
#include <ListItem.h>
#include <ListView.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <String.h>
#include <TextView.h>
#include <Window.h>

#include <algorithm>
#include <math.h>
#include <string.h>

namespace {

const float kPadding = 8;
const int32 kVisibleRows = 8;
const int32 kMinimumRows = 2;
const uint32 kEditorDone = 'TBed';          // the text field over a cell is finished: serial, keep, refocus

float RowHeight()
{
    font_height height;
    be_plain_font->GetHeight(&height);
    return ceilf(height.ascent + height.descent + height.leading) + 8;
}

}   // namespace


// #pragma mark - the field that replaces a text cell


/** The text field over a text cell. Enter and Tab keep what was typed, Escape takes it back, and so does leaving the field: the value is kept (what a field of a form does). */
class ShojiTableEditor : public BTextView {
public:
    ShojiTableEditor(ShojiTableControl* table, int32 serial)
        :
        BTextView("table editor", B_WILL_DRAW),
        fTable(table),
        fSerial(serial),
        fDone(false)
    {
        SetWordWrap(false);
        SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        SetStylable(false);
    }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (numBytes == 1 && (bytes[0] == B_RETURN || bytes[0] == B_ESCAPE || bytes[0] == B_TAB)) {
            Finish(bytes[0] != B_ESCAPE, true);
            return;
        }
        BTextView::KeyDown(bytes, numBytes);
    }

    virtual void MakeFocus(bool focus = true)
    {
        BTextView::MakeFocus(focus);
        if (!focus && !IsHidden())
            Finish(true, false);
    }

    virtual void Draw(BRect updateRect)
    {
        BTextView::Draw(updateRect);
        SetHighUIColor(B_CONTROL_BORDER_COLOR);
        StrokeRect(Bounds());
    }

private:
    // (told by a message: the field is taken away by the table, not while it runs)
    void Finish(bool keep, bool refocus)
    {
        if (fDone)
            return;
        fDone = true;
        BMessage done(kEditorDone);
        done.AddInt32("serial", fSerial);
        done.AddBool("keep", keep);
        done.AddBool("refocus", refocus);
        BMessenger(fTable).SendMessage(&done);
    }

    ShojiTableControl*  fTable;
    int32               fSerial;
    bool                fDone;
};


// #pragma mark - the rows


/** A row of the list: draws the cells, each as the control that it stands for but that cannot be used. */
class ShojiTableRowItem : public BListItem {
public:
    ShojiTableRowItem(ShojiTableControl* table) : BListItem(), fTable(table) {}

    virtual void Update(BView* owner, const BFont* font)
    {
        BListItem::Update(owner, font);
        SetHeight(RowHeight());
    }

    virtual void DrawItem(BView* owner, BRect frame, bool complete)
    {
        const rgb_color background = IsSelected() ? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : ui_color(B_LIST_BACKGROUND_COLOR);
        const rgb_color text = IsSelected() ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR) : ui_color(B_LIST_ITEM_TEXT_COLOR);
        owner->SetHighColor(background);
        owner->FillRect(frame);

        // (the index is where the row is in the list: the rows are the items, in order)
        const BListView* list = dynamic_cast<BListView*>(owner);
        const int32 index = list != NULL ? list->IndexOf(this) : -1;
        if (index < 0 || index >= fTable->CountRows())
            return;
        const ShojiTableControl::Row& row = fTable->fRows[index];
        const std::vector<float> edges = fTable->ColumnEdges(owner->Bounds().Width() + 1);

        font_height height;
        owner->GetFontHeight(&height);
        const float baseline = frame.top + (frame.Height() - (height.ascent + height.descent)) / 2 + height.ascent;

        for (size_t c = 0; c < row.size() && c < fTable->fColumns.size(); c++) {
            const ShojiTableControl::Cell& cell = row[c];
            const bool usable = fTable->_CanChange(index, (int32)c);
            BRect area(frame.left + edges[c], frame.top, frame.left + edges[c + 1] - 1, frame.bottom);

            switch (cell.kind) {
                case ShojiTableControl::kBool:
                {
                    const float size = ceilf(height.ascent + height.descent);
                    BRect box(0, 0, size, size);
                    const float left = fTable->IsCentered((int32)c) ? floorf((area.Width() - size) / 2) : kPadding - 2;
                    box.OffsetTo(area.left + left, frame.top + floorf((frame.Height() - size) / 2));
                    uint32 flags = 0;
                    if (cell.text == "true")
                        flags |= BControlLook::B_ACTIVATED;
                    if (!usable)
                        flags |= BControlLook::B_DISABLED;
                    be_control_look->DrawCheckBox(owner, box, area, background, flags);
                    break;
                }
                case ShojiTableControl::kChoice:
                {
                    BRect field = area.InsetByCopy(3, 2);
                    const float room = field.Width() - kPadding - 24;       // (DrawMenuFieldBackground() takes its frame off `field`)
                    uint32 flags = usable ? 0 : BControlLook::B_DISABLED;
                    if (fTable->fOpenRow == index && fTable->fOpenColumn == (int32)c)
                        flags |= BControlLook::B_ACTIVATED;
                    const rgb_color base = ui_color(B_CONTROL_BACKGROUND_COLOR);
                    be_control_look->DrawMenuFieldBackground(owner, field, area, base, true, flags);
                    std::string shown = cell.text;
                    for (const ShojiTableControl::Choice& choice : cell.choices) {
                        if (choice.value == cell.text)
                            shown = choice.label;
                    }
                    BString label(shown.c_str());
                    owner->TruncateString(&label, B_TRUNCATE_END, room);
                    const rgb_color ink = ui_color(B_CONTROL_TEXT_COLOR);
                    owner->SetHighColor(usable ? ink : tint_color(ink, B_LIGHTEN_2_TINT));
                    owner->SetLowColor(base);
                    owner->DrawString(label.String(), BPoint(field.left + kPadding, baseline));
                    break;
                }
                default:
                {
                    BString label(cell.text.c_str());
                    owner->TruncateString(&label, B_TRUNCATE_END, area.Width() - kPadding * 2);
                    owner->SetHighColor(usable || !cell.editable ? text : tint_color(text, B_LIGHTEN_2_TINT));
                    owner->SetLowColor(background);
                    owner->DrawString(label.String(), BPoint(area.left + kPadding, baseline));
                    break;
                }
            }

            // the cell that the keys use
            if (IsSelected() && fTable->fColumn == (int32)c && owner->IsFocus() && fTable->_CanChange(index, (int32)c)) {
                owner->SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
                owner->StrokeRect(area.InsetByCopy(1, 1));
            }
        }
        owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
    }

private:
    ShojiTableControl*  fTable;
};


/** The list: a click uses the cell that it is on, and the arrows go from cell to cell. A table that can be reordered has its rows dragged. */
class ShojiTableList : public BListView {
public:
    ShojiTableList(ShojiTableControl* table) : BListView("table rows", B_SINGLE_SELECTION_LIST), fTable(table) {}

    virtual void MouseDown(BPoint where)
    {
        fTable->_EndEdit(true, false);
        const int32 index = IndexOf(where);
        if (index < 0) {
            BListView::MouseDown(where);
            return;
        }
        MakeFocus(true);
        if (!IsItemSelected(index))
            Select(index);
        const int32 column = fTable->ColumnAt(where.x, Bounds().Width() + 1);
        if (fTable->IsReorderable() && fTable->fEditable) {
            // (a click is used when the button is let go, a move with the button down is a drag of the row)
            fPressRow = index;
            fPressColumn = column;
            fPressPoint = where;
            SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
            return;
        }
        if (column >= 0)
            fTable->_ClickCell(index, column);
    }

    virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
    {
        if (dragMessage != NULL && _IsRowDrag(dragMessage)) {
            // (a row is over the list: the line shows where it would go, and the list scrolls when the row is near its end)
            if (transit == B_EXITED_VIEW) {
                _ShowInsertion(-1);
            } else {
                _ShowInsertion(_InsertionAt(where.y));
                _ScrollNear(where.y);
            }
            return;
        }
        if (fPressRow >= 0) {
            BPoint moved = where - fPressPoint;
            if (fabsf(moved.x) + fabsf(moved.y) > 4) {
                const int32 row = fPressRow;
                fPressRow = -1;
                _BeginDrag(row);
            }
            return;
        }
        BListView::MouseMoved(where, transit, dragMessage);
    }

    virtual void MouseUp(BPoint where)
    {
        if (fPressRow >= 0) {
            const int32 row = fPressRow;
            const int32 column = fPressColumn;
            fPressRow = -1;
            if (IndexOf(where) == row && fTable->ColumnAt(where.x, Bounds().Width() + 1) == column && column >= 0)
                fTable->_ClickCell(row, column);
            return;
        }
        BListView::MouseUp(where);
    }

    virtual void MessageReceived(BMessage* message)
    {
        if (message->what == kRowDragMessage && message->WasDropped() && _IsRowDrag(message)) {
            int32 row = -1;
            if (message->FindInt32("shoji:row", &row) == B_OK) {
                const BPoint where = ConvertFromScreen(message->DropPoint());
                const int32 before = _InsertionAt(where.y);
                _ShowInsertion(-1);
                fTable->MoveRow(row, before);
            }
            return;
        }
        BListView::MessageReceived(message);
    }

    virtual void Draw(BRect updateRect)
    {
        BListView::Draw(updateRect);
        if (fInsertion >= 0) {
            SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
            const float y = fInsertion * RowHeight();
            FillRect(BRect(Bounds().left, y - 1, Bounds().right, y));
        }
    }

    virtual void KeyDown(const char* bytes, int32 numBytes)
    {
        if (numBytes == 1 && fTable->IsReorderable() && fTable->fEditable && (modifiers() & B_CONTROL_KEY) != 0
                && (bytes[0] == B_UP_ARROW || bytes[0] == B_DOWN_ARROW)) {
            // (Ctrl and an arrow: the row goes up or down by one)
            const int32 row = CurrentSelection();
            if (row >= 0)
                fTable->MoveRow(row, bytes[0] == B_UP_ARROW ? row - 1 : row + 2);
            return;
        }
        if (numBytes == 1) {
            switch (bytes[0]) {
                case B_LEFT_ARROW:
                    fTable->_MoveColumn(-1);
                    return;
                case B_RIGHT_ARROW:
                    fTable->_MoveColumn(1);
                    return;
                case B_SPACE:
                case B_RETURN:
                    fTable->_UseCurrentCell();
                    return;
            }
        }
        BListView::KeyDown(bytes, numBytes);
    }

    virtual void SelectionChanged()
    {
        BListView::SelectionChanged();
        fTable->_FixColumn();
    }

    virtual void MakeFocus(bool focus = true)
    {
        BListView::MakeFocus(focus);
        const int32 selected = CurrentSelection();
        if (selected >= 0)
            InvalidateItem(selected);
    }

    virtual bool InitiateDrag(BPoint, int32, bool) { return false; }

private:
    bool _IsRowDrag(const BMessage* message) const
    {
        return message->what == kRowDragMessage && strcmp(message->GetString("shoji:table", ""), fTable->Name()) == 0;
    }

    /** the place between rows (0 before the first, the number of rows after the last) that is nearest to a height */
    int32 _InsertionAt(float y) const
    {
        const int32 place = (int32)floorf((y + RowHeight() / 2) / RowHeight());
        return std::max<int32>(0, std::min(place, CountItems()));
    }

    void _ShowInsertion(int32 place)
    {
        if (place == fInsertion)
            return;
        fInsertion = place;
        Invalidate();
    }

    void _ScrollNear(float y)
    {
        BScrollBar* bar = ScrollBar(B_VERTICAL);
        if (bar == NULL)
            return;
        const BRect bounds = Bounds();
        if (y < bounds.top + RowHeight() * 0.6f)
            bar->SetValue(bar->Value() - RowHeight() / 2);
        else if (y > bounds.bottom - RowHeight() * 0.6f)
            bar->SetValue(bar->Value() + RowHeight() / 2);
    }

    /** the row is dragged: a message that says which (the table and the row, and the text of the first cell for a drop in a text field), with the outline of the row */
    void _BeginDrag(int32 row)
    {
        BMessage drag(kRowDragMessage);
        drag.AddString("shoji:table", fTable->Name());
        drag.AddInt32("shoji:row", row);
        if (row >= 0 && row < fTable->CountRows() && !fTable->fRows[row].empty())
            drag.AddString("text/plain", fTable->fRows[row][0].text.c_str());
        DragMessage(&drag, ItemFrame(row), this);
    }

    ShojiTableControl*  fTable;
    int32               fPressRow = -1;
    int32               fPressColumn = -1;
    BPoint              fPressPoint;
    int32               fInsertion = -1;
};


/** The titles of the columns. */
class ShojiTableHeader : public BView {
public:
    ShojiTableHeader(ShojiTableControl* table) : BView("table header", B_WILL_DRAW), fTable(table)
    {
        SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        SetExplicitMinSize(BSize(B_SIZE_UNSET, RowHeight()));
        SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, RowHeight()));
        SetExplicitPreferredSize(BSize(B_SIZE_UNSET, RowHeight()));
    }

    virtual void Draw(BRect updateRect)
    {
        const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
        BRect rect = Bounds();

        // (the cells begin where the list does: the scroll view has a border)
        const float origin = ConvertFromScreen(fTable->fList->ConvertToScreen(BPoint(0, 0))).x;
        const std::vector<float> edges = fTable->ColumnEdges(fTable->fList->Bounds().Width() + 1);
        const size_t count = std::min(fTable->fColumns.size(), edges.size() - 1);

        // a title for each column in a bar of its own, as the header of a column list view has them: a divider after each (the last reaches to the end of the bar)
        for (size_t c = 0; c < count; c++) {
            BRect cell = rect;
            cell.left = c == 0 ? rect.left : origin + edges[c];
            cell.right = c + 1 == count ? rect.right : origin + edges[c + 1] - 1;
            uint32 borders = BControlLook::B_TOP_BORDER | BControlLook::B_BOTTOM_BORDER | BControlLook::B_RIGHT_BORDER;
            if (c == 0)
                borders |= BControlLook::B_LEFT_BORDER;
            be_control_look->DrawButtonBackground(this, cell, updateRect, base, 0, borders);
        }
        if (count == 0)
            be_control_look->DrawButtonBackground(this, rect, updateRect, base, 0, BControlLook::B_ALL_BORDERS);

        font_height height;
        GetFontHeight(&height);
        const float baseline = rect.top + (rect.Height() - (height.ascent + height.descent)) / 2 + height.ascent;
        SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
        SetLowColor(base);
        for (size_t c = 0; c < count; c++) {
            BString label(fTable->fColumns[c].c_str());
            const float width = edges[c + 1] - edges[c];
            TruncateString(&label, B_TRUNCATE_END, width - kPadding);
            // (a column of check boxes has its title over them)
            const bool centered = fTable->IsCentered((int32)c);
            const float x = centered ? origin + edges[c] + std::max(2.0f, floorf((width - StringWidth(label.String())) / 2))
                : origin + edges[c] + kPadding;
            DrawString(label.String(), BPoint(x, baseline));
        }
    }

    // a divider can be dragged: the column before it gets the width
    virtual void MouseDown(BPoint where)
    {
        fDragging = _DividerAt(where);
        if (fDragging < 0)
            return;
        fTable->_EndEdit(true, false);
        const std::vector<float> edges = fTable->ColumnEdges(fTable->fList->Bounds().Width() + 1);
        fStartX = where.x;
        fStartWidth = edges[fDragging + 1] - edges[fDragging];
        SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
    }

    virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
    {
        if (fDragging >= 0) {
            fTable->ResizeColumn(fDragging, fStartWidth + (where.x - fStartX));
            return;
        }
        const bool over = transit != B_EXITED_VIEW && _DividerAt(where) >= 0;
        if (over != fOverDivider) {
            fOverDivider = over;
            if (over) {
                BCursor cursor(B_CURSOR_ID_RESIZE_EAST_WEST);
                SetViewCursor(&cursor);
            } else {
                SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
            }
        }
    }

    virtual void MouseUp(BPoint)
    {
        fDragging = -1;
    }

private:
    int32 _DividerAt(BPoint where) const
    {
        const float origin = ConvertFromScreen(fTable->fList->ConvertToScreen(BPoint(0, 0))).x;
        return fTable->DividerAt(where.x - origin, fTable->fList->Bounds().Width() + 1);
    }

    ShojiTableControl*  fTable;
    int32               fDragging = -1;
    float               fStartX = 0;
    float               fStartWidth = 0;
    bool                fOverDivider = false;
};


// #pragma mark - ShojiTableControl


ShojiTableControl::ShojiTableControl(const char* name)
    :
    BGroupView(name, B_VERTICAL, 0),
    fFlexible(-1),
    fColumn(0),
    fMessage(NULL),
    fEditable(true),
    fReorderable(false),
    fHeader(NULL),
    fList(NULL),
    fScroll(NULL),
    fEditor(NULL),
    fEditRow(-1),
    fEditColumn(-1),
    fEditSerial(0),
    fOpenRow(-1),
    fOpenColumn(-1)
{
    fHeader = new ShojiTableHeader(this);
    fList = new ShojiTableList(this);
    fScroll = new BScrollView("table scroll", fList, 0, false, true, B_FANCY_BORDER);
    GroupLayout()->AddView(fHeader);
    GroupLayout()->AddView(fScroll);
    SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
}


ShojiTableControl::~ShojiTableControl()
{
    delete fMessage;
}


void ShojiTableControl::SetColumns(const std::vector<std::string>& labels)
{
    fColumns = labels;
    _Measure();
}


void ShojiTableControl::SetRows(const std::vector<Row>& rows)
{
    _EndEdit(false, false);

    const int32 selected = fList->CurrentSelection();
    BScrollBar* bar = fScroll->ScrollBar(B_VERTICAL);
    const float scrolled = bar != NULL ? bar->Value() : 0;
    fRows = rows;
    _Measure();

    while (BListItem* item = fList->RemoveItem((int32)0))
        delete item;
    for (size_t i = 0; i < fRows.size(); i++)
        fList->AddItem(new ShojiTableRowItem(this));
    if (selected >= 0 && selected < fList->CountItems())
        fList->Select(selected);
    _FixColumn();
    if (bar != NULL)
        bar->SetValue(scrolled);
    fHeader->Invalidate();
    fList->Invalidate();

    // as high as its rows need, up to a number of them, then it scrolls
    const int32 shown = std::max(kMinimumRows, std::min((int32)fRows.size(), kVisibleRows));
    float width = 0;
    for (float w : fWidths)
        width += w;
    const float height = shown * RowHeight() + 4;
    fScroll->SetExplicitMinSize(BSize(width + B_V_SCROLL_BAR_WIDTH + 4, height));
    fScroll->SetExplicitPreferredSize(BSize(width + B_V_SCROLL_BAR_WIDTH + 4, height));
    fScroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, height));
    InvalidateLayout();
}


void ShojiTableControl::SetMessage(BMessage* message)
{
    delete fMessage;
    fMessage = message;
}


void ShojiTableControl::SetTarget(BHandler* target)
{
    fTarget = BMessenger(target);
}


void ShojiTableControl::SetEditable(bool editable)
{
    if (!editable)
        _EndEdit(false, false);
    fEditable = editable;
    fList->Invalidate();
}


const ShojiTableControl::Cell* ShojiTableControl::CellAt(int32 row, int32 column) const
{
    if (row < 0 || row >= (int32)fRows.size() || column < 0 || column >= (int32)fRows[row].size())
        return NULL;
    return &fRows[row][column];
}


bool ShojiTableControl::FindCell(const std::string& path, int32* row, int32* column) const
{
    for (size_t r = 0; r < fRows.size(); r++) {
        for (size_t c = 0; c < fRows[r].size(); c++) {
            if (!path.empty() && fRows[r][c].path == path) {
                *row = (int32)r;
                *column = (int32)c;
                return true;
            }
        }
    }
    return false;
}


int32 ShojiTableControl::SelectedRow() const
{
    return fList->CurrentSelection();
}


void ShojiTableControl::SelectRow(int32 row)
{
    if (row >= 0 && row < fList->CountItems())
        fList->Select(row);
}


bool ShojiTableControl::_CanChange(int32 row, int32 column) const
{
    const Cell* cell = CellAt(row, column);
    return fEditable && cell != NULL && cell->editable && cell->enabled;
}


bool ShojiTableControl::Activate(int32 row, int32 column, const std::string& value)
{
    if (!_CanChange(row, column))
        return false;
    const Cell& cell = fRows[row][column];
    switch (cell.kind) {
        case kBool:
        {
            const std::string wanted = value.empty() ? (cell.text == "true" ? "false" : "true") : value;
            if (wanted != "true" && wanted != "false")
                return false;
            _Tell(row, column, wanted);
            return true;
        }
        case kChoice:
        {
            const bool known = std::any_of(cell.choices.begin(), cell.choices.end(), [&value](const Choice& c) { return c.value == value; });
            if (!known)
                return false;
            if (value != cell.text)
                _Tell(row, column, value);
            return true;
        }
        default:
            if (value != cell.text)
                _Tell(row, column, value);
            return true;
    }
}


void ShojiTableControl::ChooseFor(int32 row, int32 column)
{
    if (!_CanChange(row, column) || fRows[row][column].kind != kChoice)
        return;
    const Cell cell = fRows[row][column];      // (a copy: the rows can be given again while the menu is open)

    BPopUpMenu menu("table choice", false, false);
    for (const Choice& choice : cell.choices) {
        BMenuItem* item = new BMenuItem(choice.label.c_str(), NULL);
        item->SetMarked(choice.value == cell.text);
        menu.AddItem(item);
    }

    // the cell shows that its menu is open
    const BRect frame = _CellFrame(row, column);
    fOpenRow = row;
    fOpenColumn = column;
    fList->Invalidate(frame);
    if (fList->Window() != NULL)
        fList->Window()->UpdateIfNeeded();

    BMenuItem* picked = menu.Go(fList->ConvertToScreen(BPoint(frame.left, frame.bottom + 1)), false, true);
    fOpenRow = fOpenColumn = -1;
    fList->Invalidate(frame);
    if (picked == NULL)
        return;
    const int32 chosen = menu.IndexOf(picked);
    if (chosen >= 0 && chosen < (int32)cell.choices.size() && cell.choices[chosen].value != cell.text)
        _Tell(row, column, cell.choices[chosen].value);
}


bool ShojiTableControl::BeginEdit(int32 row, int32 column)
{
    if (!_CanChange(row, column) || fRows[row][column].kind != kText)
        return false;
    _EndEdit(true, false);

    const Cell& cell = fRows[row][column];
    fEditSerial++;
    fEditor = new ShojiTableEditor(this, fEditSerial);
    const BRect frame = _CellFrame(row, column);
    font_height height;
    be_plain_font->GetHeight(&height);
    const float top = floorf((frame.Height() - (height.ascent + height.descent)) / 2);
    fEditor->SetInsets(kPadding - 1, top, 3, 0);
    fEditor->MoveTo(frame.LeftTop());
    fEditor->ResizeTo(frame.Width(), frame.Height());
    fEditor->SetText(cell.text.c_str());
    fList->AddChild(fEditor);
    fEditRow = row;
    fEditColumn = column;
    fColumn = column;
    fEditor->SelectAll();
    fEditor->MakeFocus(true);
    return true;
}


bool ShojiTableControl::PendingEdit(std::string* path, std::string* text) const
{
    if (fEditRow < 0 || fEditor == NULL || fEditRow >= (int32)fRows.size() || fEditColumn >= (int32)fRows[fEditRow].size())
        return false;
    const Cell& cell = fRows[fEditRow][fEditColumn];
    if (cell.text == fEditor->Text())
        return false;
    *path = cell.path;
    *text = fEditor->Text();
    return true;
}


void ShojiTableControl::SetEditorText(const char* text)
{
    if (fEditor != NULL)
        fEditor->SetText(text);
}


void ShojiTableControl::EndEdit(bool keep)
{
    _EndEdit(keep, false);
}


void ShojiTableControl::_EndEdit(bool keep, bool refocus)
{
    if (fEditRow < 0)
        return;
    const int32 row = fEditRow;
    const int32 column = fEditColumn;
    fEditRow = fEditColumn = -1;
    fEditSerial++;        // (what the field told while it was taken away is old)

    std::string text;
    if (fEditor != NULL) {
        text = fEditor->Text();
        fList->RemoveChild(fEditor);
        delete fEditor;
        fEditor = NULL;
    }
    if (refocus)
        fList->MakeFocus(true);
    if (keep && row < (int32)fRows.size() && column < (int32)fRows[row].size() && text != fRows[row][column].text)
        _Tell(row, column, text);
}


void ShojiTableControl::_ClickCell(int32 row, int32 column)
{
    fColumn = column;
    fList->InvalidateItem(row);
    if (!_CanChange(row, column))
        return;
    switch (fRows[row][column].kind) {
        case kBool:
            Activate(row, column);
            break;
        case kChoice:
            ChooseFor(row, column);
            break;
        default:
            BeginEdit(row, column);
            break;
    }
}


void ShojiTableControl::_UseCurrentCell()
{
    const int32 row = fList->CurrentSelection();
    if (row >= 0)
        _ClickCell(row, fColumn);
}


void ShojiTableControl::_MoveColumn(int32 by)
{
    const int32 row = fList->CurrentSelection();
    if (row < 0)
        return;
    for (int32 column = fColumn + by; column >= 0 && column < (int32)fColumns.size(); column += by) {
        if (_CanChange(row, column)) {
            fColumn = column;
            fList->InvalidateItem(row);
            return;
        }
    }
}


// the cell that the keys use is one that can be used, else the first that can
void ShojiTableControl::_FixColumn()
{
    const int32 row = fList->CurrentSelection();
    if (row < 0 || row >= (int32)fRows.size())
        return;
    if (_CanChange(row, fColumn))
        return;
    for (int32 column = 0; column < (int32)fRows[row].size(); column++) {
        if (_CanChange(row, column)) {
            fColumn = column;
            return;
        }
    }
}


void ShojiTableControl::_Tell(int32 row, int32 column, const std::string& value)
{
    if (!fTarget.IsValid())
        return;
    BMessage message(fMessage != NULL ? *fMessage : BMessage(kChanged));
    message.AddString("path", fRows[row][column].path.c_str());
    message.AddString("value", value.c_str());
    fTarget.SendMessage(&message);
}


void ShojiTableControl::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kEditorDone:
            // (a field that was taken away already is old news)
            if (message->GetInt32("serial", -1) == fEditSerial)
                _EndEdit(message->GetBool("keep", true), message->GetBool("refocus", false));
            break;
        default:
            BGroupView::MessageReceived(message);
            break;
    }
}


bool ShojiTableControl::MoveRow(int32 row, int32 before)
{
    const int32 count = (int32)fRows.size();
    if (!fReorderable || !fEditable || row < 0 || row >= count)
        return false;
    before = std::max<int32>(0, std::min(before, count));
    // (`before` is a place among the rows as they are now: after the row is taken out, the places after it are one less)
    const int32 to = before > row ? before - 1 : before;
    if (to == row)
        return false;

    _EndEdit(false, false);
    Row moved = fRows[row];
    fRows.erase(fRows.begin() + row);
    fRows.insert(fRows.begin() + to, moved);
    fList->MoveItem(row, to);
    fList->Select(to);
    fHeader->Invalidate();
    fList->Invalidate();

    // the target is told, with the message of the table: the owner moves the row in its data and gives the rows again
    if (fTarget.IsValid()) {
        BMessage message(fMessage != NULL ? *fMessage : BMessage(kChanged));
        message.AddString("table", Name());
        message.AddInt32("reorder_from", row);
        message.AddInt32("reorder_to", to);
        fTarget.SendMessage(&message);
    }
    return true;
}


BRect ShojiTableControl::_CellFrame(int32 row, int32 column) const
{
    const BRect item = fList->ItemFrame(row);
    const std::vector<float> edges = ColumnEdges(fList->Bounds().Width() + 1);
    if (column < 0 || column + 1 >= (int32)edges.size())
        return item;
    return BRect(edges[column], item.top, edges[column + 1] - 1, item.bottom);
}


// A column has the width that the user gave it, else what it needs. What is more than the columns need goes to the column that is edited as text (else the first of only text, else the
// last), what is less is taken from it, to half: so a column that is made wider takes it from that one. A column that has a width of the user does not take part.
static float
BaseWidth(const std::vector<float>& measured, const std::vector<float>& user, size_t c)
{
    if (c < user.size() && user[c] > 0)
        return user[c];
    return c < measured.size() ? measured[c] : 0;
}


std::vector<float> ShojiTableControl::ColumnEdges(float width) const
{
    std::vector<float> edges(fColumns.size() + 1, 0);
    float needed = 0;
    for (size_t c = 0; c < fColumns.size(); c++)
        needed += BaseWidth(fWidths, fUserWidths, c);
    const float extra = width - needed;
    const bool flexible = fFlexible >= 0 && (size_t)fFlexible < fUserWidths.size() ? fUserWidths[fFlexible] <= 0 : true;
    float x = 0;
    for (size_t c = 0; c < fColumns.size(); c++) {
        float w = BaseWidth(fWidths, fUserWidths, c);
        if ((int32)c == fFlexible && flexible)
            w = std::max(w * 0.5f, w + extra);
        edges[c] = x;
        x += w;
    }
    edges[fColumns.size()] = x;
    return edges;
}


int32 ShojiTableControl::DividerAt(float x, float width) const
{
    const std::vector<float> edges = ColumnEdges(width);
    // (between two columns; the end of the last one is the end of the bar)
    for (size_t c = 0; c + 1 < fColumns.size(); c++) {
        if (fabsf(x - edges[c + 1]) <= 3)
            return (int32)c;
    }
    return -1;
}


bool ShojiTableControl::ResizeColumn(int32 column, float width)
{
    if (column < 0 || column >= (int32)fColumns.size())
        return false;
    if (fUserWidths.size() != fColumns.size())
        fUserWidths.assign(fColumns.size(), 0);
    const float available = fList->Bounds().Width() + 1;
    const std::vector<float> edges = ColumnEdges(available);
    // (at least a little, and not more than leaves the columns after it what they need at least: the flexible one half)
    const float smallest = kPadding * 2 + be_plain_font->StringWidth("M") * 2;
    float after = 0;
    for (size_t c = column + 1; c < fColumns.size(); c++) {
        const float base = BaseWidth(fWidths, fUserWidths, c);
        after += (int32)c == fFlexible && fUserWidths[c] <= 0 ? base * 0.5f : base;
    }
    const float largest = std::max(smallest, available - edges[column] - after);
    fUserWidths[column] = floorf(std::max(smallest, std::min(width, largest)));
    fHeader->Invalidate();
    fList->Invalidate();
    return true;
}


bool ShojiTableControl::IsCentered(int32 column) const
{
    return column >= 0 && column < (int32)fCentered.size() && fCentered[column];
}


int32 ShojiTableControl::ColumnAt(float x, float width) const
{
    const std::vector<float> edges = ColumnEdges(width);
    for (size_t c = 0; c < fColumns.size(); c++) {
        if (x >= edges[c] && x < edges[c + 1])
            return (int32)c;
    }
    return -1;
}


void ShojiTableControl::_Measure()
{
    const float em = be_plain_font->StringWidth("M");
    font_height height;
    be_plain_font->GetHeight(&height);

    fWidths.assign(fColumns.size(), 0);
    if (fUserWidths.size() != fColumns.size())
        fUserWidths.assign(fColumns.size(), 0);
    fCentered.assign(fColumns.size(), false);
    fFlexible = -1;
    int32 firstAllText = -1;
    for (size_t c = 0; c < fColumns.size(); c++) {
        float widest = be_plain_font->StringWidth(fColumns[c].c_str()) + kPadding * 2;
        bool allText = true;
        bool allBool = !fRows.empty();
        for (const Row& row : fRows) {
            if (c >= row.size())
                continue;
            const Cell& cell = row[c];
            if (cell.kind != kBool)
                allBool = false;
            switch (cell.kind) {
                case kBool:
                    allText = false;
                    widest = std::max(widest, ceilf(height.ascent + height.descent) + kPadding * 2);
                    break;
                case kChoice:
                {
                    allText = false;
                    float choices = be_plain_font->StringWidth(cell.text.c_str());
                    for (const Choice& choice : cell.choices)
                        choices = std::max(choices, be_plain_font->StringWidth(choice.label.c_str()));
                    widest = std::max(widest, choices + 24 + kPadding * 2);
                    break;
                }
                default:
                    widest = std::max(widest, std::min(be_plain_font->StringWidth(cell.text.c_str()), em * 30) + kPadding * 2 + 2);
                    if (cell.editable) {
                        widest = std::max(widest, em * 12 + kPadding * 2);
                        if (fFlexible < 0)
                            fFlexible = (int32)c;
                    }
                    break;
            }
        }
        if (allText && firstAllText < 0)
            firstAllText = (int32)c;
        fCentered[c] = allBool;
        fWidths[c] = ceilf(widest);
    }
    if (fFlexible < 0)
        fFlexible = firstAllText >= 0 ? firstAllText : (int32)fColumns.size() - 1;
}
