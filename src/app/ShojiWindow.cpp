/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */

#include <Alert.h>
#include <Invoker.h>
#include <NodeMonitor.h>
#include <Bitmap.h>
#include <Button.h>
#include <ControlLook.h>
#include <CheckBox.h>
#include <Entry.h>
#include <Errors.h>
#include <DataIO.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <SpaceLayoutItem.h>
#include <cstdio>
#include <fs_attr.h>
#include <MimeType.h>
#include <NodeInfo.h>
#include <Path.h>
#include <TextControl.h>
#include <TranslationUtils.h>
#include <Volume.h>
#include <unistd.h>

#include <sen/SenStripeDialog.h>

#include "ShojiWindow.h"
#include "ShojiView.h"
#include "ShojiGenericFormView.h"
#include "ShojiTemplateView.h"

static const char* kAttrThumbnail = "Media:Thumbnail";
static const ssize_t kMaxThumbnailSize = 4 * 1024 * 1024;
static const float kPictureHeight = 128;
static const uint32 kMsgRevert = 'SHrv';
static const uint32 kMsgExternalChoice = 'SHxc';

ShojiWindow::ShojiWindow(const entry_ref* ref) : BWindow(
    BRect(BPoint(320.0, 128.0), BSize(580.0, 576.0)),
    "Shoji Entity Viewer",
    B_DOCUMENT_WINDOW,
    B_WILL_ACCEPT_FIRST_CLICK),
    fRef(*ref),
    fWritable(false),
    fEntityView(NULL),
    fRevertButton(NULL)
{
	// as the About windows of SEN: the picture on a dark stripe on the left, the entity on the light panel
	BGroupLayout* root = new BGroupLayout(B_HORIZONTAL, 0);
	SetLayout(root);
	root->Owner()->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    UpdateTitle();
    status_t result;

    // get MIME type for view template lookup
    char mimeTypeStr[B_MIME_TYPE_LENGTH];
    BMimeType mimeType;

    result = GetMimeTypeForRef(ref, mimeTypeStr);
    if (result == B_OK) {
        mimeType.SetTo(mimeTypeStr);
        result = mimeType.InitCheck();
    }
    if (result != B_OK) {
        ShowUserError("MIME type lookup error", "Could not identify MIME type of file.", result);
        PostMessage(B_QUIT_REQUESTED);
        return;
    }
    // store MIME type attribute info to check for displayable or editable flags
    BMessage mimeAttrInfo;
    result = mimeType.GetAttrInfo(&mimeAttrInfo);
    if (result != B_OK) {
        ShowUserError("MIME attrInfo lookup error", "Could not identify MIME type attributeInfo for filetype.", result);
        PostMessage(B_QUIT_REQUESTED);
        return;
    }
    BMimeType superType;
    BMessage superTypeAttrInfo;
    if (mimeType.GetSupertype(&superType) == B_OK)
        superType.GetAttrInfo(&superTypeAttrInfo);

    // attributes are edited only if the file can be written
    fWritable = IsFileWritable();

    result = LoadAttributes(&mimeAttrInfo, &superTypeAttrInfo);
    if (result != B_OK) {
        ShowUserError("Error opening view", "Could not map data for display.", result);
        PostMessage(B_QUIT_REQUESTED);
        return;
    }

    // look up suitable template by mimeType
    fEntityView = GetViewTemplateForType(mimeType.Type());
    if (fEntityView == NULL) {
        PostMessage(B_QUIT_REQUESTED);
        return;
    }

    fEntityView->Populate(fAttrs, &fValues);

    BGroupView* panel = new BGroupView(B_VERTICAL, 0);
    panel->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    panel->GroupLayout()->SetInsets(0, sen::DialogMargin(), sen::DialogMargin(), sen::DialogMargin());
    panel->GroupLayout()->SetSpacing(be_control_look->DefaultItemSpacing());
    // the entity takes the room that the window gets, not just what its controls ask for
    fEntityView->GetView()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
    panel->AddChild(fEntityView->GetView());

    // the changes are stored as they are made: this takes them back, to the file as it was opened
    fRevertButton = new BButton("revert", "Revert", new BMessage(kMsgRevert));
    fRevertButton->SetEnabled(false);
    BGroupView* buttons = new BGroupView(B_HORIZONTAL, 0);
    buttons->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
    buttons->GroupLayout()->AddView(fRevertButton);
    panel->AddChild(buttons);

    root->AddView(new sen::StripeView(LoadPicture(kPictureHeight)));
    root->AddView(panel);
    ConnectControls();

    // others may change the attributes while the window is open
    BNode node(&fRef);
    if (node.GetNodeRef(&fNodeRef) == B_OK)
        watch_node(&fNodeRef, B_WATCH_ATTR, this);
}

