#include "MarkdownMathObjects.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextObject>
#include <QTextOption>
#include <QLineF>

#include <algorithm>
#include <memory>

/*
 * Маркеры дроби из Private Use Area, которые генерирует
 * math_renderer для \frac{...}{...}:
 *
 *   U+E000 <числитель> U+E001 <знаменатель> U+E002
 *
 * MarkdownGraphicsView превращает эту последовательность в
 * inline-объект (QTextObjectInterface), который рисуется
 * вертикально и не приводит к переносу строки в QTextDocument.
 */
static const QChar kFracBegin(QChar(0xE000));
static const QChar kFracSep(QChar(0xE001));
static const QChar kFracEnd(QChar(0xE002));

/*
 * Тип пользовательского объекта для QTextObjectInterface.
 * Значение произвольное, но должно быть стабильным.
 */
static const int kFracObjectType = QTextFormat::UserObject + 1;

/*
 * Ключи QTextCharFormat::property(), через которые в объект
 * передаются числитель и знаменатель дроби.
 */
static const int kFracNumProperty = QTextFormat::UserProperty + 1;
static const int kFracDenProperty = QTextFormat::UserProperty + 2;

/*
 * Маркеры PUA для \sqrt и \hat:
 *   U+E010 <radicand> U+E011
 *   U+E020 <argument> U+E021
 */
static const QChar kSqrtBegin(QChar(0xE010));
static const QChar kSqrtEnd(QChar(0xE011));
static const QChar kHatBegin(QChar(0xE020));
static const QChar kHatEnd(QChar(0xE021));

static const int kSqrtObjectType = QTextFormat::UserObject + 2;
static const int kHatObjectType = QTextFormat::UserObject + 3;

/*
 * True, если часть дроби (числитель/знаменатель) сама содержит
 * вложенную формулу (дальнейший \frac, \sqrt, \hat). Такая часть
 * высокая, и для неё дробь обязан прижимать содержимое к
 * rect.bottom(), иначе знаменатель уйдёт за baseline и перекроет
 * следующий блок. Простая (однострочная) часть наоборот ставится
 * на math axis, чтобы дробь не всплывала относительно текста.
 */
static bool containsNestedMath(const QString& part)
{
    for (const QChar& symbol : part) {
        const ushort code = symbol.unicode();

        if (code >= 0xE000 && code <= 0xE002) {
            return true;
        }
    }

    return false;
}

/*
 * Единая геометрия дроби для intrinsicSize() и drawObject().
 *
 * Qt резервирует под inline-объект ровно intrinsicSize() и ставит
 * rect.bottom() на базовую линию строки (descent объекта == 0).
 * Отсюда взаимоисключающие требования: поставить черту дроби на
 * math axis строки (чтобы простая дробь не всплывала относительно
 * текста) ИЛИ не выпускать знаменатель ниже rect.bottom() (чтобы
 * он не перекрывал следующий блок). Одновременно — нельзя.
 *
 * Поэтому два режима, выбираемые по типу знаменателя:
 *
 *   simple (знаменатель без вложенной формулы): черта ставится на
 *     math axis, знаменатель слегка провисает ниже baseline (как в
 *     TeX). Это убирает всплытие простых дробей.
 *
 *   nested (знаменатель содержит \frac/\sqrt/\hat): знаменатель
 *     прижимается к rect.bottom(), весь контент дроби вписан в rect,
 *     ничего не перекрывает нижний блок.
 *
 * bar_offset — расстояние от rect.bottom() вверх до черты; height —
 * высота, возвращаемая intrinsicSize(). Оба вычисляются здесь, чтобы
 * измерение и отрисовка не разъезжались.
 */
struct FracGeom {
    qreal bar_offset = 0.0;
    qreal height = 0.0;
};

