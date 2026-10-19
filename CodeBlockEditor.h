#ifndef CODE_BLOCK_EDITOR_H
#define CODE_BLOCK_EDITOR_H

#include <QPlainTextEdit>

class MarkdownGraphicsView;
class CodeHighlighter;
class QFocusEvent;
class QMouseEvent;
class QWheelEvent;

/*
 * Прокручиваемый блок кода.
 *
 * Колесо мыши прокручивает содержимое только тогда, когда блок
 * явно активирован щелчком (см. MarkdownGraphicsView::mousePressEvent).
 * В противном случае событие уходит дальше и прокручивается
 * весь документ.
 */
class CodeBlockEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeBlockEditor(
        MarkdownGraphicsView* view,
        QWidget* parent = nullptr);

    bool isActive() const
    {
        return active_;
    }

    void setActive(bool active);

    /*
     * Подсветка блока по языку из info string (```cpp).
     * Пустая строка или неподдерживаемый язык отключают подсветку.
     */
    void setCodeLanguage(const QString& language);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    MarkdownGraphicsView* view_ = nullptr;
    bool active_ = false;

    /*
     * Владелец — документ редактора (QSyntaxHighlighter удаляется
     * вместе с QTextDocument).
     */
    CodeHighlighter* highlighter_ = nullptr;
};

#endif // CODE_BLOCK_EDITOR_H

