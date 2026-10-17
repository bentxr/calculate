#pragma once

#include <QByteArray>
#include <QString>

// matplotlib's fonts, which the plots are drawn with, live in the program's resources (:/plotfonts). They are read
// (and uncompressed) once and handed to calculate-plot as bytes.
void installPlotFonts();
// installPlotFonts() the first time, from whichever thread asks first; nothing after.
void ensurePlotFonts();

// The bytes of an embedded font by its file name, or nullptr. They stay valid while the program runs.
const QByteArray* plotFontBytes(const QString& fileName);
