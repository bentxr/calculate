#include "licensing/compliance.hpp"
#include "licensing/json.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <random>
#include <set>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using calculate::licensing::Problems;
using calculate::licensing::Tree;
namespace lic = calculate::licensing;

namespace {

std::string joined(const Problems& problems) {
    std::string all;
    for (const auto& p : problems) all += "\n  " + p;
    return all;
}

bool mentions(const Problems& problems, const std::string& what) {
    for (const auto& p : problems)
        if (p.find(what) != std::string::npos) return true;
    return false;
}

// A small, compliant tree: our own code, one borrowed runtime component with a derived file and a
// vendored file, one test-only download, a NOTICE and the manifest that describes them all.
class FakeTree {
public:
    FakeTree() {
        // Unique even across parallel test processes under node, where every process has the same pid.
        static std::atomic<int> counter{0};
        static const std::string run = std::to_string(std::random_device{}()) + "-" +
                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        root_ = fs::temp_directory_path() / ("calculate-licensing-" + run + "-" + std::to_string(counter++));
        fs::remove_all(root_);
        write(".gitignore", "/build/\n__pycache__/\n");
        write("CMakeLists.txt",
              "FetchContent_Declare(gt\n    URL      https://example.org/gt-1.0.tar.gz\n"
              "    URL_HASH SHA256=" + std::string(64, 'a') + ")\n");
        write("src/own.cpp", "int own() { return 1; }\n");
        write("src/ported.cpp",
              "// Derived from Example 2.0: lib/example.py.\n"
              "// Copyright (c) 2001 Example Team\n"
              "// Licence: LICENSES/Example.txt\n"
              "int ported() { return 2; }\n");
        write("extern/example/ex.h", "/* Example 2.0, Copyright (c) 2001 Example Team */\n");
        write("LICENSES/Example.txt", exampleLicence);
        write("LICENSES/Gt.txt", gtLicence);
        write("NOTICE", "This product includes Example 2.0.\nCopyright (c) 2001 Example Team\n");
        write("licensing/changes/example.md", "Ported lib/example.py to C++.\n");
        write("licensing/vendored/example.json",
              "{\"extern/example/ex.h\": \"" + sha("/* Example 2.0, Copyright (c) 2001 Example Team */\n") +
                  "\"}");
        manifest_ = R"({
  "version": 1,
  "ignore": ["build/**", ".git/**", "**/__pycache__/**"],
  "own": [".gitignore", "CMakeLists.txt", "src/**", "NOTICE", "licensing/**"],
  "forbidden": ["**/agg_conv_gpc.h"],
  "mayMentionCopyright": ["NOTICE"],
  "notice": "NOTICE",
  "components": [
    {"id": "example", "name": "Example", "version": "2.0", "homepage": "https://example.org",
     "use": "runtime", "licence": "Example-License",
     "licenceFiles": [{"path": "LICENSES/Example.txt", "upstream": "https://example.org/LICENSE",
                       "sha256": ")" + sha(exampleLicence) + R"("}],
     "fetch": [], "vendored": ["extern/example/**"], "pristine": "licensing/vendored/example.json",
     "derived": ["src/ported.cpp"],
     "header": ["Copyright (c) 2001 Example Team", "LICENSES/Example.txt"],
     "notices": ["Copyright (c) 2001 Example Team"],
     "changes": "licensing/changes/example.md"},
    {"id": "gt", "name": "Gt", "version": "1.0", "homepage": "https://example.org/gt",
     "use": "test", "licence": "BSD-3-Clause",
     "licenceFiles": [{"path": "LICENSES/Gt.txt", "upstream": "https://example.org/gt/LICENSE",
                       "sha256": ")" + sha(gtLicence) + R"("}],
     "fetch": [{"url": "https://example.org/gt-1.0.tar.gz", "sha256": ")" + std::string(64, 'a') + R"("}],
     "vendored": [], "derived": [], "header": [], "notices": [], "changes": null}
  ]
})";
        write("licensing/manifest.json", manifest_);
    }
    ~FakeTree() { fs::remove_all(root_); }

    Tree tree() const { return Tree{root_}; }
    void write(const std::string& path, const std::string& text) const {
        fs::create_directories((root_ / path).parent_path());
        std::ofstream(root_ / path, std::ios::binary) << text;
    }
    void remove(const std::string& path) const { fs::remove(root_ / path); }
    // Edits the manifest text: replaces the first occurrence of `from` with `to`.
    void editManifest(const std::string& from, const std::string& to) {
        const auto at = manifest_.find(from);
        ASSERT_NE(at, std::string::npos) << from;
        manifest_.replace(at, from.size(), to);
        write("licensing/manifest.json", manifest_);
    }

    static std::string sha(const std::string& text) { return calculate::support::sha256Hex(text); }

