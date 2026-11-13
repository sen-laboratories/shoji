/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The form for a BMessage (ShojiMessageView): the controls that are made from the data, what a change in a control does to the working copy and what the
// target is told, the refusal of a value that does not fit, lists, nested messages, Revert and Defaults. Build and run on Haiku, in a session with a desktop:
//   g++ -std=c++20 -Isrc/app -Isrc/message -I/boot/home/config/non-packaged/develop/headers -isystem /boot/system/develop/headers/private/shared
//       -isystem /boot/system/develop/headers/private/interface tests/MessageViewTest.cpp src/message/ShojiMessageView.cpp src/app/ShojiListControl.cpp
//       src/app/ShojiTableControl.cpp src/app/ShojiDateControl.cpp -o /tmp/message-view-test -lbe -lshared && /tmp/message-view-test [screenshot [node] | write file | date file]  (one command)
// With an argument, a window with the form stays for 25 seconds, for a screenshot (the second argument is the node of the tree to show, e.g. `nested.items_01`).

#include "ShojiMessageView.h"
#include "ShojiDateControl.h"
#include "ShojiListControl.h"
#include "ShojiTableControl.h"

#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <File.h>
#include <Node.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <SeparatorItem.h>
#include <StringView.h>
#include <RadioButton.h>
#include <Slider.h>
#include <Spinner.h>
#include <DecimalSpinner.h>
#include <OutlineListView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sen/SenMessagePath.h>
#include <sen/SenSettingsForm.h>

namespace form = sen::settings::form;
using namespace sen::settings;

static int sFailures = 0;
static int sChecks = 0;

