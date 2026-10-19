#ifndef MARKDOWN_MATH_OBJECTS_H
#define MARKDOWN_MATH_OBJECTS_H

#include <QTextObjectInterface>
#include <QObject>
#include <QFont>
#include <QSizeF>

class QPainter;
class QTextDocument;
class QTextFormat;

namespace MarkdownMathObjects
{
    /*
     * Регистрирует обработчики пользовательских inline-объектов
     * в layout указанного QTextDocument.
     */
    void registerHandlers(QTextDocument* document);

    /*
     * Единый проход по документу: превращает маркерные
     * последовательности PUA в inline-объекты.
     */
    void installObjects(QTextDocument* document);
}

class FractionObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    // Единый экземпляр на всё приложение: его можно регистрировать
    // одновременно в основном и во вложенных мини-документах дробей.
    static FractionObject* instance();

    QSizeF intrinsicSize(
        QTextDocument* document,
        int posInDocument,
        const QTextFormat& format) override;

    void drawObject(
        QPainter* painter,
        const QRectF& rect,
        QTextDocument* document,
        int posInDocument,
        const QTextFormat& format) override;

    // Дробь рисуется чуть меньше основного текста, как в TeX.
    static QFont partFont(const QFont& base_font);

    // Создаёт мини-документ с одним фрагментом (числителем или
    // знаменателем). Обработчик дробей регистрируется здесь же,
    // поэтому вложенные \frac внутри числителя/знаменателя тоже
    // отрисуются корректно.
    static QTextDocument* makePartDocument(
        const QString& html,
        const QFont& part_font);

    static void paintDocument(
        QPainter* painter,
        QTextDocument* document);

private:
    FractionObject();

    /*
     * Превращает маркерные последовательности U+E000..U+E002
     * в inline-объекты (ObjectReplacementCharacter) внутри документа.
     */
    static void installFractionObjects(QTextDocument* document);

    QSizeF measurePart(
        const QString& html,
        const QTextFormat& format);

    static const int kFracHPad = 4;
    static const int kFracBarGap = 4;
};

class SqrtObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    static SqrtObject* instance();

    QSizeF intrinsicSize(
        QTextDocument* document,
        int posInDocument,
        const QTextFormat& format) override;

    void drawObject(
        QPainter* painter,
        const QRectF& rect,
        QTextDocument* document,
        int posInDocument,
        const QTextFormat& format) override;

private:
    SqrtObject();

    static const int kRadicalWidth = 11;
    static const int kVinculumGap = 3;
    static const int kRightPad = 2;
};

class HatObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    static HatObject* instance();

    QSizeF intrinsicSize(
        QTextDocument* document,
        int posInDocument,
        const QTextFormat& format) override;

    void drawObject(
        QPainter* painter,
        const QRectF& rect,
        QTextDocument* document,
        int posInDocument,
        const QTextFormat& format) override;

private:
    HatObject();

    static const int kHatWidth = 9;
    static const int kHatHeight = 5;
    static const int kHatGap = 1;
    static const int kRightPad = 2;
};

#endif // MARKDOWN_MATH_OBJECTS_H