ShojiWindow::~ShojiWindow()
{
    stop_watching(this);

    if (fEntityView != NULL) {
        // the window deletes its views; one that never was added (a failed setup) is ours
        BView* view = fEntityView->GetView();
        if (view != NULL && view->Window() == NULL && view->Parent() == NULL)
            delete view;
        delete fEntityView;
    }
}

// The thumbnail of the file (the standard attribute Media:Thumbnail, an encoded image, as Tracker and BERT have it),
// scaled down to the height; else the icon of the file, which is a vector icon and so as sharp as it is large.
BBitmap* ShojiWindow::LoadPicture(float height)
{
    BNode node(&fRef);
    attr_info info;
    if (node.GetAttrInfo(kAttrThumbnail, &info) == B_OK && info.size > 0 && info.size <= kMaxThumbnailSize) {
        BMallocIO data;
        data.SetSize(info.size);
        if (node.ReadAttr(kAttrThumbnail, info.type, 0, (void*)data.Buffer(), info.size) == info.size) {
            BBitmap* thumbnail = BTranslationUtils::GetBitmap(&data);
            BBitmap* scaled = sen::ScaleDown(thumbnail, height);
            delete thumbnail;
            if (scaled != NULL)
                return scaled;
        }
    }

    BBitmap* icon = new BBitmap(BRect(0, 0, height - 1, height - 1), B_RGBA32);
    if (icon->IsValid() && BNodeInfo::GetTrackerIcon(&fRef, icon, (icon_size)(int)height) == B_OK)
        return icon;

    delete icon;
    return NULL;
}

void ShojiWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case SHOJI_MSG_ATTR_EDITED:
        case SHOJI_MSG_ATTR_COMMIT:
        {
            const char* name = NULL;
            if (message->FindString(SHOJI_FIELD_ATTR, &name) != B_OK)
                break;

            if (message->what == SHOJI_MSG_ATTR_COMMIT) {
                Commit(name);
            } else {
                // typing back to the stored value is no change
                if (HasChanged(name))
                    fDirty.insert(name);
                else
                    fDirty.erase(name);
                UpdateTitle();
                UpdateRevertButton();
            }
            break;
        }
        case kMsgRevert:
            Revert();
            break;
        case B_NODE_MONITOR:
        {
            int32 opcode;
            const char* name;
            if (message->FindInt32("opcode", &opcode) == B_OK && opcode == B_ATTR_CHANGED
                    && message->FindString("attr", &name) == B_OK)
                ExternalChange(name);
            break;
        }
        case kMsgExternalChoice:
        {
            // which: 0 keeps the change in the field, 1 takes the value of the file
            const char* name;
            int32 which = 0;
            if (message->FindString(SHOJI_FIELD_ATTR, &name) != B_OK)
                break;
            message->FindInt32("which", &which);
            fAsking.erase(name);
            if (which == 1)
                AdoptFileValue(name);
            break;
        }
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

// Changes are stored when the control sends its message: a text control does that on Enter and, as Haiku has it, when the field is
// left (click elsewhere, Tab) with a text that is not the one it got the focus with; a check box on each click.
// A field keeps the focus when its window is left, so that, and closing the window, store the rest.
bool ShojiWindow::QuitRequested()
{
    return CommitAll() == B_OK;
}

void ShojiWindow::WindowActivated(bool active)
{
    BWindow::WindowActivated(active);
    if (!active)
        CommitAll();
}

bool ShojiWindow::IsFileWritable() const
{
    BVolume volume(fRef.device);
    if (volume.InitCheck() != B_OK || volume.IsReadOnly() || !volume.KnowsAttr())
        return false;

    BPath path(&fRef);
    return path.InitCheck() == B_OK && access(path.Path(), W_OK) == 0;
}

