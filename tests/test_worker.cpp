#include "worker.hpp"

#include <QSignalSpy>
#include <QThread>

#include "printers.hpp"

#include <gtest/gtest.h>

using namespace calculate_core;

TEST(Worker, EvaluatesAndKeepsTheSession) {
    Worker worker;
    QSignalSpy evaluated(&worker, &Worker::evaluated);
    worker.evaluate("1 + 2", Options{});
    worker.evaluate("Ans * 2", Options{});
    ASSERT_EQ(evaluated.count(), 2);
    const Result r = evaluated.at(1).at(1).value<Result>();
    EXPECT_EQ(r.value.digits, "6");
    EXPECT_EQ(evaluated.at(1).at(0).toString(), "Ans * 2");
}

TEST(Worker, Memory) {
    Worker worker;
    QSignalSpy changed(&worker, &Worker::memoryChanged);
    QSignalSpy failed(&worker, &Worker::memoryFailed);
    worker.memoryAdd();
    EXPECT_EQ(failed.count(), 1);
    worker.evaluate("5", Options{});
    worker.memoryAdd();
    worker.memoryClear();
    ASSERT_EQ(changed.count(), 2);
    EXPECT_EQ(changed.at(0).at(0).toString(), "5");
    EXPECT_EQ(changed.at(1).at(0).toString(), "");
}

TEST(Worker, RunsOnItsOwnThreadAndCanBeCancelled) {
    QThread thread;
    auto* worker = new Worker;
    worker->moveToThread(&thread);
    thread.start();
    QSignalSpy evaluated(worker, &Worker::evaluated);
    Options exact;
    exact.type = NumberType::Exact;
    QMetaObject::invokeMethod(worker, "evaluate", Qt::QueuedConnection, Q_ARG(QString, "200000!"),
                              Q_ARG(calculate_core::Options, exact));
    QThread::msleep(100);
    worker->cancelFlag() = true;
    ASSERT_TRUE(evaluated.wait(10000));
    const Result r = evaluated.at(0).at(1).value<Result>();
    ASSERT_TRUE(r.error);
    EXPECT_EQ(r.error->code, ErrorCode::Cancelled);
    thread.quit();
    thread.wait();
    delete worker;
}

TEST(Worker, APreviewLeavesAnsAlone) {
    Worker worker;
    QSignalSpy evaluated(&worker, &Worker::evaluated);
    QSignalSpy previewed(&worker, &Worker::previewed);
    worker.evaluate("2", Options{});
    worker.previewGeneration() = 1;
    worker.preview(1, "Ans * 3", Options{});
    ASSERT_EQ(previewed.count(), 1);
    EXPECT_EQ(previewed.at(0).at(0).toInt(), 1);
    EXPECT_EQ(previewed.at(0).at(2).value<Result>().value.digits, "6");
    worker.evaluate("Ans + 1", Options{});
    ASSERT_EQ(evaluated.count(), 2);
    EXPECT_EQ(evaluated.at(1).at(1).value<Result>().value.digits, "3");  // still 2 + 1
}

TEST(Worker, AStalePreviewIsSkipped) {
    Worker worker;
    QSignalSpy previewed(&worker, &Worker::previewed);
    worker.previewGeneration() = 2;
    worker.preview(1, "1 + 1", Options{});  // request 2 already replaced it
    EXPECT_EQ(previewed.count(), 0);
    worker.preview(2, "2 + 2", Options{});
    EXPECT_EQ(previewed.count(), 1);
}

TEST(Worker, ARunningPreviewCanBeCancelled) {
    QThread thread;
    auto* worker = new Worker;
    worker->moveToThread(&thread);
    thread.start();
    QSignalSpy previewed(worker, &Worker::previewed);
    Options exact;
    exact.type = NumberType::Exact;
    worker->previewGeneration() = 1;
    QMetaObject::invokeMethod(worker, "preview", Qt::QueuedConnection, Q_ARG(int, 1), Q_ARG(QString, "200000!"),
                              Q_ARG(calculate_core::Options, exact));
    QThread::msleep(100);
    worker->previewCancelFlag() = true;
    ASSERT_TRUE(previewed.wait(10000));
    const Result r = previewed.at(0).at(2).value<Result>();
    ASSERT_TRUE(r.error);
    EXPECT_EQ(r.error->code, ErrorCode::Cancelled);
    thread.quit();
    thread.wait();
    delete worker;
}

TEST(Worker, APreviewStartsWithItsCancelFlagDown) {
    Worker worker;
    QSignalSpy previewed(&worker, &Worker::previewed);
    worker.previewGeneration() = 1;
    worker.previewCancelFlag() = true;  // left raised by the window when it replaced an earlier request
    worker.preview(1, "1 + 1", Options{});
    ASSERT_EQ(previewed.count(), 1);
    EXPECT_FALSE(previewed.at(0).at(2).value<Result>().error);
}
