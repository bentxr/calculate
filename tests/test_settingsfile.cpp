#include "settingsfile.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <gtest/gtest.h>

TEST(SettingsFile, OnlyWhatDiffersIsWritten) {
    const QMap<QString, QString> defaults{{"language", "system"}, {"theme", "system"}, {"live", "true"}};
    QMap<QString, QString> now = defaults;
    QJsonObject o = QJsonDocument::fromJson(settingsfile::write(now, defaults)).object();
    EXPECT_EQ(o.value("version").toInt(), 1);
    EXPECT_TRUE(o.value("settings").toObject().isEmpty());
    now["theme"] = "dark";
    now["live"] = "false";
    o = QJsonDocument::fromJson(settingsfile::write(now, defaults)).object();
    const QJsonObject written = o.value("settings").toObject();
    EXPECT_EQ(written.size(), 2);
    EXPECT_EQ(written.value("theme").toString(), "dark");
    EXPECT_EQ(written.value("live").toString(), "false");
}

TEST(SettingsFile, ReadingChecksEveryKeyAndValue) {
    const QMap<QString, QStringList> allowed{{"theme", {"system", "light", "dark"}}, {"live", {"true", "false"}}};
    const settingsfile::Read r =
        settingsfile::read(R"({"version":1,"settings":{"theme":"dark","colour":"red","live":"maybe"}})", allowed);
    EXPECT_EQ(r.values.value("theme"), "dark");
    EXPECT_FALSE(r.values.contains("colour"));
    EXPECT_FALSE(r.values.contains("live"));
    EXPECT_EQ(r.problems.size(), 2);  // one line per key it could not use
    EXPECT_FALSE(settingsfile::read("not json", allowed).problems.isEmpty());
    EXPECT_FALSE(settingsfile::read(R"({"settings":{}})", allowed).problems.isEmpty());              // no version
    EXPECT_FALSE(settingsfile::read(R"({"version":2,"settings":{}})", allowed).problems.isEmpty());  // a newer file
    EXPECT_TRUE(settingsfile::read(R"({"version":1,"settings":{}})", allowed).problems.isEmpty());
}

TEST(SettingsFile, AListSettingHoldsAllowedWordsOnceEach) {
    const QMap<QString, QStringList> allowed{{"common", {"sinh", "gcd", "pi"}}, {"theme", {"light", "dark"}}};
    const QSet<QString> lists{"common"};
    settingsfile::Read r = settingsfile::read(R"({"version":1,"settings":{"common":"gcd sinh"}})", allowed, lists);
    EXPECT_EQ(r.values.value("common"), "gcd sinh");  // in the file's order
    EXPECT_TRUE(r.problems.isEmpty());
    r = settingsfile::read(R"({"version":1,"settings":{"common":""}})", allowed, lists);
    EXPECT_TRUE(r.values.contains("common"));  // an empty list
    EXPECT_TRUE(r.problems.isEmpty());
    r = settingsfile::read(R"({"version":1,"settings":{"common":"gcd gcd"}})", allowed, lists);
    EXPECT_FALSE(r.values.contains("common"));
    EXPECT_EQ(r.problems.size(), 1);
    r = settingsfile::read(R"({"version":1,"settings":{"common":"gcd tan"}})", allowed, lists);
    EXPECT_FALSE(r.values.contains("common"));
    EXPECT_EQ(r.problems.size(), 1);
    r = settingsfile::read(R"({"version":1,"settings":{"theme":"light dark"}})", allowed, lists);
    EXPECT_FALSE(r.values.contains("theme"));  // a plain setting still takes exactly one of its values
    r = settingsfile::read(R"({"version":1,"settings":{"common":"gcd sinh"}})", allowed);
    EXPECT_FALSE(r.values.contains("common"));  // not declared a list: one value only
}
