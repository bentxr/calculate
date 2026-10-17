#include "ploteditor.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

// The plot page's editor (D9.5, D9.6): Data and Style tabs; Style's sections in order of use, each with its common
// properties and "More" for every keyword the engine takes; a search over all of them. Every change is an edit of
// the figure file made with the library's editing calls, turned down when the engine refuses it.
namespace {

const std::string kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
                            R"( {"on": "ax", "call": "plot", "args": [[0, 1, 2], [0, 1, 4]], "kwargs": {"label": "squares"}}]})";

QByteArray bytes(const std::string& s) { return QByteArray::fromStdString(s); }

struct Editor {
    PlotEditor editor;
    QByteArray edited;
    explicit Editor(const std::string& figure = kFigure) {
        editor.resize(360, 700);
        editor.setFigure(bytes(figure));
        QObject::connect(&editor, &PlotEditor::figureEdited, &editor, [this](const QByteArray& f) { edited = f; });
        editor.show();
        EXPECT_TRUE(QTest::qWaitForWindowExposed(&editor));
    }
    template <class W>
    W* child(const QString& name) {
        W* w = editor.findChild<W*>(name);
        EXPECT_NE(w, nullptr) << name.toStdString();
        return w;
    }
    void open(const char* section) {
        auto* header = child<QToolButton>(QStringLiteral("section:") + section);
        if (!header->isChecked()) header->click();
    }
    void more(const char* section) {
        open(section);
        auto* b = child<QToolButton>(QStringLiteral("more:") + section);
        if (!b->isChecked()) b->click();
    }
    void type(QLineEdit* line, const QString& text) {
        line->setFocus();
        line->selectAll();
        QTest::keyClick(line, Qt::Key_Delete);
        QTest::keyClicks(line, text);
        QTest::keyClick(line, Qt::Key_Return);
    }
};

std::string edit(const std::string& figure, const std::function<std::string(const std::string&, std::string*)>& f) {
    std::string error;
    const std::string out = f(figure, &error);
    EXPECT_EQ(error, "");
    return out;
}

}  // namespace

TEST(PlotEditor, HasDataAndStyleTabsAndTheSectionsInOrder) {
    Editor e;
    auto* tabs = e.child<QTabWidget>("plotTabs");
    ASSERT_EQ(tabs->count(), 2);
    EXPECT_EQ(tabs->tabText(0), "Data");
    EXPECT_EQ(tabs->tabText(1), "Style");
    EXPECT_EQ(tabs->currentIndex(), 1);
    int last = -1;
    for (const char* id : {"titles", "lines", "axes", "legend", "grid", "figure"}) {
        auto* header = e.child<QToolButton>(QStringLiteral("section:") + id);
        ASSERT_NE(header, nullptr);
        EXPECT_FALSE(header->text().isEmpty()) << id;
        const int y = header->mapTo(&e.editor, QPoint()).y();
        EXPECT_GT(y, last) << id;  // in order of use, top down
        last = y;
    }
    EXPECT_TRUE(e.child<QToolButton>("section:titles")->isChecked());  // the first two open, the rest closed
    EXPECT_TRUE(e.child<QToolButton>("section:lines")->isChecked());
    EXPECT_FALSE(e.child<QToolButton>("section:grid")->isChecked());
}

// A title typed is a set_title call; typed again it replaces that call's text; emptied the call goes.
TEST(PlotEditor, TheTitleIsASetTitleCall) {
    Editor e;
    e.type(e.child<QLineEdit>("edit:title"), "Squares");
    const std::string titled = edit(kFigure, [](const std::string& f, std::string* err) {
        return calculate_plot::appendCall(f, R"({"on": "ax", "call": "set_title", "args": ["Squares"]})", err);
    });
    EXPECT_EQ(e.edited, bytes(titled));
    e.type(e.child<QLineEdit>("edit:title"), "Cubes");
    EXPECT_EQ(e.edited, bytes(edit(titled, [](const std::string& f, std::string* err) { return calculate_plot::setArgument(f, 2, 0, R"("Cubes")", err); })));
    e.type(e.child<QLineEdit>("edit:title"), "");
    EXPECT_EQ(e.edited, bytes(edit(titled, [](const std::string& f, std::string* err) { return calculate_plot::removeCall(f, 2, err); })));
    e.type(e.child<QLineEdit>("edit:xlabel"), "x");
    EXPECT_NE(e.edited.indexOf(R"({"on":"ax","call":"set_xlabel","args":["x"]})"), -1) << e.edited.toStdString();
}

