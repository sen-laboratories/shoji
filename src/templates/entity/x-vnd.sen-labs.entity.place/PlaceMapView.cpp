/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PlaceMapView.h"

#include <Autolock.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Font.h>
#include <LayoutUtils.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Message.h>
#include <Path.h>
#include <Polygon.h>
#include <PopUpMenu.h>
#include <String.h>
#include <TranslationUtils.h>
#include <sen/SenSocketHttp.h>
#include <Window.h>

#include <sen/SenMapStyles.h>

#include "../../../app/ShojiSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <set>
#include <string>
#include <vector>

namespace {

const float kMapMinHeight = 260;
const float kMapMinWidth = 300;
const int kMinZoom = 2;
const int kMaxZoom = 17;				// the last zoom of the tiles of OpenTopoMap: scaled up they are blurred and lose their shading, so the map does not go closer
const int kMaxTileZoom = 17;
const int kPointZoom = 15;				// a place that is a point only: its street
const double kFill = 0.7;				// the box of a place takes at most this much of the view
const time_t kTileLifetime = 14 * 24 * 3600;
const uint32 kTileArrived = 'mTle';
const bigtime_t kTileTimeout = 5 * 1000000LL;			// a server that does not answer in this time is given up for the tile (the standard map is asked); the client waits 20 s by default
const bigtime_t kServerPause = 60 * 1000000LL;			// ... and is not asked for the other tiles for this long
const int kFetchThreads = 2;							// (the tile usage policy of OpenStreetMap allows two connections)
const uint32 kStyleChosen = 'mSty';
const uint32 kSpin = 'mSpn';
const bigtime_t kSpinInterval = 90000;				// (about 11 steps a second)
const int kSpokes = 12;
const char* const kUserAgent = "SEN-Shoji/1.0 (Haiku; https://sen-labs.org)";

// where the tiles come from: the style that the user chose (sen::mapstyles: the choice made in the view, else the default of the settings of the spatial navigator), and if its server does not answer the standard map of OpenStreetMap
using sen::mapstyles::Style;
const Style* FallbackStyle() { return sen::mapstyles::Find(sen::mapstyles::FallbackId()); }

unsigned long long TileKey(int zoom, int x, int y)
{
    return ((unsigned long long) zoom << 56) | ((unsigned long long) x << 28) | (unsigned long long) y;
}

bool ParseDegrees(const BMessage* values, const char* name, double* value)
{
    BString text;
    if (values->FindString(name, &text) != B_OK || text.IsEmpty())
        return false;
    char* end = NULL;
    *value = strtod(text.String(), &end);
    return end != text.String();
}

bool TilePath(const Style& style, int zoom, int x, int y, BPath* path)
{
    if (find_directory(B_USER_CACHE_DIRECTORY, path) != B_OK || path->Append("shoji/tiles") != B_OK || path->Append(style.id) != B_OK)
        return false;
    BString name;
    name << zoom << "/" << x << "/" << y << ".png";
    return path->Append(name.String()) == B_OK;
}

}   // namespace


/** the tiles that are asked for, the thread that asks, and where to tell the view; shared with the thread, so that the view can go while a request is under way */
struct PlaceMapView::Fetcher {
    struct Request { const sen::mapstyles::Style* style; int zoom, x, y; };

    BLocker             lock;
    std::deque<Request> queue;
    std::set<unsigned long long> asked;
    BMessenger          view;
    bool                quit = false;
    int                 workers = 0;
    const sen::mapstyles::Style* downStyle = NULL;	// a style whose server did not answer: it is not asked until downUntil, the tiles come from the standard map
    bigtime_t           downUntil = 0;

    Fetcher() : lock("tile fetcher") {}
};


