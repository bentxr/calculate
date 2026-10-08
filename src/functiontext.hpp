#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QString>
#include <QStringList>


// The engine's English texts for a function, in the user's language.
QString functionTitle(const calculate_core::FunctionDescription& f);        // "Inverse sine"
QString functionDescription(const calculate_core::FunctionDescription& f);  // one sentence

// Every title, description and category of the engine's functions, marked for translation.
const QStringList& functionTextsForTranslation();
