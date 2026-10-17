#include "ploteditor.hpp"

#include "plotfonts.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QTabWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <optional>
#include <tuple>

namespace {

std::string text(const QByteArray& b) { return b.toStdString(); }
std::string text(const QString& s) { return s.toStdString(); }

// A string as JSON text
QString jsonString(const QString& s) {
    const QByteArray a = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(a.mid(1, a.size() - 2));
}

// The string a JSON text holds, or nothing when it is not a string
std::optional<QString> stringOf(const QString& json) {
    const QJsonDocument d = QJsonDocument::fromJson(("[" + json + "]").toUtf8());
    if (!d.isArray() || d.array().size() != 1 || !d.array().at(0).isString()) return std::nullopt;
    return d.array().at(0).toString();
}

bool validJson(const QString& json) { return !QJsonDocument::fromJson(("[" + json + "]").toUtf8()).isNull(); }

// The schema's calls by name (for a name made on several objects, the Axes' one)
const QMap<QString, QJsonObject>& schemaCalls() {
    static const QMap<QString, QJsonObject> calls = [] {
        ensurePlotFonts();
        QMap<QString, QJsonObject> out;
        const QJsonObject schema = QJsonDocument::fromJson(QByteArray::fromStdString(calculate_plot::schema(std::string()))).object();
        for (const QJsonValue& v : schema.value("calls").toArray()) {
            const QJsonObject c = v.toObject();
            const QString name = c.value("call").toString();
            if (!out.contains(name) || c.value("on").toString() == "axes") out.insert(name, c);
        }
        return out;
    }();
    return calls;
}

// The calls whose artists are a series: what Lines and markers chooses from
const QStringList& seriesMethods() {
    static const QStringList methods{"plot", "scatter", "bar", "barh", "hist", "fill_between", "fill_betweenx", "errorbar", "step",
                                     "stairs", "stem", "boxplot", "violinplot", "pie", "fill", "hlines", "vlines", "axhline", "axvline",
                                     "axhspan", "axvspan", "loglog", "semilogx", "semilogy"};
    return methods;
}

const QRegularExpression& jsonNumber() {
    static const QRegularExpression re(QStringLiteral(R"(^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$)"));
    return re;
}

}  // namespace

