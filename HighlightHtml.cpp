#include "ColorScheme.h"
#include "HighlightHtml.h"

#include <QString>

#include <memory>
#include <vector>

namespace
{

QString escaped(const std::string& value)
{
    return QString::fromUtf8(
               value.data(),
               static_cast<int>(value.size()))
        .toHtmlEscaped();
}

QString css_for(const highlight::TokenStyle& style)
{
    QString css =
        QStringLiteral("color:#%1")
            .arg(style.color, 6, 16, QLatin1Char('0'));

    if (style.bold) {
        css += QStringLiteral(";font-weight:bold");
    }

    if (style.italic) {
        css += QStringLiteral(";font-style:italic");
    }

    if (style.underline) {
        css += QStringLiteral(";text-decoration:underline");
    }

    return css;
}

} // namespace

QString highlight_code_html(
    const std::string& code,
    highlight::Language language)
{
    if (language == highlight::Language::None) {
        return escaped(code);
    }

    std::unique_ptr<highlight::Highlighter> highlighter =
        highlight::Highlighter::create(language);

    const std::vector<highlight::Token> tokens =
        highlighter->tokenize(code);

    if (tokens.empty()) {
        return escaped(code);
    }

    const highlight::ColorScheme& scheme =
        highlight::ColorScheme::default_light();

    QString result;

    std::size_t position = 0;

    for (const highlight::Token& token : tokens) {
        if (token.start > position) {
            result += escaped(
                code.substr(position, token.start - position));
        }

        result += QStringLiteral("<span style=\"%1\">%2</span>")
            .arg(
                css_for(scheme.style_for(token.type)),
                escaped(code.substr(token.start, token.length)));

        position = token.start + token.length;
    }

    if (position < code.size()) {
        result += escaped(code.substr(position));
    }

    return result;
}
