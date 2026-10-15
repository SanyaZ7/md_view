#include "backend.h"

/*
 * Заглушка, собираемая вместо TreeSitterBackend.cpp, когда
 * tree-sitter недоступен. libhighlight.so остаётся полностью
 * работоспособной, но подсветка отключена.
 */
namespace highlight
{
namespace detail
{

std::unique_ptr<Highlighter> make_tree_sitter_highlighter(Language /*language*/)
{
    return nullptr;
}

bool tree_sitter_backend_available()
{
    return false;
}

} // namespace detail
} // namespace highlight