// The series' colour is the call's color keyword, in the place of its alias c.
TEST(PlotEditor, ASeriesColourIsItsColorKeyword) {
    const std::string withC = edit(kFigure, [](const std::string& f, std::string* err) { return calculate_plot::setKeyword(f, 1, "c", R"("C3")", {}, err); });
    Editor e(withC);
    auto* series = e.child<QComboBox>("edit:series");
    ASSERT_EQ(series->count(), 1);
    EXPECT_EQ(series->itemText(0), "squares");  // its label
    auto* colour = e.child<QLineEdit>("edit:color");
    EXPECT_EQ(colour->text(), "C3");  // what the file says, through the alias
    e.type(colour, "#ff8800");
    EXPECT_EQ(e.edited, bytes(edit(withC, [](const std::string& f, std::string* err) {
                  return calculate_plot::setKeyword(f, 1, "color", R"("#ff8800")", {"c"}, err);
              })));
    auto* width = e.child<QLineEdit>("edit:linewidth");
    e.type(width, "2.50");
    EXPECT_NE(e.edited.indexOf(R"("linewidth":2.50)"), -1) << e.edited.toStdString();  // as typed: a float stays a float
}

// What the engine refuses is turned down: the figure stays, the message says why.
TEST(PlotEditor, AnEditTheEngineRefusesIsTurnedDown) {
    Editor e;
    e.more("lines");
    auto* marker = e.child<QLineEdit>("prop:plot:marker");
    QSignalSpy spy(&e.editor, &PlotEditor::figureEdited);
    e.type(marker, R"("not a marker")");
    EXPECT_EQ(spy.count(), 0);
    EXPECT_FALSE(e.child<QLabel>("plotEditMessage")->text().isEmpty());
    EXPECT_TRUE(e.child<QLabel>("plotEditMessage")->isVisible());
    EXPECT_EQ(marker->text(), "");  // back to what the file says
    e.type(marker, R"("o")");
    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(e.child<QLabel>("plotEditMessage")->isVisible());
}

// Legend and grid are switches: a call added, or removed.
TEST(PlotEditor, LegendAndGridAreCallsSwitchedOnAndOff) {
    Editor e;
    e.open("legend");
    e.child<QCheckBox>("edit:legend")->click();
    const std::string withLegend =
        edit(kFigure, [](const std::string& f, std::string* err) { return calculate_plot::appendCall(f, R"({"on": "ax", "call": "legend"})", err); });
    EXPECT_EQ(e.edited, bytes(withLegend));
    e.child<QCheckBox>("edit:legend")->click();
    EXPECT_EQ(e.edited, bytes(edit(withLegend, [](const std::string& f, std::string* err) { return calculate_plot::removeCall(f, 2, err); })));
    e.open("grid");
    e.child<QCheckBox>("edit:grid")->click();
    EXPECT_NE(e.edited.indexOf(R"({"on":"ax","call":"grid","args":[true]})"), -1) << e.edited.toStdString();
}

// With several Axes a chooser picks the one edited.
TEST(PlotEditor, TheAxesChooserPicksTheTarget) {
    Editor e(R"({"version": 1, "calls": [{"call": "subplots", "args": [1, 2], "as": ["fig", "axs"]}]})");
    auto* axes = e.child<QComboBox>("plotAxes");
    ASSERT_EQ(axes->count(), 2);
    EXPECT_TRUE(axes->isVisible());
    axes->setCurrentIndex(1);
    e.type(e.child<QLineEdit>("edit:title"), "right");
    EXPECT_NE(e.edited.indexOf(R"({"on":"axs[1]","call":"set_title","args":["right"]})"), -1) << e.edited.toStdString();
    Editor one;
    EXPECT_FALSE(one.child<QComboBox>("plotAxes")->isVisible());  // one Axes: nothing to choose
}

// The search finds a property by name in every section, opening what holds it; empty shows the sections again.
TEST(PlotEditor, TheSearchFindsAnyProperty) {
    Editor e;
    auto* search = e.child<QLineEdit>("plotSearch");
    QTest::keyClicks(search, "markersize");
    QWidget* found = e.child<QWidget>("prop:plot:markersize");
    EXPECT_TRUE(found->isVisible());
    EXPECT_FALSE(e.child<QLineEdit>("edit:title")->isVisible());
    search->clear();
    QApplication::processEvents();
    EXPECT_TRUE(e.child<QLineEdit>("edit:title")->isVisible());
}

// Every keyword and property the engine takes for the calls the editor offers has a control (D9.6).
TEST(PlotEditor, EveryPlotPropertyHasAControl) {
    const std::string figure =
        R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
        R"( {"on": "ax", "call": "plot", "args": [[0, 1], [0, 1]]}, {"on": "ax", "call": "scatter", "args": [[0, 1], [1, 0]]},)"
        R"( {"on": "ax", "call": "bar", "args": [[0, 1], [1, 2]]}, {"on": "ax", "call": "hist", "args": [[1, 2, 2, 3]]},)"
        R"( {"on": "ax", "call": "fill_between", "args": [[0, 1], [0, 1]]}, {"on": "ax", "call": "errorbar", "args": [[0, 1], [0, 1]]}]})";
    Editor e(figure);
    std::string error;
    const QJsonObject schema = QJsonDocument::fromJson(QByteArray::fromStdString(calculate_plot::schema(std::string(CALCULATE_PLOT_FONTS)))).object();
    QMap<QString, QJsonObject> calls;
    for (const QJsonValue& c : schema.value("calls").toArray()) calls.insert(c.toObject().value("call").toString(), c.toObject());
    int checked = 0;
    const auto expectControls = [&](const QString& call) {
        const QJsonObject c = calls.value(call);
        ASSERT_FALSE(c.isEmpty()) << call.toStdString();
        for (const char* group : {"parameters", "properties"})
            for (const QJsonValue& v : c.value(group).toArray()) {
                const QJsonObject p = v.toObject();
                if (p.value("supported") == QJsonValue(false) || p.value("positional").toBool()) continue;
                const QString name = QStringLiteral("prop:%1:%2").arg(call, p.value("name").toString());
                EXPECT_NE(e.editor.findChild<QWidget*>(name), nullptr) << name.toStdString();
                ++checked;
            }
    };
    for (const char* section : {"titles", "lines", "axes", "legend", "grid", "figure"}) e.more(section);
    auto* series = e.child<QComboBox>("edit:series");
    ASSERT_EQ(series->count(), 6);
    for (int i = 0; i < series->count(); ++i) {
        series->setCurrentIndex(i);
        QApplication::processEvents();
        expectControls(QString::fromStdString(calculate_plot::figureCalls(figure, &error)[std::size_t(i) + 1].method));
    }
    for (const char* call : {"set_title", "tick_params", "legend", "grid", "suptitle", "subplots"}) expectControls(QString::fromLatin1(call));
    EXPECT_GT(checked, 150);
}
