#include "Lexers.h"
#include "CommonUtils.h"
#include <cctype>
#include <iostream>
#include <utility>

#ifndef MD_DEV_DEBUG
#define MD_DEV_DEBUG 0
#endif

// ============================================================
// Распознавание маркеров списка
// ============================================================

MarkerInfo parse_markdown_marker(const std::string& text) {
    MarkerInfo result;
    if (text.empty()) return result;

    char first = text[0];

    // Ненумерованный список:
    // "- text", "* text", "+ text"
    if (first == '-' || first == '*' || first == '+') {
        if (text.length() < 2 || !is_space_without_newline(text[1])) {
            return result;
        }
        result.valid = true;
        result.ordered = false;
        result.content_offset = 1;

        while (result.content_offset < text.length() && is_space_without_newline(text[result.content_offset])) {
            ++result.content_offset;
        }
        return result;
    }

    // Нумерованный список:
    // "1. text", "25. text"
    if (std::isdigit(static_cast<unsigned char>(first))) {
        std::size_t number_end = 0;
        while (number_end < text.length() && std::isdigit(static_cast<unsigned char>(text[number_end]))) {
            ++number_end;
        }

        if (number_end >= text.length() || text[number_end] != '.' || number_end + 1 >= text.length() || !is_space_without_newline(text[number_end + 1])) {
            return result;
        }

        result.valid = true;
        result.ordered = true;
        result.content_offset = number_end + 1;

        while (result.content_offset < text.length() && is_space_without_newline(text[result.content_offset])) {
            ++result.content_offset;
        }
        return result;
    }

    return result;
}

bool is_list_marker_at(const std::string& buffer, std::size_t position) {
    if (position >= buffer.length()) return false;
    std::size_t line_end = find_line_end(buffer, position);
    std::string line = buffer.substr(position, line_end - position);
    MarkerInfo marker = parse_markdown_marker(line);
    return marker.valid;
}

bool is_list_marker_on_line(const std::string& buffer, std::size_t line_start, std::size_t line_end) {
    std::size_t marker_position = line_start;
    while (marker_position < line_end && buffer[marker_position] == ' ') {
        marker_position++;
    }
    if (marker_position >= line_end) return false;

    std::string line = buffer.substr(marker_position, line_end - marker_position);
    MarkerInfo marker = parse_markdown_marker(line);
    return marker.valid;
}

// ============================================================
// Инлайн-лексеры
// ============================================================

LexerResult InlineCodeLexer::try_consume(const std::string& buffer, std::size_t position) const {
    std::size_t length = buffer.length();
    if (position >= length || buffer[position] != '`') return {0, {}, false};
    if (position + 1 < length && buffer[position + 1] == '`') return {0, {}, false};

    std::size_t end = buffer.find('`', position + 1);
    if (end == std::string::npos) return {0, {}, false};
    if (end + 1 < length && buffer[end + 1] == '`') return {0, {}, false};

    std::string code = buffer.substr(position + 1, end - position - 1);
    if (code.empty()) return {0, {}, false};

    MarkdownNode node;
    node.type = NodeType::InlineCode;
    node.content = code;
    return {end - position + 1, node, true};
}

LexerResult ImageLexer::try_consume(const std::string& buffer, std::size_t position) const {
    std::size_t length = buffer.length();
    if (position + 2 >= length || buffer[position] != '!' || buffer[position + 1] != '[') return {0, {}, false};

    std::size_t close_bracket = buffer.find(']', position + 2);
    if (close_bracket == std::string::npos) return {0, {}, false};
    if (close_bracket + 1 >= length || buffer[close_bracket + 1] != '(') return {0, {}, false};

    std::size_t close_parenthesis = buffer.find(')', close_bracket + 2);
    if (close_parenthesis == std::string::npos) return {0, {}, false};

    MarkdownNode node;
    node.type = NodeType::Image;
    node.content = buffer.substr(position + 2, close_bracket - position - 2);
    node.url = buffer.substr(close_bracket + 2, close_parenthesis - close_bracket - 2);
    return {close_parenthesis - position + 1, node, true};
}

