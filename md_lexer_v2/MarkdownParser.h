#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include "Lexers.h"

#include <cstddef>
#include <string>
#include <vector>

class MarkdownParser {
private:
    InlineCodeLexer inline_code_lexer_;
    ImageLexer image_lexer_;
    LinkLexer link_lexer_;
    BoldLexer bold_lexer_;
    ItalicLexer italic_lexer_;
    StrikethroughLexer strikethrough_lexer_;

    CodeBlockLexer code_block_lexer_;
    HeadingLexer heading_lexer_;
    QuoteLexer quote_lexer_;
    HorizontalRuleLexer horizontal_rule_lexer_;
    ListLexer list_lexer_;
    TableLexer table_lexer_;
	MathInlineLexer math_inline_lexer_;
	MathBlockLexer math_block_lexer_;

    static std::size_t find_fence_start(
        const std::string& text
    );

    static std::size_t find_list_marker(
        const std::string& text
    );

    static std::size_t find_block_end(
        const std::string& buffer,
        std::size_t position
    );

    std::vector<MarkdownNode> parse_inline(
        const std::string& text
    );

    void process_list(
        MarkdownNode& list_node
    );

    void process_table(
        MarkdownNode& table
    );

    /*
     * Разбирает текст блока на верхнеуровневые узлы.
     *
     * start_line - абсолютный номер строки, на которой начинается
     * переданный текст (нумерация с нуля). Если start_line >= 0,
     * каждому верхнеуровневому узлу проставляются фактические
     * исходные координаты. При start_line < 0 (вложенный разбор
     * содержимого цитат и FencedMarkdown) координаты не назначаются,
     * чтобы дочерние узлы не получали ложные абсолютные позиции.
     */
    std::vector<MarkdownNode> parse_blocks(
        const std::string& text,
        int start_line = -1
    );

public:
    std::vector<MarkdownNode> parse(
        const std::string& buffer
    );
};

#endif // MARKDOWN_PARSER_H

