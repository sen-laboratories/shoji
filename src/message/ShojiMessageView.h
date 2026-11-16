/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <GroupView.h>
#include <Message.h>
#include <Messenger.h>

#include <map>
#include <string>
#include <vector>

class BButton;
class BTextControl;
class BGridView;
class BOutlineListView;
class BScrollView;
class BSplitView;
class ShojiChangeHandler;
class ShojiTableControl;

/**
 * A form for a BMessage (docs/spec-settings-and-message-template.md in sento, sections 2 to 5 and 7). The controls are made from the data: a check box for a bool, a
 * text field for a number or a string (checked against the type and the range of the schema), a menu for a choice, a list for a field with several values; a nested
 * message is a node of the tree on the left, and the controls of the selected node are on the right. A field of a type that no text stands for is shown, read-only,
 * and kept as it is.
 *
 * A message that the schema gives `kind = table` (an array of messages: a row for each, a column for each field) or `kind = properties` (the fields of the message: a row
 * for each, with the name and the value) is one table (ShojiTableControl) in the pane, not nodes of the tree. A cell shows its value and is edited in a control that
 * replaces it when it is clicked; its path key is that of the value (`programs_02.level`, `nested.mode`).
 *
 * Every control is named with the path key of the value it shows (SenSettingsPath.h), as the controls of the attribute templates are named after the attributes.
 *
 * The view holds a **working copy** of the message. A change that the user makes is checked, put into the working copy and told to the **target** with kChanged
 * (SenSettingsForm.h); a value that does not fit goes back in the control and is told with kRefused. Revert() puts the data back that was set, Defaults() the
 * defaults. The view stores nothing: whoever owns the data does that when it hears of a change.
 *
 * The view does not use the application it is in (be_app, its catalog or its resources): it can be loaded into the process of another program, see the spec.
 */
class ShojiMessageView : public BGroupView {
    friend class ShojiChangeHandler;
public:
                            ShojiMessageView(const char* name = "message view");
    virtual                ~ShojiMessageView();

    /** The data to show. `schema` and `defaults` are optional. Nothing is told to the target: the data is what the owner has. */
    status_t                SetData(const BMessage& settings, const BMessage* schema = NULL, const BMessage* defaults = NULL);
    /** the working copy */
    const BMessage&         Settings() const { return fWorking; }
    /** whether the working copy differs from the data that was set */
    bool                    IsModified() const;
    bool                    HasDefaults() const { return !fDefaults.IsEmpty(); }

    /** where the changes go (kChanged, kRefused); else nowhere */
    void                    SetTarget(BMessenger target);
    /** a form that cannot be edited shows the values */
    void                    SetEditable(bool editable);
    bool                    IsEditable() const { return fEditable; }
    /** the text of the first node of the tree (the fields of the message itself); a message without nested messages shows no tree */
    void                    SetRootLabel(const char* label);

    /** back to the data that was set; kChanged for the whole message */
    void                    Revert();
    /** the working copy becomes the defaults; kChanged for the whole message. B_NOT_SUPPORTED if there are no defaults */
    status_t                Defaults();
    /** keeps what is typed in the fields of the node that is shown (a field is kept on Enter and when it is left; a window that is closed or left calls this) */
    void                    CommitPending();

    /** the control of a value, by its path key (of the node that is shown); the tests and the owner's tests use it */
    BView*                  ControlFor(const char* path);
    /** the path of the node that is shown, "" for the fields of the message itself */
    const std::string&      SelectedNode() const { return fNode; }
    /** show a node of the tree; false if there is no such node */
    bool                    SelectNode(const std::string& path);
    /** the number of nodes in the tree (0: no tree is shown) */
    int32                   CountNodes() const;

    /** The form as a replicant (SenSettingsFormHost.h): the stub of an archive has the class and the signature of Shoji, and `instantiate_object()` in another
     *  program loads the class from the executable of Shoji. Defined in the source file, not here: an inline function is not exported and the load fails. */
    static BArchivable*     Instantiate(BMessage* archive);
    virtual status_t        Archive(BMessage* archive, bool deep = true) const;

    virtual void            AttachedToWindow();
    virtual void            DetachedFromWindow();
    virtual void            MessageReceived(BMessage* message);

private:
    struct Table {
        std::string         path;           ///< the path key of the message that is shown as a table (the name of the control)
        std::string         node;           ///< the node it is in
        std::string         name;           ///< the field
        std::string         kind;           ///< `table` or `properties`
        uint32              command = 0;    ///< the command of the message that it sends when its selection changes (`update_msg`), 0 if it sends none
    };

    struct Known {
        std::string         name;
        type_code           type;
        bool                list;           ///< seen with several values (or declared): edited as a list also with one value or none
    };

    BMessage                _SchemaOf(const std::string& key) const;
    std::string             _SchemaKey(const std::string& path) const;
    bool                    _NodeMessage(const std::string& path, BMessage* node) const;
    std::string             _LabelOf(const BMessage& schema, const std::string& name) const;

