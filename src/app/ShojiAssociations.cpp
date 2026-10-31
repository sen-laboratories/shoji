/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

#include "ShojiAssociations.h"

#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <MimeType.h>
#include <Path.h>
#include <String.h>

#include <algorithm>
#include <set>
#include <string.h>

namespace shoji {

// where the templates are looked for (as ShojiTemplateView does): the user's settings, then the data folders
static const directory_which kTemplateFolders[] = {
    B_USER_SETTINGS_DIRECTORY,
    B_USER_DATA_DIRECTORY,
    B_USER_NONPACKAGED_DATA_DIRECTORY,
    B_SYSTEM_DATA_DIRECTORY,
    B_SYSTEM_NONPACKAGED_DATA_DIRECTORY,
};

std::vector<std::string> ChooseTypes(AssociationMode mode, const std::vector<TypeState>& types, const std::string& signature)
{
    std::vector<std::string> chosen;
    if (mode == kAssociateNothing)
        return chosen;

    for (const TypeState& state : types) {
        // (the type of an application is its signature: the file of a program is an "application/x-be-executable", not that)
        if (state.type.compare(0, 18, "application/x-vnd.") == 0)
            continue;
        if (state.type.find('/') == std::string::npos || state.preferredApp == signature)
            continue;
        const bool unassociated = state.preferredApp.empty();
        if (unassociated || (mode == kAssociateTemplated && state.hasTemplate))
            chosen.push_back(state.type);
    }
    return chosen;
}

std::vector<std::string> TemplateTypes()
{
    std::set<std::string> found;
    for (directory_which which : kTemplateFolders) {
        BPath root;
        if (find_directory(which, &root) != B_OK || root.Append("shoji/templates") != B_OK)
            continue;

        BDirectory supertypes(root.Path());
        BEntry superEntry;
        while (supertypes.GetNextEntry(&superEntry, true) == B_OK) {
            if (!superEntry.IsDirectory())
                continue;
            char supertype[B_FILE_NAME_LENGTH];
            superEntry.GetName(supertype);

            BDirectory subtypes(&superEntry);
            BEntry subEntry;
            while (subtypes.GetNextEntry(&subEntry, true) == B_OK) {
                if (!subEntry.IsDirectory())
                    continue;
                char subtype[B_FILE_NAME_LENGTH];
                subEntry.GetName(subtype);
                BPath file(&subEntry);
                if (file.Append("template.view") == B_OK && BEntry(file.Path(), true).Exists())
                    found.insert(std::string(supertype) + "/" + subtype);
            }
        }
    }
    return std::vector<std::string>(found.begin(), found.end());
}

std::vector<TypeState> InstalledTypes(const std::vector<std::string>& templateTypes)
{
    std::vector<TypeState> types;

    BMessage supertypes;
    if (BMimeType::GetInstalledSupertypes(&supertypes) != B_OK)
        return types;

    const char* supertype;
    for (int32 i = 0; supertypes.FindString("super_types", i, &supertype) == B_OK; i++) {
        BMessage subtypes;
        if (BMimeType::GetInstalledTypes(supertype, &subtypes) != B_OK)
            continue;

        const char* type;
        for (int32 j = 0; subtypes.FindString("types", j, &type) == B_OK; j++) {
            TypeState state;
            state.type = type;
            // (a type is named in full: "text/plain")
            if (state.type.find('/') == std::string::npos)
                state.type = std::string(supertype) + "/" + type;
            char app[B_MIME_TYPE_LENGTH];
            app[0] = '\0';
            BMimeType(state.type.c_str()).GetPreferredApp(app);
            state.preferredApp = app;
            state.hasTemplate = std::find(templateTypes.begin(), templateTypes.end(), state.type) != templateTypes.end();
            types.push_back(state);
        }
    }
    return types;
}

int32 Associate(const std::vector<std::string>& types, const std::string& signature, BMessage* previous)
{
    int32 changed = 0;
    for (const std::string& type : types) {
        BMimeType mime(type.c_str());
        if (mime.InitCheck() != B_OK || !mime.IsInstalled())
            continue;

        char before[B_MIME_TYPE_LENGTH];
        before[0] = '\0';
        mime.GetPreferredApp(before);
        if (signature == before)
            continue;
        if (mime.SetPreferredApp(signature.c_str()) != B_OK)
            continue;
        previous->AddString("type", type.c_str());
        previous->AddString("app", before);
        changed++;
    }
    return changed;
}

int32 Restore(const BMessage& previous, const std::string& signature)
{
    int32 restored = 0;
    const char* type;
    for (int32 i = 0; previous.FindString("type", i, &type) == B_OK; i++) {
        const char* app = "";
        previous.FindString("app", i, &app);

        BMimeType mime(type);
        char current[B_MIME_TYPE_LENGTH];
        current[0] = '\0';
        // (the user may have chosen another application since: that stays)
        if (mime.InitCheck() != B_OK || !mime.IsInstalled() || mime.GetPreferredApp(current) != B_OK || signature != current)
            continue;
        if (mime.SetPreferredApp(app[0] != '\0' ? app : NULL) == B_OK)
            restored++;
    }
    return restored;
}

int32 Apply(AssociationMode mode, const std::string& signature, BMessage* previous)
{
    Restore(*previous, signature);
    previous->MakeEmpty();
    if (mode == kAssociateNothing)
        return 0;

    const std::vector<std::string> chosen = ChooseTypes(mode, InstalledTypes(TemplateTypes()), signature);
    return Associate(chosen, signature, previous);
}

}   // namespace shoji