/** one tile from the server of the style into the file of the cache (the client over a socket of SEN, see SenSocketHttp.h: the same one that the plugins use) */
static status_t FetchTile(const Style& style, int zoom, int x, int y, const BPath& path)
{
    const std::string address = sen::mapstyles::TileUrl(style, zoom, x, y);
    std::string body;
    int code = 0;
    status_t status = sen::http::SocketGet(address.c_str(), kUserAgent, "image/png", &body, &code, kTileTimeout);
    if (status != B_OK || code != 200 || body.empty())
        return status != B_OK ? status : B_ERROR;

    BPath folder;
    if (path.GetParent(&folder) != B_OK || create_directory(folder.Path(), 0755) != B_OK)
        return B_ERROR;
    // (written under another name and then renamed: a view that reads the file never sees half of it)
    BString temporary(path.Path());
    temporary << ".part";
    BFile file(temporary.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK || file.Write(body.data(), body.size()) != (ssize_t) body.size())
        return B_ERROR;
    file.Unset();
    return rename(temporary.String(), path.Path()) == 0 ? B_OK : B_ERROR;
}


static int32 FetchThread(void* data)
{
    std::shared_ptr<PlaceMapView::Fetcher>* held = (std::shared_ptr<PlaceMapView::Fetcher>*) data;
    std::shared_ptr<PlaceMapView::Fetcher> fetcher = *held;
    delete held;

    for (;;) {
        PlaceMapView::Fetcher::Request next;
        {
            BAutolock locker(fetcher->lock);
            if (fetcher->quit || fetcher->queue.empty()) {
                fetcher->workers--;
                return 0;
            }
            next = fetcher->queue.front();
            fetcher->queue.pop_front();
        }
        // (the style that was chosen; if its server does not answer, the standard map is asked for this tile)
        bool got = false;
        const Style* chain[2] = { next.style, FallbackStyle() };
        bool skipFirst;
        {
            BAutolock locker(fetcher->lock);
            skipFirst = fetcher->downStyle == next.style && system_time() < fetcher->downUntil && chain[1] != chain[0];
        }
        for (int i = skipFirst ? 1 : 0; i < 2 && !got; i++) {
            if (i == 1 && chain[1] == chain[0])
                break;
            BPath path;
            if (!TilePath(*chain[i], next.zoom, next.x, next.y, &path))
                continue;
            const status_t status = FetchTile(*chain[i], next.zoom, next.x, next.y, path);
            got = status == B_OK;
            if (i == 0 && status != B_OK && status != B_ENTRY_NOT_FOUND) {
                // (a server that does not answer: the tiles that are still to come do not wait for it again)
                BAutolock locker(fetcher->lock);
                fetcher->downStyle = next.style;
                fetcher->downUntil = system_time() + kServerPause;
            }
        }
        if (got) {
            BMessage arrived(kTileArrived);
            arrived.AddInt32("zoom", next.zoom);
            arrived.AddInt32("x", next.x);
            arrived.AddInt32("y", next.y);
            fetcher->view.SendMessage(&arrived);
        }
        snooze(50000);	// (a few tiles, and not in a burst)
    }
}


// ---- the arithmetic

double PlaceMapView::WorldX(double longitude, int zoom)
{
    return (longitude + 180.0) / 360.0 * 256.0 * (double) (1 << zoom);
}


double PlaceMapView::WorldY(double latitude, int zoom)
{
    // (the map ends at 85.0511 degrees)
    double clipped = std::max(-85.0511, std::min(85.0511, latitude));
    double radians = clipped * M_PI / 180.0;
    return (1.0 - log(tan(radians) + 1.0 / cos(radians)) / M_PI) / 2.0 * 256.0 * (double) (1 << zoom);
}


int PlaceMapView::ZoomFor(double south, double north, double west, double east, double width, double height)
{
    for (int zoom = kMaxZoom; zoom > kMinZoom; zoom--) {
        double spanX = WorldX(east, zoom) - WorldX(west, zoom);
        double spanY = WorldY(south, zoom) - WorldY(north, zoom);
        if (spanX <= width * kFill && spanY <= height * kFill)
            return zoom;
    }
    return kMinZoom;
}


// ---- the view

