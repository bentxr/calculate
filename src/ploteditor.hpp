#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class PlotData;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

// The plot page's editor (D9.5, D9.6). Two tabs, Data and Style; Style lists sections in order of use (Titles and
// labels, Lines and markers, Axes and ticks, Legend, Grid, Figure), each with the properties used most, and "More"
// with every keyword and property the library's schema gives for the call behind it; a search box finds any of
// them. The figure file is the document (D9.2): each change is an edit of it made with the library's editing calls
// and checked by the engine before it is given out (figureEdited); one the engine refuses is turned down with its
// reason.
class PlotEditor : public QWidget {
    Q_OBJECT

public:
    explicit PlotEditor(QWidget* parent = nullptr);
    ~PlotEditor() override;

    // The document shown (no figureEdited).
    void setFigure(const QByteArray& figure);
    QByteArray figure() const { return figure_; }

signals:
    void figureEdited(const QByteArray& figure);

protected:
    void changeEvent(QEvent* event) override;

private:
    // A control of one keyword of a call: its widget and how it reads and writes the value (JSON text, "" unset)
    struct Field {
        QWidget* widget = nullptr;
        std::function<void(const QString&)> show;  // the keyword's JSON text, or "" when the call has none
        std::function<bool(QString*, QString*)> read;  // the JSON text to set ("" removes), or false and why
    };
    struct Row {
        QWidget* label = nullptr;
        QWidget* field = nullptr;
        QString search;   // what the search matches (label, name, aliases)
        QString section;  // its section's id
        QWidget* panel = nullptr;  // the More panel it is in, or none (a common row)
    };
    // The call a More panel edits: its method and how to find (or make) it in the document
    struct MoreCall {
        std::function<QString()> method;      // the call's method now (the chosen series' for Lines)
        std::function<int()> index;           // the call in the document, or -1
        std::function<QString()> make;        // the call's JSON text to append when it is missing
    };
    struct Section {
        QString id;
        QToolButton* header = nullptr;
        QWidget* body = nullptr;
        QFormLayout* common = nullptr;
        QToolButton* more = nullptr;
        QVBoxLayout* panels = nullptr;          // where the More panels go
        std::vector<MoreCall> calls;
        QMap<QString, QWidget*> panelOf;        // a panel per method, made when first needed
    };

    Section& addSection(const QString& id, bool open);
    void addCommon(Section& s, QLabel* label, QWidget* field);
    QWidget* panel(Section& s, const QString& method);  // the More panel of a method
    void showPanels(Section& s);
    Field field(const QJsonObject& property, const QString& objectName);
    void refresh();   // the controls from the document
    void apply(const std::string& edited, const std::string& error);  // an edit: checked, then given out, else the message
    QString tryApply(const std::string& edited, const std::string& error);  // the same; "" when taken, else why not
    void filter(const QString& text);
    void retranslate();

    // The document's calls and targets
    QString axes() const;        // the chosen Axes' name
    QString figureName() const;  // what the file calls the figure
    int lastCall(const QString& method, const QString& on) const;
    std::vector<int> seriesCalls() const;
    int seriesCall() const;  // the chosen series' call, or -1
    QString keyword(int call, const QString& name, const QStringList& aliases) const;
    QStringList aliasesOf(const QString& method, const QString& name) const;
    void setText(const QString& method, const QString& on, const QString& text);  // set_title and the like
    void setKeyword(int call, const QString& name, const QString& value, const MoreCall* making = nullptr);
    void toggleCall(const QString& method, const QString& makeJson, bool on);

    QByteArray figure_;
    struct Call {
        QString on, method;
        QStringList arguments;
        QList<QPair<QString, QString>> keywords;
        QStringList as;
    };
    QList<Call> calls_;
    QStringList axesNames_;
    bool refreshing_ = false;

    QTabWidget* tabs_ = nullptr;
    PlotData* data_ = nullptr;
    QLineEdit* search_ = nullptr;
    QComboBox* axesChooser_ = nullptr;
    QLabel* message_ = nullptr;
    std::vector<Section> sections_;
    QList<Row> rows_;
    QList<std::function<void()>> refreshers_;  // each control's display of the document
    QList<QPair<QLabel*, std::function<QString()>>> labels_;  // translated labels
};