static FracGeom fractionGeometry(
    const QString& num,
    const QString& den,
    const QSizeF& num_size,
    const QSizeF& den_size,
    const QFont& base_font,
    qreal gap)
{
    FracGeom geom;

    if (containsNestedMath(num) || containsNestedMath(den)) {
        geom.bar_offset = den_size.height() + gap;
        geom.height =
            num_size.height() + den_size.height() + gap * 2.0;
    } else {
        const QFontMetrics base_fm(base_font);
        const qreal math_axis = base_fm.ascent() * 0.35 + 3.0;

        geom.bar_offset = math_axis;
        geom.height = math_axis + gap + num_size.height();
    }

    return geom;
}

/*
 * Содержимое radicand/argument передаётся в объект этим свойством.
 */
static const int kMathContentProperty = QTextFormat::UserProperty + 3;

/*
 * Общий базовый функционал для inline-объектов, которые рисуют
 * один вложенный мини-документ (radicand у \sqrt, argument у \hat)
 * и измеряют его через FractionObject-хелперы.
 */
static QSizeF measureMathPart(
    const QString& html,
    const QTextFormat& format)
{
    const QFont part_font = FractionObject::partFont(
        format.toCharFormat().font());

    std::unique_ptr<QTextDocument> part_document(
        FractionObject::makePartDocument(html, part_font));

    part_document->setTextWidth(-1.0);

    return part_document->documentLayout()->documentSize();
}

FractionObject::FractionObject() = default;

SqrtObject::SqrtObject() = default;

HatObject::HatObject() = default;

QSizeF FractionObject::measurePart(
    const QString& html,
    const QTextFormat& format)
{
    const QFont part_font =
        FractionObject::partFont(
            format.toCharFormat().font());

    std::unique_ptr<QTextDocument> part_document(
        FractionObject::makePartDocument(
            html,
            part_font));

    part_document->setTextWidth(-1.0);

    return part_document->documentLayout()->documentSize();
}


FractionObject* FractionObject::instance()
{
    // Единый экземпляр на всё приложение: его можно регистрировать
    // одновременно в основном и во вложенных мини-документах дробей.
    static FractionObject object;
    return &object;
}

QSizeF FractionObject::intrinsicSize(
    QTextDocument* document,
    int,
    const QTextFormat& format)
{
    const QString num =
        format.property(kFracNumProperty).toString();
    const QString den =
        format.property(kFracDenProperty).toString();

    const QSizeF num_size = measurePart(num, format);
    const QSizeF den_size = measurePart(den, format);

    QFont base_font = format.toCharFormat().font();

    if (base_font.pointSizeF() <= 0.0 &&
        base_font.pixelSize() <= 0 &&
        document) {
        base_font = document->defaultFont();
    }

    const FracGeom geom = fractionGeometry(
        num, den, num_size, den_size, base_font, kFracBarGap);

    const qreal width =
        qMax(num_size.width(), den_size.width()) + kFracHPad * 2.0;

    return QSizeF(width, geom.height);
}

void FractionObject::drawObject(
    QPainter* painter,
    const QRectF& rect,
    QTextDocument* document,
    int,
    const QTextFormat& format)
{
    const QString num =
        format.property(kFracNumProperty).toString();

    const QString den =
        format.property(kFracDenProperty).toString();

    QFont base_font = format.toCharFormat().font();

    if (base_font.pointSizeF() <= 0.0 &&
        base_font.pixelSize() <= 0 &&
        document) {
        base_font = document->defaultFont();
    }

    const QFont part_font = FractionObject::partFont(base_font);

    std::unique_ptr<QTextDocument> num_document(
        FractionObject::makePartDocument(num, part_font));

    std::unique_ptr<QTextDocument> den_document(
        FractionObject::makePartDocument(den, part_font));

    num_document->setTextWidth(-1.0);
    den_document->setTextWidth(-1.0);

    const QSizeF num_size =
        num_document->documentLayout()->documentSize();

    const QSizeF den_size =
        den_document->documentLayout()->documentSize();

    /*
     * Геометрия дроби берётся из fractionGeometry() — того же
     * расчёта, что и в intrinsicSize(), поэтому измерение и
     * отрисовка не разъезжаются. bar_offset задаёт высоту черты над
     * rect.bottom() (= базовой линией строки): для простой дроби
     * это math axis (дробь садится ровно на текст), для составной —
     * весь контент вписан в rect без перекрытия нижнего блока.
     */
    const FracGeom geom = fractionGeometry(
        num, den, num_size, den_size, base_font, kFracBarGap);

    const qreal bar_y = rect.bottom() - geom.bar_offset;
    const qreal num_top =
        bar_y - kFracBarGap - num_size.height();
    const qreal den_top = bar_y + kFracBarGap;

    painter->save();
    painter->setPen(painter->pen().color());

    // Числитель по центру.
    painter->save();
    painter->translate(
        rect.left() + (rect.width() - num_size.width()) / 2.0,
        num_top);
    FractionObject::paintDocument(
        painter,
        num_document.get());
    painter->restore();

    // Знаменатель по центру.
    painter->save();
    painter->translate(
        rect.left() + (rect.width() - den_size.width()) / 2.0,
        den_top);
    FractionObject::paintDocument(
        painter,
        den_document.get());
    painter->restore();

    // Горизонтальная черта между числителем и знаменателем.
    const qreal bar_half =
        qMax(num_size.width(), den_size.width()) / 2.0;

    painter->drawLine(
        QLineF(
            rect.center().x() - bar_half,
            bar_y,
            rect.center().x() + bar_half,
            bar_y));

    painter->restore();
}

