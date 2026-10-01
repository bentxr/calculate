#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QMainWindow>
#include <QThread>
#include <QTimer>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;
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

signals:
    void evaluationRequested(const QString& expression, const calculate_core::Options& options);
    void memoryAddRequested();
    void memorySubtractRequested();
    void memoryClearRequested();

private:
    QWidget* buildKeypad();
    QWidget* buildKey(const Key& key, bool legends = true);  // the cursor pad has no legends
    QWidget* buildStatistics();
    calculate_core::Options options() const;
    void request(const QString& expression, bool allowUncertain);
    void showResult(const QString& expression, const calculate_core::Result& result);
    const Face& face(const Key& key) const;
    void press(const Key& key);
    void updateKeys();

    QThread thread_;
    Worker* worker_ = nullptr;
    QTimer busyTimer_;
    std::vector<calculate_core::TypeInfo> types_;
    calculate_core::Result last_;
    QString lastExpression_;

    QListWidget* modes_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QComboBox* type_ = nullptr;
    QComboBox* angle_ = nullptr;
    QLineEdit* expression_ = nullptr;
    QLabel* value_ = nullptr;
    QLabel* errorLine_ = nullptr;
    QLabel* whyLine_ = nullptr;
    QToolButton* detailsToggle_ = nullptr;
    QLabel* details_ = nullptr;
    QLabel* message_ = nullptr;
    QPushButton* proceed_ = nullptr;
    QLabel* busy_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QLabel* memory_ = nullptr;
    QListWidget* history_ = nullptr;
    QPlainTextEdit* statisticsValues_ = nullptr;
    QPushButton* shift_ = nullptr;
    QPushButton* alpha_ = nullptr;
};
