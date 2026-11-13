/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiMessageView.h"

#include <Alignment.h>
#include <Bitmap.h>
#include <Button.h>
#include <ControlLook.h>
#include <CheckBox.h>
#include <GridLayout.h>
#include <DecimalSpinner.h>
#include <File.h>
#include <FindDirectory.h>
#include <GridView.h>
#include <IconUtils.h>
#include <LayoutItem.h>
#include <ListItem.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <OutlineListView.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <RadioButton.h>
#include <Resources.h>
#include <ScrollView.h>
#include <Slider.h>
#include <SpaceLayoutItem.h>
#include <Spinner.h>
#include <SplitView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>

#include <algorithm>
#include <math.h>
#include <set>
#include <stdio.h>
#include <string.h>

#include <sen/SenList.h>
#include <sen/SenMessagePath.h>
#include <sen/SenSettingsForm.h>

#include "../app/ShojiDateControl.h"
#include "../app/ShojiListControl.h"
#include "../app/ShojiTableControl.h"

using namespace sen::settings;
namespace form = sen::settings::form;

namespace {

/** The icon of an alert of the system (the ones that BAlert shows: `dialog-information`, `dialog-warning`, `dialog-error`, vector icons in libbe), as high as @p size; NULL if there is none. */
BBitmap* AlertIcon(const std::string& state, float size)
{
    const char* name = state == form::kStateInfo ? "dialog-information" : state == form::kStateWarning ? "dialog-warning" : state == form::kStateError ? "dialog-error" : NULL;
    BPath path;
    if (name == NULL || find_directory(B_SYSTEM_LIB_DIRECTORY, &path) != B_OK || path.Append("libbe.so") != B_OK)
        return NULL;
    BFile file(path.Path(), B_READ_ONLY);
    BResources resources(&file);
    size_t length = 0;
    const void* data = resources.LoadResource(B_VECTOR_ICON_TYPE, name, &length);
    if (data == NULL)
        return NULL;
    BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
    if (bitmap->InitCheck() != B_OK || BIconUtils::GetVectorIcon((const uint8*)data, length, bitmap) != B_OK) {
        delete bitmap;
        return NULL;
    }
    return bitmap;
}


/** A view that draws a bitmap (the icon of a status line). */
class IconView : public BView {
public:
    IconView(BBitmap* icon)
        :
        BView("sen:status icon", B_WILL_DRAW),
        fIcon(icon)
    {
        SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        SetDrawingMode(B_OP_ALPHA);
        SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
        const BSize size = fIcon->Bounds().Size();
        SetExplicitMinSize(BSize(size.width + 1, size.height + 1));
        SetExplicitMaxSize(BSize(size.width + 1, size.height + 1));
        SetExplicitPreferredSize(BSize(size.width + 1, size.height + 1));
    }
    ~IconView() { delete fIcon; }
    virtual void Draw(BRect) { DrawBitmap(fIcon, BPoint(0, 0)); }

private:
    BBitmap*    fIcon;
};

// what the controls tell the view (it is their target)
enum {
    kMsgText    = 'MVtx',       // a text field was kept (Enter, or the field was left): path
    kMsgBool    = 'MVbo',       // a check box: path
    kMsgChoice  = 'MVch',       // an item of a menu: path, value
    kMsgList    = 'MVls',       // a list changed: node, name
    kMsgNumber  = 'MVnu',       // a spinner or a slider: path
    kMsgDate    = 'MVda',       // a date: path
    kMsgSelect  = 'MVse',       // a node of the tree was selected
    kMsgTable   = 'MVtb',       // a cell of a table was changed: path, value
    kMsgAction  = 'MVac'        // a button (kind `button`) was pressed: path
};

// the values of a list are written in one string for the list control, separated by this (a value that contains it is quoted)
const char kListSeparator = ';';

// a node of the tree: the path of the nested message in the working copy
class NodeItem : public BStringItem {
public:
    NodeItem(const char* label, const std::string& path, uint32 level)
        :
        BStringItem(label, level, true),
        fPath(path)
    {
    }

    const std::string& Path() const { return fPath; }

private:
    std::string fPath;
};


// A slider for a number: the positions of the track are the steps from the smallest value, and the value is shown above the thumb.
class ValueSlider : public BSlider {
public:
    ValueSlider(const char* name, BMessage* message, double minimum, double step, int32 positions, int32 decimals)
        :
        BSlider(name, NULL, message, 0, positions, B_HORIZONTAL),
        fMinimum(minimum),
        fStep(step),
        fDecimals(decimals)
    {
    }

    double ValueNumber() const { return fMinimum + Value() * fStep; }

    std::string ValueText() const
    {
        char text[64];
        snprintf(text, sizeof(text), "%.*f", (int)fDecimals, ValueNumber());
        return text;
    }

    void SetNumber(double number) { SetValue((int32)floor((number - fMinimum) / fStep + 0.5)); }

    /** The text that the schema gives for the ends of the track (`min_label`, `max_label`): shown instead of the number when the thumb is at that end. */
    void SetEndTexts(const char* atMinimum, const char* atMaximum)
    {
        fAtMinimum = atMinimum != NULL ? atMinimum : "";
        fAtMaximum = atMaximum != NULL ? atMaximum : "";
    }

    virtual const char* UpdateText() const
    {
        int32 lowest = 0, highest = 0;
        GetLimits(&lowest, &highest);
        if (Value() <= lowest && !fAtMinimum.empty())
            fText = fAtMinimum;
        else if (Value() >= highest && !fAtMaximum.empty())
            fText = fAtMaximum;
        else
            fText = ValueText();
        return fText.c_str();
    }

private:
    std::string         fAtMinimum;
    std::string         fAtMaximum;
    double              fMinimum;
    double              fStep;
    int32               fDecimals;
    mutable std::string fText;
};


bool IsIntegerType(type_code type)
{
    switch (type) {
        case B_INT8_TYPE: case B_INT16_TYPE: case B_INT32_TYPE: case B_INT64_TYPE:
        case B_UINT8_TYPE: case B_UINT16_TYPE: case B_UINT32_TYPE: case B_UINT64_TYPE:
            return true;
    }
    return false;
}


bool IsNumberType(type_code type)
{
    return IsIntegerType(type) || type == B_FLOAT_TYPE || type == B_DOUBLE_TYPE;
}


// the decimals that a step needs (0.25 needs two)
int32 DecimalsOf(double step)
{
    int32 decimals = 0;
    while (decimals < 6 && fabs(step * pow(10.0, decimals) - floor(step * pow(10.0, decimals) + 0.5)) > 1e-9)
        decimals++;
    return decimals;
}

// `max_authors` is shown as `Max authors`
std::string Humanize(const std::string& name)
{
    std::string label;
    for (char c : name)
        label += (c == '_' || c == '.') ? ' ' : c;
    if (!label.empty() && label[0] >= 'a' && label[0] <= 'z')
        label[0] = (char)(label[0] - 'a' + 'A');
    return label;
}


const char* TypeName(type_code type)
{
    switch (type) {
        case B_RGB_COLOR_TYPE:      return "color";
        case B_POINT_TYPE:          return "point";
        case B_RECT_TYPE:           return "rectangle";
        case B_REF_TYPE:            return "file reference";
        case B_RAW_TYPE:            return "raw data";
        case B_MESSENGER_TYPE:      return "messenger";
        case B_POINTER_TYPE:        return "pointer";
        case B_SIZE_TYPE:           return "size";
        case B_TIME_TYPE:           return "time";
    }
    return "data of another type";
}


// the node of a message that a path names; the empty path is the message itself
bool NodeIn(const BMessage& root, const std::string& path, BMessage* node)
{
    if (path.empty()) {
        *node = root;
        return true;
    }
    detail::Located located;
    if (detail::Locate(root, path, &located) != B_OK || located.type != B_MESSAGE_TYPE
            || located.step.index >= located.step.count)
        return false;
    return located.container.FindMessage(located.step.name.c_str(), (int32)located.step.index, node) == B_OK;
}


// whether the schema of a field makes a table of it (`kind = table`: the rows are an array of messages; `kind = properties`: the rows are the fields of a message)
bool IsTableKind(const BMessage& attributes, std::string* kind = NULL)
{
    const char* text = NULL;
    if (attributes.FindString(form::kAttrKind, &text) != B_OK || text == NULL)
        return false;
    if (strcmp(text, form::kKindTable) != 0 && strcmp(text, form::kKindProperties) != 0)
        return false;
    if (kind != NULL)
        *kind = text;
    return true;
}


// whether a condition of `enabled_when` holds for the fields of a message: `key=value`, several of them with `&` (all hold) and alternatives with `|` (one holds)
bool ConditionHolds(const std::string& condition, const BMessage& message)
{
    for (size_t from = 0; from <= condition.size(); ) {
        size_t end = condition.find('|', from);
        if (end == std::string::npos)
            end = condition.size();
        const std::string alternative = condition.substr(from, end - from);
        from = end + 1;

        bool all = !alternative.empty();
        for (size_t start = 0; start <= alternative.size() && all; ) {
            size_t stop = alternative.find('&', start);
            if (stop == std::string::npos)
                stop = alternative.size();
            const std::string term = alternative.substr(start, stop - start);
            start = stop + 1;
            const size_t equals = term.find('=');
            std::string have;
            all = equals != std::string::npos
                && GetText(message, EscapeSegment(term.substr(0, equals)), &have) == B_OK && have == term.substr(equals + 1);
        }
        if (all)
            return true;
    }
    return false;
}


// whether a path is a cell of the table at `table` (`programs_02.level` of `programs`, `nested.mode` of `nested`)
bool InTable(const std::string& path, const std::string& table)
{
    return path.compare(0, table.size(), table) == 0
        && (path.size() == table.size() || path[table.size()] == '_' || path[table.size()] == '.');
}


// the controls send to the view, which can only be their target when it is in a window
void Connect(BView* parent, BHandler* target)
{
    for (int32 i = 0; i < parent->CountChildren(); i++) {
        BView* child = parent->ChildAt(i);
        if (ShojiListControl* list = dynamic_cast<ShojiListControl*>(child)) {
            list->SetTarget(target);
            continue;
        }
        if (ShojiTableControl* table = dynamic_cast<ShojiTableControl*>(child)) {
            table->SetTarget(target);
            continue;
        }
        if (BMenuField* menuField = dynamic_cast<BMenuField*>(child)) {
            if (menuField->Menu() != NULL)
                menuField->Menu()->SetTargetForItems(target);
        } else if (BControl* control = dynamic_cast<BControl*>(child)) {
            if (control->Message() != NULL)
                control->SetTarget(target);
        }
        Connect(child, target);
    }
}

}   // namespace