private:
    static inline const std::string exampleLicence = "Example licence: keep this notice.\n";
    static inline const std::string gtLicence = "Gt licence text.\n";
    fs::path root_;
    std::string manifest_;
};

}  // namespace

TEST(LicensingChecks, AFabricatedCompliantTreePasses) {
    FakeTree t;
    const Problems problems = lic::checkAll(t.tree());
    EXPECT_TRUE(problems.empty()) << joined(problems);
}

TEST(LicensingChecks, AMissingManifestIsAProblem) {
    FakeTree t;
    t.remove("licensing/manifest.json");
    EXPECT_TRUE(mentions(lic::checkManifest(t.tree()), "manifest"));
}

TEST(LicensingChecks, DuplicateComponentIdsAreAProblem) {
    FakeTree t;
    t.editManifest(R"("id": "gt")", R"("id": "example")");
    EXPECT_TRUE(mentions(lic::checkManifest(t.tree()), "example"));
}

TEST(LicensingChecks, AnUnknownUseIsAProblem) {
    FakeTree t;
    t.editManifest(R"("use": "test")", R"("use": "sometimes")");
    EXPECT_TRUE(mentions(lic::checkManifest(t.tree()), "sometimes"));
}

TEST(LicensingChecks, AGlobThatMatchesNothingIsAProblem) {
    FakeTree t;
    t.editManifest(R"("derived": ["src/ported.cpp"])", R"("derived": ["src/ported.cpp", "src/gone.cpp"])");
    EXPECT_TRUE(mentions(lic::checkManifest(t.tree()), "src/gone.cpp"));
}

TEST(LicensingChecks, AMissingLicenceFileIsAProblem) {
    FakeTree t;
    t.remove("LICENSES/Gt.txt");
    EXPECT_TRUE(mentions(lic::checkLicenceTexts(t.tree()), "LICENSES/Gt.txt"));
}

TEST(LicensingChecks, AnEditedLicenceTextIsAProblem) {
    FakeTree t;
    t.write("LICENSES/Example.txt", "Example licence: keep this notice.\n\n");  // one extra newline
    EXPECT_TRUE(mentions(lic::checkLicenceTexts(t.tree()), "LICENSES/Example.txt"));
}

TEST(LicensingChecks, AnUnaccountedFileIsAProblem) {
    FakeTree t;
    t.write("data/mystery.ttf", "font bytes");
    EXPECT_TRUE(mentions(lic::checkEveryFileAccounted(t.tree()), "data/mystery.ttf"));
}

TEST(LicensingChecks, IgnoredFilesNeedNoAccount) {
    FakeTree t;
    t.write("build/x/y.o", "object");
    t.write("tools/__pycache__/z.pyc", "cache");
    EXPECT_TRUE(lic::checkEveryFileAccounted(t.tree()).empty());
}

TEST(LicensingChecks, AGitFileIsSkippedLikeTheGitFolder) {
    FakeTree t;  // in a git worktree or submodule, .git is a file that points to the repository
    t.write(".git", "gitdir: /somewhere/else\n");
    EXPECT_TRUE(lic::checkEveryFileAccounted(t.tree()).empty()) << joined(lic::checkEveryFileAccounted(t.tree()));
}

TEST(LicensingChecks, AFileClaimedByTwoComponentsIsAProblem) {
    FakeTree t;
    t.editManifest(R"("derived": [], "header": [])", R"("derived": ["src/ported.cpp"], "header": [])");
    EXPECT_TRUE(mentions(lic::checkEveryFileAccounted(t.tree()), "src/ported.cpp"));
}