// Дробь рисуется чуть меньше основного текста, как в TeX.
QFont FractionObject::partFont(const QFont& base_font)
{
    QFont part_font = base_font;

    const qreal point_size =
        base_font.pointSizeF() > 0.0
            ? base_font.pointSizeF()
            : static_cast<qreal>(base_font.pointSize());

    if (point_size > 0.0) {
        part_font.setPointSizeF(point_size * 0.9);
    } else {
        const int pixel_size = base_font.pixelSize();

        if (pixel_size > 0) {
            part_font.setPixelSize(
                qMax(
                    1,
                    static_cast<int>(pixel_size * 0.9)));
        }
    }

    return part_font;
}

// Создаёт мини-документ с одним фрагментом (числителем или
// знаменателем). Обработчик дробей регистрируется здесь же,
// поэтому вложенные \frac внутри числителя/знаменателя тоже
// отрисуются корректно.
QTextDocument* FractionObject::makePartDocument(
    const QString& html,
    const QFont& part_font)
{
    QTextDocument* part_document = new QTextDocument;
    part_document->setDefaultFont(part_font);
    part_document->setDocumentMargin(0.0);

    MarkdownMathObjects::registerHandlers(part_document);

    part_document->setHtml(
        QStringLiteral("<div style=\"white-space:nowrap;\">") +
        html +
        QStringLiteral("</div>"));

    MarkdownMathObjects::installObjects(part_document);

    return part_document;
}

void FractionObject::paintDocument(
    QPainter* painter,
    QTextDocument* document)
{
    const QSizeF document_size =
        document->documentLayout()->documentSize();

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(
        QPalette::Text,
        painter->pen().color());

    context.clip =
        QRectF(QPointF(0.0, 0.0), document_size);

    document->documentLayout()->draw(painter, context);
}

/*
 * Превращает маркерные последовательности U+E000..U+E002 в
 * inline-объекты (ObjectReplacementCharacter) внутри документа.
 */