PlotEditor::PlotEditor(QWidget* parent) : QWidget(parent) {
    setObjectName("plotEditor");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("plotTabs");
    layout->addWidget(tabs_);
    auto* data = new QWidget;  // the series' tables (9.09)
    data->setObjectName("plotDataTab");
    tabs_->addTab(data, QString());
    auto* style = new QWidget;
    style->setObjectName("plotStyleTab");
    tabs_->addTab(style, QString());
    tabs_->setCurrentIndex(1);
    auto* styleLayout = new QVBoxLayout(style);
    search_ = new QLineEdit(style);
    search_->setObjectName("plotSearch");
    search_->setClearButtonEnabled(true);
    styleLayout->addWidget(search_);
    axesChooser_ = new QComboBox(style);
    axesChooser_->setObjectName("plotAxes");
    styleLayout->addWidget(axesChooser_);
    message_ = new QLabel(style);
    message_->setObjectName("plotEditMessage");
    message_->setWordWrap(true);
    message_->hide();
    styleLayout->addWidget(message_);
    auto* scroll = new QScrollArea(style);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto* list = new QWidget;
    auto* listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(list);
    styleLayout->addWidget(scroll, 1);
    connect(search_, &QLineEdit::textChanged, this, &PlotEditor::filter);
    connect(axesChooser_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_) refresh();
    });

    sections_.reserve(6);  // (the sections are referred to by address)
    const auto label = [this](std::function<QString()> title) {
        auto* l = new QLabel;
        labels_.append({l, std::move(title)});
        return l;
    };
    const auto lineEdit = [](const QString& name) {
        auto* e = new QLineEdit;
        e->setObjectName(name);
        return e;
    };
    // A text that is a call's first argument (set_title, set_xlabel, set_ylabel, suptitle)
    const auto textRow = [this, label, lineEdit](Section& s, const QString& name, std::function<QString()> title, const QString& method,
                                                  std::function<QString()> on) {
        QLineEdit* e = lineEdit(name);
        addCommon(s, label(std::move(title)), e);
        connect(e, &QLineEdit::editingFinished, this, [this, e, method, on] {
            if (refreshing_) return;
            const int i = lastCall(method, on());
            const QString now = i >= 0 ? stringOf(calls_[i].arguments.value(0)).value_or(QString()) : QString();
            if (e->text() != now) setText(method, on(), e->text());
        });
        refreshers_.append([this, e, method, on] {
            const int i = lastCall(method, on());
            e->setText(i >= 0 ? stringOf(calls_[i].arguments.value(0)).value_or(QString()) : QString());
        });
    };
    const auto axesOn = [this] { return axes(); };

    // Titles and labels
    {
        Section& s = addSection("titles", true);
        textRow(s, "edit:title", [] { return tr("Title"); }, "set_title", axesOn);
        textRow(s, "edit:xlabel", [] { return tr("x label"); }, "set_xlabel", axesOn);
        textRow(s, "edit:ylabel", [] { return tr("y label"); }, "set_ylabel", axesOn);
        s.calls.push_back({[] { return QStringLiteral("set_title"); }, [this] { return lastCall("set_title", axes()); },
                           [this] { return QStringLiteral(R"({"on": %1, "call": "set_title", "args": [""]})").arg(jsonString(axes())); }});
    }
    // Lines and markers: the chosen series' call
    {
        Section& s = addSection("lines", true);
        auto* series = new QComboBox;
        series->setObjectName("edit:series");
        addCommon(s, label([] { return tr("Series"); }), series);
        connect(series, &QComboBox::currentIndexChanged, this, [this] {
            if (!refreshing_) refresh();
        });
        refreshers_.append([this, series] {
            const int keep = series->currentIndex();
            series->clear();
            int ordinal = 0;
            for (int i : seriesCalls()) {
                const auto labelText = stringOf(keyword(i, "label", {}));
                series->addItem(labelText && !labelText->isEmpty() ? *labelText : QStringLiteral("%1 %2").arg(calls_[i].method).arg(++ordinal));
            }
            series->setCurrentIndex(keep >= 0 && keep < series->count() ? keep : 0);
        });
        // the common properties of a series, as keywords of its call
        const auto common = [this, &s, label](const QString& name, std::function<QString()> title, QWidget* w, Field f) {
            addCommon(s, label(std::move(title)), w);
            refreshers_.append([this, f, name] {
                const int i = seriesCall();
                f.widget->setEnabled(i >= 0);
                f.show(i >= 0 ? keyword(i, name, aliasesOf(calls_[i].method, name)) : QString());
            });
        };
        const auto bind = [this](const QString& name, const Field& f) {
            return [this, f, name] {
                if (refreshing_) return;
                const int i = seriesCall();
                if (i < 0) return;
                QString value, why;
                if (!f.read(&value, &why)) return apply(std::string(), text(why));
                if (value != keyword(i, name, aliasesOf(calls_[i].method, name))) setKeyword(i, name, value);
            };
        };
        for (const auto& [name, kind, title] : std::vector<std::tuple<QString, QString, std::function<QString()>>>{
                 {"color", "colour", [] { return tr("Colour"); }},
                 {"linewidth", "number", [] { return tr("Width"); }},
                 {"label", "text", [] { return tr("Label"); }}}) {
            Field f = field(QJsonObject{{"name", name}, {"kind", kind}}, "edit:" + name);
            common(name, title, f.widget, f);
            auto* e = f.widget->findChild<QLineEdit*>() ? f.widget->findChild<QLineEdit*>() : qobject_cast<QLineEdit*>(f.widget);
            if (e) connect(e, &QLineEdit::editingFinished, this, bind(name, f));
        }
        for (const auto& [name, choices, title] : std::vector<std::tuple<QString, QStringList, std::function<QString()>>>{
                 {"linestyle", {"-", "--", "-.", ":", "None"}, [] { return tr("Line"); }},
                 {"marker", {"None", ".", "o", "s", "^", "v", "D", "x", "+", "*"}, [] { return tr("Marker"); }}}) {
            QJsonArray c;
            for (const QString& v : choices) c.append(v);
            Field f = field(QJsonObject{{"name", name}, {"kind", "choice"}, {"choices", c}}, "edit:" + name);
            common(name, title, f.widget, f);
            connect(qobject_cast<QComboBox*>(f.widget), &QComboBox::activated, this, bind(name, f));
        }
        s.calls.push_back({[this] { const int i = seriesCall(); return i >= 0 ? calls_[i].method : QString(); },
                           [this] { return seriesCall(); }, [] { return QString(); }});
    }
    // Axes and ticks: the scales, and tick_params in More
    {
        Section& s = addSection("axes", false);
        for (const QString& which : {QStringLiteral("x"), QStringLiteral("y")}) {
            auto* scale = new QComboBox;
            scale->setObjectName("edit:" + which + "scale");
            scale->addItems({QString(), "linear", "log", "symlog", "logit"});
            addCommon(s, label([which] { return which == "x" ? tr("x scale") : tr("y scale"); }), scale);
            const QString method = "set_" + which + "scale";
            connect(scale, &QComboBox::activated, this, [this, scale, method] {
                if (refreshing_) return;
                const QString chosen = scale->currentIndex() > 0 ? scale->currentText() : QString();
                const int i = lastCall(method, axes());
                if (chosen.isEmpty()) {
                    if (i >= 0) {
                        std::string error;
                        apply(calculate_plot::removeCall(text(figure_), std::size_t(i), &error), error);
                    }
                    return;
                }
                setText(method, axes(), chosen);
            });
            refreshers_.append([this, scale, method] {
                const int i = lastCall(method, axes());
                const QString now = i >= 0 ? stringOf(calls_[i].arguments.value(0)).value_or(QString()) : QString();
                scale->setCurrentIndex(std::max(0, scale->findText(now)));
            });
        }
        s.calls.push_back({[] { return QStringLiteral("tick_params"); }, [this] { return lastCall("tick_params", axes()); },
                           [this] { return QStringLiteral(R"({"on": %1, "call": "tick_params"})").arg(jsonString(axes())); }});
    }
    // Legend and Grid: a call switched on and off
    const auto switchRow = [this, label](Section& s, const QString& method, std::function<QString()> title, std::function<QString()> make) {
        auto* box = new QCheckBox;
        box->setObjectName("edit:" + method);
        addCommon(s, label(std::move(title)), box);
        connect(box, &QCheckBox::clicked, this, [this, method, make](bool on) {
            if (!refreshing_) toggleCall(method, make(), on);
        });
        refreshers_.append([this, box, method] { box->setChecked(lastCall(method, axes()) >= 0); });
    };
    {
        Section& s = addSection("legend", false);
        const auto make = [this] { return QStringLiteral(R"({"on": %1, "call": "legend"})").arg(jsonString(axes())); };
        switchRow(s, "legend", [] { return tr("Show"); }, make);
        s.calls.push_back({[] { return QStringLiteral("legend"); }, [this] { return lastCall("legend", axes()); }, make});
    }
    {
        Section& s = addSection("grid", false);
        const auto make = [this] { return QStringLiteral(R"({"on": %1, "call": "grid", "args": [true]})").arg(jsonString(axes())); };
        switchRow(s, "grid", [] { return tr("Show"); }, make);
        s.calls.push_back({[] { return QStringLiteral("grid"); }, [this] { return lastCall("grid", axes()); }, make});
    }
    // Figure: its title, and in More the call that makes it and suptitle's properties
    {
        Section& s = addSection("figure", false);
        textRow(s, "edit:suptitle", [] { return tr("Figure title"); }, "suptitle", [this] { return figureName(); });
        s.calls.push_back({[this] { return calls_.isEmpty() ? QString() : calls_[0].method; }, [this] { return calls_.isEmpty() ? -1 : 0; },
                           [] { return QString(); }});
        s.calls.push_back({[] { return QStringLiteral("suptitle"); }, [this] { return lastCall("suptitle", figureName()); },
                           [this] { return QStringLiteral(R"({"on": %1, "call": "suptitle", "args": [""]})").arg(jsonString(figureName())); }});
    }
    for (Section& s : sections_) listLayout->addWidget(s.header), listLayout->addWidget(s.body);
    listLayout->addStretch(1);
    retranslate();
}

