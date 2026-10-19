#include "MarkdownCodeBlocks.h"

#include "CodeBlockEditor.h"
#include "MarkdownGraphicsView.h"
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace MarkdownCodeBlocks
{

int codeBlockWidgetHeight(
    int lines,
    qreal line_height)
{
    const qreal line = line_height + 1.0;

    return static_cast<int>(
               std::ceil(lines * line))
        + kCodeBlockChrome;
}

QPushButton* createPreviewButton(
    const QString& code,
    const QString& language,
    QWidget* parent,
    const QFont& font,
    const QString& style)
{
    QPushButton* button = new QPushButton(QObject::tr("Открыть"), parent);

    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFont(font);
    button->setStyleSheet(style);
    button->setMinimumWidth(
        QFontMetrics(font).horizontalAdvance(QObject::tr("Открыть")) + 20);

    return button;
}

} // namespace MarkdownCodeBlocks

void MarkdownGraphicsView::createCopyButtons(
    const std::vector<MarkdownUtils::CodeBlockInfo>& codes)
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

        if (text.isEmpty() ||
            text.at(0) != QChar(0x2063)) {
            continue;
        }

        // Количество маркеров U+2063 равно index + 1.
        int index = 0;

        while (index < text.size() &&
               text.at(index) == QChar(0x2063)) {
            ++index;
        }

        --index;

        if (index < 0 ||
            index >= static_cast<int>(codes.size())) {
            continue;
        }

        const QRectF block_rect =
            document->documentLayout()->blockBoundingRect(block);

        const qreal y = block_rect.top();
        const qreal x = block_rect.left();

        QPushButton* button =
            new QPushButton(tr("Копировать"));

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
                metrics.horizontalAdvance(tr("Скопировано")))
            + 20;

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

        const QString code = codes[index].code;

        connect(
            button,
            &QPushButton::clicked,
            this,
            [code, button]() {
                QApplication::clipboard()->setText(code);

                button->setText(QObject::tr("Скопировано"));

                QTimer::singleShot(
                    1500,
                    button,
                    [button]() {
                        button->setText(
                            QObject::tr("Копировать"));
                    });
            });

        QGraphicsProxyWidget* proxy =
            scene()->addWidget(button);

        proxy->setPos(
            16.0 + x + 2.0,
            16.0 + y + 4.0);

        const QString language =
            QString::fromStdString(codes[index].language).trimmed().toLower();

        if (language == QStringLiteral("svg") ||
            language == QStringLiteral("html")) {
            QPushButton* preview_button =
                MarkdownCodeBlocks::createPreviewButton(
                    code,
                    language,
                    nullptr,
                    button_font,
                    button->styleSheet());

            QGraphicsProxyWidget* preview_proxy =
                scene()->addWidget(preview_button);
            preview_proxy->setPos(
                16.0 + x + 2.0 + button_width + 6.0,
                16.0 + y + 4.0);
            preview_proxy->setZValue(2.0);
        }

        /*
         * Длинный CodeBlockEditor размещается на z = 1.
         * Кнопка должна быть выше него, иначе редактор кода
         * перекрывает кнопку в нижних/длинных code-block.
         */
        proxy->setZValue(2.0);

        copy_buttons_.push_back({
            button,
            proxy,
            code
        });
    }
}

void MarkdownGraphicsView::createScrollableCodeBlocks(
    const std::vector<MarkdownUtils::CodeBlockInfo>& codes)
{
    if (code_block_max_lines_ <= 0 ||
        !text_item_ ||
        !scene()) {
        return;
    }

    QTextDocument* document = text_item_->document();

    if (!document) {
        return;
    }

    const qreal document_width =
        text_item_->textWidth();

    const qreal code_width =
        qMax<qreal>(
            100.0,
            document_width
                - 2.0 * (
                    MarkdownCodeBlocks::kCodeBlockPadding
                    + MarkdownCodeBlocks::kCodeBlockBorder));

    const int code_height =
        MarkdownCodeBlocks::codeBlockWidgetHeight(
            code_block_max_lines_,
            code_line_height_);

    for (QTextBlock block = document->begin();
         block.isValid();
         block = block.next()) {
        const QString text = block.text();

        if (text.isEmpty() ||
            text.at(0) != QChar(0x2063)) {
            continue;
        }

        int index = 0;

        while (index < text.size() &&
               text.at(index) == QChar(0x2063)) {
            ++index;
        }

        --index;

        if (index < 0 ||
            index >= static_cast<int>(codes.size())) {
            continue;
        }

        const QString& code = codes[index].code;

        const int line_count =
            1 + static_cast<int>(
                std::count(
                    code.begin(),
                    code.end(),
                    QLatin1Char('\n')));

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

        CodeBlockEditor* editor =
            new CodeBlockEditor(this);

        editor->setPlainText(code);
        editor->setReadOnly(true);
        editor->setFont(code_font_);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setVerticalScrollBarPolicy(
            Qt::ScrollBarAsNeeded);
        editor->setHorizontalScrollBarPolicy(
            Qt::ScrollBarAsNeeded);
        editor->setFrameShape(
            QPlainTextEdit::StyledPanel);

        editor->setStyleSheet(
            QStringLiteral(
                "QPlainTextEdit {"
                " background:#f3f3f3;"
                " border:1px solid #dddddd;"
                " padding:%1px %2px;"
                " color:#333333;"
                "}"
            )
            .arg(MarkdownCodeBlocks::kCodeBlockVSpacing)
            .arg(MarkdownCodeBlocks::kCodeBlockPadding));

        editor->setFixedSize(
            static_cast<int>(std::ceil(code_width)),
            code_height);

        editor->setCodeLanguage(
            QString::fromStdString(codes[index].language));

        QGraphicsProxyWidget* proxy =
            scene()->addWidget(editor);

        proxy->setPos(
            16.0 + block_rect.left(),
            16.0
                + block_rect.top()
                + MarkdownCodeBlocks::kCodeBlockButtonArea);

        proxy->setZValue(1.0);

        code_blocks_.push_back(editor);
    }
}

void MarkdownGraphicsView::applyCodeBlockHeights(
    const std::vector<MarkdownUtils::CodeBlockInfo>& codes)
{
    if (code_block_max_lines_ <= 0 ||
        !text_item_) {
        return;
    }

    QTextDocument* document =
        text_item_->document();

    if (!document ||
        !document->documentLayout()) {
        return;
    }

    const int editor_height =
        MarkdownCodeBlocks::codeBlockWidgetHeight(
            code_block_max_lines_,
            code_line_height_);

    bool geometry_changed = false;

    for (QTextBlock block = document->begin();
         block.isValid();
         block = block.next()) {
        const QString text = block.text();

        if (text.isEmpty() ||
            text.at(0) != QChar(0x2063)) {
            continue;
        }

        int index = 0;

        while (index < text.size() &&
               text.at(index) == QChar(0x2063)) {
            ++index;
        }

        --index;

        if (index < 0 ||
            index >= static_cast<int>(codes.size())) {
            continue;
        }

        const QString& code = codes[index].code;

        const int line_count =
            1 + static_cast<int>(
                std::count(
                    code.begin(),
                    code.end(),
                    QLatin1Char('\n')));

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
            MarkdownCodeBlocks::kCodeBlockButtonArea
            + editor_height
            + 12;

        QTextCursor cursor(block);

        QTextBlockFormat format =
            cursor.blockFormat();

        format.setLineHeight(
            reserved_height,
            QTextBlockFormat::FixedHeight);

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

