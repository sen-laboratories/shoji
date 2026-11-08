/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#include "PlaceView.h"

#include <ControlLook.h>
#include <Font.h>
#include <GroupLayout.h>

#include "../../FormView.h"
#include "PlaceMapView.h"

PlaceView::PlaceView(bool withType) : BGroupView(withType ? "Place View" : "Location View", B_HORIZONTAL, be_control_look->DefaultItemSpacing() * 2) {
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    GroupLayout()->SetInsets(0, 0, 0, 0);
    // two panes: the form, and next to it the map, which is as high as the form
    FormView* form = new FormView("Place Fields");
    // (wide enough for the fields to be read: the window grows to hold both panes)
    form->SetExplicitMinSize(BSize(be_plain_font->StringWidth("Description") + 330, B_SIZE_UNSET));
    AddChild(form);
    form->AddField("dc:title", "Name");
    if (withType)
        form->AddField("SEN:placeType", "Type");
    form->AddField("dc:description", "Description");
    form->AddField("schema:address", "Address");
    form->AddField("schema:addressLocality", "City");
    form->AddField("schema:addressRegion", "Region");
    form->AddField("schema:addressCountry", "Country");
    form->AddField("schema:latitude", "Latitude");
    form->AddField("schema:longitude", "Longitude");
    form->AddField("schema:elevation", "Elevation");
    form->AddField("schema:url", "Web site");
    // the names in other languages, and where else the place is known (OpenStreetMap, Wikidata, Wikipedia): lists, shown as such when the file is
    form->AddField("schema:alternateName", "Other names");
    form->AddField("schema:sameAs", "Same as");
    // the picture is credited as its author asks
    form->AddField("schema:creditText", "Picture credit");
    form->AddField("schema:license", "Picture license");
    form->Finish();
    AddChild(new PlaceMapView());
}

PlaceView::~PlaceView()
{
}
