#include "plotdata.hpp"

#include "filedialogs.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <optional>

namespace {

std::string text(const QByteArray& b) { return b.toStdString(); }
std::string text(const QString& s) { return s.toStdString(); }

QString jsonString(const QString& s) {
    const QByteArray a = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(a.mid(1, a.size() - 2));
}

const QRegularExpression& jsonNumber() {
    static const QRegularExpression re(QStringLiteral(R"(^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$)"));
    return re;
}

// The elements of a JSON array's text, each as its own text; nothing when it is not an array
std::optional<QStringList> elements(const QString& json) {
    const QString t = json.trimmed();
    if (!t.startsWith('[') || !t.endsWith(']')) return std::nullopt;
    QStringList out;
    int depth = 0, start = 1;
    bool inString = false;
    for (int i = 1; i < t.size() - 1; ++i) {
        const QChar c = t[i];
        if (inString) {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '[' || c == '{') ++depth;
        else if (c == ']' || c == '}') --depth;
        else if (c == ',' && depth == 0) out << t.mid(start, i - start).trimmed(), start = i + 1;
    }
    const QString last = t.mid(start, t.size() - 1 - start).trimmed();
    if (!last.isEmpty() || !out.isEmpty()) out << last;
    return out;
}

// A cell's text of an array element: a number as written, a non-finite float by name; nothing for anything else
std::optional<QString> cellOf(const QString& element) {
    if (jsonNumber().match(element).hasMatch()) return element;
    for (const char* name : {"nan", "inf", "-inf"})
        if (element == QStringLiteral(R"({"$float":"%1"})").arg(QLatin1String(name))) return QString::fromLatin1(name);
    return std::nullopt;
}

// An array element of a cell's text (empty: NaN), or nothing when it is not a number
std::optional<QString> elementOf(const QString& cell) {
    const QString t = cell.trimmed();
    if (t.isEmpty()) return QStringLiteral(R"({"$float":"nan"})");
    if (jsonNumber().match(t).hasMatch()) return t;
    const QString lower = t.toLower();
    if (lower == "nan") return QStringLiteral(R"({"$float":"nan"})");
    if (lower == "inf" || lower == "+inf" || lower == "infinity") return QStringLiteral(R"({"$float":"inf"})");
    if (lower == "-inf" || lower == "-infinity") return QStringLiteral(R"({"$float":"-inf"})");
    return std::nullopt;
}

const QStringList& seriesMethods() {
    static const QStringList methods{"plot", "scatter", "bar", "barh", "hist", "fill_between", "fill_betweenx", "errorbar", "step",
                                     "stairs", "stem", "boxplot", "violinplot", "pie", "fill", "hlines", "vlines", "axhline", "axvline",
                                     "axhspan", "axvspan", "loglog", "semilogx", "semilogy"};
    return methods;
}

// The rows of pasted or imported text: cells split by tabs, else semicolons, else commas
QList<QStringList> rowsOf(const QString& text) {
    QList<QStringList> rows;
    const QString separator = text.contains('\t') ? QStringLiteral("\t") : text.contains(';') ? QStringLiteral(";") : QStringLiteral(",");
    for (const QString& line : text.split(QRegularExpression(QStringLiteral("\r?\n")))) {
        if (line.trimmed().isEmpty()) continue;
        QStringList cells;
        for (const QString& c : line.split(separator)) cells << c.trimmed();
        rows << cells;
    }
    return rows;
}

}  // namespace

