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
#include <Message.h>
#include <Path.h>
#include <Polygon.h>
#include <String.h>
#include <TranslationUtils.h>
#include <sen/SenSocketHttp.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <set>
#include <string>

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
const char* const kUserAgent = "SEN-Shoji/1.0 (Haiku; https://sen-labs.org)";
const char* const kAttributionOsm = "\xC2\xA9 OpenStreetMap contributors";
const char* const kAttributionTopo[] = { "\xC2\xA9 OpenStreetMap contributors, SRTM", "\xC2\xA9 OpenTopoMap (CC-BY-SA)" };

// where the tiles come from: the map with the relief (hill shading and contour lines of OpenTopoMap, free to use within its fair use limits), and if its server does not
// answer the standard map of OpenStreetMap
enum TileStyle { kTopo = 0, kOsm = 1 };
const char* const kStyleFolder[] = { "topo", "osm" };
const char* const kStyleServer[] = { "https://tile.opentopomap.org/", "https://tile.openstreetmap.org/" };

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

bool TilePath(int style, int zoom, int x, int y, BPath* path)
{
    if (find_directory(B_USER_CACHE_DIRECTORY, path) != B_OK || path->Append("shoji/tiles") != B_OK || path->Append(kStyleFolder[style]) != B_OK)
        return false;
    BString name;
    name << zoom << "/" << x << "/" << y << ".png";
    return path->Append(name.String()) == B_OK;
}

}   // namespace


/** the tiles that are asked for, the thread that asks, and where to tell the view; shared with the thread, so that the view can go while a request is under way */
struct PlaceMapView::Fetcher {
    struct Request { int zoom, x, y; };

    BLocker             lock;
    std::deque<Request> queue;
    std::set<unsigned long long> asked;
    BMessenger          view;
    bool                quit = false;
    bool                running = false;

    Fetcher() : lock("tile fetcher") {}
};


/** one tile from the server of the style into the file of the cache (the client over a socket of SEN, see SenSocketHttp.h: the same one that the plugins use) */
static status_t FetchTile(int style, int zoom, int x, int y, const BPath& path)
{
    BString address;
    address << kStyleServer[style] << zoom << "/" << x << "/" << y << ".png";
    std::string body;
    int code = 0;
    status_t status = sen::http::SocketGet(address.String(), kUserAgent, "image/png", &body, &code);
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
                fetcher->running = false;
                return 0;
            }
            next = fetcher->queue.front();
            fetcher->queue.pop_front();
        }
        // (the map with the relief; if its server does not answer, the standard map is asked for this tile)
        bool got = false;
        for (int style = kTopo; style <= kOsm && !got; style++) {
            BPath path;
            got = TilePath(style, next.zoom, next.x, next.y, &path) && FetchTile(style, next.zoom, next.x, next.y, path) == B_OK;
        }
        if (got) {
            BMessage arrived(kTileArrived);
            arrived.AddInt32("zoom", next.zoom);
            fetcher->view.SendMessage(&arrived);
        }
        snooze(150000);	// (a few tiles, and not in a burst)
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
    fFetcher = std::make_shared<Fetcher>();
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


void PlaceMapView::SetPlace(const BMessage* values)
{
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
    for (int y = firstY; y <= lastY; y++) {
        for (int x = firstX; x <= lastX; x++)
            _TileBitmap(tileZoom, ((x % count) + count) % count, y, NULL);
    }
}


/**
 * The tile from the memory, else from the cache of the disk (of the map with the relief, else of the standard map; a tile that is old or that is only of the standard map is asked for
 * again, and shown until the new one is here), else it is asked for. NULL if there is none yet. @p topo tells whether the tile is one with the relief.
 */
BBitmap* PlaceMapView::_TileBitmap(int zoom, int x, int y, bool* topo)
{
    unsigned long long key = TileKey(zoom, x, y);
    auto found = fTiles.find(key);
    if (found != fTiles.end()) {
        if (topo != NULL)
            *topo = fTopoTiles.count(key) > 0;
        return found->second;
    }

    bool stale = true;
    bool isTopo = false;
    BBitmap* bitmap = NULL;
    for (int style = kTopo; style <= kOsm && bitmap == NULL; style++) {
        BPath path;
        if (!TilePath(style, zoom, x, y, &path))
            continue;
        BEntry entry(path.Path());
        time_t modified;
        if (entry.InitCheck() != B_OK || !entry.Exists())
            continue;
        bitmap = BTranslationUtils::GetBitmap(path.Path());
        if (bitmap != NULL) {
            isTopo = style == kTopo;
            stale = !isTopo || entry.GetModificationTime(&modified) != B_OK || time(NULL) - modified >= kTileLifetime;
        }
    }
    if (bitmap != NULL) {
        fTiles[key] = bitmap;
        if (isTopo)
            fTopoTiles.insert(key);
    }
    if (topo != NULL)
        *topo = isTopo;
    if (stale && fFetcher->view.IsValid()) {
        BAutolock locker(fFetcher->lock);
        if (fFetcher->asked.insert(key).second) {
            fFetcher->queue.push_back({zoom, x, y});
            if (!fFetcher->running) {
                fFetcher->running = true;
                thread_id thread = spawn_thread(FetchThread, "map tiles", B_LOW_PRIORITY, new std::shared_ptr<Fetcher>(fFetcher));
                if (thread < 0 || resume_thread(thread) != B_OK)
                    fFetcher->running = false;
            }
        }
    }
    return bitmap;
}


void PlaceMapView::MessageReceived(BMessage* message)
{
    if (message->what == kTileArrived) {
        // a new file for a tile: it replaces the one in the memory
        for (auto& tile : fTiles)
            delete tile.second;
        fTiles.clear();
        fTopoTiles.clear();
        Invalidate();
        return;
    }
    BView::MessageReceived(message);
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
    bool anyTopo = false;
    for (int y = firstY; y <= lastY; y++) {
        for (int x = firstX; x <= lastX; x++) {
            bool topo = false;
            BBitmap* tile = _TileBitmap(tileZoom, ((x % count) + count) % count, y, &topo);
            if (tile == NULL)
                continue;
            anyTopo = anyTopo || topo;
            float x0 = (float) (x * size - left), y0 = (float) (y * size - top);
            if (size == 256.0)
                DrawBitmap(tile, BPoint(x0, y0));
            else
                DrawBitmap(tile, tile->Bounds(), BRect(x0, y0, x0 + (float) size - 1, y0 + (float) size - 1), B_FILTER_BITMAP_BILINEAR);
        }
    }
    // (what the licences ask the map to say, in lines that fit the view)
    const char* attribution[2] = { anyTopo ? kAttributionTopo[0] : kAttributionOsm, anyTopo ? kAttributionTopo[1] : NULL };
    const int lines = anyTopo ? 2 : 1;

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
    BRect label(bounds.right - textWidth - 8, bounds.bottom - lineHeight * lines - 3, bounds.right, bounds.bottom);
    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
    SetHighColor(255, 255, 255, 190);
    FillRect(label);
    SetDrawingMode(B_OP_COPY);
    SetHighColor(40, 40, 40);
    for (int line = 0; line < lines; line++) {
        float width = StringWidth(attribution[line]);
        DrawString(attribution[line], BPoint(bounds.right - width - 4, label.top + 1 + lineHeight * (line + 1) - fontHeight.descent - 1));
    }
    SetFont(be_plain_font);

    // a thin border
    SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT));
    StrokeRect(bounds);
}
