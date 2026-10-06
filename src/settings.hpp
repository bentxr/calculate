#pragma once

#include <QPalette>
#include <QStringList>

// The application's language and colours. They change at once, while the app runs, and are not
// remembered: every start follows the system again.
namespace settings {

enum class Language { System, English, Spanish };
enum class Theme { System, Light, Dark };
enum class DecimalSeparator { Language, Point, Comma };

// Installs the translator for the language; open windows get QEvent::LanguageChange.
void setLanguage(Language language);
// Applies our light or dark palette; System follows the desktop's colour scheme, also when it changes.
void setTheme(Theme theme);
Theme theme();  // the one applied last
// The decimal separator typed and shown: Language follows the language in use (Spanish: a comma). With a comma,
// `;` separates arguments. The engine always reads a point.
void setDecimalSeparator(DecimalSeparator separator);
DecimalSeparator decimalSeparator();
bool decimalComma();
inline void setDecimalComma(bool on) { setDecimalSeparator(on ? DecimalSeparator::Comma : DecimalSeparator::Point); }

// Whether the result is worked out while the expression is typed (on at every start).
bool liveCalculation();
void setLiveCalculation(bool on);

// The theme at start: the system's on the desktop. In the browser, Dark: what browsers report of the
// system's scheme does not reach the app reliably.
#ifdef Q_OS_WASM
inline constexpr Theme startTheme = Theme::Dark;
#else
inline constexpr Theme startTheme = Theme::System;
#endif
QPalette palette(bool dark);

// The text in every language the app ships, the English source first: widgets are sized for the
// longest, so switching languages moves nothing.
QStringList inEveryLanguage(const char* context, const QString& source);

}  // namespace settings