LexerResult LinkLexer::try_consume(const std::string& buffer, std::size_t position) const {
    std::size_t length = buffer.length();
    if (position >= length || buffer[position] != '[') return {0, {}, false};

    std::size_t close_bracket = buffer.find(']', position + 1);
    if (close_bracket == std::string::npos) return {0, {}, false};
    if (close_bracket + 1 >= length || buffer[close_bracket + 1] != '(') return {0, {}, false};

    std::size_t close_parenthesis = buffer.find(')', close_bracket + 2);
    if (close_parenthesis == std::string::npos) return {0, {}, false};

    MarkdownNode node;
    node.type = NodeType::Link;
    node.content = buffer.substr(position + 1, close_bracket - position - 1);
    node.url = buffer.substr(close_bracket + 2, close_parenthesis - close_bracket - 2);
    return {close_parenthesis - position + 1, node, true};
}

LexerResult BoldLexer::try_consume(const std::string& buffer, std::size_t position) const {
    std::size_t length = buffer.length();
    if (position + 1 >= length) return {0, {}, false};

    bool asterisk = (buffer[position] == '*' && buffer[position + 1] == '*');
    bool underscore = (buffer[position] == '_' && buffer[position + 1] == '_');
    if (!asterisk && !underscore) return {0, {}, false};
    if (position + 2 < length && is_space_without_newline(buffer[position + 2])) return {0, {}, false};

    char delimiter = asterisk ? '*' : '_';
    std::size_t end = position + 2;

    while (end + 1 < length) {
        if (buffer[end] == delimiter && buffer[end + 1] == delimiter) {
            bool invalid_closing_space = (end > position + 2 && is_space_without_newline(buffer[end - 1]));
            if (!invalid_closing_space) {
                std::string inner = buffer.substr(position + 2, end - position - 2);
                if (!inner.empty()) {
                    MarkdownNode node;
                    node.type = NodeType::Bold;
                    node.content = inner;
                    return {end - position + 2, node, true};
                }
            }
        }
        end++;
    }
    return {0, {}, false};
}

LexerResult ItalicLexer::try_consume(
    const std::string& buffer,
    std::size_t position
) const
{
    const std::size_t length =
        buffer.length();

    if (position >= length ||
        (buffer[position] != '*' &&
         buffer[position] != '_')) {
        return {0, {}, false};
    }

    if (position + 1 < length &&
        buffer[position + 1] == buffer[position]) {
        return {0, {}, false};
    }

    if (position + 1 < length &&
        is_space_without_newline(
            buffer[position + 1])) {
        return {0, {}, false};
    }

    const char delimiter =
        buffer[position];

    std::size_t end =
        position + 1;

    while (end < length) {
        if (buffer[end] == delimiter) {
            if (end + 1 < length &&
                buffer[end + 1] == delimiter) {
                return {0, {}, false};
            }

            const bool invalid_closing_space =
                end > position + 1 &&
                is_space_without_newline(
                    buffer[end - 1]);

            if (!invalid_closing_space) {
                const std::string inner =
                    buffer.substr(
                        position + 1,
                        end - position - 1
                    );

                if (!inner.empty()) {
                    MarkdownNode node;
                    node.type = NodeType::Italic;
                    node.content = inner;

                    return {
                        end - position + 1,
                        std::move(node),
                        true
                    };
                }
            }
        }

        ++end;
    }

    return {0, {}, false};
}


LexerResult StrikethroughLexer::try_consume(const std::string& buffer, std::size_t position) const {
    std::size_t length = buffer.length();
    if (position + 1 >= length || buffer[position] != '~' || buffer[position + 1] != '~') return {0, {}, false};
    if (position + 2 < length && is_space_without_newline(buffer[position + 2])) return {0, {}, false};

    std::size_t end = buffer.find("~~", position + 2);
    if (end == std::string::npos) return {0, {}, false};
    if (end > position + 2 && is_space_without_newline(buffer[end - 1])) return {0, {}, false};

    std::string inner = buffer.substr(position + 2, end - position - 2);
    if (inner.empty()) return {0, {}, false};

    MarkdownNode node;
    node.type = NodeType::Strikethrough;
    node.content = inner;
    return {end - position + 2, node, true};
}

// ============================================================
// Блок-лексеры
// ============================================================

