#include "plotdata.hpp"
#include "ploteditor.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

// The editor's Data tab (D9.7): a table per series with the columns its call takes, numbers kept as typed, paste and
// CSV / TSV import, and new series.
namespace {

const std::string kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
                            R"( {"on": "ax", "call": "plot", "args": [[0, 1.0, 2], [0, 1, 4]], "kwargs": {"label": "squares"}}]})";

QByteArray bytes(const std::string& s) { return QByteArray::fromStdString(s); }

struct Data {
    PlotEditor editor;
    PlotData* data = nullptr;
    QTableWidget* table = nullptr;
    QByteArray edited;
    explicit Data(const std::string& figure = kFigure) {
        editor.resize(360, 700);
        editor.setFigure(bytes(figure));
        QObject::connect(&editor, &PlotEditor::figureEdited, &editor, [this](const QByteArray& f) { edited = f; });
        editor.findChild<QTabWidget*>("plotTabs")->setCurrentIndex(0);
        editor.show();
        EXPECT_TRUE(QTest::qWaitForWindowExposed(&editor));
        data = editor.findChild<PlotData*>("plotData");
        table = editor.findChild<QTableWidget*>("plotTable");
    }
    QString cell(int row, int column) const { return table->item(row, column) ? table->item(row, column)->text() : QString(); }
    void set(int row, int column, const QString& text) {
        auto* item = table->item(row, column);
        if (!item) {
            item = new QTableWidgetItem;
            table->setItem(row, column, item);
        }
        item->setText(text);
    }
};

std::string expect(const std::string& figure, std::size_t call, std::size_t arg, const std::string& value) {
    std::string error;
    const std::string out = calculate_plot::setArgument(figure, call, arg, value, &error);
    EXPECT_EQ(error, "");
    return out;
}

}  // namespace

TEST(PlotData, TheTableShowsTheSeriesColumns) {
    Data d;
    ASSERT_NE(d.data, nullptr);
    ASSERT_NE(d.table, nullptr);
    ASSERT_EQ(d.table->columnCount(), 2);
    EXPECT_EQ(d.table->horizontalHeaderItem(0)->text(), "x");
    EXPECT_EQ(d.table->horizontalHeaderItem(1)->text(), "y");
    ASSERT_EQ(d.table->rowCount(), 4);  // the three points and an empty row for a new one
    EXPECT_EQ(d.cell(1, 0), "1.0");     // as the file writes it
    EXPECT_EQ(d.cell(2, 1), "4");
    EXPECT_EQ(d.cell(3, 0), "");
}

// A cell typed changes that argument of the call; nan and inf are floats JSON cannot write.
TEST(PlotData, ACellIsAnEditOfTheCallsArgument) {
    Data d;
    d.set(2, 1, "9");
    EXPECT_EQ(d.edited, bytes(expect(kFigure, 1, 1, "[0,1,9]")));
    const std::string nine = d.edited.toStdString();
    d.set(0, 1, "nan");
    EXPECT_EQ(d.edited, bytes(expect(nine, 1, 1, R"([{"$float":"nan"},1,9])")));
    // the empty row at the bottom takes a new point
    d.set(3, 0, "3");
    d.set(3, 1, "-inf");
    EXPECT_NE(d.edited.indexOf(R"("args":[[0,1.0,2,3],[{"$float":"nan"},1,9,{"$float":"-inf"}]])"), -1) << d.edited.toStdString();
    EXPECT_EQ(d.table->rowCount(), 5);
}

TEST(PlotData, WhatIsNotANumberIsTurnedDown) {
    Data d;
    QSignalSpy spy(&d.editor, &PlotEditor::figureEdited);
    d.set(1, 1, "one");
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(d.cell(1, 1), "1");  // back to what the file says
    EXPECT_FALSE(d.editor.findChild<QLabel*>("plotDataMessage")->text().isEmpty());
}