TEST(LicensingChecks, ADerivedFileWithoutItsOriginIsAProblem) {
    FakeTree t;
    t.write("src/ported.cpp", "// Copyright (c) 2001 Example Team\n// Licence: LICENSES/Example.txt\nint p();\n");
    EXPECT_TRUE(mentions(lic::checkProvenanceHeaders(t.tree()), "src/ported.cpp"));
}

TEST(LicensingChecks, ADerivedFileWithoutTheCopyrightIsAProblem) {
    FakeTree t;
    t.write("src/ported.cpp", "// Derived from Example 2.0: lib/example.py.\n// Licence: LICENSES/Example.txt\n");
    EXPECT_TRUE(mentions(lic::checkProvenanceHeaders(t.tree()), "Copyright (c) 2001 Example Team"));
}

TEST(LicensingChecks, AHeaderFarDownTheFileDoesNotCount) {
    FakeTree t;
    std::string text(60, '\n');
    text += "// Derived from Example 2.0: x.\n// Copyright (c) 2001 Example Team\n// LICENSES/Example.txt\n";
    t.write("src/ported.cpp", text);
    EXPECT_FALSE(lic::checkProvenanceHeaders(t.tree()).empty());
}

TEST(LicensingChecks, AMissingNoticeIsAProblem) {
    FakeTree t;
    t.write("NOTICE", "This product includes Example 2.0.\n");
    EXPECT_TRUE(mentions(lic::checkNotice(t.tree()), "Copyright (c) 2001 Example Team"));
}

TEST(LicensingChecks, ANoticeMustNameTheComponentAndVersion) {
    FakeTree t;
    t.write("NOTICE", "Copyright (c) 2001 Example Team\n");
    EXPECT_TRUE(mentions(lic::checkNotice(t.tree()), "Example 2.0"));
}

TEST(LicensingChecks, TestOnlyComponentsNeedNoNotice) {
    FakeTree t;  // "Gt" is not in NOTICE, and that is fine: it never ships
    EXPECT_TRUE(lic::checkNotice(t.tree()).empty());
}

TEST(LicensingChecks, DerivedCodeWithoutASummaryOfChangesIsAProblem) {
    FakeTree t;
    t.write("licensing/changes/example.md", "  \n");
    EXPECT_TRUE(mentions(lic::checkChangeSummaries(t.tree()), "example"));
    t.editManifest(R"("changes": "licensing/changes/example.md")", R"("changes": null)");
    EXPECT_TRUE(mentions(lic::checkChangeSummaries(t.tree()), "example"));
}

TEST(LicensingChecks, ARequiredChangeFieldMustBePresent) {
    FakeTree t;
    t.editManifest(R"("changes": "licensing/changes/example.md")",
                   R"("changes": "licensing/changes/example.md", "changesMustSay": ["Modified by:", "Date:"])");
    t.write("licensing/changes/example.md", "Modified by: someone\nPorted it.\n");
    EXPECT_TRUE(mentions(lic::checkChangeSummaries(t.tree()), "Date:"));
}

TEST(LicensingChecks, AForbiddenFileAnywhereIsAProblem) {
    FakeTree t;
    t.write("extern/example/agg_conv_gpc.h", "gpc");
    EXPECT_TRUE(mentions(lic::checkForbiddenFiles(t.tree()), "agg_conv_gpc.h"));
}

TEST(LicensingChecks, AForbiddenFileInAFetchedSourceIsAProblem) {
    FakeTree t;
    t.write("build/_deps/agg-src/include/agg_conv_gpc.h", "gpc");
    Tree tree = t.tree();
    EXPECT_TRUE(lic::checkForbiddenFiles(tree).empty());  // build/ is ignored, and not fetched sources yet
    tree.fetchedSources.push_back(tree.root / "build/_deps/agg-src");
    EXPECT_TRUE(mentions(lic::checkForbiddenFiles(tree), "agg-src/include/agg_conv_gpc.h"));
}

