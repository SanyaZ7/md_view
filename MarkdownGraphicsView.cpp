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
#include <QPlainTextEdit>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QTextBlockFormat>
#include <cmath>
#include <algorithm>

// Геометрия прокручиваемых блоков кода (в px).
static const int kCodeBlockPadding   = 6;
static const int kCodeBlockBorder    = 1;
static const int kCodeBlockVSpacing  = 2;

// Текущее ограничение высоты блоков кода (0 = без ограничения)
// и высота строки кода; устанавливаются перед вызовом blocks_to_html().
static int   g_code_block_max_lines  = 0;
static qreal g_code_line_height      = 0.0;

// Высота виджета кода, вмещающего заданное число строк.
//
// QTextDocument игнорирует CSS-свойство height у блочных элементов,
// поэтому место под блок резервируется через QTextBlockFormat::FixedHeight
// (см. applyCodeBlockHeights()). Высота виджета и высота блока
// рассчитываются по одной и той же формуле, чтобы виджет не наезжал
// на следующий за блоком текст.
//
// По замерам QPlainTextEdit реальная высота строки равна lineSpacing + 1,
// а рамка, внутренние отступы и margin документа суммарно дают 15 px.
static const int kCodeBlockChrome = 15;

// Высота отдельной области для кнопки «Копировать».
// Она резервируется внутри каждого блока кода, чтобы кнопка
// не перекрывала первую строку кода.
static const int kCodeBlockButtonArea = 28;

static int codeBlockWidgetHeight(int lines, qreal line_height)
{
    const qreal line = line_height + 1.0;
    return static_cast<int>(std::ceil(lines * line)) + kCodeBlockChrome;
}

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

