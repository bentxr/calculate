#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QObject>
#include <QString>

#include <atomic>

// Owns the session and evaluates on its own thread, so long computations never freeze the window.
class Worker : public QObject {
    Q_OBJECT

public:
    // Set from any thread to abort the evaluation in progress.
    std::atomic<bool>& cancelFlag() { return cancel_; }

public slots:
    void evaluate(const QString& expression, const calculate_core::Options& options);
    void memoryAdd();
    void memorySubtract();
    void memoryClear();

signals:
    void evaluated(const QString& expression, const calculate_core::Result& result);
    void memoryChanged(const QString& memory);  // empty when cleared
    void memoryFailed();                        // M+ or M- without a previous result

private:
    calculate_core::Session session_;
    std::atomic<bool> cancel_{false};
};