ShojiMessageView::ShojiMessageView(const char* name)
    :
    BGroupView(name, B_VERTICAL, 0),
    fEditable(true),
    fRootLabel("Settings"),
    fSplit(NULL),
    fTreeScroll(NULL),
    fTree(NULL),
    fPaneHolder(NULL),
    fPane(NULL)
{
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    fTree = new BOutlineListView("sen:tree", B_SINGLE_SELECTION_LIST);
    fTree->SetSelectionMessage(new BMessage(kMsgSelect));
    fTreeScroll = new BScrollView("sen:tree scroll", fTree, 0, false, true, B_FANCY_BORDER);
    fTreeScroll->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 12, B_SIZE_UNSET));
    fTreeScroll->SetExplicitPreferredSize(BSize(be_plain_font->StringWidth("M") * 16, B_SIZE_UNSET));
    fTreeScroll->Hide();

    fPaneHolder = new BGroupView("sen:pane", B_VERTICAL, 0);
    fPaneHolder->GroupLayout()->SetInsets(B_USE_DEFAULT_SPACING);

    fSplit = new BSplitView(B_HORIZONTAL, 0);
    fSplit->AddChild(fTreeScroll, 0.25f);
    fSplit->AddChild(fPaneHolder, 0.75f);
    GroupLayout()->AddView(fSplit);

    fPane = new BGridView("sen:empty");
    fPaneHolder->GroupLayout()->AddView(fPane);
}


// The form is made from data, so it archives as a stub: what is needed to load the class (see SenSettingsFormHost.h).
BArchivable* ShojiMessageView::Instantiate(BMessage* archive)
{
    if (!validate_instantiation(archive, form::kViewClass))
        return NULL;
    return new ShojiMessageView(archive->GetString("_name", "message view"));
}


status_t ShojiMessageView::Archive(BMessage* archive, bool deep) const
{
    status_t result = BArchivable::Archive(archive, false);
    if (result == B_OK) {
        archive->RemoveName("add_on");
        result = archive->AddString("add_on", form::kShojiSignature);
    }
    if (result == B_OK)
        result = archive->AddString("_name", Name());
    return result;
}


ShojiMessageView::~ShojiMessageView()
{
    // the items of the tree are ours, the views are the window's
    std::vector<BListItem*> items;
    for (int32 i = 0; i < fTree->FullListCountItems(); i++)
        items.push_back(fTree->FullListItemAt(i));
    fTree->MakeEmpty();
    for (BListItem* item : items)
        delete item;
}


void ShojiMessageView::AttachedToWindow()
{
    BGroupView::AttachedToWindow();
    fTree->SetTarget(this);
    Connect(fPane, this);
}


status_t ShojiMessageView::SetData(const BMessage& settings, const BMessage* schema, const BMessage* defaults)
{
    fOriginal = settings;
    fWorking = settings;
    fSchema = schema != NULL ? *schema : BMessage();
    fDefaults = defaults != NULL ? *defaults : BMessage();
    fKnown.clear();

    fAttributes.clear();
    char* name = NULL;
    type_code type = 0;
    for (int32 i = 0; fSchema.GetInfo(B_MESSAGE_TYPE, i, &name, &type, NULL) == B_OK; i++) {
        BMessage attributes;
        if (fSchema.FindMessage(name, &attributes) == B_OK)
            fAttributes[name] = attributes;
    }

    _BuildTree();
    std::string node = fNode;
    BMessage ignored;
    if (!NodeIn(fWorking, node, &ignored))
        node.clear();
    _ShowNode(node);
    return B_OK;
}


bool ShojiMessageView::IsModified() const
{
    return !fWorking.HasSameData(fOriginal, true, true);
}


void ShojiMessageView::SetTarget(BMessenger target)
{
    fTarget = target;
}


void ShojiMessageView::SetEditable(bool editable)
{
    if (fEditable == editable)
        return;
    fEditable = editable;
    _ShowNode(fNode);
}


void ShojiMessageView::SetRootLabel(const char* label)
{
    fRootLabel = label != NULL ? label : "";
    _BuildTree();
    _ShowNode(fNode);
}


void ShojiMessageView::Revert()
{
    if (!fEditable)
        return;
    fWorking = fOriginal;
    _BuildTree();
    BMessage ignored;
    _ShowNode(NodeIn(fWorking, fNode, &ignored) ? fNode : std::string());
    _Changed(NULL, NULL);
}


status_t ShojiMessageView::Defaults()
{
    if (!HasDefaults())
        return B_NOT_SUPPORTED;
    if (!fEditable)
        return B_NOT_ALLOWED;
    fWorking = fDefaults;
    _BuildTree();
    BMessage ignored;
    _ShowNode(NodeIn(fWorking, fNode, &ignored) ? fNode : std::string());
    _Changed(NULL, NULL);
    return B_OK;
}


void ShojiMessageView::CommitPending()
{
    // (a copy: keeping a field can show it again)
    const std::vector<std::string> paths = fTextPaths;
    for (const std::string& path : paths) {
        BTextControl* control = dynamic_cast<BTextControl*>(ControlFor(path.c_str()));
        if (control == NULL || !control->IsEnabled())
            continue;
        std::string current;
        if (GetText(fWorking, path, &current) == B_OK && current != control->Text())
            _ApplyText(path, control->Text());
    }

    // a cell of a table that is being edited
    const std::vector<Table> tables = fTables;
    for (const Table& table : tables) {
        ShojiTableControl* control = dynamic_cast<ShojiTableControl*>(ControlFor(table.path.c_str()));
        std::string path, text;
        if (control != NULL && control->PendingEdit(&path, &text)) {
            control->EndEdit(false);
            _ApplyText(path, text);
        }
    }
}


BView* ShojiMessageView::ControlFor(const char* path)
{
    return fPane != NULL ? fPane->FindView(path) : NULL;
}


int32 ShojiMessageView::CountNodes() const
{
    return fTreeScroll->IsHidden(fTreeScroll) ? 0 : fTree->FullListCountItems();
}


bool ShojiMessageView::SelectNode(const std::string& path)
{
    for (int32 i = 0; i < fTree->FullListCountItems(); i++) {
        NodeItem* item = dynamic_cast<NodeItem*>(fTree->FullListItemAt(i));
        if (item != NULL && item->Path() == path) {
            CommitPending();
            _ShowNode(path);
            return true;
        }
    }
    // no tree: the fields of the message itself are what there is
    if (path.empty() && fTree->FullListCountItems() == 0) {
        _ShowNode(path);
        return true;
    }
    return false;
}