QString node_to_html(
    const MarkdownNode& node,
    std::vector<QString>* code_list)
{
    switch (node.type) {
        case NodeType::Heading: {
            const int level = qBound(1, node.level, 6);

            return QStringLiteral("<h%1>%2</h%1>")
                .arg(level)
                .arg(children_to_html(node));
        }

        case NodeType::Text:
            return QStringLiteral("<p>")
                + children_to_html(node)
                + QStringLiteral("</p>");

        case NodeType::Quote:
            return QStringLiteral(
                       "<blockquote style=\"border-left:4px solid #aaaaaa;"
                       "margin:0 0 12px 0;padding-left:12px;color:#555555;\">")
                + blocks_to_html(node.children, code_list)
                + QStringLiteral("</blockquote>");

        case NodeType::CodeBlock: {
            const QString code = utf8(node.content);

            const int index =
                code_list
                    ? static_cast<int>(code_list->size())
                    : -1;

            if (code_list) {
                code_list->push_back(code);
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
                          static_cast<int>(index) + 1,
                          QChar(0x2063)
                      )
                    : QString(QChar(0x2063));

            const int line_count =
                1 + static_cast<int>(
                    std::count(
                        node.content.begin(),
                        node.content.end(),
                        '\n'
                    )
                );

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
                g_code_block_max_lines > 0 &&
                line_count > g_code_block_max_lines) {
                return QStringLiteral(
                           "<div style=\"background:#f3f3f3;"
                           "border:1px solid #dddddd;"
                           "padding:0;"
                           "margin:0 0 12px 0;"
                           "font-family:'Noto Sans Mono';\">")
                    + marker
                    + QStringLiteral("</div>");
            }

            const QString content =
                node.content.empty()
                    ? QStringLiteral("<br/>")
                    : escape_html(node.content);

            return QStringLiteral(
                       "<pre style=\"background:#f3f3f3;"
                       "border:1px solid #dddddd;"
                       "padding:6px 10px;"
                       "font-family:'Noto Sans Mono';"
                       "white-space:pre-wrap;"
                       "margin:0 0 12px 0;\">")
                + marker
                + QStringLiteral("\n")
                + content
                + QStringLiteral("</pre>");
        }

        case NodeType::HorizontalRule:
            return QStringLiteral(
                "<hr style=\"border:0;border-top:1px solid #aaaaaa;"
                "margin:12px 0;\">"
            );

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
    // Высота строки моноширинного шрифта для расчёта высоты плейсхолдера.
    code_line_height_ = QFontMetrics(code_font_).lineSpacing();

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

void MarkdownGraphicsView::setCodeBlockMaxLines(int lines) {
    code_block_max_lines_ = qMax(0, lines);
}

int MarkdownGraphicsView::codeBlockMaxLines() const {
    return code_block_max_lines_;
}

bool MarkdownGraphicsView::renderMode() const {
    return render_mode_;
}

void MarkdownGraphicsView::clear_document() {
    if (scene()) scene()->clear();
}

void MarkdownGraphicsView::render_document()
{
    QGraphicsScene* document_scene = scene();

    if (!document_scene) {
        return;
    }

    code_blocks_.clear();
    active_code_block_ = nullptr;
    document_scene->clear();
    text_item_ = nullptr;
    source_anchors_.clear();

    const int viewport_width =
        qMax(300, viewport()->width());

    const qreal document_width =
        qMax(
            100.0,
            static_cast<qreal>(viewport_width) - 32.0
        );

    auto* item = new QGraphicsTextItem();

    text_item_ = item;

    item->setFont(text_font_);
    item->setOpenExternalLinks(false);

    connect(
        item,
        &QGraphicsTextItem::linkActivated,
        this,
        [](const QString& link) {
            QDesktopServices::openUrl(QUrl(link));
        }
    );

    std::vector<QString> code_list;

    g_code_block_max_lines = code_block_max_lines_;
    g_code_line_height = code_line_height_;

    /*
     * Для обычного <pre> место под кнопку создаётся CSS-отступом.
     * FixedHeight применяется только к marker-блокам длинного кода.
     */
    const QString html =
        QStringLiteral(
            "<html><head><style>"
            "html, body {"
            " margin:0;"
            " padding:0;"
            " font-family:'Noto Sans','Noto Color Emoji','Segoe UI Emoji','Apple Color Emoji',sans-serif;"
            " font-size:12pt;"
            "}"
            "p { margin:0 0 12px 0; }"
            "h1, h2, h3, h4, h5, h6 { margin:12px 0 8px 0; }"
            "h1:first-child, h2:first-child, h3:first-child, h4:first-child, h5:first-child, h6:first-child { margin-top:0; }"
            "table { margin:0 0 12px 0; }"
            "blockquote { margin:0 0 12px 0; }"
            "pre {"
            " margin:0 0 12px 0;"
            " padding-top:%1px !important;"
            "}"
            "ul, ol { margin-top:0; margin-bottom:12px; }"
            "hr { margin:12px 0; }"
            "</style></head><body>"
        )
        .arg(kCodeBlockButtonArea + kCodeBlockPadding)
        + MarkdownUtils::blocks_to_html(document_, &code_list)
        + QStringLiteral("</body></html>");

    item->setHtml(html);
    item->setTextWidth(document_width);

    /*
     * FixedHeight используется только у placeholder-блоков
     * длинного кода, заменяемых QPlainTextEdit.
     */
    applyCodeBlockHeights(code_list);
	if (code_block_max_lines_ > 0) {
    item->setTextWidth(-1.0);
    item->setTextWidth(document_width);
	}
    /*
     * Принудительно завершаем layout QTextDocument до получения
     * координат QTextBlock и создания QGraphicsProxyWidget.
     */
    QTextDocument* document = item->document();

    if (document && document->documentLayout()) {
        document->documentLayout()->documentSize();
    }

    item->setPos(16.0, 16.0);
    document_scene->addItem(item);

    createCopyButtons(code_list);
    createScrollableCodeBlocks(code_list);

    const auto update_scene_rect =
        [this, item, viewport_width]() {
            if (!scene() || text_item_ != item) {
                return;
            }

            QTextDocument* current_document = item->document();

            if (!current_document ||
                !current_document->documentLayout()) {
                return;
            }

            const QSizeF document_size =
                current_document->documentLayout()->documentSize();

            qreal content_bottom = 0.0;

            /*
             * В обычном режиме сохраняем старый путь расчёта:
             * в нём QGraphicsTextItem является единственным
             * источником геометрии текста и дефектов нет.
             */
            if (code_block_max_lines_ <= 0) {
                const QRectF text_rect =
                    item->sceneBoundingRect();

                const QRectF items_rect =
                    scene()->itemsBoundingRect();

                content_bottom =
                    qMax(
                        text_rect.bottom(),
                        items_rect.bottom()
                    );
            } else {
                /*
                 * При FixedHeight геометрия QGraphicsTextItem может
                 * отставать от реального QTextDocument. Используем
                 * фактическую высоту layout, а proxy-виджеты учитываем
                 * отдельно. Сам text_item_ намеренно исключён из
                 * itemsBoundingRect(), чтобы его устаревшая граница
                 * не добавляла пустую область внизу.
                 */
                content_bottom =
                    item->mapToScene(
                        QPointF(0.0, document_size.height())
                    ).y();

                const QList<QGraphicsItem*> scene_items =
                    scene()->items();

                for (QGraphicsItem* scene_item : scene_items) {
                    if (!scene_item || scene_item == item) {
                        continue;
                    }

                    content_bottom =
                        qMax(
                            content_bottom,
                            scene_item->sceneBoundingRect().bottom()
                        );
                }
            }

            const qreal scene_height =
                qMax(
                    static_cast<qreal>(viewport()->height()),
                    content_bottom + 16.0
                );

            scene()->setSceneRect(
                0.0,
                0.0,
                viewport_width,
                qMax(32.0, scene_height)
            );
        };

    update_scene_rect();

    /*
     * QTextDocument может закончить пересчёт геометрии уже после
     * выхода из render_document().
     */
    connect(
        document->documentLayout(),
        &QAbstractTextDocumentLayout::documentSizeChanged,
        this,
        [update_scene_rect](const QSizeF&) {
            update_scene_rect();
        }
    );

    /*
     * Proxy-виджеты могут уточнить геометрию только после добавления
     * в QGraphicsScene. Повторяем расчёт в следующем event loop.
     */
    QTimer::singleShot(
        0,
        this,
        [update_scene_rect]() {
            update_scene_rect();
        }
    );
}

void MarkdownGraphicsView::createCopyButtons(
    const std::vector<QString>& codes)
{
    copy_buttons_.clear();

    if (!text_item_ || !scene()) {
        return;
    }

    QTextDocument* document = text_item_->document();
    if (!document) {
        return;
    }

    for (QTextBlock block = document->begin();
         block.isValid();
         block = block.next()) {
        const QString text = block.text();

        if (text.isEmpty() || text.at(0) != QChar(0x2063)) {
            continue;
        }

        // Количество маркеров U+2063 равно index + 1.
        int index = 0;

        while (index < text.size() &&
               text.at(index) == QChar(0x2063)) {
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
            qMax(
                metrics.horizontalAdvance(tr("Копировать")),
                metrics.horizontalAdvance(tr("Скопировано"))
            ) + 20;

        button->setMinimumWidth(button_width);

        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            " background:#e6e6e6;"
            " border:1px solid #c4c4c4;"
            " border-radius:3px;"
            " padding:2px 8px;"
            " color:#333333;"
            "}"
            "QPushButton:hover {"
            " background:#dcdcdc;"
            "}"
            "QPushButton:pressed {"
            " background:#cfcfcf;"
            "}"
        ));

        const QString code = codes[index];

        connect(button,
                &QPushButton::clicked,
                this,
                [code, button]() {
                    QApplication::clipboard()->setText(code);

                    button->setText(tr("Скопировано"));

                    QTimer::singleShot(
                        1500,
                        button,
                        [button]() {
                            button->setText(tr("Копировать"));
                        }
                    );
                });

        QGraphicsProxyWidget* proxy = scene()->addWidget(button);

        proxy->setPos(
            16.0 + x + 2.0,
            16.0 + y + 4.0
        );

        /*
         * Длинный CodeBlockEditor размещается на z = 1.
         * Кнопка должна быть выше него, иначе редактор кода
         * перекрывает кнопку в нижних/длинных code-block.
         */
        proxy->setZValue(2.0);

        copy_buttons_.push_back({button, proxy, code});
    }
}

