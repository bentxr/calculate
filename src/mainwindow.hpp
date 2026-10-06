#pragma once

#include "entry.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QMainWindow>
#include <QThread>
#include <QTimer>

class DetailsCard;
class FormulaTip;
class Lcd;
class QAction;
class QComboBox;
class QFrame;
class QLabel;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class TypeChooser;
class Worker;
struct Face;
struct Key;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    void evaluate();

protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void evaluationRequested(const QString& expression, const calculate_core::Options& options);
    void memoryAddRequested();
    void memorySubtractRequested();
    void memoryClearRequested();
    void previewRequested(int generation, const QString& expression, const calculate_core::Options& options);

private:
    QWidget* buildKeypad();
    QWidget* buildDirectKeys();
    QPushButton* buildKey(const Key& key, const QString& prefix);  // prefix: "key:" or "direct:"
    QWidget* buildStatistics();
    void buildSettings();
    void retranslate();
    void present();
    calculate_core::Options options() const;
    void request(const QString& expression, bool allowUncertain);
    void showResult(const QString& expression, const calculate_core::Result& result);
    void requestPreview();
    void showPreview(int generation, const QString& expression, const calculate_core::Result& result);
    void dropPreviews();  // previews asked for so far are skipped, or cancelled if running
    void showNoPreview(const QString& notice = {});
    const calculate_core::Result& shownResult() const { return previewShown_ ? preview_ : last_; }
    void enableCopy(const calculate_core::Result* result);  // nullptr: nothing to copy
    void popUpCopyMenu();
    void showCompletions(const QString& name);  // under the caret, or hidden
    void popUpHistoryMenu(QPoint position);  // at a row, in the list's viewport coordinates
    void apply(const Face& face);
    void replay(int index);
    bool exactType() const;
    void updateKeys();
    QString exactRefusal(const QString& label) const;  // why Exact greys out a key
    void sizeKeys();

    static constexpr int keySpacing = 6;
    static constexpr int keypadGap = 12;  // between the function keys and the number keys

    QThread thread_;
    Worker* worker_ = nullptr;
    QTimer busyTimer_;
    int pending_ = 0;  // requests the worker hasn't answered yet
    std::vector<calculate_core::TypeInfo> types_;
    calculate_core::Result last_;
    QString lastExpression_;
    // The result while typing: asked for once the typing pauses, shown until = or the next edit replaces it.
    static constexpr int liveDelay = 250;
    QTimer liveTimer_;
    static constexpr int previewLimit = 2000;  // longer than this is left for =
    QTimer previewLimit_;
    int previewSerial_ = 0;
    calculate_core::Result preview_;
    QString previewExpression_;
    bool previewShown_ = false;

    QListWidget* modes_ = nullptr;
    QToolButton* panelToggle_ = nullptr;
    QToolButton* settingsButton_ = nullptr;
    QMenu* settings_ = nullptr;
    QAction* languageSection_ = nullptr;  // the headings over each setting's values
    QAction* themeSection_ = nullptr;
    QAction* inputSection_ = nullptr;
    QPushButton* equals_ = nullptr;
    QLabel* statisticsLabel_ = nullptr;
    bool hasResult_ = false;  // whether last_ holds a result to present
    QStackedWidget* pages_ = nullptr;
    QScrollArea* keys_ = nullptr;
    bool keysSized_ = false;
    TypeChooser* type_ = nullptr;
    QComboBox* angle_ = nullptr;
    Lcd* lcd_ = nullptr;
    QToolButton* detailsButton_ = nullptr;
    QToolButton* copyButton_ = nullptr;
    QMenu* copyMenu_ = nullptr;
    QMenu* copyAsMenu_ = nullptr;  // the same forms in the edit menu, where "Value" alone would be unclear
    QToolButton* editButton_ = nullptr;
    QToolButton* keyboardButton_ = nullptr;
    DetailsCard* card_ = nullptr;
    FormulaTip* formulaTip_ = nullptr;
    QLabel* message_ = nullptr;
    QPushButton* proceed_ = nullptr;
    QLabel* busy_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QToolButton* historyToggle_ = nullptr;
    QFrame* historyPanel_ = nullptr;
    QListWidget* history_ = nullptr;
    QPlainTextEdit* statisticsValues_ = nullptr;
    QToolButton* statisticsKeysToggle_ = nullptr;
    std::vector<Entry> historyEntries_;  // as typed, one per history row
    std::vector<calculate_core::Result> historyResults_;  // the same rows' results
    QMenu* historyMenu_ = nullptr;
    static constexpr int completionRows = 8;  // the most shown at once; more scroll
    QListWidget* completions_ = nullptr;
    Entry typed_;                        // the last input sent with =
    int historyIndex_ = -1;  // the history row ▲ and ▼ last showed
    Entry unfinished_;              // what was being typed when the browsing began
    bool keepsUnfinished_ = false;
};
