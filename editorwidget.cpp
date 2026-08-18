#include "editorwidget.h"

#include <QPainter>
#include <QTextBlock>
#include <QVBoxLayout>

// ============ LineNumberArea ============

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_editor->lineNumberAreaPaintEvent(event);
}

// ============ CodeEditor ============

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
    m_lineNumberArea->hide();          // по умолчанию номера скрыты
    setViewportMargins(0, 0, 0, 0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    int width = lineNumberAreaWidth();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), width, cr.height()));

    setViewportMargins(width, 0, 0, 0);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top,
                             m_lineNumberArea->width() - 4,
                             fontMetrics().height(),
                             Qt::AlignRight, number);
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
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberAreaRect(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(),
                                 m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
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
        QColor lineColor = QColor(Qt::yellow).lighter(160);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    setExtraSelections(extraSelections);
}

void CodeEditor::setLineNumberAreaVisible(bool visible)
{
    m_lineNumberArea->setVisible(visible);
    if (visible)
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    else
        setViewportMargins(0, 0, 0, 0);
}

// ============ EditorWidget ============

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_editor(new CodeEditor(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editor);
}

void EditorWidget::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
    m_editor->setProperty("filePath", filePath);
}

QString EditorWidget::filePath() const
{
    return m_filePath;
}

void EditorWidget::setPlainText(const QString &text)
{
    m_editor->setPlainText(text);
}

void EditorWidget::applySettings(const Settings &settings)
{
    if (settings.wordWrap)
        m_editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    else
        m_editor->setWordWrapMode(QTextOption::NoWrap);

    m_editor->setLineNumberAreaVisible(settings.lineNumbers);
}