PlaceMapView::PlaceMapView()
    :
    BView(ViewName(), B_WILL_DRAW | B_FRAME_EVENTS)
{
    _Init();
}


PlaceMapView::PlaceMapView(BMessage* archive)
    :
    BView(archive)
{
    _Init();
}


void PlaceMapView::_Init()
{
    fHasPosition = false;
    fLatitude = fLongitude = 0;
    fHasBox = false;
    fSouth = fNorth = fWest = fEast = 0;
    fZoom = kPointZoom;
    fAnimation = NULL;
    fSpin = 0;
    fLoading = false;
    fFetcher = std::make_shared<Fetcher>();
    fStyle = NULL;
    _LoadStyle();
    SetViewColor(B_TRANSPARENT_COLOR);
    SetExplicitMinSize(BSize(kMapMinWidth, kMapMinHeight));
}


PlaceMapView::~PlaceMapView()
{
    {
        BAutolock locker(fFetcher->lock);
        fFetcher->quit = true;
        fFetcher->queue.clear();
    }
    delete fAnimation;
    for (auto& tile : fTiles)
        delete tile.second;
}


BArchivable* PlaceMapView::Instantiate(BMessage* archive)
{
    if (!validate_instantiation(archive, "PlaceMapView"))
        return NULL;
    return new PlaceMapView(archive);
}


status_t PlaceMapView::Archive(BMessage* into, bool deep) const
{
    return BView::Archive(into, deep);
}


/** The style from the settings (they may have been changed in the settings window, or by the menu of another map). @return whether it is another one than before. */
bool PlaceMapView::_LoadStyle()
{
    shoji::Settings settings;
    settings.Load();
    const Style* style = &sen::mapstyles::Get(settings.EffectiveMapStyle());
    if (style == fStyle)
        return false;
    fStyle = style;
    return true;
}


/** the tiles of another style are not those that are in the memory, and what is asked for is not what is wanted any more */
void PlaceMapView::_ClearTiles()
{
    for (auto& tile : fTiles)
        delete tile.second;
    fTiles.clear();
    fPrimaryTiles.clear();
    BAutolock locker(fFetcher->lock);
    fFetcher->queue.clear();
    fFetcher->asked.clear();
}


void PlaceMapView::SetStyle(const char* id)
{
    // (an empty id takes the choice back: the default of the settings of the spatial navigator is used again)
    shoji::Settings settings;
    settings.Load();
    settings.SetMapStyle(id);
    settings.Save();
    if (_LoadStyle()) {
        _ClearTiles();
        _Layout();
        Invalidate();
    }
}


void PlaceMapView::MouseDown(BPoint where)
{
    int32 buttons = 0;
    if (Window() != NULL && Window()->CurrentMessage() != NULL)
        Window()->CurrentMessage()->FindInt32("buttons", &buttons);
    if ((buttons & B_SECONDARY_MOUSE_BUTTON) == 0) {
        BView::MouseDown(where);
        return;
    }
    // the styles of the map, the one in use marked; the first is the default of the settings of the spatial navigator (marked while no style was chosen here)
    shoji::Settings settings;
    settings.Load();
    const bool chosen = !settings.MapStyle().empty();
    const std::string pluginDefault = shoji::Settings::PluginMapStyle();
    BPopUpMenu* menu = new BPopUpMenu("map style", false, false);
    {
        // ("Default: Humanitarian (warm, clear)": the label of a style has brackets itself)
        BString label("Default: ");
        label << sen::mapstyles::Get(pluginDefault).label;
        BMessage* choice = new BMessage(kStyleChosen);
        choice->AddString("style", "");
        BMenuItem* item = new BMenuItem(label.String(), choice);
        item->SetMarked(!chosen);
        item->SetTarget(this);
        menu->AddItem(item);
        menu->AddSeparatorItem();
    }
    for (const Style& style : sen::mapstyles::All()) {
        BMessage* choice = new BMessage(kStyleChosen);
        choice->AddString("style", style.id);
        BMenuItem* item = new BMenuItem(style.label, choice);
        item->SetMarked(chosen && &style == fStyle);
        item->SetTarget(this);
        menu->AddItem(item);
    }
    menu->Go(ConvertToScreen(where), true, true, true);
}


