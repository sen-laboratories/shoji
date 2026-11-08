/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// Sets the style of the map of a place in the settings of Shoji (~/config/settings/shoji/settings, field map_style), as its settings window does; the other settings stay. For the test
// place-map-styles.sh. Build: g++ -std=c++17 -o mapstyle-set mapstyle-set.cpp -lbe     Use: mapstyle-set <id of a style>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: mapstyle-set <style id>\n");
        return 2;
    }
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) != B_OK || path.Append("shoji") != B_OK)
        return 1;
    create_directory(path.Path(), 0755);
    if (path.Append("settings") != B_OK)
        return 1;

    BMessage settings;
    {
        BFile in(path.Path(), B_READ_ONLY);
        if (in.InitCheck() == B_OK)
            settings.Unflatten(&in);
    }
    settings.RemoveName("map_style");
    settings.AddString("map_style", argv[1]);
    BFile out(path.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
    return out.InitCheck() == B_OK && settings.Flatten(&out) == B_OK ? 0 : 1;
}