LexerResult HeadingLexer::try_consume(
    const std::string& buffer,
    std::size_t position
) const
{
    const std::size_t length = buffer.length();

    if (position >= length) {
        return {0, {}, false};
    }

    /*
     * ATX-заголовок может иметь до трёх пробелов
     * перед символами '#'.
     */
    std::size_t marker_position = position;

    while (marker_position < length &&
           marker_position - position < 3 &&
           buffer[marker_position] == ' ') {
        ++marker_position;
    }

    if (marker_position >= length ||
        buffer[marker_position] != '#') {
        return {0, {}, false};
    }

    std::size_t level = 0;

    while (marker_position + level < length &&
           buffer[marker_position + level] == '#') {
        ++level;
    }

    if (level == 0 || level > 6) {
        return {0, {}, false};
    }

    const std::size_t separator_position =
        marker_position + level;

    /*
     * После последовательности '#' должен быть пробел
     * или табуляция. Вариант "#Заголовок" не является
     * ATX-заголовком.
     */
    if (separator_position >= length ||
        (buffer[separator_position] != ' ' &&
         buffer[separator_position] != '\t')) {
        return {0, {}, false};
    }

    const std::size_t line_end =
        find_line_end(buffer, position);

    std::size_t content_start =
        separator_position + 1;

    while (content_start < line_end &&
           (buffer[content_start] == ' ' ||
            buffer[content_start] == '\t')) {
        ++content_start;
    }

    std::size_t content_end = line_end;

    while (content_end > content_start &&
           (buffer[content_end - 1] == ' ' ||
            buffer[content_end - 1] == '\t' ||
            buffer[content_end - 1] == '\r')) {
        --content_end;
    }

    /*
     * Поддержка закрывающей последовательности '#':
     *
     * # Заголовок #
     * # Заголовок ###
     */
    if (content_end > content_start &&
        buffer[content_end - 1] == '#') {
        std::size_t hash_start = content_end;

        while (hash_start > content_start &&
               buffer[hash_start - 1] == '#') {
            --hash_start;
        }

        if (hash_start > content_start &&
            (buffer[hash_start - 1] == ' ' ||
             buffer[hash_start - 1] == '\t')) {
            content_end = hash_start - 1;

            while (content_end > content_start &&
                   (buffer[content_end - 1] == ' ' ||
                    buffer[content_end - 1] == '\t')) {
                --content_end;
            }
        }
    }

    MarkdownNode node;
    node.type = NodeType::Heading;
    node.level = static_cast<int>(level);
    node.content = buffer.substr(
        content_start,
        content_end - content_start
    );

    std::size_t consumed_end = line_end;

    if (line_end < length) {
        consumed_end =
            find_next_line_start(buffer, line_end);
    }

    return {
        consumed_end - position,
        std::move(node),
        true
    };
}

// ============================================================
// Обработка математических формул
// ============================================================

LexerResult MathInlineLexer::try_consume(
    const std::string& buffer,
    std::size_t position
) const
{
    const std::size_t length = buffer.length();

    if (position >= length) {
        return {0, {}, false};
    }

    /*
     * $formula$
     *
     * Последовательность $$ является началом блочной формулы
     * и не должна распознаваться как inline-формула.
     */
    if (buffer[position] == '$') {
        if (position + 1 < length &&
            buffer[position + 1] == '$') {
            return {0, {}, false};
        }

        std::size_t closing = position + 1;

        while (closing < length) {
            if (buffer[closing] == '\n' ||
                buffer[closing] == '\r') {
                return {0, {}, false};
            }

            if (buffer[closing] == '$') {
                /*
                 * $$ не является закрывающим delimiter-ом
                 * inline-формулы.
                 */
                if (closing + 1 < length &&
                    buffer[closing + 1] == '$') {
                    return {0, {}, false};
                }

                if (closing == position + 1) {
                    return {0, {}, false};
                }

                MarkdownNode node;
                node.type = NodeType::MathInline;
                node.content = buffer.substr(
                    position + 1,
                    closing - position - 1
                );

                return {
                    closing - position + 1,
                    std::move(node),
                    true
                };
            }

            ++closing;
        }

        return {0, {}, false};
    }

    /*
     * \(formula\)
     */
    if (position + 1 < length &&
        buffer[position] == '\\' &&
        buffer[position + 1] == '(') {
        const std::size_t content_start = position + 2;
        std::size_t closing = content_start;

        while (closing + 1 < length) {
            if (buffer[closing] == '\n' ||
                buffer[closing] == '\r') {
                return {0, {}, false};
            }

            if (buffer[closing] == '\\' &&
                buffer[closing + 1] == ')') {
                if (closing == content_start) {
                    return {0, {}, false};
                }

                MarkdownNode node;
                node.type = NodeType::MathInline;
                node.content = buffer.substr(
                    content_start,
                    closing - content_start
                );

                return {
                    closing + 2 - position,
                    std::move(node),
                    true
                };
            }

            ++closing;
        }

        return {0, {}, false};
    }

    return {0, {}, false};
}

