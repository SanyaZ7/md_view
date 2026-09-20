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
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QRegularExpression>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QTimer>
#include <QUrl>
#include <QPushButton>
#include <QGraphicsProxyWidget>
#include <QFontMetrics>

// --- Реализация MarkdownUtils ---

namespace MarkdownUtils {

QString markdown_text_to_html(const std::string& value) {
    QString text = QString::fromUtf8(value.data(), static_cast<int>(value.size()));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return text.toHtmlEscaped();
}

QString utf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString escape_html(const std::string& value) {
    return utf8(value).toHtmlEscaped();
}

QString alignment_to_css(Alignment alignment) {
    switch (alignment) {
        case Alignment::Center: return QStringLiteral("center");
        case Alignment::Right:  return QStringLiteral("right");
        default:                return QStringLiteral("left");
    }
}

QString inline_to_html(const MarkdownNode& node) {
    switch (node.type) {
        case NodeType::HardBreak:
            return QStringLiteral("<br/>");
        case NodeType::Text:
            return markdown_text_to_html(node.content);
        case NodeType::Bold:
            return QStringLiteral("<b>") + children_to_html(node) + QStringLiteral("</b>");
        case NodeType::Italic:
            return QStringLiteral("<i>") + children_to_html(node) + QStringLiteral("</i>");
        case NodeType::Strikethrough:
            return QStringLiteral("<s>") + children_to_html(node) + QStringLiteral("</s>");
        case NodeType::InlineCode:
            return QStringLiteral("<code style=\"background:#eeeeee;padding:2px 4px;font-family:'Noto Sans Mono';\">")
                   + escape_html(node.content) + QStringLiteral("</code>");
        case NodeType::Link: {
            QString link_text = node.children.empty() ? markdown_text_to_html(node.content) : children_to_html(node);
            return QStringLiteral("<a href=\"") + utf8(node.url).toHtmlEscaped() + QStringLiteral("\">") + link_text + QStringLiteral("</a>");
        }
        case NodeType::Image: {
            return QStringLiteral("<img src=\"%1\" alt=\"%2\" style=\"max-width:100%;\">").arg(utf8(node.url).toHtmlEscaped(), markdown_text_to_html(node.content));
        }
        default: break;
    }
    return escape_html(node.content);
}

QString children_to_html(const MarkdownNode& node) {
    QString result;
    for (const MarkdownNode& child : node.children) {
        result += inline_to_html(child);
    }
    return result;
}

QString list_to_html(const MarkdownNode& node) {
    const QString tag = node.ordered ? QStringLiteral("ol") : QStringLiteral("ul");
    QString result = QStringLiteral("<%1 style=\"margin-top:0\">").arg(tag);
    for (const MarkdownNode& item : node.children) {
        if (item.type != NodeType::ListItem) continue;
        result += QStringLiteral("<li>");
        for (const MarkdownNode& child : item.children) {
            if (child.type == NodeType::List) result += list_to_html(child);
            else result += inline_to_html(child);
        }
        result += QStringLiteral("</li>");
    }
    result += QStringLiteral("</%1>").arg(tag);
    return result;
}

QString table_to_html(const MarkdownNode& node) {
    QString result = QStringLiteral("<table border=\"1\" cellspacing=\"0\" cellpadding=\"5\" style=\"border-collapse:collapse\">");
    for (const MarkdownNode& row : node.children) {
        if (row.type != NodeType::TableRow) continue;
        result += QStringLiteral("<tr>");
        for (const MarkdownNode& cell : row.children) {
            if (cell.type != NodeType::TableCell) continue;
            const QString tag = row.is_header ? QStringLiteral("th") : QStringLiteral("td");
            result += QStringLiteral("<%1 align=\"%2\">").arg(tag, alignment_to_css(cell.alignment));
            result += cell.children.empty() ? escape_html(cell.content) : children_to_html(cell);
            result += QStringLiteral("</%1>").arg(tag);
        }
        result += QStringLiteral("</tr>");
    }
    result += QStringLiteral("</table>");
    return result;
}

QString node_to_html(const MarkdownNode& node, std::vector<QString>* code_list) {
    switch (node.type) {
        case NodeType::Heading: {
            const int level = qBound(1, node.level, 6);
            return QStringLiteral("<h%1>%2</h%1>").arg(level).arg(children_to_html(node));
        }
        case NodeType::Text:
            return QStringLiteral("<p>") + children_to_html(node) + QStringLiteral("</p>");
        case NodeType::Quote:
            return QStringLiteral("<blockquote style=\"border-left:4px solid #aaaaaa;margin:0 0 12px 0;padding-left:12px;color:#555555;\">")
                   + blocks_to_html(node.children, code_list) + QStringLiteral("</blockquote>");
        case NodeType::CodeBlock: {
            const int index = code_list ? static_cast<int>(code_list->size()) : -1;
            if (code_list) {
                code_list->push_back(utf8(node.content));
            }
            // Первая строка содержит невидимый маркер (U+2063), по числу
            // символов которого после установки HTML находится позиция
            // блока для размещения виджета-кнопки "Копировать".
            const QString marker = index >= 0
                ? QString(static_cast<int>(index) + 1, QChar(0x2063))
                : QString(QChar(0x2063));
            const QString content = node.content.empty() ? QStringLiteral("<br/>") : escape_html(node.content);
            return QStringLiteral("<pre style=\"background:#f3f3f3;border:1px solid #dddddd;padding:6px 10px;font-family:'Noto Sans Mono';white-space:pre-wrap;margin:0 0 12px 0;\">")
                   + marker + QStringLiteral("\n") + content + QStringLiteral("</pre>");
        }
        case NodeType::HorizontalRule:
            return QStringLiteral("<hr style=\"border:0;border-top:1px solid #aaaaaa;margin:12px 0;\">");
        case NodeType::List:
            return list_to_html(node);
        case NodeType::Table:
            return table_to_html(node);
        default:
            return children_to_html(node);
    }
}

QString blocks_to_html(const std::vector<MarkdownNode>& nodes, std::vector<QString>* code_list) {
    QString result;
    for (const MarkdownNode& child : nodes) {
        result += node_to_html(child, code_list);
    }
    return result;
}

QString sourceLineCandidate(const QString& source_line)
{
    QString value = source_line.trimmed();

    if (value.isEmpty()) {
        return QString();
    }

    value.replace(
        QRegularExpression(QStringLiteral("^\\s*(?:>\\s*)+")),
        QString()
    );

    value.replace(
        QRegularExpression(QStringLiteral("^\\s*#{1,6}\\s+")),
        QString()
    );

    value.replace(
        QRegularExpression(QStringLiteral("^\\s*(?:[-+*]|\\d+\\.)\\s+")),
        QString()
    );

    /*
     * Табличная строка.
     *
     * В rendered-документе символы "|" отсутствуют,
     * поэтому используем содержимое первой ячейки.
     */
    const bool is_table_line =
        value.startsWith(QLatin1Char('|')) ||
        value.endsWith(QLatin1Char('|'));

    if (is_table_line) {
        if (value.startsWith(QLatin1Char('|'))) {
            value.remove(0, 1);
        }

        if (value.endsWith(QLatin1Char('|'))) {
            value.chop(1);
        }

        const QStringList cells =
            value.split(QLatin1Char('|'), Qt::KeepEmptyParts);

        value.clear();

        for (const QString& raw_cell : cells) {
            const QString cell = raw_cell.trimmed();

            if (cell.isEmpty()) {
                continue;
            }

            /*
             * Разделитель таблицы:
             *
             * |---|---|
             * |:--|--:|
             */
            const bool is_separator =
                cell.contains(
                    QRegularExpression(
                        QStringLiteral("^:?-{3,}:?$")
                    )
                );

            if (!is_separator) {
                value = cell;
                break;
            }
        }

        /*
         * Строка-разделитель не имеет текстового представления
         * в QTextDocument.
         */
        if (value.isEmpty()) {
            return QString();
        }
    }

    value.replace(
        QRegularExpression(
            QStringLiteral("!\\[([^]]*)\\]\\([^)]*\\)")
        ),
        QStringLiteral("\\1")
    );

    value.replace(
        QRegularExpression(
            QStringLiteral("\\[([^]]+)\\]\\([^)]*\\)")
        ),
        QStringLiteral("\\1")
    );

    value.replace(QStringLiteral("**"), QString());
    value.replace(QStringLiteral("__"), QString());
    value.replace(QStringLiteral("~~"), QString());

    value.replace(
        QRegularExpression(QStringLiteral("(?<!\\*)\\*(?!\\*)")),
        QString()
    );

    value.replace(
        QRegularExpression(QStringLiteral("(?<!_)_(?!_)")),
        QString()
    );

    value.replace(QLatin1Char('`'), QString());

    return value.trimmed();
}


} // namespace MarkdownUtils


