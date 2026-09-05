#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QtGlobal>
#include "settings.h"

class QContextMenuEvent;
class QPaintEvent;
class QResizeEvent;
class QStackedWidget;

class MarkdownGraphicsView;

// ============================================================
// CodeEditor
// ============================================================

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;
    void updateLineNumberAreaWidth(int blockCount);
    void setLineNumberAreaVisible(bool visible);
    void setFilePath(const QString &filePath);
    void setRenderMode(bool enabled);

signals:
    void renderModeRequested(bool enabled);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void updateLineNumberAreaRect(const QRect &rect, int dy);
    void updateLineNumberArea();
    void highlightCurrentLine();

private:
    class LineNumberArea *m_lineNumberArea = nullptr;
	
    QString m_filePath;
    bool m_renderMd = false;
};

// ============================================================
// LineNumberArea
// ============================================================

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *m_editor = nullptr;
};

// ============================================================
// EditorWidget
// ============================================================

class EditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWidget(QWidget *parent = nullptr);

    CodeEditor *editor() const
    {
        return m_editor;
    }

    MarkdownGraphicsView *markdownView() const
    {
        return m_markdownView;
    }

    void setFilePath(const QString &filePath);
    QString filePath() const;

    void setPlainText(const QString &text);

    void applySettings(const Settings &settings);

    bool renderMode() const
    {
        return m_renderMd;
    }

public slots:
    void setRenderMode(bool enabled);

private slots:
    void updateMarkdownView();

private:
    CodeEditor *m_editor = nullptr;
    MarkdownGraphicsView *m_markdownView = nullptr;
    QStackedWidget *m_stack = nullptr;
	quint64 m_scrollRequestId = 0;
    QString m_filePath;
    bool m_renderMd = false;
};

#endif // EDITORWIDGET_H

