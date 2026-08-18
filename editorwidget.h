#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include "settings.h"

class QPaintEvent;
class QResizeEvent;
class LineNumberArea;

// ============================================================
//  CodeEditor — сам редактор с областью номеров строк
// ============================================================
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    // Отрисовка номеров строк (вызывается из LineNumberArea)
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

    // Обновить ширину области номеров
    void updateLineNumberAreaWidth(int blockCount);

    // Показать / скрыть область номеров
    void setLineNumberAreaVisible(bool visible);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaRect(const QRect &rect, int dy);
    void updateLineNumberArea();
    void highlightCurrentLine();

private:
    LineNumberArea *m_lineNumberArea;
};

// ============================================================
//  LineNumberArea — виджет области номеров строк
// ============================================================
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor)          // теперь CodeEditor уже полный тип
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
    CodeEditor *m_editor;
};

// ============================================================
//  EditorWidget — составной виджет (контейнер)
// ============================================================
class EditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWidget(QWidget *parent = nullptr);

    CodeEditor *editor() const { return m_editor; }

    void setFilePath(const QString &filePath);
    QString filePath() const;
    void setPlainText(const QString &text);
    void applySettings(const Settings &settings);

private:
    CodeEditor *m_editor;
    QString m_filePath;
};

#endif // EDITORWIDGET_H

