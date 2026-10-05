#include "MarkdownParser.h"

#include <iostream>
#include <utility>
#include <algorithm>

#ifndef MD_DEV_DEBUG
#define MD_DEV_DEBUG 0
#endif

// ============================================================
// Поиск начала блока кода
// ============================================================

std::size_t MarkdownParser::find_fence_start(
    const std::string& text
) {
    std::size_t position = 0;

    while (position < text.length()) {
        std::size_t found = text.find("```", position);

        if (found == std::string::npos) {
            return std::string::npos;
        }

        if (found == 0 ||
            text[found - 1] == '\n') {
            return found;
        }

        position = found + 1;
    }

    return std::string::npos;
}

// ============================================================
// Поиск маркера списка
// ============================================================

std::size_t MarkdownParser::find_list_marker(
    const std::string& text
) {
    std::size_t position = 0;

    while (position < text.length()) {
        const std::size_t line_end =
            find_line_end(text, position);

        std::size_t marker_position = position;

        while (marker_position < line_end &&
               is_space_without_newline(
                   text[marker_position])) {
            ++marker_position;
        }

        if (marker_position < line_end) {
            const std::string line =
                text.substr(
                    marker_position,
                    line_end - marker_position
                );

            const MarkerInfo marker =
                parse_markdown_marker(line);

            if (marker.valid) {
                return position;
            }
        }

        if (line_end >= text.length()) {
            break;
        }

        position =
            find_next_line_start(text, line_end);
    }

    return std::string::npos;
}


// ============================================================
// Поиск конца блока
// ============================================================

std::size_t MarkdownParser::find_block_end(
    const std::string& buffer,
    std::size_t position
) {
    const std::size_t length = buffer.length();

    const std::size_t first_line_end =
        find_line_end(buffer, position);

    std::size_t first_marker_position = position;

    while (first_marker_position < first_line_end &&
           is_space_without_newline(
               buffer[first_marker_position])) {
        ++first_marker_position;
    }

    const bool starts_with_list =
        first_marker_position < first_line_end &&
        is_list_marker_at(
            buffer,
            first_marker_position
        );

    if (!starts_with_list) {
    bool inside_fence = false;
    std::size_t current = position;

    while (current < length) {
        const std::size_t line_end =
            find_line_end(buffer, current);

        const std::string line =
            buffer.substr(
                current,
                line_end - current
            );

        std::size_t fence_position = 0;

        while (fence_position < line.size() &&
               fence_position < 4 &&
               line[fence_position] == ' ') {
            ++fence_position;
        }

        const bool is_fence =
            fence_position + 3 <= line.size() &&
            line.compare(
                fence_position,
                3,
                "```"
            ) == 0;

        if (!inside_fence) {
            if (is_fence) {
                inside_fence = true;
            } else if (is_blank_line(line)) {
                return current;
            }
        } else if (is_fence) {
            inside_fence = false;
        }

        if (line_end >= length) {
            break;
        }

        current =
            find_next_line_start(buffer, line_end);
    }

    return length;
}

    const std::string first_line =
        buffer.substr(
            position,
            first_line_end - position
        );

    const std::size_t base_indent =
        count_leading_spaces(first_line);

    std::size_t marker_position = position;

    while (marker_position < first_line_end &&
           is_space_without_newline(
               buffer[marker_position])) {
        ++marker_position;
    }

    const MarkerInfo first_marker =
        parse_markdown_marker(
            buffer.substr(
                marker_position,
                first_line_end - marker_position
            )
        );

    if (!first_marker.valid) {
        return position;
    }

    const bool ordered = first_marker.ordered;

    std::size_t current = position;

    while (current < length) {
        const std::size_t line_end =
            find_line_end(buffer, current);

        const std::string line =
            buffer.substr(
                current,
                line_end - current
            );

        if (is_blank_line(line)) {
            std::size_t next =
                find_next_line_start(buffer, line_end);

            while (next < length) {
                const std::size_t next_end =
                    find_line_end(buffer, next);

                const std::string next_line =
                    buffer.substr(
                        next,
                        next_end - next
                    );

                if (!is_blank_line(next_line)) {
                    break;
                }

                next =
                    find_next_line_start(buffer, next_end);
            }

            if (next >= length) {
                return length;
            }

            const std::size_t next_end =
                find_line_end(buffer, next);

            const std::string next_line =
                buffer.substr(
                    next,
                    next_end - next
                );

            const std::size_t next_indent =
                count_leading_spaces(next_line);

            std::size_t next_marker_position = next;

            while (next_marker_position < next_end &&
                   is_space_without_newline(
                       buffer[next_marker_position])) {
                ++next_marker_position;
            }

            MarkerInfo next_marker;

            if (next_marker_position < next_end) {
                next_marker =
                    parse_markdown_marker(
                        next_line.substr(
                            next_marker_position - next,
                            next_end - next_marker_position
                        )
                    );
            }

            const bool is_nested =
                next_indent > base_indent;

            const bool is_same_level_item =
                next_indent == base_indent &&
                next_marker.valid &&
                next_marker.ordered == ordered;

            if (is_nested || is_same_level_item) {
                current = next;
                continue;
            }

            // Пустая строка отделяет список от следующего блока.
            return next;
        }

        const std::size_t indent =
            count_leading_spaces(line);

        std::size_t current_marker_position = current;

        while (current_marker_position < line_end &&
               is_space_without_newline(
                   buffer[current_marker_position])) {
            ++current_marker_position;
        }

        MarkerInfo marker;

        if (current_marker_position < line_end) {
            marker =
                parse_markdown_marker(
                    line.substr(
                        current_marker_position - current,
                        line_end - current_marker_position
                    )
                );
        }

        if (indent < base_indent) {
            return current;
        }

        if (indent == base_indent) {
            if (!marker.valid ||
                marker.ordered != ordered) {
                return current;
            }
        }

        // При большем отступе строка может быть:
        // - вложенным элементом;
        // - продолжением текста пункта.
        if (line_end >= length) {
            return length;
        }

        current =
            find_next_line_start(buffer, line_end);
    }

    return length;
}