LexerResult MathBlockLexer::try_consume(
    const std::string& buffer,
    std::size_t position
) const
{
    const std::size_t length = buffer.length();

    if (position >= length) {
        return {0, {}, false};
    }

    const std::size_t opening_line_end =
        find_line_end(buffer, position);

    /*
     * Разрешаем до трёх ведущих пробелов перед delimiter-ом.
     */
    std::size_t marker_position = position;

    while (marker_position < opening_line_end &&
           marker_position - position < 3 &&
           buffer[marker_position] == ' ') {
        ++marker_position;
    }

    enum class Delimiter {
        DoubleDollar,
        Brackets
    };

    Delimiter delimiter;
    std::string closing_delimiter;

    if (marker_position + 2 <= opening_line_end &&
        buffer.compare(marker_position, 2, "$$") == 0) {
        delimiter = Delimiter::DoubleDollar;
        closing_delimiter = "$$";
    } else if (
        marker_position + 2 <= opening_line_end &&
        buffer.compare(marker_position, 2, "\\[") == 0
    ) {
        delimiter = Delimiter::Brackets;
        closing_delimiter = "\\]";
    } else {
        return {0, {}, false};
    }

    const std::size_t opening_length = 2;
    const std::size_t content_start_on_line =
        marker_position + opening_length;

    /*
     * Поддержка однострочной блочной формулы:
     *
     *   $$E = mc^2$$
     *   \[\frac{a}{b}\]
     *
     * После закрывающего delimiter допускаются только пробелы,
     * табуляции и CR.
     */
    std::size_t inline_closing =
        buffer.find(
            closing_delimiter,
            content_start_on_line
        );

    if (inline_closing != std::string::npos &&
        inline_closing < opening_line_end &&
        inline_closing > content_start_on_line) {
        bool only_spaces_after = true;

        for (std::size_t i =
                 inline_closing + closing_delimiter.size();
             i < opening_line_end;
             ++i) {
            if (buffer[i] != ' ' &&
                buffer[i] != '\t' &&
                buffer[i] != '\r') {
                only_spaces_after = false;
                break;
            }
        }

        if (only_spaces_after) {
            std::string content =
                buffer.substr(
                    content_start_on_line,
                    inline_closing - content_start_on_line
                );

            /*
             * Убираем пробелы и табуляции по краям содержимого
             * однострочной формулы.
             */
            while (!content.empty() &&
                   (content.front() == ' ' ||
                    content.front() == '\t' ||
                    content.front() == '\r')) {
                content.erase(content.begin());
            }

            while (!content.empty() &&
                   (content.back() == ' ' ||
                    content.back() == '\t' ||
                    content.back() == '\r')) {
                content.pop_back();
            }

            MarkdownNode node;
            node.type = NodeType::MathBlock;
            node.content = std::move(content);

            std::size_t consumed_end = opening_line_end;

            if (consumed_end < length) {
                consumed_end =
                    find_next_line_start(buffer, consumed_end);
            }

            return {
                consumed_end - position,
                std::move(node),
                true
            };
        }
    }

    /*
     * Многострочный вариант: открывающий delimiter должен
     * занимать всю строку, кроме необязательных пробелов после него.
     */
    for (std::size_t i = content_start_on_line;
         i < opening_line_end;
         ++i) {
        if (buffer[i] != ' ' &&
            buffer[i] != '\t' &&
            buffer[i] != '\r') {
            return {0, {}, false};
        }
    }

    std::size_t content_start = opening_line_end;

    if (content_start < length) {
        content_start =
            find_next_line_start(buffer, content_start);
    }

    std::size_t current = content_start;

    while (current < length) {
        const std::size_t line_end =
            find_line_end(buffer, current);

        std::size_t line_start = current;

        while (line_start < line_end &&
               line_start - current < 3 &&
               buffer[line_start] == ' ') {
            ++line_start;
        }

        const bool is_closing =
            line_start + closing_delimiter.size() <= line_end &&
            buffer.compare(
                line_start,
                closing_delimiter.size(),
                closing_delimiter
            ) == 0;

        if (is_closing) {
            bool only_spaces_after = true;

            for (std::size_t i =
                     line_start + closing_delimiter.size();
                 i < line_end;
                 ++i) {
                if (buffer[i] != ' ' &&
                    buffer[i] != '\t' &&
                    buffer[i] != '\r') {
                    only_spaces_after = false;
                    break;
                }
            }

            if (!only_spaces_after) {
                return {0, {}, false};
            }

            std::string content;

            if (content_start < current) {
                content = buffer.substr(
                    content_start,
                    current - content_start
                );
            }

            /*
             * Нормализуем CRLF/CR в LF.
             */
            std::string normalized;
            normalized.reserve(content.size());

            for (std::size_t i = 0;
                 i < content.size();
                 ++i) {
                if (content[i] == '\r') {
                    if (i + 1 < content.size() &&
                        content[i + 1] == '\n') {
                        ++i;
                    }

                    normalized.push_back('\n');
                } else {
                    normalized.push_back(content[i]);
                }
            }

            /*
             * Убираем внешний перевод строки после открывающего
             * delimiter-а и перед закрывающим delimiter-ом.
             */
            while (!normalized.empty() &&
                   (normalized.front() == '\n' ||
                    normalized.front() == '\r')) {
                normalized.erase(normalized.begin());
            }

            while (!normalized.empty() &&
                   (normalized.back() == '\n' ||
                    normalized.back() == '\r')) {
                normalized.pop_back();
            }

            MarkdownNode node;
            node.type = NodeType::MathBlock;
            node.content = std::move(normalized);

            std::size_t consumed_end = line_end;

            if (consumed_end < length) {
                consumed_end =
                    find_next_line_start(buffer, consumed_end);
            }

            return {
                consumed_end - position,
                std::move(node),
                true
            };
        }

        if (line_end >= length) {
            break;
        }

        current =
            find_next_line_start(buffer, line_end);
    }

    return {0, {}, false};
}