void MarkdownGraphicsView::createScrollableCodeBlocks(
    const std::vector<QString>& codes)
{
    if (code_block_max_lines_ <= 0 || !text_item_ || !scene()) {
        return;
    }

    QTextDocument* document = text_item_->document();

    if (!document) {
        return;
    }

    const qreal document_width = text_item_->textWidth();

    const qreal code_width =
        qMax<qreal>(
            100.0,
            document_width -
                2.0 * (kCodeBlockPadding + kCodeBlockBorder)
        );

    const int code_height =
        codeBlockWidgetHeight(
            code_block_max_lines_,
            code_line_height_
        );

    for (QTextBlock block = document->begin();
         block.isValid();
         block = block.next()) {
        const QString text = block.text();

        if (text.isEmpty() || text.at(0) != QChar(0x2063)) {
            continue;
        }

        int index = 0;

        while (index < text.size() &&
               text.at(index) == QChar(0x2063)) {
            ++index;
        }

        --index;

        if (index < 0 || index >= static_cast<int>(codes.size())) {
            continue;
        }

        const QString& code = codes[index];

        const int line_count =
            1 + static_cast<int>(
                std::count(
                    code.begin(),
                    code.end(),
                    QLatin1Char('\n')
                )
            );

        if (line_count <= code_block_max_lines_) {
            continue;
        }

        /*
         * Высота marker-block состоит из:
         *
         *   1. области под кнопку;
         *   2. области под QPlainTextEdit.
         *
         * Сам редактор начинается ниже кнопки и больше не перекрывает её.
         */
        const QRectF block_rect =
            document->documentLayout()->blockBoundingRect(block);

        auto* editor = new CodeBlockEditor(this);

        editor->setPlainText(code);
        editor->setReadOnly(true);
        editor->setFont(code_font_);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        editor->setFrameShape(QPlainTextEdit::StyledPanel);

        editor->setStyleSheet(
            QStringLiteral(
                "QPlainTextEdit {"
                " background:#f3f3f3;"
                " border:1px solid #dddddd;"
                " padding:%1px %2px;"
                " color:#333333;"
                "}"
            ).arg(kCodeBlockVSpacing).arg(kCodeBlockPadding)
        );

        editor->setFixedSize(
            static_cast<int>(std::ceil(code_width)),
            code_height
        );

        QGraphicsProxyWidget* proxy =
            scene()->addWidget(editor);

        proxy->setPos(
            16.0 + block_rect.left(),
            16.0 + block_rect.top() + kCodeBlockButtonArea
        );

        proxy->setZValue(1.0);

        code_blocks_.push_back(editor);
    }
}