// ============================================================
// Инлайн-разбор
// ============================================================
std::vector<MarkdownNode> MarkdownParser::parse_inline(
    const std::string& text
) {
    std::vector<MarkdownNode> result;

    std::vector<Lexer*> lexers;
    lexers.push_back(&inline_code_lexer_);
	lexers.push_back(&math_inline_lexer_);
	lexers.push_back(&image_lexer_);
	lexers.push_back(&link_lexer_);
	lexers.push_back(&bold_lexer_);
	lexers.push_back(&italic_lexer_);
	lexers.push_back(&strikethrough_lexer_);

    std::string plain;
    std::size_t position = 0;

    auto flush_plain = [&]() {
        if (plain.empty()) {
            return;
        }

        result.push_back(
            MarkdownNode::text(plain)
        );

        plain.clear();
    };

    while (position < text.length()) {
        /*
         * Экранирование Markdown-пунктуации.
         */
        if (text[position] == '\\' &&
            position + 1 < text.length()) {
            const char next = text[position + 1];

            const bool escapable =
                next == '\\' ||
                next == '`'  ||
                next == '*'  ||
                next == '_'  ||
                next == '{'  ||
                next == '}'  ||
                next == '['  ||
                next == ']'  ||
                next == '('  ||
                next == ')'  ||
                next == '#'  ||
                next == '+'  ||
                next == '-'  ||
                next == '.'  ||
                next == '!'  ||
                next == '>'  ||
                next == '~'  ||
                next == '|';

            if (escapable) {
                plain.push_back(next);
                position += 2;
                continue;
            }

            /*
             * Обратный слеш перед переводом строки создаёт
             * жёсткий перенос.
             */
            if (next == '\n') {
                flush_plain();

                MarkdownNode break_node;
                break_node.type = NodeType::HardBreak;
                result.push_back(std::move(break_node));

                position += 2;
                continue;
            }

            if (next == '\r') {
                std::size_t newline_length = 1;

                if (position + 2 < text.length() &&
                    text[position + 2] == '\n') {
                    newline_length = 2;
                }

                flush_plain();

                MarkdownNode break_node;
                break_node.type = NodeType::HardBreak;
                result.push_back(std::move(break_node));

                position += 1 + newline_length;
                continue;
            }
        }

        /*
         * Два пробела перед переводом строки создают
         * жёсткий перенос. Сам перевод строки не добавляется
         * в текстовый узел.
         */
        if ((text[position] == '\n' ||
             text[position] == '\r') &&
            plain.size() >= 2 &&
            plain[plain.size() - 1] == ' ' &&
            plain[plain.size() - 2] == ' ') {
            plain.pop_back();
            plain.pop_back();

            flush_plain();

            MarkdownNode break_node;
            break_node.type = NodeType::HardBreak;
            result.push_back(std::move(break_node));

            if (text[position] == '\r' &&
                position + 1 < text.length() &&
                text[position + 1] == '\n') {
                position += 2;
            } else {
                ++position;
            }

            continue;
        }

        /*
         * Обычный перевод строки — soft break.
         * Он остаётся в текстовом узле.
         */
        if (text[position] == '\r') {
            plain.push_back('\n');

            if (position + 1 < text.length() &&
                text[position + 1] == '\n') {
                position += 2;
            } else {
                ++position;
            }

            continue;
        }

        if (text[position] == '\n') {
            plain.push_back('\n');
            ++position;
            continue;
        }

        bool matched = false;

        for (Lexer* lexer : lexers) {
            LexerResult parsed =
                lexer->try_consume(text, position);

            if (!parsed.matched ||
                parsed.length == 0) {
                continue;
            }

            flush_plain();

            if (parsed.node.type == NodeType::Bold ||
    parsed.node.type == NodeType::Italic ||
    parsed.node.type == NodeType::Strikethrough ||
    parsed.node.type == NodeType::Link) {
    parsed.node.children =
        parse_inline(parsed.node.content);

    parsed.node.content.clear();
}


            result.push_back(
                std::move(parsed.node)
            );

            position += parsed.length;
            matched = true;
            break;
        }

        if (!matched) {
            plain.push_back(text[position]);
            ++position;
        }
    }

    flush_plain();

    return result;
}


