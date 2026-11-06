/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The picture on the stripe (sento SenStripeDialog.h) that shows at its full size when it is clicked: a window with a stripe, the picture shown and put away again
// (what a click on the picture, or on the large one, does). Build and run on Haiku, in a session with a desktop, with a picture as the argument:
//   g++ -std=c++20 -I/boot/home/config/non-packaged/develop/headers tests/EnlargeTest.cpp -o /tmp/enlarge-test -lbe -ltranslation && /tmp/enlarge-test picture.jpg
// It takes the screenshots /tmp/enlarge-small.png and /tmp/enlarge-large.png.

#include <sen/SenStripeDialog.h>

#include <Application.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.sen-labs.shoji-enlarge-test");
    if (argc < 2) {
        printf("usage: enlarge-test <picture>\n");
        return 2;
    }
    BBitmap* full = BTranslationUtils::GetBitmap(argv[1]);
    if (full == NULL) {
        printf("no picture\n");
        return 2;
    }
    BBitmap* small = sen::ScaleDown(full, 128);
    printf("full %d x %d, shown %d x %d\n", (int) full->Bounds().Width() + 1, (int) full->Bounds().Height() + 1, (int) small->Bounds().Width() + 1,
        (int) small->Bounds().Height() + 1);

    BWindow* window = new BWindow(BRect(300, 150, 700, 450), "enlarge test", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE);
    sen::StripeView* stripe = new sen::StripeView(small, full);
    stripe->SetExplicitMinSize(BSize(150, 300));
    window->AddChild(stripe);
    stripe->ResizeTo(150, 300);
    window->Show();
    snooze(800000);
    system("screenshot -s -f png /tmp/enlarge-small.png");

    window->Lock();
    stripe->ToggleEnlarged();
    window->Unlock();
    snooze(1200000);
    system("screenshot -s -f png /tmp/enlarge-large.png");

    window->Lock();
    stripe->ToggleEnlarged();       // (a second time: away again)
    window->Unlock();
    snooze(600000);
    printf("%d windows left (the test window)\n", (int) be_app->CountWindows());
    int left = be_app->CountWindows();

    window->Lock();
    window->Quit();

    // the enlarged picture belongs to its window: it is gone when the window closes
    BBitmap* full2 = BTranslationUtils::GetBitmap(argv[1]);
    BBitmap* small2 = sen::ScaleDown(full2, 128);
    BWindow* parent = new BWindow(BRect(300, 150, 700, 450), "enlarge test 2", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE);
    sen::StripeView* stripe2 = new sen::StripeView(small2, full2);
    stripe2->SetExplicitMinSize(BSize(150, 300));
    parent->AddChild(stripe2);
    stripe2->ResizeTo(150, 300);
    parent->Show();
    snooze(500000);
    parent->Lock();
    stripe2->ToggleEnlarged();
    parent->Unlock();
    snooze(800000);
    const int withPicture = be_app->CountWindows();
    parent->Lock();
    parent->Quit();
    snooze(800000);
    const int afterClosing = be_app->CountWindows();
    printf("%d windows with the picture, %d after the window was closed (0 expected)\n", withPicture, afterClosing);
    return left == 1 && withPicture == 2 && afterClosing == 0 ? 0 : 1;
}
