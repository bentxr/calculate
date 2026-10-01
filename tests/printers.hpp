#pragma once

#include <QString>

#include <ostream>

// Lets GoogleTest print QStrings readably in failure messages.
inline void PrintTo(const QString& s, std::ostream* os) { *os << '"' << s.toStdString() << '"'; }
