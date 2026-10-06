#include "worker.hpp"

void Worker::evaluate(const QString& expression, const calculate_core::Options& options) {
    cancel_ = false;
    calculate_core::Options o = options;
    o.cancel = &cancel_;
    emit evaluated(expression, session_.evaluate(expression.toStdString(), o));
}

void Worker::preview(int generation, const QString& expression, const calculate_core::Options& options) {
    // The window stores the new generation before raising the flag, so clearing the flag first means a stale
    // request is either skipped here or cancelled while it runs, never run in full.
    previewCancel_ = false;
    if (generation != previewGeneration_) return;
    calculate_core::Options o = options;
    o.cancel = &previewCancel_;
    emit previewed(generation, expression, session_.preview(expression.toStdString(), o));
}

void Worker::memoryAdd() {
    if (session_.memoryAdd()) emit memoryChanged(QString::fromStdString(session_.memory()));
    else emit memoryFailed();
}

void Worker::memorySubtract() {
    if (session_.memorySubtract()) emit memoryChanged(QString::fromStdString(session_.memory()));
    else emit memoryFailed();
}

void Worker::memoryClear() {
    session_.memoryClear();
    emit memoryChanged({});
}
