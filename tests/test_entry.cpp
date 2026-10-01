#include "entry.hpp"

#include "printers.hpp"

#include <gtest/gtest.h>

TEST(Entry, KeysInsertPiecesThatDeleteWhole) {
    Entry e;
    for (const char* piece : {"sin(", "3", "0", ")"}) e.insert(piece);
    EXPECT_EQ(e.text(), "sin(30)");
    e.backspace();
    EXPECT_EQ(e.text(), "sin(30");
    e.left();
    e.left();
    e.backspace();  // removes "sin(" in one go, as on the calculator
    EXPECT_EQ(e.text(), "30");
    EXPECT_EQ(e.cursor(), 0);
    e.backspace();  // nothing before the cursor
    EXPECT_EQ(e.text(), "30");
}

TEST(Entry, InsertsAtTheCursor) {
    Entry e;
    e.insert("1");
    e.insert("2");
    e.left();
    e.insert("+");
    EXPECT_EQ(e.text(), "1+2");
    e.right();
    e.right();  // already at the end
    EXPECT_EQ(e.cursor(), 3);
    e.left();
    e.left();
    e.left();
    e.left();  // already at the start
    EXPECT_EQ(e.cursor(), 0);
}

TEST(Entry, SetTextSplitsIntoPieces) {
    Entry e;
    e.setText("sin(1e10)+Ans×nCr(5, 2)");
    EXPECT_EQ(e.pieces(), QStringList({"sin(", "1", "e", "1", "0", ")", "+", "Ans", "×", "nCr(", "5", ", ", "2", ")"}));
    EXPECT_EQ(e.text(), "sin(1e10)+Ans×nCr(5, 2)");
    EXPECT_EQ(e.cursor(), 14);
    e.clear();
    EXPECT_TRUE(e.isEmpty());
    EXPECT_EQ(e.cursor(), 0);
}
