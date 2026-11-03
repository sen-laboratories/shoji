/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiMessageView.h"

#include <Alignment.h>
#include <CheckBox.h>
#include <GridLayout.h>
#include <DecimalSpinner.h>
#include <GridView.h>
#include <ListItem.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <OutlineListView.h>
#include <PopUpMenu.h>
#include <RadioButton.h>
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
#include <stdio.h>
#include <string.h>

#include <sen/SenList.h>
#include <sen/SenMessagePath.h>
#include <sen/SenSettingsForm.h>

#include "../app/ShojiDateControl.h"
#include "../app/ShojiListControl.h"

using namespace sen::settings;
namespace form = sen::settings::form;

namespace {

// what the controls tell the view (it is their target)
enum {
    kMsgText    = 'MVtx',       // a text field was kept (Enter, or the field was left): path
    kMsgBool    = 'MVbo',       // a check box: path
    kMsgChoice  = 'MVch',       // an item of a menu: path, value
    kMsgList    = 'MVls',       // a list changed: node, name
    kMsgNumber  = 'MVnu',       // a spinner or a slider: path
    kMsgDate    = 'MVda',       // a date: path
    kMsgSelect  = 'MVse'        // a node of the tree was selected
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

    virtual const char* UpdateText() const
    {
        fText = ValueText();
        return fText.c_str();
    }

private:
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


bool HasNestedMessages(const BMessage& message)
{
    char* name = NULL;
    type_code type = 0;
    for (int32 i = 0; message.GetInfo(B_ANY_TYPE, i, &name, &type, NULL) == B_OK; i++) {
        if (type == B_MESSAGE_TYPE)
            return true;
    }
    return false;
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

    if (!HasNestedMessages(fWorking)) {
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
        if (attributes.GetBool(form::kAttrHidden, false))
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
    _FillPane(pane, path);

    BView* old = fPane;
    fPaneHolder->GroupLayout()->RemoveView(old);
    old->RemoveSelf();
    delete old;
    fPane = pane;
    fPaneHolder->GroupLayout()->AddView(pane);
    if (Window() != NULL)
        Connect(pane, this);

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
            Known   field;
            double  order;
        };
        std::vector<Shown> shown;
        const std::vector<Known>& known = fKnown[node];
        for (size_t i = 0; i < known.size(); i++) {
            const BMessage attributes = _SchemaOf(ChildPath(key, known[i].name));
            if (attributes.GetBool(form::kAttrHidden, false))
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
        std::stable_sort(shown.begin(), shown.end(), [](const Shown& a, const Shown& b) { return a.order < b.order; });

        for (const Shown& item : shown)
            _AddField(pane, &row, node, key, item.field, message);
    }

    if (row == 0) {
        BStringView* note = new BStringView("sen:note", "There is nothing to set here.");
        note->SetHighUIColor(B_PANEL_TEXT_COLOR, B_DARKEN_1_TINT);
        layout->AddView(note, 0, row++, 2, 1);
    }
    layout->SetColumnWeight(1, 100.0f);
    // keep the rows together at the top
    layout->AddItem(BSpaceLayoutItem::CreateGlue(), 0, row, 2, 1);
    layout->SetRowWeight(row, 100.0f);
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
            if (segment.empty() || GetText(message, segment, &text) != B_OK)
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
        layout->AddView(list, 1, *row);
        (*row)++;
        return;
    }

    const std::string segment = SegmentFor(field.name, 0, 1, info);
    std::string text;
    if (segment.empty() || GetText(message, segment, &text) != B_OK) {
        readOnly("(not editable here)");
        return;
    }
    const std::string path = ChildPath(node, segment);

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

    const char* kindText = NULL;
    attributes.FindString(form::kAttrKind, &kindText);
    const std::string kind = kindText != NULL ? kindText : "";
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
            slider->SetLimitLabels(detail::NumberText(low).c_str(), detail::NumberText(high).c_str());
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
            if (isInteger) {
                BSpinner* whole = new BSpinner(path.c_str(), label.c_str(), changed);
                whole->SetRange((int32)low, (int32)high);
                whole->SetValue((int32)number);
                spinner = whole;
            } else {
                BDecimalSpinner* decimal = new BDecimalSpinner(path.c_str(), label.c_str(), changed);
                decimal->SetRange(low, high);
                decimal->SetStep(step);
                decimal->SetPrecision((uint32)decimals);
                decimal->SetValue(number);
                spinner = decimal;
            }
            spinner->SetEnabled(fEditable);
            if (help != NULL)
                spinner->SetToolTip(help);
            layout->AddItem(spinner->CreateLabelLayoutItem(), 0, *row);
            layout->AddItem(spinner->CreateTextViewLayoutItem(), 1, *row);
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
        layout->AddItem(menuField->CreateLabelLayoutItem(), 0, *row);
        layout->AddItem(menuField->CreateMenuBarLayoutItem(), 1, *row);
        (*row)++;
        return;
    }

    BMessage* kept = new BMessage(kMsgText);
    kept->AddString("path", path.c_str());
    BTextControl* textControl = new BTextControl(path.c_str(), label.c_str(), text.c_str(), kept);
    // a field is wide enough to edit in
    textControl->TextView()->SetExplicitMinSize(BSize(be_plain_font->StringWidth("M") * 20, B_SIZE_UNSET));
    textControl->TextView()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    if (kind == form::kKindSecret)
        textControl->TextView()->HideTyping(true);
    textControl->SetEnabled(fEditable);
    if (help != NULL)
        textControl->SetToolTip(help);
    layout->AddItem(textControl->CreateLabelLayoutItem(), 0, *row);
    layout->AddItem(textControl->CreateTextViewLayoutItem(), 1, *row);
    (*row)++;
    fTextPaths.push_back(path);
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
            if (!segment.empty() && GetText(message, segment, &text) == B_OK)
                texts.push_back(text);
        }
    }
    list->SetListText(sen::list::Join(texts, kListSeparator).c_str());
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