    void                    _BuildTree();
    void                    _AddNodes(const BMessage& message, const std::string& prefix, const std::string& key, int32 level);
    void                    _ShowNode(const std::string& path);
    void                    _FillPane(BGridView* pane, const std::string& node);
    void                    _Learn(const std::string& node, const BMessage& message);
    /** the buttons that the schema puts beside a field (`beside`): made here, placed by the field */
    /** a text field without a label of its own, as it stands beside a check box */
    BTextControl*           _PlainTextControl(const std::string& path, const std::string& text, bool secret, const char* help);
    std::vector<BButton*>   _BesideButtons(const std::string& node, const std::string& key, const std::string& partner);
    void                    _AddTable(BGridView* pane, int32* row, const std::string& node, const std::string& key, const std::string& name, const std::string& kind);
    void                    _FillTable(ShojiTableControl* control, const Table& table);
    /** makes the rows of the tables again from the working copy; of the tables that have the cell `path`, or all of them if it is empty */
    void                    _RefreshTables(const std::string& path);
    // Master and detail (SenSettingsForm.h, `update_msg` and `bind_to_update_msg`): a message command ties a control (a table) to the controls that depend on it.
    /** the controls of the pane that listen to a command are found in the schema */
    void                    _RegisterBindings();
    /** a table tells its observers what it holds (BHandler::SendNotices): the parts of the selected row that are messages, or nothing when no row is selected */
    void                    _PublishUpdate(const std::string& tablePath);
    void                    _PublishAll();
    /** the notice of a master reaches the change handler of the form */
    void                    _ApplyNotice(const BMessage& notice);
    /** the bound controls of a command take what the update has for them */
    void                    _ApplyUpdates(uint32 command, const BMessage& update);
    /** a bound control takes what the update message has for it (@p part: NULL, nothing: it is emptied and disabled) */
    void                    _ApplyUpdate(const std::string& path, const BMessage* part);
    /** a bound control was changed by the user: it tells the master of the update (kUpdated), which changes itself from it */
    void                    _Answer(const std::string& path, const std::string& text);
    /** the revert button of a field was pressed: it answers with the default that the update gave */
    void                    _AnswerDefault(const std::string& path);
    void                    _TakeAnswer(const BMessage& answer);
    bool                    _IsBound(const std::string& path) const;
    void                    _SetControlText(BView* control, const std::string& text);
    void                    _AddField(BGridView* pane, int32* row, const std::string& node, const std::string& key, const Known& field, const BMessage& message);

    /** a control was changed by the user (a text, a check box, a choice, a number, a date...: the value comes as text and is converted to the type of the field) */
    void                    _ControlChanged(const std::string& path, const std::string& text);
    void                    _ApplyList(const std::string& node, const std::string& name);
    void                    _Refuse(const std::string& path, const std::string& text, status_t status);
    void                    _Show(const std::string& path);
    /** enables and disables the controls of the shown node whose schema says `enabled_when` (a field of the same message that has a value) */
    void                    _UpdateEnabled();
    void                    _ShowList(const std::string& node, const std::string& name);
    void                    _Changed(const char* path, const char* value);
    /** a row of the table at `tablePath` was moved from one place to another: the working copy follows, the target is told */
    void                    _ReorderRows(const std::string& tablePath, int32 from, int32 to);
    void                    _HandleSetData(BMessage* message);
    void                    _Reply(BMessage* message, status_t result);

    BMessage                fOriginal;
    BMessage                fWorking;
    BMessage                fSchema;
    BMessage                fDefaults;
    std::map<std::string, BMessage> fAttributes;        ///< the schema per path key
    std::map<std::string, std::vector<Known> > fKnown;  ///< the fields of a node in the order they are shown, also those that are not in the working copy (emptied lists)

    BMessenger              fTarget;
    bool                    fEditable;
    BString                 fRootLabel;
    std::string             fNode;                      ///< the path of the node that is shown
    std::vector<std::string> fTextPaths;                ///< the text fields of the pane that is shown
    std::map<std::string, std::string> fBesideFields;           ///< the text fields that stand in the line of a check box (`beside`): by the name of the check box
    std::map<std::string, std::vector<std::string> > fBeside;   ///< the buttons that stand beside a field of the pane that is shown (`beside`): by the name of the field
    std::vector<Table>      fTables;                    ///< the tables of the pane that is shown
    struct Binding {                                    ///< a control that listens to a command (`bind_to_update_msg`)
        std::string path;                               ///< the control
        uint32      command = 0;
        std::string fieldId;                            ///< the name of the part of the message that is meant for it
        BMessenger  reply;                              ///< where the last update says that the answers go (`reply_to`)
        BMessage    last;                               ///< the part of the last update: the type of its value is the type of the answer
        bool        haveSource = false;
    };
    std::vector<Binding>    fBindings;
    ShojiChangeHandler*     fChange = NULL;             ///< the one handler of the form that the masters tell their updates (it is the observer of every table with `update_msg`)
    std::map<std::string, int32> fTableSelection;       ///< the row that was selected in a table, by its path: kept when the form is made again (the plugin answered with its new state)
    struct Waiting {                                    ///< a button that was pressed and has no answer yet: what its line said before (restored by kActionDone)
        std::string button, line, text, tip;
    };
    std::vector<Waiting>    fWaitingButtons;
    std::map<std::string, std::vector<float> > fTableWidths;    ///< the widths that the user gave the columns of a table, by its path: kept when the pane is made again

    BSplitView*             fSplit;
    BScrollView*            fTreeScroll;
    BOutlineListView*       fTree;
    BGroupView*             fPaneHolder;
    BGridView*              fPane;
};
