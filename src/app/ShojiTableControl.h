/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <GroupView.h>
#include <Message.h>
#include <Messenger.h>

#include <string>
#include <vector>

class BHandler;
class BMessageRunner;
class BScrollView;
class ShojiTableEditor;
class ShojiTableHeader;
class ShojiTableList;


/** The drag message of a row that is moved with the mouse: `shoji:table` (the name of the table), `shoji:row` (int32, the row), and the text of the first cell as `text/plain`. */
const uint32 kRowDragMessage = 'SThr';

/**
 * A table of values: a row of cells for each thing, a column for each kind of value (the form of a message makes one from an array of messages, or from the fields of a
 * message: `kind = table` and `kind = properties` of the schema, docs/spec-settings-and-message-template.md in sento).
 *
 * **A cell shows its value with a control that cannot be used** (a check box, the look of a menu field, the text) and **is changed in an interactive control that replaces it as
 * soon as it is clicked**: a click on a check box turns it over, a click on a choice opens its menu where the cell is, a click on a text puts a text field over the cell (Enter or
 * leaving the field keeps what was typed, Escape takes it back). The keys do the same on the selected row: the arrows go up and down and from cell to cell, Space and Enter use the
 * cell that has the focus.
 *
 * The table keeps nothing of what is changed. It tells the target (the message of SetMessage(), with `path` (the path key of the cell) and `value` (the new value as text) added)
 * and shows what it is given with SetRows(): whoever owns the data applies the change and gives the rows again, so that a value that was refused goes back.
 *
 * **Reordering** is a flag (SetReorderable): a row is dragged with the mouse (a message drag of the system: kRowDragMessage, so the list takes drops of rows and a line shows where the
 * row will go) or moved with Ctrl and the arrows. A click is used when the button is released, then, so that a click and a drag can be told apart.
 */
class ShojiTableControl : public BGroupView {
public:
    enum Kind {
        kText,
        kBool,
        kChoice
    };

    struct Choice {
        std::string     value;
        std::string     label;
    };

    struct Cell {
        std::string             path;               ///< the path key of the value; what a change is told with
        std::string             text;               ///< the value as the data has it (`true` and `false` for a bool)
        Kind                    kind = kText;
        std::vector<Choice>     choices;            ///< of a choice
        bool                    editable = false;   ///< a cell that only shows (the name of a row) has none
        bool                    enabled = true;     ///< editable now: a cell is disabled while its row does not fit (enabled_when)
        bool                    busy = false;       ///< work is going on for the row: a spinner before the text
    };
    typedef std::vector<Cell> Row;

    /** the message of a change when SetMessage() was not called */
    enum { kChanged = 'TBch' };

                            ShojiTableControl(const char* name);
    virtual                ~ShojiTableControl();

    void                    SetColumns(const std::vector<std::string>& labels);
    /** the rows to show: the selected row and the scrolling stay, an edit that is under way is cancelled */
    void                    SetRows(const std::vector<Row>& rows);
    void                    SetMessage(BMessage* message);
    void                    SetTarget(BHandler* target);
    /** a table that cannot be edited shows the values */
    void                    SetEditable(bool editable);
    bool                    IsEditable() const { return fEditable; }

    int32                   CountRows() const { return (int32)fRows.size(); }
    int32                   CountColumns() const { return (int32)fColumns.size(); }
    const std::string&      ColumnLabel(int32 column) const { return fColumns[column]; }
    const Cell*             CellAt(int32 row, int32 column) const;
    /** the cell of a path key; false if there is none */
    bool                    FindCell(const std::string& path, int32* row, int32* column) const;
    int32                   SelectedRow() const;
    void                    SelectRow(int32 row);
    /** the cell that the keys use */
    int32                   CurrentColumn() const { return fColumn; }

    /** What a click on the cell does, without the mouse (for the keys and the tests): a bool is turned over (or set to `value`, `true` or `false`), a choice is set to
     *  `value`, a text is set to `value`. False if the cell cannot be changed, or `value` is none of the choices. Tells the target; the display changes with the next SetRows(). */
    bool                    Activate(int32 row, int32 column, const std::string& value = std::string());