PlotEditor::~PlotEditor() {
    // the controls go after this editor's members: a field losing the focus then must not reach them
    for (QObject* child : findChildren<QObject*>()) QObject::disconnect(child, nullptr, this, nullptr);
}

PlotEditor::Section& PlotEditor::addSection(const QString& id, bool open) {
    sections_.emplace_back();
    Section& s = sections_.back();
    s.id = id;
    s.header = new QToolButton;
    s.header->setObjectName("section:" + id);
    s.header->setCheckable(true);
    s.header->setChecked(open);
    s.header->setAutoRaise(true);
    s.header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    s.header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    s.header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont bold = s.header->font();
    bold.setBold(true);
    s.header->setFont(bold);
    s.body = new QWidget;
    s.body->setObjectName("body:" + id);
    auto* bodyLayout = new QVBoxLayout(s.body);
    bodyLayout->setContentsMargins(12, 0, 0, 6);
    auto* commonWidget = new QWidget;
    s.common = new QFormLayout(commonWidget);
    s.common->setContentsMargins(0, 0, 0, 0);
    bodyLayout->addWidget(commonWidget);
    s.more = new QToolButton;
    s.more->setObjectName("more:" + id);
    s.more->setCheckable(true);
    s.more->setAutoRaise(true);
    bodyLayout->addWidget(s.more, 0, Qt::AlignLeft);
    s.panels = new QVBoxLayout;
    bodyLayout->addLayout(s.panels);
    s.body->setVisible(open);
    QToolButton* header = s.header;
    QWidget* body = s.body;
    connect(header, &QToolButton::toggled, this, [this, header, body](bool on) {
        header->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        if (search_->text().isEmpty()) body->setVisible(on);
    });
    const QString sectionId = id;
    connect(s.more, &QToolButton::toggled, this, [this, sectionId] {
        for (Section& x : sections_)
            if (x.id == sectionId) showPanels(x);
        retranslate();
    });
    return s;
}

