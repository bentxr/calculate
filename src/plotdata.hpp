#pragma once

#include <QByteArray>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QTableWidget;

// The editor's Data tab (D9.7): the chosen series' data as a table, a column for each array its call takes (x and
// y; x and height for bars; x, y, yerr and xerr for error bars...), an empty row at the bottom for a new point.
// Numbers are kept as typed (JSON numbers; nan and inf as {"$float": ...}); a cell left empty within the data is
// NaN, a row emptied goes. Pasted rows and imported CSV / TSV text (tabs, semicolons or commas) fill it; series are
// added (a line, points, bars) and removed. Each change is an edit of the figure file given to `apply`.
class PlotData : public QWidget {
    Q_OBJECT

public:
    explicit PlotData(QWidget* parent = nullptr);

    // Applies an edited figure (or reports why there is none): "" when it was taken, else why not.
    void setApply(std::function<QString(const std::string& edited, const std::string& error)> apply) { apply_ = std::move(apply); }
    // The document and the Axes the editor targets.
    void setDocument(const QByteArray& figure, const QString& axes);
    void paste();                          // the clipboard's rows, from the current cell down
    void importText(const QString& text);  // a new line series from the first two numeric columns

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;  // Ctrl+V on the table pastes
    void changeEvent(QEvent* event) override;

private:
    struct Column {
        QString title;
        int argument = -1;  // the call's positional argument, or
        QString keyword;    // the keyword holding the array
    };
    std::vector<int> seriesCalls() const;
    int seriesCall() const;
    std::vector<Column> columns(int call) const;
    void fill();                   // the table from the document
    void fillInPlace();
    void commit();                 // the table to the document
    void edit(const std::string& edited, const std::string& error);
    void retranslate();

    std::function<QString(const std::string&, const std::string&)> apply_;
    QByteArray figure_;
    QString axes_;
    struct Call {
        QString on, method;
        QStringList arguments;
        QList<QPair<QString, QString>> keywords;
    };
    QList<Call> calls_;
    bool filling_ = false;
    bool inItemChange_ = false;  // a cell's change is being handled
    bool showNewest_ = false;  // the next document's last series is shown
    QComboBox* series_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* message_ = nullptr;
};