void ShojiMessageView::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case kMsgText:
        {
            const char* path;
            BTextControl* control;
            if (message->FindString("path", &path) == B_OK
                    && (control = dynamic_cast<BTextControl*>(ControlFor(path))) != NULL)
                _ApplyText(path, control->Text());
            break;
        }
        case kMsgAction:
        {
            // a button is no value: the target is told that it was pressed, and nothing changes in the working copy
            const char* path;
            if (message->FindString("path", &path) == B_OK && fTarget.IsValid()) {
                BMessage action(form::kAction);
                action.AddString(form::kFieldPath, path);
                action.AddInt32(form::kFieldProtocol, form::kProtocol);
                fTarget.SendMessage(&action);
            }
            break;
        }
        case kMsgBool:
        {
            const char* path;
            BCheckBox* control;
            if (message->FindString("path", &path) == B_OK
                    && (control = dynamic_cast<BCheckBox*>(ControlFor(path))) != NULL)
                _ApplyText(path, control->Value() == B_CONTROL_ON ? "true" : "false");
            break;
        }
        case kMsgChoice:
        {
            const char* path;
            const char* value;
            if (message->FindString("path", &path) == B_OK && message->FindString("value", &value) == B_OK)
                _ApplyText(path, value);
            break;
        }
        case kMsgList:
        {
            const char* name;
            std::string node;
            const char* nodePath = "";
            message->FindString("node", &nodePath);
            node = nodePath;
            if (message->FindString("name", &name) == B_OK)
                _ApplyList(node, name);
            break;
        }
        case form::kSetData:
            _HandleSetData(message);
            break;
        case form::kGetData:
        {
            BMessage reply(B_REPLY);
            reply.AddMessage(form::kFieldSettings, &fWorking);
            reply.AddInt32(form::kFieldProtocol, form::kProtocol);
            reply.AddInt32(form::kFieldResult, B_OK);
            if (message->IsSourceWaiting())
                message->SendReply(&reply);
            break;
        }
        case form::kRevert:
            Revert();
            _Reply(message, B_OK);
            break;
        case form::kDefaults:
            _Reply(message, Defaults());
            break;
        case kMsgNumber:
        {
            const char* path;
            BView* control;
            if (message->FindString("path", &path) != B_OK || (control = ControlFor(path)) == NULL)
                break;
            if (BSpinner* spinner = dynamic_cast<BSpinner*>(control))
                _ApplyText(path, std::to_string(spinner->Value()));
            else if (BDecimalSpinner* decimal = dynamic_cast<BDecimalSpinner*>(control))
                _ApplyText(path, detail::NumberText(decimal->Value()));
            else if (ValueSlider* slider = dynamic_cast<ValueSlider*>(control))
                _ApplyText(path, slider->ValueText());
            break;
        }
        case kMsgDate:
        {
            const char* path;
            ShojiDateControl* control;
            if (message->FindString("path", &path) == B_OK && (control = dynamic_cast<ShojiDateControl*>(ControlFor(path))) != NULL)
                _ApplyText(path, control->DateText().String());
            break;
        }
        case kMsgTable:
        {
            const char* path;
            const char* value;
            int32 from, to;
            if (message->FindInt32("reorder_from", &from) == B_OK && message->FindInt32("reorder_to", &to) == B_OK) {
                _ReorderRows(message->GetString("table", ""), from, to);
            } else if (message->FindString("path", &path) == B_OK && message->FindString("value", &value) == B_OK) {
                _ApplyText(path, value);
            }
            break;
        }
        case kMsgSelect:
        {
            NodeItem* item = dynamic_cast<NodeItem*>(fTree->FullListItemAt(fTree->FullListCurrentSelection()));
            if (item != NULL && item->Path() != fNode) {
                CommitPending();
                _ShowNode(item->Path());
            }
            break;
        }
        default:
            BGroupView::MessageReceived(message);
            break;
    }
}


// The schema of a field or a node, by its path key without indexes (`nested.items.name`).
BMessage ShojiMessageView::_SchemaOf(const std::string& key) const
{
    std::map<std::string, BMessage>::const_iterator found = fAttributes.find(key);
    return found != fAttributes.end() ? found->second : BMessage();
}


// The path key of a node as the schema has it: the names of the fields, without the index of an array of messages.
std::string ShojiMessageView::_SchemaKey(const std::string& path) const
{
    std::vector<std::string> segments, names;
    if (!SplitPath(path, &segments))
        return std::string();

    BMessage current(fWorking);
    for (const std::string& segment : segments) {
        Step step;
        if (!ResolveSegment(segment, detail::InfoOf(current), &step)) {
            names.push_back(segment);
            break;
        }
        names.push_back(step.name);
        BMessage child;
        if (current.FindMessage(step.name.c_str(), (int32)step.index, &child) != B_OK)
            break;
        current = child;
    }
    return JoinPath(names);
}


std::string ShojiMessageView::_LabelOf(const BMessage& schema, const std::string& name) const
{
    const char* label = NULL;
    if (schema.FindString(form::kAttrLabel, &label) == B_OK && label != NULL && label[0] != '\0')
        return label;
    return Humanize(name);
}


// The fields of a node in the order they are shown: those of the data as it was set, then the others. A list that was emptied has no field left in the working
// copy and has to stay in the form, so the fields are remembered.
void ShojiMessageView::_Learn(const std::string& node, const BMessage& message)
{
    std::vector<Known>& known = fKnown[node];

    auto learn = [&known](const BMessage& from) {
        char* name = NULL;
        type_code type = 0;
        int32 count = 0;
        for (int32 i = 0; from.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
            if (type == B_MESSAGE_TYPE)
                continue;
            std::vector<Known>::iterator at = std::find_if(known.begin(), known.end(),
                [name](const Known& k) { return k.name == name; });
            if (at == known.end())
                known.push_back(Known{name, type, count > 1});
            else if (count > 1)
                at->list = true;
        }
    };

    BMessage other;
    if (NodeIn(fOriginal, node, &other))
        learn(other);
    learn(message);
    if (NodeIn(fDefaults, node, &other))
        learn(other);
}


// The tree: the fields of the message itself, then a node for every nested message (an array of messages has one for each). A message without a nested
// message needs no tree.
void ShojiMessageView::_BuildTree()
{
    std::vector<BListItem*> old;
    for (int32 i = 0; i < fTree->FullListCountItems(); i++)
        old.push_back(fTree->FullListItemAt(i));
    fTree->MakeEmpty();
    for (BListItem* item : old)
        delete item;

    // (a message that the schema shows as a table is no node of the tree)
    bool nodes = false;
    char* fieldName = NULL;
    type_code fieldType = 0;
    for (int32 i = 0; fWorking.GetInfo(B_ANY_TYPE, i, &fieldName, &fieldType, NULL) == B_OK; i++) {
        if (fieldType == B_MESSAGE_TYPE && !IsTableKind(_SchemaOf(fieldName)))
            nodes = true;
    }
    if (!nodes) {
        if (!fTreeScroll->IsHidden(fTreeScroll))
            fTreeScroll->Hide();
        return;
    }

    fTree->AddItem(new NodeItem(fRootLabel.String(), std::string(), 0));
    _AddNodes(fWorking, std::string(), std::string(), 1);
    while (fTreeScroll->IsHidden(fTreeScroll))
        fTreeScroll->Show();
}


void ShojiMessageView::_AddNodes(const BMessage& message, const std::string& prefix, const std::string& key, int32 level)
{
    const FieldInfo info = detail::InfoOf(message);
    char* name = NULL;
    type_code type = 0;
    int32 count = 0;
    for (int32 i = 0; message.GetInfo(B_MESSAGE_TYPE, i, &name, &type, &count) == B_OK; i++) {
        const std::string fieldName = name;
        const std::string fieldKey = ChildPath(key, fieldName);
        const BMessage attributes = _SchemaOf(fieldKey);
        if (attributes.GetBool(form::kAttrHidden, false) || IsTableKind(attributes))
            continue;

        for (int32 index = 0; index < count; index++) {
            // a node whose message cannot be named cannot be edited
            const std::string segment = SegmentFor(fieldName, (size_t)index, (size_t)count, info);
            if (segment.empty())
                continue;
            const std::string path = ChildPath(prefix, segment);

            std::string label = _LabelOf(attributes, fieldName);
            if (count > 1)
                label += " " + std::to_string(index + 1);
            fTree->AddItem(new NodeItem(label.c_str(), path, (uint32)level));

            BMessage child;
            if (message.FindMessage(name, index, &child) == B_OK)
                _AddNodes(child, path, fieldKey, level + 1);
        }
    }
}