void PlotEditor::addCommon(Section& s, QLabel* label, QWidget* fieldWidget) {
    s.common->addRow(label, fieldWidget);
    rows_.append({label, fieldWidget, fieldWidget->objectName(), s.id, nullptr});
}

QStringList PlotEditor::aliasesOf(const QString& method, const QString& name) const {
    const QJsonObject c = schemaCalls().value(method);
    for (const char* group : {"parameters", "properties"})
        for (const QJsonValue& v : c.value(group).toArray())
            if (v.toObject().value("name").toString() == name) {
                QStringList out;
                for (const QJsonValue& a : v.toObject().value("aliases").toArray()) out << a.toString();
                return out;
            }
    return {};
}

PlotEditor::Field PlotEditor::field(const QJsonObject& property, const QString& objectName) {
    Field f;
    const QString kind = property.value("kind").toString();
    if (kind == "bool" || kind == "choice") {
        auto* box = new QComboBox;
        box->setObjectName(objectName);
        box->addItem(QString());  // the default
        QStringList values;
        if (kind == "bool") values = {"true", "false"};
        for (const QJsonValue& c : property.value("choices").toArray()) values << c.toString();
        for (const QString& v : values) box->addItem(kind == "bool" ? (v == "true" ? QStringLiteral("True") : QStringLiteral("False")) : v, v);
        f.widget = box;
        f.show = [box, kind](const QString& json) {
            QString v = json;
            if (kind != "bool") v = stringOf(json).value_or(json);
            const int i = json.isEmpty() ? 0 : box->findData(v);
            box->setCurrentIndex(i >= 0 ? i : 0);
        };
        f.read = [box, kind](QString* value, QString*) {
            const QString v = box->currentData().toString();
            *value = box->currentIndex() <= 0 ? QString() : kind == "bool" ? v : jsonString(v);
            return true;
        };
        return f;
    }
    auto* line = new QLineEdit;
    line->setObjectName(objectName);
    f.widget = line;
    if (kind == "number") line->setValidator(new QRegularExpressionValidator(jsonNumber(), line));
    const bool plain = kind == "text" || kind == "colour";  // a string typed as is; anything else is JSON
    f.show = [line, plain](const QString& json) { line->setText(plain ? stringOf(json).value_or(json) : json); };
    f.read = [line, plain, kind](QString* value, QString* why) {
        const QString t = line->text();
        if (t.isEmpty()) return *value = QString(), true;
        if (kind == "colour" && (t.startsWith('[') || t.startsWith('{'))) {  // a tuple of numbers, or a $-form
            if (!validJson(t)) return *why = tr("Not a colour: %1").arg(t), false;
            return *value = t, true;
        }
        if (plain) return *value = jsonString(t), true;
        if (!validJson(t)) return *why = tr("Not a value a figure file can hold: %1").arg(t), false;
        return *value = t, true;
    };
    if (kind == "colour") {  // a swatch beside it opens a colour chooser
        auto* holder = new QWidget;
        auto* row = new QHBoxLayout(holder);
        row->setContentsMargins(0, 0, 0, 0);
        auto* swatch = new QToolButton(holder);
        swatch->setObjectName(objectName + ":choose");
        swatch->setText(QStringLiteral("…"));
        row->addWidget(line, 1);
        row->addWidget(swatch);
        connect(swatch, &QToolButton::clicked, this, [this, line] {
            const QColor c = QColorDialog::getColor(QColor(line->text()), this);
            if (!c.isValid()) return;
            line->setText(c.name(QColor::HexRgb));
            emit line->editingFinished();
        });
        f.widget = holder;
    }
    return f;
}

