#ifndef EXPRESSION_TYPES_H
#define EXPRESSION_TYPES_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>

namespace math_ast {

enum class TokenKind {
    End,
    MathOpen,
    MathClose,
    Number,
    Identifier,
    Greek,
    Command,
    Plus,
    Minus,
    Star,
    Slash,
    Equal,
    Caret,
    Underscore,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Pipe,
    Comma,
    Less,
    Greater
};


struct Token {
    TokenKind kind;
    std::string text;
    size_t position;
};

inline const char* token_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::End: return "конец файла";
        case TokenKind::MathOpen: return "начало формулы";
        case TokenKind::MathClose: return "конец формулы";
        case TokenKind::Number: return "число";
        case TokenKind::Identifier: return "идентификатор";
        case TokenKind::Greek: return "греческий символ";
        case TokenKind::Command: return "команда LaTeX";
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Equal: return "=";
        case TokenKind::Caret: return "^";
        case TokenKind::Underscore: return "_";
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::Pipe: return "|";
        case TokenKind::Comma: return ",";
        case TokenKind::Less: return "<";
        case TokenKind::Greater: return ">";
        default: return "неизвестный токен";
    }
}


struct AstNode {
    virtual ~AstNode() = default;
    virtual void print(std::ostream& out) const = 0;
    virtual std::string to_string() const {
        std::ostringstream oss;
        print(oss);
        return oss.str();
    }
};

using AstNodePtr = std::unique_ptr<AstNode>;

struct Number final : AstNode {
    std::string value;
    explicit Number(std::string value) : value(std::move(value)) {}
    void print(std::ostream& out) const override { out << value; }
};

struct Identifier final : AstNode {
    std::string name;
    explicit Identifier(std::string name) : name(std::move(name)) {}
    void print(std::ostream& out) const override { out << name; }
};

struct GreekSymbol final : AstNode {
    std::string name;
    explicit GreekSymbol(std::string name) : name(std::move(name)) {}
    void print(std::ostream& out) const override { out << '\\' << name; }
};

struct CommandNode final : AstNode {
    std::string name;
    explicit CommandNode(std::string name) : name(std::move(name)) {}
    void print(std::ostream& out) const override { out << '\\' << name; }
};

struct CommandWithArgument final : AstNode {
    std::string name;
    AstNodePtr argument;

    CommandWithArgument(
        std::string name,
        AstNodePtr argument
    )
        : name(std::move(name)),
          argument(std::move(argument)) {
    }

    void print(std::ostream& out) const override {
        out << '\\' << name << '{';

        if (argument) {
            argument->print(out);
        }

        out << '}';
    }
};

struct UnaryOperation final : AstNode {
    char operation;
    AstNodePtr operand;
    UnaryOperation(char operation, AstNodePtr operand)
        : operation(operation), operand(std::move(operand)) {}
    void print(std::ostream& out) const override {
        out << '(' << operation;
        operand->print(out);
        out << ')';
    }
};

struct BinaryOperation final : AstNode {
    std::string operation;
    AstNodePtr left;
    AstNodePtr right;
    BinaryOperation(std::string operation, AstNodePtr left, AstNodePtr right)
        : operation(std::move(operation)), left(std::move(left)), right(std::move(right)) {}
    void print(std::ostream& out) const override {
        out << '(';
        left->print(out);
        out << ' ' << operation << ' ';
        right->print(out);
        out << ')';
    }
};

struct Power final : AstNode {
    AstNodePtr base;
    AstNodePtr exponent;
    Power(AstNodePtr base, AstNodePtr exponent) : base(std::move(base)), exponent(std::move(exponent)) {}
    void print(std::ostream& out) const override {
        out << '(';
        base->print(out);
        out << '^';
        exponent->print(out);
        out << ')';
    }
};

struct Subscript final : AstNode {
    AstNodePtr base;
    AstNodePtr index;
    Subscript(AstNodePtr base, AstNodePtr index) : base(std::move(base)), index(std::move(index)) {}
    void print(std::ostream& out) const override {
        out << '(';
        base->print(out);
        out << '_';
        index->print(out);
        out << ')';
    }
};

} // namespace math_ast

#endif

