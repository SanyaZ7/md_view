#include "CodeHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextDocument>

#include <cstddef>

namespace
{

bool is_emoji_code_point(uint code_point)
{
    return (code_point >= 0x1F000 && code_point <= 0x1FAFF) ||
           (code_point >= 0x2600 && code_point <= 0x27BF) ||
           (code_point >= 0x2300 && code_point <= 0x23FF);
}



} // namespace

namespace
{

/*
 * Карта «байтовое смещение в UTF-8 -> позиция в QString».
 * tree-sitter считает смещения в байтах, а QSyntaxHighlighter
 * работает с позициями QString, поэтому для текста с не-ASCII
 * символами прямое использование смещений недопустимо.
 */
std::vector<int> build_byte_offsets(const QString& text)
{
    std::vector<int> offsets;

    offsets.reserve(
        static_cast<std::size_t>(text.size()) * 2 + 1);

    offsets.push_back(0);

    for (int index = 0; index < text.size();) {
        int utf16_length = 1;

        if (text.at(index).isHighSurrogate() &&
            index + 1 < text.size() &&
            text.at(index + 1).isLowSurrogate()) {
            utf16_length = 2;
        }

        const int utf8_length = text.mid(index, utf16_length).toUtf8().size();
        const int next_position = index + utf16_length;

        for (int step = 0; step < utf8_length; ++step) {
            offsets.push_back(next_position);
        }

        index = next_position;
    }

    return offsets;
}

} // namespace

CodeHighlighter::CodeHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
}

bool CodeHighlighter::available() const
{
    return highlight::Highlighter::tree_sitter_available();
}

void CodeHighlighter::setLanguage(highlight::Language language)
{
    language_ = language;
    highlighter_ = highlight::Highlighter::create(language);

    tokens_.clear();
    byte_offsets_.clear();

    refresh();
}

QTextCharFormat CodeHighlighter::format_for(
    const highlight::TokenStyle& style) const
{
    QTextCharFormat format;

    format.setForeground(
        QColor(
            static_cast<int>((style.color >> 16) & 0xFF),
            static_cast<int>((style.color >> 8) & 0xFF),
            static_cast<int>(style.color & 0xFF)));

    format.setFontWeight(style.bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(style.italic);
    format.setFontUnderline(style.underline);

    return format;
}

void CodeHighlighter::refresh()
{
    tokens_.clear();
    byte_offsets_.clear();

    if (language_ == highlight::Language::None ||
        !highlighter_ ||
        !highlighter_->available() ||
        !document()) {
        rehighlight();
        return;
    }

    const QString text = document()->toPlainText();

    if (text.size() > kMaxHighlightedLength) {
        rehighlight();
        return;
    }

    const QByteArray utf8 = text.toUtf8();

    const std::string code(
        utf8.constData(),
        static_cast<std::size_t>(utf8.size()));

    tokens_ = highlighter_->tokenize(code);
    byte_offsets_ = build_byte_offsets(text);

    rehighlight();
}

void CodeHighlighter::highlightBlock(const QString& text)
{
    if (tokens_.empty() || byte_offsets_.empty()) {
        return;
    }

    const highlight::ColorScheme& scheme =
        highlight::ColorScheme::default_light();

    /*
     * Позиция текущего блока в тексте документа берётся из
     * QTextBlock::position(), поэтому порядок вызовов
     * highlightBlock не важен.
     */
    const QTextBlock block = currentBlock();

    if (!block.isValid()) {
        return;
    }

    const int position = block.position();
    const int length = block.length() - 1;

    if (length <= 0) {
        return;
    }

    for (const highlight::Token& token : tokens_) {
        const std::size_t start_byte = token.start;
        const std::size_t end_byte = token.start + token.length;

        if (start_byte >= byte_offsets_.size() ||
            end_byte >= byte_offsets_.size()) {
            continue;
        }

        const int token_start = byte_offsets_[start_byte];
        const int token_end = byte_offsets_[end_byte];

        if (token_end <= position) {
            continue;
        }

        if (token_start >= position + length) {
            break;
        }

        const int from = qMax(token_start, position) - position;
        const int to = qMin(token_end, position + length) - position;

        if (to <= from) {
            continue;
        }

        setFormat(
            from,
            to - from,
            format_for(scheme.style_for(token.type)));
    }

    const QFont emoji_font(QStringLiteral("Noto Color Emoji"));

    for (int index = 0; index < block.length() - 1;) {
        const uint code_point = text.at(index).isHighSurrogate() &&
                index + 1 < text.size() &&
                text.at(index + 1).isLowSurrogate()
            ? QChar::surrogateToUcs4(text.at(index), text.at(index + 1))
            : text.at(index).unicode();

        const int length = code_point > 0xFFFF ? 2 : 1;

        if (is_emoji_code_point(code_point)) {
            QTextCharFormat emoji_format;
            emoji_format.setFontFamily(emoji_font.family());
            setFormat(index, length, emoji_format);
        }

        index += length;
    }
}