// --- Реализация MarkdownGraphicsView ---

MarkdownGraphicsView::MarkdownGraphicsView(QWidget* parent)
    : QGraphicsView(parent)
    , text_font_(QStringLiteral("Noto Sans"), 12)
    , code_font_(QStringLiteral("Noto Sans Mono"), 11)
    , render_mode_(false)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::TextAntialiasing, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setBackgroundBrush(palette().brush(QPalette::Base));
}

void MarkdownGraphicsView::setSourceText(const QString& text) {
    if (source_text_ == text) return;
    source_text_ = text;
    source_anchors_.clear();
}

void MarkdownGraphicsView::setDocument(const std::vector<MarkdownNode>& document) {
    document_ = document;
    render_document();
}

void MarkdownGraphicsView::setRenderMode(bool enabled) {
    render_mode_ = enabled;
}

bool MarkdownGraphicsView::renderMode() const {
    return render_mode_;
}

void MarkdownGraphicsView::clear_document() {
    if (scene()) scene()->clear();
}

void MarkdownGraphicsView::render_document() {
    QGraphicsScene* document_scene = scene();
    if (!document_scene) return;

    document_scene->clear();
    text_item_ = nullptr;
    source_anchors_.clear();

    const int viewport_width = qMax(300, viewport()->width());
    const qreal document_width = qMax(100.0, viewport_width - 32.0);

    auto* item = new QGraphicsTextItem();
    text_item_ = item;
    item->setFont(text_font_);
    item->setOpenExternalLinks(false);
    connect(item, &QGraphicsTextItem::linkActivated, this, [](const QString& link) {
        QDesktopServices::openUrl(QUrl(link));
    });

    std::vector<QString> code_list;

    QString html = QStringLiteral("<html><head><style>"
        "html, body { margin:0; padding:0; font-family:'Noto Sans','Noto Color Emoji','Segoe UI Emoji','Apple Color Emoji',sans-serif; font-size:12pt; }"
        "p { margin:0 0 12px 0; } h1, h2, h3, h4, h5, h6 { margin:12px 0 8px 0; } "
        "h1:first-child, h2:first-child, h3:first-child, h4:first-child, h5:first-child, h6:first-child { margin-top:0; }"
        "table { margin:0 0 12px 0; } blockquote { margin:0 0 12px 0; } pre { margin:0 0 12px 0; } "
        "ul, ol { margin-top:0; margin-bottom:12px; } hr { margin:12px 0; }"
        "</style></head><body>")
        + MarkdownUtils::blocks_to_html(document_, &code_list)
        + QStringLiteral("</body></html>");

    item->setHtml(html);
    item->setTextWidth(document_width);
    item->setPos(16.0, 16.0);

    const qreal document_height = item->document()->size().height();
    document_scene->addItem(item);
    document_scene->setSceneRect(0.0, 0.0, viewport_width, qMax(32.0 + document_height, static_cast<qreal>(viewport()->height())));

    createCopyButtons(code_list);
}

