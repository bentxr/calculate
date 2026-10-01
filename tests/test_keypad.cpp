#include "keypad.hpp"

#include "printers.hpp"

#include <gtest/gtest.h>

#include <QSet>

TEST(Keypad, RowsAreFullAndLabelsUnique) {
    int columns = 0;
    QSet<QString> labels;
    for (const Key& key : keypad()) {
        columns += key.span;
        EXPECT_FALSE(labels.contains(key.label)) << key.label.toStdString();
        labels.insert(key.label);
    }
    EXPECT_EQ(columns % keypadColumns, 0);
    EXPECT_EQ(columns / keypadColumns, 8);
}

TEST(Keypad, ExactAvailabilityFollowsTheEngine) {
    auto find = [](const QString& label) {
        for (const Key& key : keypad()) if (key.label == label) return key;
        return Key{};
    };
    EXPECT_FALSE(availableInExact(find("sin")));
    EXPECT_FALSE(availableInExact(find("π")));
    EXPECT_FALSE(availableInExact(find("eˣ")));
    EXPECT_TRUE(availableInExact(find("√")));
    EXPECT_TRUE(availableInExact(find("7")));
    EXPECT_TRUE(availableInExact(find("n!")));
    EXPECT_EQ(find("sin").insert, "sin(");
    EXPECT_EQ(find("=").action, KeyAction::Evaluate);
}
