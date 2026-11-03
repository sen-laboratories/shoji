/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */

// The associations of Shoji: which types are chosen (no file types of the system are involved), and that a type of its own, made for the test, gets Shoji as its
// application and its old application back. Build and run on Haiku:  g++ -std=c++20 -Isrc/app tests/AssociationsTest.cpp src/app/ShojiAssociations.cpp -o /tmp/assoc-test -lbe && /tmp/assoc-test

#include "ShojiAssociations.h"

#include <Application.h>
#include <MimeType.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>

static int sFailures = 0;
static int sChecks = 0;

#define CHECK(condition) \
    do { \
        sChecks++; \
        if (!(condition)) { \
            sFailures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while (0)

static bool Has(const std::vector<std::string>& list, const char* type)
{
    for (const std::string& each : list) {
        if (each == type)
            return true;
    }
    return false;
}

static void TestChoice()
{
    const std::string shoji = "application/x-vnd.sen-labs.shoji";
    std::vector<shoji::TypeState> types = {
        {"text/plain", "application/x-vnd.Haiku-StyledEdit", false},
        {"image/jpeg", "application/x-vnd.Haiku-ShowImage", false},    // (through the supertype, the system's answer is what is in the list)
        {"entity/x-vnd.sen-labs.entity.book", "", true},
        {"entity/x-vnd.sen-labs.entity.movie", "", false},
        {"application/x-person", "application/x-vnd.Be-PEPL", true},
        {"application/x-vnd.Be-PEPL", "", false},               // (a program: not a file type)
        {"image/png", shoji, false},                             // (Shoji's already)
        {"video/x-unknown", "", false},
        {"document/scientific-paper", "", true},
        {"audio", "", false},                                    // (a supertype)
    };

    CHECK(shoji::ChooseTypes(shoji::kAssociateNothing, types, shoji).empty());

    std::vector<std::string> none = shoji::ChooseTypes(shoji::kAssociateUnassociated, types, shoji);
    CHECK(Has(none, "entity/x-vnd.sen-labs.entity.book"));
    CHECK(Has(none, "entity/x-vnd.sen-labs.entity.movie"));
    CHECK(Has(none, "video/x-unknown"));
    CHECK(Has(none, "document/scientific-paper"));
    CHECK(!Has(none, "text/plain"));
    CHECK(!Has(none, "image/jpeg"));
    CHECK(!Has(none, "application/x-person"));              // (it has People)
    CHECK(!Has(none, "application/x-vnd.Be-PEPL"));
    CHECK(!Has(none, "image/png"));
    CHECK(!Has(none, "audio"));

    std::vector<std::string> templated = shoji::ChooseTypes(shoji::kAssociateTemplated, types, shoji);
    CHECK(Has(templated, "application/x-person"));          // (the template of the contacts replaces People)
    CHECK(Has(templated, "video/x-unknown"));
    CHECK(!Has(templated, "text/plain"));                   // (it has an application and no template)
    CHECK(!Has(templated, "image/png"));
    CHECK(templated.size() == none.size() + 1);
}

static std::string AppOf(const char* type)
{
    char app[B_MIME_TYPE_LENGTH];
    app[0] = '\0';
    BMimeType(type).GetPreferredApp(app);
    return app;
}

// A type that has no application of its own but whose supertype has one is not "without an application" (the files of image/jpeg open in ShowImage through `image`):
// what the system opens a file with is what counts. A type that nothing opens is, and so is one that only Shoji could open.
static std::string ApplicationFor(const std::vector<shoji::TypeState>& types, const char* type)
{
    for (const shoji::TypeState& state : types) {
        if (state.type == type)
            return state.application;
    }
    return "<not listed>";
}

static void TestInherited()
{
    BMimeType supertype("shojitest2");
    const bool madeSupertype = !supertype.IsInstalled() && supertype.Install() == B_OK;
    BMimeType inheriting("shojitest2/x-inherits"), alone("shojitest2/x-alone");
    CHECK(inheriting.Install() == B_OK);
    CHECK(alone.Install() == B_OK);

    // nothing opens the types yet
    std::vector<shoji::TypeState> types = shoji::InstalledTypes({});
    CHECK(ApplicationFor(types, "shojitest2/x-inherits").empty());
    CHECK(ApplicationFor(types, "shojitest2/x-alone").empty());

    // the supertype has an application: both types are opened by it
    CHECK(supertype.SetPreferredApp("application/x-vnd.Haiku-StyledEdit") == B_OK);
    types = shoji::InstalledTypes({});
    CHECK(ApplicationFor(types, "shojitest2/x-inherits") == "application/x-vnd.Haiku-StyledEdit");
    const std::vector<std::string> chosen = shoji::ChooseTypes(shoji::kAssociateUnassociated, types, "application/x-vnd.sen-labs.shoji");
    CHECK(!Has(chosen, "shojitest2/x-inherits") && !Has(chosen, "shojitest2/x-alone"));

    // an application of the type itself wins over the supertype's
    CHECK(alone.SetPreferredApp("application/x-vnd.Haiku-ShowImage") == B_OK);
    types = shoji::InstalledTypes({});
    CHECK(ApplicationFor(types, "shojitest2/x-alone") == "application/x-vnd.Haiku-ShowImage");

    inheriting.Delete();
    alone.Delete();
    if (madeSupertype)
        supertype.Delete();
}

static void TestApply()
{
    const std::string shoji = "application/x-vnd.sen-labs.shoji";
    BMimeType supertype("shojitest");
    const bool madeSupertype = !supertype.IsInstalled() && supertype.Install() == B_OK;
    BMimeType plain("shojitest/x-plain"), owned("shojitest/x-owned");
    CHECK(plain.Install() == B_OK);
    CHECK(owned.Install() == B_OK);
    CHECK(owned.SetPreferredApp("application/x-vnd.Haiku-StyledEdit") == B_OK);

    BMessage previous;
    // (only the types of the test: the types of the system are not touched by a test)
    CHECK(shoji::Associate({"shojitest/x-plain", "shojitest/x-owned"}, shoji, &previous) == 2);
    CHECK(AppOf("shojitest/x-plain") == shoji);
    CHECK(AppOf("shojitest/x-owned") == shoji);
    CHECK(shoji::Associate({"shojitest/x-plain"}, shoji, &previous) == 0);      // (already: nothing to remember)

    // the user chose another application for one of them since: that one stays
    CHECK(plain.SetPreferredApp("application/x-vnd.Haiku-StyledEdit") == B_OK);
    CHECK(shoji::Restore(previous, shoji) == 1);
    CHECK(AppOf("shojitest/x-plain") == "application/x-vnd.Haiku-StyledEdit");
    CHECK(AppOf("shojitest/x-owned") == "application/x-vnd.Haiku-StyledEdit");

    plain.Delete();
    owned.Delete();
    if (madeSupertype)
        supertype.Delete();
}

// With SHOJI_TEST_LIVE=1: the whole way with the file types of this system: apply each mode, look at what changed, and go back to nothing: every type
// must have the application that it had before. (It changes the file type database of the user for a moment, and puts it back.)
#include <map>
static std::map<std::string, std::string> Snapshot()
{
    std::map<std::string, std::string> apps;
    for (const shoji::TypeState& state : shoji::InstalledTypes(shoji::TemplateTypes()))
        apps[state.type] = state.application;
    return apps;
}

static void TestLive()
{
    const std::string signature = "application/x-vnd.sen-labs.shoji";
    const std::map<std::string, std::string> before = Snapshot();
    const std::vector<std::string> templates = shoji::TemplateTypes();
    printf("     %zu types, %zu templates\n", before.size(), templates.size());

    BMessage previous;
    // (types of the ontologies have Shoji as their application already: they are not "taken" and not changed)
    int32 shojiBefore = 0;
    for (const auto& entry : before)
        shojiBefore += entry.second == signature ? 1 : 0;
    int32 unassociated = shoji::Apply(shoji::kAssociateUnassociated, signature, &previous);
    std::map<std::string, std::string> afterOne = Snapshot();
    int32 shojiOne = 0;
    for (const auto& entry : afterOne)
        shojiOne += entry.second == signature ? 1 : 0;
    printf("     unassociated: %d types taken, %d have Shoji\n", (int) unassociated, (int) shojiOne);
    CHECK(unassociated > 0 && shojiOne == unassociated + shojiBefore);
    for (const auto& entry : before)
        CHECK(entry.second.empty() || afterOne[entry.first] == entry.second || entry.second == signature);   // (nothing that had an application lost it)

    int32 templated = shoji::Apply(shoji::kAssociateTemplated, signature, &previous);
    std::map<std::string, std::string> afterTwo = Snapshot();
    int32 shojiTwo = 0;
    for (const auto& entry : afterTwo)
        shojiTwo += entry.second == signature ? 1 : 0;
    printf("     with templates: %d types taken, %d have Shoji\n", (int) templated, (int) shojiTwo);
    CHECK(shojiTwo == templated + shojiBefore && templated >= unassociated);
    CHECK(afterTwo["application/x-person"] == signature);

    shoji::Apply(shoji::kAssociateNothing, signature, &previous);
    const std::map<std::string, std::string> after = Snapshot();
    CHECK(after == before);
    CHECK(previous.IsEmpty());
}

int main()
{
    BApplication app("application/x-vnd.sen-labs.shoji-associations-test");    // (the roster tells which application opens a type)
    TestChoice();
    TestApply();
    TestInherited();
    if (getenv("SHOJI_TEST_LIVE") != NULL)
        TestLive();
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
