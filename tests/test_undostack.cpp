#include "undostack.hpp"

#include "printers.hpp"

#include <gtest/gtest.h>

TEST(UndoStack, UndoesAndRedoesWholeStates) {
    UndoStack stack;
    Entry e;
    stack.record(e);
    e.insert("1");
    stack.record(e);
    e.insert("2");
    EXPECT_TRUE(stack.undo(e));
    EXPECT_EQ(e.text(), "1");
    EXPECT_TRUE(stack.undo(e));
    EXPECT_EQ(e.text(), "");
    EXPECT_FALSE(stack.undo(e));
    EXPECT_TRUE(stack.redo(e));
    EXPECT_EQ(e.text(), "1");
    stack.record(e);  // a new edit drops what could be redone
    e.insert("5");
    EXPECT_FALSE(stack.redo(e));
    EXPECT_TRUE(stack.canUndo());
}

TEST(UndoStack, KeepsTheLastHundredStates) {
    UndoStack stack;
    Entry e;
    for (int i = 0; i < 150; ++i) {
        stack.record(e);
        e.insert("1");
    }
    int undone = 0;
    while (stack.undo(e)) ++undone;
    EXPECT_EQ(undone, 100);
    EXPECT_EQ(e.text().size(), 50);
}
