#include "CodeBlockEditor.h"

#include "CodeHighlighter.h"
#include "FontUtils.h"
#include "MarkdownGraphicsView.h"
#include "highlight.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

CodeBlockEditor::CodeBlockEditor(
    MarkdownGraphicsView* view,
    QWidget* parent)
    : QPlainTextEdit(parent)
    , view_(view)
{
    setFont(make_code_font());
    setFocusPolicy(Qt::ClickFocus);
}

void CodeBlockEditor::setActive(bool active)
{
    active_ = active;

    if (active_) {
        setFocus(Qt::MouseFocusReason);
    } else {
        clearFocus();
    }
}

void CodeBlockEditor::setCodeLanguage(const QString& language)
{
    const highlight::Language code_language =
        highlight::language_from_name(language.toStdString());

    if (code_language == highlight::Language::None) {
        return;
    }

    highlighter_ = new CodeHighlighter(document());
    highlighter_->setLanguage(code_language);
}

void CodeBlockEditor::mousePressEvent(QMouseEvent* event)
{
    if (view_) {
        view_->activateCodeBlock(this);
    }

    QPlainTextEdit::mousePressEvent(event);
}

void CodeBlockEditor::wheelEvent(QWheelEvent* event)
{
    if (active_) {
        QPlainTextEdit::wheelEvent(event);
        return;
    }

    /*
     * QGraphicsProxyWidget не всегда передаёт ignored wheel-event
     * из вложенного QPlainTextEdit в QGraphicsView. Поэтому до явного
     * клика по блоку прокручиваем общий scrollbar preview вручную.
     */
    if (!view_) {
        event->ignore();
        return;
    }

    QScrollBar* scroll_bar =
        view_->verticalScrollBar();

    if (!scroll_bar ||
        scroll_bar->maximum() <= scroll_bar->minimum()) {
        event->ignore();
        return;
    }

    int scroll_delta = 0;

    const QPoint pixel_delta =
        event->pixelDelta();

    if (!pixel_delta.isNull()) {
        /*
         * Тачпад: Qt уже передаёт величину в пикселях.
         * Положительное значение означает прокрутку вверх.
         */
        scroll_delta = pixel_delta.y();
    } else {
        /*
         * Обычное колесо: angleDelta() измеряется в 1/8 градуса,
         * один стандартный шаг равен 120.
         */
        const int angle_delta =
            event->angleDelta().y();

        if (angle_delta != 0) {
            const int lines_per_step =
                qMax(
                    1,
                    QApplication::wheelScrollLines());

            const int pixels_per_step =
                qMax(
                    scroll_bar->singleStep(),
                    QFontMetrics(
                        view_->font()).lineSpacing())
                * lines_per_step;

            scroll_delta =
                (angle_delta * pixels_per_step) / 120;

            /*
             * Не теряем частичные wheel-события от некоторых мышей
             * и тачпадов, если целочисленное деление дало ноль.
             */
            if (scroll_delta == 0) {
                scroll_delta =
                    angle_delta > 0
                        ? pixels_per_step
                        : -pixels_per_step;
            }
        }
    }

    if (scroll_delta == 0) {
        event->ignore();
        return;
    }

    const int new_value =
        qBound(
            scroll_bar->minimum(),
            scroll_bar->value() - scroll_delta,
            scroll_bar->maximum());

    scroll_bar->setValue(new_value);
    event->accept();
}

void CodeBlockEditor::focusOutEvent(QFocusEvent* event)
{
    active_ = false;
    QPlainTextEdit::focusOutEvent(event);
}

