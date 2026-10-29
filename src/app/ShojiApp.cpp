/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <Alert.h>
#include <Directory.h>
#include <Entry.h>
#include <Errors.h>
#include <File.h>
#include <Path.h>
#include <cstdio>
#include <cstring>

#include "ShojiApp.h"
#include "ShojiWindow.h"

// custom template views
#include "../templates/application/x-person/ContactView.h"
#include "../templates/entity/x-vnd.sen-labs.entity.book/BookView.h"

const char* kApplicationSignature = "application/x-vnd.sen-labs.Shoji";

namespace {

// the templates that Shoji brings: the MIME type, and the view that is archived as its template
struct TemplateSpec {
    const char* mimeType;
    BView*      (*create)();
};

BView* CreateContactView() { return new ContactView(); }
BView* CreateBookView() { return new BookView(); }

const TemplateSpec kTemplates[] = {
    { "application/x-person", CreateContactView },
    { "entity/x-vnd.sen-labs.entity.book", CreateBookView },
};

}   // namespace


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
        delete app;
        return 1;
    }
	app->Run();

	delete app;
	return 0;
}

void ShojiApp::ArgvReceived(int32 argc, char **argv)
{
    if (argc < 2) {
        // todo: show empty splash screen and allow drag&drop
        return;
    }

    if (strcmp(argv[1], "-g") == 0 || strcmp(argv[1], "--generate") == 0) {
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

    // the files to open: all arguments
    BMessage refsMsg(B_REFS_RECEIVED);
    for (int32 i = 1; i < argc; i++) {
        BEntry entry(argv[i]);
        entry_ref ref;
        status_t status = entry.InitCheck();
        if (status == B_OK)
            status = entry.GetRef(&ref);
        if (status != B_OK) {
            fprintf(stderr, "cannot open %s: %s\n", argv[i], strerror(status));
            continue;
        }
        refsMsg.AddRef("refs", &ref);
    }

    RefsReceived(&refsMsg);
}

void ShojiApp::RefsReceived(BMessage *message)
{
    entry_ref ref;
    int32 opened = 0;

    for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
        ShojiWindow* window = new ShojiWindow(&ref);
        window->Show();
        opened++;
    }

    if (opened == 0) {
        BAlert* alert = new BAlert("Error launching Shoji",
            "Failed to resolve input file.",
            "Oh no.");
        alert->SetFlags(alert->Flags() | B_WARNING_ALERT | B_CLOSE_ON_ESCAPE);
        alert->Go();

        // nothing to show: do not stay around without a window
        if (CountWindows() == 0)
            Quit();
    }
}

// archives the template views to ./generated/<MIME type>/template.view
status_t ShojiApp::GenerateTemplates() {
    BPath outPath("./generated");
    status_t result = create_directory(outPath.Path(), B_READ_WRITE);
    if (result != B_OK && result != B_FILE_EXISTS) {
        return result;
    }

    for (const TemplateSpec& spec : kTemplates) {
        BPath viewPath(outPath);
        viewPath.Append(spec.mimeType);

        printf("generating template %s...\n", viewPath.Path());
        result = create_directory(viewPath.Path(), B_READ_WRITE);
        if (result != B_OK && result != B_FILE_EXISTS)
            return result;

        BView* view = spec.create();
        BMessage flat;
        result = view->Archive(&flat);
        delete view;
        if (result != B_OK)
            return result;

        viewPath.Append("template.view");
        BFile outFile(viewPath.Path(), B_CREATE_FILE | B_ERASE_FILE | B_READ_WRITE);
        result = outFile.InitCheck();
        if (result == B_OK)
            result = flat.Flatten(&outFile);
        if (result != B_OK)
            return result;
    }

    return B_OK;
}