// shows the controls of a node, and selects it in the tree
void ShojiMessageView::_ShowNode(const std::string& path)
{
    fNode = path;

    BGridView* pane = new BGridView("sen:node");
    pane->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    fTextPaths.clear();
    // (the widths that the user gave the columns of the tables stay, also when the form is made again)
    for (const Table& shownTable : fTables) {
        if (ShojiTableControl* control = dynamic_cast<ShojiTableControl*>(ControlFor(shownTable.path.c_str())))
            fTableWidths[shownTable.path] = control->UserWidths();
    }
    fTables.clear();
    _FillPane(pane, path);

    BView* old = fPane;
    fPaneHolder->GroupLayout()->RemoveView(old);
    old->RemoveSelf();
    delete old;
    fPane = pane;
    fPaneHolder->GroupLayout()->AddView(pane);
    if (Window() != NULL)
        Connect(pane, this);
    _UpdateEnabled();

    for (int32 i = 0; i < fTree->FullListCountItems(); i++) {
        NodeItem* item = dynamic_cast<NodeItem*>(fTree->FullListItemAt(i));
        if (item != NULL && item->Path() == path) {
            if (fTree->FullListCurrentSelection() != i)
                fTree->Select(fTree->IndexOf(item));
            break;
        }
    }
}


void ShojiMessageView::_FillPane(BGridView* pane, const std::string& node)
{
    BGridLayout* layout = pane->GridLayout();
    int32 row = 0;

    BMessage message;
    if (NodeIn(fWorking, node, &message)) {
        _Learn(node, message);
        const std::string key = node.empty() ? std::string() : _SchemaKey(node);

        // the fields that are there, and the lists that were emptied; in the order of the schema where it has one
        struct Shown {
            Known       field;
            double      order;
            bool        table = false;      ///< a message that is shown as a table
            std::string kind;
        };
        std::vector<Shown> shown;
        const std::vector<Known>& known = fKnown[node];
        // the text fields that a button shows beside itself (`shows`): they have no line of their own
        std::set<std::string> besideButton;
        for (size_t i = 0; i < known.size(); i++) {
            const BMessage attributes = _SchemaOf(ChildPath(key, known[i].name));
            const char* kindOfField = NULL;
            const char* shows = NULL;
            if (attributes.FindString(form::kAttrKind, &kindOfField) == B_OK && strcmp(kindOfField, form::kKindButton) == 0
                    && attributes.FindString(form::kAttrShows, &shows) == B_OK)
                besideButton.insert(shows);
        }
        for (size_t i = 0; i < known.size(); i++) {
            const BMessage attributes = _SchemaOf(ChildPath(key, known[i].name));
            if (attributes.GetBool(form::kAttrHidden, false) || besideButton.count(known[i].name) != 0)
                continue;
            Known field = known[i];
            if (attributes.GetBool(form::kAttrList, false))
                field.list = true;

            type_code have = 0;
            int32 count = 0;
            const bool present = message.GetInfo(known[i].name.c_str(), &have, &count) == B_OK;
            if (!present && !(field.list && detail::IsText(field.type)))
                continue;
            if (present && have != field.type)
                continue;       // (the type of a field is what it was; one that changed is not edited under the old one)
            shown.push_back(Shown{field, (double)attributes.GetInt32(form::kAttrOrder, (int32)i)});
        }

        // the messages that the schema makes tables (`kind = table` or `properties`), after the other fields unless the schema has an order
        char* messageName = NULL;
        type_code messageType = 0;
        for (int32 i = 0; message.GetInfo(B_MESSAGE_TYPE, i, &messageName, &messageType, NULL) == B_OK; i++) {
            const BMessage attributes = _SchemaOf(ChildPath(key, messageName));
            std::string kind;
            if (attributes.GetBool(form::kAttrHidden, false) || !IsTableKind(attributes, &kind))
                continue;
            shown.push_back(Shown{Known{messageName, B_MESSAGE_TYPE, false},
                (double)attributes.GetInt32(form::kAttrOrder, (int32)(known.size() + i)), true, kind});
        }
        // the tables that the schema has and the message has no rows for (an array without values is no field of the message): shown empty, with the titles of their columns
        for (const std::pair<const std::string, BMessage>& entry : fAttributes) {
            std::string tableKind;
            if (!IsTableKind(entry.second, &tableKind) || tableKind != form::kKindTable || entry.second.GetBool(form::kAttrHidden, false))
                continue;
            std::string name;
            if (key.empty()) {
                name = entry.first;
            } else if (entry.first.compare(0, key.size() + 1, key + ".") == 0) {
                name = entry.first.substr(key.size() + 1);
            } else {
                continue;
            }
            type_code presentType = 0;
            if (name.empty() || name.find('.') != std::string::npos || message.GetInfo(name.c_str(), &presentType) == B_OK)
                continue;
            shown.push_back(Shown{Known{name, B_MESSAGE_TYPE, false}, (double)entry.second.GetInt32(form::kAttrOrder, (int32)(known.size() + shown.size())), true, "table"});
        }
        std::stable_sort(shown.begin(), shown.end(), [](const Shown& a, const Shown& b) { return a.order < b.order; });

        for (const Shown& item : shown) {
            if (item.table)
                _AddTable(pane, &row, node, key, item.field.name, item.kind);
            else
                _AddField(pane, &row, node, key, item.field, message);
        }
    }

    if (row == 0) {
        BStringView* note = new BStringView("sen:note", "There is nothing to set here.");
        note->SetHighUIColor(B_PANEL_TEXT_COLOR, B_DARKEN_1_TINT);
        layout->AddView(note, 0, row++, 2, 1);
    }
    // The form follows the window: the pane is as wide as the window lets it be. What limits it are the controls that are only as wide as they need to be (a button, a label, a menu bar): the layout
    // takes the smallest maximum of a column. They keep their size and stay at the left of their column, and the controls that can use more width (the menus, the text fields and the
    // tables, which have the alignment "full width") take the rest.
    for (int32 i = 0; i < layout->CountItems(); i++) {
        BLayoutItem* item = layout->BLayout::ItemAt(i);
        const BSize limit = item->MaxSize();
        if (limit.width >= B_SIZE_UNLIMITED / 2)
            continue;
        const BAlignment alignment = item->Alignment();
        item->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
        if (alignment.horizontal != B_ALIGN_USE_FULL_WIDTH && alignment.horizontal != B_ALIGN_LEFT)
            item->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, alignment.vertical));
    }
    layout->SetColumnWeight(1, 100.0f);
    // keep the rows together at the top
    layout->AddItem(BSpaceLayoutItem::CreateGlue(), 0, row, 2, 1);
    layout->SetRowWeight(row, 100.0f);
}


/**
 * The line of a text that only shows: the text, and if the schema of the field names a state field (`state_field`) the icon of that state before it, and a detail field (`detail_field`) the
 * tool tip. Plain text view if the schema asks for nothing of it. @p name is the name of the text view (its path).
 */
static BView* MakeStatusLine(const std::string& name, const std::string& text, const BMessage& attributes, const BMessage& message, const char* help)
{
    auto fieldText = [&](const char* attribute) {
        const char* field = NULL;
        std::string value;
        if (attributes.FindString(attribute, &field) == B_OK && field != NULL && GetText(message, EscapeSegment(field), &value) == B_OK)
            return value;
        return std::string();
    };
    const std::string state = fieldText(form::kAttrStateField);
    std::string detail = fieldText(form::kAttrDetailField);

    BStringView* shown = new BStringView(name.c_str(), text.c_str());
    shown->SetTruncation(B_TRUNCATE_END);
    shown->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 8, B_SIZE_UNSET));
    shown->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_CENTER));
    shown->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    if (detail.empty() && help != NULL)
        detail = help;
    if (detail.empty())
        detail = text;
    if (!detail.empty())
        shown->SetToolTip(detail.c_str());

    font_height height;
    be_plain_font->GetHeight(&height);
    BBitmap* icon = state.empty() ? NULL : AlertIcon(state, ceilf(height.ascent + height.descent + height.leading) + 4);
    if (icon == NULL)
        return shown;

    BGroupView* line = new BGroupView(B_HORIZONTAL, be_control_look->DefaultItemSpacing() / 2);
    line->GroupLayout()->SetInsets(0, 0, 0, 0);
    IconView* iconView = new IconView(icon);
    if (!detail.empty())
        iconView->SetToolTip(detail.c_str());
    line->GroupLayout()->AddView(iconView);
    line->GroupLayout()->AddView(shown);
    line->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_CENTER));
    line->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    return line;
}


