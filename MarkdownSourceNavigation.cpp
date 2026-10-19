#include "MarkdownGraphicsView.h"

#include "MarkdownHtmlRenderer.h"
#include <QGraphicsTextItem>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QDebug>

/*
 * Якоря навигации строятся по явным невидимым маркерам, которые
 * рендерер вставляет в начало каждого верхнеуровневого блока.
 *
 * Соответствие raw <-> rendered НЕ зависит от совпадения исходного
 * текста с отрендеренным: одинаковые абзацы, пропавшая разметка,
 * таблицы и формулы больше не влияют на поиск.
 */
void MarkdownGraphicsView::rebuildSourceAnchors() const
{
    source_anchors_.clear();

    if (!text_item_ || nav_source_lines_.empty()) {
        qDebug().noquote()
            << "[NAV] rebuildSourceAnchors: SKIP text_item="
            << (text_item_ ? "ok" : "null")
            << "nav_source_lines="
            << static_cast<long long>(nav_source_lines_.size());
        return;
    }

    QTextDocument* text_document =
        text_item_->document();

    if (!text_document || !text_document->documentLayout()) {
        return;
    }

    /*
     * QGraphicsTextItem стоит в позиции (16, 16). Координата блока
     * в QTextDocument переводится в координаты сцены прибавлением
     * этой позиции: item не масштабируется и не поворачивается.
     */
    const QPointF item_scene_pos = text_item_->pos();

    QAbstractTextDocumentLayout* layout =
        text_document->documentLayout();

    const ushort nav_marker = 0x2060;    // WORD JOINER (начало забега)
    const ushort nav_terminator = 0x2061; // FUNCTION APPLICATION (конец)

    /*
     * Сопоставление по сканированию блоков, а не поиском подстрок.
     *
     * Каждый маркер - это забег U+2060 длиной source_line + 1,
     * закрытый U+2061. В одном QTextBlock после схлопывания пустых
     * блоков может оказаться несколько маркеров (склеенных текстов);
     * терминатор разделяет их, поэтому извлекаем ВСЕ забеги блока по
     * порядку и обрабатываем каждый отдельно.
     *
     * Сопоставление монотонное по nav_source_lines_: для забега с
     * номером L ищем НАИМЕНЬШИЙ индекс k >= next_index с
     * nav_source_lines_[k] == L. Если такого k нет (маркер этого блока
     * схлопнулся/испорчен) - пропускаем забег, но НЕ сдвигаем
     * next_index. В отличие от прежнего строгого равенства, одно
     * несовпадение больше не обрывает все последующие якоря:
     * следующий корректный маркер сравняется уже со своим ожидаемым
     * номером, а не с застывшим.
     */
    std::size_t next_index = 0;

    for (QTextBlock block = text_document->begin();
         block.isValid() && next_index < nav_source_lines_.size();
         block = block.next()) {

        const QString text = block.text();

        if (text.isEmpty()) {
            continue;
        }

        const qreal block_top_in_document =
            layout->blockBoundingRect(block).top();

        const qreal scene_y =
            block_top_in_document + item_scene_pos.y();

        // Извлекаем каждый забег U+2060+ с терминатором U+2061.
        int run = 0;

        for (int i = 0; i < text.size(); ++i) {
            const ushort u = text.at(i).unicode();

            if (u == nav_marker) {
                ++run;
                continue;
            }

            if (u == nav_terminator && run > 0) {
                const int candidate_line = run - 1;
                run = 0;

                // Монотонный поиск ближайшего ещё не взятого номера.
                std::size_t match = next_index;

                while (match < nav_source_lines_.size() &&
                       nav_source_lines_[match] != candidate_line) {
                    ++match;
                }

                if (match >= nav_source_lines_.size()) {
                    // Нет ожидаемого номера: пропускаем забег,
                    // next_index не трогаем.
                    continue;
                }

                source_anchors_.push_back(
                    {candidate_line, scene_y});

                next_index = match + 1;
                continue;
            }

            // Любой другой символ прерывает незавершённый забег.
            run = 0;
        }
    }

    qDebug().noquote()
        << "[NAV] rebuildSourceAnchors: expected="
        << static_cast<long long>(nav_source_lines_.size())
        << "actual=" << static_cast<long long>(source_anchors_.size());
}