void MarkdownGraphicsView::createCopyButtons(const std::vector<QString>& codes) {
    copy_buttons_.clear();

    if (!text_item_ || !scene()) {
        return;
    }

    QTextDocument* document = text_item_->document();
    if (!document) {
        return;
    }

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QString text = block.text();
        if (text.isEmpty() || text.at(0) != QChar(0x2063)) {
            continue;
        }

        // Количество маркеров U+2063 равно index + 1.
        int index = 0;
        while (index < text.size() && text.at(index) == QChar(0x2063)) {
            ++index;
        }
        --index;

        if (index < 0 || index >= static_cast<int>(codes.size())) {
            continue;
        }

        const QRectF block_rect =
            document->documentLayout()->blockBoundingRect(block);
        const qreal y = block_rect.top();
        const qreal x = block_rect.left();

        auto* button = new QPushButton(tr("Копировать"));
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);

        QFont button_font(QStringLiteral("Noto Sans"));
        button_font.setPointSizeF(9.0);
        button->setFont(button_font);

        // Ширина рассчитана по более длинной надписи ("Скопировано"),
        // чтобы текст не обрезался при смене состояния.
        const QFontMetrics metrics(button_font);
        const int button_width =
            qMax(metrics.horizontalAdvance(tr("Копировать")),
                 metrics.horizontalAdvance(tr("Скопировано"))) + 20;
        button->setMinimumWidth(button_width);

        button->setStyleSheet(QStringLiteral(
            "QPushButton { background:#e6e6e6; border:1px solid #c4c4c4; border-radius:3px;"
            " padding:2px 8px; color:#333333; }"
            "QPushButton:hover { background:#dcdcdc; }"
            "QPushButton:pressed { background:#cfcfcf; }"));

        const QString code = codes[index];
        connect(button, &QPushButton::clicked, this, [code, button]() {
            QApplication::clipboard()->setText(code);
            button->setText(tr("Скопировано"));
            QTimer::singleShot(1500, button, [button]() {
                button->setText(tr("Копировать"));
            });
        });

        QGraphicsProxyWidget* proxy = scene()->addWidget(button);
        proxy->setPos(16.0 + x + 2.0, 16.0 + y + 4.0);

        copy_buttons_.push_back({button, proxy, code});
    }
}

