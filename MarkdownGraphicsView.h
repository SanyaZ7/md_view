#ifndef MARKDOWN_GRAPHICS_VIEW_H
#define MARKDOWN_GRAPHICS_VIEW_H

#include "MarkdownNode.h"

#include <QFont>
#include <QGraphicsView>
#include <QString>

#include <string>
#include <vector>

// Пространство имен с утилитами рендеринга Markdown.
namespace MarkdownUtils
{
    QString markdown_text_to_html(const std::string& value);
    QString utf8(const std::string& value);
    QString escape_html(const std::string& value);
    QString alignment_to_css(Alignment alignment);

    QString node_to_html(
        const MarkdownNode& node,
        std::vector<QString>* code_list = nullptr);

    QString inline_to_html(const MarkdownNode& node);
    QString children_to_html(const MarkdownNode& node);
    QString list_to_html(const MarkdownNode& node);
    QString table_to_html(const MarkdownNode& node);

    QString blocks_to_html(
        const std::vector<MarkdownNode>& nodes,
        std::vector<QString>* code_list = nullptr);

    // Утилита для поиска строк.
    QString sourceLineCandidate(const QString& source_line);
}

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

    void setSourceText(const QString& text);
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

    QString source_text_;
    QGraphicsTextItem* text_item_ = nullptr;
    mutable std::vector<SourceAnchor> source_anchors_;

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

