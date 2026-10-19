#ifndef HIGHLIGHT_BACKEND_H
#define HIGHLIGHT_BACKEND_H

#include "highlight.h"

#include <memory>

namespace highlight
{
namespace detail
{

/*
 * Реализация backend'а. Определяется ровно в одном месте:
 *
 *   - TreeSitterBackend.cpp — когда tree-sitter доступен;
 *   - NoopBackend.cpp       — когда tree-sitter отсутствует.
 *
 * Выбор файла выполняет Makefile, поэтому дублирования символов нет.
 */

/* nullptr, если backend не смог инициализироваться. */
std::unique_ptr<Highlighter> make_tree_sitter_highlighter(
    Language language);

std::unique_ptr<Highlighter> make_embedded_html_highlighter();

bool tree_sitter_backend_available();

} // namespace detail
} // namespace highlight

#endif // HIGHLIGHT_BACKEND_H
