/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
#pragma once

#include <Message.h>
#include <Node.h>
#include <String.h>
#include <TypeConstants.h>
#include <View.h>

#include <vector>

// sent by a control to its window: the value was changed but is not stored yet
#define SHOJI_MSG_ATTR_EDITED   'SHed'
// sent by a control to its window: store the value (Enter, click on a check box)
#define SHOJI_MSG_ATTR_COMMIT   'SHco'
// string field of both messages: the name of the attribute
#define SHOJI_FIELD_ATTR        "shoji:attr"

/*
 * One attribute of the MIME type of the edited file, as declared in its attribute info
 * ("attr:name", "attr:public_name", "attr:type", "attr:viewable", "attr:editable").
 */
struct ShojiAttribute {
    BString     name;
    BString     label;
    type_code   type;
    bool        editable;   // declared editable, and the file can be written
};

typedef std::vector<ShojiAttribute> ShojiAttributeList;

// The raw value of an attribute (type and bytes), to put it back exactly as it was, whatever its type.
struct ShojiAttributeSnapshot {
    ShojiAttributeSnapshot() : present(false), type(0) {}

    bool                present;    // false: the file did not have the attribute
    type_code           type;
    std::vector<uint8>  data;
};

namespace shoji {

// types that can be shown and stored: strings, booleans, integers, floating point numbers and time
bool        IsSupportedType(type_code type);

// appends the viewable attributes of a MIME type attribute info that are not in the list yet.
void        AttributesFromMimeInfo(const BMessage& attrInfo, ShojiAttributeList* attrs);

// reads an attribute of the file as text. The type of the attribute is set to the type found in
// the file, so that storing never changes it. B_ENTRY_NOT_FOUND: the file does not have it.
status_t    LoadAttribute(BNode& node, ShojiAttribute* attr, BString* text);

// stores text as value of the attribute, converted to its type. An empty value removes the attribute.
// B_BAD_VALUE: the text is not a valid value of the type, B_NOT_SUPPORTED: the type is not supported.
status_t    StoreAttribute(BNode& node, const ShojiAttribute& attr, const char* text);

// B_NOT_SUPPORTED: too large to be kept
status_t    SnapshotAttribute(BNode& node, const char* name, ShojiAttributeSnapshot* snapshot);
bool        SnapshotMatches(BNode& node, const char* name, const ShojiAttributeSnapshot& snapshot);
status_t    RestoreAttribute(BNode& node, const char* name, const ShojiAttributeSnapshot& snapshot);

// the value of a control as text (check boxes: "1" or "0"), false if the control is not supported.
bool        GetControlValue(BView* view, BString* text);
bool        SetControlValue(BView* view, const char* text);

}   // namespace shoji