    /** the text field over a text cell (what a click on it does); false if the cell cannot be edited */
    bool                    BeginEdit(int32 row, int32 column);
    bool                    IsEditing() const { return fEditRow >= 0; }
    /** the path and the text of the cell that is edited, if it differs from the value; false if there is nothing to keep */
    bool                    PendingEdit(std::string* path, std::string* text) const;
    void                    SetEditorText(const char* text);
    /** ends the edit: the text is kept (the target is told if it differs from the value) or taken back */
    void                    EndEdit(bool keep);

    /** the menu of a choice cell at a place on the screen (a click on the cell, or Enter); returns when it is closed */
    void                    ChooseFor(int32 row, int32 column);

    // the columns as they are laid out in a width (the header and the rows use the same)
    std::vector<float>      ColumnEdges(float width) const;
    int32                   ColumnAt(float x, float width) const;
    /** a column of check boxes only has them (and its title) in the middle, any other column has its cells at the left */
    bool                    IsCentered(int32 column) const;

    /** The width of a column that the user made with the mouse on the divider in the header (0: as the table lays it out); the form keeps them when it makes the table again. */
    const std::vector<float>& UserWidths() const { return fUserWidths; }
    void                    SetUserWidths(const std::vector<float>& widths) { fUserWidths = widths; }
    /** sets the width of a column (as the divider of the header does, within what fits); false if the column is not there */
    bool                    ResizeColumn(int32 column, float width);
    /** the divider (between a column and the next) that is at `x` in the coordinates of the list, within a few pixels; -1 if there is none */
    int32                   DividerAt(float x, float width) const;

    /**
     * Whether the rows can be moved: dragged with the mouse (a drag message of the kind kRowDragMessage that carries the table and the row, dropped on the list), or with Ctrl and the arrows up and
     * down. The target gets the message of the table (`SetMessage`) with `reorder_from` and `reorder_to` (int32, the row before and after the move) and `table` (the name of the control).
     */
    void                    SetReorderable(bool reorderable) { fReorderable = reorderable; }
    bool                    IsReorderable() const { return fReorderable; }
    /** moves a row to a place before the row `before` (0 to the number of rows: the end); the rows, the selection and the target follow. False if nothing moves. */
    bool                    MoveRow(int32 row, int32 before);

    virtual void            AttachedToWindow();
    virtual void            MessageReceived(BMessage* message);

private:
    friend class ShojiTableList;
    friend class ShojiTableHeader;
    friend class ShojiTableEditor;
    friend class ShojiTableRowItem;

    void                    _Tell(int32 row, int32 column, const std::string& value);
    void                    _UpdateSpinner();
    int32                   SpinStep() const { return fSpinStep; }
    void                    _Measure();
    BRect                   _CellFrame(int32 row, int32 column) const;
    bool                    _CanChange(int32 row, int32 column) const;
    void                    _EndEdit(bool keep, bool refocus);
    void                    _MoveColumn(int32 by);
    void                    _FixColumn();
    void                    _UseCurrentCell();
    void                    _ClickCell(int32 row, int32 column);

    std::vector<std::string> fColumns;
    std::vector<Row>        fRows;
    std::vector<float>      fWidths;
    BMessageRunner*         fSpinTimer;     ///< turns the spinners of the busy cells (there while a row has one)
    int32                   fSpinStep;
    std::vector<float>      fUserWidths;    ///< what the user dragged the columns to (0: not)
    std::vector<bool>       fCentered;
    int32                   fFlexible;
    int32                   fColumn;
    BMessage*               fMessage;
    BMessenger              fTarget;
    bool                    fEditable;
    bool                    fReorderable;

    ShojiTableHeader*       fHeader;
    ShojiTableList*         fList;
    BScrollView*            fScroll;

    ShojiTableEditor*       fEditor;
    int32                   fEditRow;
    int32                   fEditColumn;
    int32                   fEditSerial;
    int32                   fOpenRow;       ///< the choice cell whose menu is open: drawn as used
    int32                   fOpenColumn;
};