#define CHECK(condition) \
    do { \
        sChecks++; \
        if (!(condition)) { \
            sFailures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while (0)

static BMessage
MakeSettings()
{
    BMessage item1, item2, deeper, nested, settings;
    item1.AddString("name", "one");
    item1.AddInt32("n", 1);
    item2.AddString("name", "two");
    item2.AddInt32("n", 2);
    deeper.AddString("deepest", "z");
    nested.AddInt32("depth", 2);
    nested.AddString("label", "deep");
    nested.AddMessage("items", &item1);
    nested.AddMessage("items", &item2);
    nested.AddMessage("deeper", &deeper);

    settings.AddString("email", "a@b.c");
    settings.AddBool("unpaywall", true);
    settings.AddInt32("max_authors", 10);
    settings.AddDouble("ratio", 0.5);
    settings.AddString("mode", "fast");
    settings.AddString("key", "secret-key");
    settings.AddString("tags", "x");
    settings.AddString("tags", "y");
    settings.AddMessage("nested", &nested);
    settings.AddData("blob", B_RAW_TYPE, "12345", 5);
    settings.AddColor("accent", (rgb_color){10, 20, 30, 255});
    settings.AddInt32("level", 5);
    settings.AddDouble("gain", 0.5);
    settings.AddInt32("volume", 40);
    settings.AddString("speed", "normal");
    const time_t released = 86400 * 11017;
    settings.AddData("released", B_TIME_TYPE, &released, sizeof(released));
    const time_t stamp = 86400 * 11017 + 3600 * 14;
    settings.AddData("stamp", B_TIME_TYPE, &stamp, sizeof(stamp));
    return settings;
}

static BMessage
MakeSchema()
{
    BMessage email, maxAuthors, mode, key, tags, nestedLabel, hidden, list, level, gain, volume, speed;
    email.AddString(form::kAttrLabel, "E-mail address");
    email.AddString(form::kAttrHelp, "Sent to services that ask who you are.");
    maxAuthors.AddDouble(form::kAttrMin, 1);
    maxAuthors.AddDouble(form::kAttrMax, 100);
    maxAuthors.AddString(form::kAttrKind, form::kKindText);        // (a range with a text field: the typed value is checked)
    level.AddDouble(form::kAttrMin, 1);
    level.AddDouble(form::kAttrMax, 10);
    gain.AddDouble(form::kAttrMin, 0);
    gain.AddDouble(form::kAttrMax, 1);
    gain.AddDouble(form::kAttrStep, 0.05);
    gain.AddString(form::kAttrKind, form::kKindSlider);
    volume.AddDouble(form::kAttrMin, 0);
    volume.AddDouble(form::kAttrMax, 100);
    volume.AddString(form::kAttrKind, form::kKindSlider);
    speed.AddString(form::kAttrKind, form::kKindRadio);
    speed.AddString(form::kAttrChoices, "slow");
    speed.AddString(form::kAttrChoices, "normal");
    speed.AddString(form::kAttrChoices, "fast");
    mode.AddString(form::kAttrChoices, "fast");
    mode.AddString(form::kAttrChoices, "careful");
    mode.AddString(form::kAttrChoiceLabels, "Fast");
    mode.AddString(form::kAttrChoiceLabels, "Careful");
    key.AddString(form::kAttrKind, form::kKindSecret);
    nestedLabel.AddString(form::kAttrLabel, "Nested things");
    hidden.AddBool(form::kAttrHidden, true);
    list.AddBool(form::kAttrList, true);
    BMessage schema;
    schema.AddInt32(form::kSchemaVersion, 1);
    schema.AddMessage("email", &email);
    schema.AddMessage("max_authors", &maxAuthors);
    schema.AddMessage("mode", &mode);
    schema.AddMessage("key", &key);
    schema.AddMessage("level", &level);
    schema.AddMessage("gain", &gain);
    schema.AddMessage("volume", &volume);
    schema.AddMessage("speed", &speed);
    schema.AddMessage("nested", &nestedLabel);
    schema.AddMessage("nested.deeper", &hidden);
    schema.AddMessage("ratio", &hidden);
    schema.AddMessage("nested.ids", &list);
    return schema;
}

static BMessage
MakeDefaults()
{
    BMessage defaults;
    defaults.AddString("email", "");
    defaults.AddBool("unpaywall", false);
    defaults.AddInt32("max_authors", 20);
    defaults.AddString("mode", "fast");
    defaults.AddString("tags", "d");
    return defaults;
}

static const uint32 kShow = 'tshw';

class TestWindow : public BWindow {
public:
    TestWindow()
        :
        BWindow(BRect(100, 100, 600, 500), "message view test", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
        changed(0),
        refused(0),
        whole(0),
        actions(0)
    {
        view = new ShojiMessageView();
        view->SetTarget(BMessenger(this));
        BLayoutBuilder::Group<>(this, B_VERTICAL, 0).Add(view).End();
    }

    virtual void MessageReceived(BMessage* message)
    {
        switch (message->what) {
            case form::kChanged:
            {
                changed++;
                if (message->HasString(form::kFieldPath)) {
                    lastPath = message->GetString(form::kFieldPath, "");
                    lastValue = message->GetString(form::kFieldValue, "");
                } else {
                    whole++;
                }
                BMessage settings;
                if (message->FindMessage(form::kFieldSettings, &settings) == B_OK)
                    lastSettings = settings;
                break;
            }
            case form::kAction:
                actions++;
                lastAction = message->GetString(form::kFieldPath, "");
                break;
            case form::kRefused:
                refused++;
                lastRefusedPath = message->GetString(form::kFieldPath, "");
                break;
            default:
                BWindow::MessageReceived(message);
        }
    }

    /** the notifications and the controls' messages are asynchronous */
    void Settle()
    {
        Unlock();
        snooze(150000);
        Lock();
    }

    /** the user types into a text field and presses Enter */
    void Type(const char* path, const char* text)
    {
        BTextControl* control = dynamic_cast<BTextControl*>(view->ControlFor(path));
        if (control == NULL) {
            printf("no text field %s\n", path);
            sFailures++;
            return;
        }
        control->SetText(text);
        control->Invoke();
        Settle();
    }

    std::string Text(const char* path)
    {
        std::string text = "<none>";
        GetText(view->Settings(), path, &text);
        return text;
    }

    void Reset()
    {
        changed = refused = whole = actions = 0;
        lastAction.clear();
        lastPath.clear();
        lastValue.clear();
    }

    ShojiMessageView*   view;
    int32               changed;
    int32               refused;
    int32               whole;
    int32               actions;
    std::string         lastAction;
    std::string         lastPath;
    std::string         lastValue;
    std::string         lastRefusedPath;
    BMessage            lastSettings;
};

static BTextControl* TextField(TestWindow* w, const char* path) { return dynamic_cast<BTextControl*>(w->view->ControlFor(path)); }

static void
TestControls(TestWindow* w)
{
    CHECK(w->view->CountNodes() > 0);            // there are nested messages: the tree is shown
    BTextControl* email = TextField(w, "email");
    CHECK(email != NULL && strcmp(email->Text(), "a@b.c") == 0);
    CHECK(email != NULL && strcmp(email->Label(), "E-mail address") == 0);        // from the schema
    BCheckBox* unpaywall = dynamic_cast<BCheckBox*>(w->view->ControlFor("unpaywall"));
    CHECK(unpaywall != NULL && unpaywall->Value() == B_CONTROL_ON);
    CHECK(unpaywall != NULL && strcmp(unpaywall->Label(), "Unpaywall") == 0);
    BTextControl* max = TextField(w, "max_authors");
    CHECK(max != NULL && strcmp(max->Text(), "10") == 0);
    CHECK(max != NULL && strcmp(max->Label(), "Max authors") == 0);                // derived from the name
    CHECK(w->view->ControlFor("ratio") == NULL);                                // hidden by the schema
    BMenuField* mode = dynamic_cast<BMenuField*>(w->view->ControlFor("mode"));
    CHECK(mode != NULL && mode->Menu()->CountItems() == 2);
    CHECK(mode != NULL && mode->Menu()->FindMarked() != NULL && strcmp(mode->Menu()->FindMarked()->Label(), "Fast") == 0);
    BTextControl* key = TextField(w, "key");
    CHECK(key != NULL && key->TextView()->IsTypingHidden());
    ShojiListControl* tags = dynamic_cast<ShojiListControl*>(w->view->ControlFor("tags"));
    CHECK(tags != NULL && tags->ListText() == "x;y");
    CHECK(w->view->ControlFor("blob") != NULL);                                    // a line with a note
    CHECK(dynamic_cast<BControl*>(w->view->ControlFor("blob")) == NULL);
    // a colour is a text now (#rrggbb: SenMessagePath.h); a control of its own is for later
    CHECK(TextField(w, "accent") != NULL && strcmp(TextField(w, "accent")->Text(), "#0a141e") == 0);
    CHECK(w->view->ControlFor("nested") == NULL);                                // a node, not a field
}

static void
TestTextAndNumbers(TestWindow* w)
{
    w->Reset();
    w->Type("email", "b@c.d");
    CHECK(w->Text("email") == "b@c.d");
    CHECK(w->changed == 1 && w->lastPath == "email" && w->lastValue == "b@c.d");
    CHECK(w->lastSettings.GetString("email", "") != NULL && strcmp(w->lastSettings.GetString("email", ""), "b@c.d") == 0);
    CHECK(w->view->IsModified());

    // the same text again is no change
    w->Reset();
    w->Type("email", "b@c.d");
    CHECK(w->changed == 0 && w->refused == 0);

    // a number is stored in its type, as it is written then (007 is 7)
    w->Reset();
    w->Type("max_authors", "007");
    CHECK(w->Text("max_authors") == "7");
    CHECK(strcmp(TextField(w, "max_authors")->Text(), "7") == 0);
    int32 value = 0;
    CHECK(w->view->Settings().FindInt32("max_authors", &value) == B_OK && value == 7);
    CHECK(w->changed == 1 && w->lastValue == "7");

    // a word for a number is refused, the control goes back
    w->Reset();
    w->Type("max_authors", "many");
    CHECK(w->refused == 1 && w->lastRefusedPath == "max_authors");
    CHECK(w->changed == 0);
    CHECK(strcmp(TextField(w, "max_authors")->Text(), "7") == 0);
    CHECK(w->Text("max_authors") == "7");

    // the range of the schema
    w->Reset();
    w->Type("max_authors", "500");
    CHECK(w->refused == 1 && w->changed == 0);
    CHECK(w->Text("max_authors") == "7");
    w->Type("max_authors", "0");
    CHECK(w->refused == 2);
    w->Type("max_authors", "100");
    CHECK(w->Text("max_authors") == "100" && w->changed == 1);

    // a key that is hidden is still a string like the others
    w->Type("key", "new-key");
    CHECK(w->Text("key") == "new-key");
}

static void
TestCheckBoxAndChoice(TestWindow* w)
{
    w->Reset();
    BCheckBox* unpaywall = dynamic_cast<BCheckBox*>(w->view->ControlFor("unpaywall"));
    unpaywall->SetValue(B_CONTROL_OFF);
    unpaywall->Invoke();
    w->Settle();
    bool on = true;
    CHECK(w->view->Settings().FindBool("unpaywall", &on) == B_OK && !on);
    CHECK(w->changed == 1 && w->lastPath == "unpaywall" && w->lastValue == "false");

    w->Reset();
    BMenuField* mode = dynamic_cast<BMenuField*>(w->view->ControlFor("mode"));
    // (the item sends its message to the view, which is its target; sent to the view here, as the menu item protects its Invoke)
    BMessage chosen(*mode->Menu()->ItemAt(1)->Message());
    w->view->MessageReceived(&chosen);
    w->Settle();
    CHECK(w->Text("mode") == "careful");
    CHECK(w->changed == 1 && w->lastPath == "mode" && w->lastValue == "careful");
}

static void
TestNativeControls(TestWindow* w)
{
    // a number with a range and no kind: a spinner; a decimal one for a double
    BSpinner* level = dynamic_cast<BSpinner*>(w->view->ControlFor("level"));
    CHECK(level != NULL && level->Value() == 5 && level->MinValue() == 1 && level->MaxValue() == 10);
    w->Reset();
    if (level != NULL) {
        level->SetValue(8);
        w->Settle();
        CHECK(w->Text("level") == "8");
        CHECK(w->changed == 1 && w->lastPath == "level" && w->lastValue == "8");
        level->SetValue(99);                                       // (the spinner keeps to its range)
        w->Settle();
        CHECK(w->Text("level") == "10");
    }

    // a slider if the schema says so, for an integer and for a double with a step
    BSlider* volume = dynamic_cast<BSlider*>(w->view->ControlFor("volume"));
    CHECK(volume != NULL && volume->Value() == 40);
    w->Reset();
    if (volume != NULL) {
        volume->SetValue(75);
        volume->Invoke();
        w->Settle();
        CHECK(w->Text("volume") == "75");
        int32 stored = 0;
        CHECK(w->view->Settings().FindInt32("volume", &stored) == B_OK && stored == 75);
        CHECK(w->changed == 1 && w->lastPath == "volume");
    }
    BSlider* gain = dynamic_cast<BSlider*>(w->view->ControlFor("gain"));
    int32 gainLow = 0, gainHigh = 0;
    if (gain != NULL)
        gain->GetLimits(&gainLow, &gainHigh);
    CHECK(gain != NULL && gain->Value() == 10 && gainHigh == 20);        // 0.5 in steps of 0.05 from 0 to 1
    w->Reset();
    if (gain != NULL) {
        gain->SetValue(3);
        gain->Invoke();
        w->Settle();
        double stored = 0;
        CHECK(w->view->Settings().FindDouble("gain", &stored) == B_OK && fabs(stored - 0.15) < 1e-9);
        CHECK(w->lastPath == "gain");
    }

    // radio buttons for the choices
    BView* speed = w->view->ControlFor("speed");
    CHECK(speed != NULL && dynamic_cast<BMenuField*>(speed) == NULL);
    BRadioButton* fast = NULL;
    int radios = 0;
    for (int32 i = 0; speed != NULL && i < speed->CountChildren(); i++) {
        BRadioButton* button = dynamic_cast<BRadioButton*>(speed->ChildAt(i));
        if (button == NULL)
            continue;
        radios++;
        if (strcmp(button->Label(), "fast") == 0)
            fast = button;
        if (strcmp(button->Label(), "normal") == 0)
            CHECK(button->Value() == B_CONTROL_ON);
    }
    CHECK(radios == 3 && fast != NULL);
    w->Reset();
    if (fast != NULL) {
        fast->SetValue(B_CONTROL_ON);
        fast->Invoke();
        w->Settle();
        CHECK(w->Text("speed") == "fast");
        CHECK(w->changed == 1 && w->lastPath == "speed");
    }

    // a date: the field type says it, no kind; a time of day needs a text field
    ShojiDateControl* released = dynamic_cast<ShojiDateControl*>(w->view->ControlFor("released"));
    CHECK(released != NULL && released->DateText() == "2000-03-01");
    CHECK(w->view->ControlFor("stamp") != NULL && dynamic_cast<ShojiDateControl*>(w->view->ControlFor("stamp")) == NULL);
    CHECK(TextField(w, "stamp") != NULL && strcmp(TextField(w, "stamp")->Text(), "2000-03-01T14:00:00Z") == 0);
    w->Reset();
    if (released != NULL) {
        released->SetDateText("2024-05-06");
        released->Invoke();
        w->Settle();
        CHECK(w->Text("released") == "2024-05-06");
        type_code type; size_t count;
        CHECK(FieldOf(w->view->Settings(), "released", &type, &count) == B_OK && type == B_TIME_TYPE);
        CHECK(w->changed == 1 && w->lastPath == "released" && w->lastValue == "2024-05-06");
    }
    w->Type("stamp", "2001-01-01T08:15:00Z");
    CHECK(w->Text("stamp") == "2001-01-01T08:15:00Z");
    w->Reset();
    w->Type("stamp", "tomorrow");
    CHECK(w->refused == 1);
    CHECK(strcmp(TextField(w, "stamp")->Text(), "2001-01-01T08:15:00Z") == 0);
}

/** the user adds a value to a list: the field of the editor lies over the list and takes Enter */
static void
AddToList(TestWindow* w, ShojiListControl* list, const char* text)
{
    BMessage add(ShojiListControl::kAddValue);
    list->MessageReceived(&add);
    BTextView* editor = dynamic_cast<BTextView*>(w->FindView("value editor"));
    editor->SetText(text);
    BMessage kept(ShojiListControl::kEditorKept);
    list->MessageReceived(&kept);
    w->Settle();
}

static void
TestLists(TestWindow* w)
{
    w->Reset();
    ShojiListControl* tags = dynamic_cast<ShojiListControl*>(w->view->ControlFor("tags"));
    AddToList(w, tags, "z");
    CHECK(w->Text("tags_02") == "z");
    type_code type; size_t count = 0;
    CHECK(FieldOf(w->view->Settings(), "tags", &type, &count) == B_OK && count == 3);
    CHECK(w->changed == 1 && w->lastPath == "tags");

    // a list that is emptied: the field goes from the working copy, the list stays and takes values again
    BListView* values = dynamic_cast<BListView*>(w->FindView("values"));
    for (int i = 0; i < 3; i++) {
        values->Select(0);
        BMessage remove(ShojiListControl::kDeleteValue);
        tags->MessageReceived(&remove);
        w->Settle();
    }
    CHECK(!w->view->Settings().HasString("tags"));
    tags = dynamic_cast<ShojiListControl*>(w->view->ControlFor("tags"));
    CHECK(tags != NULL);
    AddToList(w, tags, "again");
    CHECK(w->Text("tags") == "again");
}

static void
TestNodes(TestWindow* w)
{
    CHECK(w->view->SelectNode("nested"));
    CHECK(w->view->SelectedNode() == "nested");
    CHECK(w->view->ControlFor("email") == NULL);                                // the fields of the other node are not shown
    CHECK(TextField(w, "nested.depth") != NULL && strcmp(TextField(w, "nested.depth")->Text(), "2") == 0);
    CHECK(strcmp(TextField(w, "nested.label")->Text(), "deep") == 0);

    w->Reset();
    w->Type("nested.depth", "5");
    CHECK(w->Text("nested.depth") == "5");
    CHECK(w->changed == 1 && w->lastPath == "nested.depth");
    // the rest of the nested message is as it was
    BMessage nested;
    CHECK(w->view->Settings().FindMessage("nested", &nested) == B_OK);
    CHECK(strcmp(nested.GetString("label", ""), "deep") == 0);
    CHECK(nested.CountNames(B_MESSAGE_TYPE) == 2);

    // a message of an array has a node of its own, with an index in its path
    CHECK(w->view->SelectNode("nested.items_01"));
    CHECK(TextField(w, "nested.items_01.name") != NULL && strcmp(TextField(w, "nested.items_01.name")->Text(), "two") == 0);
    w->Type("nested.items_01.name", "second");
    CHECK(w->Text("nested.items_01.name") == "second");
    CHECK(w->Text("nested.items_00.name") == "one");

    CHECK(!w->view->SelectNode("nested.nope"));

    // a field that no data has is not shown, whatever the schema says: the type is not known (the plugin gives a first value)
    CHECK(w->view->SelectNode("nested"));
    CHECK(w->view->ControlFor("nested.ids") == NULL);
    // the node that is hidden by the schema is not in the tree: `deeper`
    CHECK(!w->view->SelectNode("nested.deeper") || w->view->SelectedNode() == "nested.deeper");
    w->view->SelectNode("");
}

static void
TestRevertAndDefaults(TestWindow* w, const BMessage& original, const BMessage& defaults)
{
    CHECK(w->view->IsModified());
    w->Reset();
    w->view->Revert();
    w->Settle();
    CHECK(w->view->Settings().HasSameData(original, true, true));
    CHECK(!w->view->IsModified());
    CHECK(w->whole == 1 && w->lastSettings.HasSameData(original, true, true));
    CHECK(strcmp(TextField(w, "email")->Text(), "a@b.c") == 0);          // the controls show the data again
    CHECK(dynamic_cast<ShojiListControl*>(w->view->ControlFor("tags"))->ListText() == "x;y");

    w->Reset();
    CHECK(w->view->HasDefaults());
    CHECK(w->view->Defaults() == B_OK);
    w->Settle();
    CHECK(w->view->Settings().HasSameData(defaults, true, true));
    CHECK(w->whole == 1);
    CHECK(w->view->CountNodes() == 0);                                        // the defaults have no nested message: no tree
    CHECK(strcmp(TextField(w, "max_authors")->Text(), "20") == 0);

    w->view->Revert();
    w->Settle();
    CHECK(w->view->Settings().HasSameData(original, true, true));
    CHECK(w->view->CountNodes() > 0);
}

static void
TestReadOnly(TestWindow* w)
{
    w->view->SetEditable(false);
    BTextControl* email = TextField(w, "email");
    CHECK(email != NULL && !email->IsEnabled());
    CHECK(!dynamic_cast<BCheckBox*>(w->view->ControlFor("unpaywall"))->IsEnabled());
    w->Reset();
    // a message that arrives all the same changes nothing
    BMessage text('MVtx');
    text.AddString("path", "email");
    email->SetText("other");
    w->view->MessageReceived(&text);
    w->Settle();
    CHECK(w->Text("email") == "a@b.c");
    CHECK(w->changed == 0);
    CHECK(w->view->Defaults() == B_NOT_ALLOWED);
    w->view->SetEditable(true);
    CHECK(TextField(w, "email")->IsEnabled());
}

static void
TestNoTree()
{
    // a message without a nested message needs no tree
    BMessage plain;
    plain.AddString("name", "n");
    plain.AddInt32("count", 3);
    TestWindow* w = new TestWindow();
    w->Show();
    w->Lock();
    w->view->SetData(plain);
    CHECK(w->view->CountNodes() == 0);
    CHECK(TextField(w, "count") != NULL);
    w->Type("count", "4");
    CHECK(w->Text("count") == "4");
    CHECK(!w->view->HasDefaults());
    CHECK(w->view->Defaults() == B_NOT_SUPPORTED);
    // an empty message says so
    w->view->SetData(BMessage());
    CHECK(w->view->ControlFor("sen:note") != NULL);
    w->Quit();
}

// A field with `enabled_when` ("key=value") can be edited only while another field has that value: the address and the key of a custom mirror (the spatial navigator).
static void
TestEnabledWhen(TestWindow* w)
{
    BMessage settings, schema, mirror, address, key;
    settings.AddString("mirror", "default");
    settings.AddString("address", "");
    settings.AddString("key", "");
    schema.AddInt32(form::kSchemaVersion, 1);
    mirror.AddString(form::kAttrKind, form::kKindChoice);
    mirror.AddString(form::kAttrChoices, "default");
    mirror.AddString(form::kAttrChoices, form::kChoiceSeparator);
    mirror.AddString(form::kAttrChoices, "custom");
    mirror.AddInt32(form::kAttrOrder, 0);
    address.AddString(form::kAttrEnabledWhen, "mirror=custom");
    address.AddInt32(form::kAttrOrder, 1);
    key.AddString(form::kAttrEnabledWhen, "mirror=custom");
    key.AddString(form::kAttrKind, form::kKindSecret);
    key.AddInt32(form::kAttrOrder, 2);
    schema.AddMessage("mirror", &mirror);
    schema.AddMessage("address", &address);
    schema.AddMessage("key", &key);
    w->view->SetData(settings, &schema, NULL);
    w->Settle();

    BMenuField* menu = dynamic_cast<BMenuField*>(w->view->ControlFor("mirror"));
    BTextControl* field = TextField(w, "address");
    BTextControl* secret = TextField(w, "key");
    CHECK(menu != NULL && menu->IsEnabled());
    CHECK(field != NULL && !field->IsEnabled() && secret != NULL && !secret->IsEnabled());		// (the default server is chosen)

    CHECK(dynamic_cast<BSeparatorItem*>(menu->Menu()->ItemAt(1)) != NULL);			// (the line between the servers and the custom mirror)
    BMessage chosen(*menu->Menu()->ItemAt(2)->Message());			// "custom"
    w->view->MessageReceived(&chosen);
    w->Settle();
    field = TextField(w, "address");
    secret = TextField(w, "key");
    CHECK(w->Text("mirror") == "custom");
    CHECK(field != NULL && field->IsEnabled() && secret != NULL && secret->IsEnabled());

    BMessage back(*menu->Menu()->ItemAt(0)->Message());
    w->view->MessageReceived(&back);
    w->Settle();
    CHECK(!TextField(w, "address")->IsEnabled() && !TextField(w, "key")->IsEnabled());

    // revert: the choice is the one of the start again, and so is what is enabled
    w->view->MessageReceived(&chosen);
    w->Settle();
    CHECK(TextField(w, "address")->IsEnabled());
    w->view->Revert();
    w->Settle();
    CHECK(w->Text("mirror") == "default" && !TextField(w, "address")->IsEnabled());

    // conditions: "or" of alternatives, "and" within one (a key is for a server that requires one: the listed ones, or the custom one that has the flag)
    BMessage more(settings), moreSchema(schema), flag, keyOnly(key);
    more.AddBool("flag", false);
    flag.AddInt32(form::kAttrOrder, 3);
    moreSchema.AddMessage("flag", &flag);
    moreSchema.RemoveName("key");
    keyOnly.ReplaceString(form::kAttrEnabledWhen, 0, "mirror=other|mirror=custom&flag=true");
    moreSchema.AddMessage("key", &keyOnly);
    w->view->SetData(more, &moreSchema, NULL);
    w->Settle();
    CHECK(!TextField(w, "key")->IsEnabled());
    BMenuField* menu2 = dynamic_cast<BMenuField*>(w->view->ControlFor("mirror"));
    BMessage custom(*menu2->Menu()->ItemAt(2)->Message());
    w->view->MessageReceived(&custom);
    w->Settle();
    CHECK(!TextField(w, "key")->IsEnabled());			// (custom, but the flag is not set)
    BCheckBox* box = dynamic_cast<BCheckBox*>(w->view->ControlFor("flag"));
    box->SetValue(B_CONTROL_ON);
    box->Invoke();
    w->Settle();
    CHECK(TextField(w, "key")->IsEnabled());			// (custom and flagged)
    BMessage defaultAgain(*menu2->Menu()->ItemAt(0)->Message());
    w->view->MessageReceived(&defaultAgain);
    w->Settle();
    CHECK(!TextField(w, "key")->IsEnabled());			// (the flag stays, the mirror is not custom)
}


static ShojiTableControl* Table(TestWindow* w, const char* path) { return dynamic_cast<ShojiTableControl*>(w->view->ControlFor(path)); }

// a table from an array of messages (`kind = table`) and one from the fields of a message (`kind = properties`)
static void
BuildTableData(BMessage* out, BMessage* outSchema)
{
    BMessage settings, schema;
    const char* names[] = { "Alpha", "Beta", "Gamma" };
    const char* programs[] = { "application/x-vnd.alpha", "application/x-vnd.beta", "application/x-vnd.gamma" };
    const bool allowed[] = { true, true, false };
    const char* levels[] = { "city", "country", "country" };
    for (int i = 0; i < 3; i++) {
        BMessage row;
        row.AddString("name", names[i]);
        row.AddString("program", programs[i]);
        row.AddBool("allowed", allowed[i]);
        row.AddString("level", levels[i]);
        row.AddInt32("limit", 5 + i);
        settings.AddMessage("programs", &row);
    }
    BMessage server;
    server.AddString("host", "example.org");
    server.AddInt32("port", 80);
    server.AddBool("secure", true);
    server.AddString("mode", "fast");
    settings.AddMessage("server", &server);
    settings.AddString("lookup", "ask");
    settings.AddString("a.b", "dotted");

    BMessage programsAttr, nameAttr, programAttr, allowedAttr, levelAttr, limitAttr, serverAttr, portAttr, modeAttr, hostAttr;
    programsAttr.AddString(form::kAttrKind, form::kKindTable);
    programsAttr.AddString(form::kAttrLabel, "Programs");
    programsAttr.AddInt32(form::kAttrOrder, 1);
    nameAttr.AddString(form::kAttrLabel, "Program");
    nameAttr.AddBool(form::kAttrReadOnly, true);
    nameAttr.AddInt32(form::kAttrOrder, 0);
    programAttr.AddBool(form::kAttrHidden, true);
    allowedAttr.AddString(form::kAttrLabel, "Allowed");
    allowedAttr.AddInt32(form::kAttrOrder, 1);
    levelAttr.AddString(form::kAttrLabel, "May know");
    levelAttr.AddInt32(form::kAttrOrder, 2);
    levelAttr.AddString(form::kAttrEnabledWhen, "allowed=true");
    levelAttr.AddString(form::kAttrChoices, "city");
    levelAttr.AddString(form::kAttrChoices, "region");
    levelAttr.AddString(form::kAttrChoices, "country");
    levelAttr.AddString(form::kAttrChoiceLabels, "The city");
    levelAttr.AddString(form::kAttrChoiceLabels, "The region");
    levelAttr.AddString(form::kAttrChoiceLabels, "The country");
    limitAttr.AddDouble(form::kAttrMin, 1);
    limitAttr.AddDouble(form::kAttrMax, 10);
    limitAttr.AddInt32(form::kAttrOrder, 3);
    serverAttr.AddString(form::kAttrKind, form::kKindProperties);
    serverAttr.AddString(form::kAttrLabel, "Server");
    serverAttr.AddInt32(form::kAttrOrder, 2);
    portAttr.AddDouble(form::kAttrMin, 1);
    portAttr.AddDouble(form::kAttrMax, 65535);
    portAttr.AddString(form::kAttrLabel, "Port number");
    modeAttr.AddString(form::kAttrChoices, "fast");
    modeAttr.AddString(form::kAttrChoices, "careful");
    hostAttr.AddString(form::kAttrEnabledWhen, "secure=true");
    schema.AddInt32(form::kSchemaVersion, 1);
    schema.AddMessage("programs", &programsAttr);
    schema.AddMessage("programs.name", &nameAttr);
    schema.AddMessage("programs.program", &programAttr);
    schema.AddMessage("programs.allowed", &allowedAttr);
    schema.AddMessage("programs.level", &levelAttr);
    schema.AddMessage("programs.limit", &limitAttr);
    schema.AddMessage("server", &serverAttr);
    schema.AddMessage("server.port", &portAttr);
    schema.AddMessage("server.mode", &modeAttr);
    schema.AddMessage("server.host", &hostAttr);
    *out = settings;
    *outSchema = schema;
}

// a button (`kind = button`) tells the target that it was pressed and changes nothing; a text field that is `readonly` only shows; `shows` puts such a text beside the button
static void
TestButton(TestWindow* w)
{
    BMessage settings, schema, field;
    settings.AddBool("go", false);
    settings.AddString("note", "Vienna, AT");
    settings.AddString("state", "idle");
    settings.AddInt32("count", 5);
    field.AddString(form::kAttrLabel, "Go now");
    field.AddString(form::kAttrKind, form::kKindButton);
    field.AddString(form::kAttrShows, "note");
    field.AddInt32(form::kAttrOrder, 0);
    schema.AddMessage("go", &field);
    BMessage note;
    note.AddBool(form::kAttrReadOnly, true);
    schema.AddMessage("note", &note);
    BMessage state;
    state.AddString(form::kAttrLabel, "State");
    state.AddBool(form::kAttrReadOnly, true);
    state.AddInt32(form::kAttrOrder, 1);
    schema.AddMessage("state", &state);
    BMessage count;
    count.AddString(form::kAttrLabel, "Count");
    count.AddDouble(form::kAttrMin, 1);
    count.AddDouble(form::kAttrMax, 100);
    count.AddInt32(form::kAttrOrder, 2);
    schema.AddMessage("count", &count);
    w->view->SetData(settings, &schema, NULL);
    w->Settle();
    w->Reset();

    BButton* button = dynamic_cast<BButton*>(w->view->ControlFor("go"));
    CHECK(button != NULL && strcmp(button->Label(), "Go now") == 0);
    BStringView* shown = dynamic_cast<BStringView*>(w->view->ControlFor("note"));
    CHECK(shown != NULL && strcmp(shown->Text(), "Vienna, AT") == 0);                  // (beside the button, in the same line)
    CHECK(shown != NULL && button->Parent() != NULL && shown->Parent() == button->Parent()->Parent());          // (the button is in a line of its own at the right of the column of the labels, the text in the grid next to it)
    BStringView* line = dynamic_cast<BStringView*>(w->view->ControlFor("state"));
    CHECK(line != NULL && strcmp(line->Text(), "idle") == 0);                           // (a line of its own, no text field)
    CHECK(dynamic_cast<BTextControl*>(w->view->ControlFor("state")) == NULL);
    BSpinner* spinner = dynamic_cast<BSpinner*>(w->view->ControlFor("count"));
    CHECK(spinner != NULL && spinner->ButtonStyle() == SPINNER_BUTTON_VERTICAL_ARROWS && spinner->Value() == 5);

    button->Invoke();
    w->Settle();
    CHECK(w->actions == 1 && w->lastAction == "go" && w->changed == 0);                 // (nothing changed in the working copy)
    CHECK(w->Text("go") == "false");
}


// a table with `reorderable` in its schema: the rows are moved (the mouse does it through the same call, as a drag of the system), the working copy has the array in the new order, the target is
// told the whole message, the cells are named by their new rows; a table without the flag does not move
static std::string
RowName(const BMessage& settings, int32 index)
{
    BMessage row;
    return settings.FindMessage("rows", index, &row) == B_OK ? row.GetString("name", "") : std::string("<none>");
}


static void
TestReorder(TestWindow* w)
{
    BMessage settings, schema;
    for (const char* name : {"a", "b", "c"}) {
        BMessage row;
        row.AddString("name", name);
        row.AddBool("on", true);
        settings.AddMessage("rows", &row);
    }
    BMessage rows;
    rows.AddString(form::kAttrKind, form::kKindTable);
    rows.AddBool(form::kAttrReorderable, true);
    schema.AddMessage("rows", &rows);
    w->view->SetData(settings, &schema, NULL);
    w->Settle();
    w->Reset();

    ShojiTableControl* table = Table(w, "rows");
    CHECK(table != NULL && table->IsReorderable() && table->CountRows() == 3);
    CHECK(table != NULL && table->CellAt(0, 0)->text == "a");

    // the first row to the end: the places are those between rows (3 is after the last)
    CHECK(table->MoveRow(0, 3));
    w->Settle();
    CHECK(w->whole == 1 && w->changed == 1);
    CHECK(RowName(w->lastSettings, 0) == "b" && RowName(w->lastSettings, 1) == "c" && RowName(w->lastSettings, 2) == "a");
    CHECK(RowName(w->view->Settings(), 2) == "a");
    table = Table(w, "rows");
    CHECK(table->CellAt(0, 0)->text == "b" && table->CellAt(2, 0)->text == "a");
    CHECK(table->CellAt(0, 1)->path == "rows_00.on");                               // (named by the new row)

    // the last one to the front, and a move that moves nothing
    w->Reset();
    CHECK(table->MoveRow(2, 0));
    w->Settle();
    CHECK(RowName(w->view->Settings(), 0) == "a" && RowName(w->view->Settings(), 1) == "b" && RowName(w->view->Settings(), 2) == "c");
    table = Table(w, "rows");
    w->Reset();
    CHECK(!table->MoveRow(1, 1) && !table->MoveRow(1, 2) && !table->MoveRow(7, 0) && !table->MoveRow(-1, 0));
    w->Settle();
    CHECK(w->changed == 0);

    // MoveValue itself: the other values keep their order, a wrong index changes nothing
    BMessage moved(w->view->Settings());
    CHECK(MoveValue(&moved, "", "rows", 0, 2) == B_OK && RowName(moved, 0) == "b" && RowName(moved, 1) == "c" && RowName(moved, 2) == "a");
    CHECK(MoveValue(&moved, "", "rows", 0, 3) == B_BAD_INDEX && MoveValue(&moved, "", "nothing", 0, 1) == B_NAME_NOT_FOUND);
    CHECK(RowName(moved, 0) == "b");
}


// a table that the schema has and the message has no rows for is shown empty, with the titles of its columns; a cell whose column has `busy_field` shows a spinner while the field of its row is true
static void
TestEmptyAndBusy(TestWindow* w)
{
    BMessage settings, schema;
    settings.AddString("title", "x");
    BMessage rows;
    rows.AddString(form::kAttrKind, form::kKindTable);
    schema.AddMessage("rows", &rows);
    BMessage name;
    name.AddString(form::kAttrLabel, "Name");
    name.AddInt32(form::kAttrOrder, 0);
    schema.AddMessage("rows.name", &name);
    BMessage hidden;
    hidden.AddBool(form::kAttrHidden, true);
    schema.AddMessage("rows.id", &hidden);
    w->view->SetData(settings, &schema, NULL);
    w->Settle();
    ShojiTableControl* table = Table(w, "rows");
    CHECK(table != NULL && table->CountRows() == 0 && table->CountColumns() == 1 && table->ColumnLabel(0) == "Name");

    // the same with two rows, one of them busy
    for (const char* each : {"a", "b"}) {
        BMessage row;
        row.AddString("name", each);
        row.AddString("id", each);
        row.AddBool("busy", strcmp(each, "a") == 0);
        settings.AddMessage("rows", &row);
    }
    BMessage busyColumn;
    busyColumn.AddString(form::kAttrLabel, "Name");
    busyColumn.AddBool(form::kAttrReadOnly, true);
    busyColumn.AddString(form::kAttrBusyField, "busy");
    busyColumn.AddInt32(form::kAttrOrder, 0);
    schema.ReplaceMessage("rows.name", &busyColumn);
    BMessage hiddenBusy;
    hiddenBusy.AddBool(form::kAttrHidden, true);
    schema.AddMessage("rows.busy", &hiddenBusy);
    w->view->SetData(settings, &schema, NULL);
    w->Settle();
    table = Table(w, "rows");
    CHECK(table != NULL && table->CountRows() == 2 && table->CountColumns() == 1);
    CHECK(table != NULL && table->CellAt(0, 0)->busy && !table->CellAt(1, 0)->busy);
}


static void
TestTables(TestWindow* w)
{
    BMessage settings, schema, server;
    BuildTableData(&settings, &schema);
    w->view->SetData(settings, &schema, NULL);
    w->Settle();
    w->Reset();

    // a message that is a table is no node of the tree
    CHECK(w->view->CountNodes() == 0);
    ShojiTableControl* table = Table(w, "programs");
    CHECK(table != NULL && table->CountRows() == 3);
    CHECK(table != NULL && table->CountColumns() == 4);                              // (the program is hidden)
    CHECK(table != NULL && table->ColumnLabel(0) == "Program" && table->ColumnLabel(2) == "May know");
    CHECK(table != NULL && table->ColumnLabel(3) == "Limit");                         // (from the name)
    const ShojiTableControl::Cell* name = table->CellAt(1, 0);
    CHECK(name != NULL && name->text == "Beta" && !name->editable && name->kind == ShojiTableControl::kText);
    const ShojiTableControl::Cell* box = table->CellAt(0, 1);
    CHECK(box != NULL && box->kind == ShojiTableControl::kBool && box->text == "true" && box->editable && box->path == "programs_00.allowed");
    const ShojiTableControl::Cell* level = table->CellAt(1, 2);
    CHECK(level != NULL && level->kind == ShojiTableControl::kChoice && level->choices.size() == 3 && level->choices[1].label == "The region");
    CHECK(level != NULL && level->text == "country" && level->enabled && level->path == "programs_01.level");
    CHECK(table->CellAt(2, 2) != NULL && !table->CellAt(2, 2)->enabled);              // (not allowed: no level)
    const ShojiTableControl::Cell* limit = table->CellAt(2, 3);
    CHECK(limit != NULL && limit->kind == ShojiTableControl::kText && limit->text == "7" && limit->editable);
    int32 row = -1, column = -1;
    CHECK(table->FindCell("programs_02.level", &row, &column) && row == 2 && column == 2);

    // the columns can be given a width with the dividers of the header: the column before the divider changes, a width has a smallest, the others keep theirs
    {
        const float listWidth = 460;
        const std::vector<float> before = table->ColumnEdges(listWidth);
        CHECK(table->DividerAt(before[1], listWidth) == 0 && table->DividerAt(before[1] + 2, listWidth) == 0 && table->DividerAt(before[1] + 10, listWidth) == -1);
        CHECK(table->UserWidths().size() == 4 && table->UserWidths()[0] == 0);
        const float first = before[1] - before[0];
        CHECK(table->ResizeColumn(0, first + 12));
        CHECK(table->UserWidths()[0] == first + 12);
        CHECK(!table->ResizeColumn(9, 100));
        CHECK(table->ResizeColumn(0, 1) && table->UserWidths()[0] >= 16 && table->UserWidths()[0] < first);           // (a little at least)
        const std::vector<float> after = table->ColumnEdges(listWidth);
        CHECK(after[1] - after[0] == table->UserWidths()[0] && after[2] - after[1] == before[2] - before[1]);          // (the next column is as it was)
        std::vector<float> kept = table->UserWidths();
        table->SetUserWidths(std::vector<float>(4, 0));
        CHECK(table->ColumnEdges(listWidth) == before);
        table->SetUserWidths(kept);
        table->SetUserWidths(std::vector<float>(4, 0));
    }

    // a click on a check box turns it over; the row waits for it
    CHECK(table->Activate(0, 1));
    w->Settle();
    CHECK(w->changed == 1 && w->lastPath == "programs_00.allowed" && w->lastValue == "false");
    CHECK(w->Text("programs_00.allowed") == "false");
    table = Table(w, "programs");
    CHECK(table->CellAt(0, 1)->text == "false" && !table->CellAt(0, 2)->enabled);       // (the rows are made again)
    CHECK(!table->Activate(0, 2, "region"));                                           // (disabled)
    CHECK(table->Activate(2, 1, "true"));
    w->Settle();
    table = Table(w, "programs");
    CHECK(table->CellAt(2, 2)->enabled && w->Text("programs_02.allowed") == "true");

    // a choice is set to one of its values only
    w->Reset();
    CHECK(table->Activate(1, 2, "region"));
    w->Settle();
    CHECK(w->Text("programs_01.level") == "region" && w->changed == 1);
    table = Table(w, "programs");
    CHECK(table->CellAt(1, 2)->text == "region");
    CHECK(!table->Activate(1, 2, "planet"));
    CHECK(!table->Activate(1, 0, "x"));                                                // (the name is only shown)
    CHECK(w->Text("programs_01.name") == "Beta");

    // a text: the field over the cell; Enter keeps it, a value out of the range is refused and the cell shows the old one
    w->Reset();
    CHECK(table->BeginEdit(2, 3) && table->IsEditing());
    table->SetEditorText("9");
    table->EndEdit(true);
    w->Settle();
    CHECK(!table->IsEditing() && w->Text("programs_02.limit") == "9" && w->changed == 1);
    table = Table(w, "programs");
    CHECK(table->CellAt(2, 3)->text == "9");
    CHECK(table->BeginEdit(2, 3));
    table->SetEditorText("99");
    table->EndEdit(true);
    w->Settle();
    CHECK(w->refused == 1 && w->lastRefusedPath == "programs_02.limit" && w->Text("programs_02.limit") == "9");
    table = Table(w, "programs");
    CHECK(table->CellAt(2, 3)->text == "9");
    // (Escape takes it back)
    w->Reset();
    CHECK(table->BeginEdit(0, 3));
    table->SetEditorText("3");
    table->EndEdit(false);
    w->Settle();
    CHECK(w->changed == 0 && w->Text("programs_00.limit") == "5");
    // (a window that is closed keeps what is typed)
    CHECK(table->BeginEdit(0, 3));
    table->SetEditorText("4");
    w->view->CommitPending();
    w->Settle();
    CHECK(w->Text("programs_00.limit") == "4" && !Table(w, "programs")->IsEditing());
    CHECK(!table->BeginEdit(0, 0) && !table->BeginEdit(0, 1) && !table->BeginEdit(0, 2));  // (a name, a check box and a menu have no field)

    // the mouse: a click on a cell is the control that edits it (the menu of a choice is shown by the system and not clicked here)
    BListView* rows = dynamic_cast<BListView*>(table->FindView("table rows"));
    CHECK(rows != NULL);
    auto cellPoint = [&](int32 r, int32 c) {
        const BRect item = rows->ItemFrame(r);
        const std::vector<float> edges = table->ColumnEdges(rows->Bounds().Width() + 1);
        return BPoint((edges[c] + edges[c + 1]) / 2, (item.top + item.bottom) / 2);
    };
    w->Reset();
    rows->MouseDown(cellPoint(1, 1));                       // (Beta: the box is on)
    w->Settle();
    CHECK(w->Text("programs_01.allowed") == "false" && w->changed == 1 && table->SelectedRow() == 1);
    rows->MouseDown(cellPoint(1, 1));
    w->Settle();
    CHECK(w->Text("programs_01.allowed") == "true" && w->changed == 2);
    w->Reset();
    rows->MouseDown(cellPoint(0, 2));                       // (the menu of Alpha is disabled: its box is off)
    CHECK(!table->IsEditing() && w->changed == 0);
    rows->MouseDown(cellPoint(0, 0));                       // (a name is only shown)
    CHECK(!table->IsEditing());
    rows->MouseDown(cellPoint(0, 3));                       // (a text: the field is over the cell)
    CHECK(table->IsEditing());
    table->SetEditorText("3");
    rows->MouseDown(cellPoint(1, 1));                       // (a click on another cell keeps it and uses that cell)
    w->Settle();
    CHECK(!table->IsEditing() && w->Text("programs_00.limit") == "3" && w->Text("programs_01.allowed") == "false" && w->changed == 2);
    rows->MouseDown(cellPoint(1, 1));
    w->Settle();
    CHECK(w->Text("programs_01.allowed") == "true");

    // the table of a form that cannot be edited shows the values
    w->view->SetEditable(false);
    w->Settle();
    table = Table(w, "programs");
    CHECK(table != NULL && !table->IsEditable() && !table->Activate(1, 1));
    w->view->SetEditable(true);
    w->Settle();

    // the fields of a message as rows: the name and the value, the value in the control of its kind
    table = Table(w, "server");
    CHECK(table != NULL && table->CountRows() == 4 && table->CountColumns() == 2);
    CHECK(table != NULL && table->ColumnLabel(0) == "Name" && table->ColumnLabel(1) == "Value");
    // (the order of the schema: host, port... have none; they keep the order of the message)
    int32 portRow = -1, secureRow = -1, hostRow = -1, modeRow = -1;
    for (int32 i = 0; i < table->CountRows(); i++) {
        const std::string label = table->CellAt(i, 0)->text;
        if (label == "Port number") portRow = i;
        else if (label == "Secure") secureRow = i;
        else if (label == "Host") hostRow = i;
        else if (label == "Mode") modeRow = i;
    }
    CHECK(portRow >= 0 && secureRow >= 0 && hostRow >= 0 && modeRow >= 0);
    CHECK(table->CellAt(portRow, 1)->kind == ShojiTableControl::kText && table->CellAt(portRow, 1)->path == "server.port" && table->CellAt(portRow, 1)->text == "80");
    CHECK(table->CellAt(secureRow, 1)->kind == ShojiTableControl::kBool && table->CellAt(modeRow, 1)->kind == ShojiTableControl::kChoice);
    CHECK(!table->CellAt(portRow, 0)->editable);
    CHECK(table->CellAt(hostRow, 1)->enabled);                                          // (secure)
    w->Reset();
    CHECK(table->Activate(secureRow, 1, "false"));
    w->Settle();
    table = Table(w, "server");
    CHECK(w->Text("server.secure") == "false" && !table->CellAt(hostRow, 1)->enabled);   // (the host waits for it)
    CHECK(table->BeginEdit(portRow, 1));
    table->SetEditorText("8080");
    table->EndEdit(true);
    w->Settle();
    CHECK(w->Text("server.port") == "8080");
    int32 port = 0;
    CHECK(w->view->Settings().FindMessage("server", &server) == B_OK && server.FindInt32("port", &port) == B_OK && port == 8080);   // (still an int32)

    // the plain fields beside the tables, and a name with a dot (the control is named with the escaped path)
    CHECK(TextField(w, "lookup") != NULL && strcmp(TextField(w, "lookup")->Text(), "ask") == 0);
    CHECK(TextField(w, "a\\.b") != NULL && strcmp(TextField(w, "a\\.b")->Text(), "dotted") == 0);

    // revert: the rows are those of the start
    w->view->Revert();
    w->Settle();
    table = Table(w, "programs");
    CHECK(table->CellAt(0, 1)->text == "true" && table->CellAt(2, 1)->text == "false" && table->CellAt(2, 3)->text == "7");
}


int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.sen-labs.shoji-message-view-test");
    const BMessage original = MakeSettings();
    const BMessage defaults = MakeDefaults();

    // with `date <file>`: an attribute of type time at midnight UTC on a file, for the attribute form of Shoji
    if (argc > 2 && strcmp(argv[1], "date") == 0) {
        BNode node(argv[2]);
        const time_t released = 86400 * 11017;
        return node.InitCheck() == B_OK && node.WriteAttr("released", B_TIME_TYPE, 0, &released, sizeof(released)) == (ssize_t)sizeof(released) ? 0 : 1;
    }

    // with `write <file>`: the settings as a flattened message, for opening the file with Shoji (the type is the one of a settings file)
    if (argc > 2 && strcmp(argv[1], "write") == 0) {
        BFile file(argv[2], B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
        return file.InitCheck() == B_OK && original.Flatten(&file) == B_OK ? 0 : 1;
    }

    TestWindow* window = new TestWindow();
    window->Show();
    window->Lock();
    window->view->SetRootLabel("Test settings");
    const BMessage schema = MakeSchema();
    window->view->SetData(original, &schema, &defaults);
    window->Settle();

    if (argc > 2 && strcmp(argv[1], "screenshot") == 0 && strcmp(argv[2], "tables") == 0) {
        BMessage tableSettings, tableSchema;
        BuildTableData(&tableSettings, &tableSchema);
        window->view->SetData(tableSettings, &tableSchema, NULL);
        window->Settle();
        window->Unlock();
        snooze(40 * 1000000LL);
        window->Lock();
        window->Quit();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "screenshot") == 0) {
        if (argc > 2)
            window->view->SelectNode(argv[2]);
        window->Unlock();
        snooze(25 * 1000000LL);
        window->Lock();
        window->Quit();
        return 0;
    }

    TestControls(window);
    TestTextAndNumbers(window);
    TestCheckBoxAndChoice(window);
    TestNativeControls(window);
    TestLists(window);
    TestNodes(window);
    TestRevertAndDefaults(window, original, defaults);
    TestReadOnly(window);
    TestEnabledWhen(window);
    TestTables(window);
    TestButton(window);
    TestReorder(window);
    TestEmptyAndBusy(window);
    window->Quit();
    TestNoTree();

    printf("%d checks, %d failure(s)\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
