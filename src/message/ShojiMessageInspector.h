/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <GroupView.h>
#include <Message.h>
#include <Messenger.h>
#include <String.h>

#include <set>
#include <string>

class BOutlineListView;
class BScrollView;
class BStringView;
class BTextView;

namespace shoji {
class InspectorList;
class InspectorHeader;
class ValueEditor;
}

/**
 * The inspector for any BMessage (docs/spec-settings-and-message-template.md in sento, section 5, variant B): every value of the message in one outline, a row for each, with
 * its name, its type and its value. A nested message is a row with the rows of its fields below it (it can be collapsed), an array has a row for each value. All types are shown: those
 * that no text stands for (raw data, references, messengers, pointers) with what can be said of them. What a text can stand for (SenMessagePath.h: strings, numbers, bool,
 * time, point, rectangle, colour) is edited in the row: Enter or a double click puts a field over the value, Enter keeps it, Escape takes it back, a bool is toggled; a row can be removed
 * (Delete). The context menu copies the path key or the value.
 *
 * It does what ShojiMessageView does with the data: a working copy, kChanged (path, value, settings) and kRefused to the target (SenSettingsForm.h), nothing stored here. Where the
 * form shows a message as a person would set it, this shows it as it is, which is what a developer (or a message file of something else) needs.
 */
class ShojiMessageInspector : public BGroupView {
public:
    // what the list and the editor tell the inspector
    enum {
        kEdit           = 'IEed',
        kEditorKept     = 'IEok',
        kEditorCancelled= 'IEcn',
        kRemove         = 'IErm',
        kCopyPath       = 'IEcp',
        kCopyValue      = 'IEcv',
        kSelected       = 'IEse'
    };

                            ShojiMessageInspector(const char* name = "message inspector");
    virtual                ~ShojiMessageInspector();

    /** The message to show; nothing is told to the target. */
    status_t                SetData(const BMessage& message);
    const BMessage&         Settings() const { return fWorking; }

    void                    SetTarget(BMessenger target) { fTarget = target; }
    void                    SetEditable(bool editable);
    bool                    IsEditable() const { return fEditable; }
    /** keeps the value that is being edited */
    void                    CommitPending();

    /** Sets the value that a path names, as the editor does (the type is kept, a text that does not fit is refused: B_BAD_VALUE). */
    status_t                SetValue(const std::string& path, const std::string& text);
    /** Removes a value (B_NOT_ALLOWED if the inspector is not editable). */
    status_t                RemoveValue(const std::string& path);

    /** the rows, in the order that they are shown (not those below a collapsed row) */
    int32                   CountRows() const;
    /** the number of rows, also those that are collapsed */
    int32                   CountAllRows() const;
    /** the row of a path key among all rows; -1 if there is none */
    int32                   RowOf(const std::string& path) const;
    BString                 RowName(int32 row) const;
    BString                 RowType(int32 row) const;
    BString                 RowValue(int32 row) const;
    std::string             RowPath(int32 row) const;
    bool                    RowEditable(int32 row) const;
    bool                    SelectPath(const std::string& path);
    /** the editor, while one is shown (for tests); NULL if not */
    BTextView*              Editor() const;

    virtual void            AttachedToWindow();
    virtual void            MessageReceived(BMessage* message);

private:
    void                    _Rebuild();
    void                    _EditSelected();
    void                    _EndEdit(bool keep);
    void                    _ShowStatus();
    void                    _Changed(const std::string& path, const std::string& value);
    void                    _Copy(const std::string& text);
    int32                   _SelectedRow() const;

    BMessage                fWorking;
    BMessenger              fTarget;
    bool                    fEditable;
    std::set<std::string>   fCollapsed;
    shoji::InspectorList*   fList;
    shoji::InspectorHeader* fHeader;
    BScrollView*            fScroll;
    BStringView*            fStatus;
    shoji::ValueEditor*     fEditor;
    int32                   fEditing;           // the row that is edited, -1: none
};
