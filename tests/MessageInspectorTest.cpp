/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The inspector for any BMessage (ShojiMessageInspector): a row for every value with its name, type and value, all types shown, the types that a text stands for edited
// in the row (field, toggle), refused values, removal, and what the target is told. Build and run on Haiku, in a session with a desktop:
//   g++ -std=c++20 -Isrc/message -I/boot/home/config/non-packaged/develop/headers tests/MessageInspectorTest.cpp src/message/ShojiMessageInspector.cpp
//       -o /tmp/inspector-test -lbe && /tmp/inspector-test [screenshot]      (one command)
// With an argument, a window with the inspector stays for 25 seconds, for a screenshot.

#include "ShojiMessageInspector.h"

#include <Application.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <TextView.h>
#include <Window.h>

#include <math.h>
#include <stdio.h>
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
MakeMessage()
{
    BMessage item1, item2, nested, message;
    item1.AddString("name", "one");
    item2.AddString("name", "two");
    nested.AddInt32("depth", 2);
    nested.AddMessage("items", &item1);
    nested.AddMessage("items", &item2);
    nested.AddString("empty", "");

    message.AddString("title", "A title");
    message.AddBool("flag", true);
    message.AddInt32("count", 42);
    message.AddInt32("scores", 1);
    message.AddInt32("scores", 2);
    message.AddInt32("scores", 3);
    message.AddFloat("ratio", 0.5f);
    message.AddDouble("big", 1.25);
    const time_t released = 86400 * 11017;
    message.AddData("released", B_TIME_TYPE, &released, sizeof(released));
    message.AddPoint("origin", BPoint(10, 20.5f));
    message.AddRect("frame", BRect(0, 0, 100, 50));
    message.AddColor("accent", (rgb_color){10, 20, 30, 255});
    message.AddString("multi", "line one\nline two");
    message.AddMessage("nested", &nested);
    message.AddData("blob", B_RAW_TYPE, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26);
    entry_ref home;
    get_ref_for_path("/boot/home", &home);
    message.AddRef("where", &home);
    message.AddMessenger("owner", BMessenger(be_app));
    message.AddData("odd", 'ABCD', "xyz", 3);
    return message;
}

