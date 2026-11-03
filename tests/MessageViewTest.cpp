/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The form for a BMessage (ShojiMessageView): the controls that are made from the data, what a change in a control does to the working copy and what the
// target is told, the refusal of a value that does not fit, lists, nested messages, Revert and Defaults. Build and run on Haiku, in a session with a desktop:
//   g++ -std=c++20 -Isrc/app -Isrc/message -I/boot/home/config/non-packaged/develop/headers -isystem /boot/system/develop/headers/private/shared
//       -isystem /boot/system/develop/headers/private/interface tests/MessageViewTest.cpp src/message/ShojiMessageView.cpp src/app/ShojiListControl.cpp
//       src/app/ShojiDateControl.cpp -o /tmp/message-view-test -lbe -lshared && /tmp/message-view-test [screenshot [node] | write file | date file]  (one command)
// With an argument, a window with the form stays for 25 seconds, for a screenshot (the second argument is the node of the tree to show, e.g. `nested.items_01`).

#include "ShojiMessageView.h"
#include "ShojiDateControl.h"
#include "ShojiListControl.h"

#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <File.h>
#include <Node.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
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
        whole(0)
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
        changed = refused = whole = 0;
        lastPath.clear();
        lastValue.clear();
    }

    ShojiMessageView*   view;
    int32               changed;
    int32               refused;
    int32               whole;
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
    CHECK(dynamic_cast<BControl*>(w->view->ControlFor("accent")) == NULL);
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
    window->Quit();
    TestNoTree();

    printf("%d checks, %d failure(s)\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
