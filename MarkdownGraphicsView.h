#ifndef MARKDOWN_GRAPHICS_VIEW_H
#define MARKDOWN_GRAPHICS_VIEW_H

#include "MarkdownNode.h"

#include <QGraphicsView>
#include <QFont>

#include <vector>

class QContextMenuEvent;
class QContextMenuEvent;
class QResizeEvent;

class MarkdownGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MarkdownGraphicsView(QWidget* parent = nullptr);

    void setDocument(
        const std::vector<MarkdownNode>& document
    );

    void setRenderMode(bool enabled);
    bool renderMode() const;

signals:
    void renderModeRequested(bool enabled);

protected:
    void contextMenuEvent(
        QContextMenuEvent *event
    ) override;

    void resizeEvent(
        QResizeEvent *event
    ) override;

private:
    QFont text_font_;
    QFont code_font_;
    bool render_mode_ = false;

    std::vector<MarkdownNode> document_;

    QString inline_to_html(
        const MarkdownNode& node
    ) const;

    QString children_to_html(
        const MarkdownNode& node
    ) const;

    QString node_to_html(
        const MarkdownNode& node
    ) const;

    QString list_to_html(
        const MarkdownNode& node
    ) const;

    QString table_to_html(
        const MarkdownNode& node
    ) const;
	
	QString blocks_to_html(const std::vector<MarkdownNode>& nodes) const;

    void clear_document();

    void render_document();
};

#endif // MARKDOWN_GRAPHICS_VIEW_H