void ShojiMessageView::_AddField(BGridView* pane, int32* row, const std::string& node, const std::string& key, const Known& field,
    const BMessage& message)
{
    BGridLayout* layout = pane->GridLayout();
    const BMessage attributes = _SchemaOf(ChildPath(key, field.name));
    const std::string label = _LabelOf(attributes, field.name);
    const char* help = NULL;
    attributes.FindString(form::kAttrHelp, &help);
    const std::string fieldPath = ChildPath(node, field.name);

    type_code have = 0;
    int32 values = 0;
    message.GetInfo(field.name.c_str(), &have, &values);
    const size_t count = (size_t)values;
    const FieldInfo info = detail::InfoOf(message);

    // a line with a label and a note, for what cannot be edited here (the value is kept as it is)
    auto readOnly = [&](const std::string& note) {
        BStringView* name = new BStringView(fieldPath.c_str(), label.c_str());
        name->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
        BStringView* text = new BStringView("sen:note", note.c_str());
        text->SetHighUIColor(B_PANEL_TEXT_COLOR, B_DARKEN_1_TINT);
        layout->AddView(name, 0, *row);
        layout->AddView(text, 1, *row);
        (*row)++;
    };

    if (!detail::IsText(field.type)) {
        std::string note = "(";
        note += TypeName(field.type);
        note += ", not editable here)";
        readOnly(note);
        return;
    }

    // a list: all values of the field in one control
    if (field.list) {
        std::vector<std::string> texts;
        bool addressable = true;
        for (size_t i = 0; i < count && addressable; i++) {
            const std::string segment = SegmentFor(field.name, i, count, info);
            std::string text;
            if (segment.empty() || GetText(message, EscapeSegment(segment), &text) != B_OK)
                addressable = false;
            else
                texts.push_back(text);
        }
        if (!addressable) {
            readOnly("(values with names that clash, not editable here)");
            return;
        }

        ShojiListControl* list = new ShojiListControl(fieldPath.c_str(), kListSeparator);
        list->SetListText(sen::list::Join(texts, kListSeparator).c_str());
        BMessage* changed = new BMessage(kMsgList);
        changed->AddString("node", node.c_str());
        changed->AddString("name", field.name.c_str());
        list->SetMessage(changed);
        list->SetEnabled(fEditable);
        if (help != NULL)
            list->SetToolTip(help);

        BStringView* name = new BStringView("sen:label", label.c_str());
        name->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
        layout->AddView(name, 0, *row);
        list->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_TOP));
        layout->AddView(list, 1, *row);
        (*row)++;
        return;
    }

    const std::string segment = SegmentFor(field.name, 0, 1, info);
    std::string text;
    if (segment.empty() || GetText(message, EscapeSegment(segment), &text) != B_OK) {
        readOnly("(not editable here)");
        return;
    }
    const std::string path = ChildPath(node, segment);

    const char* kindText = NULL;
    attributes.FindString(form::kAttrKind, &kindText);
    const std::string kind = kindText != NULL ? kindText : "";

    // a button: pressing it asks the owner of the settings to do something (a field of the type bool that is never stored); it stands in the column of the controls
    if (kind == form::kKindButton) {
        BMessage* pressed = new BMessage(kMsgAction);
        pressed->AddString("path", path.c_str());
        BButton* button = new BButton(path.c_str(), label.c_str(), pressed);
        button->SetEnabled(fEditable);
        if (help != NULL)
            button->SetToolTip(help);
        // the text that the button shows (what came of it) stands in the same line, in the column of the controls (under the menu above it, with room for the text); the button is
        // in the column of the labels, at its right edge, where the labels end
        const char* shows = NULL;
        std::string shownText;
        if (attributes.FindString(form::kAttrShows, &shows) == B_OK && GetText(message, EscapeSegment(shows), &shownText) == B_OK) {
            // (in a line of its own with room before it: a button that is given the width of its column would fill it, an alignment does not make it smaller)
            BGroupView* right = new BGroupView(B_HORIZONTAL, 0);
            right->GroupLayout()->SetInsets(0, 0, 0, 0);
            right->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
            right->GroupLayout()->AddView(button);
            right->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_CENTER));
            layout->AddView(right, 0, *row);
            BView* result = MakeStatusLine(ChildPath(node, shows), shownText, _SchemaOf(ChildPath(key, shows)), message, NULL);
            layout->AddView(result, 1, *row);
        } else {
            button->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
            layout->AddView(button, 1, *row);
        }
        (*row)++;
        return;
    }

    // a text that is only shown (the owner makes it: the place that was found, a state), selectable by being in a text field that cannot be edited
    if (attributes.GetBool(form::kAttrReadOnly, false) && field.type != B_BOOL_TYPE) {
        BStringView* name = new BStringView("sen:label", label.c_str());
        name->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
        BView* shown = MakeStatusLine(path, text, attributes, message, help);
        layout->AddView(name, 0, *row);
        layout->AddView(shown, 1, *row);
        (*row)++;
        return;
    }

    if (field.type == B_BOOL_TYPE) {
        BMessage* changed = new BMessage(kMsgBool);
        changed->AddString("path", path.c_str());
        BCheckBox* checkBox = new BCheckBox(path.c_str(), label.c_str(), changed);
        checkBox->SetValue(text == "true" ? B_CONTROL_ON : B_CONTROL_OFF);
        checkBox->SetEnabled(fEditable);
        if (help != NULL)
            checkBox->SetToolTip(help);
        layout->AddView(checkBox, 0, *row, 2, 1);
        (*row)++;
        return;
    }

    // a date: the date control of the Time preferences (a time of day needs a text field)
    if (field.type == B_TIME_TYPE && ShojiDateControl::IsDateText(text.c_str())) {
        BMessage* changed = new BMessage(kMsgDate);
        changed->AddString("path", path.c_str());
        ShojiDateControl* date = new ShojiDateControl(path.c_str(), changed);
        date->SetDateText(text.c_str());
        date->SetEnabled(fEditable);
        date->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
        if (help != NULL)
            date->SetToolTip(help);
        BStringView* name = new BStringView("sen:label", label.c_str());
        layout->AddView(name, 0, *row);
        layout->AddView(date, 1, *row);
        (*row)++;
        return;
    }

    type_code choicesType = 0;
    int32 choiceCount = 0;
    const bool hasChoices = attributes.GetInfo(form::kAttrChoices, &choicesType, &choiceCount) == B_OK && choicesType == B_STRING_TYPE && choiceCount > 0;

    // a number with a range: a spinner, or a slider if the schema says so
    double low = 0, high = 0;
    const bool hasRange = IsNumberType(field.type) && attributes.FindDouble(form::kAttrMin, &low) == B_OK
        && attributes.FindDouble(form::kAttrMax, &high) == B_OK && low <= high;
    double number = 0;
    const bool isInteger = IsIntegerType(field.type);
    if (hasRange && detail::ParseDouble(text, &number) && number >= low && number <= high
            && (kind == form::kKindSlider || kind == form::kKindSpinner || (kind.empty() && !hasChoices))) {
        double step = attributes.GetDouble(form::kAttrStep, isInteger ? 1.0 : ((high - low) >= 10 ? 1.0 : 0.1));
        if (isInteger)
            step = std::max(1.0, floor(step + 0.5));
        else if (step <= 0)
            step = 0.1;
        const int32 decimals = isInteger ? 0 : DecimalsOf(step);
        const double positions = floor((high - low) / step + 0.5);

        BMessage* changed = new BMessage(kMsgNumber);
        changed->AddString("path", path.c_str());
        if (kind == form::kKindSlider && positions >= 1 && positions <= 100000) {
            ValueSlider* slider = new ValueSlider(path.c_str(), changed, low, step, (int32)positions, decimals);
            slider->SetNumber(number);
            slider->SetEnabled(fEditable);
            // (the ends of the track and the value above the thumb at the ends say "None" or "All", if the schema has texts for them, else the numbers)
            const char* minLabel = NULL;
            const char* maxLabel = NULL;
            attributes.FindString(form::kAttrMinLabel, &minLabel);
            attributes.FindString(form::kAttrMaxLabel, &maxLabel);
            slider->SetLimitLabels(minLabel != NULL && minLabel[0] != '\0' ? minLabel : detail::NumberText(low).c_str(),
                maxLabel != NULL && maxLabel[0] != '\0' ? maxLabel : detail::NumberText(high).c_str());
            slider->SetEndTexts(minLabel, maxLabel);
            slider->SetHashMarks(B_HASH_MARKS_BOTTOM);
            slider->SetHashMarkCount((int32)std::min(positions, 10.0) + 1);
            slider->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 20, B_SIZE_UNSET));
            if (help != NULL)
                slider->SetToolTip(help);
            BStringView* name = new BStringView("sen:label", label.c_str());
            name->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
            layout->AddView(name, 0, *row);
            layout->AddView(slider, 1, *row);
            (*row)++;
            return;
        }
        // (a spinner for the integers that a BSpinner holds, which are 32 bit)
        if (!isInteger || (low >= -2147483648.0 && high <= 2147483647.0)) {
            BAbstractSpinner* spinner;
            // (no label of its own: the label stands in the first column like the others, and the spinner in a line of its own that is as wide as the numbers need,
            // at the left of the column, so that it does not grow with the window)
            if (isInteger) {
                BSpinner* whole = new BSpinner(path.c_str(), "", changed);
                whole->SetRange((int32)low, (int32)high);
                whole->SetValue((int32)number);
                spinner = whole;
            } else {
                BDecimalSpinner* decimal = new BDecimalSpinner(path.c_str(), "", changed);
                decimal->SetRange(low, high);
                decimal->SetStep(step);
                decimal->SetPrecision((uint32)decimals);
                decimal->SetValue(number);
                spinner = decimal;
            }
            spinner->SetEnabled(fEditable);
            // (the arrows up and down of the system's spinner, not a plus and a minus)
            spinner->SetButtonStyle(SPINNER_BUTTON_VERTICAL_ARROWS);
            if (help != NULL)
                spinner->SetToolTip(help);
            const float digits = std::max<float>(6, std::max(detail::NumberText(low).size(), detail::NumberText(high).size()) + 2);
            spinner->SetExplicitMaxSize(BSize(be_plain_font->StringWidth("0") * digits + 3 * be_plain_font->Size() + 16, B_SIZE_UNSET));
            BGroupView* holder = new BGroupView(B_HORIZONTAL, 0);
            holder->GroupLayout()->SetInsets(0, 0, 0, 0);
            holder->GroupLayout()->AddView(spinner);
            holder->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
            holder->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_CENTER));
            BStringView* spinnerLabel = new BStringView("sen:label", label.c_str());
            spinnerLabel->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
            layout->AddView(spinnerLabel, 0, *row);
            layout->AddView(holder, 1, *row);
            (*row)++;
            return;
        }
        delete changed;
    }

    // the choices as radio buttons
    if (hasChoices && kind == form::kKindRadio) {
        BGroupView* group = new BGroupView(path.c_str(), B_VERTICAL, 0);
        for (int32 i = 0; i < choiceCount; i++) {
            const char* value = attributes.GetString(form::kAttrChoices, i, "");
            if (strcmp(value, form::kChoiceSeparator) == 0)
                continue;
            const char* shownAs = attributes.GetString(form::kAttrChoiceLabels, i, value);
            BMessage* chosen = new BMessage(kMsgChoice);
            chosen->AddString("path", path.c_str());
            chosen->AddString("value", value);
            BRadioButton* button = new BRadioButton("sen:radio", shownAs, chosen);
            button->SetValue(text == value ? B_CONTROL_ON : B_CONTROL_OFF);
            button->SetEnabled(fEditable);
            group->GroupLayout()->AddView(button);
        }
        if (help != NULL)
            group->SetToolTip(help);
        BStringView* name = new BStringView("sen:label", label.c_str());
        name->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
        layout->AddView(name, 0, *row);
        layout->AddView(group, 1, *row);
        (*row)++;
        return;
    }

    // a choice of the schema: a menu
    if (hasChoices) {
        BPopUpMenu* menu = new BPopUpMenu(path.c_str());
        menu->SetRadioMode(true);
        menu->SetLabelFromMarked(true);
        bool found = false;
        for (int32 i = 0; i < choiceCount; i++) {
            const char* value = attributes.GetString(form::kAttrChoices, i, "");
            // a choice "-" is a line between the choices (the presets and the choice of one's own)
            if (strcmp(value, form::kChoiceSeparator) == 0) {
                menu->AddSeparatorItem();
                continue;
            }
            const char* shownAs = attributes.GetString(form::kAttrChoiceLabels, i, value);
            BMessage* chosen = new BMessage(kMsgChoice);
            chosen->AddString("path", path.c_str());
            chosen->AddString("value", value);
            BMenuItem* item = new BMenuItem(shownAs, chosen);
            if (text == value) {
                item->SetMarked(true);
                found = true;
            }
            menu->AddItem(item);
        }
        if (!found) {
            // the value is none of the choices: shown, and kept until another is chosen
            BMenuItem* item = new BMenuItem(text.c_str(), NULL);
            item->SetEnabled(false);
            item->SetMarked(true);
            menu->AddItem(item, 0);
        }
        BMenuField* menuField = new BMenuField(path.c_str(), label.c_str(), menu);
        menuField->SetEnabled(fEditable);
        if (help != NULL)
            menuField->SetToolTip(help);
        BLayoutItem* menuLabel = menuField->CreateLabelLayoutItem();
        BLayoutItem* menuBar = menuField->CreateMenuBarLayoutItem();
        // (the menu takes the width of its column: the window can be made wider, and the controls follow)
        menuBar->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
        menuBar->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_UNSET));
        layout->AddItem(menuLabel, 0, *row);
        layout->AddItem(menuBar, 1, *row);
        (*row)++;
        return;
    }

    BMessage* kept = new BMessage(kMsgText);
    kept->AddString("path", path.c_str());
    // a password field is made as the E-mail preferences make theirs: HideTyping() first, because it empties the field, then the text, which shows as bullets
    const bool secret = kind == form::kKindSecret;
    BTextControl* textControl = new BTextControl(path.c_str(), label.c_str(), secret ? "" : text.c_str(), kept);
    // a field is wide enough to edit in
    textControl->TextView()->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 20, B_SIZE_UNSET));
    textControl->TextView()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    if (secret) {
        textControl->TextView()->HideTyping(true);
        textControl->SetText(text.c_str());
    }
    textControl->SetEnabled(fEditable);
    if (help != NULL)
        textControl->SetToolTip(help);
    BLayoutItem* textLabel = textControl->CreateLabelLayoutItem();
    BLayoutItem* textItem = textControl->CreateTextViewLayoutItem();
    textItem->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_VERTICAL_UNSET));
    layout->AddItem(textLabel, 0, *row);
    layout->AddItem(textItem, 1, *row);
    (*row)++;
    fTextPaths.push_back(path);
}


