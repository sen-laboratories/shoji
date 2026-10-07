/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <Alert.h>
#include <Directory.h>
#include <Entry.h>
#include <Errors.h>
#include <Path.h>
#include <cstdio>
#include <cstdio>

#include "ShojiApp.h"
#include "ShojiWindow.h"

// custom template views
#include "../templates/application/x-person/ContactView.h"

const char* kApplicationSignature = "application/x-vnd.sen-labs.Shoji";

ShojiApp::ShojiApp() : BApplication(kApplicationSignature)
{
}

ShojiApp::~ShojiApp()
{
}

int main()
{
	ShojiApp* app = new ShojiApp();
    if (app->InitCheck() != B_OK) {
        return 1;
    }
	app->Run();

	delete app;
	return 0;
}

void ShojiApp::ArgvReceived(int32 argc, char **argv)
{
    if (argc < 1) {
        // todo: show empty splash screen and allow drag&drop
        return;
    }

    if (strncmp(argv[1], "-g", 2) == 0 || strncmp(argv[1], "--generate", 10) == 0) {
        printf("generating templates...\n");
        status_t status = GenerateTemplates();

        if (status == B_OK) {
            printf("templates generated successfully.\n");
        } else {
            printf("error generating templates: %s\n", strerror(status));
        }
        Quit();
        return;
    }

    BMessage refsMsg(B_REFS_RECEIVED);
    BEntry entry(argv[1]);
    entry_ref ref;

    entry.GetRef(&ref);
    refsMsg.AddRef("refs", &ref);

    RefsReceived(&refsMsg);
}

void ShojiApp::RefsReceived(BMessage *message)
{
    entry_ref ref;

    if (message->FindRef("refs", &ref) != B_OK) {
        BAlert* alert = new BAlert("Error launching SHOI",
            "Failed to resolve input file.",
            "Oh no.");
        alert->SetFlags(alert->Flags() | B_WARNING_ALERT | B_CLOSE_ON_ESCAPE);

        return;
    }

    (new ShojiWindow(new entry_ref(ref.device, ref.directory, ref.name)))->Show();
}

status_t ShojiApp::GenerateTemplates() {
    BPath outPath("./generated");
    status_t result = create_directory(outPath.Path(), B_READ_WRITE);
    if (result != B_OK && result != B_FILE_EXISTS) {
        return result;
    }

    BPath viewPath(outPath);

    // Person View
    BMessage contactViewFlat;
    (new ContactView())->Archive(&contactViewFlat);
    const char* mimeType = "application/x-person";
    viewPath.Append(mimeType);

    printf("generating template %s...\n", viewPath.Path());
    result = create_directory(viewPath.Path(), B_READ_WRITE);
    if (result != B_OK) return result;

    viewPath.Append("template.view");
    BFile outFile(viewPath.Path(), B_CREATE_FILE | B_READ_WRITE);
    result = outFile.InitCheck();
    if (result == B_OK) result = contactViewFlat.Flatten(&outFile);

    return result;
}
