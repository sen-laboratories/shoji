/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <CheckBox.h>
#include <TextControl.h>
#include <fs_attr.h>

#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <type_traits>

#include "ShojiAttributes.h"

namespace shoji {

static const ssize_t kMaxAttributeSize = 64 * 1024;
static const char* kTimeFormat = "%Y-%m-%d %H:%M:%S";

bool IsSupportedType(type_code type)
{
    switch (type) {
        case B_STRING_TYPE:
        case B_BOOL_TYPE:
        case B_INT8_TYPE:
        case B_INT16_TYPE:
        case B_INT32_TYPE:
        case B_INT64_TYPE:
        case B_UINT8_TYPE:
        case B_UINT16_TYPE:
        case B_UINT32_TYPE:
        case B_UINT64_TYPE:
        case B_FLOAT_TYPE:
        case B_DOUBLE_TYPE:
        case B_TIME_TYPE:
            return true;
        default:
            return false;
    }
}

void AttributesFromMimeInfo(const BMessage& attrInfo, ShojiAttributeList* attrs)
{
    const char* name;
    for (int32 i = 0; attrInfo.FindString("attr:name", i, &name) == B_OK; i++) {
        bool viewable = false;
        attrInfo.FindBool("attr:viewable", i, &viewable);
        if (!viewable)
            continue;

        bool known = false;
        for (const ShojiAttribute& attr : *attrs) {
            if (attr.name == name) {
                known = true;
                break;
            }
        }
        if (known)
            continue;

        ShojiAttribute attr;
        attr.name = name;

        const char* label = NULL;
        attrInfo.FindString("attr:public_name", i, &label);
        attr.label = (label != NULL && label[0] != '\0') ? label : name;

        int32 type = B_STRING_TYPE;
        attrInfo.FindInt32("attr:type", i, &type);
        attr.type = (type_code)type;

        attr.editable = false;
        attrInfo.FindBool("attr:editable", i, &attr.editable);

        attrs->push_back(attr);
    }
}

// --- reading: attribute data to text

template<typename T>
static bool Fetch(const uint8* data, ssize_t size, T* value)
{
    if (size != (ssize_t)sizeof(T))
        return false;
    memcpy(value, data, sizeof(T));
    return true;
}

template<typename T>
static status_t FormatNumber(const uint8* data, ssize_t size, const char* format, BString* text)
{
    T value;
    if (!Fetch(data, size, &value))
        return B_BAD_DATA;

    char buffer[64];
    if constexpr (std::is_floating_point<T>::value)
        snprintf(buffer, sizeof(buffer), format, (double)value);
    else if constexpr (std::is_signed<T>::value)
        snprintf(buffer, sizeof(buffer), format, (long long)value);
    else
        snprintf(buffer, sizeof(buffer), format, (unsigned long long)value);

    text->SetTo(buffer);
    return B_OK;
}

static status_t FormatValue(type_code type, const uint8* data, ssize_t size, BString* text)
{
    switch (type) {
        case B_STRING_TYPE:
            text->SetTo((const char*)data, strnlen((const char*)data, size));
            return B_OK;
        case B_BOOL_TYPE:
            if (size < 1)
                return B_BAD_DATA;
            text->SetTo(data[0] != 0 ? "1" : "0");
            return B_OK;
        case B_INT8_TYPE:   return FormatNumber<int8>(data, size, "%lld", text);
        case B_INT16_TYPE:  return FormatNumber<int16>(data, size, "%lld", text);
        case B_INT32_TYPE:  return FormatNumber<int32>(data, size, "%lld", text);
        case B_INT64_TYPE:  return FormatNumber<int64>(data, size, "%lld", text);
        case B_UINT8_TYPE:  return FormatNumber<uint8>(data, size, "%llu", text);
        case B_UINT16_TYPE: return FormatNumber<uint16>(data, size, "%llu", text);
        case B_UINT32_TYPE: return FormatNumber<uint32>(data, size, "%llu", text);
        case B_UINT64_TYPE: return FormatNumber<uint64>(data, size, "%llu", text);
        case B_FLOAT_TYPE:  return FormatNumber<float>(data, size, "%.7g", text);
        case B_DOUBLE_TYPE: return FormatNumber<double>(data, size, "%.15g", text);
        case B_TIME_TYPE: {
            // 32 or 64 bit, depending on who wrote it
            int64 seconds;
            if (size == (ssize_t)sizeof(int32)) {
                int32 value;
                memcpy(&value, data, sizeof(value));
                seconds = value;
            } else if (size == (ssize_t)sizeof(int64)) {
                memcpy(&seconds, data, sizeof(seconds));
            } else {
                return B_BAD_DATA;
            }
            time_t time = (time_t)seconds;
            struct tm parts;
            if (localtime_r(&time, &parts) == NULL)
                return B_BAD_DATA;
            char buffer[64];
            strftime(buffer, sizeof(buffer), kTimeFormat, &parts);
            text->SetTo(buffer);
            return B_OK;
        }
        default:
            return B_NOT_SUPPORTED;
    }
}

status_t LoadAttribute(BNode& node, ShojiAttribute* attr, BString* text)
{
    text->SetTo("");

    attr_info info;
    status_t status = node.GetAttrInfo(attr->name.String(), &info);
    if (status != B_OK)
        return status;

    attr->type = info.type;
    if (!IsSupportedType(info.type) || info.size > kMaxAttributeSize)
        return B_NOT_SUPPORTED;

    // one byte more, so that a string is terminated even if the stored one is not
    std::vector<uint8> data(info.size + 1, 0);
    ssize_t bytes = node.ReadAttr(attr->name.String(), info.type, 0, data.data(), info.size);
    if (bytes < 0)
        return (status_t)bytes;

    return FormatValue(info.type, data.data(), bytes, text);
}

// --- writing: text to attribute data

static void Append(std::vector<uint8>* data, const void* value, size_t size)
{
    const uint8* bytes = (const uint8*)value;
    data->insert(data->end(), bytes, bytes + size);
}

template<typename T>
static status_t ParseInteger(const char* text, std::vector<uint8>* data)
{
    errno = 0;
    char* end = NULL;
    T value;

    if constexpr (std::is_signed<T>::value) {
        long long parsed = strtoll(text, &end, 10);
        if (errno == ERANGE || parsed < std::numeric_limits<T>::min()
                || parsed > std::numeric_limits<T>::max())
            return B_BAD_VALUE;
        value = (T)parsed;
    } else {
        if (strchr(text, '-') != NULL)  // strtoull accepts a sign
            return B_BAD_VALUE;
        unsigned long long parsed = strtoull(text, &end, 10);
        if (errno == ERANGE || parsed > std::numeric_limits<T>::max())
            return B_BAD_VALUE;
        value = (T)parsed;
    }
    if (end == text || *end != '\0')
        return B_BAD_VALUE;

    Append(data, &value, sizeof(value));
    return B_OK;
}

static status_t ParseValue(type_code type, const char* text, std::vector<uint8>* data)
{
    switch (type) {
        case B_STRING_TYPE:
            Append(data, text, strlen(text) + 1);
            return B_OK;
        case B_BOOL_TYPE: {
            bool value;
            if (strcmp(text, "1") == 0 || strcasecmp(text, "true") == 0)
                value = true;
            else if (strcmp(text, "0") == 0 || strcasecmp(text, "false") == 0)
                value = false;
            else
                return B_BAD_VALUE;
            Append(data, &value, sizeof(value));
            return B_OK;
        }
        case B_INT8_TYPE:   return ParseInteger<int8>(text, data);
        case B_INT16_TYPE:  return ParseInteger<int16>(text, data);
        case B_INT32_TYPE:  return ParseInteger<int32>(text, data);
        case B_INT64_TYPE:  return ParseInteger<int64>(text, data);
        case B_UINT8_TYPE:  return ParseInteger<uint8>(text, data);
        case B_UINT16_TYPE: return ParseInteger<uint16>(text, data);
        case B_UINT32_TYPE: return ParseInteger<uint32>(text, data);
        case B_UINT64_TYPE: return ParseInteger<uint64>(text, data);
        case B_FLOAT_TYPE:
        case B_DOUBLE_TYPE: {
            errno = 0;
            char* end = NULL;
            double value = strtod(text, &end);
            if (end == text || *end != '\0' || errno == ERANGE || !std::isfinite(value))
                return B_BAD_VALUE;
            if (type == B_FLOAT_TYPE) {
                if (fabs(value) > FLT_MAX)
                    return B_BAD_VALUE;
                float number = (float)value;
                Append(data, &number, sizeof(number));
            } else {
                Append(data, &value, sizeof(value));
            }
            return B_OK;
        }
        case B_TIME_TYPE: {
            // date and time, or just a date
            static const char* formats[] = { kTimeFormat, "%Y-%m-%d %H:%M", "%Y-%m-%d" };
            for (const char* format : formats) {
                struct tm parts;
                memset(&parts, 0, sizeof(parts));
                parts.tm_isdst = -1;
                const char* rest = strptime(text, format, &parts);
                if (rest == NULL || *rest != '\0')
                    continue;
                time_t time = mktime(&parts);
                if (time == (time_t)-1)
                    return B_BAD_VALUE;
                Append(data, &time, sizeof(time));
                return B_OK;
            }
            return B_BAD_VALUE;
        }
        default:
            return B_NOT_SUPPORTED;
    }
}

status_t StoreAttribute(BNode& node, const ShojiAttribute& attr, const char* text)
{
    if (!IsSupportedType(attr.type))
        return B_NOT_SUPPORTED;

    BString value(text);
    if (attr.type != B_STRING_TYPE)
        value.Trim();

    // an empty value is no value: do not leave empty attributes behind
    if (value.IsEmpty() && attr.type != B_BOOL_TYPE) {
        status_t status = node.RemoveAttr(attr.name.String());
        return status == B_ENTRY_NOT_FOUND ? B_OK : status;
    }

    std::vector<uint8> data;
    status_t status = ParseValue(attr.type, value.String(), &data);
    if (status != B_OK)
        return status;

    // written at offset 0, which also replaces a longer value
    ssize_t written = node.WriteAttr(attr.name.String(), attr.type, 0, data.data(), data.size());
    if (written < 0)
        return (status_t)written;
    return written == (ssize_t)data.size() ? B_OK : B_IO_ERROR;
}

// --- snapshots

status_t SnapshotAttribute(BNode& node, const char* name, ShojiAttributeSnapshot* snapshot)
{
    *snapshot = ShojiAttributeSnapshot();

    attr_info info;
    status_t status = node.GetAttrInfo(name, &info);
    if (status == B_ENTRY_NOT_FOUND)
        return B_OK;
    if (status != B_OK)
        return status;
    if (info.size > kMaxAttributeSize)
        return B_NOT_SUPPORTED;

    snapshot->data.resize(info.size);
    ssize_t bytes = node.ReadAttr(name, info.type, 0, snapshot->data.data(), info.size);
    if (bytes < 0)
        return (status_t)bytes;

    snapshot->data.resize(bytes);
    snapshot->present = true;
    snapshot->type = info.type;
    return B_OK;
}

bool SnapshotMatches(BNode& node, const char* name, const ShojiAttributeSnapshot& snapshot)
{
    ShojiAttributeSnapshot current;
    if (SnapshotAttribute(node, name, &current) != B_OK)
        return false;

    return current.present == snapshot.present && current.type == snapshot.type && current.data == snapshot.data;
}

status_t RestoreAttribute(BNode& node, const char* name, const ShojiAttributeSnapshot& snapshot)
{
    if (!snapshot.present) {
        status_t status = node.RemoveAttr(name);
        return status == B_ENTRY_NOT_FOUND ? B_OK : status;
    }

    ssize_t written = node.WriteAttr(name, snapshot.type, 0, snapshot.data.data(), snapshot.data.size());
    if (written < 0)
        return (status_t)written;
    return written == (ssize_t)snapshot.data.size() ? B_OK : B_IO_ERROR;
}

// --- controls

bool GetControlValue(BView* view, BString* text)
{
    if (BTextControl* textControl = dynamic_cast<BTextControl*>(view)) {
        text->SetTo(textControl->Text());
        return true;
    }
    if (BCheckBox* checkBox = dynamic_cast<BCheckBox*>(view)) {
        text->SetTo(checkBox->Value() == B_CONTROL_ON ? "1" : "0");
        return true;
    }
    return false;
}

bool SetControlValue(BView* view, const char* text)
{
    if (BTextControl* textControl = dynamic_cast<BTextControl*>(view)) {
        // leave the caret alone if there is nothing to change
        if (strcmp(textControl->Text(), text) != 0)
            textControl->SetText(text);
        return true;
    }
    if (BCheckBox* checkBox = dynamic_cast<BCheckBox*>(view)) {
        checkBox->SetValue(strcmp(text, "1") == 0 ? B_CONTROL_ON : B_CONTROL_OFF);
        return true;
    }
    return false;
}

}   // namespace shoji