// A message that is shown as a table: a label and the table, over the whole width of the pane.
void ShojiMessageView::_AddTable(BGridView* pane, int32* row, const std::string& node, const std::string& key, const std::string& name, const std::string& kind)
{
    BGridLayout* layout = pane->GridLayout();
    const BMessage attributes = _SchemaOf(ChildPath(key, name));
    const std::string label = _LabelOf(attributes, name);
    const char* help = NULL;
    attributes.FindString(form::kAttrHelp, &help);

    Table entry;
    entry.path = ChildPath(node, name);
    entry.node = node;
    entry.name = name;
    entry.kind = kind;

    ShojiTableControl* table = new ShojiTableControl(entry.path.c_str());
    table->SetMessage(new BMessage(kMsgTable));
    table->SetEditable(fEditable);
    table->SetReorderable(kind == "table" && attributes.GetBool(form::kAttrReorderable, false));
    std::map<std::string, std::vector<float> >::const_iterator widths = fTableWidths.find(entry.path);
    if (widths != fTableWidths.end())
        table->SetUserWidths(widths->second);
    if (help != NULL)
        table->SetToolTip(help);
    fTables.push_back(entry);
    _FillTable(table, entry);

    BStringView* title = new BStringView("sen:label", label.c_str());
    title->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
    layout->AddView(title, 0, *row, 2, 1);
    (*row)++;
    layout->AddView(table, 0, *row, 2, 1);
    (*row)++;
}


