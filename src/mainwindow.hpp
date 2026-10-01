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
    QWidget* buildStatistics();
    calculate_core::Options options() const;
    void request(const QString& expression, bool allowUncertain);
    void showResult(const QString& expression, const calculate_core::Result& result);
    void updateExactAvailability();

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
};
