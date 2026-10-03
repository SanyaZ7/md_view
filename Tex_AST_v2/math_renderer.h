#ifndef MATH_RENDERER_H
#define MATH_RENDERER_H

#include <string>

namespace math_ast {

std::string render_inline_formula(
    const std::string& source
);

std::string render_block_formula(
    const std::string& source
);

} // namespace math_ast

#endif // MATH_RENDERER_H