// The columns and the rows of a table from the working copy. `table`: a row for each message of the array, a column for each field that they have (the order and the
// label of the column are those of the schema of the field: `programs.level`). `properties`: a row for each field of the message, the name and the value.
// A cell takes its kind from the value and the schema of its field: a check box for a bool, a menu if the schema gives choices, else a text; `readonly` in the schema makes a
// cell that only shows; `enabled_when` is read in the message of the row, so that a cell can wait for another of its row.
void ShojiMessageView::_FillTable(ShojiTableControl* control, const Table& table)
{
    BMessage message;
    if (!NodeIn(fWorking, table.node, &message))
        return;
    const std::string nodeKey = table.node.empty() ? std::string() : _SchemaKey(table.node);
    const std::string tableKey = ChildPath(nodeKey, table.name);
    const BMessage tableAttributes = _SchemaOf(tableKey);
    const FieldInfo info = detail::InfoOf(message);
    type_code type = 0;
    int32 count = 0;
    message.GetInfo(table.name.c_str(), &type, &count);

    // the cell of a field of a message (`ownerPath` is the path of the message)
    auto cellFor = [this, &tableKey](const BMessage& owner, const std::string& ownerPath, const std::string& field) {
        ShojiTableControl::Cell cell;
        const BMessage attributes = _SchemaOf(ChildPath(tableKey, field));
        type_code have = 0;
        int32 values = 0;
        if (owner.GetInfo(field.c_str(), &have, &values) != B_OK)
            return cell;
        if (!detail::IsText(have)) {
            cell.text = std::string("(") + TypeName(have) + ")";
            return cell;
        }
        if (values != 1) {
            cell.text = "(" + std::to_string(values) + " values)";
            return cell;
        }
        const std::string segment = SegmentFor(field, 0, 1, detail::InfoOf(owner));
        std::string text;
        if (segment.empty() || GetText(owner, EscapeSegment(segment), &text) != B_OK) {
            cell.text = "(not editable here)";
            return cell;
        }
        cell.path = ChildPath(ownerPath, segment);
        cell.text = text;
        cell.editable = !attributes.GetBool(form::kAttrReadOnly, false);
        // (a spinner before the text while the row is busy: the schema names the field of the row that says so)
        const char* busyField = NULL;
        if (attributes.FindString(form::kAttrBusyField, &busyField) == B_OK && busyField != NULL)
            cell.busy = owner.GetBool(busyField, false);
        const char* rule = NULL;
        if (attributes.FindString(form::kAttrEnabledWhen, &rule) == B_OK && rule != NULL)
            cell.enabled = ConditionHolds(rule, owner);

        type_code choicesType = 0;
        int32 choiceCount = 0;
        if (have == B_BOOL_TYPE) {
            cell.kind = ShojiTableControl::kBool;
        } else if (attributes.GetInfo(form::kAttrChoices, &choicesType, &choiceCount) == B_OK && choicesType == B_STRING_TYPE && choiceCount > 0) {
            cell.kind = ShojiTableControl::kChoice;
            for (int32 i = 0; i < choiceCount; i++) {
                const char* value = attributes.GetString(form::kAttrChoices, i, "");
                if (strcmp(value, form::kChoiceSeparator) == 0)
                    continue;
                cell.choices.push_back(ShojiTableControl::Choice{value, attributes.GetString(form::kAttrChoiceLabels, i, value)});
            }
        }
        return cell;
    };

    // the fields of a message that a table shows, in the order of the schema (else of the message)
    auto fieldsOf = [this, &tableKey](const BMessage& owner, std::vector<std::string>* names) {
        char* name = NULL;
        type_code have = 0;
        for (int32 i = 0; owner.GetInfo(B_ANY_TYPE, i, &name, &have, NULL) == B_OK; i++) {
            if (have == B_MESSAGE_TYPE || _SchemaOf(ChildPath(tableKey, name)).GetBool(form::kAttrHidden, false))
                continue;
            if (std::find(names->begin(), names->end(), name) == names->end())
                names->push_back(name);
        }
    };
    auto sortByOrder = [this, &tableKey](std::vector<std::string>* names) {
        std::vector<std::pair<double, std::string> > ordered;
        for (size_t i = 0; i < names->size(); i++)
            ordered.push_back(std::make_pair((double)_SchemaOf(ChildPath(tableKey, (*names)[i])).GetInt32(form::kAttrOrder, (int32)i), (*names)[i]));
        std::stable_sort(ordered.begin(), ordered.end(), [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) { return a.first < b.first; });
        for (size_t i = 0; i < names->size(); i++)
            (*names)[i] = ordered[i].second;
    };

    std::vector<std::string> labels;
    std::vector<ShojiTableControl::Row> rows;

    if (table.kind == form::kKindProperties) {
        labels.push_back(tableAttributes.GetString(form::kAttrColumnLabels, 0, "Name"));
        labels.push_back(tableAttributes.GetString(form::kAttrColumnLabels, 1, "Value"));
        BMessage nested;
        const std::string segment = SegmentFor(table.name, 0, (size_t)count, info);
        if (count == 1 && !segment.empty() && message.FindMessage(table.name.c_str(), 0, &nested) == B_OK) {
            const std::string ownerPath = ChildPath(table.node, segment);
            std::vector<std::string> names;
            fieldsOf(nested, &names);
            sortByOrder(&names);
            for (const std::string& field : names) {
                ShojiTableControl::Row row;
                ShojiTableControl::Cell name;
                name.text = _LabelOf(_SchemaOf(ChildPath(tableKey, field)), field);
                row.push_back(name);
                row.push_back(cellFor(nested, ownerPath, field));
                rows.push_back(row);
            }
        }
    } else {
        std::vector<std::string> names;
        for (int32 i = 0; i < count; i++) {
            BMessage each;
            if (message.FindMessage(table.name.c_str(), i, &each) == B_OK)
                fieldsOf(each, &names);
        }
        if (names.empty()) {
            // (no rows: the columns are those that the schema names under the table, `programs.name`, and are not hidden)
            for (const std::pair<const std::string, BMessage>& entry : fAttributes) {
                if (entry.first.compare(0, tableKey.size() + 1, tableKey + ".") != 0)
                    continue;
                const std::string column = entry.first.substr(tableKey.size() + 1);
                if (!column.empty() && column.find('.') == std::string::npos && !entry.second.GetBool(form::kAttrHidden, false))
                    names.push_back(column);
            }
        }
        sortByOrder(&names);
        for (const std::string& field : names)
            labels.push_back(_LabelOf(_SchemaOf(ChildPath(tableKey, field)), field));

        for (int32 i = 0; i < count; i++) {
            // a row whose message cannot be named cannot be edited: it is not shown
            const std::string segment = SegmentFor(table.name, (size_t)i, (size_t)count, info);
            BMessage each;
            if (segment.empty() || message.FindMessage(table.name.c_str(), i, &each) != B_OK)
                continue;
            const std::string ownerPath = ChildPath(table.node, segment);
            ShojiTableControl::Row row;
            for (const std::string& field : names)
                row.push_back(cellFor(each, ownerPath, field));
            rows.push_back(row);
        }
    }

    control->SetColumns(labels);
    control->SetRows(rows);
}


void ShojiMessageView::_RefreshTables(const std::string& path)
{
    const std::vector<Table> tables = fTables;
    for (const Table& table : tables) {
        if (!path.empty() && !InTable(path, table.path))
            continue;
        ShojiTableControl* control = dynamic_cast<ShojiTableControl*>(ControlFor(table.path.c_str()));
        if (control != NULL)
            _FillTable(control, table);
    }
}


// A value that was typed or chosen: checked against its type and the range of the schema, put into the working copy, and told to the target. What is shown is
// the value as it is stored (007 is 7). A value that does not fit is refused and the control shows the old one.
void ShojiMessageView::_ApplyText(const std::string& path, const std::string& text)
{
    if (!fEditable) {
        _Show(path);
        return;
    }

    std::string before;
    type_code type = 0;
    size_t count = 0;
    if (GetText(fWorking, path, &before) != B_OK || FieldOf(fWorking, path, &type, &count) != B_OK) {
        _Refuse(path, text, B_BAD_VALUE);
        return;
    }

    // the range, if the schema gives one
    const BMessage attributes = _SchemaOf(_SchemaKey(path));
    double number = 0;
    if (type != B_STRING_TYPE && type != B_BOOL_TYPE && detail::ParseDouble(text, &number)) {
        double limit = 0;
        if ((attributes.FindDouble(form::kAttrMin, &limit) == B_OK && number < limit)
                || (attributes.FindDouble(form::kAttrMax, &limit) == B_OK && number > limit)) {
            _Refuse(path, text, B_BAD_VALUE);
            return;
        }
    }

    BMessage changed(fWorking);
    status_t result = SetText(&changed, path, text);
    if (result != B_OK) {
        _Refuse(path, text, result);
        return;
    }
    fWorking = changed;

    std::string after;
    GetText(fWorking, path, &after);
    _Show(path);
    _UpdateEnabled();
    if (after != before)
        _Changed(path.c_str(), after.c_str());
}


// A list was changed in the list control: all values of the field are set at once.
void ShojiMessageView::_ApplyList(const std::string& node, const std::string& name)
{
    const std::string path = ChildPath(node, name);
    ShojiListControl* list = dynamic_cast<ShojiListControl*>(ControlFor(path.c_str()));
    if (list == NULL)
        return;
    if (!fEditable) {
        _ShowList(node, name);
        return;
    }

    type_code type = 0;
    for (const Known& known : fKnown[node]) {
        if (known.name == name)
            type = known.type;
    }
    const std::string joined = list->ListText().String();
    const std::vector<std::string> texts = sen::list::Split(joined, kListSeparator);

    BMessage changed(fWorking);
    status_t result = SetValues(&changed, node, name, type, texts);
    if (result != B_OK) {
        _Refuse(path, joined, result);
        _ShowList(node, name);
        return;
    }
    fWorking = changed;
    _Changed(path.c_str(), sen::list::Join(texts, kListSeparator).c_str());
}


