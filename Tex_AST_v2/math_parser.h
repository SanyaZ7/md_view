#ifndef MATH_PARSER_H
#define MATH_PARSER_H

#include "expression_types.h"
#include <vector>
#include <string>

namespace math_ast {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::vector<AstNodePtr> parse_document();
    const std::vector<std::string>& get_errors() const;

private:
    const std::vector<Token>& tokens;
    size_t current = 0;
    std::vector<std::string> errors;

    /*
     * Истина, пока разбирается внутренность модуля |...|.
     * Нужна, чтобы закрывающая вертикальная черта не принималась
     * за начало неявного умножения.
     */
    bool in_pipe_ = false;

    const Token& peek() const;
    bool check(TokenKind kind) const;
    bool accept(TokenKind kind);
    void error(const std::string& message);
    bool expect(TokenKind kind, const std::string& message);

    AstNodePtr parse_relation();
    AstNodePtr parse_addition();
    AstNodePtr parse_multiplication();
    AstNodePtr parse_unary();
    AstNodePtr parse_postfix();
    AstNodePtr parse_script();
    AstNodePtr parse_argument_list();
    AstNodePtr parse_atom();
};

} // namespace math_ast

#endif

