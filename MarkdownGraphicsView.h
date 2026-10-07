#ifndef MARKDOWN_GRAPHICS_VIEW_H
#define MARKDOWN_GRAPHICS_VIEW_H

#include "MarkdownNode.h"

#include <QFont>
#include <QGraphicsView>
#include <QString>

#include <string>
#include <vector>

class QContextMenuEvent;
class QGraphicsProxyWidget;
class QGraphicsTextItem;
class QMouseEvent;
class QPushButton;
class QResizeEvent;
class CodeBlockEditor;

class MarkdownGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MarkdownGraphicsView(QWidget* parent = nullptr);

    void scrollToSourceLine(int line);
    int sourceLineForCurrentScroll() const;

    void setDocument(
        const std::vector<MarkdownNode>& document);

    // Делает активным (перехватывающим колесо мыши) только один блок кода.
    void activateCodeBlock(CodeBlockEditor* active);

    void setRenderMode(bool enabled);
    bool renderMode() const;

    void setCodeBlockMaxLines(int lines);
    int codeBlockMaxLines() const;

signals:
    void renderModeRequested(bool enabled);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QFont text_font_;
    QFont code_font_;
    qreal code_line_height_ = 0.0;
    bool render_mode_ = false;

    std::vector<MarkdownNode> document_;

    void clear_document();
    void render_document();

    struct SourceAnchor {
        int line = -1;
        qreal y = 0.0;
    };

    QGraphicsTextItem* text_item_ = nullptr;
    mutable std::vector<SourceAnchor> source_anchors_;

    /*
     * Исходные строки верхнеуровневых узлов с заданным
     * source_start_line, в порядке появления в документе.
     * По ним строится соответствие raw<->rendered через
     * невидимые маркеры в QTextDocument.
     */
    std::vector<int> nav_source_lines_;

    void rebuildSourceAnchors() const;

    void createCopyButtons(
        const std::vector<QString>& codes);

    void createScrollableCodeBlocks(
        const std::vector<QString>& codes);

    void applyCodeBlockHeights(
        const std::vector<QString>& codes);

    int code_block_max_lines_ = 0; // 0 = без ограничения

    struct CopyButton {
        QPushButton* button = nullptr;
        QGraphicsProxyWidget* proxy = nullptr;
        QString code;
    };

    std::vector<CopyButton> copy_buttons_;

    std::vector<CodeBlockEditor*> code_blocks_;
    CodeBlockEditor* active_code_block_ = nullptr;
};

#endif // MARKDOWN_GRAPHICS_VIEW_H