PlotData::PlotData(QWidget* parent) : QWidget(parent) {
    setObjectName("plotData");
    auto* layout = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    series_ = new QComboBox(this);
    series_->setObjectName("plotDataSeries");
    top->addWidget(series_, 1);
    auto* add = new QToolButton(this);
    add->setObjectName("plotAddSeries");
    add->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(add);
    for (const char* method : {"plot", "scatter", "bar"}) {
        QAction* a = menu->addAction(QString());
        a->setObjectName(QStringLiteral("add:%1").arg(QLatin1String(method)));
        const QString m = QString::fromLatin1(method);
        connect(a, &QAction::triggered, this, [this, m] {
            std::string error;
            showNewest_ = true;
            edit(calculate_plot::appendCall(text(figure_), text(QStringLiteral(R"({"on": %1, "call": %2, "args": [[], []]})").arg(jsonString(axes_), jsonString(m))),
                                            &error),
                 error);
        });
    }
    add->setMenu(menu);
    top->addWidget(add);
    auto* removeButton = new QToolButton(this);
    auto* remove = new QAction(this);
    remove->setObjectName("plotRemoveSeries");
    removeButton->setDefaultAction(remove);
    top->addWidget(removeButton);
    connect(remove, &QAction::triggered, this, [this] {
        const int call = seriesCall();
        if (call < 0) return;
        std::string error;
        edit(calculate_plot::removeCall(text(figure_), std::size_t(call), &error), error);
    });
    layout->addLayout(top);
    message_ = new QLabel(this);
    message_->setObjectName("plotDataMessage");
    message_->setWordWrap(true);
    layout->addWidget(message_);
    table_ = new QTableWidget(this);
    table_->setObjectName("plotTable");
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->installEventFilter(this);
    layout->addWidget(table_, 1);
    // a file of rows (CSV, TSV) as a new series
    auto* import = new QPushButton(this);
    import->setObjectName("plotImport");
    connect(import, &QPushButton::clicked, this, [this] {
        filedialogs::open(
            tr("Data (*.csv *.tsv *.txt)"),
            [this](const QString& name, const QByteArray& content) {
                if (!name.isEmpty()) importText(QString::fromUtf8(content));
            },
            this);
    });
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(import);
    bottom->addStretch(1);
    layout->addLayout(bottom);
    connect(series_, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_) fill();
    });
    connect(table_, &QTableWidget::itemChanged, this, [this] {
        if (filling_) return;
        inItemChange_ = true;
        commit();
        inItemChange_ = false;
    });
    retranslate();
}

