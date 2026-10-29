/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <Alert.h>
#include <Control.h>
#include <Layout.h>
#include <TextControl.h>
#include <Directory.h>
#include <Entry.h>
#include <Errors.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <cstdio>
#include <cstring>

#include "ShojiTemplateView.h"
#include "ShojiListControl.h"

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
    return LookupView(fType);
}

// The folders that may hold templates, in the order of the search: the user's own settings first (they replace a template
// that a package brings), then the data folders of the user and of the system (where packages put `data/shoji/templates`).
static const directory_which kTemplateFolders[] = {
    B_USER_SETTINGS_DIRECTORY,
    B_USER_DATA_DIRECTORY,
    B_USER_NONPACKAGED_DATA_DIRECTORY,
    B_SYSTEM_DATA_DIRECTORY,
    B_SYSTEM_NONPACKAGED_DATA_DIRECTORY,
};

status_t ShojiTemplateView::FindTemplateFile(const char* mimeType, BPath* file)
{
    // a MIME type names a folder: nothing that leaves the folder of the templates
    if (mimeType == NULL || mimeType[0] == '\0' || strstr(mimeType, "..") != NULL || mimeType[0] == '/')
        return B_BAD_VALUE;

    for (directory_which which : kTemplateFolders) {
        BPath path;
        if (find_directory(which, &path) != B_OK
                || path.Append(SHOJI_TEMPLATE_PATH) != B_OK
                || path.Append(mimeType) != B_OK
                || path.Append(SHOJI_TEMPLATE_NAME) != B_OK)
            continue;

        BEntry entry(path.Path(), true);
        if (entry.Exists() && entry.IsFile()) {
            *file = path;
            return B_OK;
        }
    }
    return B_ENTRY_NOT_FOUND;
}

status_t ShojiTemplateView::LookupView(const char* mimeType) {
    BPath path;
    status_t status = FindTemplateFile(mimeType, &path);
    if (status != B_OK) {
        printf("no view template defined for MIME type %s.\n", mimeType);
        return status;
    }

    // the view template is an archived view, in the folder of the MIME type
    // (this folder will contain other assets later, like DataProviders)
    BFile templateFile(path.Path(), B_READ_ONLY);
    if ((status = templateFile.InitCheck()) != B_OK) {
        printf("error reading template file %s: %s\n", path.Path(), strerror(status));
        return status;
    }
    BMessage archivedViewMsg;
    status = archivedViewMsg.Unflatten(&templateFile);
    if (status != B_OK) {
        printf("error unwrapping template view %s: %s\n", path.Path(), strerror(status));
        return status;
    }

    fView = new BView(&archivedViewMsg);
    printf("template %s unwrapped, view is %s\n", path.Path(), (fView == NULL ? "NULL" : "SET"));

    return (fView != NULL ? B_OK : B_ERROR);
}

// A template made for text fields shows a list attribute in one: its field is replaced by the control for lists, in the place of the field
// (a template that has the control for lists itself is left alone). The control keeps the label and the width of the label of the field.
static BView* UpgradeToList(BView* view, const ShojiAttribute& attr)
{
    BTextControl* field = dynamic_cast<BTextControl*>(view);
    BView* parent = view->Parent();
    BLayout* layout = parent != NULL ? parent->GetLayout() : NULL;
    if (field == NULL || layout == NULL || attr.type != B_STRING_TYPE)
        return view;

    ShojiListControl* list = new ShojiListControl(attr.name.String(), attr.list.separator, field->Label(), field->Divider());
    int32 index = layout->IndexOfView(field);
    if (index < 0 || !layout->RemoveView(field)) {
        delete list;
        return view;
    }
    layout->AddView(index, list);
    delete field;
    return list;
}

// controls are named after the attributes they show, the others of the template are left alone.
status_t ShojiTemplateView::Populate(const ShojiAttributeList& attrs, const BMessage* values)
{
    for (const ShojiAttribute& attr : attrs) {
        BView* view = fView->FindView(attr.name.String());
        if (view == NULL)
            continue;
        if (attr.list.isList)
            view = UpgradeToList(view, attr);

        BString text;
        values->FindString(attr.name.String(), &text);
        if (!shoji::SetControlValue(view, text.String()))
            continue;

        if (BControl* control = dynamic_cast<BControl*>(view))
            control->SetEnabled(attr.editable);
    }

    return B_OK;
}
