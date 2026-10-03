#include "math_parser.h"

namespace math_ast {

Parser::Parser(
    const std::vector<Token>& tokens
)
    : tokens(tokens)
{
}

const Token& Parser::peek() const
{
    return tokens[current];
}

bool Parser::check(TokenKind kind) const
{
    return peek().kind == kind;
}

bool Parser::accept(TokenKind kind)
{
    if (!check(kind)) {
        return false;
    }

    ++current;
    return true;
}

void Parser::error(
    const std::string& message
)
{
    errors.push_back(
        "Синтаксическая ошибка в позиции " +
        std::to_string(peek().position) +
        ": " +
        message
    );
}

bool Parser::expect(
    TokenKind kind,
    const std::string& message
)
{
    if (accept(kind)) {
        return true;
    }

    error(
        message +
        ", получено: " +
        token_name(peek().kind)
    );

    return false;
}

/*
 * relation:
 *
 *   a = b
 *   a = b, c = d
 *
 * Запятая используется как оператор объединения нескольких
 * частей одной формулы.
 */
AstNodePtr Parser::parse_relation()
{
    AstNodePtr result =
        parse_addition();

    if (!result) {
        return nullptr;
    }

    while (accept(TokenKind::Equal)) {
        AstNodePtr right =
            parse_addition();

        if (!right) {
            return nullptr;
        }

        result =
            std::make_unique<BinaryOperation>(
                "=",
                std::move(result),
                std::move(right)
            );
    }

    while (accept(TokenKind::Comma)) {
        AstNodePtr right =
            parse_relation();

        if (!right) {
            return nullptr;
        }

        result =
            std::make_unique<BinaryOperation>(
                ",",
                std::move(result),
                std::move(right)
            );
    }

    return result;
}

/*
 * addition:
 *
 *   a + b
 *   a - b
 */
AstNodePtr Parser::parse_addition()
{
    AstNodePtr result =
        parse_multiplication();

    if (!result) {
        return nullptr;
    }

    while (check(TokenKind::Plus) ||
           check(TokenKind::Minus)) {
        const std::string operation =
            peek().text;

        ++current;

        AstNodePtr right =
            parse_multiplication();

        if (!right) {
            return nullptr;
        }

        result =
            std::make_unique<BinaryOperation>(
                operation,
                std::move(result),
                std::move(right)
            );
    }

    return result;
}

/*
 * multiplication:
 *
 *   a * b
 *   a / b
 *   2m
 *   \gamma m c^2
 *   V(\mathbf{r})
 */
AstNodePtr Parser::parse_multiplication()
{
    AstNodePtr result =
        parse_unary();

    if (!result) {
        return nullptr;
    }

    while (true) {
        /*
         * Явное умножение или обычное деление.
         *
         * Важно: "/" остаётся операцией "/".
         * Только \frac создаёт операцию "frac".
         */
        if (check(TokenKind::Star) ||
            check(TokenKind::Slash)) {
            const std::string operation =
                peek().text;

            ++current;

            AstNodePtr right =
                parse_unary();

            if (!right) {
                return nullptr;
            }

            result =
                std::make_unique<BinaryOperation>(
                    operation,
                    std::move(result),
                    std::move(right)
                );

            continue;
        }

        /*
         * Неявное умножение.
         */
        const bool starts_implicit_factor =
            check(TokenKind::Number) ||
            check(TokenKind::Identifier) ||
            check(TokenKind::Greek) ||
            check(TokenKind::Command) ||
            check(TokenKind::LParen) ||
            check(TokenKind::LBrace) ||
            check(TokenKind::LBracket) ||
            (check(TokenKind::Pipe) && !in_pipe_);

        if (!starts_implicit_factor) {
            break;
        }

        AstNodePtr right =
            parse_unary();

        if (!right) {
            return nullptr;
        }

        result =
            std::make_unique<BinaryOperation>(
                "*",
                std::move(result),
                std::move(right)
            );
    }

    return result;
}

/*
 * Разбор одного атома.
 */
AstNodePtr Parser::parse_atom()
{
    /*
     * Число.
     */
    if (check(TokenKind::Number)) {
        const std::string value =
            peek().text;

        ++current;

        return std::make_unique<Number>(
            value
        );
    }

    /*
     * Обычный идентификатор.
     */
    if (check(TokenKind::Identifier)) {
        const std::string name =
            peek().text;

        ++current;

        return std::make_unique<Identifier>(
            name
        );
    }

    /*
     * Греческий или специальный символ,
     * который lexer классифицировал как Greek.
     */
    if (check(TokenKind::Greek)) {
        std::string name =
            peek().text;

        ++current;

        if (!name.empty() &&
            name.front() == '\\') {
            name.erase(0, 1);
        }

        return std::make_unique<GreekSymbol>(
            name
        );
    }

    /*
     * Команда LaTeX.
     */
    if (check(TokenKind::Command)) {
        std::string name =
            peek().text;

        ++current;

        /*
         * В token.text команда хранится как "\gamma".
         * В AST имя храним без обратного слеша.
         */
        if (!name.empty() &&
            name.front() == '\\') {
            name.erase(0, 1);
        }

        /*
         * \frac имеет два аргумента:
         *
         *   \frac{a}{b}
         *
         * Важно: операция называется "frac", а не "/".
         * Благодаря этому обычное a / b и \frac{a}{b}
         * рендерятся по-разному.
         */
        if (name == "frac") {
            if (!expect(
                    TokenKind::LBrace,
                    "после команды \\frac "
                    "ожидался открывающий аргумент"
                )) {
                return nullptr;
            }

            AstNodePtr numerator =
                parse_relation();

            if (!numerator) {
                return nullptr;
            }

            if (!expect(
                    TokenKind::RBrace,
                    "ожидалась закрывающая скобка "
                    "числителя"
                )) {
                return nullptr;
            }

            if (!expect(
                    TokenKind::LBrace,
                    "после числителя \\frac "
                    "ожидался знаменатель"
                )) {
                return nullptr;
            }

            AstNodePtr denominator =
                parse_relation();

            if (!denominator) {
                return nullptr;
            }

            if (!expect(
                    TokenKind::RBrace,
                    "ожидалась закрывающая скобка "
                    "знаменателя"
                )) {
                return nullptr;
            }

            return std::make_unique<BinaryOperation>(
                "frac",
                std::move(numerator),
                std::move(denominator)
            );
        }

        /*
         * Команды с одним аргументом:
         *
         *   \hat{H}
         *   \mathbf{r}
         *   \sqrt{x}
         *   \mathcal{L}
         */
        if (name == "hat" ||
            name == "mathbf" ||
            name == "sqrt" ||
            name == "mathcal") {
            if (!expect(
                    TokenKind::LBrace,
                    "после команды \\" +
                    name +
                    " ожидался открывающий аргумент"
                )) {
                return nullptr;
            }

            AstNodePtr argument =
                parse_relation();

            if (!argument) {
                return nullptr;
            }

            if (!expect(
                    TokenKind::RBrace,
                    "ожидалась закрывающая скобка "
                    "аргумента команды \\" +
                    name
                )) {
                return nullptr;
            }

            return std::make_unique<CommandWithArgument>(
                name,
                std::move(argument)
            );
        }

        /*
         * Если неизвестная команда имеет аргумент в фигурных
         * скобках, разбираем аргумент, но сохраняем имя команды.
         *
         * Это лучше, чем молча возвращать только argument:
         *
         *   \unknown{x}
         *
         * не превращается просто в x.
         */
        if (check(TokenKind::LBrace)) {
            ++current;

            AstNodePtr argument =
                parse_relation();

            if (!argument) {
                return nullptr;
            }

            if (!expect(
                    TokenKind::RBrace,
                    "ожидалась закрывающая фигурная "
                    "скобка аргумента команды"
                )) {
                return nullptr;
            }

            return std::make_unique<CommandWithArgument>(
                name,
                std::move(argument)
            );
        }

        /*
         * Команда без аргумента.
         *
         * Сюда попадают:
         *
         *   \gamma
         *   \varepsilon
         *   \mu
         *   \cdot
         *   \hbar
         *   \partial
         *   \nabla
         */
        return std::make_unique<CommandNode>(
            name
        );
    }

    /*
     * Круглые скобки.
     *
     * Содержимое скобок разбирается как выражение.
     */
    if (accept(TokenKind::LParen)) {
        AstNodePtr result =
            parse_argument_list();

        if (!result) {
            return nullptr;
        }

        if (!expect(
                TokenKind::RParen,
                "ожидалась закрывающая круглая скобка"
            )) {
            return nullptr;
        }

        return result;
    }

    /*
     * Квадратные скобки.
     *
     * В lexer команды \left и \right пропускаются,
     * поэтому:
     *
     *   \left[ x \right]
     *
     * поступает сюда как:
     *
     *   [ x ]
     */
    if (accept(TokenKind::LBracket)) {
        AstNodePtr result =
            parse_relation();

        if (!result) {
            return nullptr;
        }

        if (!expect(
                TokenKind::RBracket,
                "ожидалась закрывающая квадратная скобка"
            )) {
            return nullptr;
        }

        return result;
    }

    /*
     * Модуль:
     *
     *   |\psi|^2
     */
    if (accept(TokenKind::Pipe)) {
        const bool previous_in_pipe =
            in_pipe_;

        in_pipe_ = true;

        AstNodePtr result =
            parse_relation();

        if (!result) {
            in_pipe_ = previous_in_pipe;
            return nullptr;
        }

        if (!expect(
                TokenKind::Pipe,
                "ожидалась закрывающая "
                "вертикальная черта модуля"
            )) {
            in_pipe_ = previous_in_pipe;
            return nullptr;
        }

        in_pipe_ = previous_in_pipe;

        /*
         * На текущем этапе модуль хранится как Identifier.
         * Это сохраняет существующую модель AST.
         */
        return std::make_unique<Identifier>(
            "|" +
            result->to_string() +
            "|"
        );
    }

    /*
     * Фигурная группа:
     *
     *   {x + y}
     *
     * или аргумент, если он дошёл сюда напрямую.
     */
    if (accept(TokenKind::LBrace)) {
        AstNodePtr result =
            parse_relation();

        if (!result) {
            return nullptr;
        }

        if (!expect(
                TokenKind::RBrace,
                "ожидалась закрывающая фигурная скобка"
            )) {
            return nullptr;
        }

        return result;
    }

    error(
        "ожидалось число, имя, греческий символ, "
        "команда LaTeX или скобочная группа"
    );

    return nullptr;
}

/*
 * Унарные операции:
 *
 *   -x
 *   +x
 */
AstNodePtr Parser::parse_unary()
{
    if (check(TokenKind::Plus) ||
        check(TokenKind::Minus)) {
        const char operation =
            peek().text[0];

        ++current;

        AstNodePtr operand =
            parse_unary();

        if (!operand) {
            return nullptr;
        }

        return std::make_unique<UnaryOperation>(
            operation,
            std::move(operand)
        );
    }

    return parse_postfix();
}

/*
 * Степени и индексы:
 *
 *   x^2
 *   x_i
 *   x_{ij}
 */
AstNodePtr Parser::parse_postfix()
{
    AstNodePtr result =
        parse_atom();

    if (!result) {
        return nullptr;
    }

    while (check(TokenKind::Caret) ||
           check(TokenKind::Underscore)) {
        const TokenKind operation =
            peek().kind;

        ++current;

        AstNodePtr script =
            parse_script();

        if (!script) {
            return nullptr;
        }

        if (operation == TokenKind::Caret) {
            result =
                std::make_unique<Power>(
                    std::move(result),
                    std::move(script)
                );
        } else {
            result =
                std::make_unique<Subscript>(
                    std::move(result),
                    std::move(script)
                );
        }
    }

    return result;
}

/*
 * Аргумент степени или индекса.
 *
 *   x^2
 *   x^{2+1}
 */
AstNodePtr Parser::parse_script()
{
    if (accept(TokenKind::LBrace)) {
        AstNodePtr result =
            parse_relation();

        if (!result) {
            return nullptr;
        }

        if (!expect(
                TokenKind::RBrace,
                "ожидалась закрывающая скобка "
                "аргумента степени или индекса"
            )) {
            return nullptr;
        }

        return result;
    }

    return parse_atom();
}

/*
 * Содержимое круглых скобок.
 *
 * Например:
 *
 *   (x + y)
 *   \Psi(\mathbf{r}, t)
 *
 * На текущем уровне AST аргументы функции объединяются
 * неявным умножением.
 */
AstNodePtr Parser::parse_argument_list()
{
    AstNodePtr result;

    while (!check(TokenKind::RParen) &&
           !check(TokenKind::End)) {
        AstNodePtr argument =
            parse_relation();

        if (!argument) {
            return nullptr;
        }

        if (!result) {
            result =
                std::move(argument);
        } else {
            result =
                std::make_unique<BinaryOperation>(
                    "*",
                    std::move(result),
                    std::move(argument)
                );
        }

        /*
         * Запятая обычно уже обрабатывается внутри
         * parse_relation(). Если она всё ещё осталась,
         * пропускаем её здесь.
         */
        if (accept(TokenKind::Comma)) {
            continue;
        }

        if (check(TokenKind::RParen)) {
            break;
        }

        /*
         * Если после аргумента встретился другой токен,
         * parse_relation() мог разобрать его как часть
         * выражения. Если это не так, цикл завершится
         * последующей ошибкой в parse_document().
         */
    }

    if (!result) {
        /*
         * Пустые круглые скобки не являются полноценным
         * математическим выражением. Но возвращаем 1,
         * чтобы сохранить прежнее поведение parser-а.
         */
        return std::make_unique<Number>("1");
    }

    return result;
}

/*
 * Документ может содержать несколько математических
 * фрагментов, каждый со своими delimiters.
 */
std::vector<AstNodePtr> Parser::parse_document()
{
    std::vector<AstNodePtr> formulas;

    while (!check(TokenKind::End)) {
        if (!expect(
                TokenKind::MathOpen,
                "ожидалось начало "
                "математической формулы"
            )) {
            return {};
        }

        AstNodePtr formula =
            parse_relation();

        if (!formula) {
            return {};
        }

        if (!expect(
                TokenKind::MathClose,
                "ожидался конец "
                "математической формулы"
            )) {
            return {};
        }

        formulas.push_back(
            std::move(formula)
        );
    }

    if (formulas.empty()) {
        errors.push_back(
            "В документе не найдено ни одной формулы"
        );
    }

    return formulas;
}

const std::vector<std::string>&
Parser::get_errors() const
{
    return errors;
}

} // namespace math_ast

