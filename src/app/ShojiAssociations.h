/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <Message.h>
#include <SupportDefs.h>

#include <string>
#include <vector>

/**
 * Which file types open in Shoji by default: the preferred application of a file type is that of the file type database of the user (the system's stays as it
 * is). Nothing is associated unless the user chooses it in the settings of Shoji:
 *
 *   - nothing (the default): Shoji opens what it is asked to open (the SEN menus and "Open with" do that)
 *   - all file types that have no application yet (a file type with no template gets the form that Shoji makes from the attributes of the type)
 *   - the same, and the file types that Shoji has a template for, whatever application they have (People of Haiku is replaced for its files)
 *
 * What was the application of a file type before is remembered, so that going back to "nothing" (or choosing another way) puts it back.
 */
namespace shoji {

enum AssociationMode {
    kAssociateNothing       = 0,
    kAssociateUnassociated  = 1,
    kAssociateTemplated     = 2
};

/** What is known about an installed file type when it is decided whether Shoji takes it. */
struct TypeState {
    std::string type;               // "document/scientific-paper"
    std::string preferredApp;       // the signature, empty: none
    bool        hasTemplate;
};

/**
 * Which of the types Shoji takes. Only file types are taken (a type has a supertype, and the types of applications, application/x-vnd.*, are no file types), and not those that
 * are Shoji's already. The types are those as they are when nothing of Shoji's is associated (see Restore()).
 */
std::vector<std::string> ChooseTypes(AssociationMode mode, const std::vector<TypeState>& types, const std::string& signature);

/** The installed types with their application and whether Shoji has a template for them (see TemplateTypes()). */
std::vector<TypeState> InstalledTypes(const std::vector<std::string>& templateTypes);

/** The types that Shoji has a template for: the folders `shoji/templates/<supertype>/<subtype>/template.view` of the settings and the data folders. */
std::vector<std::string> TemplateTypes();

/**
 * Makes these types open in Shoji.
 * @param previous gets, for each type that was changed, the type and its application before ("type" and "app" strings in the same order; an empty app: it had none)
 * @return the number of types that were changed
 */
int32 Associate(const std::vector<std::string>& types, const std::string& signature, BMessage* previous);

/** Puts the applications back that Associate() changed. A type that Shoji has not as its application any more (the user chose another one) is left alone. */
int32 Restore(const BMessage& previous, const std::string& signature);

/** Restores what was associated before, then associates for the mode (what the settings window does). @return the types that Shoji has now */
int32 Apply(AssociationMode mode, const std::string& signature, BMessage* previous);

}   // namespace shoji