void MarkdownGraphicsView::scrollToSourceLine(int line)
{
    if (!text_item_) {
        return;
    }

    if (source_anchors_.empty()) {
        rebuildSourceAnchors();
    }

    if (source_anchors_.empty()) {
        verticalScrollBar()->setValue(
            verticalScrollBar()->minimum());

        return;
    }

    /*
     * Ищем rendered-координату для произвольной исходной строки
     * линейной интерполяцией между двумя ближайшими якорями.
     */
    qreal target_scene_y = 0.0;

    const SourceAnchor& first = source_anchors_.front();
    const SourceAnchor& last = source_anchors_.back();

    if (line <= first.line) {
        target_scene_y = first.y;
    } else if (line >= last.line) {
        target_scene_y = last.y;
    } else {
        for (std::size_t i = 0; i + 1 < source_anchors_.size(); ++i) {
            const SourceAnchor& a = source_anchors_[i];
            const SourceAnchor& b = source_anchors_[i + 1];

            if (line >= a.line && line <= b.line) {
                const int span = b.line - a.line;

                const qreal ratio =
                    (span > 0)
                        ? static_cast<qreal>(line - a.line) / span
                        : 0.0;

                target_scene_y = a.y + ratio * (b.y - a.y);
                break;
            }
        }
    }

    /*
     * Якорь задан в координатах сцены. Переводим его в положение
     * вертикальной полосы прокрутки через текущую координату
     * верхней границы видимой области.
     */
    const qreal current_scene_y =
        mapToScene(
            viewport()->rect().topLeft()).y();

    const qreal delta =
        target_scene_y - current_scene_y;

    const int target_value =
        verticalScrollBar()->value()
        + qRound(delta);

    verticalScrollBar()->setValue(
        qBound(
            verticalScrollBar()->minimum(),
            target_value,
            verticalScrollBar()->maximum()));
}

int MarkdownGraphicsView::sourceLineForCurrentScroll() const
{
    if (!text_item_) {
        qDebug() << "[NAV] sourceLineForCurrentScroll: NO text_item -> 0";
        return 0;
    }

    if (source_anchors_.empty()) {
        rebuildSourceAnchors();
    }

    if (source_anchors_.empty()) {
        qDebug().noquote()
            << "[NAV] sourceLineForCurrentScroll: anchors EMPTY after rebuild"
            << "(nav_source_lines=" << static_cast<long long>(nav_source_lines_.size())
            << ") -> 0";
        return 0;
    }

    const qreal current_y =
        mapToScene(
            viewport()->rect().topLeft()).y();

    const SourceAnchor& first = source_anchors_.front();
    const SourceAnchor& last = source_anchors_.back();

    qDebug().noquote()
        << "[NAV] sourceLineForCurrentScroll current_y=" << current_y
        << "anchors=" << static_cast<long long>(source_anchors_.size())
        << "first(line,y)=(" << first.line << "," << first.y << ")"
        << "last(line,y)=(" << last.line << "," << last.y << ")"
        << "sb(value,min,max)=(" << verticalScrollBar()->value()
        << "," << verticalScrollBar()->minimum()
        << "," << verticalScrollBar()->maximum() << ")";

    if (current_y <= first.y) {
        qDebug() << "[NAV]   -> branch BEFORE-FIRST, line=" << qMax(0, first.line);
        return qMax(0, first.line);
    }

    if (current_y >= last.y) {
        /*
         * last.y — это ВЕРХ последнего верхнеуровневого блока, а не
         * низ документа. Жёсткий возврат last.line «прилипал» к началу
         * последнего блока: при прокрутке rendered вниз raw всегда
         * прыгал в один и тот же (почти конец) блок. Вместо этого
         * экстраполируем вниз локальным масштабом последнего интервала
         * якорей, чтобы позиция монотонно шла к концу документа.
         * Итоговое значение всё равно ограничивается blockCount в
         * CodeEditor::scrollToSourceLine().
         */
        if (source_anchors_.size() >= 2) {
            const SourceAnchor& prev =
                source_anchors_[source_anchors_.size() - 2];

            const qreal y_span = last.y - prev.y;
            const int line_span = last.line - prev.line;

            if (y_span > 0.0 && line_span > 0) {
                const qreal scale =
                    static_cast<qreal>(line_span) / y_span;

                const int extra =
                    qRound((current_y - last.y) * scale);

                qDebug() << "[NAV]   -> branch AFTER-LAST extrapolate, line="
                         << qMax(0, last.line + extra);
                return qMax(0, last.line + extra);
            }
        }

        qDebug() << "[NAV]   -> branch AFTER-LAST clamp, line=" << qMax(0, last.line);
        return qMax(0, last.line);
    }

    /*
     * Обратная интерполяция: доля положения между двумя якорями
     * переносится в соответствующий интервал исходных строк.
     */
    for (std::size_t i = 0; i + 1 < source_anchors_.size(); ++i) {
        const SourceAnchor& a = source_anchors_[i];
        const SourceAnchor& b = source_anchors_[i + 1];

        if (current_y >= a.y && current_y <= b.y) {
            const qreal span = b.y - a.y;

            const qreal ratio =
                (span > 0.0)
                    ? (current_y - a.y) / span
                    : 0.0;

            const int line_span = b.line - a.line;

            const int result = qMax(0, a.line + qRound(ratio * line_span));
            qDebug() << "[NAV]   -> branch INTERP i=" << static_cast<long long>(i)
                     << "a.line=" << a.line << "b.line=" << b.line
                     << "ratio=" << ratio << "line=" << result;
            return result;
        }
    }

    qDebug() << "[NAV]   -> branch FALLTHROUGH, line=" << qMax(0, last.line);
    return qMax(0, last.line);
}