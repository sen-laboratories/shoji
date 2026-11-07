/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Archivable.h>
#include <Bitmap.h>
#include <Locker.h>
#include <Messenger.h>
#include <View.h>

#include <map>
#include <memory>
#include <set>

class BMessage;

/**
 * The map of a place: the tiles of OpenStreetMap around its position, with a pin where the place is. The zoom is the largest at which the bounding box of the place (if it has
 * one: a city, a country, a building) fits into the view, else a street level one.
 *
 * The tiles are asked for one after the other by a thread of their own, with a User-Agent that names the program, and kept in `~/config/cache/shoji/tiles/<topo|osm>/<zoom>/<x>/<y>.png` for
 * two weeks, as the tile usage policy of OpenStreetMap asks (https://operations.osmfoundation.org/policies/tiles/). Nothing is sent but the addresses of the tiles: the tiles that cover the
 * view, never more than a few. The map is © OpenStreetMap contributors (ODbL), which the view says on the map.
 *
 * The tiles are those of OpenTopoMap (https://opentopomap.org: OpenStreetMap data with hill shading, contour lines and shaded relief; © OpenStreetMap contributors, SRTM, © OpenTopoMap,
 * CC-BY-SA, said on the map); if its server does not answer, a tile of the standard map of OpenStreetMap is shown instead, which has no relief.
 *
 * The view is part of the archived template of a place, so it can be restored from the archive (Instantiate()); the place is set when the file is shown (SetPlace()).
 */
class PlaceMapView : public BView {
    public:
                            PlaceMapView();
                            PlaceMapView(BMessage* archive);
        virtual            ~PlaceMapView();

        static BArchivable* Instantiate(BMessage* archive);
        virtual status_t    Archive(BMessage* into, bool deep = true) const;

        /** The name that the template has for it (the place finds it by that). */
        static const char*  ViewName() { return "shoji:map"; }

        /** The position and the box of a place from the attribute values of its file (`schema:latitude`, `schema:longitude`, `SEN:boxSouth`...); shows no map if there is no position. */
        void                SetPlace(const BMessage* values);
        void                SetPosition(double latitude, double longitude, bool hasBox, double south, double north, double west, double east);

        virtual void        AttachedToWindow();
        virtual void        FrameResized(float width, float height);
        virtual void        MessageReceived(BMessage* message);
        virtual void        Draw(BRect updateRect);
        virtual void        GetPreferredSize(float* width, float* height);
        virtual BSize       MinSize();
        virtual BSize       MaxSize();

        // the arithmetic of the tiles (Web Mercator, tiles of 256 pixels), public for the tests
        static double       WorldX(double longitude, int zoom);
        static double       WorldY(double latitude, int zoom);
        static int          ZoomFor(double south, double north, double west, double east, double width, double height);

        /** what the thread that asks for the tiles shares with the view (PlaceMapView.cpp) */
        struct Fetcher;

    private:
        void                _Init();
        void                _Layout();
        BBitmap*            _TileBitmap(int zoom, int x, int y, bool* topo);
        void                _DrawPin(BPoint tip);

        bool                fHasPosition;
        double              fLatitude, fLongitude;
        bool                fHasBox;
        double              fSouth, fNorth, fWest, fEast;
        int                 fZoom;
        std::map<unsigned long long, BBitmap*>  fTiles;
        std::set<unsigned long long>            fTopoTiles;
        std::shared_ptr<Fetcher>                fFetcher;
};
