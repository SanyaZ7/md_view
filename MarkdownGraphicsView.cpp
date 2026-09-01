#include "MarkdownGraphicsView.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QFontDatabase>
#include <QPalette>
#include <QResizeEvent>

namespace {

QString markdown_text_to_html(
    const std::string& value
) {
    QString text =
        QString::fromUtf8(
            value.data(),
            static_cast<int>(value.size())
        );

    /*
     * Декодируем HTML-сущности Markdown-текста
     * до последующего HTML-экранирования.
     *
     * Порядок важен: &amp; обрабатывается последней,
     * чтобы &amp;lt; превратилось в отображаемый текст
     * "&lt;", а не в символ "<".
     */
    text.replace(
        QStringLiteral("&lt;"),
        QStringLiteral("<")
    );

    text.replace(
        QStringLiteral("&gt;"),
        QStringLiteral(">")
    );

    text.replace(
        QStringLiteral("&quot;"),
        QStringLiteral("\"")
    );

    text.replace(
        QStringLiteral("&#39;"),
        QStringLiteral("'")
    );

    text.replace(
        QStringLiteral("&apos;"),
        QStringLiteral("'")
    );

    text.replace(
        QStringLiteral("&amp;"),
        QStringLiteral("&")
    );

    return text.toHtmlEscaped();
}


QString utf8(
    const std::string& value
) {
    return QString::fromUtf8(
        value.data(),
        static_cast<int>(value.size())
    );
}

QString escape_html(
    const std::string& value
) {
    return utf8(value).toHtmlEscaped();
}

QString alignment_to_css(
    Alignment alignment
) {
    switch (alignment) {
        case Alignment::Center:
            return QStringLiteral("center");

        case Alignment::Right:
            return QStringLiteral("right");

        case Alignment::Left:
        default:
            return QStringLiteral("left");
    }
}

} // namespace


MarkdownGraphicsView::MarkdownGraphicsView(
    QWidget* parent
)
    : QGraphicsView(parent)
    , text_font_(
          QStringLiteral("Noto Sans"),
          12)
    , code_font_(
          QStringLiteral("Noto Sans Mono"),
          11)
    , render_mode_(false)
{
    setScene(new QGraphicsScene(this));

    setRenderHint(
        QPainter::Antialiasing,
        false);

    setRenderHint(
        QPainter::TextAntialiasing,
        true);

    setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);

    setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded);

    setAlignment(
        Qt::AlignLeft | Qt::AlignTop);

    setBackgroundBrush(
        palette().brush(QPalette::Base));
}

QString MarkdownGraphicsView::inline_to_html(
    const MarkdownNode& node
) const
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

        case NodeType::Italic:
            return QStringLiteral("<i>")
                + children_to_html(node)
                + QStringLiteral("</i>");

        case NodeType::Strikethrough:
            return QStringLiteral("<s>")
                + children_to_html(node)
                + QStringLiteral("</s>");

        case NodeType::InlineCode:
            /*
             * В коде HTML-сущности не декодируем:
             * `&lt;` в inline-коде должно оставаться
             * именно текстом `&lt;`.
             */
            return QStringLiteral(
                "<code style=\""
                "background:#eeeeee;"
                "padding:2px 4px;"
                "font-family:'Noto Sans Mono';"
                "\">"
            )
            + escape_html(node.content)
            + QStringLiteral("</code>");

        case NodeType::Link: {
    		QString link_text;

    	if (!node.children.empty()) {
     	   link_text = children_to_html(node);
    		} else {
       		 link_text = markdown_text_to_html(node.content);
    		}

    return QStringLiteral("<a href=\"")
        + utf8(node.url).toHtmlEscaped()
        + QStringLiteral("\">")
        + link_text
        + QStringLiteral("</a>");
}


        case NodeType::Image: {
    const QString alt =
        markdown_text_to_html(node.content);

    const QString source =
        utf8(node.url).toHtmlEscaped();

    return QStringLiteral(
        "<img src=\"%1\" "
        "alt=\"%2\" "
        "style=\"max-width:100%;\">"
    )
    .arg(source, alt);
}


        default:
            break;
    }

    return escape_html(node.content);
}


QString MarkdownGraphicsView::children_to_html(
    const MarkdownNode& node
) const {
    QString result;

    for (const MarkdownNode& child : node.children) {
        result += inline_to_html(child);
    }

    return result;
}


QString MarkdownGraphicsView::list_to_html(
    const MarkdownNode& node
) const {
    const QString tag =
        node.ordered
            ? QStringLiteral("ol")
            : QStringLiteral("ul");

    QString result =
        QStringLiteral("<%1 style=\"margin-top:0\">")
            .arg(tag);

    for (const MarkdownNode& item : node.children) {
        if (item.type != NodeType::ListItem) {
            continue;
        }

        result += QStringLiteral("<li>");

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


QString MarkdownGraphicsView::table_to_html(
    const MarkdownNode& node
) const {
    QString result =
        QStringLiteral(
            "<table border=\"1\" "
            "cellspacing=\"0\" "
            "cellpadding=\"5\" "
            "style=\"border-collapse:collapse\">"
        );

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
                .arg(tag)
                .arg(alignment_to_css(cell.alignment));

            if (!cell.children.empty()) {
                result += children_to_html(cell);
            } else {
                result += escape_html(cell.content);
            }

            result += QStringLiteral("</%1>").arg(tag);
        }

        result += QStringLiteral("</tr>");
    }

    result += QStringLiteral("</table>");

    return result;
}