void FractionObject::installFractionObjects(QTextDocument* document)
{
    if (!document) {
        return;
    }

    const QString plain = document->toPlainText();

    struct FractionRange {
        int begin = -1;
        int sep = -1;
        int end = -1;
    };

    QList<FractionRange> ranges;

    int depth = 0;
    int begin_pos = -1;
    int sep_pos = -1;

    for (int i = 0; i < plain.size(); ++i) {
        const QChar symbol = plain.at(i);

        if (symbol == kFracBegin) {
            if (depth == 0) {
                begin_pos = i;
            }

            ++depth;
        } else if (symbol == kFracSep) {
            if (depth == 1) {
                sep_pos = i;
            }
        } else if (symbol == kFracEnd) {
            --depth;

            if (depth == 0 &&
                begin_pos >= 0 &&
                sep_pos > begin_pos) {
                FractionRange range;
                range.begin = begin_pos;
                range.sep = sep_pos;
                range.end = i;
                ranges.append(range);

                begin_pos = -1;
                sep_pos = -1;
            }
        }
    }

    if (ranges.isEmpty()) {
        return;
    }

    QTextCursor cursor(document);

    // Замена справа налево, чтобы позиции предыдущих
    // диапазонов не сбивались.
    for (int r = ranges.size() - 1; r >= 0; --r) {
        const FractionRange& range = ranges.at(r);

        const QString num =
            plain.mid(
                range.begin + 1,
                range.sep - range.begin - 1);

        const QString den =
            plain.mid(
                range.sep + 1,
                range.end - range.sep - 1);

        cursor.setPosition(range.begin);
        cursor.setPosition(
            range.end + 1,
            QTextCursor::KeepAnchor);

        QFont base_font = cursor.charFormat().font();

        if (base_font.pointSizeF() <= 0.0 &&
            base_font.pixelSize() <= 0) {
            base_font = document->defaultFont();
        }

        QTextCharFormat object_format;
        object_format.setFont(base_font);

        object_format.setObjectType(kFracObjectType);
        object_format.setProperty(kFracNumProperty, num);
        object_format.setProperty(kFracDenProperty, den);

        cursor.insertText(
            QString(QChar(QChar::ObjectReplacementCharacter)),
            object_format);
    }
}

/*
 * Inline-объект \sqrt{...}: рисует radical слева и полную
 * надчёркивающую черту (vinculum) над всем подкорренным выражением.
 */
SqrtObject* SqrtObject::instance()
{
    static SqrtObject object;
    return &object;
}

QSizeF SqrtObject::intrinsicSize(
    QTextDocument* document,
    int,
    const QTextFormat& format)
{
    Q_UNUSED(document);

    const QSizeF content = measureMathPart(
        format.property(kMathContentProperty).toString(),
        format);

    const qreal width =
        kRadicalWidth + content.width() + kRightPad;

    // documentSize() мини-документа = ascent+descent его строки.
    // Qt не резервирует ничего ниже rect.bottom(), поэтому
    // intrinsicSize обязан покрывать и descent radicand'а, и
    // винкулум сверху — иначе подчерённое (особенно с дробью или
    // буквами с хвостами) перекрывает нижний блок.
    const qreal height =
        content.height() + kVinculumGap;

    return QSizeF(width, height);
}

void SqrtObject::drawObject(
    QPainter* painter,
    const QRectF& rect,
    QTextDocument* document,
    int,
    const QTextFormat& format)
{
    const QString content_html =
        format.property(kMathContentProperty).toString();

    QFont base_font = format.toCharFormat().font();

    if (base_font.pointSizeF() <= 0.0 &&
        base_font.pixelSize() <= 0 &&
        document) {
        base_font = document->defaultFont();
    }

    const QFont part_font = FractionObject::partFont(base_font);

    std::unique_ptr<QTextDocument> content_document(
        FractionObject::makePartDocument(
            content_html,
            part_font));

    content_document->setTextWidth(-1.0);

    const QSizeF content =
        content_document->documentLayout()->documentSize();

    const qreal radical_x = rect.left();
    const qreal radicand_x =
        rect.left() + kRadicalWidth;

    /*
     * Qt ставит rect.bottom() inline-объекта на базовую линию
     * строки и не резервирует ничего ниже. Поэтому весь radicand
     * (вместе с его descent) сажаем строго в rect: верх — на
     * rect.top()+kVinculumGap, низ — ровно на rect.bottom().
     * Винкулум идёт по rect.top(). Такая геометрия тождественна
     * высоте из intrinsicSize() (content.height()+kVinculumGap) и
     * согласована с вложенными дробями/корнями внутри radicand'а.
     */
    const qreal content_top_y = rect.top() + kVinculumGap;
    const qreal vinculum_y = rect.top();
    const qreal baseline_y = rect.bottom();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(
        QPen(painter->pen().color(), 1.2));

    // Radicand.
    painter->save();
    painter->translate(
        radicand_x,
        content_top_y);
    FractionObject::paintDocument(
        painter,
        content_document.get());
    painter->restore();

    // Vinculum (горизонтальная черта над всем подкоренным).
    painter->drawLine(
        QLineF(
            radicand_x,
            vinculum_y,
            rect.right(),
            vinculum_y));

    // Radical: короткая ножка вниз + длинная диагональ вверх.
    painter->drawLine(
        QLineF(
            radical_x,
            baseline_y - kRadicalWidth * 0.6,
            radical_x + kRadicalWidth * 0.32,
            baseline_y));

    painter->drawLine(
        QLineF(
            radical_x + kRadicalWidth * 0.32,
            baseline_y,
            radicand_x,
            vinculum_y));

    painter->restore();
}