void PlaceMapView::SetPlace(const BMessage* values)
{
    // (a style that was chosen in the settings window since the map was made)
    if (_LoadStyle())
        _ClearTiles();
    double latitude, longitude, south, north, west, east;
    if (values == NULL || !ParseDegrees(values, "schema:latitude", &latitude) || !ParseDegrees(values, "schema:longitude", &longitude)) {
        fHasPosition = false;
        Invalidate();
        return;
    }
    bool hasBox = ParseDegrees(values, "SEN:boxSouth", &south) && ParseDegrees(values, "SEN:boxNorth", &north)
        && ParseDegrees(values, "SEN:boxWest", &west) && ParseDegrees(values, "SEN:boxEast", &east);
    SetPosition(latitude, longitude, hasBox, south, north, west, east);
}


void PlaceMapView::SetPosition(double latitude, double longitude, bool hasBox, double south, double north, double west, double east)
{
    fHasPosition = latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180;
    fLatitude = latitude;
    fLongitude = longitude;
    // (a box that is a point, or that does not hold the position, is none)
    fHasBox = hasBox && north > south && east > west && latitude >= south - 1e-6 && latitude <= north + 1e-6;
    fSouth = south;
    fNorth = north;
    fWest = west;
    fEast = east;
    _Layout();
    Invalidate();
}


void PlaceMapView::AttachedToWindow()
{
    BView::AttachedToWindow();
    fFetcher->view = BMessenger(this);
    _Layout();
}


void PlaceMapView::FrameResized(float width, float height)
{
    BView::FrameResized(width, height);
    _Layout();
    Invalidate();
}


void PlaceMapView::GetPreferredSize(float* width, float* height)
{
    if (width != NULL)
        *width = kMapMinWidth * 1.3f;
    if (height != NULL)
        *height = kMapMinHeight;
}


BSize PlaceMapView::MinSize()
{
    return BLayoutUtils::ComposeSize(ExplicitMinSize(), BSize(kMapMinWidth, kMapMinHeight));
}