TEST(LicensingChecks, AnUndeclaredDownloadIsAProblem) {
    FakeTree t;
    t.write("cmake/extra.cmake", "FetchContent_Declare(z\n  URL https://example.org/z.tgz\n  URL_HASH SHA256=" +
                                     std::string(64, 'b') + ")\n");
    t.editManifest(R"("own": [".gitignore",)", R"("own": ["cmake/**", ".gitignore",)");
    EXPECT_TRUE(mentions(lic::checkFetchedDependencies(t.tree()), "https://example.org/z.tgz"));
}

TEST(LicensingChecks, ADownloadWithoutAHashIsAProblem) {
    FakeTree t;
    t.write("CMakeLists.txt", "FetchContent_Declare(gt\n    URL https://example.org/gt-1.0.tar.gz)\n");
    EXPECT_TRUE(mentions(lic::checkFetchedDependencies(t.tree()), "hash"));
}

TEST(LicensingChecks, AGitRepositoryDownloadIsAProblem) {
    FakeTree t;  // a tag can move; only hashed archives pin the exact code whose licence we checked
    t.write("CMakeLists.txt", "FetchContent_Declare(gt\n    GIT_REPOSITORY https://example.org/gt.git\n"
                              "    GIT_TAG v1)\nFetchContent_Declare(gt2\n    URL      https://example.org/gt-1.0.tar.gz\n"
                              "    URL_HASH SHA256=" + std::string(64, 'a') + ")\n");
    EXPECT_TRUE(mentions(lic::checkFetchedDependencies(t.tree()), "GIT_REPOSITORY"));
}

// The project's own library is followed on its branch while both are developed together: no third-party
// licence to pin. Only the repositories the manifest names as the project's own.
TEST(LicensingChecks, TheProjectsOwnRepositoryMayBeFetchedByBranch) {
    FakeTree t;
    t.editManifest(R"("notice": "NOTICE",)", R"("notice": "NOTICE", "ownRepositories": ["https://example.org/own.git"],)");
    t.write("CMakeLists.txt", "FetchContent_Declare(own\n    GIT_REPOSITORY https://example.org/own.git\n"
                              "    GIT_TAG        main)\nFetchContent_Declare(gt\n    URL      https://example.org/gt-1.0.tar.gz\n"
                              "    URL_HASH SHA256=" + std::string(64, 'a') + ")\n");
    EXPECT_TRUE(lic::checkFetchedDependencies(t.tree()).empty()) << joined(lic::checkFetchedDependencies(t.tree()));
    t.write("CMakeLists.txt", "FetchContent_Declare(other\n    GIT_REPOSITORY https://example.org/other.git\n"
                              "    GIT_TAG        main)\nFetchContent_Declare(gt\n    URL      https://example.org/gt-1.0.tar.gz\n"
                              "    URL_HASH SHA256=" + std::string(64, 'a') + ")\n");
    EXPECT_TRUE(mentions(lic::checkFetchedDependencies(t.tree()), "GIT_REPOSITORY"));
}

TEST(LicensingChecks, ADeclaredDownloadTheBuildNoLongerUsesIsAProblem) {
    FakeTree t;
    t.write("CMakeLists.txt", "# nothing fetched\n");
    EXPECT_TRUE(mentions(lic::checkFetchedDependencies(t.tree()), "gt"));
}

TEST(LicensingChecks, ADownloadWithAnotherHashIsAProblem) {
    FakeTree t;
    t.write("CMakeLists.txt", "FetchContent_Declare(gt\n    URL      https://example.org/gt-1.0.tar.gz\n"
                              "    URL_HASH SHA256=" + std::string(64, 'c') + ")\n");
    EXPECT_FALSE(lic::checkFetchedDependencies(t.tree()).empty());
}

TEST(LicensingChecks, ASilentlyModifiedVendoredFileIsAProblem) {
    FakeTree t;
    t.write("extern/example/ex.h", "/* Example 2.0, Copyright (c) 2001 Example Team */\nint changed;\n");
    EXPECT_TRUE(mentions(lic::checkVendoredFiles(t.tree()), "extern/example/ex.h"));
}

TEST(LicensingChecks, AMarkedModificationOfAVendoredFileIsAccepted) {
    FakeTree t;
    t.write("extern/example/ex.h",
            "/* Example 2.0, Copyright (c) 2001 Example Team */\n"
            "/* Modified by calculate: see licensing/changes/example.md */\nint changed;\n");
    EXPECT_TRUE(lic::checkVendoredFiles(t.tree()).empty()) << joined(lic::checkVendoredFiles(t.tree()));
}

TEST(LicensingChecks, AVendoredFileMissingFromThePristineListIsAProblem) {
    FakeTree t;
    t.write("extern/example/new.h", "/* Example 2.0 */\n");
    EXPECT_TRUE(mentions(lic::checkVendoredFiles(t.tree()), "extern/example/new.h"));
}

TEST(LicensingChecks, AGitignoreEntryTheWalkDoesNotSkipIsAProblem) {
    FakeTree t;
    t.write(".gitignore", "/build/\n__pycache__/\n/secret/\n");
    EXPECT_TRUE(mentions(lic::checkIgnoredPaths(t.tree()), "secret"));
}

TEST(LicensingChecks, ACopyrightLineInOurOwnFileIsAProblem) {
    FakeTree t;  // a borrowed file dropped among our sources would otherwise pass as ours
    t.write("src/sneaky.cpp", "/* Copyright (C) 1999 Somebody Else */\nint s;\n");
    EXPECT_TRUE(mentions(lic::checkOwnFiles(t.tree()), "src/sneaky.cpp"));
    t.write("src/sneaky.cpp", "/* (c) 1999 Somebody Else, all rights reserved */\nint s;\n");
    EXPECT_TRUE(mentions(lic::checkOwnFiles(t.tree()), "src/sneaky.cpp"));
    t.write("src/sneaky.cpp", "// SPDX-License-Identifier: MIT\nint s;\n");
    EXPECT_TRUE(mentions(lic::checkOwnFiles(t.tree()), "src/sneaky.cpp"));
}

TEST(LicensingChecks, ListedFilesMayMentionCopyrights) {
    FakeTree t;  // NOTICE quotes other people's copyrights on purpose
    EXPECT_TRUE(lic::checkOwnFiles(t.tree()).empty()) << joined(lic::checkOwnFiles(t.tree()));
}

// The real thing: this repository honours every licence it declares.
TEST(Licensing, ThisRepositoryIsCompliant) {
    Tree tree{CALCULATE_SOURCE_DIR};
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fs::path(CALCULATE_BINARY_DIR) / "_deps", ec)) {
        const std::string name = entry.path().filename().string();
        if (name.size() > 4 && name.compare(name.size() - 4, 4, "-src") == 0) tree.fetchedSources.push_back(entry.path());
    }
    EXPECT_FALSE(tree.fetchedSources.empty());
    const Problems problems = lic::checkAll(tree);
    EXPECT_TRUE(problems.empty()) << joined(problems);
}

