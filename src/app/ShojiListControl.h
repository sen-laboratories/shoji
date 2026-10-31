/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Alignment.h>
#include <Control.h>
#include <String.h>

class BListView;
class BScrollView;
class BTextView;
class ShojiValueEditor;

/**
 * The value of an attribute that is a list (the attribute info of its MIME type says so, see sento SenAttrInfo.h) as a list of values instead of one
 * string with separators. The values are edited in the list itself, there are no buttons and no field beside it:
 *
 *   - a double click on a value changes it, in the list; a double click on the empty part of the list (below the values) adds one
 *   - Enter on the selected value changes it, Delete (or Backspace) removes it, + (or Insert) adds one
 *   - the context menu (right click) has *Add…* and *Delete*
 *   - Enter or leaving the field keeps what was typed, Escape takes it back; a value that is made empty is removed
 *
 * The text of the attribute is split and joined with the rules of SenList.h, so a value can hold the separator (it is quoted when it is stored).
 *
 * It tells its window what a text control does: the modification message when the list changes (a value was kept or removed), the message of the control (SetMessage)
 * when a change is complete (the same moment: every change of a list is one).
 *
 * ListText() is the text that was set as long as the list was not changed: a value that somebody wrote as `a; b` is not rewritten as `a;b` by looking at it.
 */
class ShojiListControl : public BControl {
public:
                        /** @param label shown on the left, the width of @p divider (as the label of a text control), NULL for none; it is in the middle of the height of the
                         *  list */
                        ShojiListControl(const char* name, char separator, const char* label = NULL, float divider = 0, alignment labelAlignment = B_ALIGN_LEFT);
    virtual            ~ShojiListControl();

    /** The values of the list from a text (the attribute) in the format of the list; nothing is edited yet. */
    void                SetListText(const char* text);
    /** The text of the attribute: the one that was set if nothing was changed, else the values joined. */
    BString             ListText() const;

    char                Separator() const { return fSeparator; }

    /** sent (a copy, with the control as source) to the target when the list changes */
    void                SetModificationMessage(BMessage* message);

    /** the list and its field can send to the control (the mouse and the keys are what they hand to it): for tests */
    bool                IsConnected() const;

    virtual void        AttachedToWindow();
    virtual void        MessageReceived(BMessage* message);
    virtual void        SetEnabled(bool enabled);

    // what the list asks of the control (messages of the list and of the editor, and the menu)
    enum {
        kAddValue       = 'LCad',
        kDeleteValue    = 'LCrm',
        kEditValue      = 'LCed',
        kEditorKept     = 'LCok',
        kEditorCancelled= 'LCcn'
    };

private:
    friend class ShojiValueEditor;

    void                _BeginEdit(int32 index, bool isNew);
    void                _EndEdit(bool keep);
    void                _AddValue();
    void                _DeleteSelected();
    void                _Changed();
    void                _SizeForValues();

    BListView*          fList;
    BScrollView*        fScroll;
    ShojiValueEditor*   fEditor;
    int32               fEditing;       // the index of the value that is edited, -1: none
    bool                fEditingNew;    // it was made for this editing: it goes if nothing is kept
    char                fSeparator;
    bool                fEdited;
    BString             fOriginal;
    BMessage*           fModificationMessage;
};
