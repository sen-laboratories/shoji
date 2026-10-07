/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.
 */
/**
 * simple demo view for People files
 */
#include <ControlLook.h>
#include <GroupView.h>
#include <TextControl.h>

#include "ContactView.h"

ContactView::ContactView() : BGroupView(B_VERTICAL, 0) {
	SetFlags(Flags() | B_NAVIGABLE);
	SetName("Contact View");
    GroupLayout()->SetInsets(be_control_look->DefaultLabelSpacing());

	// Set view color to standard background grey
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFont(be_plain_font);

    // todo: add picture to the left

    // name | address
    BGroupView* containerNameAdress = new BGroupView(B_HORIZONTAL, 240.0);
	// Set view color to standard background grey
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    BTextControl* nameView = new BTextControl("META:name", "", "Name", NULL);
    nameView->SetEnabled(true);
    containerNameAdress->GroupLayout()->AddView(nameView);

    BTextControl* addressView = new BTextControl("META:address", "", "Address", NULL);
    addressView->SetEnabled(true);
    containerNameAdress->GroupLayout()->AddView(addressView);

    GroupLayout()->AddView(containerNameAdress);

    // Company | ZIP
    BGroupView* containerCompanyZip = new BGroupView(B_HORIZONTAL, 240.0);
	// Set view color to standard background grey
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    BTextControl* companyView = new BTextControl("META:company", "", "Company", NULL);
    companyView->SetEnabled(true);
    containerCompanyZip->GroupLayout()->AddView(companyView);

    BTextControl* zipView = new BTextControl("META:zip", "", "ZIP code", NULL);
    zipView->SetEnabled(true);
    containerCompanyZip->GroupLayout()->AddView(zipView);

    GroupLayout()->AddView(containerCompanyZip);

    // E-Mail | City
    BGroupView* containerEmailCity = new BGroupView(B_HORIZONTAL, 240.0);
	// Set view color to standard background grey
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    BTextControl* emailView = new BTextControl("META:email", "", "E-Mail", NULL);
    emailView->SetEnabled(true);
    containerEmailCity->GroupLayout()->AddView(emailView);

    BTextControl* cityView = new BTextControl("META:city", "", "City", NULL);
    cityView->SetEnabled(true);
    containerEmailCity->GroupLayout()->AddView(cityView);

    GroupLayout()->AddView(containerEmailCity);

    // Phone | Country
    BGroupView* containerPhoneCountry = new BGroupView(B_HORIZONTAL, 240.0);
	// Set view color to standard background grey
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    BTextControl* phoneView = new BTextControl("META:mphone", "","Mobile", NULL);
    phoneView->SetEnabled(true);
    containerPhoneCountry->GroupLayout()->AddView(phoneView);

    BTextControl* countryView = new BTextControl("META:country", "", "Country", NULL);
    countryView->SetEnabled(true);
    containerPhoneCountry->GroupLayout()->AddView(countryView);

    GroupLayout()->AddView(containerPhoneCountry);
}

ContactView::~ContactView()
{
}