class TestWindow : public BWindow {
public:
    TestWindow()
        :
        BWindow(BRect(100, 100, 700, 560), "inspector test", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
        changed(0),
        whole(0),
        refused(0)
    {
        inspector = new ShojiMessageInspector();
        inspector->SetTarget(BMessenger(this));
        BLayoutBuilder::Group<>(this, B_VERTICAL, 0).Add(inspector).End();
    }

    virtual void MessageReceived(BMessage* message)
    {
        switch (message->what) {
            case form::kChanged:
                changed++;
                if (message->HasString(form::kFieldPath)) {
                    lastPath = message->GetString(form::kFieldPath, "");
                    lastValue = message->GetString(form::kFieldValue, "");
                } else {
                    whole++;
                }
                message->FindMessage(form::kFieldSettings, &lastSettings);
                break;
            case form::kRefused:
                refused++;
                lastRefusedPath = message->GetString(form::kFieldPath, "");
                break;
            default:
                BWindow::MessageReceived(message);
        }
    }

    void Settle()
    {
        Unlock();
        snooze(150000);
        Lock();
    }

    void Reset()
    {
        changed = whole = refused = 0;
        lastPath.clear();
        lastValue.clear();
        lastRefusedPath.clear();
    }

    std::string Text(const char* path)
    {
        std::string text = "<none>";
        GetText(inspector->Settings(), path, &text);
        return text;
    }

    void Send(uint32 what) { BMessage message(what); inspector->MessageReceived(&message); Settle(); }

    ShojiMessageInspector*  inspector;
    int32                   changed;
    int32                   whole;
    int32                   refused;
    std::string             lastPath;
    std::string             lastValue;
    std::string             lastRefusedPath;
    BMessage                lastSettings;
};

static void
TestRows(TestWindow* w)
{
    ShojiMessageInspector* i = w->inspector;
    std::vector<Leaf> leaves;
    Enumerate(i->Settings(), &leaves);
    CHECK(i->CountAllRows() == (int32)leaves.size());
    CHECK(i->CountAllRows() == 24);

    int32 row = i->RowOf("title");
    CHECK(row >= 0 && i->RowName(row) == "title" && i->RowType(row) == "string" && i->RowValue(row) == "A title" && i->RowEditable(row));
    row = i->RowOf("count");
    CHECK(i->RowType(row) == "int32" && i->RowValue(row) == "42");
    row = i->RowOf("scores_01");
    CHECK(row >= 0 && i->RowName(row) == "scores[1]" && i->RowValue(row) == "2");     // an array: a row for each value
    CHECK(i->RowType(i->RowOf("ratio")) == "float" && i->RowValue(i->RowOf("ratio")) == "0.5");
    CHECK(i->RowType(i->RowOf("big")) == "double");
    CHECK(i->RowType(i->RowOf("flag")) == "bool" && i->RowValue(i->RowOf("flag")) == "true");
    CHECK(i->RowType(i->RowOf("released")) == "time" && i->RowValue(i->RowOf("released")) == "2000-03-01");
    CHECK(i->RowType(i->RowOf("origin")) == "point" && i->RowValue(i->RowOf("origin")) == "10, 20.5");
    CHECK(i->RowType(i->RowOf("frame")) == "rect" && i->RowValue(i->RowOf("frame")) == "0, 0, 100, 50");
    CHECK(i->RowType(i->RowOf("accent")) == "color" && i->RowValue(i->RowOf("accent")) == "#0a141e" && i->RowEditable(i->RowOf("accent")));

    // a nested message: its row says how many fields it has, and its fields are rows below it, an array of messages has a row for each message
    row = i->RowOf("nested");
    CHECK(i->RowType(row) == "message" && i->RowValue(row) == "{3 fields}" && !i->RowEditable(row));
    CHECK(i->RowOf("nested.depth") > row && i->RowValue(i->RowOf("nested.depth")) == "2");
    CHECK(i->RowOf("nested.items_00") > 0 && i->RowOf("nested.items_01.name") > 0);
    CHECK(i->RowValue(i->RowOf("nested.items_00")) == "{1 field}");

    // what no text stands for is shown with what can be said of it, and not edited
    row = i->RowOf("blob");
    CHECK(i->RowType(row) == "raw" && i->RowValue(row).StartsWith("26 bytes: 41 42 43") && i->RowValue(row).EndsWith(B_UTF8_ELLIPSIS) && !i->RowEditable(row));
    row = i->RowOf("where");
    CHECK(i->RowType(row) == "ref" && i->RowValue(row) == "/boot/home" && !i->RowEditable(row));
    row = i->RowOf("owner");
    CHECK(i->RowType(row) == "messenger" && i->RowValue(row).StartsWith("team ") && !i->RowEditable(row));
    row = i->RowOf("odd");
    CHECK(i->RowType(row) == "'ABCD'" && i->RowValue(row) == "3 bytes: 78 79 7a");

    // a string with a line break cannot go into the one line of the field: shown, not edited
    row = i->RowOf("multi");
    CHECK(i->RowValue(row) == "line one line two" && !i->RowEditable(row));
    CHECK(i->RowValue(i->RowOf("nested.empty")) == "" && i->RowEditable(i->RowOf("nested.empty")));
}

static void
TestEditing(TestWindow* w)
{
    ShojiMessageInspector* i = w->inspector;
    w->Reset();

    // the types stay, what is shown is what is stored
    CHECK(i->SetValue("count", "007") == B_OK);
    CHECK(w->Text("count") == "7" && i->RowValue(i->RowOf("count")) == "7");
    int32 count = 0;
    CHECK(i->Settings().FindInt32("count", &count) == B_OK && count == 7);
    w->Settle();
    CHECK(w->changed == 1 && w->lastPath == "count" && w->lastValue == "7");
    CHECK(w->lastSettings.HasSameData(i->Settings(), true, true));

    CHECK(i->SetValue("nested.depth", "5") == B_OK && w->Text("nested.depth") == "5");
    CHECK(i->SetValue("scores_02", "30") == B_OK && w->Text("scores_02") == "30");
    CHECK(i->SetValue("origin", "3 4") == B_OK && i->RowValue(i->RowOf("origin")) == "3, 4");
    CHECK(i->SetValue("frame", "1,2,3,4") == B_OK && i->RowValue(i->RowOf("frame")) == "1, 2, 3, 4");
    CHECK(i->SetValue("accent", "#fff") == B_OK && i->RowValue(i->RowOf("accent")) == "#ffffff");
    CHECK(i->SetValue("released", "2024-02-29") == B_OK && i->RowValue(i->RowOf("released")) == "2024-02-29");
    w->Settle();

    // the same value again is no change
    w->Reset();
    CHECK(i->SetValue("count", "7") == B_OK);
    w->Settle();
    CHECK(w->changed == 0);

    // a value that does not fit is refused and told; nothing changes
    BMessage before(i->Settings());
    CHECK(i->SetValue("count", "many") == B_BAD_VALUE);
    CHECK(i->SetValue("origin", "3") == B_BAD_VALUE);
    CHECK(i->SetValue("accent", "red") == B_BAD_VALUE);
    CHECK(i->SetValue("blob", "x") != B_OK);
    CHECK(i->SetValue("nested", "x") != B_OK);
    CHECK(i->SetValue("nope", "x") != B_OK);
    w->Settle();
    CHECK(w->refused >= 3 && w->changed == 0);
    CHECK(i->Settings().HasSameData(before, true, true));

    // the field over the value: Enter keeps what was typed, Escape takes it back
    w->Reset();
    CHECK(i->SelectPath("title"));
    w->Send(ShojiMessageInspector::kEdit);
    BTextView* editor = i->Editor();
    CHECK(editor != NULL && strcmp(editor->Text(), "A title") == 0);
    if (editor != NULL) {
        editor->SetText("Another title");
        w->Send(ShojiMessageInspector::kEditorKept);
    }
    CHECK(i->Editor() == NULL && w->Text("title") == "Another title" && w->lastPath == "title");
    w->Send(ShojiMessageInspector::kEdit);
    editor = i->Editor();
    if (editor != NULL) {
        editor->SetText("never");
        w->Send(ShojiMessageInspector::kEditorCancelled);
    }
    CHECK(i->Editor() == NULL && w->Text("title") == "Another title");
    // a value that is typed and does not fit: the field goes, the value stays, the target is told
    w->Reset();
    CHECK(i->SelectPath("count"));
    w->Send(ShojiMessageInspector::kEdit);
    editor = i->Editor();
    if (editor != NULL) {
        editor->SetText("x1");
        w->Send(ShojiMessageInspector::kEditorKept);
    }
    CHECK(i->Editor() == NULL && w->Text("count") == "7" && w->refused == 1 && w->lastRefusedPath == "count");

    // a bool is toggled, not edited; what no text stands for does not open a field
    w->Reset();
    CHECK(i->SelectPath("flag"));
    w->Send(ShojiMessageInspector::kEdit);
    CHECK(i->Editor() == NULL && w->Text("flag") == "false" && w->lastPath == "flag" && w->lastValue == "false");
    w->Send(ShojiMessageInspector::kEdit);
    CHECK(w->Text("flag") == "true");
    CHECK(i->SelectPath("blob"));
    w->Send(ShojiMessageInspector::kEdit);
    CHECK(i->Editor() == NULL);
    CHECK(i->SelectPath("multi"));
    w->Send(ShojiMessageInspector::kEdit);
    CHECK(i->Editor() == NULL);
}

static void
TestRemoving(TestWindow* w)
{
    ShojiMessageInspector* i = w->inspector;
    const int32 rows = i->CountAllRows();
    w->Reset();

    // a value of an array goes, the others move up (the rows are made again), the whole message is told
    CHECK(i->RemoveValue("scores_00") == B_OK);
    w->Settle();
    CHECK(i->CountAllRows() == rows - 1);
    CHECK(w->Text("scores_00") == "2" && w->Text("scores_01") == "30");
    CHECK(w->whole == 1 && w->lastSettings.HasSameData(i->Settings(), true, true));

    // a nested message goes with its fields
    CHECK(i->RemoveValue("nested") == B_OK);
    w->Settle();
    CHECK(i->RowOf("nested") < 0 && i->RowOf("nested.depth") < 0 && i->RowOf("nested.items_00") < 0);
    CHECK(i->CountAllRows() == rows - 1 - 7);
    CHECK(i->RemoveValue("nope") != B_OK);

    // the key at the Delete key: the selected row
    CHECK(i->SelectPath("ratio"));
    w->Send(ShojiMessageInspector::kRemove);
    CHECK(i->RowOf("ratio") < 0);
}

static void
TestReadOnly(TestWindow* w)
{
    ShojiMessageInspector* i = w->inspector;
    w->Reset();
    i->SetEditable(false);
    CHECK(!i->RowEditable(i->RowOf("title")));
    CHECK(i->SetValue("title", "x") == B_NOT_ALLOWED);
    CHECK(i->RemoveValue("title") == B_NOT_ALLOWED);
    CHECK(i->SelectPath("flag"));
    w->Send(ShojiMessageInspector::kEdit);
    CHECK(w->Text("flag") == "true" && i->Editor() == NULL);
    w->Settle();
    CHECK(w->changed == 0);
    i->SetEditable(true);
    CHECK(i->RowEditable(i->RowOf("title")));
}

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.sen-labs.shoji-inspector-test");
    TestWindow* window = new TestWindow();
    window->Show();
    window->Lock();
    window->inspector->SetData(MakeMessage());
    window->Settle();

    if (argc > 1 && strcmp(argv[1], "screenshot") == 0) {
        window->inspector->SelectPath("accent");
        window->Unlock();
        snooze(25 * 1000000LL);
        window->Lock();
        window->Quit();
        return 0;
    }

    TestRows(window);
    TestEditing(window);
    TestRemoving(window);
    TestReadOnly(window);
    window->Quit();
    printf("%d checks, %d failure(s)\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