QString MarkdownGraphicsView::node_to_html(
    const MarkdownNode& node
) const
{
    switch (node.type) {
        case NodeType::Heading: {
            const int level =
                qBound(1, node.level, 6);

            return QStringLiteral("<h%1>")
                .arg(level)
                + children_to_html(node)
                + QStringLiteral("</h%1>")
                    .arg(level);
        }

        case NodeType::Text:
            return QStringLiteral("<p>")
                + children_to_html(node)
                + QStringLiteral("</p>");

        case NodeType::Quote:
            /*
             * Цитата содержит полноценные блочные узлы:
             * абзацы, заголовки, списки, таблицы и т. д.
             * Поэтому здесь нужен blocks_to_html(), а не
             * children_to_html().
             */
            return QStringLiteral(
                "<blockquote style=\""
                "border-left:4px solid #aaaaaa;"
                "margin:0 0 12px 0;"
                "padding-left:12px;"
                "color:#555555;"
                "\">"
            )
            + blocks_to_html(node.children)
            + QStringLiteral("</blockquote>");

        case NodeType::CodeBlock: {
            /*
             * Пустой pre без содержимого может иметь нулевую
             * высоту в QTextDocument. Тег <br/> сохраняет
             * видимую строку пустого fenced-блока.
             */
            const QString content =
                node.content.empty()
                    ? QStringLiteral("<br/>")
                    : escape_html(node.content);

            return QStringLiteral(
                "<pre style=\""
                "background:#f3f3f3;"
                "border:1px solid #dddddd;"
                "padding:10px;"
                "font-family:'Noto Sans Mono';"
                "white-space:pre-wrap;"
                "margin:0 0 12px 0;"
                "\">"
            )
            + content
            + QStringLiteral("</pre>");
        }

        case NodeType::HorizontalRule:
            return QStringLiteral(
                "<hr style=\""
                "border:0;"
                "border-top:1px solid #aaaaaa;"
                "margin:12px 0;"
                "\">"
            );

        case NodeType::List:
            return list_to_html(node);

        case NodeType::Table:
            return table_to_html(node);

        default:
            return children_to_html(node);
    }
}


void MarkdownGraphicsView::clear_document()
{
    QGraphicsScene* document_scene = scene();

    if (!document_scene) {
        return;
    }

    document_scene->clear();
}

void MarkdownGraphicsView::setDocument(
    const std::vector<MarkdownNode>& document
)
{
    document_ = document;
    render_document();
}


void MarkdownGraphicsView::setRenderMode(bool enabled)
{
    render_mode_ = enabled;
}

bool MarkdownGraphicsView::renderMode() const
{
    return render_mode_;
}

void MarkdownGraphicsView::contextMenuEvent(
    QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *renderAction =
        menu.addAction(tr("Отрисовка md"));

    renderAction->setCheckable(true);
    renderAction->setChecked(render_mode_);

    connect(renderAction, &QAction::toggled,
            this,
            [this](bool checked) {
                render_mode_ = checked;
                emit renderModeRequested(checked);
            });

    menu.exec(event->globalPos());
}

void MarkdownGraphicsView::render_document()
{
    QGraphicsScene* document_scene = scene();

    if (!document_scene) {
        return;
    }

    document_scene->clear();

    const int viewport_width =
        qMax(300, viewport()->width());

    const qreal document_width =
        qMax(100.0, viewport_width - 32.0);

    auto* item = new QGraphicsTextItem();

    item->setFont(text_font_);
    item->setOpenExternalLinks(true);

    const QString html =
        QStringLiteral(
            "<html>"
            "<head>"
            "<style>"
            "html, body {"
                "margin:0;"
                "padding:0;"
                "font-family:'Noto Sans',"
                    "'Noto Color Emoji',"
                    "'Segoe UI Emoji',"
                    "'Apple Color Emoji',"
                    "sans-serif;"
                "font-size:12pt;"
            "}"

            "p {"
                "margin:0 0 12px 0;"
            "}"

            "h1, h2, h3, h4, h5, h6 {"
                "margin:12px 0 8px 0;"
            "}"

            "h1:first-child, "
            "h2:first-child, "
            "h3:first-child, "
            "h4:first-child, "
            "h5:first-child, "
            "h6:first-child {"
                "margin-top:0;"
            "}"

            "table {"
                "margin:0 0 12px 0;"
            "}"

            "blockquote {"
                "margin:0 0 12px 0;"
            "}"

            "pre {"
                "margin:0 0 12px 0;"
            "}"

            "ul, ol {"
                "margin-top:0;"
                "margin-bottom:12px;"
            "}"

            "hr {"
                "margin:12px 0;"
            "}"
            "</style>"
            "</head>"
            "<body>"
        )
        + blocks_to_html(document_)
        + QStringLiteral(
            "</body>"
            "</html>"
        );

    item->setHtml(html);

    /*
     * Устанавливаем ширину после HTML.
     * После этого QTextDocument пересчитывает переносы
     * и высоту всего документа.
     */
    item->setTextWidth(document_width);

    const qreal document_height =
        item->document()->size().height();

    item->setPos(16.0, 16.0);

    document_scene->addItem(item);

    document_scene->setSceneRect(
        0.0,
        0.0,
        viewport_width,
        qMax(
            32.0 + document_height,
            static_cast<qreal>(viewport()->height())
        )
    );
}


void MarkdownGraphicsView::resizeEvent(
    QResizeEvent* event
)
{
    QGraphicsView::resizeEvent(event);

    if (event->size().width() ==
        event->oldSize().width()) {
        return;
    }

    render_document();
}

QString MarkdownGraphicsView::blocks_to_html(
    const std::vector<MarkdownNode>& nodes
) const {
    QString result;

    for (const MarkdownNode& child : nodes) {
        result += node_to_html(child);
    }

    return result;
}

