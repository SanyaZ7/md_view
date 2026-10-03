#ifndef MATH_LEXER_H
#define MATH_LEXER_H

#include "expression_types.h"
#include <string>
#include <vector>
#include <unordered_set>

namespace math_ast {

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> run();
    const std::vector<std::string>& get_errors() const;

private:
    enum class MathMode {
        None,
        Dollar,
        DoubleDollar,
        Parentheses,
        Brackets
    };

    const std::string& source;
    std::vector<Token> tokens;
    std::vector<std::string> errors;
    MathMode math_mode = MathMode::None;
    const std::unordered_set<std::string> greek = {
    "alpha", "beta", "gamma", "delta", "pi", "theta", "lambda", "sigma", "omega",
    "Gamma", "Delta", "Theta", "Lambda", "Sigma", "Omega",
    "Psi", "psi", "hbar",  // Psi, psi - греческие; hbar - гравис (Planck constant)
    // Убираем partial и nabla, они будем обрабатывать как Commands
    "hat", "mathbf", "frac", "partial", "nabla"
};

    void add(TokenKind kind, const std::string& text, size_t position);
    void error(size_t position, const std::string& message);
    bool starts_with(size_t position, const std::string& value) const;
    bool is_supported_command(const std::string& name) const;
};

} // namespace math_ast

#endif