QWidget* PlotEditor::panel(Section& s, const QString& method) {
    if (s.panelOf.contains(method)) return s.panelOf.value(method);
    auto* p = new QWidget;
    p->setObjectName("panel:" + s.id + ":" + method);
    auto* form = new QFormLayout(p);
    form->setContentsMargins(0, 4, 0, 4);
    const QJsonObject call = schemaCalls().value(method);
    // the MoreCall of this section that edits this method
    std::size_t which = 0;
    for (std::size_t k = 0; k < s.calls.size(); ++k)
        if (s.calls[k].method() == method) which = k;
    const QString sectionId = s.id;
    for (const char* group : {"parameters", "properties"})
        for (const QJsonValue& v : call.value(group).toArray()) {
            const QJsonObject property = v.toObject();
            if (property.value("supported") == QJsonValue(false) || property.value("positional").toBool()) continue;
            const QString name = property.value("name").toString();
            if (form->rowCount() && p->findChild<QWidget*>(QStringLiteral("prop:%1:%2").arg(method, name))) continue;  // (a name in both)
            QStringList aliases;
            for (const QJsonValue& a : property.value("aliases").toArray()) aliases << a.toString();
            Field f = field(property, QStringLiteral("prop:%1:%2").arg(method, name));
            auto* label = new QLabel(name);
            label->setToolTip(property.value("valid").toString() + (aliases.isEmpty() ? QString() : "\n" + aliases.join(", ")));
            form->addRow(label, f.widget);
            rows_.append({label, f.widget, name + " " + aliases.join(" "), s.id, p});
            const auto target = [this, sectionId, which]() -> const MoreCall* {
                for (const Section& x : sections_)
                    if (x.id == sectionId) return &x.calls[which];
                return nullptr;
            };
            refreshers_.append([this, f, name, aliases, target, method] {
                const MoreCall* m = target();
                const int i = m && m->method() == method ? m->index() : -1;
                f.show(i >= 0 ? keyword(i, name, aliases) : QString());
            });
            const auto edited = [this, f, name, aliases, target, method] {
                if (refreshing_) return;
                const MoreCall* m = target();
                if (!m || m->method() != method) return;
                QString value, why;
                if (!f.read(&value, &why)) return apply(std::string(), text(why));
                const int i = m->index();
                if (i >= 0 && value == keyword(i, name, aliases)) return;
                if (i < 0 && value.isEmpty()) return;
                setKeyword(i, name, value, m);
            };
            if (auto* box = qobject_cast<QComboBox*>(f.widget)) connect(box, &QComboBox::activated, this, edited);
            else if (auto* line = f.widget->findChild<QLineEdit*>() ? f.widget->findChild<QLineEdit*>() : qobject_cast<QLineEdit*>(f.widget))
                connect(line, &QLineEdit::editingFinished, this, edited);
        }
    s.panels->addWidget(p);
    s.panelOf.insert(method, p);
    const bool was = refreshing_;
    refreshing_ = true;
    for (const auto& r : refreshers_) r();
    refreshing_ = was;
    return p;
}

