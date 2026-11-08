/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Message.h>
#include <SupportDefs.h>

#include <string>

#include "ShojiAssociations.h"

namespace shoji {

/**
 * The settings of Shoji, a message in `~/config/settings/shoji/settings`: which file types Shoji is the default application of (an AssociationMode), what the
 * applications of the types were before, to put them back, and the style of the map of a place.
 */
class Settings {
public:
    Settings();

    status_t            Load();
    status_t            Save() const;

    AssociationMode     Mode() const { return fMode; }
    void                SetMode(AssociationMode mode) { fMode = mode; }
    BMessage&           Previous() { return fPrevious; }
    const BMessage&     Previous() const { return fPrevious; }
    /**
     * The style of the map of a place that was chosen in the view of the map (an id of sen::mapstyles, SenMapStyles.h); empty if none was: the map then has the default style, which is
     * set in the settings of the spatial navigator (see EffectiveMapStyle()).
     */
    const std::string&  MapStyle() const { return fMapStyle; }
    /** an id; empty (or an id that is not known) takes the choice back, the default of the plugin settings is used again */
    void                SetMapStyle(const std::string& id);
    /** how many file types are Shoji's by the settings */
    int32               CountAssociated() const;

    /** the mode that nothing is changed for */
    static AssociationMode  Default() { return kAssociateNothing; }

    /**
     * The default style of the map: the setting `map_style` of the spatial navigator (`~/config/settings/sen/spatial-navigator.settings`, set in the settings window of SEN); the built-in
     * default if there is none or the style is not known.
     */
    static std::string  PluginMapStyle();
    /** the style of the map that is used: the choice that was made in the view of the map, else the default of the plugin settings */
    std::string         EffectiveMapStyle() const;

private:
    AssociationMode     fMode;
    BMessage            fPrevious;
    std::string         fMapStyle;
};

}   // namespace shoji
