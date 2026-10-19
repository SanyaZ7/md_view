#ifndef MARKDOWN_CODE_BLOCKS_H
#define MARKDOWN_CODE_BLOCKS_H

#include <QGraphicsProxyWidget>
#include <QString>

#include <vector>

class CodeBlockEditor;
class MarkdownGraphicsView;
class QPushButton;
class QTextBlock;

/*
 * Геометрия прокручиваемых блоков кода (в px).
 */
namespace MarkdownCodeBlocks
{
    constexpr int kCodeBlockPadding = 6;
    constexpr int kCodeBlockBorder = 1;
    constexpr int kCodeBlockVSpacing = 2;

    /*
     * Высота виджета кода, вмещающего заданное число строк.
     *
     * QTextDocument игнорирует CSS-свойство height у блочных элементов,
     * поэтому место под блок резервируется через QTextBlockFormat::FixedHeight
     * (см. MarkdownGraphicsView::applyCodeBlockHeights()). Высота виджета
     * и высота блока рассчитываются по одной и той же формуле, чтобы виджет
     * не наезжал на следующий за блоком текст.
     *
     * По замерам QPlainTextEdit реальная высота строки равна lineSpacing + 1,
     * а рамка, внутренние отступы и margin документа суммарно дают 15 px.
     */
    constexpr int kCodeBlockChrome = 15;

    /*
     * Высота отдельной области для кнопки «Копировать».
     * Она резервируется внутри каждого блока кода, чтобы кнопка
     * не перекрывала первую строку кода.
     */
    constexpr int kCodeBlockButtonArea = 28;

    int codeBlockWidgetHeight(
        int lines,
        qreal line_height);
}

#endif // MARKDOWN_CODE_BLOCKS_H

