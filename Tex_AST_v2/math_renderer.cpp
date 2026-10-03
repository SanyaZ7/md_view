#include "math_renderer.h"

#include "math_lexer.h"
#include "math_parser.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace math_ast {

std::string render_inline_formula(
    const std::string& source
)
{
    const auto escape_html =
        [](const std::string& value) {
            std::string result;
            result.reserve(value.size());

            for (char symbol : value) {
                switch (symbol) {
                    case '&':
                        result += "&amp;";
                        break;
                    case '<':
                        result += "&lt;";
                        break;
                    case '>':
                        result += "&gt;";
                        break;
                    case '"':
                        result += "&quot;";
                        break;
                    case '\'':
                        result += "&#39;";
                        break;
                    default:
                        result.push_back(symbol);
                        break;
                }
            }

            return result;
        };

    const auto join_errors =
        [&escape_html](const std::vector<std::string>& errors) {
            std::string result;

            for (std::size_t i = 0; i < errors.size(); ++i) {
                if (i != 0) {
                    result += "; ";
                }

                result += errors[i];
            }

            return escape_html(result);
        };

    const auto error_html =
        [&escape_html, &join_errors](
            const std::string& original,
            const std::vector<std::string>& errors
        ) {
            std::string title =
                join_errors(errors);

            if (title.empty()) {
                title = "Ошибка математической формулы";
            }

            return std::string(
                       "<span class=\"math-error\" "
                       "title=\""
                   ) +
                   title +
                   "\">" +
                   escape_html(original) +
                   "</span>";
        };

    /*
     * Символы, которые lexer/parser передают как GreekSymbol
     * или которые дополнительно используются renderer-ом
     * для команд LaTeX.
     */
    const auto greek_symbol =
        [](const std::string& name) {
            if (name == "alpha") {
                return std::string("α");
            }

            if (name == "beta") {
                return std::string("β");
            }

            if (name == "gamma") {
                return std::string("γ");
            }

            if (name == "delta") {
                return std::string("δ");
            }

            if (name == "pi") {
                return std::string("π");
            }

            if (name == "theta") {
                return std::string("θ");
            }

            if (name == "lambda") {
                return std::string("λ");
            }

            if (name == "sigma") {
                return std::string("σ");
            }

            if (name == "omega") {
                return std::string("ω");
            }

            if (name == "Gamma") {
                return std::string("Γ");
            }

            if (name == "Delta") {
                return std::string("Δ");
            }

            if (name == "Theta") {
                return std::string("Θ");
            }

            if (name == "Lambda") {
                return std::string("Λ");
            }

            if (name == "Sigma") {
                return std::string("Σ");
            }

            if (name == "Omega") {
                return std::string("Ω");
            }

            if (name == "Psi") {
                return std::string("Ψ");
            }

            if (name == "psi") {
                return std::string("ψ");
            }

            if (name == "hbar") {
                return std::string("ℏ");
            }

            if (name == "partial") {
                return std::string("∂");
            }

            if (name == "nabla") {
                return std::string("∇");
            }

            if (name == "varepsilon") {
                return std::string("ε");
            }

            if (name == "mu") {
                return std::string("μ");
            }

            return std::string();
        };

    /*
     * Команды, которые являются одиночными символами
     * и не имеют аргументов.
     */
    const auto command_symbol =
        [&greek_symbol](const std::string& name) {
            if (name == "cdot") {
                return std::string("·");
            }

            if (name == "times") {
                return std::string("×");
            }

            if (name == "pm") {
                return std::string("±");
            }

            if (name == "infty") {
                return std::string("∞");
            }

            const std::string symbol =
                greek_symbol(name);

            return symbol;
        };

    std::function<std::string(const AstNode&)> render_node;

    render_node =
        [&](
            const AstNode& node
        ) -> std::string {
            /*
             * Число.
             */
            if (const Number* number =
                    dynamic_cast<const Number*>(&node)) {
                return escape_html(number->value);
            }

            /*
             * Обычный идентификатор.
             *
             * Теперь \hat и \mathbf сюда не попадают:
             * для них parser создаёт CommandWithArgument.
             */
            if (const Identifier* identifier =
                    dynamic_cast<const Identifier*>(&node)) {
                return std::string("<i>") +
                       escape_html(identifier->name) +
                       "</i>";
            }

            /*
             * Греческий символ или специальный символ,
             * представленный узлом GreekSymbol.
             */
            if (const GreekSymbol* greek =
                    dynamic_cast<const GreekSymbol*>(&node)) {
                const std::string symbol =
                    greek_symbol(greek->name);

                if (!symbol.empty()) {
                    return escape_html(symbol);
                }

                return escape_html(
                    std::string("\\") +
                    greek->name
                );
            }

            /*
             * Команда без аргументов:
             *
             *   \gamma
             *   \varepsilon
             *   \cdot
             *   \mu
             */
            if (const CommandNode* command =
                    dynamic_cast<const CommandNode*>(&node)) {
                const std::string symbol =
                    command_symbol(command->name);

                if (!symbol.empty()) {
                    return escape_html(symbol);
                }

                return escape_html(
                    std::string("\\") +
                    command->name
                );
            }

            /*
             * Команда с одним аргументом:
             *
             *   \hat{H}
             *   \mathbf{r}
             *   \sqrt{x}
             *   \mathcal{L}
             */
            if (const CommandWithArgument* command =
                    dynamic_cast<const CommandWithArgument*>(&node)) {
                const std::string& name =
                    command->name;

                const std::string argument =
                    command->argument
                        ? render_node(*command->argument)
                        : std::string();

               if (name == "hat") {
    /*
     * Используем отдельный верхний слой с символом шапки.
     * Это надёжнее, чем CSS-класс без явного стиля:
     *
     *   \hat{H}
     */
    return std::string(
               "<span style=\"display:inline-block;"
               "position:relative;"
               "padding-top:3px;\">"
               "<span style=\"position:absolute;"
               "left:0;"
               "right:0;"
               "top:-4px;"
               "text-align:center;"
               "font-size:80%;\">"
               "&#770;"
               "</span>"
           ) +
           argument +
           "</span>";
}

if (name == "mathbf") {
    /*
     * Сохраняем математический курсив и добавляем жирность.
     */
    return std::string(
               "<span style=\"font-weight:bold;"
               "font-style:italic;\">"
           ) +
           argument +
           "</span>";
}

                if (name == "sqrt") {
                    return std::string(
                               "<span class=\"math-sqrt\">"
                               "<span class=\"math-sqrt-symbol\">"
                               "√"
                               "</span>"
                               "<span class=\"math-sqrt-content\">"
                           ) +
                           argument +
                           "</span>"
                           "</span>";
                }

                if (name == "mathcal") {
                    return std::string(
                               "<span class=\"math-cal\">"
                           ) +
                           argument +
                           "</span>";
                }

                /*
                 * Защита на случай появления новой команды,
                 * которую parser уже научился хранить,
                 * но renderer ещё не умеет рисовать.
                 */
                return escape_html(
                           std::string("\\") +
                           name +
                           "{"
                       ) +
                       argument +
                       "}";
            }

            /*
             * Унарный плюс или минус.
             */
            if (const UnaryOperation* unary =
                    dynamic_cast<const UnaryOperation*>(&node)) {
                return escape_html(
                           std::string(
                               1,
                               unary->operation
                           )
                       ) +
                       render_node(*unary->operand);
            }

            /*
             * Бинарные операции.
             */
            if (const BinaryOperation* binary =
                    dynamic_cast<const BinaryOperation*>(&node)) {
                const std::string left =
                    render_node(*binary->left);

                const std::string right =
                    render_node(*binary->right);

                /*
                 * ВАЖНО:
                 *
                 * "frac" создаётся parser-ом только для:
                 *
                 *   \frac{a}{b}
                 *
                 * Обычный оператор "/" сюда не попадает
                 * и отображается как обычный inline-оператор.
                 */
                if (binary->operation == "frac") {
                    return std::string(
                               "<table class=\"math-frac\" "
                               "cellspacing=\"0\" "
                               "cellpadding=\"0\" "
                               "style=\"vertical-align:middle;"
                               "border-collapse:collapse;\">"
                               "<tr>"
                               "<td align=\"center\" "
                               "style=\"border-bottom:"
                               "1px solid #333333;"
                               "padding:0 3px;\">"
                           ) +
                           left +
                           "</td>"
                           "</tr>"
                           "<tr>"
                           "<td align=\"center\" "
                           "style=\"padding:0 3px;\">"
                           +
                           right +
                           "</td>"
                           "</tr>"
                           "</table>";
                }

                /*
                 * Неявное умножение.
                 */
                if (binary->operation == "*") {
                    return left +
                           "<span "
                           "class=\"math-mul\">"
                           "&#8239;"
                           "</span>" +
                           right;
                }

                /*
                 * Сюда попадают:
                 *
                 *   +
                 *   -
                 *   /
                 *   =
                 *   ,
                 *
                 * Обычное деление теперь отображается
                 * горизонтально, а не таблицей.
                 */
                return left +
                       "<span class=\"math-op\"> " +
                       escape_html(binary->operation) +
                       " </span>" +
                       right;
            }

            /*
             * Степень.
             */
            if (const Power* power =
                    dynamic_cast<const Power*>(&node)) {
                return render_node(*power->base) +
                       "<sup>" +
                       render_node(*power->exponent) +
                       "</sup>";
            }

            /*
             * Нижний индекс.
             */
            if (const Subscript* subscript =
                    dynamic_cast<const Subscript*>(&node)) {
                return render_node(*subscript->base) +
                       "<sub>" +
                       render_node(*subscript->index) +
                       "</sub>";
            }

            return escape_html(
                node.to_string()
            );
        };

    /*
     * Lexer ожидает математические delimiters.
     * Поэтому исходную inline-формулу оборачиваем
     * в одиночные долларовые delimiters.
     */
    const std::string wrapped =
        std::string("$") +
        source +
        "$";

    Lexer lexer(wrapped);

    const std::vector<Token> tokens =
        lexer.run();

    Parser parser(tokens);

    const std::vector<AstNodePtr> formulas =
        parser.parse_document();

    std::vector<std::string> errors =
        lexer.get_errors();

    const std::vector<std::string>& parser_errors =
        parser.get_errors();

    errors.insert(
        errors.end(),
        parser_errors.begin(),
        parser_errors.end()
    );

    if (!errors.empty() ||
        formulas.empty()) {
        return error_html(
            source,
            errors
        );
    }

    std::string rendered;

    for (const AstNodePtr& formula : formulas) {
        if (!formula) {
            return error_html(
                source,
                errors
            );
        }

        rendered += render_node(*formula);
    }

    return std::string(
               "<span class=\"math-inline\" "
               "style=\"white-space:nowrap;\">"
           ) +
           rendered +
           "</span>";
}

