/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <Alert.h>
#include <Control.h>
#include <Directory.h>
#include <Errors.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <cstdio>

#include "ShojiTemplateView.h"

/*
 * custom MIME-specific template view for the provided source file type.
 * Builds a view from an archived view serving as a template and populates view controls
 * named after source file attributes according to type and attribute value.
 */
ShojiTemplateView::ShojiTemplateView(const char* mimeType) : ShojiView()
{
    fMimeType = mimeType;
    fType = fMimeType.String();
}

ShojiTemplateView::~ShojiTemplateView()
{
}

const char* ShojiTemplateView::GetType() {
    return fType;
}

status_t ShojiTemplateView::Initialize() {
    return LookupView(fType, fView);
}

status_t ShojiTemplateView::LookupView(const char* mimeType, BView* view) {
    BPath path;
    BDirectory templatesDir;
    status_t status;

	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
    {
        printf("could not access user settings dir, falling back to current dir.\n");
        templatesDir.SetTo(".");
        if (! templatesDir.Contains("templates", B_DIRECTORY_NODE)) {
            return B_ENTRY_NOT_FOUND;
        }
        path.SetTo("./templates");
    } else {
        path.Append(SHOJI_TEMPLATE_PATH);
        status = create_directory(path.Path(), B_READ_WRITE);
        if (status != B_OK) {
            printf("failed to create settings dir %s: %s\n", path.Path(), strerror(status));
            return status;
        }
    }
    templatesDir.SetTo(path.Path());
    if ((status = templatesDir.InitCheck()) != B_OK) {
        printf("failed to access templates dir at path %s.\n", path.Path());
        return status;
    }

    // now search for template corresponding to the mimetype
    if (! templatesDir.Contains(mimeType, B_DIRECTORY_NODE)) {
        printf("no view template defined for MIME type %s.\n", mimeType);
        return B_ENTRY_NOT_FOUND;
    }
    path.Append(mimeType);
    status = templatesDir.SetTo(path.Path());
	if (status != B_OK) {
        printf("failed to lookup template for MIME type %s: %s\n", mimeType, strerror(status));
        return status;
    }
    // view template is stored in MIME type dir as archived view
    // this directory will contain other assets later, like DataProviders.
    if (! templatesDir.Contains(SHOJI_TEMPLATE_NAME)) {
        printf("no view template file found for the MIME type %s.\n", mimeType);
        return B_ENTRY_NOT_FOUND;
    }
    BFile templateFile(&templatesDir, SHOJI_TEMPLATE_NAME, B_READ_ONLY);
    if ((status = templateFile.InitCheck()) != B_OK) {
        printf("error reading template file for MIME type %s: %s\n", mimeType, strerror(status));
        return status;
    }
    BMessage archivedViewMsg;
    status = archivedViewMsg.Unflatten(&templateFile);
    if (status != B_OK) {
        printf("error unwrapping template view for MIME type %s: %s\n", mimeType, strerror(status));
        return status;
    }

    fView = new BView(&archivedViewMsg);
    printf("template view unwrapped with status %s, view is %s\n", strerror(status), (fView == NULL ? "NULL" : "SET"));

    return (fView != NULL ? B_OK : B_ERROR);
}

// controls are named after the attributes they show, the others of the template are left alone.
status_t ShojiTemplateView::Populate(const ShojiAttributeList& attrs, const BMessage* values)
{
    for (const ShojiAttribute& attr : attrs) {
        BView* view = fView->FindView(attr.name.String());
        if (view == NULL)
            continue;

        BString text;
        values->FindString(attr.name.String(), &text);
        if (!shoji::SetControlValue(view, text.String()))
            continue;

        if (BControl* control = dynamic_cast<BControl*>(view))
            control->SetEnabled(attr.editable);
    }

    return B_OK;
}
