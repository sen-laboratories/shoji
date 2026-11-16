/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "ShojiStateIcon.h"

#include <Bitmap.h>
#include <File.h>
#include <FindDirectory.h>
#include <IconUtils.h>
#include <Path.h>
#include <Resources.h>
#include <View.h>

#include <math.h>

#include <sen/SenSettingsForm.h>

namespace form = sen::settings::form;


/** The vector icon of an alert of the system (`dialog-information`, `dialog-warning`, `dialog-error`, resources of libbe). */
static BBitmap*
AlertIcon(const char* name, float size)
{
	BPath path;
	if (find_directory(B_SYSTEM_LIB_DIRECTORY, &path) != B_OK || path.Append("libbe.so") != B_OK)
		return NULL;
	BFile file(path.Path(), B_READ_ONLY);
	BResources resources(&file);
	size_t length = 0;
	const void* data = resources.LoadResource(B_VECTOR_ICON_TYPE, name, &length);
	if (data == NULL)
		return NULL;
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	if (bitmap->InitCheck() != B_OK || BIconUtils::GetVectorIcon((const uint8*)data, length, bitmap) != B_OK) {
		delete bitmap;
		return NULL;
	}
	return bitmap;
}


/** A green check mark on a transparent ground, drawn with a round pen so that it is as clear as the alert icons at the size of a line of text. */
static BBitmap*
CheckMarkIcon(float size)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	BView* view = new BView(bitmap->Bounds(), "check", B_FOLLOW_NONE, B_WILL_DRAW | B_SUBPIXEL_PRECISE);
	bitmap->AddChild(view);
	if (bitmap->Lock()) {
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(0, 0, 0, 0);
		view->FillRect(view->Bounds());
		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		view->SetHighColor(40, 160, 70, 255);
		view->SetPenSize(fmaxf(2.0f, floorf(size / 5)));
		view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
		view->MovePenTo(BPoint(size * 0.18f, size * 0.55f));
		view->StrokeLine(BPoint(size * 0.18f, size * 0.55f), BPoint(size * 0.42f, size * 0.78f));
		view->StrokeLine(BPoint(size * 0.42f, size * 0.78f), BPoint(size * 0.84f, size * 0.24f));
		view->Sync();
		bitmap->Unlock();
	}
	bitmap->RemoveChild(view);
	delete view;
	return bitmap;
}


BBitmap*
ShojiStateIcon(const std::string& state, float size)
{
	if (state == form::kStateOk)
		return CheckMarkIcon(size);
	const char* name = state == form::kStateInfo ? "dialog-information" : state == form::kStateWarning ? "dialog-warning"
		: state == form::kStateError ? "dialog-error" : NULL;
	return name != NULL ? AlertIcon(name, size) : NULL;
}
