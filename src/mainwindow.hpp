#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QMainWindow>
#include <QThread>
#include <QTimer>

class DetailsCard;
class Lcd;
class QComboBox;
class QDialog;
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

signals:
    void evaluationRequested(const QString& expression, const calculate_core::Options& options);
    void memoryAddRequested();
    void memorySubtractRequested();
    void memoryClearRequested();

private:
    QWidget* buildKeypad();
    QWidget* buildDirectKeys();
    QWidget* buildKey(const Key& key, bool legends = true);  // the cursor pad has no legends
    QWidget* buildStatistics();
    void buildMenus();
    void buildSettings();
    void retranslate();
    void colourLegends();
    void present();
    calculate_core::Options options() const;
    void request(const QString& expression, bool allowUncertain);
    void showResult(const QString& expression, const calculate_core::Result& result);
    const Face& face(const Key& key) const;
    void press(const Key& key);
    void apply(const Face& face);
    void popUp(QMenu* menu);
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

    QListWidget* modes_ = nullptr;
    QToolButton* panelToggle_ = nullptr;
    QToolButton* settingsButton_ = nullptr;
    QDialog* settings_ = nullptr;
    QLabel* languageLabel_ = nullptr;
    QComboBox* languageBox_ = nullptr;
    QLabel* themeLabel_ = nullptr;
    QComboBox* themeBox_ = nullptr;
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
    DetailsCard* card_ = nullptr;
    QPushButton* proceed_ = nullptr;
    QLabel* busy_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QToolButton* historyToggle_ = nullptr;
    QFrame* historyPanel_ = nullptr;
    QListWidget* history_ = nullptr;
    QPlainTextEdit* statisticsValues_ = nullptr;
    QToolButton* statisticsKeysToggle_ = nullptr;
    QPushButton* shift_ = nullptr;
    QPushButton* alpha_ = nullptr;
    QMenu* modeMenu_ = nullptr;
    QMenu* configMenu_ = nullptr;
    QMenu* optionsMenu_ = nullptr;
    int historyIndex_ = -1;  // the history row ▲ and ▼ last showed
};