LexerResult CodeBlockLexer::try_consume(
    const std::string& buffer,
    std::size_t position
) const {
    const std::size_t length = buffer.length();

    if (position >= length) {
        return {0, {}, false};
    }

    const std::size_t opening_line_end =
        find_line_end(buffer, position);

    std::size_t opening_indent = 0;

    while (position + opening_indent < opening_line_end &&
           opening_indent < 4 &&
           buffer[position + opening_indent] == ' ') {
        ++opening_indent;
    }

    if (position + opening_indent + 3 > opening_line_end ||
        buffer.compare(
            position + opening_indent,
            3,
            "```"
        ) != 0) {
        return {0, {}, false};
    }

    const std::size_t fence_start =
        position + opening_indent;

    std::size_t fence_length = 0;

    while (fence_start + fence_length < opening_line_end &&
           buffer[fence_start + fence_length] == '`') {
        ++fence_length;
    }

    if (fence_length < 3) {
        return {0, {}, false};
    }

    /*
     * Всё после открывающего fence является info string.
     *
     * Пустая info string означает математический fenced-блок:
     *
     *   ```
     *   \frac{a}{b}
     *   ```
     *
     * Непустая info string сохраняет обычное поведение code-блока:
     *
     *   ```cpp
     *   int main() {}
     *   ```
     */
    std::string info_string;

    if (fence_start + fence_length < opening_line_end) {
        info_string = buffer.substr(
            fence_start + fence_length,
            opening_line_end -
                fence_start -
                fence_length
        );

        while (!info_string.empty() &&
               (info_string.back() == ' ' ||
                info_string.back() == '\t' ||
                info_string.back() == '\r')) {
            info_string.pop_back();
        }

        for (char value : info_string) {
            if (value == '`') {
                return {0, {}, false};
            }
        }
    }

    const bool is_fenced_markdown =
        info_string.empty();

    std::size_t body_start = opening_line_end;

    if (body_start < length) {
        body_start =
            find_next_line_start(buffer, body_start);
    }

    std::size_t closing_start = std::string::npos;
    std::size_t closing_end = std::string::npos;

    std::size_t current = body_start;

    while (current < length) {
        const std::size_t line_end =
            find_line_end(buffer, current);

        std::size_t indent = 0;

        while (current + indent < line_end &&
               indent < 4 &&
               buffer[current + indent] == ' ') {
            ++indent;
        }

        std::size_t run_length = 0;

        while (current + indent + run_length < line_end &&
               buffer[current + indent + run_length] == '`') {
            ++run_length;
        }

        bool only_spaces_after_fence = true;

        for (std::size_t i =
                 current + indent + run_length;
             i < line_end;
             ++i) {
            if (buffer[i] != ' ' &&
                buffer[i] != '\t' &&
                buffer[i] != '\r') {
                only_spaces_after_fence = false;
                break;
            }
        }

        if (run_length >= fence_length &&
            only_spaces_after_fence) {
            closing_start = current;
            closing_end = line_end;

            if (closing_end < length) {
                closing_end =
                    find_next_line_start(buffer, closing_end);
            }

            break;
        }

        if (line_end >= length) {
            break;
        }

        current =
            find_next_line_start(buffer, line_end);
    }

    /*
     * По CommonMark незакрытый fence распространяется до конца
     * документа.
     */
    if (closing_start == std::string::npos) {
        closing_start = length;
        closing_end = length;
    }

    std::string content;

    if (body_start < closing_start) {
        content = buffer.substr(
            body_start,
            closing_start - body_start
        );
    }

    while (!content.empty() &&
           (content.back() == '\n' ||
            content.back() == '\r')) {
        content.pop_back();
    }

    MarkdownNode node;

    node.type = is_fenced_markdown
        ? NodeType::FencedMarkdown
        : NodeType::CodeBlock;

    node.content = std::move(content);

    return {
        closing_end - position,
        std::move(node),
        true
    };
}