/*
 * Inline-объект \hat{...}: рисует circumflex (крышу) точно по центру
 * над аргументом, не смещая базовую линию строки.
 */
HatObject* HatObject::instance()
{
    static HatObject object;
    return &object;
}

QSizeF HatObject::intrinsicSize(
    QTextDocument* document,
    int,
    const QTextFormat& format)
{
    Q_UNUSED(document);

    const QSizeF content = measureMathPart(
        format.property(kMathContentProperty).toString(),
        format);

    const qreal width =
        qMax(
            content.width(),
            static_cast<qreal>(kHatWidth));

    const qreal height =
        content.height() + kHatHeight + kHatGap;

    return QSizeF(width + kRightPad, height);
}

void HatObject::drawObject(
    QPainter* painter,
    const QRectF& rect,
    QTextDocument* document,
    int,
    const QTextFormat& format)
{
    const QString content_html =
        format.property(kMathContentProperty).toString();

    QFont base_font = format.toCharFormat().font();

    if (base_font.pointSizeF() <= 0.0 &&
        base_font.pixelSize() <= 0 &&
        document) {
        base_font = document->defaultFont();
    }

    const QFont part_font =
        FractionObject::partFont(base_font);

    const QFontMetrics part_fm(part_font);

    std::unique_ptr<QTextDocument> content_document(
        FractionObject::makePartDocument(
            content_html,
            part_font));

    content_document->setTextWidth(-1.0);

    const QSizeF content =
        content_document->documentLayout()->documentSize();

    const qreal content_x =
        rect.left() +
        (rect.width() - kRightPad - content.width()) / 2.0;

    /*
     * Baseline аргумента сажаем на rect.bottom() (базовую линию
     * строки), чтобы H не всплывал над остальной формулой.
     */
    const qreal baseline_y = rect.bottom();
    const qreal content_top_y =
        baseline_y - part_fm.ascent();

    // Основание крыши — вплотную над капительными буквами аргумента
    // (cap height), а не над ascent шрифта, иначе шапка сидит
    // слишком высоко над буквой.
    const qreal hat_base_y =
        baseline_y - part_fm.capHeight() - kHatGap;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Argument.
    painter->save();
    painter->translate(
        content_x,
        content_top_y);
    FractionObject::paintDocument(
        painter,
        content_document.get());
    painter->restore();

    // Circumflex (крыша) по центру над аргументом.
    const qreal cx =
        rect.left() +
        (rect.width() - kRightPad) / 2.0;

    painter->setPen(
        QPen(painter->pen().color(), 1.2));

    painter->drawLine(
        QLineF(
            cx - kHatWidth / 2.0,
            hat_base_y,
            cx,
            hat_base_y - kHatHeight));

    painter->drawLine(
        QLineF(
            cx,
            hat_base_y - kHatHeight,
            cx + kHatWidth / 2.0,
            hat_base_y));

    painter->restore();
}

namespace MarkdownMathObjects
{
    void registerHandlers(QTextDocument* document)
    {
        if (!document || !document->documentLayout()) {
            return;
        }

        document->documentLayout()->registerHandler(
            kFracObjectType,
            FractionObject::instance());

        document->documentLayout()->registerHandler(
            kSqrtObjectType,
            SqrtObject::instance());

        document->documentLayout()->registerHandler(
            kHatObjectType,
            HatObject::instance());
    }

