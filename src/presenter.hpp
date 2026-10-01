#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QList>
#include <QPair>
#include <QString>

// Pure translation from the engine's results to display text; the numeric logic stays in the engine.
namespace view {

QString typeLabel(const calculate_core::TypeInfo& type);
// Noise digits follow a bar and take noiseColor, so colour is never the only cue.
QString valueHtml(const calculate_core::Result& result, const QString& noiseColor);
QString errorLine(const calculate_core::Result& result);
QString whyLine(const calculate_core::Result& result, const calculate_core::TypeInfo& type);
QString verdict(const QString& conditionNumber);
QList<QPair<QString, QString>> details(const calculate_core::Result& result);
QString errorText(const calculate_core::Error& error, const QString& expression);
QString statisticsExpression(const QString& function, const QString& values);

}  // namespace view