LexerResult QuoteLexer::try_consume(
    const std::string& buffer,
    std::size_t position
) const
{
    const std::size_t length = buffer.length();

    if (position >= length) {
        return {0, {}, false};
    }

    auto find_quote_marker = [](
        const std::string& text,
        std::size_t line_start,
        std::size_t line_end,
        std::size_t& marker_position
    ) -> bool {
        marker_position = line_start;

        /*
         * До трёх пробелов перед маркером цитаты
         * разрешены стандартом Markdown.
         */
        while (marker_position < line_end &&
               marker_position - line_start < 3 &&
               text[marker_position] == ' ') {
            ++marker_position;
        }

        return marker_position < line_end &&
               text[marker_position] == '>';
    };

    const std::size_t first_line_end =
        find_line_end(buffer, position);

    std::size_t first_marker_position = 0;

    if (!find_quote_marker(
            buffer,
            position,
            first_line_end,
            first_marker_position)) {
        return {0, {}, false};
    }

    std::string content;
    std::size_t current = position;

    while (current < length) {
        const std::size_t line_end =
            find_line_end(buffer, current);

        std::size_t marker_position = 0;

        if (!find_quote_marker(
                buffer,
                current,
                line_end,
                marker_position)) {
            break;
        }

        /*
         * Удаляем только внешний маркер '>'.
         *
         * Примеры:
         *
         * > текст       -> текст
         * >> текст      -> > текст
         * > > текст     -> > текст
         * >>> текст     -> >> текст
         * >текст        -> текст
         */
        std::size_t text_start =
            marker_position + 1;

        if (text_start < line_end &&
            (buffer[text_start] == ' ' ||
             buffer[text_start] == '\t')) {
            ++text_start;
        }

        if (!content.empty()) {
            content.push_back('\n');
        }

        content.append(
            buffer,
            text_start,
            line_end - text_start
        );

        if (line_end >= length) {
            current = length;
            break;
        }

        current =
            find_next_line_start(buffer, line_end);
    }

    if (current <= position) {
        return {0, {}, false};
    }

    MarkdownNode node;
    node.type = NodeType::Quote;
    node.content = std::move(content);

    return {
        current - position,
        std::move(node),
        true
    };
}


LexerResult HorizontalRuleLexer::try_consume(const std::string& buffer, std::size_t position) const {
    if (position >= buffer.length()) return {0, {}, false};
    const std::size_t line_end = find_line_end(buffer, position);
    std::size_t symbol_count = 0;
    char symbol = '\0';

    for (std::size_t current = position; current < line_end; ++current) {
        const char value = buffer[current];
        if (value == ' ' || value == '\t') continue;
        if (value != '-' && value != '*' && value != '_') return {0, {}, false};
        if (symbol == '\0') symbol = value; 
        else if (symbol != value) return {0, {}, false};
        ++symbol_count;
    }

    if (symbol_count < 3) return {0, {}, false};

    MarkdownNode node;
    node.type = NodeType::HorizontalRule;
    std::size_t consumed = (line_end < buffer.length()) ? find_next_line_start(buffer, line_end) : line_end;
    return {consumed - position, node, true};
}

