#ifndef HIGHLIGHT_HTML_H
#define HIGHLIGHT_HTML_H

#include "highlight.h"

#include <QString>

#include <string>

/*
 * Подсвеченный HTML для блока кода в preview.
 *
 * Возвращает фрагменты кода, обёрнутые в <span> с цветами из
 * ColorScheme. Если язык не поддерживается или подсветка недоступна,
 * результат эквивалентен обычному экранированию.
 */
QString highlight_code_html(
    const std::string& code,
    highlight::Language language);

#endif // HIGHLIGHT_HTML_H