void ShojiMessageView::_Refuse(const std::string& path, const std::string& text, status_t status)
{
    _Show(path);

    BMessage refused(form::kRefused);
    refused.AddString(form::kFieldPath, path.c_str());
    refused.AddString(form::kFieldValue, text.c_str());
    refused.AddInt32(form::kFieldStatus, status);
    refused.AddString(form::kFieldDetail, status == B_BAD_VALUE ? "The value does not fit." : strerror(status));
    refused.AddInt32(form::kFieldProtocol, form::kProtocol);
    if (fTarget.IsValid())
        fTarget.SendMessage(&refused);
}


// the control of a value shows the value that the working copy has
void ShojiMessageView::_Show(const std::string& path)
{
    // a cell of a table: the rows are made again from the working copy
    for (const Table& table : fTables) {
        if (InTable(path, table.path)) {
            _RefreshTables(path);
            return;
        }
    }

    BView* control = ControlFor(path.c_str());
    std::string text;
    if (control == NULL || GetText(fWorking, path, &text) != B_OK)
        return;

    if (BTextControl* textControl = dynamic_cast<BTextControl*>(control)) {
        if (text != textControl->Text())
            textControl->SetText(text.c_str());
    } else if (BCheckBox* checkBox = dynamic_cast<BCheckBox*>(control)) {
        checkBox->SetValue(text == "true" ? B_CONTROL_ON : B_CONTROL_OFF);
    } else if (BSpinner* spinner = dynamic_cast<BSpinner*>(control)) {
        const int32 value = (int32)strtol(text.c_str(), NULL, 10);
        if (spinner->Value() != value)
            spinner->SetValue(value);
    } else if (BDecimalSpinner* decimal = dynamic_cast<BDecimalSpinner*>(control)) {
        const double value = strtod(text.c_str(), NULL);
        if (decimal->Value() != value)
            decimal->SetValue(value);
    } else if (ValueSlider* slider = dynamic_cast<ValueSlider*>(control)) {
        slider->SetNumber(strtod(text.c_str(), NULL));
    } else if (ShojiDateControl* date = dynamic_cast<ShojiDateControl*>(control)) {
        if (ShojiDateControl::IsDateText(text.c_str()) && date->DateText() != text.c_str())
            date->SetDateText(text.c_str());
    } else if (BGroupView* radio = dynamic_cast<BGroupView*>(control)) {
        for (int32 i = 0; i < radio->CountChildren(); i++) {
            BRadioButton* button = dynamic_cast<BRadioButton*>(radio->ChildAt(i));
            if (button != NULL && button->Message() != NULL)
                button->SetValue(text == button->Message()->GetString("value", "") ? B_CONTROL_ON : B_CONTROL_OFF);
        }
    }
}


// A control whose schema has `enabled_when` is enabled while the condition holds, and only if the form can be edited at all. The condition is `key=value` (the field `key` of the
// same message has that value), several of them with `&` (all hold) and alternatives with `|` (one holds): `mode=custom&flag=true|mode=other`.
void ShojiMessageView::_UpdateEnabled()
{
    BMessage message;
    if (fPane == NULL || !NodeIn(fWorking, fNode, &message))
        return;
    const std::string key = fNode.empty() ? std::string() : _SchemaKey(fNode);
    const std::vector<Known>& known = fKnown[fNode];
    for (size_t i = 0; i < known.size(); i++) {
        const BMessage attributes = _SchemaOf(ChildPath(key, known[i].name));
        const char* rule = NULL;
        if (attributes.FindString(form::kAttrEnabledWhen, &rule) != B_OK || rule == NULL)
            continue;

        bool enabled = ConditionHolds(rule, message);
        enabled = enabled && fEditable;

        BView* control = ControlFor(ChildPath(fNode, known[i].name).c_str());
        if (BControl* each = dynamic_cast<BControl*>(control))
            each->SetEnabled(enabled);
        else if (BMenuField* menuField = dynamic_cast<BMenuField*>(control))
            menuField->SetEnabled(enabled);
    }
}


void ShojiMessageView::_ShowList(const std::string& node, const std::string& name)
{
    ShojiListControl* list = dynamic_cast<ShojiListControl*>(ControlFor(ChildPath(node, name).c_str()));
    BMessage message;
    if (list == NULL || !NodeIn(fWorking, node, &message))
        return;

    const FieldInfo info = detail::InfoOf(message);
    type_code type = 0;
    int32 values = 0;
    std::vector<std::string> texts;
    if (message.GetInfo(name.c_str(), &type, &values) == B_OK) {
        for (int32 i = 0; i < values; i++) {
            std::string text;
            const std::string segment = SegmentFor(name, (size_t)i, (size_t)values, info);
            if (!segment.empty() && GetText(message, EscapeSegment(segment), &text) == B_OK)
                texts.push_back(text);
        }
    }
    list->SetListText(sen::list::Join(texts, kListSeparator).c_str());
}


// the data of the owner: `settings`, and the optional schema, defaults, target, editable and root_label (SenSettingsForm.h)
void ShojiMessageView::_HandleSetData(BMessage* message)
{
    BMessenger target;
    message->FindMessenger(form::kFieldTarget, &target);

    // a protocol that is newer than this form: not guessed at, the owner is told
    const int32 protocol = message->GetInt32(form::kFieldProtocol, form::kProtocol);
    BMessage settings;
    status_t result = protocol > form::kProtocol ? B_NOT_SUPPORTED : message->FindMessage(form::kFieldSettings, &settings);
    if (result != B_OK) {
        if (result != B_NOT_SUPPORTED)
            result = B_BAD_VALUE;
        if (target.IsValid()) {
            BMessage refused(form::kRefused);
            refused.AddInt32(form::kFieldStatus, result);
            refused.AddString(form::kFieldDetail, result == B_NOT_SUPPORTED ? "The form does not know this protocol." : "The data has no settings.");
            refused.AddInt32(form::kFieldProtocol, form::kProtocol);
            target.SendMessage(&refused);
        }
        _Reply(message, result);
        return;
    }

    BMessage schema, defaults;
    const bool hasSchema = message->FindMessage(form::kFieldSchema, &schema) == B_OK;
    const bool hasDefaults = message->FindMessage(form::kFieldDefaults, &defaults) == B_OK;
    SetTarget(target);
    const char* rootLabel;
    if (message->FindString(form::kFieldRootLabel, &rootLabel) == B_OK) {
        fRootLabel = rootLabel;
    }
    fEditable = message->GetBool(form::kFieldEditable, true);
    SetData(settings, hasSchema ? &schema : NULL, hasDefaults ? &defaults : NULL);
    _Reply(message, B_OK);
}


// answers a message that is waited for
void ShojiMessageView::_Reply(BMessage* message, status_t result)
{
    if (!message->IsSourceWaiting())
        return;
    BMessage reply(B_REPLY);
    reply.AddInt32(form::kFieldResult, result);
    reply.AddInt32(form::kFieldProtocol, form::kProtocol);
    message->SendReply(&reply);
}


// A row of a table was moved (the table that the schema calls `reorderable`): the rows of the array are moved the same in the working copy, and the target is told the whole
// message, in which the array has the new order (a change of one value does not tell an order). The rows are made again, because their cells are named by their row.
void ShojiMessageView::_ReorderRows(const std::string& tablePath, int32 from, int32 to)
{
    for (const Table& table : fTables) {
        if (table.path != tablePath || table.kind != "table")
            continue;
        BMessage changed(fWorking);
        if (fEditable && MoveValue(&changed, table.node, table.name, from, to) == B_OK) {
            fWorking = changed;
            _RefreshTables(std::string());
            _Changed(NULL, NULL);
        } else {
            _RefreshTables(std::string());
        }
        return;
    }
}


// tells the target of a change of one value, or (no path) of the whole message
void ShojiMessageView::_Changed(const char* path, const char* value)
{
    if (!fTarget.IsValid())
        return;

    BMessage changed(form::kChanged);
    if (path != NULL) {
        changed.AddString(form::kFieldPath, path);
        changed.AddString(form::kFieldValue, value != NULL ? value : "");
    }
    changed.AddMessage(form::kFieldSettings, &fWorking);
    changed.AddInt32(form::kFieldProtocol, form::kProtocol);
    fTarget.SendMessage(&changed);
}