void PlotEditor::showPanels(Section& s) {
    QStringList shown;
    if (s.more->isChecked())
        for (const MoreCall& m : s.calls)
            if (!m.method().isEmpty()) shown << m.method();
    for (const QString& method : shown) panel(s, method);
    for (auto it = s.panelOf.begin(); it != s.panelOf.end(); ++it) it.value()->setVisible(shown.contains(it.key()));
}

void PlotEditor::setFigure(const QByteArray& figure) {
    figure_ = figure;
    refresh();
}

void PlotEditor::refresh() {
    refreshing_ = true;
    std::string error;
    calls_.clear();
    for (const calculate_plot::FigureCall& c : calculate_plot::figureCalls(text(figure_), &error)) {
        Call call;
        call.on = QString::fromStdString(c.on);
        call.method = QString::fromStdString(c.method);
        for (const auto& a : c.arguments) call.arguments << QString::fromStdString(a);
        for (const auto& [k, v] : c.keywords) call.keywords.append({QString::fromStdString(k), QString::fromStdString(v)});
        for (const auto& n : c.as) call.as << QString::fromStdString(n);
        calls_.append(call);
    }
    ensurePlotFonts();
    const QString chosen = axesChooser_->currentText();
    axesNames_.clear();
    for (const auto& n : calculate_plot::axesNames(text(figure_), std::string(), &error)) axesNames_ << QString::fromStdString(n);
    axesChooser_->clear();
    axesChooser_->addItems(axesNames_);
    axesChooser_->setCurrentIndex(std::max(0, int(axesNames_.indexOf(chosen))));
    axesChooser_->setVisible(axesNames_.size() > 1);
    for (const auto& r : refreshers_) r();
    for (Section& s : sections_) showPanels(s);
    refreshing_ = false;
    if (!search_->text().isEmpty()) filter(search_->text());
}

void PlotEditor::apply(const std::string& edited, const std::string& error) {
    std::string why = error;
    if (why.empty() && edited.empty()) why = "nothing to change";
    if (why.empty()) {
        ensurePlotFonts();
        why = calculate_plot::checkFigure(edited, std::string());
    }
    if (!why.empty()) {
        message_->setText(QString::fromStdString(why));
        message_->show();
        refresh();  // the controls say what the file says again
        return;
    }
    message_->hide();
    figure_ = QByteArray::fromStdString(edited);
    refresh();
    emit figureEdited(figure_);
}

QString PlotEditor::axes() const { return axesChooser_->currentIndex() >= 0 ? axesNames_.value(axesChooser_->currentIndex()) : QString(); }

QString PlotEditor::figureName() const {
    for (const Call& c : calls_)
        if ((c.method == "subplots" || c.method == "figure") && !c.as.isEmpty()) return c.as.first();
    return QStringLiteral("fig");
}

int PlotEditor::lastCall(const QString& method, const QString& on) const {
    for (int i = int(calls_.size()) - 1; i >= 0; --i)
        if (calls_[i].method == method && calls_[i].on == on) return i;
    return -1;
}

std::vector<int> PlotEditor::seriesCalls() const {
    std::vector<int> out;
    const QString on = axes();
    for (int i = 0; i < calls_.size(); ++i)
        if (calls_[i].on == on && seriesMethods().contains(calls_[i].method)) out.push_back(i);
    return out;
}

int PlotEditor::seriesCall() const {
    const auto all = seriesCalls();
    auto* box = findChild<QComboBox*>("edit:series");
    const int k = box ? box->currentIndex() : -1;
    return k >= 0 && std::size_t(k) < all.size() ? all[std::size_t(k)] : -1;
}

QString PlotEditor::keyword(int call, const QString& name, const QStringList& aliases) const {
    if (call < 0 || call >= calls_.size()) return {};
    QString found;
    for (const auto& [k, v] : calls_[call].keywords)
        if (k == name || aliases.contains(k)) found = v;
    return found;
}