    void installObjects(QTextDocument* document)
    {
        if (!document) {
            return;
        }

        const QString plain = document->toPlainText();

        if (plain.isEmpty()) {
            return;
        }

        enum MathType {
            FracType = 0,
            SqrtType = 1,
            HatType = 2
        };

        struct MathNode {
            int type = FracType;
            int begin = -1;
            int sep = -1;
            int end = -1;
        };

        QList<MathNode> roots;

        int depth = 0;
        int current_begin = -1;
        int current_sep = -1;
        int current_type = FracType;
        int frac_depth = 0;

        for (int i = 0; i < plain.size(); ++i) {
            const QChar symbol = plain.at(i);

            const bool is_open =
                symbol == kFracBegin ||
                symbol == kSqrtBegin ||
                symbol == kHatBegin;

            const bool is_close =
                symbol == kFracEnd ||
                symbol == kSqrtEnd ||
                symbol == kHatEnd;

            if (is_open) {
                if (depth == 0) {
                    current_begin = i;
                    current_sep = -1;
                    current_type =
                        symbol == kFracBegin
                            ? FracType
                            : (symbol == kSqrtBegin
                                   ? SqrtType
                                   : HatType);
                    frac_depth = 0;
                }

                if (symbol == kFracBegin) {
                    ++frac_depth;
                }

                ++depth;
                continue;
            }

            if (is_close) {
                --depth;

                if (symbol == kFracEnd) {
                    --frac_depth;
                }

                if (depth == 0 &&
                    current_begin >= 0) {
                    MathNode node;
                    node.type = current_type;
                    node.begin = current_begin;
                    node.sep = current_sep;
                    node.end = i;
                    roots.append(node);

                    current_begin = -1;
                    current_sep = -1;
                }

                continue;
            }

            if (symbol == kFracSep &&
                depth == 1 &&
                frac_depth == 1 &&
                current_sep < 0) {
                current_sep = i;
            }
        }

        if (roots.isEmpty()) {
            return;
        }

        QTextCursor cursor(document);

        // Замена справа налево, чтобы позиции предыдущих
        // корневых диапазонов не сбивались.
        for (int r = roots.size() - 1; r >= 0; --r) {
            const MathNode& node = roots.at(r);

            QFont base_font;
            {
                QTextCursor probe(document);
                probe.setPosition(node.begin);
                base_font = probe.charFormat().font();
            }

            if (base_font.pointSizeF() <= 0.0 &&
                base_font.pixelSize() <= 0) {
                base_font = document->defaultFont();
            }

            QTextCharFormat object_format;
            object_format.setFont(base_font);

            if (node.type == FracType &&
                node.sep > node.begin) {
                const QString num =
                    plain.mid(
                        node.begin + 1,
                        node.sep - node.begin - 1);

                const QString den =
                    plain.mid(
                        node.sep + 1,
                        node.end - node.sep - 1);

                object_format.setObjectType(kFracObjectType);
                object_format.setProperty(
                    kFracNumProperty,
                    num);
                object_format.setProperty(
                    kFracDenProperty,
                    den);
            } else if (node.type == SqrtType) {
                const QString radicand =
                    plain.mid(
                        node.begin + 1,
                        node.end - node.begin - 1);

                object_format.setObjectType(kSqrtObjectType);
                object_format.setProperty(
                    kMathContentProperty,
                    radicand);
            } else if (node.type == HatType) {
                const QString argument =
                    plain.mid(
                        node.begin + 1,
                        node.end - node.begin - 1);

                object_format.setObjectType(kHatObjectType);
                object_format.setProperty(
                    kMathContentProperty,
                    argument);
            } else {
                continue;
            }

            cursor.setPosition(node.begin);
            cursor.setPosition(
                node.end + 1,
                QTextCursor::KeepAnchor);

            cursor.insertText(
                QString(QChar(QChar::ObjectReplacementCharacter)),
                object_format);
        }
    }
}