void MarkdownGraphicsView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (event->size().width() != event->oldSize().width()) {
        render_document();
    }
}

void MarkdownGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);
    QAction *renderAction = menu.addAction(tr("Отрисовка md"));
    renderAction->setCheckable(true);
    renderAction->setChecked(render_mode_);
    connect(renderAction, &QAction::toggled, this, [this](bool checked) {
        render_mode_ = checked;
        emit renderModeRequested(checked);
    });
    menu.exec(event->globalPos());
}

void MarkdownGraphicsView::rebuildSourceAnchors() const
{
    source_anchors_.clear();

    if (!text_item_ || source_text_.isEmpty()) {
        return;
    }

    QTextDocument *text_document =
        text_item_->document();

    if (!text_document) {
        return;
    }

    const QStringList source_lines =
        source_text_.split(
            QRegularExpression(QStringLiteral("\\r\\n|\\r|\\n")),
            Qt::KeepEmptyParts
        );

    int search_position = 0;

    for (int line_number = 0;
         line_number < source_lines.size();
         ++line_number) {
        const QString candidate =
            MarkdownUtils::sourceLineCandidate(
                source_lines[line_number]
            );

        if (candidate.isEmpty()) {
            continue;
        }

        QTextCursor cursor =
            text_document->find(
                candidate,
                search_position
            );

        /*
         * Не перезапускаем поиск с начала документа.
         * Иначе одинаковый текст может привязаться к неправильному
         * месту и нарушить порядок якорей.
         */
        if (cursor.isNull()) {
            continue;
        }

        search_position = cursor.position();

        const QTextBlock block =
            cursor.block();

        if (!block.isValid()) {
            continue;
        }

        const qreal y =
            text_document->documentLayout()
                ->blockBoundingRect(block)
                .top();

        source_anchors_.push_back({
            line_number,
            y + 16.0
        });
    }
}


void MarkdownGraphicsView::scrollToSourceLine(int line)
{
    if (!text_item_) {
        return;
    }

    rebuildSourceAnchors();

    if (source_anchors_.empty()) {
        verticalScrollBar()->setValue(
            verticalScrollBar()->minimum()
        );
        return;
    }

    const SourceAnchor* best_anchor =
        &source_anchors_.front();

    /*
     * Выбираем якорь исходной строки, наиболее близкий к
     * запрошенной строке, а не только якорь с line <= line.
     */
    int best_distance =
        qAbs(best_anchor->line - line);

    for (const SourceAnchor& anchor : source_anchors_) {
        const int distance =
            qAbs(anchor.line - line);

        if (distance < best_distance) {
            best_anchor = &anchor;
            best_distance = distance;
        }
    }

    /*
     * Якорь задан в координатах сцены, поэтому переводим его
     * в положение вертикальной полосы прокрутки через текущую
     * координату верхней границы сцены.
     */
    const qreal current_scene_y =
        mapToScene(viewport()->rect().topLeft()).y();

    const qreal delta =
        best_anchor->y - current_scene_y;

    const int target_value =
        verticalScrollBar()->value() + qRound(delta);

    verticalScrollBar()->setValue(
        qBound(
            verticalScrollBar()->minimum(),
            target_value,
            verticalScrollBar()->maximum()
        )
    );
}

int MarkdownGraphicsView::sourceLineForCurrentScroll() const
{
    if (!text_item_) {
        return 0;
    }

    rebuildSourceAnchors();

    if (source_anchors_.empty()) {
        return 0;
    }

    const QPoint viewport_top_left =
        viewport()->rect().topLeft();

    const qreal current_y =
        mapToScene(viewport_top_left).y();

    const SourceAnchor *best_anchor =
        &source_anchors_.front();

    for (const SourceAnchor& anchor : source_anchors_) {
        if (anchor.y <= current_y) {
            best_anchor = &anchor;
        } else {
            break;
        }
    }

    return qMax(0, best_anchor->line);
}



