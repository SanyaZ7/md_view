#ifndef MARKDOWN_HTML_RENDERER_H
#define MARKDOWN_HTML_RENDERER_H

#include "MarkdownNode.h"

#include <QString>

#include <string>
#include <vector>

namespace MarkdownUtils
{
    struct RenderOptions
    {
        // 0 означает отсутствие ограничения высоты блока кода.
        int codeBlockMaxLines = 0;
    };

    /*
     * Блок кода, попавший в HTML: содержимое и язык из info string.
     * Порядок элементов в списке задаёт индекс, закодированный в
     * marker-строке QTextBlock (см. node_to_html).
     */
    struct CodeBlockInfo
    {
        QString code;
        std::string language;
    };

    QString markdown_text_to_html(const std::string& value);
    QString utf8(const std::string& value);
    QString escape_html(const std::string& value);
    QString alignment_to_css(Alignment alignment);

    QString inline_to_html(const MarkdownNode& node);
    QString children_to_html(const MarkdownNode& node);
    QString list_to_html(
        const MarkdownNode& node,
        const QString& nav_prefix = QString());
    QString table_to_html(
        const MarkdownNode& node,
        const QString& nav_prefix = QString());

    /*
     * nav_prefix - необязательный невидимый маркер навигации,
     * который вставляется ПЕРЕД блочным тегом (как соседний
     * inline-элемент, а не обёртка). Заполняется только для
     * верхнеуровневых узлов с заданной исходной строкой.
     */
    QString node_to_html(
        const MarkdownNode& node,
        std::vector<CodeBlockInfo>* code_list = nullptr,
        const RenderOptions& options = RenderOptions(),
        const QString& nav_prefix = QString());

    QString blocks_to_html(
        const std::vector<MarkdownNode>& nodes,
        std::vector<CodeBlockInfo>* code_list = nullptr,
        const RenderOptions& options = RenderOptions(),
        bool with_nav = false);

    /*
     * Невидимый маркер навигации, однозначно соответствующий
     * исходной строке блока. Символы из private use area
     * (U+E800..) оборачиваются в span с font-size:0, поэтому
     * маркер невидим и не влияет на layout, но сохраняется в
     * QTextDocument и находится через QTextDocument::find().
     */
    QString navMarker(int source_line);

    /*
     * Текстовая (без HTML) форма маркера навигации. Именно эту
     * строку ищут в QTextDocument через QTextDocument::find().
     */
    QString navMarkerText(int source_line);
}

#endif // MARKDOWN_HTML_RENDERER_H