// Pasted rows (tabs, commas or semicolons) fill the table from the current cell down.
TEST(PlotData, PastedRowsFillTheTable) {
    Data d;
    d.table->setCurrentCell(1, 0);
    QApplication::clipboard()->setText("10\t100\n20\t200\n30\t300\n");
    d.data->paste();
    EXPECT_NE(d.edited.indexOf(R"("args":[[0,10,20,30],[0,100,200,300]])"), -1) << d.edited.toStdString();
}

// A CSV / TSV file is a new line series of its first two columns (a header row is skipped).
TEST(PlotData, AnImportedFileIsANewSeries) {
    Data d;
    d.data->importText("time;speed\n1;10.5\n2;20\n3;31e1\n");
    EXPECT_NE(d.edited.indexOf(R"({"on":"ax","call":"plot","args":[[1,2,3],[10.5,20,31e1]]})"), -1) << d.edited.toStdString();
    EXPECT_EQ(d.editor.findChild<QComboBox*>("plotDataSeries")->currentIndex(), 1);  // the new series is shown
    d.data->importText("only words\n");
    EXPECT_FALSE(d.editor.findChild<QLabel*>("plotDataMessage")->text().isEmpty());
}

// Add series: a line, points or bars, empty, to fill in.
TEST(PlotData, ASeriesIsAdded) {
    Data d;
    auto* add = d.editor.findChild<QToolButton*>("plotAddSeries");
    ASSERT_NE(add, nullptr);
    QAction* bars = d.editor.findChild<QAction*>("add:bar");
    ASSERT_NE(bars, nullptr);
    bars->trigger();
    EXPECT_NE(d.edited.indexOf(R"({"on":"ax","call":"bar","args":[[],[]]})"), -1) << d.edited.toStdString();
    EXPECT_EQ(d.table->horizontalHeaderItem(1)->text(), "height");
    EXPECT_EQ(d.table->rowCount(), 1);
    QAction* remove = d.editor.findChild<QAction*>("plotRemoveSeries");
    ASSERT_NE(remove, nullptr);
    remove->trigger();
    std::string error;
    const std::string withBars = calculate_plot::appendCall(kFigure, R"({"on":"ax","call":"bar","args":[[],[]]})", &error);
    EXPECT_EQ(d.edited, bytes(calculate_plot::removeCall(withBars, 2, &error)));
}

// Error bars have their errors as columns (keywords); a series whose data the table cannot hold says so.
TEST(PlotData, ColumnsFollowTheCall) {
    Data errors(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
                R"( {"on": "ax", "call": "errorbar", "args": [[1, 2], [3, 4]], "kwargs": {"yerr": [0.5, 0.25]}}]})");
    ASSERT_EQ(errors.table->columnCount(), 4);
    EXPECT_EQ(errors.table->horizontalHeaderItem(2)->text(), "yerr");
    EXPECT_EQ(errors.cell(1, 2), "0.25");
    errors.set(0, 3, "0.1");  // xerr, not given yet: the keyword is made
    EXPECT_NE(errors.edited.indexOf(R"("xerr":[0.1,{"$float":"nan"}])"), -1) << errors.edited.toStdString();
    Data hist(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]}, {"on": "ax", "call": "hist", "args": [[1, 2, 2]]}]})");
    ASSERT_EQ(hist.table->columnCount(), 1);
    EXPECT_EQ(hist.table->horizontalHeaderItem(0)->text(), "x");
    // dates are not numbers the table holds
    Data other(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]}, {"on": "ax", "call": "plot", "args":)"
               R"( [[{"$datetime": "2021-05-03T00:00:00"}, {"$datetime": "2021-05-04T00:00:00"}], [1, 2]]}]})");
    EXPECT_EQ(other.editor.findChild<QComboBox*>("plotDataSeries")->count(), 1);
    EXPECT_FALSE(other.table->isEnabled());
    EXPECT_FALSE(other.editor.findChild<QLabel*>("plotDataMessage")->text().isEmpty());
}