// Collects the viewable attributes of the MIME type and its supertype, and their values in the file.
status_t ShojiWindow::LoadAttributes(const BMessage* mimeAttrInfo, const BMessage* superTypeAttrInfo)
{
    shoji::AttributesFromMimeInfo(*mimeAttrInfo, &fAttrs);
    shoji::AttributesFromMimeInfo(*superTypeAttrInfo, &fAttrs);

    BNode node(&fRef);
    status_t result = node.InitCheck();
    if (result != B_OK)
        return result;

    for (ShojiAttribute& attr : fAttrs) {
        BString text;
        result = shoji::LoadAttribute(node, &attr, &text);
        if (result == B_OK) {
            fValues.AddString(attr.name.String(), text);
        } else if (result != B_ENTRY_NOT_FOUND) {
            // unsupported or unreadable value: shown, but never overwritten
            attr.editable = false;
        }
        if (!shoji::IsSupportedType(attr.type) || !fWritable)
            attr.editable = false;

        // what to go back to
        if (attr.editable) {
            ShojiAttributeSnapshot snapshot;
            if (shoji::SnapshotAttribute(node, attr.name.String(), &snapshot) == B_OK)
                fOriginal[attr.name.String()] = snapshot;
            else
                attr.editable = false;
        }
    }

    return B_OK;
}

// controls are named after the attribute they show, whether they come from a template or are generated.
void ShojiWindow::ConnectControls()
{
    for (const ShojiAttribute& attr : fAttrs) {
        if (!attr.editable)
            continue;

        BMessage* commit = new BMessage(SHOJI_MSG_ATTR_COMMIT);
        commit->AddString(SHOJI_FIELD_ATTR, attr.name.String());

        BView* view = FindControl(attr.name.String());
        if (BTextControl* textControl = dynamic_cast<BTextControl*>(view)) {
            BMessage* edited = new BMessage(SHOJI_MSG_ATTR_EDITED);
            edited->AddString(SHOJI_FIELD_ATTR, attr.name.String());

            textControl->SetModificationMessage(edited);
            textControl->SetMessage(commit);
            textControl->SetTarget(this);
        } else if (BCheckBox* checkBox = dynamic_cast<BCheckBox*>(view)) {
            checkBox->SetMessage(commit);
            checkBox->SetTarget(this);
        } else {
            delete commit;
        }
    }
}

BView* ShojiWindow::FindControl(const char* name)
{
    return fEntityView != NULL ? fEntityView->GetView()->FindView(name) : NULL;
}

const ShojiAttribute* ShojiWindow::FindAttribute(const char* name) const
{
    for (const ShojiAttribute& attr : fAttrs) {
        if (attr.name == name)
            return &attr;
    }
    return NULL;
}

// does the control differ from what the file has?
bool ShojiWindow::HasChanged(const char* name)
{
    BString text;
    if (!shoji::GetControlValue(FindControl(name), &text))
        return false;

    BString stored;
    fValues.FindString(name, &stored);
    return text != stored;
}

// stores the value of the control in the attribute and shows what has been stored.
status_t ShojiWindow::Commit(const char* name)
{
    fDirty.erase(name);
    UpdateTitle();
    UpdateRevertButton();

    const ShojiAttribute* attr = FindAttribute(name);
    BView* view = FindControl(name);
    BString text;
    if (attr == NULL || !attr->editable || !shoji::GetControlValue(view, &text))
        return B_OK;

    BString stored;
    fValues.FindString(name, &stored);
    if (text == stored)
        return B_OK;

    BNode node(&fRef);
    status_t result = node.InitCheck();
    if (result == B_OK)
        result = shoji::StoreAttribute(node, *attr, text.String());
    if (result != B_OK) {
        // back to what the file has, so that the view does not claim a value that is not there
        shoji::SetControlValue(view, stored.String());
        ShowUserEditError(*attr, result);
        return result;
    }

    // read it back: the stored form can differ from the typed one (7 for 007, the form of a date)
    ShojiAttribute reread(*attr);
    BString current;
    result = shoji::LoadAttribute(node, &reread, &current);
    if (result == B_OK || result == B_ENTRY_NOT_FOUND) {
        shoji::SetControlValue(view, current.String());
        fValues.RemoveName(name);
        if (result == B_OK)
            fValues.AddString(name, current);
    }
    UpdateRevertButton();

    return B_OK;
}