// ============================================================
// Списки
// ============================================================
LexerResult ListLexer::try_consume(const std::string& buffer, std::size_t position) const {
    const std::size_t length = buffer.length();
    if (position >= length) return {0, {}, false};

    const std::size_t first_line_end = find_line_end(buffer, position);
    const std::string first_line = buffer.substr(position, first_line_end - position);
    const std::size_t base_indent = count_leading_spaces(first_line);
    if (base_indent >= first_line.length()) return {0, {}, false};

    const std::string first_trimmed = first_line.substr(base_indent);
    const MarkerInfo first_marker = parse_markdown_marker(first_trimmed);
    if (!first_marker.valid) return {0, {}, false};

    const bool list_is_ordered = first_marker.ordered;
    std::vector<LineInfo> lines;
    std::size_t current = position;

    while (current < length) {
        const std::size_t line_end = find_line_end(buffer, current);
        const std::string line = buffer.substr(current, line_end - current);

        if (is_blank_line(line)) {
            std::size_t next = find_next_line_start(buffer, line_end);
            while (next < length) {
                const std::size_t next_end = find_line_end(buffer, next);
                if (!is_blank_line(buffer.substr(next, next_end - next))) break;
                next = find_next_line_start(buffer, next_end);
            }
            if (next >= length) { current = length; break; }

            const std::size_t next_end = find_line_end(buffer, next);
            const std::string next_line = buffer.substr(next, next_end - next);
            const std::size_t next_indent = count_leading_spaces(next_line);
            std::size_t next_marker_pos = next;
            while (next_marker_pos < next_end && is_space_without_newline(buffer[next_marker_pos])) ++next_marker_pos;

            MarkerInfo next_marker;
            if (next_marker_pos < next_end) next_marker = parse_markdown_marker(buffer.substr(next_marker_pos, next_end - next_marker_pos));

            if (next_indent > base_indent || (next_indent == base_indent && next_marker.valid && next_marker.ordered == list_is_ordered)) {
                current = next; continue;
            }
            current = next; break;
        }

        const std::size_t indent = count_leading_spaces(line);
        if (indent >= line.length()) { current = find_next_line_start(buffer, line_end); continue; }

        const std::string trimmed = line.substr(indent);
        const MarkerInfo marker = parse_markdown_marker(trimmed);

        if (indent < base_indent) break;

        bool has_marker = false;
        std::size_t content_offset = 0;

        if (indent == base_indent) {
            if (!marker.valid || marker.ordered != list_is_ordered) break;
            has_marker = true;
            content_offset = marker.content_offset;
        } else {
            if (marker.valid) { has_marker = true; content_offset = marker.content_offset; }
            else {
                if (!lines.empty()) lines.back().end = line_end;
                current = find_next_line_start(buffer, line_end);
                continue;
            }
        }

        LineInfo info;
        info.start = current; info.end = line_end; info.indent = indent; info.has_marker = has_marker;
        info.ordered = marker.ordered; info.content_start = current + indent + content_offset;
        lines.push_back(info);

        if (line_end >= length) { current = length; break; }
        current = find_next_line_start(buffer, line_end);
    }

    if (lines.empty()) return {0, {}, false};

    MarkdownNode list_node;
    list_node.type = NodeType::List;
    list_node.ordered = list_is_ordered;
    std::size_t index = 0;

    while (index < lines.size()) {
        if (lines[index].indent != base_indent || !lines[index].has_marker) { ++index; continue; }

        MarkdownNode item_node;
        item_node.type = NodeType::ListItem;
        const std::size_t item_start = index;
        std::size_t next = index + 1;
        while (next < lines.size() && lines[next].indent > base_indent) ++next;

        item_node.content.clear();
        item_node.content.append(buffer, lines[index].content_start, lines[index].end - lines[index].content_start);

        for (std::size_t i = index + 1; i < next; ++i) {
            const LineInfo& line = lines[i];
            if (line.indent > base_indent && line.has_marker) break;
            if (!item_node.content.empty()) item_node.content.push_back('\n');
            std::size_t content_start = line.start;
            while (content_start < line.end && content_start < buffer.length() && is_space_without_newline(buffer[content_start])) ++content_start;
            item_node.content.append(buffer, content_start, line.end - content_start);
        }

        if (next > item_start + 1) {
            const std::size_t nested_start_index = item_start + 1;
            bool has_nested_list = false;
            for (std::size_t i = nested_start_index; i < next; ++i) {
                if (lines[i].indent > base_indent && lines[i].has_marker) { has_nested_list = true; break; }
            }

            if (has_nested_list) {
                const std::size_t nested_start = lines[nested_start_index].start;
                const std::size_t nested_end = lines[next - 1].end;
                std::string nested_buffer = buffer.substr(nested_start, nested_end - nested_start);
                std::size_t nested_indent = lines[nested_start_index].indent;
                for (std::size_t i = nested_start_index; i < next; ++i) {
                    if (lines[i].indent > base_indent && lines[i].indent < nested_indent) nested_indent = lines[i].indent;
                }
                nested_buffer = remove_common_indent(nested_buffer, nested_indent);
                while (!nested_buffer.empty() && is_line_break(nested_buffer.back())) nested_buffer.pop_back();

                if (!nested_buffer.empty()) {
                    ListLexer nested_lexer;
                    LexerResult nested_result = nested_lexer.try_consume(nested_buffer, 0);
                    if (nested_result.matched) item_node.children.push_back(std::move(nested_result.node));
                }
            }
        }
        list_node.children.push_back(std::move(item_node));
        index = next;
    }

    return {current - position, list_node, true};
}

