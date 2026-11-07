/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <GroupView.h>

/**
 * The template for places (entity/x-vnd.sen-labs.entity.place) and locations (entity/x-vnd.sen-labs.entity.location): the map with a pin where the place is
 * (PlaceMapView) next to the form with the name and the type of the place, where it is and what else is known of it: two panes, the form on the left, the map on the right as high as the form.
 * A location is a position without the kind of a place, so it has no type. The map is a view of its own beside the form, not in its grid: a view that spans both columns of a grid cannot be told from a field
 * when the lists are put into the form.
 */
class PlaceView : public BGroupView {
    public:
                        PlaceView(bool withType);
        virtual        ~PlaceView();
};