bool PlotData::eventFilter(QObject* watched, QEvent* event) {
    if (watched == table_ && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->matches(QKeySequence::Paste)) {
        paste();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PlotData::setDocument(const QByteArray& figure, const QString& axes) {
    figure_ = figure;
    axes_ = axes;
    std::string error;
    calls_.clear();
    for (const calculate_plot::FigureCall& c : calculate_plot::figureCalls(text(figure_), &error)) {
        Call call;
        call.on = QString::fromStdString(c.on);
        call.method = QString::fromStdString(c.method);
        for (const auto& a : c.arguments) call.arguments << QString::fromStdString(a);
        for (const auto& [k, v] : c.keywords) call.keywords.append({QString::fromStdString(k), QString::fromStdString(v)});
        calls_.append(call);
    }
    filling_ = true;
    const int keep = series_->currentIndex();
    series_->clear();
    int ordinal = 0;
    for (int i : seriesCalls()) {
        QString label;
        for (const auto& [k, v] : calls_[i].keywords)
            if (k == "label") label = QJsonDocument::fromJson(("[" + v + "]").toUtf8()).array().at(0).toString();
        series_->addItem(!label.isEmpty() ? label : QStringLiteral("%1 %2").arg(calls_[i].method).arg(++ordinal));
    }
    series_->setCurrentIndex(showNewest_ ? series_->count() - 1 : std::max(0, std::min(keep, series_->count() - 1)));
    showNewest_ = false;
    filling_ = false;
    fill();
}

std::vector<int> PlotData::seriesCalls() const {
    std::vector<int> out;
    for (int i = 0; i < calls_.size(); ++i)
        if (calls_[i].on == axes_ && seriesMethods().contains(calls_[i].method)) out.push_back(i);
    return out;
}

int PlotData::seriesCall() const {
    const auto all = seriesCalls();
    const int k = series_->currentIndex();
    return k >= 0 && std::size_t(k) < all.size() ? all[std::size_t(k)] : -1;
}

std::vector<PlotData::Column> PlotData::columns(int call) const {
    const QString& m = calls_[call].method;
    const int n = int(calls_[call].arguments.size());
    std::vector<Column> out;
    const auto arg = [&out](const char* title, int index) { out.push_back(Column{QString::fromLatin1(title), index, QString()}); };
    const auto kw = [&out](const char* name) { out.push_back(Column{QString::fromLatin1(name), -1, QString::fromLatin1(name)}); };
    if (m == "plot" || m == "step" || m == "scatter" || m == "loglog" || m == "semilogx" || m == "semilogy" || m == "fill") {
        if (n == 1 && m != "fill") arg("y", 0);
        else arg("x", 0), arg("y", 1);
    } else if (m == "bar") {
        arg("x", 0), arg("height", 1);
    } else if (m == "barh") {
        arg("y", 0), arg("width", 1);
    } else if (m == "hist" || m == "pie") {
        arg("x", 0);
    } else if (m == "fill_between") {
        arg("x", 0), arg("y1", 1), arg("y2", 2);
    } else if (m == "fill_betweenx") {
        arg("y", 0), arg("x1", 1), arg("x2", 2);
    } else if (m == "errorbar") {
        arg("x", 0), arg("y", 1), kw("yerr"), kw("xerr");
    } else if (m == "stem") {
        if (n == 1) arg("heads", 0);
        else arg("locs", 0), arg("heads", 1);
    }
    return out;
}

void PlotData::fill() {
    filling_ = true;
    if (inItemChange_) {  // the item whose change runs must outlive it: texts in place, the rest tidied later
        fillInPlace();
        filling_ = false;
        QTimer::singleShot(0, this, [this] {
            if (!inItemChange_) fill();
        });
        return;
    }
    table_->clear();
    const int call = seriesCall();
    const std::vector<Column> cols = call >= 0 ? columns(call) : std::vector<Column>{};
    QList<QStringList> data;  // per column, its cells
    QString why;
    for (const Column& c : cols) {
        QString json;
        if (c.argument >= 0) {
            json = calls_[call].arguments.value(c.argument);
            if (json.isEmpty() && c.argument >= calls_[call].arguments.size()) json = QStringLiteral("[]");  // (an optional array not given)
        } else {
            json = QStringLiteral("[]");
            for (const auto& [k, v] : calls_[call].keywords)
                if (k == c.keyword) json = v;
        }
        const auto items = elements(json);
        QStringList cells;
        if (!items) why = tr("The data of %1 are not a list of numbers: they cannot be shown as a table.").arg(c.title);
        for (const QString& e : items.value_or(QStringList())) {
            const auto cell = cellOf(e);
            if (!cell) {
                why = tr("The data of %1 are not a list of numbers: they cannot be shown as a table.").arg(c.title);
                break;
            }
            cells << *cell;
        }
        data << cells;
    }
    int rows = 0;
    for (const QStringList& cells : data) rows = std::max(rows, int(cells.size()));
    table_->setColumnCount(int(cols.size()));
    QStringList titles;
    for (const Column& c : cols) titles << c.title;
    table_->setHorizontalHeaderLabels(titles);
    table_->setRowCount(cols.empty() ? 0 : rows + 1);  // and an empty row for a new point
    for (int c = 0; c < data.size(); ++c)
        for (int r = 0; r < data[c].size(); ++r) table_->setItem(r, c, new QTableWidgetItem(data[c][r]));
    const bool editable = why.isEmpty() && !cols.empty();
    table_->setEnabled(editable);
    if (!why.isEmpty()) message_->setText(why);
    else if (call >= 0 && cols.empty()) message_->setText(tr("This series' data cannot be shown as a table."));
    filling_ = false;
}

// fill() without deleting an item: the document's cells written into the table, which only grows
void PlotData::fillInPlace() {
    const int call = seriesCall();
    if (call < 0) return;
    const std::vector<Column> cols = columns(call);
    int rows = 0;
    QList<QStringList> data;
    for (const Column& c : cols) {
        QString json = QStringLiteral("[]");
        if (c.argument >= 0 && c.argument < calls_[call].arguments.size()) json = calls_[call].arguments[c.argument];
        for (const auto& [k, v] : calls_[call].keywords)
            if (c.argument < 0 && k == c.keyword) json = v;
        QStringList cells;
        for (const QString& e : elements(json).value_or(QStringList())) cells << cellOf(e).value_or(QString());
        rows = std::max(rows, int(cells.size()));
        data << cells;
    }
    if (table_->rowCount() < rows + 1) table_->setRowCount(rows + 1);
    for (int c = 0; c < data.size() && c < table_->columnCount(); ++c)
        for (int r = 0; r < table_->rowCount(); ++r) {
            const QString t = data[c].value(r);
            if (QTableWidgetItem* item = table_->item(r, c)) {
                if (item->text() != t) item->setText(t);
            } else if (!t.isEmpty()) {
                table_->setItem(r, c, new QTableWidgetItem(t));
            }
        }
}

void PlotData::commit() {
    const int call = seriesCall();
    if (call < 0) return;
    const std::vector<Column> cols = columns(call);
    // the rows up to the last one with a value; within them an empty cell is NaN
    int rows = 0;
    for (int r = 0; r < table_->rowCount(); ++r)
        for (int c = 0; c < table_->columnCount(); ++c)
            if (table_->item(r, c) && !table_->item(r, c)->text().trimmed().isEmpty()) rows = r + 1;
    std::string figure = text(figure_), error;
    for (int c = 0; c < int(cols.size()); ++c) {
        QStringList items;
        bool any = false;
        for (int r = 0; r < rows; ++r) {
            const QString cell = table_->item(r, c) ? table_->item(r, c)->text() : QString();
            const auto e = elementOf(cell);
            if (!e) return edit(std::string(), text(tr("Not a number: %1").arg(cell)));
            any = any || !cell.trimmed().isEmpty();
            items << *e;
        }
        const QString array = "[" + items.join(',') + "]";
        if (cols[std::size_t(c)].argument >= 0) {
            if (cols[std::size_t(c)].argument >= calls_[call].arguments.size() && !any) continue;  // an optional array left out
            figure = calculate_plot::setArgument(figure, std::size_t(call), std::size_t(cols[std::size_t(c)].argument), text(array), &error);
        } else {
            figure = calculate_plot::setKeyword(figure, std::size_t(call), text(cols[std::size_t(c)].keyword), any ? text(array) : std::string(), {}, &error);
        }
        if (!error.empty()) return edit(std::string(), error);
    }
    edit(figure, std::string());
}

void PlotData::edit(const std::string& edited, const std::string& error) {
    const QString why = apply_ ? apply_(edited, error) : QString::fromStdString(error);
    message_->setText(why);
    if (!why.isEmpty()) {
        showNewest_ = false;
        fill();  // the table says what the file says again
    }
}

void PlotData::paste() {
    const QList<QStringList> rows = rowsOf(QApplication::clipboard()->text());
    if (rows.isEmpty() || !table_->isEnabled()) return;
    const int top = std::max(0, table_->currentRow()), left = std::max(0, table_->currentColumn());
    filling_ = true;
    if (table_->rowCount() < top + rows.size() + 1) table_->setRowCount(top + int(rows.size()) + 1);
    for (int r = 0; r < rows.size(); ++r)
        for (int c = 0; c < rows[r].size() && left + c < table_->columnCount(); ++c) table_->setItem(top + r, left + c, new QTableWidgetItem(rows[r][c]));
    filling_ = false;
    commit();
}

void PlotData::importText(const QString& content) {
    QStringList xs, ys;
    for (const QStringList& row : rowsOf(content)) {
        if (row.size() < 2) continue;
        const auto x = elementOf(row[0]), y = elementOf(row[1]);
        if (!x || !y || row[0].isEmpty() || row[1].isEmpty()) {
            if (xs.isEmpty()) continue;  // a header row
            break;
        }
        xs << *x, ys << *y;
    }
    if (xs.isEmpty()) {
        message_->setText(tr("No columns of numbers to import: each row needs two numbers, separated by tabs, semicolons or commas."));
        return;
    }
    std::string error;
    showNewest_ = true;
    edit(calculate_plot::appendCall(text(figure_),
                                    text(QStringLiteral(R"({"on": %1, "call": "plot", "args": [[%2], [%3]]})").arg(jsonString(axes_), xs.join(','), ys.join(','))),
                                    &error),
         error);
}

void PlotData::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslate();
    QWidget::changeEvent(event);
}

void PlotData::retranslate() {
    findChild<QToolButton*>("plotAddSeries")->setText(tr("Add series"));
    findChild<QAction*>("add:plot")->setText(tr("Line"));
    findChild<QAction*>("add:scatter")->setText(tr("Points"));
    findChild<QAction*>("add:bar")->setText(tr("Bars"));
    findChild<QAction*>("plotRemoveSeries")->setText(tr("Remove"));
    findChild<QPushButton*>("plotImport")->setText(tr("Import…"));
}
