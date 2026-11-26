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
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PlaceView"


PlaceView::PlaceView(bool withType) : BGroupView(withType ? "Place View" : "Location View", B_HORIZONTAL, be_control_look->DefaultItemSpacing() * 2) {
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    GroupLayout()->SetInsets(0, 0, 0, 0);
    // two panes: the form, and next to it the map, which is as high as the form
    FormView* form = new FormView("Place Fields");
    // (wide enough for the fields to be read: the window grows to hold both panes)
    form->SetExplicitMinSize(BSize(be_plain_font->StringWidth("Description") + 330, B_SIZE_UNSET));
    AddChild(form);
    form->AddField("dc:title", B_TRANSLATE("Name"));
    if (withType)
        form->AddField("SEN:placeType", B_TRANSLATE("Type"));
    form->AddField("dc:description", B_TRANSLATE("Description"));
    form->AddField("schema:address", B_TRANSLATE("Address"));
    form->AddField("schema:addressLocality", B_TRANSLATE("City"));
    form->AddField("schema:addressRegion", B_TRANSLATE("Region"));
    form->AddField("schema:addressCountry", B_TRANSLATE("Country"));
    form->AddField("schema:latitude", B_TRANSLATE("Latitude"));
    form->AddField("schema:longitude", B_TRANSLATE("Longitude"));
    form->AddField("schema:elevation", B_TRANSLATE("Elevation"));
    form->AddField("schema:url", B_TRANSLATE("Web site"));
    // the names in other languages, and where else the place is known (OpenStreetMap, Wikidata, Wikipedia): lists, shown as such when the file is
    form->AddField("schema:alternateName", B_TRANSLATE("Other names"));
    form->AddField("schema:sameAs", B_TRANSLATE("Same as"));
    // the picture is credited as its author asks
    form->AddField("schema:creditText", B_TRANSLATE("Picture credit"));
    form->AddField("schema:license", B_TRANSLATE("Picture license"));
    form->AddField("META:TYPE", B_TRANSLATE("Semantic Type"));
    form->Finish();
    AddChild(new PlaceMapView());
}

PlaceView::~PlaceView()
{
}