// ============================================================
// Обработка списков
// ============================================================

void MarkdownParser::process_list(
    MarkdownNode& list_node
) {
    for (std::size_t i = 0;
         i < list_node.children.size();
         ++i) {
        MarkdownNode& item =
            list_node.children[i];

        if (item.type != NodeType::ListItem) {
            continue;
        }

        std::string item_text =
            item.content;

        std::vector<MarkdownNode> nested_lists;

        for (std::size_t j = 0;
             j < item.children.size();
             ++j) {
            if (item.children[j].type == NodeType::List) {
                nested_lists.push_back(
                    std::move(item.children[j])
                );
            }
        }

        item.content.clear();
        item.children.clear();

        std::vector<MarkdownNode> inline_nodes =
            parse_inline(item_text);

        for (std::size_t j = 0;
             j < inline_nodes.size();
             ++j) {
            item.children.push_back(
                std::move(inline_nodes[j])
            );
        }

        for (std::size_t j = 0;
             j < nested_lists.size();
             ++j) {
            process_list(nested_lists[j]);

            item.children.push_back(
                std::move(nested_lists[j])
            );
        }
    }
}

// ============================================================
// Обработка таблиц
// ============================================================

void MarkdownParser::process_table(
    MarkdownNode& table
) {
    for (std::size_t r = 0;
         r < table.children.size();
         ++r) {
        MarkdownNode& row =
            table.children[r];

        for (std::size_t c = 0;
             c < row.children.size();
             ++c) {
            MarkdownNode& cell =
                row.children[c];

            cell.children =
                parse_inline(cell.content);

            cell.content.clear();
        }
    }
}

// ============================================================
// Разбор блоков
// ============================================================

