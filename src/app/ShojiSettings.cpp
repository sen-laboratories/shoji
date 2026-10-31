/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiSettings.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

namespace shoji {

static const char* kModeField = "associate";
static const char* kPreviousField = "previous";

Settings::Settings()
    :
    fMode(Default())
{
}

static status_t SettingsPath(BPath* path)
{
    status_t result = find_directory(B_USER_SETTINGS_DIRECTORY, path, true);
    if (result == B_OK)
        result = path->Append("shoji");
    if (result == B_OK)
        result = create_directory(path->Path(), 0755);
    if (result == B_FILE_EXISTS)
        result = B_OK;
    if (result == B_OK)
        result = path->Append("settings");
    return result;
}

status_t Settings::Load()
{
    BPath path;
    status_t result = SettingsPath(&path);
    if (result != B_OK)
        return result;

    BFile file(path.Path(), B_READ_ONLY);
    BMessage settings;
    if (file.InitCheck() != B_OK || settings.Unflatten(&file) != B_OK)
        return B_ENTRY_NOT_FOUND;      // (no settings yet: the defaults)

    int32 mode = settings.GetInt32(kModeField, Default());
    fMode = mode == kAssociateUnassociated || mode == kAssociateTemplated ? (AssociationMode) mode : kAssociateNothing;
    fPrevious.MakeEmpty();
    settings.FindMessage(kPreviousField, &fPrevious);
    return B_OK;
}

status_t Settings::Save() const
{
    BPath path;
    status_t result = SettingsPath(&path);
    if (result != B_OK)
        return result;

    BMessage settings;
    settings.AddInt32(kModeField, fMode);
    settings.AddMessage(kPreviousField, &fPrevious);

    BFile file(path.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
    result = file.InitCheck();
    if (result == B_OK)
        result = settings.Flatten(&file);
    return result;
}

int32 Settings::CountAssociated() const
{
    int32 count = 0;
    type_code type;
    fPrevious.GetInfo("type", &type, &count);
    return count;
}

}   // namespace shoji
