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
class QBoxLayout;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
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

    // The settings as a file: each key with the values it can take (from the gear's menu, plus the type and the angle),
    // the file of those that differ from the defaults, and reading one back. Import returns what it couldn't use.
    QMap<QString, QStringList> settingKeys() const;
    QByteArray exportSettings() const;
    QStringList importSettings(const QByteArray& file);

    // Lays the keys out for a screen of this size (the window's own; tests pass a simulated one).
    void layOutKeys(QSize screen);

    QStringList common() const { return common_; }  // the ids of Common's keys, in order

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
    void memoryStoreRequested();
    void memoryClearRequested();
    void previewRequested(int generation, const QString& expression, const calculate_core::Options& options);
    void answerRequested(int generation, const QString& key, const QString& expression, const calculate_core::Options& options);

private:
    QWidget* buildKeypad();
    QWidget* buildDirectKeys();
    QWidget* keyGrid(const QString& name, const QList<Key>& keys, const QString& prefix);
    QPushButton* buildKey(const Key& key, const QString& prefix);  // prefix: "key:" or "direct:"
    QWidget* buildStatistics();
    QWidget* buildPercentages();
    void requestPercentages();  // every answer again, a moment after the last change
    void showPercentage(int generation, const QString& key, const calculate_core::Result& result);
    void buildSettings();
    QMap<QString, QString> settingValues() const;
    void retranslate();
    void drawIcons();
    void present();
    calculate_core::Options options() const;
    void request(const QString& expression, bool allowUncertain);
    void showResult(const QString& expression, const calculate_core::Result& result);
    void requestPreview();
    void showPreview(int generation, const QString& expression, const calculate_core::Result& result);
    void dropPreviews();  // previews asked for so far are skipped, or cancelled if running
    void showNoPreview(const QString& notice = {});
    bool namePending(const calculate_core::Error& error, const QString& expression) const;
    const calculate_core::Result& shownResult() const { return previewShown_ ? preview_ : last_; }
    void enableCopy(const calculate_core::Result* result);  // nullptr: nothing to copy
    void popUpCopyMenu();
    void showCompletions(const QString& name);  // under the caret, or hidden
    // A history row's text from its expression and its value as the engine writes them (with a point).
    QString historyLabel(const QString& expression, const QString& value) const;
    void relabelHistory();  // after a change of the decimal separator
    QString shownExpression(const QString& expression) const;  // with the decimal separator in use
    void hideCompletions();
    void chooseCompletion(const QString& name);
    void popUpHistoryMenu(QPoint position);  // at a row, in the list's viewport coordinates
    void apply(const Face& face);
    void replay(int index);
    bool exactType() const;
    void updateKeys();
    QString exactRefusal(const QString& label) const;  // why Exact greys out a key
    void arrange();
    bool editCommon(const Key& key, const QString& prefix);
    void setCommon(const QStringList& ids);
    void placeCommon();
    void showMore(QWidget* key, const QStringList& others);
    void updatePreviews();
    void relabelLetters();  // small or capital, as ⇧ says
    void fillSearch();
    void filterSearch(const QString& text);
    void showSearch();

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
    QWidget* rail_ = nullptr;
    QBoxLayout* railLayout_ = nullptr;
    QMenu* modesMenu_ = nullptr;  // the modes on a phone, from ☰
    QBoxLayout* screenRow_ = nullptr;  // the screen and the side (angle, type, =)
    QBoxLayout* side_ = nullptr;
    int sideWidth_ = 0;  // the side's shared width (see retranslate)
    QBoxLayout* keyboards_ = nullptr;  // the column and the main pad
    QWidget* commonBlock_ = nullptr;  // Common's title and keys
    QWidget* commonHeader_ = nullptr;  // Common's title, Edit and Reset
    QToolButton* editCommon_ = nullptr;
    QToolButton* resetCommon_ = nullptr;
    QStringList common_;  // the ids of Common's keys (the user may change them)
    bool editingCommon_ = false;
    QSize keySize_;  // the function keys' size, as layOutKeys last gave it
    QFrame* drawer_ = nullptr;
    QFrame* moreKeys_ = nullptr;  // a key's other faces
    QLineEdit* search_ = nullptr;  // finds any function or constant
    QListWidget* searchList_ = nullptr;
    QPushButton* drawerToggle_ = nullptr;
    QPushButton* drawerClose_ = nullptr;
    bool narrow_ = false;  // the phone arrangement
    int arranged_ = -1;    // the arrangement the widgets are in: -1 none yet, 0 wide, 1 narrow
    bool shifted_ = false;  // ⇧ was pressed: the next letter is a capital
    QSize designScreen_;   // the screen the keys were laid out for
    QToolButton* panelToggle_ = nullptr;
    QToolButton* settingsButton_ = nullptr;
    QMenu* settings_ = nullptr;
    QAction* languageSection_ = nullptr;  // the headings over each setting's values
    QAction* themeSection_ = nullptr;
    QAction* decimalSection_ = nullptr;
    QAction* inputSection_ = nullptr;
    QAction* fileSection_ = nullptr;
    QMap<QString, QString> defaults_;  // settingValues() as the window started
    QPushButton* equals_ = nullptr;
    QLabel* statisticsLabel_ = nullptr;
    bool hasResult_ = false;  // whether last_ holds a result to present
    QStackedWidget* pages_ = nullptr;
    QScrollArea* keys_ = nullptr;
    QScrollArea* directScroll_ = nullptr;
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
    QLineEdit* percentFirst_ = nullptr;
    QLineEdit* percentSecond_ = nullptr;
    QToolButton* percentKeysToggle_ = nullptr;
    QTimer percentTimer_;
    int percentSerial_ = 0;
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