// ============================================================
// Таблицы
// ============================================================

std::string TableLexer::trim(const std::string& value) {
    std::size_t begin = value.find_first_not_of(" \t\r");
    if (begin == std::string::npos) return "";
    std::size_t end = value.find_last_not_of(" \t\r");
    return value.substr(begin, end - begin + 1);
}

bool TableLexer::is_separator_row(const std::string& line) {
    if (line.find('-') == std::string::npos) return false;
    for (std::size_t i = 0; i < line.length(); ++i) {
        char symbol = line[i];
        if (symbol != '|' && symbol != '-' && symbol != ':' && symbol != ' ' && symbol != '\t' && symbol != '\r') return false;
    }
    return true;
}

std::vector<std::string> TableLexer::split_row(const std::string& line) {
    std::vector<std::string> cells;
    if (line.empty() || line[0] != '|') return cells;
    std::size_t start = 1;
    std::size_t end = line.length();
    if (end > 0 && line[end - 1] == '|') end--;
    std::string current;
    for (std::size_t i = start; i < end; ++i) {
        if (line[i] == '|') { cells.push_back(trim(current)); current.clear(); }
        else current += line[i];
    }
    cells.push_back(trim(current));
    return cells;
}

Alignment TableLexer::parse_alignment(const std::string& separator) {
    std::string value = trim(separator);
    if (value.empty()) return Alignment::Left;
    bool left = value.front() == ':';
    bool right = value.back() == ':';
    if (left && right) return Alignment::Center;
    if (right) return Alignment::Right;
    return Alignment::Left;
}

MarkdownNode TableLexer::build_row(const std::string& line, const std::vector<Alignment>& alignments) {
    MarkdownNode row;
    row.type = NodeType::TableRow;
    std::vector<std::string> cells = split_row(line);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        MarkdownNode cell;
        cell.type = NodeType::TableCell;
        cell.content = cells[i];
        if (i < alignments.size()) cell.alignment = alignments[i];
        row.children.push_back(std::move(cell));
    }
    return row;
}

LexerResult TableLexer::try_consume(const std::string& buffer, std::size_t position) const {
    std::size_t length = buffer.length();
    if (position >= length) return {0, {}, false};
    std::size_t current = position;
    while (current < length && buffer[current] == ' ' && current - position < 3) ++current;
    if (current >= length || buffer[current] != '|') return {0, {}, false};

    std::vector<std::string> lines;
    while (current < length) {
        std::size_t line_end = find_line_end(buffer, current);
        std::string line = buffer.substr(current, line_end - current);
        std::size_t left_spaces = count_leading_spaces(line);
        if (left_spaces > 0) line = line.substr(left_spaces);
        if (line.empty() || line[0] != '|') break;
        lines.push_back(line);
        current = find_next_line_start(buffer, line_end);
    }

    if (lines.size() < 2 || !is_separator_row(lines[1])) return {0, {}, false};
    std::vector<std::string> separators = split_row(lines[1]);
    std::vector<Alignment> alignments;
    for (std::size_t i = 0; i < separators.size(); ++i) alignments.push_back(parse_alignment(separators[i]));

    MarkdownNode table;
    table.type = NodeType::Table;
    MarkdownNode header = build_row(lines[0], alignments);
    header.is_header = true;
    table.children.push_back(std::move(header));
    for (std::size_t i = 2; i < lines.size(); ++i) table.children.push_back(build_row(lines[i], alignments));

    return {current - position, table, true};
}

