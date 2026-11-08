/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiSettings.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

#include <sen/SenMapStyles.h>

namespace shoji {

static const char* kModeField = "associate";
static const char* kPreviousField = "previous";
static const char* kMapStyleField = "map_style";

Settings::Settings()
    :
    fMode(Default())
{
}

void Settings::SetMapStyle(const std::string& id)
{
    fMapStyle = sen::mapstyles::Find(id) != NULL ? id : std::string();
}

std::string Settings::PluginMapStyle()
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK || path.Append(sen::mapstyles::PluginSettingsFile()) != B_OK)
        return sen::mapstyles::DefaultId();
    BFile file(path.Path(), B_READ_ONLY);
    off_t size = 0;
    if (file.InitCheck() != B_OK || file.GetSize(&size) != B_OK || size <= 0 || size > 64 * 1024)
        return sen::mapstyles::DefaultId();
    std::string text(size, '\0');
    if (file.Read(&text[0], size) != size)
        return sen::mapstyles::DefaultId();

    // lines of `key = value` (a # starts a comment), the last one counts
    std::string found;
    size_t from = 0;
    while (from < text.size()) {
        size_t end = text.find('\n', from);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(from, end - from);
        from = end + 1;
        const size_t hash = line.find('#');
        if (hash != std::string::npos)
            line.resize(hash);
        const size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        auto trim = [](std::string value) {
            const char* blanks = " \t\r";
            const size_t first = value.find_first_not_of(blanks);
            if (first == std::string::npos)
                return std::string();
            return value.substr(first, value.find_last_not_of(blanks) - first + 1);
        };
        if (trim(line.substr(0, equals)) == sen::mapstyles::SettingKey())
            found = trim(line.substr(equals + 1));
    }
    return sen::mapstyles::Find(found) != NULL ? found : std::string(sen::mapstyles::DefaultId());
}

std::string Settings::EffectiveMapStyle() const
{
    return fMapStyle.empty() ? PluginMapStyle() : fMapStyle;
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
    SetMapStyle(settings.GetString(kMapStyleField, ""));
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
    settings.AddString(kMapStyleField, fMapStyle.c_str());

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
