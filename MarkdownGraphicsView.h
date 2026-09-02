#ifndef MARKDOWN_GRAPHICS_VIEW_H
#define MARKDOWN_GRAPHICS_VIEW_H

#include "MarkdownNode.h"
#include <QString>
#include <QGraphicsView>
#include <QFont>
#include <string>
#include <vector>

// Пространство имен с утилитами (вынесено из класса)
namespace MarkdownUtils {
    QString markdown_text_to_html(const std::string& value);
    QString utf8(const std::string& value);
    QString escape_html(const std::string& value);
    QString alignment_to_css(Alignment alignment);
    
    // Основные функции рендеринга
    QString node_to_html(const MarkdownNode& node);
    QString inline_to_html(const MarkdownNode& node);
    QString children_to_html(const MarkdownNode& node);
    QString list_to_html(const MarkdownNode& node);
    QString table_to_html(const MarkdownNode& node);
    QString blocks_to_html(const std::vector<MarkdownNode>& nodes);
    
    // Утилита для поиска строк
    QString sourceLineCandidate(const QString& source_line);
}

class QContextMenuEvent;
class QResizeEvent;
class QGraphicsTextItem;

class MarkdownGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MarkdownGraphicsView(QWidget* parent = nullptr);
    void setSourceText(const QString& text);
    void scrollToSourceLine(int line);
    int sourceLineForCurrentScroll() const;
    void setDocument(const std::vector<MarkdownNode>& document);

    void setRenderMode(bool enabled);
    bool renderMode() const;

signals:
    void renderModeRequested(bool enabled);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QFont text_font_;
    QFont code_font_;
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
};

#endif // MARKDOWN_GRAPHICS_VIEW_H

