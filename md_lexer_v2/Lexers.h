#ifndef LEXERS_H
#define LEXERS_H

#include "MarkdownNode.h"

#include <cstddef>
#include <string>
#include <vector>

// ============================================================
// Общие вспомогательные функции
// ============================================================

bool is_space_without_newline(
    char value
);

bool is_line_break(
    char value
);

bool is_blank_line(
    const std::string& line
);

std::size_t find_line_end(
    const std::string& text,
    std::size_t start
);

std::size_t find_next_line_start(
    const std::string& text,
    std::size_t line_end
);

std::size_t count_leading_spaces(
    const std::string& line
);

// ============================================================
// Распознавание маркеров списка
// ============================================================

struct MarkerInfo {
    bool valid = false;
    bool ordered = false;
    std::size_t content_offset = 0;
};

MarkerInfo parse_markdown_marker(
    const std::string& text
);

bool is_list_marker_at(
    const std::string& buffer,
    std::size_t position
);

bool is_list_marker_on_line(
    const std::string& buffer,
    std::size_t line_start,
    std::size_t line_end
);

// ============================================================
// Базовый интерфейс лексера
// ============================================================

class Lexer {
public:
    virtual ~Lexer() = default;

    virtual LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const = 0;
};

// ============================================================
// Инлайн-лексеры
// ============================================================

class InlineCodeLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class ImageLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class LinkLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class BoldLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class ItalicLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class StrikethroughLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

// ============================================================
// Блок-лексеры
// ============================================================

class HeadingLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class CodeBlockLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class QuoteLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class HorizontalRuleLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

// ============================================================
// Списки
// ============================================================

class ListLexer : public Lexer {
private:
    struct LineInfo {
        std::size_t start = 0;
        std::size_t end = 0;
        std::size_t indent = 0;
        bool has_marker = false;
        bool ordered = false;
        std::size_t content_start = 0;
    };

public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

// ============================================================
// Таблицы
// ============================================================

class TableLexer : public Lexer {
private:
    static std::string trim(
        const std::string& value
    );

    static bool is_separator_row(
        const std::string& line
    );

    static std::vector<std::string> split_row(
        const std::string& line
    );

    static Alignment parse_alignment(
        const std::string& separator
    );

    static MarkdownNode build_row(
        const std::string& line,
        const std::vector<Alignment>& alignments
    );

public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class MathInlineLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};

class MathBlockLexer : public Lexer {
public:
    LexerResult try_consume(
        const std::string& buffer,
        std::size_t position
    ) const override;
};


#endif // LEXERS_H