// puts all attributes back as the file had them when it was opened
void ShojiWindow::Revert()
{
    fDirty.clear();

    BNode node(&fRef);
    status_t result = node.InitCheck();

    for (const ShojiAttribute& attr : fAttrs) {
        std::map<std::string, ShojiAttributeSnapshot>::const_iterator original = fOriginal.find(attr.name.String());
        if (original == fOriginal.end())
            continue;

        if (result == B_OK && !shoji::SnapshotMatches(node, attr.name.String(), original->second)) {
            status_t status = shoji::RestoreAttribute(node, attr.name.String(), original->second);
            if (status != B_OK)
                ShowUserEditError(attr, status);
        }

        // show what the file has now
        ShojiAttribute reread(attr);
        BString text;
        status_t status = shoji::LoadAttribute(node, &reread, &text);
        if (status == B_OK || status == B_ENTRY_NOT_FOUND) {
            shoji::SetControlValue(FindControl(attr.name.String()), text.String());
            fValues.RemoveName(attr.name.String());
            if (status == B_OK)
                fValues.AddString(attr.name.String(), text);
        }
    }
    UpdateTitle();
    UpdateRevertButton();
}

// An attribute was changed, by this window or by someone else. Our own writes find the file as we know it and are ignored.
// A field that is not being edited shows the new value; one with a change that is not stored yet asks what to do.
// The new value is also the one that Revert goes back to: it must not put back what others have replaced since.
void ShojiWindow::ExternalChange(const char* name)
{
    const ShojiAttribute* attr = FindAttribute(name);
    BNode node(&fRef);
    if (attr == NULL || node.InitCheck() != B_OK)
        return;

    ShojiAttribute reread(*attr);
    BString text;
    status_t status = shoji::LoadAttribute(node, &reread, &text);
    if (status != B_OK && status != B_ENTRY_NOT_FOUND)
        return;
    bool present = status == B_OK;

    BString known;
    bool wasPresent = fValues.FindString(name, &known) == B_OK;
    if (present == wasPresent && text == known)
        return;

    BString mine;
    bool editing = fDirty.count(name) > 0 && shoji::GetControlValue(FindControl(name), &mine) && mine != text;
    if (!editing) {
        AdoptFileValue(name);
        return;
    }

    // the field is compared to the file as it is now, and Revert goes back to that
    fValues.RemoveName(name);
    if (present)
        fValues.AddString(name, text);
    if (fOriginal.count(name) > 0) {
        ShojiAttributeSnapshot snapshot;
        if (shoji::SnapshotAttribute(node, name, &snapshot) == B_OK)
            fOriginal[name] = snapshot;
    }
    UpdateRevertButton();

    if (fAsking.insert(name).second)
        AskAboutExternalChange(*attr, text.String(), mine.String());
}

// shows the value that the file has now, and takes it as the one to revert to
void ShojiWindow::AdoptFileValue(const char* name)
{
    const ShojiAttribute* attr = FindAttribute(name);
    BNode node(&fRef);
    if (attr == NULL || node.InitCheck() != B_OK)
        return;

    ShojiAttribute reread(*attr);
    BString text;
    status_t status = shoji::LoadAttribute(node, &reread, &text);
    if (status != B_OK && status != B_ENTRY_NOT_FOUND)
        return;

    fValues.RemoveName(name);
    if (status == B_OK)
        fValues.AddString(name, text);

    if (fOriginal.count(name) > 0) {
        ShojiAttributeSnapshot snapshot;
        if (shoji::SnapshotAttribute(node, name, &snapshot) == B_OK)
            fOriginal[name] = snapshot;
    }

    fDirty.erase(name);
    shoji::SetControlValue(FindControl(name), text.String());
    UpdateTitle();
    UpdateRevertButton();
}

