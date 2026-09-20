#include "editorwidget.h"
#include "MarkdownGraphicsView.h"

#include "MarkdownParser.h"

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

    QRect cr = contentsRect();
    int width = lineNumberAreaWidth();

    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), width, cr.height()));

    if (m_lineNumberArea->isVisible()) {
        setViewportMargins(width, 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }
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
    if (m_lineNumberArea->isVisible()) {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }
}

void CodeEditor::updateLineNumberAreaRect(
    const QRect &rect,
    int dy)
{
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(
            0,
            rect.y(),
            m_lineNumberArea->width(),
            rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
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

    if (visible) {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }
}

void CodeEditor::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
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
    m_editor->setPlainText(text);
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
     * Increase request id to cancel any pending scroll‑restore
     * from a previous (now obsolete) mode switch.
     */
    const quint64 request_id = ++m_scrollRequestId;

    if (enabled) {
        // ---------- Switch to RENDERED view ----------
        QScrollBar* editorScroll = m_editor->verticalScrollBar();
        const int  editorMax   = editorScroll->maximum();
        const int  editorVal   = editorScroll->value();
        const qreal ratio      = (editorMax > 0)
                                     ? static_cast<qreal>(editorVal) / editorMax
                                     : 0.0;

        m_renderMd = enabled;
        m_editor->setRenderMode(enabled);
        m_markdownView->setRenderMode(enabled);

        m_stack->setCurrentWidget(m_markdownView);
        updateMarkdownView();               // rebuilds HTML, sets scene rect

        QTimer::singleShot(0, this, [this, ratio, request_id]() {
            if (request_id != m_scrollRequestId ||
                !m_renderMd ||
                m_stack->currentWidget() != m_markdownView) {
                return;
            }

            QScrollBar* viewScroll = m_markdownView->verticalScrollBar();
            const int   viewMax    = viewScroll->maximum();
            const int   target     = qBound(viewScroll->minimum(),
                                            qRound(ratio * viewMax),
                                            viewScroll->maximum());
            viewScroll->setValue(target);
        });
    } else {
        // ---------- Switch to RAW TEXT editor ----------
        QScrollBar* viewScroll = m_markdownView->verticalScrollBar();
        const int   viewMax    = viewScroll->maximum();
        const int   viewVal    = viewScroll->value();
        const qreal ratio      = (viewMax > 0)
                                     ? static_cast<qreal>(viewVal) / viewMax
                                     : 0.0;

        m_renderMd = enabled;
        m_editor->setRenderMode(enabled);
        m_markdownView->setRenderMode(enabled);

        m_stack->setCurrentWidget(m_editor);

        QTimer::singleShot(0, this, [this, ratio, request_id]() {
            if (request_id != m_scrollRequestId ||
                m_renderMd ||
                m_stack->currentWidget() != m_editor) {
                return;
            }

            QScrollBar* editorScroll = m_editor->verticalScrollBar();
            const int   editorMax    = editorScroll->maximum();
            const int   target       = qBound(editorScroll->minimum(),
                                              qRound(ratio * editorMax),
                                              editorScroll->maximum());
            editorScroll->setValue(target);
        });
    }
}

void EditorWidget::updateMarkdownView()
{
    if (!m_renderMd) {
        return;
    }

    const QString source_text =
        m_editor->toPlainText();

    m_markdownView->setSourceText(source_text);

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


