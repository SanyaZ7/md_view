#include "MarkdownGraphicsView.h"

#include "CodeBlockEditor.h"
#include "MarkdownCodeBlocks.h"
#include "MarkdownHtmlRenderer.h"
#include "MarkdownMathObjects.h"

#include <QAction>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

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

void MarkdownGraphicsView::setDocument(
    const std::vector<MarkdownNode>& document)
{
    document_ = document;
    render_document();
}

void MarkdownGraphicsView::setRenderMode(bool enabled)
{
    render_mode_ = enabled;
}

void MarkdownGraphicsView::setCodeBlockMaxLines(int lines)
{
    code_block_max_lines_ = qMax(0, lines);
}

int MarkdownGraphicsView::codeBlockMaxLines() const
{
    return code_block_max_lines_;
}

bool MarkdownGraphicsView::renderMode() const
{
    return render_mode_;
}

void MarkdownGraphicsView::clear_document()
{
    if (scene()) {
        scene()->clear();
    }
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

    /*
     * Список исходных строк верхнеуровневых узлов, для которых
     * рендерер вставляет навигационный маркер. Порядок и критерий
     * (source_start_line >= 0) совпадают с blocks_to_html(with_nav).
     */
    nav_source_lines_.clear();

    for (const MarkdownNode& node : document_) {
        if (node.source_start_line >= 0) {
            nav_source_lines_.push_back(node.source_start_line);
        }
    }

    const int viewport_width =
        qMax(300, viewport()->width());

    const qreal document_width =
        qMax(
            100.0,
            static_cast<qreal>(viewport_width) - 32.0);

    QGraphicsTextItem* item = new QGraphicsTextItem();

    text_item_ = item;

    item->setFont(text_font_);
    item->setOpenExternalLinks(false);

    connect(
        item,
        &QGraphicsTextItem::linkActivated,
        this,
        [](const QString& link) {
            QDesktopServices::openUrl(QUrl(link));
        });

    std::vector<QString> code_list;

    MarkdownUtils::RenderOptions render_options;
    render_options.codeBlockMaxLines = code_block_max_lines_;

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
            ".math-inline, .math-block, .math-op, .math-mul {"
            " font-family:'Noto Sans Math','DejaVu Sans','Noto Sans';"
            " }"
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
            "</style></head><body>")
            .arg(
                MarkdownCodeBlocks::kCodeBlockButtonArea
                + MarkdownCodeBlocks::kCodeBlockPadding)
        + MarkdownUtils::blocks_to_html(
            document_,
            &code_list,
            render_options,
            /*with_nav=*/true)
        + QStringLiteral("</body></html>");

    item->setHtml(html);

    /*
     * Дроби \frac передаются рендерером как маркерные токены
     * U+E000..U+E002. Регистрируем inline-объект и превращаем
     * токены в ObjectReplacementCharacter, чтобы дробь рисовалась
     * вертикально и не вызывала переноса строки.
     */
    QTextDocument* math_document = item->document();

    if (math_document) {
        MarkdownMathObjects::registerHandlers(math_document);
        MarkdownMathObjects::installObjects(math_document);
    }

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

            QTextDocument* current_document =
                item->document();

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
                        items_rect.bottom());
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
                        QPointF(0.0, document_size.height()))
                        .y();

                const QList<QGraphicsItem*> scene_items =
                    scene()->items();

                for (QGraphicsItem* scene_item : scene_items) {
                    if (!scene_item || scene_item == item) {
                        continue;
                    }

                    content_bottom =
                        qMax(
                            content_bottom,
                            scene_item->sceneBoundingRect().bottom());
                }
            }

            const qreal scene_height =
                qMax(
                    static_cast<qreal>(viewport()->height()),
                    content_bottom + 16.0);

            scene()->setSceneRect(
                0.0,
                0.0,
                viewport_width,
                qMax(32.0, scene_height));

            /*
             * Якоря навигации зависят от итоговой геометрии блоков,
             * поэтому перестраиваются вместе с rect сцены: после
             * первичного layout, documentSizeChanged и отложенного
             * вызова в следующем event loop.
             */
            rebuildSourceAnchors();
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
        });

    /*
     * Proxy-виджеты могут уточнить геометрию только после добавления
     * в QGraphicsScene. Повторяем расчёт в следующем event loop.
     */
    QTimer::singleShot(
        0,
        this,
        [update_scene_rect]() {
            update_scene_rect();
        });
}

void MarkdownGraphicsView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);

    if (event->size() != event->oldSize()) {
        render_document();
    }
}

void MarkdownGraphicsView::contextMenuEvent(
    QContextMenuEvent* event)
{
    QMenu menu(this);

    QAction* render_action =
        menu.addAction(tr("Отрисовка md"));

    render_action->setCheckable(true);
    render_action->setChecked(render_mode_);

    connect(
        render_action,
        &QAction::toggled,
        this,
        [this](bool checked) {
            render_mode_ = checked;
            emit renderModeRequested(checked);
        });

    menu.exec(event->globalPos());
}

void MarkdownGraphicsView::activateCodeBlock(
    CodeBlockEditor* active)
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

