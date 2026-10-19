#ifndef CODE_HIGHLIGHTER_H
#define CODE_HIGHLIGHTER_H

#include "ColorScheme.h"
#include "highlight.h"

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <memory>
#include <string>
#include <vector>

class QTextDocument;

/*
 * Подсветка исходника C/C++ в QPlainTextEdit.
 *
 * tree-sitter разбирает текст целиком, поэтому токены считаются
 * для всего документа сразу (refresh()), а QSyntaxHighlighter
 * раскрашивает их поблочно. Такой путь не изменяет документ и не
 * попадает в undo-стек редактора.
 */
class CodeHighlighter : public QSyntaxHighlighter
{
public:
    explicit CodeHighlighter(QTextDocument* document);

    /*
     * Язык определяется вызывающей стороной (расширение файла или
     * info string блока). Language::None отключает подсветку.
     */
    void setLanguage(highlight::Language language);

    highlight::Language language() const
    {
        return language_;
    }

    /* Есть ли реальная подсветка (доступен ли tree-sitter). */
    bool available() const;

    /*
     * Пересчёт токенов по текущему тексту документа и перекраска.
     * Вызывается после изменений текста.
     */
    void refresh();

protected:
    void highlightBlock(const QString& text) override;

private:
    QTextCharFormat format_for(const highlight::TokenStyle& style) const;

    highlight::Language language_ = highlight::Language::None;

    std::unique_ptr<highlight::Highlighter> highlighter_;

    /*
     * Токены в байтах UTF-8 и карта «байт -> позиция в QString»,
     * посчитанные для текущего текста документа в refresh().
     */
    std::vector<highlight::Token> tokens_;
    std::vector<int> byte_offsets_;

    /*
     * Слишком большие документы не подсвечиваются, чтобы ввод
     * текста оставался отзывчивым.
     */
    static constexpr int kMaxHighlightedLength = 200000;
};

#endif // CODE_HIGHLIGHTER_H