void PlotEditor::setText(const QString& method, const QString& on, const QString& value) {
    std::string error;
    const int i = lastCall(method, on);
    std::string edited;
    if (value.isEmpty()) {
        if (i < 0) return;
        edited = calculate_plot::removeCall(text(figure_), std::size_t(i), &error);
    } else if (i >= 0) {
        edited = calculate_plot::setArgument(text(figure_), std::size_t(i), 0, text(jsonString(value)), &error);
    } else {
        edited = calculate_plot::appendCall(text(figure_),
                                            text(QStringLiteral(R"({"on": %1, "call": %2, "args": [%3]})").arg(jsonString(on), jsonString(method), jsonString(value))),
                                            &error);
    }
    apply(edited, error);
}

void PlotEditor::setKeyword(int call, const QString& name, const QString& value, const MoreCall* making) {
    std::string error;
    std::string figure = text(figure_);
    if (call < 0) {  // the call does not exist yet: made first
        if (!making || making->make().isEmpty()) return;
        figure = calculate_plot::appendCall(figure, text(making->make()), &error);
        if (!error.empty()) return apply(std::string(), error);
        call = int(calculate_plot::figureCalls(figure, &error).size()) - 1;
    }
    const QString method = QString::fromStdString(calculate_plot::figureCalls(figure, &error)[std::size_t(call)].method);
    std::vector<std::string> aliases;
    for (const QString& a : aliasesOf(method, name)) aliases.push_back(text(a));
    apply(calculate_plot::setKeyword(figure, std::size_t(call), text(name), text(value), aliases, &error), error);
}

void PlotEditor::toggleCall(const QString& method, const QString& makeJson, bool on) {
    std::string error;
    const int i = lastCall(method, axes());
    if (on && i < 0) apply(calculate_plot::appendCall(text(figure_), text(makeJson), &error), error);
    else if (!on && i >= 0) apply(calculate_plot::removeCall(text(figure_), std::size_t(i), &error), error);
}

void PlotEditor::filter(const QString& query) {
    const QString q = query.trimmed();
    if (q.isEmpty()) {  // the sections as their headers and More buttons say
        for (const Row& r : rows_) r.label->show(), r.field->show();
        for (Section& s : sections_) {
            s.header->show();
            s.body->setVisible(s.header->isChecked());
            s.more->show();
            showPanels(s);
        }
        return;
    }
    // every More panel is searched: the calls' panels are made
    for (Section& s : sections_)
        for (const MoreCall& m : s.calls)
            if (!m.method().isEmpty()) panel(s, m.method());
    for (Section& s : sections_) {
        bool any = false;
        QStringList current;
        for (const MoreCall& m : s.calls) current << m.method();
        for (auto it = s.panelOf.begin(); it != s.panelOf.end(); ++it) {
            bool inPanel = false;
            for (const Row& r : rows_)
                if (r.panel == it.value() && r.search.contains(q, Qt::CaseInsensitive)) inPanel = true;
            it.value()->setVisible(inPanel && current.contains(it.key()));
        }
        for (const Row& r : rows_) {
            if (r.section != s.id) continue;
            const bool match = r.search.contains(q, Qt::CaseInsensitive) || (qobject_cast<QLabel*>(r.label) &&
                                                                              qobject_cast<QLabel*>(r.label)->text().contains(q, Qt::CaseInsensitive));
            const bool live = !r.panel || r.panel->isVisibleTo(s.body);
            r.label->setVisible(match && live);
            r.field->setVisible(match && live);
            any = any || (match && live);
        }
        s.header->setVisible(any);
        s.body->setVisible(any);
        s.more->hide();
    }
}

void PlotEditor::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslate();
    QWidget::changeEvent(event);
}

void PlotEditor::retranslate() {
    tabs_->setTabText(0, tr("Data"));
    tabs_->setTabText(1, tr("Style"));
    search_->setPlaceholderText(tr("Find a property"));
    const std::pair<const char*, QString> titles[] = {{"titles", tr("Titles and labels")}, {"lines", tr("Lines and markers")},
                                                      {"axes", tr("Axes and ticks")},      {"legend", tr("Legend")},
                                                      {"grid", tr("Grid")},                {"figure", tr("Figure")}};
    for (Section& s : sections_) {
        for (const auto& [id, title] : titles)
            if (s.id == QLatin1String(id)) s.header->setText(title);
        s.more->setText(s.more->isChecked() ? tr("Fewer properties") : tr("More: every property"));
    }
    for (const auto& [label, title] : labels_) label->setText(title());
}
