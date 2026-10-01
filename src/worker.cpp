#include "worker.hpp"

void Worker::evaluate(const QString& expression, const calculate_core::Options& options) {
    cancel_ = false;
    calculate_core::Options o = options;
    o.cancel = &cancel_;
    emit evaluated(expression, session_.evaluate(expression.toStdString(), o));
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
