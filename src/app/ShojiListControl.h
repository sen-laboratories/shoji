/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Alignment.h>
#include <Control.h>
#include <String.h>

class BButton;
class BListView;
class BTextControl;

/**
 * The value of an attribute that is a list (the attribute info of its MIME type says so, see sento SenAttrInfo.h) as a list of values instead of one
 * string with separators: the values in a list, the selected one in a field to edit, Add and Remove. The text of the attribute is split and joined
 * with the rules of SenList.h, so a value can hold the separator (it is quoted when it is stored).
 *
 * It tells its window what a text control does: the modification message when the list changes (typing, Add, Remove), the message of the control (SetMessage)
 * when a change is complete (Enter, the field is left, a value is added or removed).
 *
 * ListText() is the text that was set as long as the list was not changed: a value that somebody wrote as `a; b` is not rewritten as `a;b` by looking at it.
 */
class ShojiListControl : public BControl {
public:
                        /** @param label shown on the left, the width of @p divider (as the label of a text control), NULL for none; it is in the middle of the height of the
                         *  list, not of the field and the buttons below it */
                        ShojiListControl(const char* name, char separator, const char* label = NULL, float divider = 0, alignment labelAlignment = B_ALIGN_LEFT);
    virtual            ~ShojiListControl();

    /** The values of the list from a text (the attribute) in the format of the list; nothing is edited yet. */
    void                SetListText(const char* text);
    /** The text of the attribute: the one that was set if nothing was changed, else the values joined. */
    BString             ListText() const;

    char                Separator() const { return fSeparator; }

    /** sent (a copy, with the control as source) to the target when the list changes */
    void                SetModificationMessage(BMessage* message);

    virtual void        AttachedToWindow();
    virtual void        MessageReceived(BMessage* message);
    virtual void        SetEnabled(bool enabled);

private:
    enum {
        kSelected   = 'LCsl',
        kAdd        = 'LCad',
        kRemove     = 'LCrm',
        kItemEdited = 'LCed',
        kItemDone   = 'LCdn'
    };

    void                _UpdateButtons();
    void                _Changed();

    BListView*          fList;
    BTextControl*       fItem;
    BButton*            fAdd;
    BButton*            fRemove;
    char                fSeparator;
    bool                fEdited;
    BString             fOriginal;
    BMessage*           fModificationMessage;
};
