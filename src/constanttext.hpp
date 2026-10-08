#pragma once

#include <QString>
#include <QStringList>

// The engine's English title of a named value (G, phi, dozen…), in the user's language; empty for an unknown name.
QString constantTitle(const QString& name);

// Every title of the engine's named values, marked for translation.
const QStringList& constantTextsForTranslation();