void MarkdownGraphicsView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);

    if (event->size() != event->oldSize()) {
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

void MarkdownGraphicsView::applyCodeBlockHeights(
    const std::vector<QString>& codes)
{
    if (code_block_max_lines_ <= 0 || !text_item_) {
        return;
    }

    QTextDocument* document = text_item_->document();

    if (!document || !document->documentLayout()) {
        return;
    }

    const int editor_height =
        codeBlockWidgetHeight(
            code_block_max_lines_,
            code_line_height_
        );

    bool geometry_changed = false;

    for (QTextBlock block = document->begin();
         block.isValid();
         block = block.next()) {
        const QString text = block.text();

        if (text.isEmpty() || text.at(0) != QChar(0x2063)) {
            continue;
        }

        int index = 0;

        while (index < text.size() &&
               text.at(index) == QChar(0x2063)) {
            ++index;
        }

        --index;

        if (index < 0 || index >= static_cast<int>(codes.size())) {
            continue;
        }

        const QString& code = codes[index];

        const int line_count =
            1 + static_cast<int>(
                std::count(
                    code.begin(),
                    code.end(),
                    QLatin1Char('\n')
                )
            );

        /*
         * Обычный <pre> остаётся полностью в управлении QTextDocument.
         * Его marker находится в первой строке <pre>, поэтому его
         * формат и геометрию здесь не меняем.
         */
        if (line_count <= code_block_max_lines_) {
            continue;
        }

        /*
         * Длинный блок представлен отдельным <div> с marker-строкой.
         * Высота резервируется исключительно для:
         * - зоны кнопки;
         * - QPlainTextEdit;
         * - нижнего интервала между Markdown-блоками.
         */
        const int reserved_height =
            kCodeBlockButtonArea +
            editor_height +
            12;

        QTextCursor cursor(block);

        QTextBlockFormat format =
            cursor.blockFormat();

        format.setLineHeight(
            reserved_height,
            QTextBlockFormat::FixedHeight
        );

        cursor.setBlockFormat(format);
        geometry_changed = true;
    }

    if (!geometry_changed) {
        return;
    }

    /*
     * Qt может отложить пересчёт QTextDocument. Завершаем layout
     * синхронно до получения координат blockBoundingRect() и до
     * расчёта sceneRect.
     */
    document->documentLayout()->documentSize();

    /*
     * FixedHeight меняет высоту документа после setHtml(). В режиме
     * со скроллируемыми code-block нужно явно обновить область
     * QGraphicsTextItem; иначе часть Markdown после изменённого блока
     * может иметь координаты в новом layout, но не попасть в область
     * перерисовки item. В обычном режиме эта ветка не выполняется.
     */
    text_item_->update();
}


void MarkdownGraphicsView::activateCodeBlock(CodeBlockEditor* active)
{
    active_code_block_ = active;

    for (CodeBlockEditor* block : code_blocks_) {
        if (block) {
            block->setActive(block == active_code_block_);
        }
    }
}

void MarkdownGraphicsView::mousePressEvent(QMouseEvent* event)
{
    activateCodeBlock(nullptr);
    QGraphicsView::mousePressEvent(event);
}

CodeBlockEditor::CodeBlockEditor(
    MarkdownGraphicsView* view,
    QWidget* parent)
    : QPlainTextEdit(parent)
    , view_(view)
{
    setFocusPolicy(Qt::ClickFocus);
}

void CodeBlockEditor::setActive(bool active)
{
    active_ = active;

    if (active_) {
        setFocus(Qt::MouseFocusReason);
    } else {
        clearFocus();
    }
}

void CodeBlockEditor::mousePressEvent(QMouseEvent* event)
{
    if (view_) {
        view_->activateCodeBlock(this);
    }

    QPlainTextEdit::mousePressEvent(event);
}

void CodeBlockEditor::wheelEvent(QWheelEvent* event)
{
    if (active_) {
        QPlainTextEdit::wheelEvent(event);
        return;
    }

    /*
     * QGraphicsProxyWidget не всегда передаёт ignored wheel-event
     * из вложенного QPlainTextEdit в QGraphicsView. Поэтому до явного
     * клика по блоку прокручиваем общий scrollbar preview вручную.
     */
    if (!view_) {
        event->ignore();
        return;
    }

    QScrollBar* scroll_bar = view_->verticalScrollBar();

    if (!scroll_bar ||
        scroll_bar->maximum() <= scroll_bar->minimum()) {
        event->ignore();
        return;
    }

    int scroll_delta = 0;

    const QPoint pixel_delta = event->pixelDelta();

    if (!pixel_delta.isNull()) {
        /*
         * Тачпад: Qt уже передаёт величину в пикселях.
         * Положительное значение означает прокрутку вверх.
         */
        scroll_delta = pixel_delta.y();
    } else {
        /*
         * Обычное колесо: angleDelta() измеряется в 1/8 градуса,
         * один стандартный шаг равен 120.
         */
        const int angle_delta = event->angleDelta().y();

        if (angle_delta != 0) {
            const int lines_per_step =
                qMax(1, QApplication::wheelScrollLines());

            const int pixels_per_step =
                qMax(
                    scroll_bar->singleStep(),
                    QFontMetrics(view_->font()).lineSpacing()
                ) * lines_per_step;

            scroll_delta =
                (angle_delta * pixels_per_step) / 120;

            /*
             * Не теряем частичные wheel-события от некоторых мышей
             * и тачпадов, если целочисленное деление дало ноль.
             */
            if (scroll_delta == 0) {
                scroll_delta =
                    angle_delta > 0
                        ? pixels_per_step
                        : -pixels_per_step;
            }
        }
    }

    if (scroll_delta == 0) {
        event->ignore();
        return;
    }

    const int new_value = qBound(
        scroll_bar->minimum(),
        scroll_bar->value() - scroll_delta,
        scroll_bar->maximum()
    );

    scroll_bar->setValue(new_value);
    event->accept();
}

void CodeBlockEditor::focusOutEvent(QFocusEvent* event)
{
    active_ = false;
    QPlainTextEdit::focusOutEvent(event);
}

