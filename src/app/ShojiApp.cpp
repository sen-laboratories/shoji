/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <Alert.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <FindDirectory.h>
#include <Roster.h>
#include <String.h>
#include <Directory.h>
#include <Entry.h>
#include <Errors.h>
#include <File.h>
#include <Path.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

#include <sen/SenStripeDialog.h>
#include <sen/SenVersion.h>

#include "ShojiApp.h"
#include "ShojiWindow.h"

// custom template views
#include "../templates/application/x-person/ContactView.h"
#include "../templates/entity/x-vnd.sen-labs.entity.book/BookView.h"

const char* kApplicationSignature = "application/x-vnd.sen-labs.Shoji";

// the permissions of the folders of the templates (rwxr-xr-x: it is a mode, not the flag of an open file, which B_READ_WRITE is)
static const mode_t kFolderMode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

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

// The logo of Shoji for the stripe of the About window: what the package brings (data/shoji/logo.jpg of the data folders), else, for a program that runs
// from its source tree, the picture of the repository.
static BBitmap* LoadLogo(float height)
{
    const directory_which folders[] = {B_USER_DATA_DIRECTORY, B_USER_NONPACKAGED_DATA_DIRECTORY, B_SYSTEM_DATA_DIRECTORY, B_SYSTEM_NONPACKAGED_DATA_DIRECTORY};
    for (directory_which folder : folders) {
        BPath path;
        if (find_directory(folder, &path) != B_OK || path.Append("shoji/logo.jpg") != B_OK)
            continue;
        BBitmap* logo = sen::LoadDialogImage(path.Path(), height);
        if (logo != NULL)
            return logo;
    }
    app_info info;
    BPath path;
    if (be_app->GetAppInfo(&info) == B_OK && path.SetTo(&info.ref) == B_OK && path.GetParent(&path) == B_OK && path.GetParent(&path) == B_OK
            && path.Append("images/shoji-logo.jpg") == B_OK)
        return sen::LoadDialogImage(path.Path(), height);
    return NULL;
}

// The About window of SEN (sento, SenStripeDialog.h): the name, the version and the description of the version resource, and the logo on the stripe.
void ShojiApp::AboutRequested()
{
    app_info info;
    if (GetAppInfo(&info) != B_OK)
        return;

    version_info version;
    memset(&version, 0, sizeof(version));
    BFile file(&info.ref, B_READ_ONLY);
    BAppFileInfo appInfo(&file);
    bool hasVersion = file.InitCheck() == B_OK && appInfo.GetVersionInfo(&version, B_APP_VERSION_KIND) == B_OK;

    BString name(hasVersion && version.short_info[0] != '\0' ? version.short_info : info.ref.name);
    BString number;
    if (hasVersion)
        number = sen::VersionText(version);
    BString copyright("Copyright " B_UTF8_COPYRIGHT " 2024-2026 SEN Labs e.U.\nPart of SEN, the Semantic Extensions\nNative for Haiku (MIT license).");

    sen::AboutDialog* window = new sen::AboutDialog(name.String(), number.String(), hasVersion ? version.long_info : "", copyright.String(),
        LoadLogo(128));
    window->ShowCentered();
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
    status_t result = create_directory(outPath.Path(), kFolderMode);
    if (result != B_OK && result != B_FILE_EXISTS) {
        return result;
    }

    for (const TemplateSpec& spec : kTemplates) {
        BPath viewPath(outPath);
        viewPath.Append(spec.mimeType);

        printf("generating template %s...\n", viewPath.Path());
        result = create_directory(viewPath.Path(), kFolderMode);
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
