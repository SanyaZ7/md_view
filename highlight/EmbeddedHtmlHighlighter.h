#ifndef EMBEDDED_HTML_HIGHLIGHTER_H
#define EMBEDDED_HTML_HIGHLIGHTER_H

#include "highlight.h"

#include <memory>

namespace highlight
{
namespace detail
{

std::unique_ptr<Highlighter> make_embedded_html_highlighter();

} // namespace detail
} // namespace highlight

#endif // EMBEDDED_HTML_HIGHLIGHTER_H