// does not block the window; the answer comes back as a message
void ShojiWindow::AskAboutExternalChange(const ShojiAttribute& attr, const char* external, const char* mine)
{
    BString theirs(external), yours(mine);
    theirs.Truncate(60);
    yours.Truncate(60);

    BString msg("'");
    msg << attr.label << "' was changed outside of Shoji while you are editing it.\n\n"
        << "New value:  " << (theirs.IsEmpty() ? "(removed)" : theirs.String()) << "\n"
        << "Your value:  " << (yours.IsEmpty() ? "(empty)" : yours.String());

    BAlert* alert = new BAlert("Attribute changed", msg.String(), "Keep my change", "Use the new value", NULL,
        B_WIDTH_AS_USUAL, B_WARNING_ALERT);
    alert->SetShortcut(0, B_ESCAPE);

    BMessage* choice = new BMessage(kMsgExternalChoice);
    choice->AddString(SHOJI_FIELD_ATTR, attr.name.String());
    alert->Go(new BInvoker(choice, this));
}

// is there anything to go back to: a change in the fields not stored yet, or an attribute that differs from the opened file
bool ShojiWindow::HasChangesSinceOpen()
{
    if (!fDirty.empty())
        return true;

    BNode node(&fRef);
    if (node.InitCheck() != B_OK)
        return false;

    for (const auto& original : fOriginal) {
        if (!shoji::SnapshotMatches(node, original.first.c_str(), original.second))
            return true;
    }
    return false;
}

void ShojiWindow::UpdateRevertButton()
{
    if (fRevertButton != NULL)
        fRevertButton->SetEnabled(HasChangesSinceOpen());
}

status_t ShojiWindow::CommitAll()
{
    status_t result = B_OK;
    std::set<std::string> dirty(fDirty);

    for (const std::string& name : dirty) {
        status_t status = Commit(name.c_str());
        if (status != B_OK)
            result = status;
    }
    return result;
}

void ShojiWindow::UpdateTitle()
{
    BString title;
    if (!fDirty.empty())
        title << "* ";
    title << fRef.name << " - Shoji";
    SetTitle(title.String());
}

// look up suitable view based on MIME type, fall back to generic form view if not available.
ShojiView* ShojiWindow::GetViewTemplateForType(const char* mimeType) {
    ShojiView* view = new ShojiTemplateView(mimeType);
    status_t status = view->Initialize();

    if (status == B_ENTRY_NOT_FOUND) {
        printf("no template found for type %s, falling back to generic form view.\n", mimeType);
        delete view;
        view = new ShojiGenericFormView();
        status = view->Initialize();
    }

    if (status != B_OK) {
        BString detail("Failed to set up view.");
        ShowUserError("Error", detail.String(), status);
        delete view;
        return NULL;
    }

    return view;
}

status_t ShojiWindow::GetMimeTypeForRef(const entry_ref* ref, char* mimeType) {
    BNode sourceNode(ref);
    status_t result;
    if ((result = sourceNode.InitCheck()) != B_OK) {
        return result;
    }
    BNodeInfo sourceInfo(&sourceNode);
    if ((result = sourceInfo.InitCheck()) != B_OK) {
        return result;
    }
    if ((result = sourceInfo.GetType(mimeType)) != B_OK) {
        return result;
    }

    return B_OK;
}

void ShojiWindow::ShowUserError(const char *title, const char* message, status_t errorCode)
{
    BString msg(message);
    msg.Append("\nDetail: ").Append(strerror(errorCode));

    BAlert* alert = new BAlert(
        title,
        msg.String(),
        "Oh no.");

    alert->SetFlags(alert->Flags() | B_WARNING_ALERT | B_CLOSE_ON_ESCAPE);
    alert->Go();
}

// does not block the window, it may be shown while the window is left.
void ShojiWindow::ShowUserEditError(const ShojiAttribute& attr, status_t errorCode)
{
    BString msg("Could not store the value of '");
    msg << attr.label << "'.\n";
    if (errorCode == B_BAD_VALUE)
        msg << "The value is not valid for this attribute, the previous one has been restored.";
    else
        msg << "Detail: " << strerror(errorCode);

    BAlert* alert = new BAlert("Error storing value", msg.String(), "Oh no.");
    alert->SetFlags(alert->Flags() | B_WARNING_ALERT | B_CLOSE_ON_ESCAPE);
    alert->Go(NULL);
}
