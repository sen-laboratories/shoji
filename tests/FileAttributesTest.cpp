/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The attributes of a file as the form of a type without attributes of its own (a Markdown file) shows them: those that Shoji can edit, not the ones of the system,
// the file manager and SEN, not raw data, not those that the type declares already. Build and run on Haiku:
//   g++ -std=c++20 -Isrc/app -I/boot/home/config/non-packaged/develop/headers -I/boot/system/develop/headers/private/shared tests/FileAttributesTest.cpp src/app/ShojiAttributes.cpp src/app/ShojiListControl.cpp src/app/ShojiDateControl.cpp src/app/ShojiDurationControl.cpp -o /tmp/fileattr-test -lbe -lshared && /tmp/fileattr-test

#include "ShojiAttributes.h"

#include <File.h>
#include <fs_attr.h>

#include <stdio.h>
#include <string.h>

static int sFailures = 0;
static int sChecks = 0;

#define CHECK(condition) \
    do { \
        sChecks++; \
        if (!(condition)) { \
            sFailures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while (0)

static const ShojiAttribute* Find(const ShojiAttributeList& attrs, const char* name)
{
    for (const ShojiAttribute& attr : attrs) {
        if (attr.name == name)
            return &attr;
    }
    return NULL;
}

int main()
{
    const char* path = "/tmp/shoji-fileattr-test";
    BFile file(path, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    CHECK(file.InitCheck() == B_OK);

    file.WriteAttr("dc:title", B_STRING_TYPE, 0, "A note", 7);
    int32 count = 3;
    file.WriteAttr("note:count", B_INT32_TYPE, 0, &count, sizeof(count));
    file.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0, "text/markdown", 14);
    file.WriteAttr("SEN:ID", B_STRING_TYPE, 0, "abc", 4);
    file.WriteAttr("_trk/pinfo", B_STRING_TYPE, 0, "x", 2);
    file.WriteAttr("SYS:PACKAGE", B_STRING_TYPE, 0, "x", 2);
    file.WriteAttr("Media:Thumbnail", B_RAW_TYPE, 0, "xyz", 3);
    file.WriteAttr("note:raw", B_RAW_TYPE, 0, "xyz", 3);

    ShojiAttributeList attrs;
    shoji::AttributesOfFile(file, &attrs);
    CHECK(attrs.size() == 2);
    const ShojiAttribute* title = Find(attrs, "dc:title");
    CHECK(title != NULL && title->type == B_STRING_TYPE && title->viewable && title->editable && title->label == "dc:title");
    const ShojiAttribute* number = Find(attrs, "note:count");
    CHECK(number != NULL && number->type == B_INT32_TYPE);
    CHECK(Find(attrs, "BEOS:TYPE") == NULL);
    CHECK(Find(attrs, "SEN:ID") == NULL);
    CHECK(Find(attrs, "_trk/pinfo") == NULL);
    CHECK(Find(attrs, "SYS:PACKAGE") == NULL);
    CHECK(Find(attrs, "Media:Thumbnail") == NULL);
    CHECK(Find(attrs, "note:raw") == NULL);

    // what the type declares stays as it is, once
    ShojiAttributeList declared;
    ShojiAttribute attr;
    attr.name = "dc:title";
    attr.label = "Title";
    attr.type = B_STRING_TYPE;
    attr.viewable = false;
    attr.editable = false;
    declared.push_back(attr);
    shoji::AttributesOfFile(file, &declared);
    CHECK(declared.size() == 2);
    CHECK(Find(declared, "dc:title")->label == "Title");

    // a file with only attributes of the system has nothing to show
    BFile bare("/tmp/shoji-fileattr-bare", B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    bare.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0, "text/markdown", 14);
    ShojiAttributeList none;
    shoji::AttributesOfFile(bare, &none);
    CHECK(none.empty());

    // a MIME string (the semantic type, META:TYPE) is read and written as a string, and stays a MIME string
    BFile typed("/tmp/shoji-fileattr-mime", B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    const char* event = "entity/x-vnd.sen-labs.entity.event";
    typed.WriteAttr("META:TYPE", B_MIME_STRING_TYPE, 0, event, strlen(event) + 1);
    CHECK(shoji::IsSupportedType(B_MIME_STRING_TYPE));
    ShojiAttribute meta;
    meta.name = "META:TYPE";
    BString text;
    CHECK(shoji::LoadAttribute(typed, &meta, &text) == B_OK && meta.type == B_MIME_STRING_TYPE && text == event);
    CHECK(shoji::StoreAttribute(typed, meta, "entity/x-vnd.sen-labs.entity.calendar") == B_OK);
    attr_info info;
    CHECK(typed.GetAttrInfo("META:TYPE", &info) == B_OK && info.type == B_MIME_STRING_TYPE);
    CHECK(shoji::LoadAttribute(typed, &meta, &text) == B_OK && text == "entity/x-vnd.sen-labs.entity.calendar");
    CHECK(shoji::StoreAttribute(typed, meta, "not a mime type") == B_BAD_VALUE);
    CHECK(shoji::LoadAttribute(typed, &meta, &text) == B_OK && text == "entity/x-vnd.sen-labs.entity.calendar");	// (unchanged)
    CHECK(shoji::StoreAttribute(typed, meta, "") == B_OK && typed.GetAttrInfo("META:TYPE", &info) == B_ENTRY_NOT_FOUND);

    remove(path);
    remove("/tmp/shoji-fileattr-mime");
    remove("/tmp/shoji-fileattr-bare");
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
