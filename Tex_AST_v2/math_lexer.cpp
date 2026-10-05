#include "math_lexer.h"
#include <cctype>

namespace math_ast {

Lexer::Lexer(const std::string& source) : source(source) {}

void Lexer::add(TokenKind kind, const std::string& text, size_t position) {
    tokens.push_back({kind, text, position});
}

void Lexer::error(size_t position, const std::string& message) {
    errors.push_back("Лексическая ошибка в позиции " + std::to_string(position) + ": " + message);
}

bool Lexer::starts_with(size_t position, const std::string& value) const {
    return source.compare(position, value.size(), value) == 0;
}

bool Lexer::is_supported_command(const std::string& name) const {
    return greek.find(name) != greek.end();
}

std::vector<Token> Lexer::run() {
    size_t pos = 0;

    while (pos < source.size()) {
        char c = source[pos];

        if (math_mode == MathMode::None) {
            if (starts_with(pos, "$$")) {
                add(TokenKind::MathOpen, "$$", pos);
                math_mode = MathMode::DoubleDollar;
                pos += 2;
            } else if (c == '$') {
                add(TokenKind::MathOpen, "$", pos);
                math_mode = MathMode::Dollar;
                ++pos;
            } else if (starts_with(pos, "\\[")) {
                add(TokenKind::MathOpen, "\\[", pos);
                math_mode = MathMode::Brackets;
                pos += 2;
            } else if (starts_with(pos, "\\(")) {
                add(TokenKind::MathOpen, "\\(", pos);
                math_mode = MathMode::Parentheses;
                pos += 2;
            } else {
                ++pos;
            }

            continue;
        }

        if (math_mode == MathMode::DoubleDollar &&
            starts_with(pos, "$$")) {
            add(TokenKind::MathClose, "$$", pos);
            math_mode = MathMode::None;
            pos += 2;
            continue;
        }

        if (math_mode == MathMode::Dollar &&
            c == '$') {
            add(TokenKind::MathClose, "$", pos);
            math_mode = MathMode::None;
            ++pos;
            continue;
        }

        if (math_mode == MathMode::Brackets &&
            starts_with(pos, "\\]")) {
            add(TokenKind::MathClose, "\\]", pos);
            math_mode = MathMode::None;
            pos += 2;
            continue;
        }

        if (math_mode == MathMode::Parentheses &&
            starts_with(pos, "\\)")) {
            add(TokenKind::MathClose, "\\)", pos);
            math_mode = MathMode::None;
            pos += 2;
            continue;
        }

        if (std::isspace(
                static_cast<unsigned char>(c))) {
            ++pos;
            continue;
        }

        if (std::isdigit(
                static_cast<unsigned char>(c)) ||
            (c == '.' &&
             pos + 1 < source.size() &&
             std::isdigit(
                 static_cast<unsigned char>(
                     source[pos + 1])))) {
            const size_t begin = pos;
            bool has_dot = false;

            while (pos < source.size()) {
                char current = source[pos];

                if (std::isdigit(
                        static_cast<unsigned char>(
                            current))) {
                    ++pos;
                } else if (current == '.' &&
                           !has_dot) {
                    has_dot = true;
                    ++pos;
                } else {
                    break;
                }
            }

            add(
                TokenKind::Number,
                source.substr(begin, pos - begin),
                begin
            );

            continue;
        }

        if (std::isalpha(
                static_cast<unsigned char>(c))) {
            const size_t begin = pos;

            while (pos < source.size() &&
                   std::isalnum(
                       static_cast<unsigned char>(
                           source[pos]))) {
                ++pos;
            }

            add(
                TokenKind::Identifier,
                source.substr(begin, pos - begin),
                begin
            );

            continue;
        }

        if (c == '\\') {
            const size_t begin = pos;
            ++pos;

            const size_t name_begin = pos;

            while (pos < source.size() &&
                   (std::isalpha(
                        static_cast<unsigned char>(
                            source[pos])) ||
                    std::isdigit(
                        static_cast<unsigned char>(
                            source[pos])))) {
                ++pos;
            }

            if (name_begin == pos) {
                error(
                    begin,
                    "после '\\' ожидалось имя символа"
                );
                continue;
            }

            const std::string name =
                source.substr(
                    name_begin,
                    pos - name_begin
                );

            /*
             * \left, \right и \quad пока не создают
             * отдельных AST-узлов.
             *
             * \left и \right будут проигнорированы,
             * а следующий разделитель обработается
             * обычным токеном.
             *
             * \quad также пока не отображается отдельно.
             */
            if (name == "left" ||
                name == "right" ||
                name == "quad") {
                continue;
            }

            /*
             * Специальные символы, которые parser уже
             * умеет представлять как GreekSymbol.
             */
            if (name == "Psi" ||
                name == "psi" ||
                name == "hbar" ||
                name == "partial" ||
                name == "nabla") {
                add(
                    TokenKind::Greek,
                    "\\" + name,
                    begin
                );
            } else {
                /*
                 * Остальные команды сохраняются как
                 * TokenKind::Command и обрабатываются
                 * parser-ом и renderer-ом.
                 */
                add(
                    TokenKind::Command,
                    "\\" + name,
                    begin
                );
            }

            continue;
        }

        TokenKind kind;
        bool known = true;

        switch (c) {
            case '+':
                kind = TokenKind::Plus;
                break;

            case '-':
                kind = TokenKind::Minus;
                break;

            case '*':
                kind = TokenKind::Star;
                break;

            case '/':
                kind = TokenKind::Slash;
                break;

            case '=':
                kind = TokenKind::Equal;
                break;

            case '^':
                kind = TokenKind::Caret;
                break;

            case '_':
                kind = TokenKind::Underscore;
                break;

            case '(':
                kind = TokenKind::LParen;
                break;

            case ')':
                kind = TokenKind::RParen;
                break;

            case '{':
                kind = TokenKind::LBrace;
                break;

            case '}':
                kind = TokenKind::RBrace;
                break;

            case '[':
                kind = TokenKind::LBracket;
                break;

            case ']':
                kind = TokenKind::RBracket;
                break;

            case '|':
                kind = TokenKind::Pipe;
                break;

            case ',':
                kind = TokenKind::Comma;
                break;

            case '<':
                kind = TokenKind::Less;
                break;

            case '>':
                kind = TokenKind::Greater;
                break;

            default:
                known = false;
                break;
        }

        if (known) {
            add(
                kind,
                std::string(1, c),
                pos
            );
            ++pos;
        } else {
            error(
                pos,
                "неизвестный символ '" +
                std::string(1, c) +
                "'"
            );
            ++pos;
        }
    }

    if (math_mode != MathMode::None) {
        error(
            source.size(),
            "формула не закрыта"
        );
    }

    tokens.push_back({
        TokenKind::End,
        "",
        source.size()
    });

    return tokens;
}


const std::vector<std::string>& Lexer::get_errors() const {
    return errors;
}

} // namespace math_ast