// The browser build links Qt statically, and with it the third-party code inside Qt: each piece that Qt
// 6.11.2's qt_attribution.json files name for Qt Core, GUI, Widgets, SVG and the WebAssembly platform (those of
// other platforms and of the build tools left out) is a runtime component, so that NOTICE carries its notices and
// the licences view its licence.
TEST(Licensing, QtsThirdPartyCodeIsDeclared) {
    const auto text = [] {
        std::ifstream in(fs::path(CALCULATE_SOURCE_DIR) / "licensing/manifest.json", std::ios::binary);
        std::ostringstream all;
        all << in.rdbuf();
        return all.str();
    }();
    const auto manifest = calculate::support::parseJson(text, nullptr);
    ASSERT_TRUE(manifest.has_value());
    std::set<std::string> runtime;
    for (const auto& component : manifest->find("components")->items())
        if (component.find("use")->asString() == "runtime") runtime.insert(component.find("id")->asString());
    for (const char* id :
         {"qt-aglfn", "qt-blake2", "qt-dejavu", "qt-doubleconversion", "qt-easing", "qt-emoji-segmenter", "qt-freetype",
          "qt-freetype-bdf", "qt-freetype-pcf", "qt-freetype-zlib", "qt-grayraster", "qt-harfbuzz-ng",
          "qt-icc-srgb-color-profile", "qt-libjpeg", "qt-libpng", "qt-md4", "qt-md4c", "qt-md5", "qt-pcre2",
          "qt-pcre2-sljit", "qt-rfc6234", "qt-sha1", "qt-sha3_endian", "qt-sha3_keccak", "qt-siphash",
          "qt-smooth-scaling-algorithm", "qt-tika-mimetypes", "qt-tinycbor", "qt-tlexpected",
          "qt-unicode-character-database", "qt-unicode-cldr", "qt-webgradients", "qt-xserverhelper", "qt-xsvg",
          "qt-zlib"})
        EXPECT_TRUE(runtime.count(id)) << id;
}