std::string render_block_formula(
    const std::string& source
)
{
    const auto escape_html =
        [](const std::string& value) {
            std::string result;
            result.reserve(value.size());

            for (char symbol : value) {
                switch (symbol) {
                    case '&':
                        result += "&amp;";
                        break;
                    case '<':
                        result += "&lt;";
                        break;
                    case '>':
                        result += "&gt;";
                        break;
                    case '"':
                        result += "&quot;";
                        break;
                    case '\'':
                        result += "&#39;";
                        break;
                    default:
                        result.push_back(symbol);
                        break;
                }
            }

            return result;
        };

    const auto error_html =
        [&escape_html](
            const std::string& original,
            const std::vector<std::string>& errors
        ) {
            std::string title;

            for (std::size_t i = 0; i < errors.size(); ++i) {
                if (i != 0) {
                    title += "; ";
                }

                title += errors[i];
            }

            if (title.empty()) {
                title = "Ошибка математической формулы";
            }

            return std::string(
                       "<div class=\"math-error\" "
                       "title=\""
                   ) +
                   escape_html(title) +
                   "\" "
                   "style=\"margin:12px 0;\">"
                   +
                   escape_html(original) +
                   "</div>";
        };

    const auto trim =
        [](const std::string& value) {
            std::size_t begin = 0;
            std::size_t end = value.size();

            while (begin < end &&
                   (value[begin] == ' ' ||
                    value[begin] == '\t' ||
                    value[begin] == '\r' ||
                    value[begin] == '\n')) {
                ++begin;
            }

            while (end > begin &&
                   (value[end - 1] == ' ' ||
                    value[end - 1] == '\t' ||
                    value[end - 1] == '\r' ||
                    value[end - 1] == '\n')) {
                --end;
            }

            return value.substr(begin, end - begin);
        };

    std::string formula = trim(source);

    /*
     * Поддержка содержимого вида:

           $$
           x^2 + y^2
           $$

       или:

           \[
           x^2 + y^2
           \]
    */
    if (formula.size() >= 4 &&
        formula.compare(0, 2, "$$") == 0 &&
        formula.compare(formula.size() - 2, 2, "$$") == 0) {
        formula = formula.substr(
            2,
            formula.size() - 4
        );
    } else if (
        formula.size() >= 4 &&
        formula.compare(0, 2, "\\[") == 0 &&
        formula.compare(formula.size() - 2, 2, "\\]") == 0
    ) {
        formula = formula.substr(
            2,
            formula.size() - 4
        );
    }

    formula = trim(formula);

    /*
     * render_inline_formula() работает с формулой в одну строку.
     * Для MathBlock заменяем переводы строк пробелами.
     */
    std::string single_line;
    single_line.reserve(formula.size());

    bool previous_was_space = false;

    for (char symbol : formula) {
        const bool is_space =
            symbol == ' ' ||
            symbol == '\t' ||
            symbol == '\r' ||
            symbol == '\n';

        if (is_space) {
            if (!previous_was_space) {
                single_line.push_back(' ');
                previous_was_space = true;
            }
        } else {
            single_line.push_back(symbol);
            previous_was_space = false;
        }
    }

    single_line = trim(single_line);

    const std::string inline_result =
        render_inline_formula(single_line);

    if (inline_result.find("class=\"math-error\"") !=
        std::string::npos) {
        std::vector<std::string> errors;
        errors.push_back("Ошибка математической формулы");

        return error_html(source, errors);
    }

    const std::size_t content_start =
        inline_result.find('>');

    const std::size_t content_end =
        inline_result.rfind("</span>");

    if (content_start == std::string::npos ||
        content_end == std::string::npos ||
        content_end <= content_start) {
        return std::string(
                   "<div class=\"math-block\" "
                   "style=\"margin:12px 0;"
                   "text-align:center;\">"
               ) +
               inline_result +
               "</div>";
    }

    const std::string content =
        inline_result.substr(
            content_start + 1,
            content_end - content_start - 1
        );

    return std::string(
               "<div class=\"math-block\" "
               "style=\"margin:12px 0;"
               "text-align:center;\">"
           ) +
           content +
           "</div>";
}

} // namespace math_ast

