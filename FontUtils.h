#ifndef FONT_UTILS_H
#define FONT_UTILS_H

#include <QFont>
#include <QtGlobal>

inline QFont make_code_font(int point_size = -1)
{
    QFont font(QStringLiteral("Noto Sans Mono"));
    font.setStyleHint(QFont::Monospace, QFont::PreferMatch);

    if (point_size > 0) {
        font.setPointSize(point_size);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    font.setFallbackFamilies({
        QStringLiteral("Noto Color Emoji"),
        QStringLiteral("Noto Emoji"),
        QStringLiteral("Symbola"),
        QStringLiteral("DejaVu Sans")
    });
#endif

    return font;
}

#endif // FONT_UTILS_H
