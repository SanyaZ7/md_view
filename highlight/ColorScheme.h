#ifndef HIGHLIGHT_COLOR_SCHEME_H
#define HIGHLIGHT_COLOR_SCHEME_H

#include <string>
#include <unordered_map>

namespace highlight
{

/*
 * Оформление одного семантического типа токена.
 *
 * Цвет хранится упакованным как 0xRRGGBB, без зависимости от Qt.
 */
struct TokenStyle
{
    unsigned int color = 0x000000;
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

/*
 * Соответствие «семантический тип токена -> оформление».
 *
 * Сейчас используется схема по умолчанию (default_light()).
 * Загрузка схемы из настроек и её редактирование в GUI — отдельная
 * задача; для этого у класса есть set().
 */
class ColorScheme
{
public:
    ColorScheme();

    void set(const std::string& type, const TokenStyle& style);

    /*
     * Оформление для типа. Составные типы наследуют оформление
     * родителя: "string.special.key" -> "string.special" ->
     * "string". Если совпадений нет — оформление по умолчанию.
     */
    TokenStyle style_for(const std::string& type) const;

    /* Светлая схема по умолчанию. */
    static const ColorScheme& default_light();

private:
    std::unordered_map<std::string, TokenStyle> styles_;
    TokenStyle fallback_;
};

} // namespace highlight

#endif // HIGHLIGHT_COLOR_SCHEME_H