std::vector<MarkdownNode> MarkdownParser::parse_blocks(
    const std::string& text
)
{
    std::vector<MarkdownNode> result;

    if (text.empty()) {
        return result;
    }

    std::vector<Lexer*> block_lexers;

/*
     * CodeBlockLexer должен иметь приоритет над MathBlockLexer:
     * математические delimiters внутри fenced code не разбираются.
     */
    block_lexers.push_back(&code_block_lexer_);
    block_lexers.push_back(&math_block_lexer_);
    block_lexers.push_back(&heading_lexer_);
    block_lexers.push_back(&quote_lexer_);
    block_lexers.push_back(&table_lexer_);
    block_lexers.push_back(&horizontal_rule_lexer_);
    block_lexers.push_back(&list_lexer_);

    auto prepare_node = [this](
        MarkdownNode& node
    ) {
        if (node.type == NodeType::Heading) {
            node.children =
                parse_inline(node.content);

            node.content.clear();
		} else if (node.type == NodeType::FencedMarkdown) {
    		node.children=parse_blocks(node.content);
			node.content.clear();
        } else if (node.type == NodeType::Quote) {
            /*
             * Внутри цитаты могут находиться полноценные
             * блочные элементы, включая вложенные цитаты.
             */
            node.children =
                parse_blocks(node.content);

            node.content.clear();
        } else if (node.type == NodeType::List) {
            process_list(node);
        } else if (node.type == NodeType::Table) {
            process_table(node);
        } else if (node.type == NodeType::Link) {
            node.children =
                parse_inline(node.content);

            node.content.clear();
        }
    };

    auto remove_line_breaks_at_beginning = [](
        std::string& value
    ) {
        while (!value.empty() &&
               (value.front() == '\n' ||
                value.front() == '\r')) {
            value.erase(value.begin());
        }
    };

    auto remove_line_breaks_at_end = [](
        std::string& value
    ) {
        while (!value.empty() &&
               (value.back() == '\n' ||
                value.back() == '\r')) {
            value.pop_back();
        }
    };

    /*
     * Сначала проверяем блок в самом начале текста.
     */
    for (Lexer* lexer : block_lexers) {
        LexerResult parsed =
            lexer->try_consume(text, 0);

        if (!parsed.matched ||
            parsed.length == 0) {
            continue;
        }

        MarkdownNode node =
            std::move(parsed.node);

        prepare_node(node);
        result.push_back(std::move(node));

        std::size_t consumed =
            std::min(parsed.length, text.length());

        std::string rest =
            text.substr(consumed);

        remove_line_breaks_at_beginning(rest);
        remove_line_breaks_at_end(rest);

        if (!rest.empty()) {
            std::vector<MarkdownNode> more =
                parse_blocks(rest);

            for (MarkdownNode& more_node : more) {
                result.push_back(std::move(more_node));
            }
        }

        return result;
    }

    /*
     * Ищем следующий блочный элемент в начале строки.
     *
     * Это важно для содержимого цитаты:
     *
     * > Обычный текст
     * > > Вложенная цитата
     * > # Заголовок
     *
     * После удаления внешних маркеров цитаты
     * вложенный блок может находиться не в позиции 0.
     */
    std::size_t block_position =
        std::string::npos;

    LexerResult block_result;

    std::size_t candidate = 0;

    while (candidate < text.length()) {
        const std::size_t line_end =
            find_line_end(text, candidate);

        if (candidate > 0) {
            for (Lexer* lexer : block_lexers) {
                LexerResult parsed =
                    lexer->try_consume(text, candidate);

                if (!parsed.matched ||
                    parsed.length == 0) {
                    continue;
                }

                block_position = candidate;
                block_result = std::move(parsed);
                break;
            }

            if (block_position != std::string::npos) {
                break;
            }
        }

        if (line_end >= text.length()) {
            break;
        }

        candidate =
            find_next_line_start(text, line_end);
    }

    if (block_position != std::string::npos) {
        std::string prefix =
            text.substr(0, block_position);

        remove_line_breaks_at_end(prefix);

        if (!prefix.empty()) {
            MarkdownNode paragraph;
            paragraph.type = NodeType::Text;
            paragraph.children =
                parse_inline(prefix);

            result.push_back(std::move(paragraph));
        }

        MarkdownNode node =
            std::move(block_result.node);

        prepare_node(node);
        result.push_back(std::move(node));

        std::size_t consumed =
            block_position + block_result.length;

        if (consumed > text.length()) {
            consumed = text.length();
        }

        std::string rest =
            text.substr(consumed);

        remove_line_breaks_at_beginning(rest);
        remove_line_breaks_at_end(rest);

        if (!rest.empty()) {
            std::vector<MarkdownNode> more =
                parse_blocks(rest);

            for (MarkdownNode& more_node : more) {
                result.push_back(std::move(more_node));
            }
        }

        return result;
    }

    /*
     * Блочных элементов больше нет — оставшийся текст является
     * обычным абзацем.
     */
    MarkdownNode paragraph;
    paragraph.type = NodeType::Text;
    paragraph.children =
        parse_inline(text);

    result.push_back(std::move(paragraph));

    return result;
}