BSize PlaceMapView::MaxSize()
{
    return BLayoutUtils::ComposeSize(ExplicitMaxSize(), BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
}


/** the zoom for the size of the view, and the tiles that are not here yet are asked for */
void PlaceMapView::_Layout()
{
    if (!fHasPosition)
        return;
    BRect bounds = Bounds();
    double width = bounds.Width() + 1, height = bounds.Height() + 1;
    if (width < 16 || height < 16)
        return;

    fZoom = fHasBox ? ZoomFor(fSouth, fNorth, fWest, fEast, width, height) : kPointZoom;
    double centerLatitude = fHasBox ? (fSouth + fNorth) / 2 : fLatitude;
    double centerLongitude = fHasBox ? (fWest + fEast) / 2 : fLongitude;
    double left = WorldX(centerLongitude, fZoom) - width / 2, top = WorldY(centerLatitude, fZoom) - height / 2;

    // (beyond the zoom of the tiles the tiles of the last zoom are scaled up)
    const int tileZoom = std::min(fZoom, kMaxTileZoom);
    const double size = 256.0 * (double) (1 << (fZoom - tileZoom));
    int count = 1 << tileZoom;
    int firstX = (int) floor(left / size), lastX = (int) floor((left + width) / size);
    int firstY = std::max(0, (int) floor(top / size)), lastY = std::min(count - 1, (int) floor((top + height) / size));
    // (the tiles in the middle of the view first: they are what is looked at)
    struct Wanted { double distance; int x, y; };
    std::vector<Wanted> wanted;
    const double centerX = (left + width / 2) / size, centerY = (top + height / 2) / size;
    for (int y = firstY; y <= lastY; y++) {
        for (int x = firstX; x <= lastX; x++)
            wanted.push_back({(x + 0.5 - centerX) * (x + 0.5 - centerX) + (y + 0.5 - centerY) * (y + 0.5 - centerY), x, y});
    }
    std::sort(wanted.begin(), wanted.end(), [](const Wanted& a, const Wanted& b) { return a.distance < b.distance; });
    for (const Wanted& tile : wanted)
        _TileBitmap(tileZoom, ((tile.x % count) + count) % count, tile.y, NULL);
}


/**
 * The tile from the memory, else from the cache of the disk (of the style that was chosen, else of the standard map; a tile that is old or that is only of the standard map is asked for
 * again, and shown until the new one is here), else it is asked for. NULL if there is none yet. @p primary tells whether the tile is one of the style that was chosen.
 */
BBitmap* PlaceMapView::_TileBitmap(int zoom, int x, int y, bool* primary)
{
    unsigned long long key = TileKey(zoom, x, y);
    auto found = fTiles.find(key);
    if (found != fTiles.end()) {
        if (primary != NULL)
            *primary = fPrimaryTiles.count(key) > 0;
        return found->second;
    }

    bool stale = true;
    bool isPrimary = false;
    BBitmap* bitmap = NULL;
    const Style* chain[2] = { fStyle, FallbackStyle() };
    for (int i = 0; i < 2 && bitmap == NULL; i++) {
        if (i == 1 && chain[1] == chain[0])
            break;
        BPath path;
        if (!TilePath(*chain[i], zoom, x, y, &path))
            continue;
        BEntry entry(path.Path());
        time_t modified;
        if (entry.InitCheck() != B_OK || !entry.Exists())
            continue;
        bitmap = BTranslationUtils::GetBitmap(path.Path());
        if (bitmap != NULL) {
            isPrimary = i == 0;
            stale = !isPrimary || entry.GetModificationTime(&modified) != B_OK || time(NULL) - modified >= kTileLifetime;
        }
    }
    if (bitmap != NULL) {
        fTiles[key] = bitmap;
        if (isPrimary)
            fPrimaryTiles.insert(key);
    }
    if (primary != NULL)
        *primary = isPrimary;
    if (stale && fFetcher->view.IsValid()) {
        BAutolock locker(fFetcher->lock);
        if (fFetcher->asked.insert(key).second) {
            fFetcher->queue.push_back({fStyle, zoom, x, y});
            if (fFetcher->workers < kFetchThreads && (int) fFetcher->queue.size() > fFetcher->workers) {
                fFetcher->workers++;
                thread_id thread = spawn_thread(FetchThread, "map tiles", B_LOW_PRIORITY, new std::shared_ptr<Fetcher>(fFetcher));
                if (thread < 0 || resume_thread(thread) != B_OK)
                    fFetcher->workers--;
            }
        }
    }
    return bitmap;
}


void PlaceMapView::MessageReceived(BMessage* message)
{
    if (message->what == kTileArrived) {
        // a new file for a tile: it replaces the one in the memory (only that one: the others are not read and decoded again)
        int32 zoom, x, y;
        if (message->FindInt32("zoom", &zoom) == B_OK && message->FindInt32("x", &x) == B_OK && message->FindInt32("y", &y) == B_OK) {
            const unsigned long long key = TileKey(zoom, x, y);
            auto found = fTiles.find(key);
            if (found != fTiles.end()) {
                delete found->second;
                fTiles.erase(found);
            }
            fPrimaryTiles.erase(key);
        } else {
            for (auto& tile : fTiles)
                delete tile.second;
            fTiles.clear();
            fPrimaryTiles.clear();
        }
        Invalidate();
        return;
    }
    if (message->what == kSpin) {
        // (only while tiles are on their way: the spinner turns, nothing else is drawn again)
        if (!fLoading) {
            _StopAnimation();
            return;
        }
        fSpin = (fSpin + 1) % kSpokes;
        Invalidate();
        return;
    }
    if (message->what == kStyleChosen) {
        SetStyle(message->GetString("style", ""));
        return;
    }
    BView::MessageReceived(message);
}


void PlaceMapView::_StopAnimation()
{
    delete fAnimation;
    fAnimation = NULL;
}


/** a card in the middle of the map: a spinner and "Loading map..." (or, when no server answers and nothing is on its way, "Map not available") */
void PlaceMapView::_DrawLoading(BRect bounds, bool active)
{
    const char* text = active ? "Loading map..." : "Map not available";
    BFont font;
    GetFont(&font);
    font_height height;
    font.GetHeight(&height);
    const float textHeight = height.ascent + height.descent;
    const float spinner = active ? ceilf(textHeight * 1.3f) : 0;
    const float gap = active ? 8 : 0;
    const float width = spinner + gap + font.StringWidth(text) + 24, cardHeight = std::max(textHeight, spinner) + 16;
    BRect card(0, 0, width, cardHeight);
    // (in the upper part: the pin is in the middle)
    card.OffsetTo(floorf((bounds.Width() - width) / 2), floorf(bounds.Height() * 0.25f - cardHeight / 2));

    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
    SetHighColor(255, 255, 255, 215);
    FillRoundRect(card, 6, 6);
    SetHighColor(0, 0, 0, 60);
    StrokeRoundRect(card, 6, 6);

    if (active) {
        // (twelve spokes; the one that is "first" moves, and the ones behind it fade)
        const BPoint center(card.left + 12 + spinner / 2, (card.top + card.bottom) / 2);
        const float inner = spinner * 0.22f, outer = spinner * 0.5f;
        SetPenSize(2);
        SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
        for (int spoke = 0; spoke < kSpokes; spoke++) {
            const float angle = (float) spoke * 2 * (float) M_PI / kSpokes;
            const int age = ((fSpin - spoke) % kSpokes + kSpokes) % kSpokes;
            SetHighColor(40, 40, 40, (uint8) (230 - age * 17));
            StrokeLine(BPoint(center.x + sinf(angle) * inner, center.y - cosf(angle) * inner), BPoint(center.x + sinf(angle) * outer, center.y - cosf(angle) * outer));
        }
        SetPenSize(1);
    }
    SetDrawingMode(B_OP_COPY);
    SetHighColor(40, 40, 40);
    DrawString(text, BPoint(card.left + 12 + spinner + gap, card.top + 8 + height.ascent + (cardHeight - 16 - textHeight) / 2));
}


void PlaceMapView::_DrawPin(BPoint tip)
{
    SetDrawingMode(B_OP_COPY);
    SetHighColor(0, 0, 0, 90);
    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
    FillEllipse(BPoint(tip.x, tip.y), 5, 2);       // its shadow
    SetDrawingMode(B_OP_COPY);

    const float radius = 8;
    BPoint head(tip.x, tip.y - 18);
    SetHighColor(200, 30, 30);
    BPoint points[3] = { BPoint(head.x - 6.5f, head.y + 5), BPoint(head.x + 6.5f, head.y + 5), tip };
    FillPolygon(points, 3);
    FillEllipse(head, radius, radius);
    SetHighColor(120, 10, 10);
    StrokeEllipse(head, radius, radius);
    SetHighColor(255, 255, 255);
    FillEllipse(head, 3, 3);
}


void PlaceMapView::Draw(BRect updateRect)
{
    BRect bounds = Bounds();
    SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    FillRect(bounds & updateRect);

    if (!fHasPosition) {
        SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
        FillRect(bounds);
        SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
        const char* text = "No position yet";
        font_height height;
        GetFontHeight(&height);
        DrawString(text, BPoint((bounds.Width() - StringWidth(text)) / 2, bounds.Height() / 2 + height.ascent / 2));
        return;
    }

    double width = bounds.Width() + 1, height = bounds.Height() + 1;
    double centerLatitude = fHasBox ? (fSouth + fNorth) / 2 : fLatitude;
    double centerLongitude = fHasBox ? (fWest + fEast) / 2 : fLongitude;
    double left = WorldX(centerLongitude, fZoom) - width / 2, top = WorldY(centerLatitude, fZoom) - height / 2;

    const int tileZoom = std::min(fZoom, kMaxTileZoom);
    const double size = 256.0 * (double) (1 << (fZoom - tileZoom));
    int count = 1 << tileZoom;
    int firstX = (int) floor(left / size), lastX = (int) floor((left + width) / size);
    int firstY = std::max(0, (int) floor(top / size)), lastY = std::min(count - 1, (int) floor((top + height) / size));
    SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
    FillRect(bounds & updateRect);
    bool anyPrimary = false;
    int missing = 0, drawn = 0;
    for (int y = firstY; y <= lastY; y++) {
        for (int x = firstX; x <= lastX; x++) {
            bool primary = false;
            BBitmap* tile = _TileBitmap(tileZoom, ((x % count) + count) % count, y, &primary);
            if (tile == NULL) {
                missing++;
                continue;
            }
            anyPrimary = anyPrimary || primary;
            drawn++;
            float x0 = (float) (x * size - left), y0 = (float) (y * size - top);
            if (size == 256.0)
                DrawBitmap(tile, BPoint(x0, y0));
            else
                DrawBitmap(tile, tile->Bounds(), BRect(x0, y0, x0 + (float) size - 1, y0 + (float) size - 1), B_FILTER_BITMAP_BILINEAR);
        }
    }
    // tiles that are not here yet: the map says that it is working on them (and, if the server does not answer at all, that it does not have them)
    bool active = false;
    if (missing > 0) {
        BAutolock locker(fFetcher->lock);
        active = fFetcher->workers > 0 || !fFetcher->queue.empty();
    }
    fLoading = missing > 0 && active;
    // (the card whenever tiles are on their way, also when some are shown already; when nothing is on its way and the map is empty, it says so)
    if (missing > 0 && (active || drawn == 0))
        _DrawLoading(bounds, active);
    if (fLoading && fAnimation == NULL && Window() != NULL) {
        fAnimation = new BMessageRunner(BMessenger(this), new BMessage(kSpin), kSpinInterval);
        if (fAnimation->InitCheck() != B_OK)
            _StopAnimation();
    }

    // (what the licences ask the map to say, in lines that fit the view)
    const Style& shown = anyPrimary ? *fStyle : *FallbackStyle();
    const char* const* attribution = shown.attribution;
    const int lines = attribution[1] != NULL ? 2 : 1;

    // the pin is where the place is (not where the middle of its box is)
    BPoint tip((float) (WorldX(fLongitude, fZoom) - left), (float) (WorldY(fLatitude, fZoom) - top));
    if (bounds.Contains(tip))
        _DrawPin(tip);

    // the licence of the data asks for it
    BFont small(be_plain_font);
    small.SetSize(be_plain_font->Size() * 0.8f);
    SetFont(&small);
    font_height fontHeight;
    GetFontHeight(&fontHeight);
    const float lineHeight = fontHeight.ascent + fontHeight.descent + 1;
    float textWidth = 0;
    for (int line = 0; line < lines; line++)
        textWidth = std::max(textWidth, StringWidth(attribution[line]));
    // (a line that is wider than the view starts at its left edge: the end of it is cut, not the beginning)
    textWidth = std::min(textWidth, (float) bounds.Width() - 8);
    BRect label(bounds.right - textWidth - 8, bounds.bottom - lineHeight * lines - 3, bounds.right, bounds.bottom);
    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
    SetHighColor(255, 255, 255, 190);
    FillRect(label);
    SetDrawingMode(B_OP_COPY);
    SetHighColor(40, 40, 40);
    for (int line = 0; line < lines; line++) {
        float width = StringWidth(attribution[line]);
        DrawString(attribution[line], BPoint(std::max(bounds.left + 4, bounds.right - width - 4), label.top + 1 + lineHeight * (line + 1) - fontHeight.descent - 1));
    }
    SetFont(be_plain_font);

    // a thin border
    SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT));
    StrokeRect(bounds);
}
