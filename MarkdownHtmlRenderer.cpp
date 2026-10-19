#include "MarkdownHtmlRenderer.h"

#include "HighlightHtml.h"
#include "Tex_AST_v2/math_renderer.h"

#include <algorithm>

namespace MarkdownUtils
{

QString markdown_text_to_html(const std::string& value)
{
    QString text = QString::fromUtf8(
        value.data(),
        static_cast<int>(value.size()));

    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

    return text.toHtmlEscaped();
}

QString utf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<int>(value.size()));
}

QString escape_html(const std::string& value)
{
    return utf8(value).toHtmlEscaped();
}

QString alignment_to_css(Alignment alignment)
{
    switch (alignment) {
        case Alignment::Center:
            return QStringLiteral("center");

        case Alignment::Right:
            return QStringLiteral("right");

        default:
            return QStringLiteral("left");
    }
}

QString navMarkerText(int source_line)
{
    /*
     * Маркер - забег WORD JOINER (U+2060, категория Cf) длиной
     * source_line + 1, закрытый терминатором FUNCTION APPLICATION
     * (U+2061, тоже Cf нулевой ширины).
     *
     * Оба символа не рисуются, не влияют на шрифт и layout, но
     * сохраняются в QTextDocument. Терминатор принципиален: при
     * FixedHeight соседние пустые блоки схлопываются, и тексты двух
     * маркеров склеиваются в один QTextBlock. Без терминатора два
     * забега U+2060 слились бы в один и дали неверную длину
     * (замер: 244 + 274 = 518 вместо ожидаемых 244). С терминатором
     * каждый забег остаётся различимым: считывается ровно тот, за
     * которым следует U+2061.
     */
    return QString(source_line + 1, QChar(0x2060))
        + QChar(0x2061);
}

QString navMarker(int source_line)
{
    /*
     * Навигационный маркер вставляется в HTML как "сырой" невидимый
     * символ, без обёртки span: форматные Cf-символы и так нулевой
     * ширины, а span с font-size:0 Qt в rich text не поддерживает.
     */
    return navMarkerText(source_line);
}

QString inline_to_html(const MarkdownNode& node)
{
    switch (node.type) {
        case NodeType::HardBreak:
            return QStringLiteral("<br/>");

        case NodeType::Text:
            return markdown_text_to_html(node.content);

        case NodeType::Bold:
            return QStringLiteral("<b>")
                + children_to_html(node)
                + QStringLiteral("</b>");

        case NodeType::MathInline: {
            const std::string html =
                math_ast::render_inline_formula(node.content);

            return QString::fromUtf8(
                html.data(),
                static_cast<int>(html.size()));
        }

        case NodeType::Italic:
            return QStringLiteral("<i>")
                + children_to_html(node)
                + QStringLiteral("</i>");

        case NodeType::Strikethrough:
            return QStringLiteral("<s>")
                + children_to_html(node)
                + QStringLiteral("</s>");

        case NodeType::InlineCode:
            return QStringLiteral(
                       "<code style=\"background:#eeeeee;"
                       "padding:2px 4px;"
                       "font-family:'Noto Sans Mono';\">")
                + escape_html(node.content)
                + QStringLiteral("</code>");

        case NodeType::Link: {
            const QString link_text =
                node.children.empty()
                    ? markdown_text_to_html(node.content)
                    : children_to_html(node);

            return QStringLiteral("<a href=\"")
                + utf8(node.url).toHtmlEscaped()
                + QStringLiteral("\">")
                + link_text
                + QStringLiteral("</a>");
        }

        case NodeType::Image:
            return QStringLiteral(
                       "<img src=\"%1\" alt=\"%2\" "
                       "style=\"max-width:100%;\">")
                .arg(
                    utf8(node.url).toHtmlEscaped(),
                    markdown_text_to_html(node.content));

        default:
            break;
    }

    return escape_html(node.content);
}

QString children_to_html(const MarkdownNode& node)
{
    QString result;

    for (const MarkdownNode& child : node.children) {
        result += inline_to_html(child);
    }

    return result;
}

QString list_to_html(const MarkdownNode& node,
                     const QString& nav_prefix)
{
    const QString tag =
        node.ordered
            ? QStringLiteral("ol")
            : QStringLiteral("ul");

    QString result =
        QStringLiteral("<%1 style=\"margin-top:0\">").arg(tag);

    bool nav_placed = false;

    for (const MarkdownNode& item : node.children) {
        if (item.type != NodeType::ListItem) {
            continue;
        }

        result += QStringLiteral("<li>");

        /*
         * Маркер навигации помещается в первый элемент списка,
         * чтобы его QTextBlock соответствовал началу списка.
         */
        if (!nav_placed && !nav_prefix.isEmpty()) {
            result += nav_prefix;
            nav_placed = true;
        }

        for (const MarkdownNode& child : item.children) {
            if (child.type == NodeType::List) {
                result += list_to_html(child);
            } else {
                result += inline_to_html(child);
            }
        }

        result += QStringLiteral("</li>");
    }

    result += QStringLiteral("</%1>").arg(tag);

    return result;
}

