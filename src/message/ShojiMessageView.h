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

class BGridView;
class BOutlineListView;
class BScrollView;
class BSplitView;
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
    virtual void            MessageReceived(BMessage* message);

private:
    struct Table {
        std::string         path;           ///< the path key of the message that is shown as a table (the name of the control)
        std::string         node;           ///< the node it is in
        std::string         name;           ///< the field
        std::string         kind;           ///< `table` or `properties`
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
    void                    _AddTable(BGridView* pane, int32* row, const std::string& node, const std::string& key, const std::string& name, const std::string& kind);
    void                    _FillTable(ShojiTableControl* control, const Table& table);
    /** makes the rows of the tables again from the working copy; of the tables that have the cell `path`, or all of them if it is empty */
    void                    _RefreshTables(const std::string& path);
    void                    _AddField(BGridView* pane, int32* row, const std::string& node, const std::string& key, const Known& field, const BMessage& message);

    void                    _ApplyText(const std::string& path, const std::string& text);
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
    std::vector<Table>      fTables;                    ///< the tables of the pane that is shown
    std::map<std::string, std::vector<float> > fTableWidths;    ///< the widths that the user gave the columns of a table, by its path: kept when the pane is made again

    BSplitView*             fSplit;
    BScrollView*            fTreeScroll;
    BOutlineListView*       fTree;
    BGroupView*             fPaneHolder;
    BGridView*              fPane;
};
