/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The list of values with inline editing (ShojiListControl): add, change and remove a value, what is kept and what is taken back, and the messages to the window. The
// control is driven by its messages (the mouse and keys of the list only send them). Build and run on Haiku, in a session with a desktop:
//   g++ -std=c++20 -Isrc/app -I/boot/home/config/non-packaged/develop/headers tests/ListControlTest.cpp src/app/ShojiListControl.cpp -o /tmp/list-test -lbe && /tmp/list-test [screenshot]
// With an argument, a window with the list stays for a moment in the state "a value is being edited", for a screenshot (/tmp/list-test.png).

#include "ShojiListControl.h"

#include <Application.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <TextView.h>
#include <Window.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>

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

static const uint32 kModified = 'tmod';
static const uint32 kCommitted = 'tcom';

class TestWindow : public BWindow {
public:
    TestWindow()
        :
        BWindow(BRect(100, 100, 500, 300), "list test", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
        modified(0),
        committed(0)
    {
        list = new ShojiListControl("authors", ',', "Authors", 60, B_ALIGN_RIGHT);
        list->SetModificationMessage(new BMessage(kModified));
        list->SetMessage(new BMessage(kCommitted));
        list->SetTarget(this);
        BLayoutBuilder::Group<>(this, B_VERTICAL).SetInsets(10).Add(list).AddGlue().End();
    }

    virtual void MessageReceived(BMessage* message)
    {
        switch (message->what) {
            case kModified:     modified++; break;
            case kCommitted:    committed++; break;
            default:            BWindow::MessageReceived(message);
        }
    }

    BTextView* Editor() { return dynamic_cast<BTextView*>(FindView("value editor")); }
    BListView* Values() { return dynamic_cast<BListView*>(FindView("values")); }

    /** the user types into the field that is shown and presses Enter (or Escape) */
    void Type(const char* text, bool enter)
    {
        BTextView* editor = Editor();
        editor->SetText(text);
        Send(enter ? (uint32) ShojiListControl::kEditorKept : (uint32) ShojiListControl::kEditorCancelled);
    }

    void Send(uint32 what) { BMessage message(what); list->MessageReceived(&message); }

    /** the notifications that the window got (they are asynchronous) */
    void Settle()
    {
        Unlock();
        snooze(200000);
        Lock();
    }

    ShojiListControl*   list;
    int32               modified;
    int32               committed;
};

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.sen-labs.shoji-list-test");
    TestWindow* window = new TestWindow();
    window->Show();
    window->Lock();

    // What the mouse of the list does is to send messages to the control (a messenger that the list has): that must reach it. (The first version made the messenger
    // before the control was in a window, and nothing happened.)
    {
        window->list->SetListText("one,two");
        BMessage ask(ShojiListControl::kAddValue);
        // the list sends to the messenger that it was given: asked the way it does it
        BView* values = window->FindView("values");
        CHECK(values != NULL);
        BMessenger toControl(window->list);
        CHECK(toControl.IsValid());
        toControl.SendMessage(&ask);
        window->Unlock();
        snooze(300000);
        window->Lock();
        CHECK(!window->Editor()->IsHidden());
        window->Type("", false);
        // the pieces that the control hands out: the list and the field of the value must hold a valid messenger, too
        CHECK(window->list->IsConnected());
    }

    window->list->SetListText("Andy Hunt,\"Thomas, Dave\",Third");
    CHECK(window->Values()->CountItems() == 3);
    CHECK(window->list->ListText() == "Andy Hunt,\"Thomas, Dave\",Third");      // (not changed: as it was set)
    CHECK(window->Editor()->IsHidden());

    // add: an empty value is made and edited; nothing is kept until Enter
    window->Send(ShojiListControl::kAddValue);
    CHECK(window->Values()->CountItems() == 4);
    CHECK(!window->Editor()->IsHidden());
    CHECK(window->list->ListText() == "Andy Hunt,\"Thomas, Dave\",Third");      // (the new value is no value yet)
    window->Type("Fourth", true);
    CHECK(window->Values()->CountItems() == 4 && window->Editor()->IsHidden());
    CHECK(window->list->ListText() == "Andy Hunt,\"Thomas, Dave\",Third,Fourth");

    // add and cancel, add and keep nothing: the new value goes
    window->Send(ShojiListControl::kAddValue);
    window->Type("never", false);
    CHECK(window->Values()->CountItems() == 4);
    window->Send(ShojiListControl::kAddValue);
    window->Type("   ", true);
    CHECK(window->Values()->CountItems() == 4);

    // change the selected value, in place
    window->Values()->Select(1);
    window->Send(ShojiListControl::kEditValue);
    CHECK(!window->Editor()->IsHidden() && std::string(window->Editor()->Text()) == "Thomas, Dave");
    window->Type("Dave Thomas", true);
    CHECK(window->list->ListText() == "Andy Hunt,Dave Thomas,Third,Fourth");

    // a change that is taken back, and one that is the same text (no change)
    window->Values()->Select(2);
    window->Send(ShojiListControl::kEditValue);
    window->Type("Changed my mind", false);
    CHECK(window->list->ListText() == "Andy Hunt,Dave Thomas,Third,Fourth");
    window->Send(ShojiListControl::kEditValue);
    window->Type("Third", true);

    // a value that is emptied is removed
    window->Values()->Select(2);
    window->Send(ShojiListControl::kEditValue);
    window->Type("", true);
    CHECK(window->list->ListText() == "Andy Hunt,Dave Thomas,Fourth");

    // delete the selected value, the next is selected; with nothing selected nothing happens
    window->Values()->Select(0);
    window->Send(ShojiListControl::kDeleteValue);
    CHECK(window->list->ListText() == "Dave Thomas,Fourth" && window->Values()->CurrentSelection() == 0);
    window->Values()->DeselectAll();
    window->Send(ShojiListControl::kDeleteValue);
    CHECK(window->list->ListText() == "Dave Thomas,Fourth");

    // a value that holds the separator is quoted when it is stored
    window->Send(ShojiListControl::kAddValue);
    window->Type("Hunt, Andy", true);
    CHECK(window->list->ListText() == "Dave Thomas,Fourth,\"Hunt, Andy\"");

    // setting a new text ends an edit that is going on
    window->Send(ShojiListControl::kAddValue);
    window->list->SetListText("x,y");
    CHECK(window->Editor()->IsHidden() && window->Values()->CountItems() == 2 && window->list->ListText() == "x,y");

    // a disabled list is not edited
    window->list->SetEnabled(false);
    window->Send(ShojiListControl::kAddValue);
    CHECK(window->Values()->CountItems() == 2 && window->Editor()->IsHidden());
    window->list->SetEnabled(true);

    // the window was told of each change: kept values, changes, removals (not of the ones that were taken back)
    window->Settle();
    printf("     modified %d, committed %d\n", (int) window->modified, (int) window->committed);
    CHECK(window->modified == 5 && window->committed == 5);

    if (argc > 1) {
        // the state for a screenshot: a value in the middle is edited
        window->list->SetListText("Andy Hunt,Dave Thomas,Third,Fourth");
        window->Values()->Select(1);
        window->Send(ShojiListControl::kEditValue);
        window->Editor()->SetText("Dave Thomas (editing)");
        window->Unlock();
        snooze(4000000);
        system("screenshot -s -f png /tmp/list-test.png");
        window->Lock();
    }

    window->Quit();
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