QString table_to_html(const MarkdownNode& node,
                      const QString& nav_prefix)
{
    QString result =
        QStringLiteral(
            "<table border=\"1\" cellspacing=\"0\" cellpadding=\"5\" "
            "style=\"border-collapse:collapse\">");

    bool nav_placed = false;

    for (const MarkdownNode& row : node.children) {
        if (row.type != NodeType::TableRow) {
            continue;
        }

        result += QStringLiteral("<tr>");

        for (const MarkdownNode& cell : row.children) {
            if (cell.type != NodeType::TableCell) {
                continue;
            }

            const QString tag =
                row.is_header
                    ? QStringLiteral("th")
                    : QStringLiteral("td");

            result += QStringLiteral("<%1 align=\"%2\">")
                .arg(tag, alignment_to_css(cell.alignment));

            /*
             * Маркер навигации помещается в первую ячейку,
             * чтобы его QTextBlock соответствовал началу таблицы.
             */
            if (!nav_placed && !nav_prefix.isEmpty()) {
                result += nav_prefix;
                nav_placed = true;
            }

            result +=
                cell.children.empty()
                    ? escape_html(cell.content)
                    : children_to_html(cell);

            result += QStringLiteral("</%1>").arg(tag);
        }

        result += QStringLiteral("</tr>");
    }

    result += QStringLiteral("</table>");

    return result;
}

QString node_to_html(
    const MarkdownNode& node,
    std::vector<CodeBlockInfo>* code_list,
    const RenderOptions& options,
    const QString& nav_prefix)
{
    switch (node.type) {
        case NodeType::Heading: {
            const int level = qBound(1, node.level, 6);

            return QStringLiteral("<h%1>%2%3</h%1>")
                .arg(level)
                .arg(nav_prefix)
                .arg(children_to_html(node));
        }

        case NodeType::MathBlock: {
            const std::string html =
                math_ast::render_block_formula(node.content);

            return nav_prefix + QString::fromUtf8(
                html.data(),
                static_cast<int>(html.size()));
        }

        case NodeType::Text:
            return QStringLiteral("<p>")
                + nav_prefix
                + children_to_html(node)
                + QStringLiteral("</p>");

        case NodeType::Quote:
            return QStringLiteral(
                       "<blockquote style=\"border-left:4px solid #aaaaaa;"
                       "margin:0 0 12px 0;"
                       "padding-left:12px;"
                       "color:#555555;\">")
                + nav_prefix
                + blocks_to_html(
                    node.children,
                    code_list,
                    options)
                + QStringLiteral("</blockquote>");

        case NodeType::FencedMarkdown:
            return nav_prefix + blocks_to_html(
                node.children,
                code_list,
                options);

        case NodeType::CodeBlock: {
            const QString code = utf8(node.content);

            const int index =
                code_list
                    ? static_cast<int>(code_list->size())
                    : -1;

            if (code_list) {
                code_list->push_back({code, node.language});
            }

            /*
             * Маркер используется для поиска QTextBlock, к которому
             * будет привязан QPlainTextEdit и кнопка копирования.
             *
             * Число символов U+2063 равно index + 1.
             */
            const QString marker =
                index >= 0
                    ? QString(
                          index + 1,
                          QChar(0x2063))
                    : QString(QChar(0x2063));

            const int line_count =
                1 + static_cast<int>(
                    std::count(
                        node.content.begin(),
                        node.content.end(),
                        '\n'));

            /*
             * Для длинного блока оставляем только один QTextBlock
             * с маркером. Его высота позднее фиксируется в
             * applyCodeBlockHeights().
             *
             * Раньше после маркера добавлялся отдельный div-placeholder.
             * В результате высота строки маркера дополнительно
             * попадала в layout и образовывала пустой зазор.
             */
            if (index >= 0 &&
                options.codeBlockMaxLines > 0 &&
                line_count > options.codeBlockMaxLines) {
                return QStringLiteral(
                           "<div style=\"background:#f3f3f3;"
                           "border:1px solid #dddddd;"
                           "padding:0;"
                           "margin:0 0 12px 0;"
                           "font-family:'Noto Sans Mono';\">")
                    + marker
                    + nav_prefix
                    + QStringLiteral("</div>");
            }

            const QString content =
                node.content.empty()
                    ? QStringLiteral("<br/>")
                    : highlight_code_html(
                          node.content,
                          highlight::language_from_name(
                              node.language));

            return QStringLiteral(
                       "<pre style=\"background:#f3f3f3;"
                       "border:1px solid #dddddd;"
                       "padding:6px 10px;"
                       "font-family:'Noto Sans Mono';"
                       "white-space:pre-wrap;"
                       "margin:0 0 12px 0;\">")
                + marker
                + nav_prefix
                + QStringLiteral("\n")
                + content
                + QStringLiteral("</pre>");
        }

        case NodeType::HorizontalRule:
            return nav_prefix + QStringLiteral(
                "<hr style=\"border:0;"
                "border-top:1px solid #aaaaaa;"
                "margin:12px 0;\">");

        case NodeType::List:
            return list_to_html(node, nav_prefix);

        case NodeType::Table:
            return table_to_html(node, nav_prefix);

        default:
            return nav_prefix + children_to_html(node);
    }
}

QString blocks_to_html(
    const std::vector<MarkdownNode>& nodes,
    std::vector<CodeBlockInfo>* code_list,
    const RenderOptions& options,
    bool with_nav)
{
    QString result;

    for (const MarkdownNode& child : nodes) {
        const QString nav_prefix =
            (with_nav && child.source_start_line >= 0)
                ? navMarker(child.source_start_line)
                : QString();

        result += node_to_html(
            child,
            code_list,
            options,
            nav_prefix);
    }

    return result;
}

} // namespace MarkdownUtils

