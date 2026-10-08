#include "editorwidget.h"
#include "MarkdownGraphicsView.h"

#include "MarkdownParser.h"
#include <QShowEvent>
#include <QAction>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QDebug>

// ============================================================
// LineNumberArea
// ============================================================

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_editor->lineNumberAreaPaintEvent(event);
}

// ============================================================
// CodeEditor
// ============================================================

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);

    connect(this, &CodeEditor::updateRequest,
            this, &CodeEditor::updateLineNumberAreaRect);

    connect(this, &CodeEditor::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);

    m_lineNumberArea->hide();
    setViewportMargins(0, 0, 0, 0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    const int width =
        m_lineNumberArea->isVisible()
            ? lineNumberAreaWidth()
            : 0;

    if (m_lineNumberArea->isVisible()) {
        setViewportMargins(width, 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }

    const QRect cr = contentsRect();

    m_lineNumberArea->setGeometry(
        cr.left(),
        cr.top(),
        width,
        cr.height()
    );

    m_lineNumberArea->update();
    viewport()->update();
    update();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();

    int top = qRound(
        blockBoundingGeometry(block)
            .translated(contentOffset())
            .top());

    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() &&
           top <= event->rect().bottom()) {
        if (block.isVisible() &&
            bottom >= event->rect().top()) {
            QString number =
                QString::number(blockNumber + 1);

            painter.setPen(Qt::black);
            painter.drawText(
                0,
                top,
                m_lineNumberArea->width() - 4,
                fontMetrics().height(),
                Qt::AlignRight,
                number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int maximum = qMax(1, blockCount());

    while (maximum >= 10) {
        maximum /= 10;
        ++digits;
    }

    return 8 +
           fontMetrics().horizontalAdvance(QLatin1Char('9')) *
           digits;
}

void CodeEditor::updateLineNumberAreaWidth(int /*blockCount*/)
{
    const int width =
        m_lineNumberArea->isVisible()
            ? lineNumberAreaWidth()
            : 0;

    if (m_lineNumberArea->isVisible()) {
        setViewportMargins(width, 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }

    const QRect cr = contentsRect();

    m_lineNumberArea->setGeometry(
        cr.left(),
        cr.top(),
        width,
        cr.height()
    );

    m_lineNumberArea->update();
    viewport()->update();
}

void CodeEditor::updateLineNumberAreaRect(
    const QRect &rect,
    int dy)
{
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(
            0,
            rect.y(),
            m_lineNumberArea->width(),
            rect.height()
        );
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
        m_lineNumberArea->update();
        viewport()->update();
    }
}

void CodeEditor::updateLineNumberArea()
{
    m_lineNumberArea->update();
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor =
            QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(
            QTextFormat::FullWidthSelection,
            true);

        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::setLineNumberAreaVisible(bool visible)
{
    m_lineNumberArea->setVisible(visible);

    const int width =
        visible
            ? lineNumberAreaWidth()
            : 0;

    if (visible) {
        setViewportMargins(width, 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }

    const QRect cr = contentsRect();

    m_lineNumberArea->setGeometry(
        cr.left(),
        cr.top(),
        width,
        cr.height()
    );

    m_lineNumberArea->update();
    viewport()->update();
    update();
}

void CodeEditor::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

int CodeEditor::firstVisibleSourceLine() const
{
    /*
     * В QPlainTextEdit один QTextBlock соответствует одной строке
     * исходника (word wrap не создаёт новых блоков), поэтому номер
     * блока и есть номер исходной строки (0-based).
     */
    QTextBlock block = firstVisibleBlock();

    if (!block.isValid()) {
        return 0;
    }

    /*
     * Первый видимый блок может быть виден лишь частично снизу
     * предыдущей прокрутки. Если его верх уже ушёл за верхнюю
     * границу viewport, берём следующий блок как «верх видимой строки».
     */
    qreal top =
        blockBoundingGeometry(block)
            .translated(contentOffset())
            .top();

    if (top < 0.0) {
        const QTextBlock next = block.next();

        if (next.isValid()) {
            return next.blockNumber();
        }
    }

    return block.blockNumber();
}

void CodeEditor::scrollToSourceLine(int line)
{
    const int blockCount = this->blockCount();

    if (blockCount <= 0) {
        return;
    }

    const int target = qBound(0, line, blockCount - 1);

    QTextBlock block = document()->findBlockByNumber(target);

    if (!block.isValid()) {
        verticalScrollBar()->setValue(
            verticalScrollBar()->minimum());

        return;
    }

    /*
     * QPlainTextEdit раскладывает блоки лениво: для блока далеко за
     * пределами видимой области blockBoundingGeometry() недостоверна,
     * пока блок не разложен. Без форса раскладки относительная формула
     * ниже даёт мусор, и qBound зажимает результат в minimum()/maximum()
     * — визуально это прыжок в начало или конец документа.
     *
     * Раскладываем целевой блок через временный курсор + ensureCursorVisible,
     * затем возвращаем исходную позицию каретки (это не сдвигает прокрутку).
     */
    const QTextCursor saved_cursor = textCursor();
    setTextCursor(QTextCursor(block));
    ensureCursorVisible();

    /*
     * blockBoundingGeometry(...).translated(contentOffset()).top()
     * — это позиция верха блока относительно viewport при ТЕКУЩЕМ
     * значении полосы прокрутки. Чтобы поместить блок к верхней
     * границе, к текущему значению прибавляем эту позицию.
     */
    const qreal block_top =
        blockBoundingGeometry(block)
            .translated(contentOffset())
            .top();

    const qreal abs_top =
        document()->documentLayout()->blockBoundingRect(block).top();

    const int target_value =
        qBound(verticalScrollBar()->minimum(),
               verticalScrollBar()->value() + qRound(block_top),
               verticalScrollBar()->maximum());

    qDebug().noquote()
        << "[NAV] CodeEditor::scrollToSourceLine target(line)=" << line
        << "blockCount=" << blockCount
        << "block_top=" << block_top
        << "abs_top=" << abs_top
        << "cur=" << verticalScrollBar()->value()
        << "->set=" << target_value
        << "(min,max)=(" << verticalScrollBar()->minimum()
        << "," << verticalScrollBar()->maximum() << ")"
        << "editorVisible=" << isVisible();

    verticalScrollBar()->setValue(target_value);

    /*
     * QPlainTextEdit::setTextCursor сам прокручивает документ к
     * курсору, поэтому восстановление saved_cursor отменило бы
     * только что выставленную позицию (каретка до позиционирования
     * обычно в начале — прокрутка возвращалась к 0). Сначала
     * возвращаем каретку, затем повторно ставим целевое значение:
     * финальное слово остаётся за прокруткой, а не за курсором.
     */
    setTextCursor(saved_cursor);
    verticalScrollBar()->setValue(target_value);
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();

    QFileInfo fileInfo(m_filePath);

    if (fileInfo.suffix().compare(
            QStringLiteral("md"),
            Qt::CaseInsensitive) == 0) {
        menu->addSeparator();

        QAction *renderAction =
            menu->addAction(
                tr("Отрисовка md"));

        renderAction->setCheckable(true);
        renderAction->setChecked(m_renderMd);

        connect(renderAction, &QAction::toggled,
                this,
                [this](bool checked) {
                    m_renderMd = checked;
                    emit renderModeRequested(checked);
                });
    }

    menu->exec(event->globalPos());
    delete menu;
}

void CodeEditor::showEvent(QShowEvent *event)
{
    QPlainTextEdit::showEvent(event);

    const int width =
        m_lineNumberArea->isVisible()
            ? lineNumberAreaWidth()
            : 0;

    if (m_lineNumberArea->isVisible()) {
        setViewportMargins(width, 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }

    const QRect cr = contentsRect();

    m_lineNumberArea->setGeometry(
        cr.left(),
        cr.top(),
        width,
        cr.height()
    );

    /*
     * При переключении вкладок область редактора может быть
     * показана с устаревшим содержимым backing store. Полная
     * перерисовка устраняет наложение текста и номеров строк.
     */
    m_lineNumberArea->update();
    viewport()->update();
    update();
}

// ============================================================
// EditorWidget
// ============================================================

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_editor(new CodeEditor(this))
    , m_markdownView(new MarkdownGraphicsView(this))
    , m_stack(new QStackedWidget(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack->addWidget(m_editor);
    m_stack->addWidget(m_markdownView);

    layout->addWidget(m_stack);

    connect(m_editor, &CodeEditor::renderModeRequested,
            this, &EditorWidget::setRenderMode);

    connect(m_markdownView,
            &MarkdownGraphicsView::renderModeRequested,
            this,
            &EditorWidget::setRenderMode);

    connect(m_editor, &QPlainTextEdit::textChanged,
            this, &EditorWidget::updateMarkdownView);

    connect(m_editor, &QPlainTextEdit::textChanged,
            this, &EditorWidget::textChangedFlag);

    /*
     * Двусторонняя синхронизация прокрутки. Синхронизируем только
     * прокрутку видимого (активного) виджета, чтобы скрытый виджет
     * обновлялся в фоне, а программная прокрутка не вызывала цикл.
     */
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() {
                if (!m_renderMd) {
                    syncScrollFromEditor();
                }
            });

    connect(m_markdownView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() {
                if (m_renderMd) {
                    syncScrollFromMarkdownView();
                }
            });

    m_stack->setCurrentWidget(m_editor);
}

void EditorWidget::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
    m_editor->setFilePath(filePath);
}

QString EditorWidget::filePath() const
{
    return m_filePath;
}

void EditorWidget::setPlainText(const QString &text)
{
    m_blockSignals = true;
    m_editor->setPlainText(text);
    m_blockSignals = false;
    updateMarkdownView();
}

void EditorWidget::applySettings(const Settings &settings)
{
    if (settings.wordWrap) {
        m_editor->setWordWrapMode(
            QTextOption::WrapAtWordBoundaryOrAnywhere);
    } else {
        m_editor->setWordWrapMode(
            QTextOption::NoWrap);
    }

    m_editor->setLineNumberAreaVisible(
        settings.lineNumbers);

    m_markdownView->setCodeBlockMaxLines(
        settings.limitCodeBlockHeight ? qMax(8, settings.codeBlockMaxLines) : 0);

    // Настройка влияет на отрисовку блоков кода — перерисовываем.
    if (m_renderMd) {
        updateMarkdownView();
    }
}

void CodeEditor::setRenderMode(bool enabled)
{
    m_renderMd = enabled;
}

void EditorWidget::setRenderMode(bool enabled)
{
    if (m_renderMd == enabled) {
        return;
    }

    /*
     * Увеличиваем request id, чтобы отменить любую отложенную
     * прокрутку от предыдущего (уже неактуального) переключения
     * режимов.
     */
    const quint64 request_id = ++m_scrollRequestId;

    /*
     * На время переключения полностью отключаем live-синхронизацию:
     * смена виджета и пересчёт sceneRect сдвигают scrollbar, что не
     * должно запускать обратную прокрутку и давать перескок.
     */
    m_switchingScroll = true;

    /*
     * Позиция определяется через исходную строку, а не через
     * отношение значений scrollbar: в raw и rendered режимах
     * вертикальная прокрутка имеет разный смысл.
     */
    if (enabled) {
        // ---------- Переход в RENDERED ----------
        const int source_line = m_editor->firstVisibleSourceLine();

        qDebug().noquote()
            << "[SWITCH] ->RENDERED captured source_line=" << source_line
            << "request_id=" << request_id;

        m_renderMd = enabled;
        m_editor->setRenderMode(enabled);
        m_markdownView->setRenderMode(enabled);

        m_stack->setCurrentWidget(m_markdownView);
        updateMarkdownView();               // пересобирает HTML и якоря

        /*
         * Якоря готовы только после завершения layout rendered
         * документа. Восстанавливаем позицию в следующем event loop,
         * сверяясь с request id и текущим виджетом стека.
         */
        QTimer::singleShot(0, this, [this, source_line, request_id]() {
            if (request_id != m_scrollRequestId ||
                !m_renderMd ||
                m_stack->currentWidget() != m_markdownView) {
                qDebug() << "[SWITCH] ->RENDERED singleShot SKIPPED"
                         << "request_id=" << request_id
                         << "cur=" << m_scrollRequestId;
                return;
            }

            qDebug().noquote()
                << "[SWITCH] ->RENDERED apply scrollToSourceLine"
                << source_line
                << "mdview visible=" << m_markdownView->isVisible()
                << "width=" << m_markdownView->width();

            m_markdownView->scrollToSourceLine(source_line);
            m_switchingScroll = false;
        });
    } else {
        // ---------- Переход в RAW TEXT ----------
        //
        // Диагностика положения rendered view ДО считывания строки:
        // если scrollbar уже сброшен к началу (value==minimum) до
        // sourceLineForCurrentScroll(), строка будет 0 независимо от
        // корректности якорей.
        QScrollBar *md_sb = m_markdownView->verticalScrollBar();
        const QRectF md_scene_rect = m_markdownView->sceneRect();

        qDebug().noquote()
            << "[SWITCH] ->RAW pre-capture mdview visible="
            << m_markdownView->isVisible()
            << "stackCurrentIsMd="
            << (m_stack->currentWidget() == m_markdownView)
            << "scrollbar(value,min,max)=("
            << md_sb->value() << "," << md_sb->minimum() << ","
            << md_sb->maximum() << ")"
            << "sceneRect=" << md_scene_rect;

        const int source_line =
            m_markdownView->sourceLineForCurrentScroll();

        qDebug().noquote()
            << "[SWITCH] ->RAW captured source_line=" << source_line
            << "request_id=" << request_id;

        m_renderMd = enabled;
        m_editor->setRenderMode(enabled);
        m_markdownView->setRenderMode(enabled);

        m_stack->setCurrentWidget(m_editor);

        QTimer::singleShot(0, this, [this, source_line, request_id]() {
            if (request_id != m_scrollRequestId ||
                m_renderMd ||
                m_stack->currentWidget() != m_editor) {
                qDebug() << "[SWITCH] ->RAW singleShot SKIPPED"
                         << "request_id=" << request_id
                         << "cur=" << m_scrollRequestId;
                return;
            }

            qDebug().noquote()
                << "[SWITCH] ->RAW apply scrollToSourceLine"
                << source_line
                << "editor visible=" << m_editor->isVisible()
                << "width=" << m_editor->width()
                << "viewportW=" << m_editor->viewport()->width()
                << "blockCount=" << m_editor->blockCount()
                << "sb(max)=" << m_editor->verticalScrollBar()->maximum();

            // Принудительно завершаем layout редактора до позиционирования.
            m_editor->document()->documentLayout()->documentSize();

            m_editor->scrollToSourceLine(source_line);

            const int sb_after = m_editor->verticalScrollBar()->value();
            const int first_line_after = m_editor->firstVisibleSourceLine();

            qDebug().noquote()
                << "[SWITCH] ->RAW after scrollToSourceLine sb="
                << sb_after
                << "firstVisibleLine=" << first_line_after
                << "expected_line=" << source_line;

            m_switchingScroll = false;
        });
    }
}

void EditorWidget::syncScrollFromEditor()
{
    /*
     * Прокрутка выполняется программно (в ответ на синхронизацию из
     * rendered), идёт пересоздание или переключение режима — не
     * запускаем обратную волну.
     */
    if (m_syncingScroll || m_switchingScroll || m_blockSignals) {
        return;
    }

    /*
     * Скрытый в QStackedWidget виджет прокручивать бессмысленно: его
     * layout не финализирован, а геометрия блоков недостоверна. Это
     * только портит позицию, на которую опирается инкрементальная
     * формула прокрутки. Активная сторона синхронизируется при
     * переключении режима.
     */
    if (!m_markdownView->isVisible()) {
        return;
    }

    const int source_line = m_editor->firstVisibleSourceLine();

    m_syncingScroll = true;
    m_markdownView->scrollToSourceLine(source_line);
    m_syncingScroll = false;
}

void EditorWidget::syncScrollFromMarkdownView()
{
    if (m_syncingScroll || m_switchingScroll || m_blockSignals) {
        return;
    }

    if (!m_editor->isVisible()) {
        return;
    }

    const int source_line =
        m_markdownView->sourceLineForCurrentScroll();

    m_syncingScroll = true;
    m_editor->scrollToSourceLine(source_line);
    m_syncingScroll = false;
}

void EditorWidget::updateMarkdownView()
{
    if (!m_renderMd) {
        return;
    }

    const QString source_text =
        m_editor->toPlainText();

    const QByteArray utf8Text =
        source_text.toUtf8();

    const std::string markdown(
        utf8Text.constData(),
        static_cast<std::size_t>(utf8Text.size()));

    MarkdownParser parser;

    const std::vector<MarkdownNode> document =
        parser.parse(markdown);

    m_markdownView->setDocument(document);
}