// ============================================================
// Публичный интерфейс парсера
// ============================================================

std::vector<MarkdownNode> MarkdownParser::parse(
    const std::string& buffer
)
{
    std::vector<MarkdownNode> document;

    std::size_t position = 0;

    /*
     * Возвращает номер строки для позиции в исходном буфере.
     *
     * Учитываются LF и CR. Для CRLF одна физическая строка
     * считается одной строкой.
     */
    const auto lineAt = [&buffer](std::size_t offset) {
        if (offset > buffer.size()) {
            offset = buffer.size();
        }

        int line = 0;

        for (std::size_t i = 0; i < offset; ++i) {
            if (buffer[i] == '\n') {
                ++line;
            } else if (buffer[i] == '\r') {
                if (i + 1 >= offset || buffer[i + 1] != '\n') {
                    ++line;
                }
            }
        }

        return line;
    };

    while (position < buffer.length()) {
        /*
         * Пропускаем пустые строки, включая строки,
         * состоящие только из пробелов и табуляций.
         */
        while (position < buffer.length()) {
            const std::size_t line_end =
                find_line_end(buffer, position);

            const std::string line =
                buffer.substr(
                    position,
                    line_end - position
                );

            if (!is_blank_line(line)) {
                break;
            }

            if (line_end >= buffer.length()) {
                position = buffer.length();
                break;
            }

            position =
                find_next_line_start(
                    buffer,
                    line_end
                );
        }

        if (position >= buffer.length()) {
            break;
        }

        std::size_t block_end =
            find_block_end(buffer, position);

        if (block_end > buffer.length()) {
            block_end = buffer.length();
        }

        std::string block =
            buffer.substr(
                position,
                block_end - position
            );

        /*
         * Убираем завершающие переводы строк, как и раньше.
         */
        while (!block.empty()) {
            const std::size_t line_end =
                find_line_end(
                    block,
                    block.length() - 1
                );

            if (line_end != block.length() ||
                block.back() == '\n' ||
                block.back() == '\r') {
                block.pop_back();
                continue;
            }

            break;
        }

#if MD_DEV_DEBUG
        std::cerr
            << "\n[DEV] block start=" << position
            << ", end=" << block_end
            << ", size=" << block.size()
            << "\n";

        std::cerr << "[DEV] block content:\n"
                  << block
                  << "\n[DEV] end block content\n";
#endif

        if (!block.empty() &&
            !is_blank_line(block)) {
            std::vector<MarkdownNode> blocks =
                parse_blocks(block);

            const int start_line =
                lineAt(position);

            const int end_line =
                lineAt(block_end);

            /*
             * parse_blocks() работает с локальной копией блока,
             * поэтому назначаем каждому созданному узлу диапазон
             * исходного блока.
             *
             * Для верхнеуровневого скролла этого достаточно:
             * каждый визуальный Markdown-блок получает устойчивую
             * исходную строку и не зависит от текста внутри него.
             */
            for (MarkdownNode& node : blocks) {
                node.source_start_line = start_line;
                node.source_end_line = end_line;

                document.push_back(
                    std::move(node)
                );
            }
        }

        if (block_end <= position) {
            ++position;
        } else {
            position = block_end;
        }
    }

    return document;
}



